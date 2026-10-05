#pragma once

// Temporary read-only evidence at the native vertex-format adaptation and
// Renderer record publication boundaries. Neither boundary is a GPU draw.
namespace NativeRender {
using NativeUpload::ReadAt;
constexpr uintptr_t recordRva = 0xC7B750, formatRva = 0xD7340;
constexpr uintptr_t renderManagerRva = 0x1D17EC0, skinManagerRva = 0x1CF5CC8;
using RecordFn = uint64_t(__fastcall *)(void *);
using FormatFn = uint64_t(__fastcall *)(void *, void *, void *, uint32_t *);
RecordFn originalRecord = nullptr;
FormatFn originalFormat = nullptr;
std::atomic_bool recordReady{false}, formatReady{false};
std::mutex mutex;
struct FormatCapture {
  std::array<unsigned char, 56> channels{};
  std::array<unsigned char, 28> selected{};
  uint32_t vertices = 0, changed = 0;
};
constexpr size_t formatLimit = 8192;
std::map<uintptr_t, FormatCapture> formats;
std::set<uintptr_t> layoutsSeen;
struct Cadence { uintptr_t mesh = 0; unsigned samples = 0; ULONGLONG first = 0; };
std::map<uintptr_t, Cadence> cadence;
std::atomic<unsigned> records{0}, formatDrops{0}, gaps{0};
template <class F> void Guard(F observe, const char *stage) noexcept {
  try { observe(); }
  catch (const std::exception &error) {
    if (++gaps <= 64) Log("[DEBUG-skin-v15-GAP] stage=%s reason=%s originalCallPreserved=1", stage, error.what());
  } catch (...) {
    if (++gaps <= 64) Log("[DEBUG-skin-v15-GAP] stage=%s reason=unknown originalCallPreserved=1", stage);
  }
}
FormatCapture ReadFormat(void *vertexData) {
  FormatCapture capture;
  capture.vertices = ReadAt<uint32_t>((uintptr_t)vertexData, 0x70);
  Require(capture.vertices <= 1000000 && CopyRaw(vertexData, capture.channels.data(), capture.channels.size()),
          "vertex format input extent");
  return capture;
}
void RetainFormat(uintptr_t vertexData, FormatCapture capture, void *selected, uint32_t *changed) {
  Require(CopyRaw(selected, capture.selected.data(), capture.selected.size()) &&
              CopyRaw(changed, &capture.changed, sizeof(capture.changed)), "vertex format output extent");
  std::lock_guard<std::mutex> lock(mutex);
  if (!formats.count(vertexData) && formats.size() >= formatLimit) { ++formatDrops; return; }
  formats[vertexData] = capture;
}
uint64_t __fastcall Format(void *device, void *vertexData, void *selected, uint32_t *changed) {
  FormatCapture capture; bool captured = false;
  if (formatReady) Guard([&] { capture = ReadFormat(vertexData); captured = true; }, "format-before");
  auto result = originalFormat(device, vertexData, selected, changed);
  if (captured) Guard([&] { RetainFormat((uintptr_t)vertexData, capture, selected, changed); }, "format-after");
  return result;
}
void Remember(void *managedMesh, const Expected &expected) {
  if (!formatReady || !NativeUpload::cachedPtrField) return;
  uintptr_t mesh = 0;
  il2cpp_field_get_value(managedMesh, NativeUpload::cachedPtrField, &mesh);
  Require(mesh && ReadAt<uintptr_t>(mesh, 0) == NativeUpload::unityBase + NativeUpload::meshVtableRva,
          "vertex layout Mesh identity");
  auto data = ReadAt<uintptr_t>(mesh, 0x38);
  auto vertexData = data + 8;
  FormatCapture chosen; bool found = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (layoutsSeen.size() >= NativeUpload::targetLimit || !layoutsSeen.insert(mesh).second) return;
    auto it = formats.find(vertexData);
    if (it != formats.end()) { chosen = it->second; found = true; }
  }
  auto current = ReadFormat((void *)vertexData);
  Log("[DEBUG-skin-v15-LAYOUT] mesh=%p vertexData=%p part=%s vertices=%u expectedVertices=%u "
      "conversionObserved=%d conversionVertices=%u changedMask=%u nativeMask=%u formatCacheDrops=%u GPUActualDrawObserved=0 writes=0",
      (void *)mesh, (void *)vertexData, expected.part.c_str(), current.vertices, expected.mesh->vertices,
      found, chosen.vertices, chosen.changed, ReadAt<uint32_t>(vertexData, 0x6C), formatDrops.load());
  for (size_t attr = 0; attr < 14; ++attr) {
    const auto *now = current.channels.data() + attr * 4;
    const auto *before = chosen.channels.data() + attr * 4;
    const auto *after = chosen.selected.data() + attr * 2;
    if (!(now[3] & 15) && (!found || !(before[3] & 15))) continue;
    Log("[DEBUG-skin-v15-CHANNEL] mesh=%p part=%s attr=%zu stream=%u offset=%u format=%u dimension=%u "
        "conversionObserved=%d beforeStream=%u beforeOffset=%u beforeFormat=%u beforeDimension=%u selectedFormat=%u selectedDimension=%u",
        (void *)mesh, expected.part.c_str(), attr, now[0], now[1], now[2], now[3], found,
        before[0], before[1], before[2], before[3], after[0], after[1]);
  }
  for (size_t vertexStream = 0; vertexStream < 4; ++vertexStream)
    Log("[DEBUG-skin-v15-STREAM] mesh=%p part=%s stream=%zu start=%u stride=%u channelMask=%u nativeGpuBuffer=%p",
        (void *)mesh, expected.part.c_str(), vertexStream,
        ReadAt<uint32_t>(vertexData, 0x3C + vertexStream * 12), ReadAt<uint32_t>(vertexData, 0x40 + vertexStream * 12),
        ReadAt<uint32_t>(vertexData, 0x44 + vertexStream * 12), (void *)ReadAt<uintptr_t>(mesh, 0x58 + vertexStream * 8));
}
unsigned Popcount(uint64_t value) {
  unsigned count = 0; while (value) { value &= value - 1; ++count; } return count;
}
// Read-only translation of 0x16D520. It resolves an existing record without
// calling the native getter, allocating a record or modifying feature flags.
uintptr_t ResolveRecord(uintptr_t store, uint32_t handle) {
  Require(handle < 0x1000000, "native record handle bound");
  auto table = ReadAt<uintptr_t>(store, 0x20);
  auto entry = table + uint64_t(handle) * 16;
  auto blockIndex = ReadAt<uint32_t>(entry, 0), slot = ReadAt<uint32_t>(entry, 4);
  Require(blockIndex < 65536, "native record block bound");
  auto block = ReadAt<uintptr_t>(store, 0) + uint64_t(blockIndex) * 0x240;
  auto flags = ReadAt<uint64_t>(block, 0x10);
  if (!(flags & (uint64_t(1) << 18))) return 0;
  auto buffer = ReadAt<uintptr_t>(block, 0x20);
  auto capacity = ReadAt<uint32_t>(block, 0x30);
  auto descriptor = block + 0x40 + Popcount(flags & 0x3FFFF) * 8;
  auto stride = ReadAt<int16_t>(descriptor, 2);
  auto blockOffset = ReadAt<uint32_t>(descriptor, 4);
  Require(buffer && slot < capacity && capacity <= 1000000 && stride >= 0x58 && stride <= 4096,
          "native record allocation extent");
  auto offset = uint64_t(stride) * slot + uint64_t(blockOffset) * capacity;
  Require(offset <= INT32_MAX && buffer <= UINTPTR_MAX - offset, "native record offset extent");
  return buffer + size_t(offset);
}
struct RingLink {
  uintptr_t segment = 0, base = 0, expected = 0;
  uint32_t origin = 0, rows = 0;
  bool inRange = false, pointerMatches = false;
};
RingLink LinkRing(uintptr_t segment, uint32_t current, uint32_t bones, uintptr_t palette) {
  RingLink link; link.segment = segment;
  link.base = ReadAt<uintptr_t>(segment, 0x28);
  link.rows = ReadAt<uint32_t>(segment, 8);
  link.origin = ReadAt<uint32_t>(segment, 12);
  uint64_t needed = (uint64_t(bones) + 1) * 3;
  if (current < link.origin || uint64_t(current) - link.origin + needed > link.rows) return link;
  auto offset = (uint64_t(current) - link.origin) * 16;
  Require(link.base && link.base <= UINTPTR_MAX - offset, "native ring address extent");
  link.inRange = true; link.expected = link.base + size_t(offset);
  link.pointerMatches = link.expected == palette;
  return link;
}
void ObserveRecord(void *nativeRenderer, uint64_t returned) {
  if (!recordReady || !NativeUpload::ready) return;
  NativeUpload::Target target;
  auto renderer = (uintptr_t)nativeRenderer;
  {
    std::lock_guard<std::mutex> lock(NativeUpload::targetMutex);
    auto it = NativeUpload::targets.find(renderer);
    if (it == NativeUpload::targets.end()) return;
    target = it->second;
  }
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!cadence.count(renderer) && cadence.size() >= NativeUpload::targetLimit) return;
    auto &entry = cadence[renderer];
    if (entry.mesh != target.mesh) entry = {target.mesh, 0, 0};
    auto tick = GetTickCount64();
    constexpr ULONGLONG delays[]{0, 1000, 5000};
    if (entry.samples >= 3 || (entry.samples && tick - entry.first < delays[entry.samples])) return;
    if (!entry.samples) entry.first = tick;
    ++entry.samples;
  }
  Require(ReadAt<uintptr_t>(renderer, 0x298) == target.mesh &&
              ReadAt<uintptr_t>(target.mesh, 0) == NativeUpload::unityBase + NativeUpload::meshVtableRva,
          "native record target identity changed");
  auto handle = ReadAt<uint32_t>(renderer, 0x268);
  auto manager = ReadAt<uintptr_t>(NativeUpload::unityBase + renderManagerRva, 0);
  auto store = ReadAt<uintptr_t>(manager, 0x38);
  auto record = ResolveRecord(store, handle);
  Require(record, "native Renderer record unavailable");
  auto current = ReadAt<uint32_t>(renderer, 0x374), previous = ReadAt<uint32_t>(renderer, 0x370);
  auto recordCurrent = ReadAt<uint32_t>(record, 0x50), recordPrevious = ReadAt<uint32_t>(record, 0x54);
  auto palette = ReadAt<uintptr_t>(renderer, 0x388);
  auto skinManager = ReadAt<uintptr_t>(NativeUpload::unityBase + skinManagerRva, 0);
  auto ring = ReadAt<uintptr_t>(skinManager, 0xC0060);
  auto link = LinkRing(ring, current, target.bones, palette);
  auto meshData = ReadAt<uintptr_t>(target.mesh, 0x38);
  auto shape = ReadAt<uintptr_t>(meshData, 0x190);
  auto flags = ReadAt<uint32_t>(record, 0x4C);
  auto seq = ++records;
  Log("[DEBUG-skin-v15-RECORD] seq=%u renderer=%p mesh=%p part=%s handle=%u record=%p return=%llu "
      "recordFlags=%u shaderInfluences=%u nativeInfluences=%u currentBase=%u previousBase=%u "
      "recordCurrent=%u recordPrevious=%u basesMatched=%d palette=%p ring=%p ringBase=%p ringOrigin=%u ringRows=%u "
      "ringExpected=%p ringInRange=%d ringPointerMatched=%d shapeData=%p shapeVertexArray=%p GPUActualDrawObserved=0 writes=0",
      seq, nativeRenderer, (void *)target.mesh, target.part.c_str(), handle, (void *)record,
      (unsigned long long)returned, flags, flags & 15, ReadAt<uint32_t>(target.mesh, 0x1C8), current, previous,
      recordCurrent, recordPrevious, current == recordCurrent && previous == recordPrevious,
      (void *)palette, (void *)ring, (void *)link.base, link.origin, link.rows, (void *)link.expected,
      link.inRange, link.pointerMatches, (void *)shape, shape ? (void *)ReadAt<uintptr_t>(shape, 0x50) : nullptr);
}
uint64_t __fastcall Record(void *renderer) {
  auto result = originalRecord(renderer);
  Guard([&] { ObserveRecord(renderer, result); }, "record-after");
  return result;
}
bool Prefix(uintptr_t address, const char *hex) {
  auto length = strlen(hex);
  Require(length && !(length & 1) && length <= 128, "native prefix extent");
  std::vector<unsigned char> bytes(length / 2);
  std::string expected(hex);
  std::transform(expected.begin(), expected.end(), expected.begin(), [](char c) { return char(toupper(c)); });
  return CopyRaw((void *)address, bytes.data(), bytes.size()) && Hex(bytes.data(), bytes.size()) == expected;
}
void InstallHook(uintptr_t rva, const char *prefix, void *detour, void **original) {
  auto address = (void *)(NativeUpload::unityBase + rva);
  Require(Prefix((uintptr_t)address, prefix) && !installed.count(address), "native record/format prefix gate");
  auto status = MH_CreateHook(address, detour, original);
  if (status == MH_OK) {
    status = MH_EnableHook(address);
    if (status != MH_OK) MH_RemoveHook(address);
  }
  Require(status == MH_OK, "native record/format hook installation failed");
  installed.insert(address);
}
void Install() {
  if (!std::filesystem::exists(gameDirectory / "plugin" / "resource-input-diagnostics" / "native-render-record.enabled")) return;
  Require(NativeUpload::ready, "native record exact-image and target registration prerequisite");
  InstallHook(recordRva, "40574883ec20488b4138488bf94883c138ff908000000084", (void *)Record, (void **)&originalRecord);
  recordReady = true;
  Log("[DEBUG-skin-v15-READY] boundary=UnityPlayer+0xC7B750 scope=renderer-record maxTargets=128 samplesPerTarget=3 GPUActualDrawObserved=0 writes=0");
  InstallHook(formatRva, "48895c240848896c2418565741564883ec40488bea41c70100000000", (void *)Format, (void **)&originalFormat);
  formatReady = true;
  Log("[DEBUG-skin-v15-READY] boundary=UnityPlayer+0xD7340 scope=vertex-format-adaptation maxCachedVertexData=%zu GPUActualDrawObserved=0 writes=0", formatLimit);
}
} // namespace NativeRender
