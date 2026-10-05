#pragma once

// Read-only diagnostic for the native state of EIEM-owned Mesh objects.
// This deliberately does not hook Unity internals or write native memory.  It
// records one bounded snapshot after a replacement Mesh has been finalized,
// then compares the same native object when the afterimage proxy submits it.

#if defined(EIEM_NATIVE_MESH_RUNTIME_SNAPSHOT_BUILD)

#include <windows.h>
#include <cstdint>
#include <cstring>

static constexpr uintptr_t kEiemRuntimeSnapshotMeshVtableRva = 0x19C8230;
static constexpr size_t kEiemRuntimeSnapshotBytes = 0x400;
static constexpr size_t kEiemRuntimeSnapshotMaxMeshes = 256;
static constexpr size_t kEiemRuntimeSnapshotMaxDiffs = 32;

struct EiemRuntimeMeshSnapshotEntry {
  uintptr_t native = 0;
  void *managed = nullptr;
  char path[384] = {};
  unsigned char creation[kEiemRuntimeSnapshotBytes] = {};
  uint64_t creationHash = 0;
  uintptr_t sourceNative = 0;
  void *sourceManaged = nullptr;
  unsigned char sourceCreation[kEiemRuntimeSnapshotBytes] = {};
  uint64_t sourceCreationHash = 0;
  bool sourceCreationValid = false;
  bool valid = false;
  LONG runtimeSamples = 0;
};

static SRWLOCK s_eiemRuntimeMeshSnapshotLock = SRWLOCK_INIT;
static EiemRuntimeMeshSnapshotEntry
    s_eiemRuntimeMeshSnapshots[kEiemRuntimeSnapshotMaxMeshes] = {};
static size_t s_eiemRuntimeMeshSnapshotCount = 0;

static bool EiemRuntimeSnapshotRead(uintptr_t address, void *out,
                                    size_t size) {
  if (!address || !out || !size) return false;
  __try {
    memcpy(out, reinterpret_cast<const void *>(address), size);
    return true;
  }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static uint64_t EiemRuntimeSnapshotHash(const unsigned char *bytes,
                                        size_t size) {
  uint64_t hash = 1469598103934665603ull;
  if (!bytes) return hash;
  for (size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ull;
  }
  return hash;
}

static bool EiemRuntimeSnapshotNativeIsMesh(uintptr_t native) {
  if (!native) return false;
  const uintptr_t unity = reinterpret_cast<uintptr_t>(
      GetModuleHandleW(L"UnityPlayer.dll"));
  uintptr_t vtable = 0;
  if (!unity || !EiemRuntimeSnapshotRead(native, &vtable, sizeof(vtable)))
    return false;
  return vtable == unity + kEiemRuntimeSnapshotMeshVtableRva;
}

static bool EiemRuntimeSnapshotCaptureMesh(
    void *mesh, unsigned char *out, uint64_t *outHash,
    uintptr_t *outNative = nullptr) {
  if (outHash) *outHash = 0;
  if (outNative) *outNative = 0;
  if (!mesh || !out) return false;
  const uintptr_t native = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(mesh));
  if (outNative) *outNative = native;
  if (!EiemRuntimeSnapshotNativeIsMesh(native)) return false;
  if (!EiemRuntimeSnapshotRead(native, out, kEiemRuntimeSnapshotBytes))
    return false;
  if (outHash)
    *outHash = EiemRuntimeSnapshotHash(out, kEiemRuntimeSnapshotBytes);
  return true;
}

static void EiemRuntimeSnapshotFormatDiff(
    const unsigned char *creation, const unsigned char *current,
    size_t *outChanged, char *outFirst, size_t outFirstSize) {
  if (outChanged) *outChanged = 0;
  if (outFirst && outFirstSize) outFirst[0] = '\0';
  if (!creation || !current) return;
  size_t changed = 0;
  size_t used = 0;
  for (size_t offset = 0; offset < kEiemRuntimeSnapshotBytes; ++offset) {
    if (creation[offset] == current[offset]) continue;
    ++changed;
    if (outFirst && outFirstSize && changed <= kEiemRuntimeSnapshotMaxDiffs &&
        used + 40 < outFirstSize) {
      const int written = _snprintf_s(
          outFirst + used, outFirstSize - used, _TRUNCATE,
          "%s+0x%03zX:%02X>%02X", used ? "," : "", offset,
          static_cast<unsigned int>(creation[offset]),
          static_cast<unsigned int>(current[offset]));
      if (written > 0) used += static_cast<size_t>(written);
    }
  }
  if (outChanged) *outChanged = changed;
}

