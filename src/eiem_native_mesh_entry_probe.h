#pragma once

// Native Mesh payload-entry observation. Trace builds observe the game's
// source Meshes only and do not install the finalizer detour. The old alias /
// finalizer replacement experiment remains archived behind its separate macro.

#if defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD) && \
    (defined(EIEM_NATIVE_MESH_ENTRY_REPLACE_BUILD) || \
     defined(EIEM_NATIVE_MESH_CLONE_BUILD))
#error Native Mesh source tracing must not enable replacement or clone experiments
#endif

#if defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD) || \
    defined(EIEM_NATIVE_MESH_ENTRY_REPLACE_BUILD)

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <memory>
#include <vector>
#include "eiem_native_mesh_read_observation.h"

static constexpr uintptr_t kEiemNativeMeshDeserializeRva = 0x1EA2A0;
static constexpr uintptr_t kEiemNativeMeshDeserializeThunkRva = 0x4710D0;
static constexpr uintptr_t kEiemNativeMeshFinalizerRva = 0xD58A0;
static constexpr uintptr_t kEiemNativeMeshVtableRva = 0x19C8230;
static constexpr uintptr_t kEiemNativeObjectReadRva = 0x133800;
static constexpr uintptr_t kEiemNativeMeshFileProviderVtableRva = 0x18678D0;
static constexpr LONG kEiemNativeMeshEntryTraceLimit = 32;

using EiemNativeMeshDeserializeFn = uintptr_t(__fastcall *)(uintptr_t nativeMesh,
                                                             uintptr_t stream);
using EiemNativeMeshFinalizerFn = uintptr_t(__fastcall *)(uintptr_t nativeMesh);
// +0x133800: four register arguments, then two output pointers and the native
// object on the stack. The third register is unused but forwarded unchanged.
using EiemNativeObjectReadFn = uintptr_t(__fastcall *)(
    uintptr_t owner, int64_t objectKey, uintptr_t unused, uint8_t flags,
    uintptr_t *typeOutput, uint8_t *readOutput, uintptr_t nativeObject);

static EiemNativeMeshDeserializeFn s_eiemNativeMeshDeserializeOriginal = nullptr;
static EiemNativeMeshFinalizerFn s_eiemNativeMeshFinalizerOriginal = nullptr;
static volatile LONG s_eiemNativeMeshEntryTraceCount = 0;
static volatile LONG s_eiemNativeMeshFinalizerTraceCount = 0;
static thread_local bool s_eiemNativeMeshEntryInHook = false;
static thread_local uintptr_t s_eiemNativeMeshEntryActiveNative = 0;
static thread_local bool s_eiemNativeMeshFinalizerInHook = false;
#if defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD)
static EiemNativeObjectReadFn s_eiemNativeObjectReadOriginal = nullptr;
struct EiemNativeMeshReadContext {
  uintptr_t owner = 0;
  uintptr_t native = 0;
  int64_t objectKey = 0;
  uintptr_t caller = 0;
  uint8_t flags = 0;
  uintptr_t stream = 0;
  LONG ordinal = 0;
  char name[192] = {};
  bool observed = false;
  bool declaredTarget = false;
  bool nestedObject = false;
  EiemNativeReadWindow before;
  EiemNativeReadWindow after;
};
static thread_local EiemNativeMeshReadContext *s_eiemNativeMeshReadContext = nullptr;
static volatile LONG s_eiemNativeMeshReadMissingContextCount = 0;
#endif
struct EiemNativeMeshEntryTraceTarget { char asset[192] = {}; };
// Immutable after the detour is enabled. These names only select diagnostic
// samples; they do not establish resource identity or authorize replacement.
static std::vector<EiemNativeMeshEntryTraceTarget> s_eiemNativeMeshEntryTraceTargets;

static bool EiemNativeMeshEntryIsTraceTarget(const char *name) {
  if (!name || !name[0]) return false;
  for (const auto &target : s_eiemNativeMeshEntryTraceTargets)
    if (_stricmp(target.asset, name) == 0) return true;
  return false;
}

