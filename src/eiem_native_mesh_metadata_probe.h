#pragma once

#if defined(EIEM_NATIVE_MESH_METADATA_TRACE_BUILD)

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Observation-only native Mesh provenance and metadata trace.
// The hook set is deliberately small and bounded.  It never changes a Mesh,
// a stream, or a return value.

static constexpr uintptr_t kEiemMetadataMeshVtableRva = 0x19C8230;
// The Mesh vtable's +0x90 slot points to this thunk in the current UnityPlayer
// image.  The thunk is the live serialized-load boundary for native Meshes;
// its implementation may tail-call the lower-level +0x1EA2A0 routine.
static constexpr uintptr_t kEiemMetadataDeserializeThunkRva = 0x4710D0;
// 0x1EA2A0/0x2E9AD0 are lower-level native Mesh transfer routines.  The two
// 0x170A* methods are the "Base" transfer methods used by the serialized
// load/save path.  None is called by EIEM to initialize replacement Meshes.
static constexpr uintptr_t kEiemMetadataSerialize18Rva =
    kEiemMetadataDeserializeThunkRva;
static constexpr uintptr_t kEiemMetadataSerialize19Rva = 0x2E9AD0;
// Generic serialized-resource dispatcher immediately above the Mesh virtual
// deserializer. It still has the resource-loader context and the stream
// record, unlike the two-argument Mesh thunk which only receives
// (nativeMesh, stream).
static constexpr uintptr_t kEiemMetadataDeserializeDispatchRva = 0x133800;
static constexpr uintptr_t kEiemMetadataTransferBaseWriteRva = 0x170A12C;
static constexpr uintptr_t kEiemMetadataTransferBaseReadRva = 0x170A0DC;
static constexpr uintptr_t kEiemMetadataSkinValidationRva = 0x19CE90;
static constexpr size_t kEiemMetadataMaxTargets = 512;
// Read-only first native Mesh object window.  This is deliberately bounded;
// it is a comparison sample, not an assumed complete class layout.
static constexpr size_t kEiemMetadataSnapshotBytes = 0x400;
static constexpr LONG kEiemMetadataMaxSerializeRecords = 768;
static constexpr LONG kEiemMetadataMaxConstructionRecords = 256;

struct EiemMetadataMeshTarget {
  uintptr_t native = 0;
  void *managed = nullptr;
  bool sourceTemplate = false;
  LONG generation = 0;
  char path[384] = {};
  bool sourceSnapshotValid = false;
  uint64_t sourceSnapshotHash = 0;
  unsigned char sourceSnapshot[kEiemMetadataSnapshotBytes] = {};
};

static SRWLOCK s_eiemMetadataTargetsLock = SRWLOCK_INIT;
static EiemMetadataMeshTarget s_eiemMetadataTargets[kEiemMetadataMaxTargets] = {};
static size_t s_eiemMetadataTargetCount = 0;
static volatile LONG s_eiemMetadataSerializeCount = 0;
static volatile LONG s_eiemMetadataDispatchCount = 0;
static volatile LONG s_eiemMetadataConstructionCount = 0;
static volatile LONG s_eiemMetadataValidationCount = 0;
static volatile LONG s_eiemMetadataSnapshotCount = 0;
// Keep recursion guards per hook family.  The resource dispatcher calls the
// Mesh virtual transfer functions while it is itself hooked; sharing one
// guard here would hide the exact nested transfer call we need to observe.
static thread_local bool s_eiemMetadataInTransferHook = false;
static thread_local bool s_eiemMetadataInDispatchHook = false;
static thread_local bool s_eiemMetadataInValidationHook = false;

static uintptr_t EiemMetadataUnityBase() {
  return reinterpret_cast<uintptr_t>(GetModuleHandleW(L"UnityPlayer.dll"));
}