static void EiemRuntimeMeshSnapshotRemember(void *mesh, const char *path) {
  if (!mesh) return;
  const uintptr_t native = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(mesh));
  if (!EiemRuntimeSnapshotNativeIsMesh(native)) return;

  unsigned char creation[kEiemRuntimeSnapshotBytes] = {};
  if (!EiemRuntimeSnapshotRead(native, creation, sizeof(creation))) {
    Log("[MESH-RUNTIME-SNAPSHOT] phase=creation mesh=%p native=%p "
        "valid=0 reason=read-failed path=%s",
        mesh, reinterpret_cast<void *>(native), path ? path : "<none>");
    return;
  }
  const uint64_t hash = EiemRuntimeSnapshotHash(creation, sizeof(creation));
  AcquireSRWLockExclusive(&s_eiemRuntimeMeshSnapshotLock);
  size_t slot = s_eiemRuntimeMeshSnapshotCount;
  for (size_t index = 0; index < s_eiemRuntimeMeshSnapshotCount; ++index) {
    if (s_eiemRuntimeMeshSnapshots[index].native == native) {
      slot = index;
      break;
    }
  }
  if (slot == s_eiemRuntimeMeshSnapshotCount) {
    if (s_eiemRuntimeMeshSnapshotCount >= kEiemRuntimeSnapshotMaxMeshes) {
      ReleaseSRWLockExclusive(&s_eiemRuntimeMeshSnapshotLock);
      Log("[MESH-RUNTIME-SNAPSHOT] phase=creation mesh=%p native=%p "
          "valid=0 reason=table-full path=%s",
          mesh, reinterpret_cast<void *>(native), path ? path : "<none>");
      return;
    }
    ++s_eiemRuntimeMeshSnapshotCount;
  }
  auto &entry = s_eiemRuntimeMeshSnapshots[slot];
  entry.native = native;
  entry.managed = mesh;
  entry.creationHash = hash;
  entry.valid = true;
  entry.runtimeSamples = 0;
  strncpy_s(entry.path, sizeof(entry.path), path ? path : "", _TRUNCATE);
  memcpy(entry.creation, creation, sizeof(creation));
  char savedPath[384] = {};
  strncpy_s(savedPath, sizeof(savedPath), entry.path, _TRUNCATE);
  ReleaseSRWLockExclusive(&s_eiemRuntimeMeshSnapshotLock);

  Log("[MESH-RUNTIME-SNAPSHOT] phase=creation mesh=%p native=%p "
      "valid=1 hash=0x%llX bytes=0x%zX path=%s",
      mesh, reinterpret_cast<void *>(native),
      static_cast<unsigned long long>(hash), sizeof(creation),
      savedPath[0] ? savedPath : "<none>");
}