// Read-only helpers are shared by the trace-only and replacement builds. The
// replacement experiment is intentionally disabled in normal builds, but the
// creation-entry probe must still be compilable as an observation-only build.
static bool EiemNativeMeshEntryReadBytes(uintptr_t address, void *out,
                                         size_t size) {
  if (!address || !out || !size) return false;
  __try {
    memcpy(out, reinterpret_cast<const void *>(address), size);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

static uintptr_t EiemNativeMeshEntryReadPtr(uintptr_t base, size_t offset) {
  uintptr_t value = 0;
  EiemNativeMeshEntryReadBytes(base + offset, &value, sizeof(value));
  return value;
}

static uint32_t EiemNativeMeshEntryReadU32(uintptr_t base, size_t offset) {
  uint32_t value = 0;
  EiemNativeMeshEntryReadBytes(base + offset, &value, sizeof(value));
  return value;
}

// The +0x30 name field exposes an ASCII pointer for this player build. The
// previous reader used this same offset (decimal 48), but incorrectly assumed
// the MSVC std::string size/capacity layout. This is a representation correction,
// not an offset correction.
static bool EiemNativeMeshEntryReadName(uintptr_t native, char *out,
                                        size_t outSize) {
  if (!native || !out || outSize < 2) return false;
  out[0] = '\0';
  const uintptr_t name = EiemNativeMeshEntryReadPtr(native, 0x30);
  if (!name) return false;
  __try {
    size_t i = 0;
    for (; i + 1 < outSize; ++i) {
      const unsigned char c = *(const unsigned char *)(name + i);
      if (c == 0) break;
      if (c < 0x20 || c > 0x7E) {
        out[0] = '\0';
        return false;
      }
      out[i] = (char)c;
    }
    out[i < outSize ? i : outSize - 1] = '\0';
    return i > 0 && i + 1 < outSize;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    out[0] = '\0';
    return false;
  }
}

#if defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD)
static EiemNativeReadWindow EiemNativeMeshReadWindow(uintptr_t stream) {
  EiemNativeReadWindow window;
  // Binary reader (+0x133800's local at rsp+0x90) follows the outer transfer
  // context at rsp+0x60. These offsets are from the verified assembly, not a
  // scan for pointers. Copy values only; never seek/release a provider block.
  unsigned char bytes[64] = {};
  if (!stream || !EiemNativeMeshEntryReadBytes(stream + 0x30, bytes,
                                               sizeof(bytes)))
    return window;
  memcpy(&window.cursor, bytes + 0x00, sizeof(window.cursor));
  memcpy(&window.base, bytes + 0x08, sizeof(window.base));
  memcpy(&window.limit, bytes + 0x10, sizeof(window.limit));
  memcpy(&window.provider, bytes + 0x18, sizeof(window.provider));
  memcpy(&window.blockIndex, bytes + 0x20, sizeof(window.blockIndex));
  memcpy(&window.blockSize, bytes + 0x28, sizeof(window.blockSize));
  memcpy(&window.start, bytes + 0x30, sizeof(window.start));
  memcpy(&window.end, bytes + 0x38, sizeof(window.end));
  EiemNativeReadWindowPosition(&window);
  return window;
}

static void EiemNativeMeshReadLogContext(
    const EiemNativeMeshReadContext &context, uintptr_t result,
    uintptr_t typeOutput, uint8_t readOutput) {
  const uintptr_t unity = reinterpret_cast<uintptr_t>(
      GetModuleHandleW(L"UnityPlayer.dll"));
  // owner+0x60 is the game's object-record table. +0x347BC0 accesses its
  // vector at +0x48/+0x50 and optional synchronization object at +0x38.
  // Without its lock contract we must not inspect a synchronized live table.
  unsigned char table[32] = {};
  uintptr_t lock = 0, begin = 0, end = 0;
  EiemSerializedObjectReadRecord record;
  uintptr_t recordAddress = 0;
  const char *recordFailure = "record-table-unreadable";
  if (EiemNativeMeshEntryReadBytes(context.owner + 0x98, table, sizeof(table))) {
    memcpy(&lock, table + 0, sizeof(lock));
    memcpy(&begin, table + 0x10, sizeof(begin));
    memcpy(&end, table + 0x18, sizeof(end));
    recordFailure = lock ? "record-lock-contract-unverified"
                         : EiemFindSerializedObjectReadRecord(
                               begin, end, context.objectKey,
                               EiemNativeMeshEntryReadBytes, &record,
                               &recordAddress);
  }
  uint64_t dataStart = 0;
  const bool hasDataStart = EiemNativeMeshEntryReadBytes(
      context.owner + 0x50, &dataStart, sizeof(dataStart));
  const bool recordStartValid = !recordFailure && hasDataStart &&
      record.offset <= (std::numeric_limits<uint64_t>::max)() - dataStart;
  const uint64_t recordStart = recordStartValid ? dataStart + record.offset : 0;
  const bool consumedValid = context.before.valid && context.after.valid &&
      context.after.position >= context.before.position;
  const uint64_t consumed = consumedValid
      ? context.after.position - context.before.position : 0;
  const bool declaredRangeValid = context.before.valid &&
      context.before.end >= context.before.start;
  const uint64_t declaredLength = declaredRangeValid
      ? context.before.end - context.before.start : 0;
  const uintptr_t ownerProvider = EiemNativeMeshEntryReadPtr(context.owner, 0x168);
  const uintptr_t providerVtable = EiemNativeMeshEntryReadPtr(ownerProvider, 0);
  const uintptr_t vtableRva = unity && providerVtable >= unity
      ? providerVtable - unity : 0;
  char resourceFile[1024] = {};
  const char *fileFailure = "provider-type-unverified";
  if (unity && vtableRva == kEiemNativeMeshFileProviderVtableRva &&
      ownerProvider <= UINTPTR_MAX - 0x30) {
    // +0x1BF710 copies its file input string to +0x30. Its vtable read
    // methods forward that same value to the game's file-read service.
    fileFailure = EiemReadPlayerFileString(
        ownerProvider + 0x30, EiemNativeMeshEntryReadBytes,
        resourceFile, sizeof(resourceFile));
  }
  Log("[MESH-READ-OWNER] ordinal=%ld target=%d name=%s native=%p "
      "owner=%p objectKey=%lld keyMeaning=unverified flags=0x%X "
      "callerRva=0x%llX stream=%p nestedObject=%d result=0x%llX "
      "readOutput=%u typeOutput=%p recordStatus=%s record=%p "
      "dataStart=%llu recordOffset=%llu recordLength=%u typeIndex=%u "
      "recordStartMatch=%d recordLengthMatch=%d provider=%p "
      "ownerProvider=%p providerVtableRva=0x%llX providerMatch=%d "
      "rangeStart=%llu rangeEnd=%llu beforePos=%llu afterPos=%llu "
      "beforeValid=%d afterValid=%d consumedValid=%d consumed=%llu "
      "rangeLength=%llu fullRangeConsumed=%d blockBefore=%d blockAfter=%d "
      "blockSize=%llu resourcePath=unresolved resourceFileStatus=%s "
      "resourceFile=\"%s\" tid=%lu",
      context.ordinal, context.declaredTarget ? 1 : 0, context.name,
      (void *)context.native, (void *)context.owner, (long long)context.objectKey,
      (unsigned)context.flags,
      (unsigned long long)(unity && context.caller >= unity
                              ? context.caller - unity : 0),
      (void *)context.stream, context.nestedObject ? 1 : 0,
      (unsigned long long)result, (unsigned)readOutput, (void *)typeOutput,
      recordFailure ? recordFailure : "ok", (void *)recordAddress,
      (unsigned long long)dataStart, (unsigned long long)record.offset,
      record.length, record.typeIndex,
      recordStartValid && context.before.valid
          ? (recordStart == context.before.start ? 1 : 0) : -1,
      !recordFailure && declaredRangeValid
          ? (record.length == declaredLength ? 1 : 0) : -1,
      (void *)context.before.provider, (void *)ownerProvider,
      (unsigned long long)vtableRva,
      ownerProvider && context.before.provider
          ? (ownerProvider == context.before.provider ? 1 : 0) : -1,
      (unsigned long long)context.before.start,
      (unsigned long long)context.before.end,
      (unsigned long long)context.before.position,
      (unsigned long long)context.after.position,
      context.before.valid ? 1 : 0, context.after.valid ? 1 : 0,
      consumedValid ? 1 : 0, (unsigned long long)consumed,
      (unsigned long long)declaredLength,
      consumedValid && declaredRangeValid
          ? (context.before.position == context.before.start &&
             context.after.start == context.before.start &&
             context.after.end == context.before.end &&
             context.after.position == context.before.end ? 1 : 0) : -1,
      context.before.blockIndex, context.after.blockIndex,
      (unsigned long long)context.before.blockSize,
      fileFailure ? fileFailure : "ok", resourceFile,
      (unsigned long)GetCurrentThreadId());
}

static uintptr_t __fastcall EiemNativeObjectReadHook(
    uintptr_t owner, int64_t objectKey, uintptr_t unused, uint8_t flags,
    uintptr_t *typeOutput, uint8_t *readOutput, uintptr_t native) {
  if (!s_eiemNativeObjectReadOriginal) return 0;
  const uintptr_t unity = reinterpret_cast<uintptr_t>(
      GetModuleHandleW(L"UnityPlayer.dll"));
  const bool isMesh = unity && EiemNativeMeshEntryReadPtr(native, 0) ==
                                  unity + kEiemNativeMeshVtableRva;
  EiemNativeMeshReadContext context;
  context.owner = owner;
  context.native = native;
  context.objectKey = objectKey;
  context.caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  context.flags = flags;
  EiemNativeMeshReadContext *previous = s_eiemNativeMeshReadContext;
  context.nestedObject = previous != nullptr;
  // Mask a parent Mesh transaction for non-Mesh nested reads as well. Match
  // the exact native pointer at the inner entry; thread order alone is not proof.
  s_eiemNativeMeshReadContext = isMesh ? &context : nullptr;
  uintptr_t result = 0;
  __try {
    result = s_eiemNativeObjectReadOriginal(
        owner, objectKey, unused, flags, typeOutput, readOutput, native);
  } __finally {
    s_eiemNativeMeshReadContext = previous;
  }
  if (context.observed) {
    uintptr_t typeValue = 0;
    uint8_t readValue = 0;
    EiemNativeMeshEntryReadBytes((uintptr_t)typeOutput, &typeValue, sizeof(typeValue));
    EiemNativeMeshEntryReadBytes((uintptr_t)readOutput, &readValue, sizeof(readValue));
    EiemNativeMeshReadLogContext(context, result, typeValue, readValue);
  }
  return result;
}
#endif

#if defined(EIEM_NATIVE_MESH_ENTRY_REPLACE_BUILD)
struct EiemNativeMeshEntryApplied {
  uintptr_t native = 0;
  LONG generation = 0;
  char path[kEiemResourceDiskPathCapacity] = {};
  std::shared_ptr<const EiemSkinIdentity> skin;
  std::shared_ptr<EiemMeshSubmeshVisibilityState> visibility;
};

static SRWLOCK s_eiemNativeMeshEntryAppliedLock = SRWLOCK_INIT;
static std::vector<EiemNativeMeshEntryApplied> s_eiemNativeMeshEntryApplied;
static std::vector<uintptr_t> s_eiemNativeMeshEntryAttempted;
static int s_eiemNativeMeshEntryCachedPtrOffset = -2;
static thread_local bool s_eiemNativeMeshEntryReplacing = false;

struct EiemNativeMeshEntryRuleCache {
  char asset[192] = {};
  char path[kEiemResourceDiskPathCapacity] = {};
  EiemModResource resource = {};
};
static SRWLOCK s_eiemNativeMeshEntryRuleCacheLock = SRWLOCK_INIT;
static LONG s_eiemNativeMeshEntryRuleCacheGeneration = -1;
static std::vector<EiemNativeMeshEntryRuleCache> s_eiemNativeMeshEntryRuleCache;

static bool EiemNativeMeshEntryWritePtr(uintptr_t address, uintptr_t value) {
  if (!address) return false;
  __try {
    memcpy(reinterpret_cast<void *>(address), &value, sizeof(value));
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

#endif // EIEM_NATIVE_MESH_ENTRY_REPLACE_BUILD

static void EiemNativeMeshEntryLogState(const char *phase, uintptr_t native,
                                        uintptr_t stream, uintptr_t result,
                                        LONG ordinal, uintptr_t caller) {
  const uintptr_t unity = reinterpret_cast<uintptr_t>(
      GetModuleHandleW(L"UnityPlayer.dll"));
  const uintptr_t vtable = EiemNativeMeshEntryReadPtr(native, 0);
  const uintptr_t vtableRva = unity && vtable >= unity ? vtable - unity : 0;
  const uintptr_t data = EiemNativeMeshEntryReadPtr(native, 56);
  const uintptr_t asyncData = EiemNativeMeshEntryReadPtr(native, 384);
  const uintptr_t data400 = EiemNativeMeshEntryReadPtr(data, 400);
  const uint32_t flags120 = EiemNativeMeshEntryReadU32(native, 120);
  const uint32_t flags124 = EiemNativeMeshEntryReadU32(native, 124);
  const uint32_t flags128 = EiemNativeMeshEntryReadU32(native, 128);
  const uint32_t flags154 = EiemNativeMeshEntryReadU32(native, 340);
  char name[192] = {};
  const bool hasName = EiemNativeMeshEntryReadName(native, name, sizeof(name));
  char streamPath[768] = {};
  TraceLookupStreamPath(reinterpret_cast<void *>(stream), streamPath,
                         (int)sizeof(streamPath));
  // The first qword observed here is 0x800101, not a vtable pointer.
  const uintptr_t streamHeader = EiemNativeMeshEntryReadPtr(stream, 0);
  const uintptr_t streamCursor = EiemNativeMeshEntryReadPtr(stream, 0x30);
  const uintptr_t streamBase = EiemNativeMeshEntryReadPtr(stream, 0x38);
  const uintptr_t streamLimit = EiemNativeMeshEntryReadPtr(stream, 0x40);
  const uintptr_t callerRva = unity && caller >= unity ? caller - unity : 0;
  Log("[MESH-ENTRY] phase=%s ordinal=%ld native=%p stream=%p result=%p "
      "name=%s nameOk=%d vtable=%p vtableRva=0x%llX data=%p data400=%p "
      "asyncData=%p streamPath=\"%s\" streamHeader=0x%llX streamCursor=%p "
      "streamBase=%p streamLimit=%p caller=%p callerRva=0x%llX "
      "fields=120:0x%X,124:0x%X,128:0x%X,154:0x%X tid=%lu",
      phase ? phase : "unknown", ordinal,
      reinterpret_cast<void *>(native), reinterpret_cast<void *>(stream),
      reinterpret_cast<void *>(result), hasName ? name : "<unreadable>",
      hasName ? 1 : 0, reinterpret_cast<void *>(vtable),
      static_cast<unsigned long long>(vtableRva),
      reinterpret_cast<void *>(data), reinterpret_cast<void *>(data400),
      reinterpret_cast<void *>(asyncData), streamPath[0] ? streamPath : "?",
      static_cast<unsigned long long>(streamHeader),
      reinterpret_cast<void *>(streamCursor), reinterpret_cast<void *>(streamBase),
      reinterpret_cast<void *>(streamLimit), reinterpret_cast<void *>(caller),
      static_cast<unsigned long long>(callerRva), flags120, flags124, flags128,
      flags154, static_cast<unsigned long>(GetCurrentThreadId()));
}

#if defined(EIEM_NATIVE_MESH_ENTRY_REPLACE_BUILD)
static int EiemNativeMeshEntryCachedPtrFieldOffset() {
  if (s_eiemNativeMeshEntryCachedPtrOffset != -2)
    return s_eiemNativeMeshEntryCachedPtrOffset;
  const char *fieldName = nullptr;
  const char *names[] = {"m_CachedPtr"};
  const int offset = FindFieldInHierarchy(s_eiemMeshClass, names, 1,
                                           &fieldName);
  if (offset < 0 || offset > 0x200 || (offset % (int)sizeof(void *)) != 0) {
    s_eiemNativeMeshEntryCachedPtrOffset = -1;
    Log("[MESH-ENTRY-FAIL] cached-ptr-field unresolved class=%p offset=%d",
        s_eiemMeshClass, offset);
    return -1;
  }
  s_eiemNativeMeshEntryCachedPtrOffset = offset;
  Log("[MESH-ENTRY] cached-ptr-field name=%s offset=0x%X class=%p",
      fieldName ? fieldName : "<unknown>", offset, s_eiemMeshClass);
  return offset;
}

static bool EiemNativeMeshEntryResolvePath(
    uintptr_t native, char *nameOut, size_t nameSize, char *pathOut,
    size_t pathSize, EiemModResource *resourceOut) {
  if (!EiemNativeMeshEntryReadName(native, nameOut, nameSize)) {
    Log("[MESH-ENTRY-FAIL] native=%p reason=name-unreadable", (void *)native);
    return false;
  }
  const LONG generation = InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  AcquireSRWLockShared(&s_eiemNativeMeshEntryRuleCacheLock);
  const bool cacheReady = s_eiemNativeMeshEntryRuleCacheGeneration == generation;
  ReleaseSRWLockShared(&s_eiemNativeMeshEntryRuleCacheLock);
  if (!cacheReady) {
    std::vector<EiemModRule> rules;
    EiemFindStandaloneRenderRules(&rules);
    std::vector<EiemNativeMeshEntryRuleCache> rebuilt;
    for (const auto &rule : rules) {
      if (!rule.hasMesh) continue;
      EiemModResource resource = {};
      if (!EiemFindModResource(rule.modPath, rule.mesh, "Mesh", &resource))
        continue;
      EiemNativeMeshEntryRuleCache item;
      strncpy_s(item.asset, sizeof(item.asset), rule.asset, _TRUNCATE);
      if (!EiemResolveResourceDiskPath(resource, item.path,
                                       sizeof(item.path)))
        continue;
      item.resource = resource;
      rebuilt.push_back(std::move(item));
    }
    AcquireSRWLockExclusive(&s_eiemNativeMeshEntryRuleCacheLock);
    if (s_eiemNativeMeshEntryRuleCacheGeneration != generation) {
      s_eiemNativeMeshEntryRuleCache = std::move(rebuilt);
      s_eiemNativeMeshEntryRuleCacheGeneration = generation;
      Log("[MESH-ENTRY] rule-cache generation=%ld entries=%zu",
          (long)generation, s_eiemNativeMeshEntryRuleCache.size());
    }
    ReleaseSRWLockExclusive(&s_eiemNativeMeshEntryRuleCacheLock);
  }
  EiemModResource selected = {};
  char selectedPath[kEiemResourceDiskPathCapacity] = {};
  size_t matches = 0;
  AcquireSRWLockShared(&s_eiemNativeMeshEntryRuleCacheLock);
  for (const auto &item : s_eiemNativeMeshEntryRuleCache) {
    if (_stricmp(item.asset, nameOut) != 0) continue;
    if (!matches) {
      selected = item.resource;
      strncpy_s(selectedPath, sizeof(selectedPath), item.path, _TRUNCATE);
    } else if (_stricmp(selectedPath, item.path) != 0) {
      Log("[MESH-ENTRY-FAIL] native=%p name=%s reason=ambiguous-rules "
          "first=%s other=%s", (void *)native, nameOut, selectedPath,
          item.path);
      ReleaseSRWLockShared(&s_eiemNativeMeshEntryRuleCacheLock);
      return false;
    }
    ++matches;
  }
  ReleaseSRWLockShared(&s_eiemNativeMeshEntryRuleCacheLock);
  if (!matches) return false;
  strncpy_s(pathOut, pathSize, selectedPath, _TRUNCATE);
  if (resourceOut) *resourceOut = selected;
  Log("[MESH-ENTRY] match native=%p name=%s path=%s matches=%zu", (void *)native,
      nameOut, pathOut, matches);
  return true;
}

static bool EiemNativeMeshEntryCreateAlias(uintptr_t native, void **aliasOut,
                                           uintptr_t *oldNativeOut,
                                           int *cachedOffsetOut) {
  if (aliasOut) *aliasOut = nullptr;
  if (oldNativeOut) *oldNativeOut = 0;
  if (cachedOffsetOut) *cachedOffsetOut = -1;
  if (!native || !s_eiemMeshClass || !s_eiemMeshCtor ||
      !il2cpp_object_new || !il2cpp_runtime_invoke)
    return false;
  const int offset = EiemNativeMeshEntryCachedPtrFieldOffset();
  if (offset < 0) return false;
  void *alias = il2cpp_object_new(s_eiemMeshClass);
  if (!alias) {
    Log("[MESH-ENTRY-FAIL] native=%p reason=managed-alias-allocation",
        (void *)native);
    return false;
  }
  void *exception = nullptr;
  il2cpp_runtime_invoke(s_eiemMeshCtor, alias, nullptr, &exception);
  if (exception) {
    Log("[MESH-ENTRY-FAIL] native=%p alias=%p reason=managed-alias-ctor "
        "exception=%p", (void *)native, alias, exception);
    return false;
  }
  void *aliasNativeObject = EiemGetNativeMeshPointer(alias);
  uintptr_t oldNative = EiemNativeMeshEntryReadPtr((uintptr_t)alias, offset);
  if (!aliasNativeObject || !oldNative ||
      oldNative != reinterpret_cast<uintptr_t>(aliasNativeObject)) {
    Log("[MESH-ENTRY-FAIL] native=%p alias=%p reason=alias-cached-ptr-mismatch "
        "field=%p getter=%p", (void *)native, alias, (void *)oldNative,
        aliasNativeObject);
    return false;
  }
  if (!EiemNativeMeshEntryWritePtr((uintptr_t)alias + offset, native) ||
      reinterpret_cast<uintptr_t>(EiemGetNativeMeshPointer(alias)) != native) {
    Log("[MESH-ENTRY-FAIL] native=%p alias=%p reason=alias-pointer-swap-rejected "
        "offset=0x%X", (void *)native, alias, offset);
    EiemNativeMeshEntryWritePtr((uintptr_t)alias + offset, oldNative);
    return false;
  }
  if (aliasOut) *aliasOut = alias;
  if (oldNativeOut) *oldNativeOut = oldNative;
  if (cachedOffsetOut) *cachedOffsetOut = offset;
  Log("[MESH-ENTRY] alias-ready native=%p alias=%p oldNative=%p offset=0x%X",
      (void *)native, alias, (void *)oldNative, offset);
  return true;
}

static bool EiemNativeMeshEntryMarkAttempted(uintptr_t native) {
  bool already = false;
  AcquireSRWLockExclusive(&s_eiemNativeMeshEntryAppliedLock);
  for (uintptr_t value : s_eiemNativeMeshEntryAttempted)
    already = already || value == native;
  if (!already) s_eiemNativeMeshEntryAttempted.push_back(native);
  ReleaseSRWLockExclusive(&s_eiemNativeMeshEntryAppliedLock);
  return !already;
}

static bool EiemNativeMeshEntryTryReplace(uintptr_t native) {
  if (!native || s_eiemNativeMeshEntryReplacing || !EiemOnUnityThread())
    return false;
  char name[192] = {};
  char path[kEiemResourceDiskPathCapacity] = {};
  EiemModResource resource = {};
  if (!EiemNativeMeshEntryResolvePath(native, name, sizeof(name), path,
                                      sizeof(path), &resource))
    return false;
  // Only remember native objects that actually match a declared replacement;
  // every unrelated Mesh finalizer must stay on the zero-cost path.
  if (!EiemNativeMeshEntryMarkAttempted(native)) return false;
  EiemNativeMeshDocument document;
  char error[256] = {};
  if (!EiemReadNativeMesh(path, &document, error, sizeof(error))) {
    Log("[MESH-ENTRY-FAIL] native=%p name=%s path=%s reason=payload-read %s",
        (void *)native, name, path, error[0] ? error : "unknown");
    return false;
  }
  std::shared_ptr<const EiemSkinIdentity> skin;
  if (!EiemBuildInPlaceSkinIdentity(document, &skin, error, sizeof(error))) {
    Log("[MESH-ENTRY-FAIL] native=%p path=%s reason=skin-identity %s",
        (void *)native, path, error[0] ? error : "unknown");
    return false;
  }
  void *alias = nullptr;
  uintptr_t oldNative = 0;
  int cachedOffset = -1;
  if (!EiemNativeMeshEntryCreateAlias(native, &alias, &oldNative,
                                      &cachedOffset))
    return false;
  s_eiemNativeMeshEntryReplacing = true;
  std::shared_ptr<EiemMeshSubmeshVisibilityState> visibility;
  const bool applied = EiemApplyNativeMeshDocumentInPlace(
      alias, document, path, &visibility, error, sizeof(error));
  s_eiemNativeMeshEntryReplacing = false;
  const bool restored = EiemNativeMeshEntryWritePtr(
      (uintptr_t)alias + cachedOffset, oldNative);
  const bool aliasRestored = restored &&
      reinterpret_cast<uintptr_t>(EiemGetNativeMeshPointer(alias)) == oldNative;
  if (!applied || !aliasRestored) {
    Log("[MESH-ENTRY-FAIL] native=%p alias=%p path=%s reason=%s restored=%d "
        "aliasRestored=%d", (void *)native, alias, path,
        applied ? "alias-restore-failed" : (error[0] ? error : "setter-write"),
        restored ? 1 : 0, aliasRestored ? 1 : 0);
    return false;
  }
  EiemNativeMeshEntryApplied record;
  record.native = native;
  record.generation = InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  strncpy_s(record.path, sizeof(record.path), path, _TRUNCATE);
  record.skin = std::move(skin);
  record.visibility = std::move(visibility);
  AcquireSRWLockExclusive(&s_eiemNativeMeshEntryAppliedLock);
  s_eiemNativeMeshEntryApplied.push_back(std::move(record));
  ReleaseSRWLockExclusive(&s_eiemNativeMeshEntryAppliedLock);
  Log("[MESH-ENTRY] applied native=%p name=%s path=%s generation=%ld; "
      "original-finalizer-will-run", (void *)native, name, path,
      (long)InterlockedCompareExchange(&s_eiemModGeneration, 0, 0));
  return true;
}

static bool EiemNativeMeshEntryLookupApplied(
    void *templateMesh, const char *path,
    std::shared_ptr<const EiemSkinIdentity> *skin,
    std::shared_ptr<EiemMeshSubmeshVisibilityState> *visibility) {
  if (skin) skin->reset();
  if (visibility) visibility->reset();
  if (!templateMesh || !path) return false;
  const uintptr_t native = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(templateMesh));
  const LONG generation = InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  bool found = false;
  AcquireSRWLockShared(&s_eiemNativeMeshEntryAppliedLock);
  for (const auto &entry : s_eiemNativeMeshEntryApplied) {
    if (entry.native != native || entry.generation != generation ||
        _stricmp(entry.path, path) != 0)
      continue;
    if (skin) *skin = entry.skin;
    if (visibility) *visibility = entry.visibility;
    found = true;
    break;
  }
  ReleaseSRWLockShared(&s_eiemNativeMeshEntryAppliedLock);
  return found;
}
#else
static bool EiemNativeMeshEntryLookupApplied(
    void *, const char *, std::shared_ptr<const EiemSkinIdentity> *,
    std::shared_ptr<EiemMeshSubmeshVisibilityState> *) { return false; }
#endif

static uintptr_t EiemNativeMeshEntryModuleBase() {
  return reinterpret_cast<uintptr_t>(GetModuleHandleW(L"UnityPlayer.dll"));
}

static uintptr_t __fastcall EiemNativeMeshDeserializeHook(uintptr_t native,
                                                            uintptr_t stream) {
  if (!s_eiemNativeMeshDeserializeOriginal) return 0;
  if (s_eiemNativeMeshEntryInHook &&
      s_eiemNativeMeshEntryActiveNative == native)
    return s_eiemNativeMeshDeserializeOriginal(native, stream);
  const bool previousInHook = s_eiemNativeMeshEntryInHook;
  const uintptr_t previousNative = s_eiemNativeMeshEntryActiveNative;
  s_eiemNativeMeshEntryInHook = true;
  s_eiemNativeMeshEntryActiveNative = native;
  const LONG ordinal = InterlockedIncrement(&s_eiemNativeMeshEntryTraceCount);
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  const uintptr_t beforeCursor = EiemNativeMeshEntryReadPtr(stream, 0x30);
  const uintptr_t beforeBase = EiemNativeMeshEntryReadPtr(stream, 0x38);
  const uintptr_t beforeLimit = EiemNativeMeshEntryReadPtr(stream, 0x40);
#if defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD)
  const EiemNativeReadWindow beforeWindow = EiemNativeMeshReadWindow(stream);
#endif
  if (ordinal <= kEiemNativeMeshEntryTraceLimit)
    EiemNativeMeshEntryLogState("deserialize-before", native, stream, 0,
                                ordinal, caller);
  uintptr_t result = 0;
  __try {
    result = s_eiemNativeMeshDeserializeOriginal(native, stream);
  } __finally {
    s_eiemNativeMeshEntryInHook = previousInHook;
    s_eiemNativeMeshEntryActiveNative = previousNative;
  }
  char name[192] = {};
  const bool target = EiemNativeMeshEntryReadName(native, name, sizeof(name)) &&
                      EiemNativeMeshEntryIsTraceTarget(name);
#if defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD)
  if (ordinal <= kEiemNativeMeshEntryTraceLimit || target) {
    auto *context = s_eiemNativeMeshReadContext;
    if (context && context->native == native) {
      context->stream = stream;
      context->ordinal = ordinal;
      context->observed = true;
      context->declaredTarget = target;
      strncpy_s(context->name, sizeof(context->name), name, _TRUNCATE);
      context->before = beforeWindow;
      context->after = EiemNativeMeshReadWindow(stream);
    } else if (target || InterlockedIncrement(
                   &s_eiemNativeMeshReadMissingContextCount) <= 8) {
      Log("[MESH-READ-FAIL] ordinal=%ld target=%d native=%p name=%s "
          "reason=%s contextNative=%p callerRva=0x%llX",
          ordinal, target ? 1 : 0, (void *)native, name,
          context ? "native-context-mismatch" : "outer-context-missing",
          context ? (void *)context->native : nullptr,
          (unsigned long long)(caller - EiemNativeMeshEntryModuleBase()));
    }
  }
#endif
  if (ordinal <= kEiemNativeMeshEntryTraceLimit || target) {
    EiemNativeMeshEntryLogState("deserialize-after", native, stream, result,
                                ordinal, caller);
    if (target)
      Log("[MESH-ENTRY-TARGET] ordinal=%ld native=%p name=%s stream=%p "
          "beforeCursor=%p beforeBase=%p beforeLimit=%p caller=%p",
          ordinal, (void *)native, name, (void *)stream, (void *)beforeCursor,
          (void *)beforeBase, (void *)beforeLimit, (void *)caller);
  }
  return result;
}

static uintptr_t __fastcall EiemNativeMeshFinalizerHook(uintptr_t native) {
  if (!s_eiemNativeMeshFinalizerOriginal) return 0;
  if (s_eiemNativeMeshFinalizerInHook)
    return s_eiemNativeMeshFinalizerOriginal(native);
  s_eiemNativeMeshFinalizerInHook = true;
  const LONG ordinal = InterlockedIncrement(&s_eiemNativeMeshFinalizerTraceCount);
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
#if defined(EIEM_NATIVE_MESH_ENTRY_REPLACE_BUILD)
  EiemNativeMeshEntryTryReplace(native);
#endif
  if (ordinal <= kEiemNativeMeshEntryTraceLimit)
    EiemNativeMeshEntryLogState("finalizer-before", native, 0, 0, ordinal,
                                caller);
  uintptr_t result = s_eiemNativeMeshFinalizerOriginal(native);
  if (ordinal <= kEiemNativeMeshEntryTraceLimit)
    EiemNativeMeshEntryLogState("finalizer-after", native, 0, result, ordinal,
                                caller);
  s_eiemNativeMeshFinalizerInHook = false;
  return result;
}

#if defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD)
static bool EiemNativeMeshReadVerifyPlayer(uintptr_t base) {
  IMAGE_DOS_HEADER dos = {};
  IMAGE_NT_HEADERS64 nt = {};
  if (!EiemNativeMeshEntryReadBytes(base, &dos, sizeof(dos)) ||
      dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 ||
      dos.e_lfanew > 0x1000 ||
      !EiemNativeMeshEntryReadBytes(base + dos.e_lfanew, &nt, sizeof(nt)) ||
      nt.Signature != IMAGE_NT_SIGNATURE ||
      nt.FileHeader.TimeDateStamp != 0x6A85914F ||
      nt.OptionalHeader.SizeOfImage != 0x208A000) {
    Log("[MESH-READ-FAIL] phase=install reason=player-version-mismatch");
    return false;
  }
  struct Signature { uintptr_t rva; size_t size; unsigned char bytes[32]; };
  // Gate both hook ABIs and the internal layouts used for our read-only math.
  static const Signature signatures[] = {
      {0x133800, 18, {0x48,0x89,0x54,0x24,0x10,0x55,0x53,0x56,0x41,
                      0x55,0x48,0x8D,0xAC,0x24,0x68,0xFF,0xFF,0xFF}},
      {0x1EA2A0, 18, {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,
                      0x10,0x48,0x89,0x7C,0x24,0x18,0x55,0x41,0x56}},
      {0x133AD8, 6, {0xFF,0x90,0x90,0x00,0x00,0x00}},
      {0x133CB0, 11, {0x48,0x69,0xC2,0xD8,0x00,0x00,0x00,0x48,0x03,0x01,0xC3}},
      {0xAF980, 32, {0x80,0x79,0x20,0x01,0x74,0x12,0x4C,0x8B,
                    0x41,0x10,0x48,0x8B,0xC2,0x48,0x8B,0x09,
                    0x48,0x89,0x0A,0x4C,0x89,0x42,0x08,0xC3,
                    0x48,0x0F,0xBE,0x41,0x18,0x41,0xB8,0x18}},
      {0x1BF73D, 17, {0x48,0x8D,0x54,0x24,0x20,0xC6,0x41,0x30,
                     0x00,0xC6,0x41,0x48,0x18,0xC6,0x41,0x50,0x01}},
      {0x1BF7CF, 13, {0x48,0x8D,0x4B,0x30,0xE8,0xA8,0x01,0xEF,
                     0xFF,0x48,0x8B,0xD0,0x48}},
  };
  for (const auto &signature : signatures) {
    unsigned char actual[32] = {};
    if (!EiemNativeMeshEntryReadBytes(base + signature.rva, actual,
                                      signature.size) ||
        memcmp(actual, signature.bytes, signature.size) != 0) {
      Log("[MESH-READ-FAIL] phase=install reason=abi-signature-mismatch rva=0x%llX",
          (unsigned long long)signature.rva);
      return false;
    }
  }
  return true;
}

static bool EiemInstallNativeMeshReadOwnerTrace(uintptr_t base) {
  if (!EiemNativeMeshReadVerifyPlayer(base)) return false;
  void *outer = reinterpret_cast<void *>(base + kEiemNativeObjectReadRva);
  void *inner = reinterpret_cast<void *>(base + kEiemNativeMeshDeserializeRva);
  MH_STATUS status = MH_CreateHook(
      outer, reinterpret_cast<void *>(&EiemNativeObjectReadHook),
      reinterpret_cast<void **>(&s_eiemNativeObjectReadOriginal));
  if (status != MH_OK) {
    Log("[MESH-READ-FAIL] phase=install reason=outer-create status=%d", (int)status);
    return false;
  }
  status = MH_CreateHook(
      inner, reinterpret_cast<void *>(&EiemNativeMeshDeserializeHook),
      reinterpret_cast<void **>(&s_eiemNativeMeshDeserializeOriginal));
  if (status != MH_OK) {
    MH_RemoveHook(outer);
    s_eiemNativeObjectReadOriginal = nullptr;
    Log("[MESH-READ-FAIL] phase=install reason=inner-create status=%d", (int)status);
    return false;
  }
  status = MH_QueueEnableHook(outer);
  if (status == MH_OK) status = MH_QueueEnableHook(inner);
  if (status == MH_OK) status = MH_ApplyQueued();
  if (status != MH_OK) {
    MH_DisableHook(inner);
    MH_DisableHook(outer);
    MH_RemoveHook(inner);
    MH_RemoveHook(outer);
    s_eiemNativeMeshDeserializeOriginal = nullptr;
    s_eiemNativeObjectReadOriginal = nullptr;
    Log("[MESH-READ-FAIL] phase=install reason=paired-enable status=%d", (int)status);
    return false;
  }
  Log("[MESH-ENTRY] deserialize-hooked rva=0x%llX thunk=0x%llX",
      (unsigned long long)kEiemNativeMeshDeserializeRva,
      (unsigned long long)kEiemNativeMeshDeserializeThunkRva);
  Log("[MESH-READ-OWNER] outer-hooked rva=0x%llX "
      "context=thread-local-exact-native-pointer keyMeaning=unverified",
      (unsigned long long)kEiemNativeObjectReadRva);
  Log("[MESH-ENTRY] mode=source-observation-v4 finalizer-hook=off "
      "mod-render-actions=off mesh-clone=off reader-ownership=observe-only");
  return true;
}
#endif

static bool EiemInstallNativeMeshEntryTrace() {
#if defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD)
  if (s_eiemNativeObjectReadOriginal && s_eiemNativeMeshDeserializeOriginal)
    return true;
#endif
  HMODULE unity = GetModuleHandleW(L"UnityPlayer.dll");
  if (!unity) {
    Log("[MESH-ENTRY] UnityPlayer.dll not loaded");
    return false;
  }
  const uintptr_t base = reinterpret_cast<uintptr_t>(unity);
  const uintptr_t deserialize = base + kEiemNativeMeshDeserializeRva;
  const uintptr_t finalizer = base + kEiemNativeMeshFinalizerRva;
  bool any = false;
  std::vector<EiemModRule> traceRules;
  EiemFindStandaloneRenderRules(&traceRules);
  for (const auto &rule : traceRules) {
    if (!rule.hasMesh || !rule.asset[0] ||
        EiemNativeMeshEntryIsTraceTarget(rule.asset)) continue;
    EiemNativeMeshEntryTraceTarget target;
    strncpy_s(target.asset, sizeof(target.asset), rule.asset, _TRUNCATE);
    s_eiemNativeMeshEntryTraceTargets.push_back(target);
  }
  Log("[MESH-ENTRY] trace-targets=%zu unrelated-samples=%ld "
      "declared-target-samples=unlimited",
      s_eiemNativeMeshEntryTraceTargets.size(), kEiemNativeMeshEntryTraceLimit);
#if defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD)
  const bool installed = EiemInstallNativeMeshReadOwnerTrace(base);
  Log("[MESH-ENTRY] install result=%d", installed ? 1 : 0);
  return installed;
#else
  const MH_STATUS deserializeStatus = MH_CreateHook(
      reinterpret_cast<void *>(deserialize),
      reinterpret_cast<void *>(&EiemNativeMeshDeserializeHook),
      reinterpret_cast<void **>(&s_eiemNativeMeshDeserializeOriginal));
  if (deserializeStatus == MH_OK || deserializeStatus == MH_ERROR_ALREADY_CREATED) {
    if (MH_EnableHook(reinterpret_cast<void *>(deserialize)) == MH_OK) {
      Log("[MESH-ENTRY] deserialize-hooked rva=0x%llX thunk=0x%llX",
          (unsigned long long)kEiemNativeMeshDeserializeRva,
          (unsigned long long)kEiemNativeMeshDeserializeThunkRva);
      any = true;
    }
  } else {
    Log("[MESH-ENTRY] deserialize-hook-failed rva=0x%llX status=%d",
        (unsigned long long)kEiemNativeMeshDeserializeRva,
        (int)deserializeStatus);
  }
#if defined(EIEM_NATIVE_MESH_ENTRY_REPLACE_BUILD)
  const MH_STATUS finalizerStatus = MH_CreateHook(
      reinterpret_cast<void *>(finalizer),
      reinterpret_cast<void *>(&EiemNativeMeshFinalizerHook),
      reinterpret_cast<void **>(&s_eiemNativeMeshFinalizerOriginal));
  if (finalizerStatus == MH_OK || finalizerStatus == MH_ERROR_ALREADY_CREATED) {
    if (MH_EnableHook(reinterpret_cast<void *>(finalizer)) == MH_OK) {
      Log("[MESH-ENTRY] finalizer-hooked rva=0x%llX replace=%d",
          (unsigned long long)kEiemNativeMeshFinalizerRva,
#if defined(EIEM_NATIVE_MESH_ENTRY_REPLACE_BUILD)
          1
#else
          0
#endif
      );
      any = true;
    }
  } else {
    Log("[MESH-ENTRY] finalizer-hook-failed rva=0x%llX status=%d",
        (unsigned long long)kEiemNativeMeshFinalizerRva,
        (int)finalizerStatus);
  }
#else
  Log("[MESH-ENTRY] mode=source-observation-v2 finalizer-hook=off "
      "mod-render-actions=off mesh-clone=off");
#endif
  Log("[MESH-ENTRY] install result=%d", any ? 1 : 0);
  return any;
#endif
}

#else
static bool EiemInstallNativeMeshEntryTrace() { return false; }
#endif