static bool EiemMetadataReadBytes(uintptr_t address, void *out, size_t size) {
  if (!address || !out || !size) return false;
  __try {
    memcpy(out, reinterpret_cast<const void *>(address), size);
    return true;
  }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static uint64_t EiemMetadataHashBytes(const unsigned char *bytes, size_t size) {
  uint64_t hash = 1469598103934665603ull;
  if (!bytes) return hash;
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
  return hash;
}

static bool EiemMetadataCaptureSnapshot(uintptr_t native,
                                        unsigned char *out,
                                        uint64_t *outHash) {
  if (outHash) *outHash = 0;
  if (!native || !out) return false;
  if (!EiemMetadataReadBytes(native, out, kEiemMetadataSnapshotBytes))
    return false;
  if (outHash)
    *outHash = EiemMetadataHashBytes(out, kEiemMetadataSnapshotBytes);
  return true;
}

static void EiemMetadataLogSourceSnapshot(uintptr_t native, const char *label) {
  if (!native) return;
  unsigned char current[kEiemMetadataSnapshotBytes] = {};
  uint64_t currentHash = 0;
  const bool currentValid =
      EiemMetadataCaptureSnapshot(native, current, &currentHash);
  AcquireSRWLockShared(&s_eiemMetadataTargetsLock);
  const EiemMetadataMeshTarget *target = nullptr;
  for (size_t i = 0; i < s_eiemMetadataTargetCount; ++i) {
    if (s_eiemMetadataTargets[i].native == native) {
      target = &s_eiemMetadataTargets[i];
      break;
    }
  }
  if (!target || !target->sourceSnapshotValid || !currentValid) {
    ReleaseSRWLockShared(&s_eiemMetadataTargetsLock);
    Log("[MESH-META-SNAPSHOT] label=%s native=%p baseline=%s current=%s "
        "hash=0x%llX path=<unknown>",
        label ? label : "?", (void *)native,
        target && target->sourceSnapshotValid ? "yes" : "no",
        currentValid ? "yes" : "no",
        (unsigned long long)currentHash);
    return;
  }
  size_t differences = 0;
  char firstChanges[512] = {};
  size_t used = 0;
  for (size_t i = 0; i < kEiemMetadataSnapshotBytes; ++i) {
    if (target->sourceSnapshot[i] == current[i]) continue;
    ++differences;
    if (used + 40 < sizeof(firstChanges) && differences <= 16) {
      const int written = _snprintf_s(
          firstChanges + used, sizeof(firstChanges) - used, _TRUNCATE,
          "%s+0x%03zX:%02X>%02X", used ? "," : "", i,
          (unsigned int)target->sourceSnapshot[i], (unsigned int)current[i]);
      if (written > 0) used += (size_t)written;
    }
  }
  const uint64_t baselineHash = target->sourceSnapshotHash;
  char path[384] = {};
  strncpy_s(path, sizeof(path), target->path, _TRUNCATE);
  ReleaseSRWLockShared(&s_eiemMetadataTargetsLock);
  Log("[MESH-META-SNAPSHOT] label=%s native=%p baselineHash=0x%llX "
      "currentHash=0x%llX changedBytes=%zu first=%s path=%s window=0x%zX",
      label ? label : "?", (void *)native, (unsigned long long)baselineHash,
      (unsigned long long)currentHash, differences,
      firstChanges[0] ? firstChanges : "<none>",
      path[0] ? path : "<none>", kEiemMetadataSnapshotBytes);
}

static bool EiemMetadataIsNativeMesh(uintptr_t native) {
  if (!native) return false;
  const uintptr_t base = EiemMetadataUnityBase();
  uintptr_t vtable = 0;
  if (!base || !EiemMetadataReadBytes(native, &vtable, sizeof(vtable)))
    return false;
  return vtable == base + kEiemMetadataMeshVtableRva;
}

static bool EiemMetadataUnityImageMatches(HMODULE unity) {
  if (!unity) return false;
  __try {
    const auto *base = reinterpret_cast<const unsigned char *>(unity);
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
      return false;
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
        base + static_cast<size_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
      return false;
    return nt->FileHeader.TimeDateStamp == 0x6A85914Fu &&
           nt->OptionalHeader.SizeOfImage == 0x208A000u;
  }
  __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

static int32_t EiemMetadataReadField(uintptr_t native, uintptr_t offset) {
  int32_t value = 0;
  if (!native || !EiemMetadataReadBytes(native + offset, &value, sizeof(value)))
    return 0;
  return value;
}

static uint64_t EiemMetadataReadQword(uintptr_t address) {
  uint64_t value = 0;
  EiemMetadataReadBytes(address, &value, sizeof(value));
  return value;
}

static bool EiemMetadataReadAscii(uintptr_t address, char *out, size_t size) {
  if (!out || size < 2 || !address) return false;
  out[0] = 0;
  __try {
    size_t i = 0;
    for (; i + 1 < size; ++i) {
      const unsigned char c = *(const unsigned char *)(address + i);
      if (c == 0) break;
      if (c < 0x20 || c > 0x7E) {
        out[0] = 0;
        return false;
      }
      out[i] = (char)c;
    }
    out[i < size ? i : size - 1] = 0;
    return i > 0;
  }
  __except (EXCEPTION_EXECUTE_HANDLER) {
    out[0] = 0;
    return false;
  }
}

static void EiemMetadataReadName(uintptr_t native, char *out, size_t size) {
  uintptr_t name = 0;
  if (EiemMetadataReadBytes(native + 0x30, &name, sizeof(name)) &&
      EiemMetadataReadAscii(name, out, size))
    return;
  if (out && size) out[0] = 0;
}

static bool EiemMetadataDescribeTarget(uintptr_t native, char *role,
                                        size_t roleSize, char *path,
                                        size_t pathSize) {
  if (role && roleSize) role[0] = 0;
  if (path && pathSize) path[0] = 0;
  bool found = false;
  AcquireSRWLockShared(&s_eiemMetadataTargetsLock);
  for (size_t i = 0; i < s_eiemMetadataTargetCount; ++i) {
    if (s_eiemMetadataTargets[i].native == native) {
      found = true;
      if (role && roleSize)
        strncpy_s(role, roleSize,
                  s_eiemMetadataTargets[i].sourceTemplate ? "source"
                                                           : "replacement",
                  _TRUNCATE);
      if (path && pathSize)
        strncpy_s(path, pathSize, s_eiemMetadataTargets[i].path, _TRUNCATE);
      break;
    }
  }
  ReleaseSRWLockShared(&s_eiemMetadataTargetsLock);
  return found;
}

// Gate renderer-side probes to source Meshes for which a creation snapshot
// exists. This keeps the diagnostic focused on the requested source object.
static bool EiemMetadataHasSourceSnapshot(uintptr_t native) {
  if (!native) return false;
  bool found = false;
  AcquireSRWLockShared(&s_eiemMetadataTargetsLock);
  for (size_t i = 0; i < s_eiemMetadataTargetCount; ++i) {
    const auto &target = s_eiemMetadataTargets[i];
    if (target.native == native && target.sourceTemplate &&
        target.sourceSnapshotValid) {
      found = true;
      break;
    }
  }
  ReleaseSRWLockShared(&s_eiemMetadataTargetsLock);
  return found;
}

static void EiemNativeMeshMetadataProbeRemember(void *mesh, const char *path,
                                                 bool sourceTemplate) {
  if (!mesh) return;
  const uintptr_t native = reinterpret_cast<uintptr_t>(EiemGetNativeMeshPointer(mesh));
  if (!EiemMetadataIsNativeMesh(native)) return;
  AcquireSRWLockExclusive(&s_eiemMetadataTargetsLock);
  size_t slot = s_eiemMetadataTargetCount;
  for (size_t i = 0; i < s_eiemMetadataTargetCount; ++i) {
    if (s_eiemMetadataTargets[i].native == native) {
      slot = i;
      break;
    }
  }
  if (slot == s_eiemMetadataTargetCount) {
    if (s_eiemMetadataTargetCount >= kEiemMetadataMaxTargets) {
      ReleaseSRWLockExclusive(&s_eiemMetadataTargetsLock);
      return;
    }
    ++s_eiemMetadataTargetCount;
  }
  auto &target = s_eiemMetadataTargets[slot];
  const bool hadSourceSnapshot = target.sourceSnapshotValid;
  target.native = native;
  target.managed = mesh;
  target.sourceTemplate = sourceTemplate;
  target.generation = InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  strncpy_s(target.path, sizeof(target.path), path ? path : "", _TRUNCATE);
  if (sourceTemplate && !hadSourceSnapshot) {
    target.sourceSnapshotValid = EiemMetadataCaptureSnapshot(
        native, target.sourceSnapshot, &target.sourceSnapshotHash);
  }
  const LONG generation = target.generation;
  const bool snapshotValid = target.sourceSnapshotValid;
  const uint64_t snapshotHash = target.sourceSnapshotHash;
  char targetPath[384] = {};
  strncpy_s(targetPath, sizeof(targetPath), target.path, _TRUNCATE);
  ReleaseSRWLockExclusive(&s_eiemMetadataTargetsLock);
  Log("[MESH-META-TARGET] role=%s managed=%p native=%p generation=%ld "
      "field110=%d field124=%d field1C8=%d path=%s",
      sourceTemplate ? "source" : "replacement", mesh, (void *)native,
      (long)generation, EiemMetadataReadField(native, 0x110),
      EiemMetadataReadField(native, 0x124), EiemMetadataReadField(native, 0x1C8),
      targetPath[0] ? targetPath : "<none>");
  if (sourceTemplate && !hadSourceSnapshot) {
    Log("[MESH-META-SNAPSHOT] label=source-ready native=%p valid=%d "
        "hash=0x%llX path=%s window=0x%zX",
        (void *)native, snapshotValid ? 1 : 0,
        (unsigned long long)snapshotHash,
        targetPath[0] ? targetPath : "<none>", kEiemMetadataSnapshotBytes);
  }
}

// Capture a source Mesh at the game's renderer-registration boundary.  This
// is deliberately separate from EiemBuildNativeMesh: that function receives
// the mod's replacement object, while this probe must snapshot the Mesh the
// game created before EIEM writes any replacement.  The caller is expected to
// run before the registration code is allowed to apply mod rules.
static void EiemNativeMeshMetadataProbeObserveSourceUse(void *mesh,
                                                         void *renderer,
                                                         const char *stage) {
  if (!mesh) return;
  const uintptr_t native = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(mesh));
  if (!EiemMetadataIsNativeMesh(native)) return;

  // If the resource loader already registered this exact source object, keep
  // its logical path.  Otherwise create a source-only baseline now, using the
  // first renderer boundary at which the game has finished assembling it.
  if (!EiemMetadataHasSourceSnapshot(native))
    EiemNativeMeshMetadataProbeRemember(mesh, "<renderer-source>", true);

  static volatile LONG s_sourceUseLogs = 0;
  const LONG index = InterlockedIncrement(&s_sourceUseLogs);
  if (index > 512) return;
  char meshName[160] = {};
  EiemMetadataReadName(native, meshName, sizeof(meshName));
  Log("[MESH-META-SOURCE-USE] stage=%s renderer=%p mesh=%p native=%p "
      "sourceSnapshot=%d field110=%d field124=%d field1C8=%d name=%s",
      stage ? stage : "?", renderer, mesh, (void *)native,
      EiemMetadataHasSourceSnapshot(native) ? 1 : 0,
      EiemMetadataReadField(native, 0x110),
      EiemMetadataReadField(native, 0x124),
      EiemMetadataReadField(native, 0x1C8), meshName[0] ? meshName : "<none>");
}

static uintptr_t EiemMetadataFindMeshArgument(const uintptr_t *args, size_t count,
                                               size_t *outIndex) {
  if (outIndex) *outIndex = SIZE_MAX;
  for (size_t i = 0; i < count; ++i) {
    if (EiemMetadataIsNativeMesh(args[i])) {
      if (outIndex) *outIndex = i;
      return args[i];
    }
  }
  return 0;
}

static void EiemMetadataCaptureStack(char *out, size_t size) {
  if (!out || size < 2) return;
  out[0] = 0;
  void *frames[10] = {};
  const USHORT count = CaptureStackBackTrace(2, _countof(frames), frames, nullptr);
  const uintptr_t base = EiemMetadataUnityBase();
  size_t used = 0;
  for (USHORT i = 0; i < count && used + 16 < size; ++i) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(frames[i]);
    const uintptr_t rva = base && address >= base ? address - base : address;
    const int written = _snprintf_s(out + used, size - used, _TRUNCATE,
                                    "%s0x%llX", i ? "," : "",
                                    (unsigned long long)rva);
    if (written <= 0) break;
    used += (size_t)written;
  }
}