// Records the replacement and its source at the same build boundary.  The
// source sample is intentionally named source-ready: the source Mesh has
// already been created by the game before this function is called, so this is
// not a claim that we intercepted the exact deserialize return point.
static void EiemRuntimeMeshSnapshotRememberWithSource(
    void *replacement, void *source, const char *path) {
  if (!replacement) return;
  unsigned char replacementBytes[kEiemRuntimeSnapshotBytes] = {};
  unsigned char sourceBytes[kEiemRuntimeSnapshotBytes] = {};
  uint64_t replacementHash = 0;
  uint64_t sourceHash = 0;
  uintptr_t replacementNative = 0;
  uintptr_t sourceNative = 0;
  const bool replacementValid = EiemRuntimeSnapshotCaptureMesh(
      replacement, replacementBytes, &replacementHash, &replacementNative);
  const bool sourceValid = source && EiemRuntimeSnapshotCaptureMesh(
                                       source, sourceBytes, &sourceHash,
                                       &sourceNative);
  if (!replacementValid) {
    Log("[MESH-RUNTIME-SNAPSHOT] phase=creation-pair valid=0 "
        "reason=replacement-read-failed replacement=%p source=%p path=%s",
        replacement, source, path ? path : "<none>");
    return;
  }

  AcquireSRWLockExclusive(&s_eiemRuntimeMeshSnapshotLock);
  size_t slot = s_eiemRuntimeMeshSnapshotCount;
  for (size_t index = 0; index < s_eiemRuntimeMeshSnapshotCount; ++index) {
    if (s_eiemRuntimeMeshSnapshots[index].native == replacementNative) {
      slot = index;
      break;
    }
  }
  if (slot == s_eiemRuntimeMeshSnapshotCount) {
    if (s_eiemRuntimeMeshSnapshotCount >= kEiemRuntimeSnapshotMaxMeshes) {
      ReleaseSRWLockExclusive(&s_eiemRuntimeMeshSnapshotLock);
      Log("[MESH-RUNTIME-SNAPSHOT] phase=creation-pair valid=0 "
          "reason=table-full replacement=%p source=%p path=%s",
          replacement, source, path ? path : "<none>");
      return;
    }
    ++s_eiemRuntimeMeshSnapshotCount;
  }
  auto &entry = s_eiemRuntimeMeshSnapshots[slot];
  entry.native = replacementNative;
  entry.managed = replacement;
  entry.creationHash = replacementHash;
  entry.valid = true;
  entry.runtimeSamples = 0;
  strncpy_s(entry.path, sizeof(entry.path), path ? path : "", _TRUNCATE);
  memcpy(entry.creation, replacementBytes, sizeof(entry.creation));
  entry.sourceNative = sourceNative;
  entry.sourceManaged = source;
  entry.sourceCreationHash = sourceHash;
  entry.sourceCreationValid = sourceValid;
  if (sourceValid)
    memcpy(entry.sourceCreation, sourceBytes, sizeof(entry.sourceCreation));
  else
    memset(entry.sourceCreation, 0, sizeof(entry.sourceCreation));
  char savedPath[384] = {};
  strncpy_s(savedPath, sizeof(savedPath), entry.path, _TRUNCATE);
  ReleaseSRWLockExclusive(&s_eiemRuntimeMeshSnapshotLock);

  size_t creationDiff = 0;
  if (sourceValid) {
    for (size_t offset = 0; offset < kEiemRuntimeSnapshotBytes; ++offset)
      creationDiff += replacementBytes[offset] != sourceBytes[offset] ? 1 : 0;
  }
  auto readField = [](const unsigned char *bytes, size_t offset) -> int32_t {
    int32_t value = 0;
    memcpy(&value, bytes + offset, sizeof(value));
    return value;
  };
  Log("[MESH-RUNTIME-SNAPSHOT] phase=creation-pair valid=1 "
      "replacement=%p replacementNative=%p replacementHash=0x%llX "
      "source=%p sourceNative=%p sourceReady=%d sourceHash=0x%llX "
      "creationChangedBytes=%zu replacementFields=110:%d,124:%d,1C8:%d "
      "sourceFields=110:%d,124:%d,1C8:%d path=%s",
      replacement, reinterpret_cast<void *>(replacementNative),
      static_cast<unsigned long long>(replacementHash), source,
      reinterpret_cast<void *>(sourceNative), sourceValid ? 1 : 0,
      static_cast<unsigned long long>(sourceHash), creationDiff,
      readField(replacementBytes, 0x110), readField(replacementBytes, 0x124),
      readField(replacementBytes, 0x1C8), readField(sourceBytes, 0x110),
      readField(sourceBytes, 0x124), readField(sourceBytes, 0x1C8),
      savedPath[0] ? savedPath : "<none>");
}

// Observe the source Mesh at the same runtime boundary as its replacement.
// This closes the old probe's gap, which only tracked replacement creation ->
// replacement runtime changes.
static void EiemRuntimeMeshSnapshotObserveSource(
    void *replacement, void *source, const char *phase) {
  if (!replacement || !source) return;
  const uintptr_t replacementNative = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(replacement));
  if (!EiemRuntimeSnapshotNativeIsMesh(replacementNative)) return;
  unsigned char current[kEiemRuntimeSnapshotBytes] = {};
  uint64_t currentHash = 0;
  uintptr_t sourceNative = 0;
  if (!EiemRuntimeSnapshotCaptureMesh(source, current, &currentHash,
                                      &sourceNative))
    return;

  unsigned char baseline[kEiemRuntimeSnapshotBytes] = {};
  uint64_t baselineHash = 0;
  char path[384] = {};
  bool found = false;
  bool baselineValid = false;
  AcquireSRWLockShared(&s_eiemRuntimeMeshSnapshotLock);
  for (size_t index = 0; index < s_eiemRuntimeMeshSnapshotCount; ++index) {
    const auto &entry = s_eiemRuntimeMeshSnapshots[index];
    if (entry.native != replacementNative || !entry.valid) continue;
    found = true;
    baselineValid = entry.sourceCreationValid && entry.sourceNative == sourceNative;
    if (baselineValid) {
      memcpy(baseline, entry.sourceCreation, sizeof(baseline));
      baselineHash = entry.sourceCreationHash;
    }
    strncpy_s(path, sizeof(path), entry.path, _TRUNCATE);
    break;
  }
  ReleaseSRWLockShared(&s_eiemRuntimeMeshSnapshotLock);
  if (!found || !baselineValid) {
    Log("[MESH-RUNTIME-SOURCE-LIFECYCLE] phase=%s valid=0 "
        "replacement=%p replacementNative=%p source=%p sourceNative=%p "
        "reason=%s path=%s",
        phase ? phase : "?", replacement,
        reinterpret_cast<void *>(replacementNative), source,
        reinterpret_cast<void *>(sourceNative),
        found ? "source-baseline-missing-or-native-changed"
              : "replacement-not-recorded",
        path[0] ? path : "<none>");
    return;
  }
  size_t changed = 0;
  char first[1600] = {};
  EiemRuntimeSnapshotFormatDiff(baseline, current, &changed, first,
                                sizeof(first));
  Log("[MESH-RUNTIME-SOURCE-LIFECYCLE] phase=%s valid=1 "
      "replacement=%p replacementNative=%p source=%p sourceNative=%p "
      "sourceCreationHash=0x%llX sourceCurrentHash=0x%llX "
      "sourceChangedBytes=%zu first=%s path=%s",
      phase ? phase : "?", replacement,
      reinterpret_cast<void *>(replacementNative), source,
      reinterpret_cast<void *>(sourceNative),
      static_cast<unsigned long long>(baselineHash),
      static_cast<unsigned long long>(currentHash), changed,
      first[0] ? first : "<none>", path[0] ? path : "<none>");
}

