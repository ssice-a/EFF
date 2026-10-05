#pragma once

// Value-only diagnostic records. No pointers retained here own game memory.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

// Span semantics from the inspected player's +0xAF980, not std::string.
// +0x1BF710 stores this string at the observed file provider's +0x30.
// A null status denotes an exact value copy; faults/truncation remain explicit.
// This never invokes the game's string getter, provider, or file operations.
template <typename ReadBytes>
static const char *EiemReadPlayerFileString(
    uintptr_t stringAddress, ReadBytes read, char *out, size_t capacity) {
  if (!out || capacity < 2) return "file-string-missing-output";
  out[0] = '\0';
  unsigned char header[33] = {};
  if (!stringAddress || stringAddress > UINTPTR_MAX - sizeof(header) ||
      !read(stringAddress, header, sizeof(header)))
    return "file-string-header-unreadable";
  const unsigned char mode = header[0x20];
  uintptr_t data = 0;
  uint64_t length = 0;
  if (mode == 1) {
    // +0xAF980 uses signed byte remaining capacity at +0x18.
    const int remaining = static_cast<int8_t>(header[0x18]);
    if (remaining < 0 || remaining > 24) return "file-string-inline-invalid";
    data = stringAddress;
    length = 24 - remaining;
  } else if (mode == 0 || mode == 2) {
    memcpy(&data, header, sizeof(data));
    memcpy(&length, header + 0x10, sizeof(length));
  } else {
    return "file-string-mode-unverified";
  }
  if (!length) return "file-string-empty";
  if (length >= capacity) return "file-string-too-long";
  if (!data || data > UINTPTR_MAX - length ||
      !read(data, out, static_cast<size_t>(length))) {
    out[0] = '\0';
    return "file-string-data-unreadable";
  }
  for (size_t i = 0; i < static_cast<size_t>(length); ++i) {
    const unsigned char c = static_cast<unsigned char>(out[i]);
    if (c < 0x20 || c == 0x7F || c == '"') {
      out[0] = '\0';
      return "file-string-text-invalid";
    }
  }
  out[length] = '\0';
  return nullptr;
}

struct EiemSerializedObjectReadRecord {
  int64_t key = 0;
  uint64_t offset = 0;
  uint32_t length = 0;
  uint32_t typeIndex = 0;
};
static_assert(sizeof(EiemSerializedObjectReadRecord) == 24,
              "Serialized object record layout changed");

struct EiemNativeReadWindow {
  uintptr_t cursor = 0;
  uintptr_t base = 0;
  uintptr_t limit = 0;
  uintptr_t provider = 0;
  int32_t blockIndex = -1;
  uint64_t blockSize = 0;
  uint64_t start = 0;
  uint64_t end = 0;
  uint64_t position = 0;
  bool valid = false;
};

// Mirrors the position arithmetic in +0x1340F0 without calling it. That game
// function also releases a provider block and invalidates the index, so it is
// not a read-only getter and must never be invoked by the diagnostic.
static bool EiemNativeReadWindowPosition(EiemNativeReadWindow *window) {
  if (!window) return false;
  window->valid = false;
  if (!window->provider || window->blockIndex < 0 || !window->blockSize ||
      !window->base || window->cursor < window->base ||
      window->limit < window->cursor || window->start > window->end)
    return false;
  const uint64_t index = static_cast<uint64_t>(window->blockIndex);
  if (index > (std::numeric_limits<uint64_t>::max)() / window->blockSize)
    return false;
  const uint64_t blockOffset = index * window->blockSize;
  const uint64_t inBlock = window->cursor - window->base;
  if (inBlock > (std::numeric_limits<uint64_t>::max)() - blockOffset)
    return false;
  window->position = blockOffset + inBlock;
  window->valid = window->position >= window->start &&
                  window->position <= window->end;
  return window->valid;
}

// Bounded lower_bound against the same 24-byte vector used by +0x347BC0.
// A caller supplies fault-contained reads and must establish the live table's
// ownership separately; this helper never calls a game method or locks it.
template <typename ReadBytes>
static const char *EiemFindSerializedObjectReadRecord(
    uintptr_t begin, uintptr_t end, int64_t key, ReadBytes read,
    EiemSerializedObjectReadRecord *out, uintptr_t *address) {
  if (!out || !address) return "missing-output";
  *out = {};
  *address = 0;
  if (!begin || end < begin || (end - begin) % sizeof(*out) != 0)
    return "record-vector-invalid";
  const size_t count = (end - begin) / sizeof(*out);
  if (count > 1000000) return "record-vector-too-large";
  size_t low = 0;
  size_t high = count;
  while (low < high) {
    const size_t mid = low + (high - low) / 2;
    int64_t candidate = 0;
    if (!read(begin + mid * sizeof(*out), &candidate, sizeof(candidate)))
      return "record-key-unreadable";
    if (candidate < key) low = mid + 1;
    else high = mid;
  }
  if (low == count) return "record-key-not-found";
  EiemSerializedObjectReadRecord candidate;
  const uintptr_t selected = begin + low * sizeof(*out);
  if (!read(selected, &candidate, sizeof(candidate)))
    return "record-unreadable";
  if (candidate.key != key) return "record-key-not-found";
  *out = candidate;
  *address = selected;
  return nullptr;
}