static void EiemMetadataLogTransition(const char *kind, const char *label,
                                      uintptr_t native, int before110,
                                      int after110, int before124, int after124,
                                      int before1C8, int after1C8,
                                      size_t argIndex, uintptr_t returnValue) {
  char name[128] = {};
  EiemMetadataReadName(native, name, sizeof(name));
  char role[32] = {};
  char path[384] = {};
  const bool known = EiemMetadataDescribeTarget(native, role, sizeof(role),
                                                path, sizeof(path));
  char stack[256] = {};
  EiemMetadataCaptureStack(stack, sizeof(stack));
  Log("[MESH-META-%s] label=%s native=%p role=%s arg=%zu "
      "before110=%d after110=%d before124=%d after124=%d "
      "before1C8=%d after1C8=%d return=%p name=%s stack=%s path=%s",
      kind ? kind : "CALL", label ? label : "?", (void *)native,
      known ? role : "unknown",
      argIndex, before110, after110, before124, after124, before1C8, after1C8,
      (void *)returnValue, name[0] ? name : "<none>",
      stack[0] ? stack : "<none>", path[0] ? path : "<none>");
}

// Report the complete native-object byte ranges changed by the game's
// deserializer. This is observation-only and bounded; it discovers metadata
// beyond the three hand-picked fields above.
static void EiemMetadataLogTransferDiff(const char *label, uintptr_t native,
                                         const unsigned char *before,
                                         bool beforeValid) {
  if (!native || !before || !beforeValid) return;
  unsigned char after[kEiemMetadataSnapshotBytes] = {};
  uint64_t afterHash = 0;
  if (!EiemMetadataCaptureSnapshot(native, after, &afterHash)) return;
  char name[128] = {};
  EiemMetadataReadName(native, name, sizeof(name));
  if (strncmp(name, "S_actor_", 8) != 0) return;

  char ranges[2048] = {};
  size_t used = 0;
  size_t changed = 0;
  size_t rangeStart = SIZE_MAX;
  size_t rangeEnd = SIZE_MAX;
  auto flushRange = [&]() {
    if (rangeStart == SIZE_MAX) return;
    ++changed;
    if (used + 40 < sizeof(ranges)) {
      const int written = _snprintf_s(
          ranges + used, sizeof(ranges) - used, _TRUNCATE,
          "%s+0x%03zX-0x%03zX", used ? "," : "", rangeStart, rangeEnd);
      if (written > 0) used += (size_t)written;
    }
    rangeStart = SIZE_MAX;
    rangeEnd = SIZE_MAX;
  };
  for (size_t i = 0; i < kEiemMetadataSnapshotBytes; ++i) {
    if (before[i] == after[i]) {
      flushRange();
      continue;
    }
    if (rangeStart == SIZE_MAX) {
      rangeStart = i;
      rangeEnd = i;
    } else {
      rangeEnd = i;
    }
  }
  flushRange();
  Log("[MESH-META-TRANSFER-DIFF] label=%s mesh=%p name=%s changedRanges=%zu "
      "ranges=%s afterHash=0x%llX window=0x%zX",
      label ? label : "?", (void *)native, name[0] ? name : "<none>",
      changed, ranges[0] ? ranges : "<none>", (unsigned long long)afterHash,
      kEiemMetadataSnapshotBytes);
}