static void EiemRuntimeMeshSnapshotObserve(void *mesh, const char *phase) {
  if (!mesh) return;
  const uintptr_t native = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(mesh));
  if (!EiemRuntimeSnapshotNativeIsMesh(native)) return;
  unsigned char current[kEiemRuntimeSnapshotBytes] = {};
  if (!EiemRuntimeSnapshotRead(native, current, sizeof(current))) return;

  unsigned char creation[kEiemRuntimeSnapshotBytes] = {};
  uint64_t creationHash = 0;
  char path[384] = {};
  LONG sample = 0;
  bool found = false;
  AcquireSRWLockExclusive(&s_eiemRuntimeMeshSnapshotLock);
  for (size_t index = 0; index < s_eiemRuntimeMeshSnapshotCount; ++index) {
    auto &entry = s_eiemRuntimeMeshSnapshots[index];
    if (entry.native != native || !entry.valid) continue;
    ++entry.runtimeSamples;
    sample = entry.runtimeSamples;
    memcpy(creation, entry.creation, sizeof(creation));
    creationHash = entry.creationHash;
    strncpy_s(path, sizeof(path), entry.path, _TRUNCATE);
    found = true;
    break;
  }
  ReleaseSRWLockExclusive(&s_eiemRuntimeMeshSnapshotLock);
  if (!found || sample > 8) return;

  size_t changed = 0;
  char first[1600] = {};
  EiemRuntimeSnapshotFormatDiff(creation, current, &changed, first,
                                sizeof(first));
  const uint64_t currentHash =
      EiemRuntimeSnapshotHash(current, sizeof(current));
  Log("[MESH-RUNTIME-SNAPSHOT] phase=%s sample=%ld mesh=%p native=%p "
      "creationHash=0x%llX currentHash=0x%llX changedBytes=%zu first=%s "
      "window=0x%zX path=%s",
      phase ? phase : "?", sample, mesh, reinterpret_cast<void *>(native),
      static_cast<unsigned long long>(creationHash),
      static_cast<unsigned long long>(currentHash), changed,
      first[0] ? first : "<none>", sizeof(current), path[0] ? path : "<none>");
}

// Compare the replacement Mesh currently bound to an afterimage renderer with
// the renderer's original Mesh at the same call boundary.  This is diagnostic
// only: it reads the two native records and never copies or patches them.
static void EiemRuntimeSnapshotCompareMeshes(void *replacement, void *source,
                                              const char *label) {
  if (!replacement || !source) return;
  const uintptr_t replacementNative = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(replacement));
  const uintptr_t sourceNative = reinterpret_cast<uintptr_t>(
      EiemGetNativeMeshPointer(source));
  if (!EiemRuntimeSnapshotNativeIsMesh(replacementNative) ||
      !EiemRuntimeSnapshotNativeIsMesh(sourceNative)) {
    Log("[MESH-RUNTIME-SOURCE-COMPARE] valid=0 label=%s replacement=%p "
        "replacementNative=%p source=%p sourceNative=%p reason=vtable-mismatch",
        label ? label : "<none>", replacement,
        reinterpret_cast<void *>(replacementNative), source,
        reinterpret_cast<void *>(sourceNative));
    return;
  }
  unsigned char replacementBytes[kEiemRuntimeSnapshotBytes] = {};
  unsigned char sourceBytes[kEiemRuntimeSnapshotBytes] = {};
  if (!EiemRuntimeSnapshotRead(replacementNative, replacementBytes,
                               sizeof(replacementBytes)) ||
      !EiemRuntimeSnapshotRead(sourceNative, sourceBytes,
                               sizeof(sourceBytes))) {
    Log("[MESH-RUNTIME-SOURCE-COMPARE] valid=0 label=%s replacement=%p "
        "replacementNative=%p source=%p sourceNative=%p reason=read-failed",
        label ? label : "<none>", replacement,
        reinterpret_cast<void *>(replacementNative), source,
        reinterpret_cast<void *>(sourceNative));
    return;
  }
  size_t changed = 0;
  size_t firstOffset = SIZE_MAX;
  for (size_t offset = 0; offset < sizeof(replacementBytes); ++offset) {
    if (replacementBytes[offset] == sourceBytes[offset]) continue;
    ++changed;
    if (firstOffset == SIZE_MAX) firstOffset = offset;
  }
  auto readField = [](const unsigned char *bytes, size_t offset) -> int32_t {
    int32_t value = 0;
    memcpy(&value, bytes + offset, sizeof(value));
    return value;
  };
  uint64_t replacementCreationHash = 0;
  uint64_t sourceCreationHash = 0;
  bool sourceCreationValid = false;
  uintptr_t pairedSourceNative = 0;
  AcquireSRWLockShared(&s_eiemRuntimeMeshSnapshotLock);
  for (size_t index = 0; index < s_eiemRuntimeMeshSnapshotCount; ++index) {
    const auto &entry = s_eiemRuntimeMeshSnapshots[index];
    if (entry.native != replacementNative || !entry.valid) continue;
    replacementCreationHash = entry.creationHash;
    sourceCreationHash = entry.sourceCreationHash;
    sourceCreationValid = entry.sourceCreationValid;
    pairedSourceNative = entry.sourceNative;
    break;
  }
  ReleaseSRWLockShared(&s_eiemRuntimeMeshSnapshotLock);
  Log("[MESH-RUNTIME-SOURCE-COMPARE] valid=1 label=%s replacement=%p "
      "replacementNative=%p source=%p sourceNative=%p replacementHash=0x%llX "
      "sourceHash=0x%llX changedBytes=%zu firstOffset=%s%zX "
      "replacementFields=110:%d,124:%d,1C8:%d sourceFields=110:%d,124:%d,1C8:%d",
      label ? label : "<none>", replacement,
      reinterpret_cast<void *>(replacementNative), source,
      reinterpret_cast<void *>(sourceNative),
      static_cast<unsigned long long>(
          EiemRuntimeSnapshotHash(replacementBytes, sizeof(replacementBytes))),
      static_cast<unsigned long long>(
          EiemRuntimeSnapshotHash(sourceBytes, sizeof(sourceBytes))),
      changed, firstOffset == SIZE_MAX ? "<none>" : "+0x", 
      firstOffset == SIZE_MAX ? 0 : firstOffset,
      readField(replacementBytes, 0x110), readField(replacementBytes, 0x124),
      readField(replacementBytes, 0x1C8), readField(sourceBytes, 0x110),
      readField(sourceBytes, 0x124), readField(sourceBytes, 0x1C8));
  Log("[MESH-RUNTIME-SOURCE-COMPARE] lifecycle label=%s "
      "replacementCreationHash=0x%llX sourceCreationHash=0x%llX "
      "sourceCreationValid=%d pairedSourceNative=%p currentSourceNative=%p",
      label ? label : "<none>",
      static_cast<unsigned long long>(replacementCreationHash),
      static_cast<unsigned long long>(sourceCreationHash),
      sourceCreationValid ? 1 : 0,
      reinterpret_cast<void *>(pairedSourceNative),
      reinterpret_cast<void *>(sourceNative));
}

#else

static void EiemRuntimeMeshSnapshotRemember(void *, const char *) {}
static void EiemRuntimeMeshSnapshotRememberWithSource(void *, void *, const char *) {}
static void EiemRuntimeMeshSnapshotObserveSource(void *, void *, const char *) {}
static void EiemRuntimeMeshSnapshotObserve(void *, const char *) {}
static void EiemRuntimeSnapshotCompareMeshes(void *, void *, const char *) {}

#endif