using EiemMetadataTwoArgFn = int64_t(__fastcall *)(uintptr_t mesh,
                                                   uintptr_t stream);
static EiemMetadataTwoArgFn s_eiemMetadataTwoArgOriginals[4] = {};
using EiemMetadataDispatchFn = int64_t(__fastcall *)(
    uintptr_t context, uintptr_t stream, uintptr_t record, uintptr_t flags,
    uintptr_t output, uintptr_t cursor, uintptr_t object);
static EiemMetadataDispatchFn s_eiemMetadataDispatchOriginal = nullptr;
using EiemMetadataValidationFn = int64_t(__fastcall *)(uintptr_t, int32_t,
                                                       uintptr_t);
static EiemMetadataValidationFn s_eiemMetadataValidationOriginal = nullptr;

template <int Index>
static int64_t __fastcall EiemMetadataTwoArgHook(uintptr_t mesh,
                                                   uintptr_t stream) {
  auto original = s_eiemMetadataTwoArgOriginals[Index];
  if (!original) return 0;
  if (s_eiemMetadataInTransferHook) return original(mesh, stream);
  s_eiemMetadataInTransferHook = true;
  const uintptr_t native = EiemMetadataIsNativeMesh(mesh) ? mesh : 0;
  unsigned char beforeSnapshot[kEiemMetadataSnapshotBytes] = {};
  const bool beforeSnapshotValid =
      native && EiemMetadataCaptureSnapshot(native, beforeSnapshot, nullptr);
  const LONG transferOrdinal =
      native ? InterlockedIncrement(&s_eiemMetadataSerializeCount) : 0;
  const int before110 = EiemMetadataReadField(native, 0x110);
  const int before124 = EiemMetadataReadField(native, 0x124);
  const int before1C8 = EiemMetadataReadField(native, 0x1C8);
  const int64_t result = original(mesh, stream);
  if (native && transferOrdinal <= kEiemMetadataMaxSerializeRecords) {
    static const char *labels[] = {
        "UnityPlayer+0x170A12C", "UnityPlayer+0x170A0DC",
        "UnityPlayer+0x4710D0(thunk)", "UnityPlayer+0x2E9AD0"};
    char name[128] = {};
    EiemMetadataReadName(native, name, sizeof(name));
    char role[32] = {};
    char path[384] = {};
    const bool known = EiemMetadataDescribeTarget(native, role, sizeof(role),
                                                  path, sizeof(path));
    char stack[256] = {};
    EiemMetadataCaptureStack(stack, sizeof(stack));
    Log("[MESH-META-TRANSFER] label=%s mesh=%p stream=%p role=%s "
        "before110=%d after110=%d before124=%d after124=%d "
        "before1C8=%d after1C8=%d return=%lld "
        "streamQ0=%llX streamQ1=%llX streamQ2=%llX streamQ3=%llX "
        "name=%s path=%s stack=%s",
        labels[Index], (void *)native, (void *)stream,
        known ? role : "unknown", before110,
        EiemMetadataReadField(native, 0x110), before124,
        EiemMetadataReadField(native, 0x124), before1C8,
        EiemMetadataReadField(native, 0x1C8), (long long)result,
        (unsigned long long)EiemMetadataReadQword(stream + 0x00),
        (unsigned long long)EiemMetadataReadQword(stream + 0x08),
        (unsigned long long)EiemMetadataReadQword(stream + 0x10),
        (unsigned long long)EiemMetadataReadQword(stream + 0x18),
        name[0] ? name : "<none>", path[0] ? path : "<none>",
        stack[0] ? stack : "<none>");
    if (transferOrdinal <= 64)
      EiemMetadataLogTransferDiff(labels[Index], native, beforeSnapshot,
                                  beforeSnapshotValid);
  }
  s_eiemMetadataInTransferHook = false;
  return result;
}

// Observation-only bridge for the layer that still knows the resource
// context before it dispatches to Mesh::Deserialize.  The hook does not
// change any argument or return value.  It is intentionally bounded and only
// records calls whose object is a native Unity Mesh, so unrelated serialized
// assets do not fill the log.
static int64_t __fastcall EiemMetadataDispatchHook(
    uintptr_t context, uintptr_t stream, uintptr_t record, uintptr_t flags,
    uintptr_t output, uintptr_t cursor, uintptr_t object) {
  auto original = s_eiemMetadataDispatchOriginal;
  if (!original) return 0;
  if (s_eiemMetadataInDispatchHook)
    return original(context, stream, record, flags, output, cursor, object);

  const bool objectIsMesh = EiemMetadataIsNativeMesh(object);
  uint64_t outputBefore = EiemMetadataReadQword(output);
  uint64_t cursorBefore = EiemMetadataReadQword(cursor);
  const uintptr_t objectVtable = objectIsMesh ? EiemMetadataReadQword(object) : 0;
  const uintptr_t objectSlot78 = objectIsMesh
                                     ? EiemMetadataReadQword(objectVtable + 0x78)
                                     : 0;
  const uintptr_t objectSlot90 = objectIsMesh
                                     ? EiemMetadataReadQword(objectVtable + 0x90)
                                     : 0;
  const int objectBefore110 = EiemMetadataReadField(object, 0x110);
  const int objectBefore124 = EiemMetadataReadField(object, 0x124);
  const int objectBefore1C8 = EiemMetadataReadField(object, 0x1C8);
  int64_t result = 0;
  s_eiemMetadataInDispatchHook = true;
  result = original(context, stream, record, flags, output, cursor, object);
  s_eiemMetadataInDispatchHook = false;

  const bool resultIsMesh = EiemMetadataIsNativeMesh(
      static_cast<uintptr_t>(result));
  if ((objectIsMesh || resultIsMesh) &&
      InterlockedIncrement(&s_eiemMetadataDispatchCount) <= 512) {
    char name[128] = {};
    const uintptr_t mesh = objectIsMesh
                               ? object
                               : static_cast<uintptr_t>(result);
    EiemMetadataReadName(mesh, name, sizeof(name));
    char stack[256] = {};
    EiemMetadataCaptureStack(stack, sizeof(stack));
    Log("[MESH-META-DISPATCH] context=%p stream=%p record=%p flags=%llu "
        "output=%p outputBefore=%p outputAfter=%p cursor=%p "
        "cursorBefore=%p cursorAfter=%p object=%p result=%p "
        "stream30=%p stream38=%p stream40=%p stream48=%p stream50=%p "
        "stream58=%p stream60=%p context20=%p context30=%p context50=%p "
        "vtable=%p slot78=%p slot90=%p "
        "before110=%d before124=%d before1C8=%d "
        "after110=%d after124=%d after1C8=%d name=%s stack=%s",
        (void *)context, (void *)stream, (void *)record,
        static_cast<unsigned long long>(flags), (void *)output,
        (void *)outputBefore, (void *)EiemMetadataReadQword(output),
        (void *)cursor, (void *)cursorBefore, (void *)EiemMetadataReadQword(cursor),
        (void *)object, (void *)result,
        (void *)EiemMetadataReadQword(stream + 0x30),
        (void *)EiemMetadataReadQword(stream + 0x38),
        (void *)EiemMetadataReadQword(stream + 0x40),
        (void *)EiemMetadataReadQword(stream + 0x48),
        (void *)EiemMetadataReadQword(stream + 0x50),
        (void *)EiemMetadataReadQword(stream + 0x58),
        (void *)EiemMetadataReadQword(stream + 0x60),
        (void *)EiemMetadataReadQword(context + 0x20),
        (void *)EiemMetadataReadQword(context + 0x30),
        (void *)EiemMetadataReadQword(context + 0x50),
        (void *)objectVtable, (void *)objectSlot78, (void *)objectSlot90,
        objectBefore110, objectBefore124, objectBefore1C8,
        EiemMetadataReadField(mesh, 0x110), EiemMetadataReadField(mesh, 0x124),
        EiemMetadataReadField(mesh, 0x1C8),
        name[0] ? name : "<none>", stack[0] ? stack : "<none>");
  }
  return result;
}

// Called by EiemBuildNativeMesh at major managed construction boundaries.
// It records snapshots only; it never writes the Mesh or invokes a native
// serializer.  This lets us distinguish Unity setter effects from later game
// initialization effects.
static void EiemNativeMeshMetadataProbeConstructionPhase(
    const char *phase, void *mesh, void *templateMesh, const char *path) {
  if (!mesh || InterlockedIncrement(&s_eiemMetadataConstructionCount) >
                   kEiemMetadataMaxConstructionRecords)
    return;
  const uintptr_t native = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(mesh));
  const uintptr_t templateNative = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(templateMesh));
  char stack[192] = {};
  EiemMetadataCaptureStack(stack, sizeof(stack));
  Log("[MESH-META-CONSTRUCTION] phase=%s mesh=%p native=%p template=%p "
      "templateNative=%p field110=%d field124=%d field1C8=%d "
      "template110=%d template124=%d template1C8=%d path=%s stack=%s",
      phase ? phase : "?", mesh, (void *)native, templateMesh,
      (void *)templateNative, EiemMetadataReadField(native, 0x110),
      EiemMetadataReadField(native, 0x124), EiemMetadataReadField(native, 0x1C8),
      EiemMetadataReadField(templateNative, 0x110),
      EiemMetadataReadField(templateNative, 0x124),
      EiemMetadataReadField(templateNative, 0x1C8), path ? path : "<none>",
      stack[0] ? stack : "<none>");
}

// IDA identified this as the low-frequency renderer-side skin validation /
// record construction boundary.  It is the next boundary after Mesh creation
// where the game can accept, rebuild, or reject private Mesh metadata.  Keep
// the ABI typed (three arguments) and call through unchanged.
static int64_t __fastcall EiemMetadataValidationHook(uintptr_t self,
                                                      int32_t flags,
                                                      uintptr_t context) {
  auto original = s_eiemMetadataValidationOriginal;
  if (!original) return 0;
  if (s_eiemMetadataInValidationHook)
    return original(self, flags, context);
  s_eiemMetadataInValidationHook = true;

  uintptr_t meshBefore = 0;
  EiemMetadataReadBytes(self + 0x298, &meshBefore, sizeof(meshBefore));
  const bool trackedBefore = EiemMetadataIsNativeMesh(meshBefore) &&
                             EiemMetadataDescribeTarget(meshBefore, nullptr,
                                                        0, nullptr, 0);
  const int before110 = EiemMetadataReadField(meshBefore, 0x110);
  const int before124 = EiemMetadataReadField(meshBefore, 0x124);
  const int before1C8 = EiemMetadataReadField(meshBefore, 0x1C8);
  const int64_t result = original(self, flags, context);

  uintptr_t meshAfter = 0;
  EiemMetadataReadBytes(self + 0x298, &meshAfter, sizeof(meshAfter));
  const bool trackedAfter = EiemMetadataIsNativeMesh(meshAfter) &&
                            EiemMetadataDescribeTarget(meshAfter, nullptr, 0,
                                                       nullptr, 0);
  if (trackedBefore || trackedAfter) {
    char role[32] = {};
    char path[384] = {};
    const uintptr_t mesh = trackedBefore ? meshBefore : meshAfter;
    EiemMetadataDescribeTarget(mesh, role, sizeof(role), path, sizeof(path));
    char stack[256] = {};
    EiemMetadataCaptureStack(stack, sizeof(stack));
    const LONG ordinal = InterlockedIncrement(&s_eiemMetadataValidationCount);
    if (ordinal <= 256) {
      Log("[MESH-META-VALIDATION] ordinal=%ld self=%p flags=0x%X "
          "context=%p meshBefore=%p meshAfter=%p role=%s "
          "before110=%d after110=%d before124=%d after124=%d "
          "before1C8=%d after1C8=%d result=%lld path=%s stack=%s",
          ordinal, reinterpret_cast<void *>(self),
          static_cast<unsigned int>(flags), reinterpret_cast<void *>(context),
          reinterpret_cast<void *>(meshBefore),
          reinterpret_cast<void *>(meshAfter), role[0] ? role : "unknown",
          before110, EiemMetadataReadField(meshAfter, 0x110), before124,
          EiemMetadataReadField(meshAfter, 0x124), before1C8,
          EiemMetadataReadField(meshAfter, 0x1C8),
          static_cast<long long>(result), path[0] ? path : "<none>",
          stack[0] ? stack : "<none>");
    }
    const LONG snapshotOrdinal =
        InterlockedIncrement(&s_eiemMetadataSnapshotCount);
    if (snapshotOrdinal <= 256) {
      if (trackedBefore)
        EiemMetadataLogSourceSnapshot(
            meshBefore, "runtime-validation-before");
      if (trackedAfter && meshAfter != meshBefore)
        EiemMetadataLogSourceSnapshot(meshAfter, "runtime-validation-after");
    }
  }
  s_eiemMetadataInValidationHook = false;
  return result;
}

static bool EiemInstallNativeMeshMetadataTrace() {
  HMODULE unity = GetModuleHandleW(L"UnityPlayer.dll");
  if (!unity) {
    Log("[MESH-META] UnityPlayer.dll not loaded");
    return false;
  }
  if (!EiemMetadataUnityImageMatches(unity)) {
    Log("[MESH-META] UnityPlayer image mismatch; trace refused");
    return false;
  }
  const uintptr_t base = reinterpret_cast<uintptr_t>(unity);
  // These four targets all have an IDA-confirmed two-argument ABI.  The
  // earlier generic eight-register writer hooks were removed because a
  // mismatched detour can corrupt the game stack even when no record is
  // emitted.
  const uintptr_t targets[] = {kEiemMetadataTransferBaseWriteRva,
                               kEiemMetadataTransferBaseReadRva,
                               kEiemMetadataSerialize18Rva,
                               kEiemMetadataSerialize19Rva};
  void *detours[] = {(void *)&EiemMetadataTwoArgHook<0>,
                     (void *)&EiemMetadataTwoArgHook<1>,
                     (void *)&EiemMetadataTwoArgHook<2>,
                     (void *)&EiemMetadataTwoArgHook<3>};
  bool any = false;
  for (size_t i = 0; i < _countof(targets); ++i) {
    void *target = reinterpret_cast<void *>(base + targets[i]);
    if (!EiemUnityRvaIsMapped(unity, targets[i], 16)) {
      Log("[MESH-META] transfer rva=0x%llX unmapped", (unsigned long long)targets[i]);
      continue;
    }
    const MH_STATUS status = MH_CreateHook(
        target, detours[i], reinterpret_cast<void **>(&s_eiemMetadataTwoArgOriginals[i]));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
      Log("[MESH-META] transfer rva=0x%llX hook failed status=%d",
          (unsigned long long)targets[i], (int)status);
      continue;
    }
    if (MH_EnableHook(target) != MH_OK) {
      Log("[MESH-META] transfer rva=0x%llX enable failed", (unsigned long long)targets[i]);
      continue;
    }
    Log("[MESH-META] transfer rva=0x%llX hooked", (unsigned long long)targets[i]);
    any = true;
  }
  void *dispatch = reinterpret_cast<void *>(
      base + kEiemMetadataDeserializeDispatchRva);
  if (EiemUnityRvaIsMapped(unity, kEiemMetadataDeserializeDispatchRva, 16)) {
    const MH_STATUS status = MH_CreateHook(
        dispatch, (void *)&EiemMetadataDispatchHook,
        reinterpret_cast<void **>(&s_eiemMetadataDispatchOriginal));
    if (status == MH_OK || status == MH_ERROR_ALREADY_CREATED) {
      if (MH_EnableHook(dispatch) == MH_OK) {
        Log("[MESH-META] dispatch rva=0x%llX hooked",
            (unsigned long long)kEiemMetadataDeserializeDispatchRva);
        any = true;
      }
    } else {
      Log("[MESH-META] dispatch rva=0x%llX hook failed status=%d",
          (unsigned long long)kEiemMetadataDeserializeDispatchRva,
          (int)status);
    }
  } else {
    Log("[MESH-META] dispatch rva=0x%llX unmapped",
        (unsigned long long)kEiemMetadataDeserializeDispatchRva);
  }
  // Serializer hooks are installed above with their exact two-argument ABI.
  Log("[MESH-META] install result=%d", any ? 1 : 0);
  void *validation = reinterpret_cast<void *>(
      base + kEiemMetadataSkinValidationRva);
  if (EiemUnityRvaIsMapped(unity, kEiemMetadataSkinValidationRva, 16)) {
    const MH_STATUS status = MH_CreateHook(
        validation, (void *)&EiemMetadataValidationHook,
        reinterpret_cast<void **>(&s_eiemMetadataValidationOriginal));
    if (status == MH_OK || status == MH_ERROR_ALREADY_CREATED) {
      if (MH_EnableHook(validation) == MH_OK) {
        Log("[MESH-META] validation rva=0x%llX hooked",
            (unsigned long long)kEiemMetadataSkinValidationRva);
        any = true;
      }
    } else {
      Log("[MESH-META] validation hook failed status=%d", (int)status);
    }
  } else {
    Log("[MESH-META] validation rva=0x%llX unmapped",
        (unsigned long long)kEiemMetadataSkinValidationRva);
  }
  return any;
}

#else

static bool EiemInstallNativeMeshMetadataTrace() { return false; }

#endif
