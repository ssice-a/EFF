#pragma once

// Read-only, opt-in observation of the game-owned matrix packing boundary.
// The exact UnityPlayer image and function prefix are verified before hooking.
// No managed getters run on the native job thread and no game memory is written.
namespace NativeUpload {
constexpr uintptr_t uploadRva = 0xEE960;
constexpr uintptr_t meshVtableRva = 0x19C8230;
constexpr size_t targetLimit = 128, perTargetLimit = 3;
using UploadFn = void(__fastcall *)(void *, const float *, uint64_t);
UploadFn originalUpload = nullptr;
void *cachedPtrField = nullptr;
uintptr_t unityBase = 0;
std::atomic_bool ready{false};
struct Target {
  uintptr_t mesh = 0;
  unsigned bones = 0, vertices = 0, captures = 0;
  ULONGLONG firstTick = 0;
  std::string part;
};
std::mutex targetMutex;
std::map<uintptr_t, Target> targets;
std::atomic<unsigned> records{0};
std::atomic<unsigned> gaps{0};
template <class F> void Guard(F observe, const char *stage, unsigned seq = 0) noexcept {
  try { observe(); }
  catch (const std::exception &error) {
    if (++gaps <= 64) Log("[DEBUG-skin-v14-GAP] stage=%s seq=%u reason=%s originalCallPreserved=1", stage, seq, error.what());
  } catch (...) {
    if (++gaps <= 64) Log("[DEBUG-skin-v14-GAP] stage=%s seq=%u reason=unknown originalCallPreserved=1", stage, seq);
  }
}
struct Capture {
  uintptr_t renderer = 0, mesh = 0;
  unsigned seq = 0, ordinal = 0, expectedBones = 0, expectedVertices = 0;
  uint64_t count = 0;
  std::string part;
  std::vector<float> input;
};
template <class T> T ReadAt(uintptr_t object, size_t offset) {
  T value{};
  Require(object && offset <= UINTPTR_MAX - object && CopyRaw((void *)(object + offset), &value, sizeof(value)),
          "native upload read range");
  return value;
}
void Remember(void *renderer, void *mesh, const Expected &expected) {
  if (!ready || !cachedPtrField) return;
  uintptr_t nativeRenderer = 0, nativeMesh = 0;
  il2cpp_field_get_value(renderer, cachedPtrField, &nativeRenderer);
  il2cpp_field_get_value(mesh, cachedPtrField, &nativeMesh);
  Require(nativeRenderer && nativeMesh &&
              ReadAt<uintptr_t>(nativeRenderer, 0x298) == nativeMesh &&
              ReadAt<uintptr_t>(nativeMesh, 0) == unityBase + meshVtableRva,
          "native upload Renderer/Mesh identity mismatch");
  std::lock_guard<std::mutex> lock(targetMutex);
  auto found = targets.find(nativeRenderer);
  if (found != targets.end() && found->second.mesh == nativeMesh) return;
  if (found == targets.end() && targets.size() >= targetLimit) return;
  targets[nativeRenderer] = {nativeMesh, unsigned(expected.mesh->bones.size()), expected.mesh->vertices, 0, 0, expected.part};
  Log("[DEBUG-skin-v14-TARGET] renderer=%p mesh=%p expectedBones=%zu expectedVertices=%u part=%s",
      (void *)nativeRenderer, (void *)nativeMesh, expected.mesh->bones.size(), expected.mesh->vertices, expected.part.c_str());
}
Capture Before(void *renderer, const float *matrices, uint64_t count) {
  Capture capture;
  if (!ready || !renderer || !matrices) return capture;
  {
    std::lock_guard<std::mutex> lock(targetMutex);
    auto found = targets.find((uintptr_t)renderer);
    if (found == targets.end()) return capture;
    auto &target = found->second;
    if (target.captures >= perTargetLimit) return capture;
    auto tick = GetTickCount64();
    // Sample distinct animation times through calls the game already makes.
    // This is an evidence cadence, not a replacement/update delay.
    constexpr ULONGLONG delays[]{0, 1000, 5000};
    if (target.captures && tick - target.firstTick < delays[target.captures]) return capture;
    Require(count && count <= 256, "native upload matrix count limit");
    Require(ReadAt<uintptr_t>((uintptr_t)renderer, 0x298) == target.mesh &&
                ReadAt<uintptr_t>(target.mesh, 0) == unityBase + meshVtableRva,
            "native upload target generation changed");
    if (!target.captures) target.firstTick = tick;
    capture.renderer = (uintptr_t)renderer;
    capture.mesh = target.mesh;
    capture.expectedBones = target.bones;
    capture.expectedVertices = target.vertices;
    capture.part = target.part;
    capture.ordinal = ++target.captures;
    capture.seq = ++records;
  }
  capture.count = count;
  capture.input.resize(size_t(count) * 16);
  Require(CopyRaw(matrices, capture.input.data(), capture.input.size() * sizeof(float)), "native upload input matrix bytes");
  return capture;
}
struct PackedComparison {
  size_t inputNonFinite = 0, outputNonFinite = 0, packedBitMismatches = 0;
  double inputMaxAbs = 0, outputMaxAbs = 0, maxElementError = 0;
};
PackedComparison Compare(const std::vector<float> &input, const std::vector<float> &output) {
  Require(input.size() % 16 == 0 && output.size() == input.size() / 16 * 12,
          "native upload comparison extent");
  PackedComparison result;
  for (float value : input) {
    if (!std::isfinite(value)) ++result.inputNonFinite;
    else result.inputMaxAbs = std::max(result.inputMaxAbs, std::abs(double(value)));
  }
  for (size_t bone = 0; bone < input.size() / 16; ++bone)
    for (size_t row = 0; row < 3; ++row) for (size_t column = 0; column < 4; ++column) {
      const float &expected = input[bone * 16 + row + column * 4];
      const float &actual = output[bone * 12 + row * 4 + column];
      result.packedBitMismatches += std::memcmp(&expected, &actual, 4) != 0;
      if (!std::isfinite(actual)) ++result.outputNonFinite;
      else result.outputMaxAbs = std::max(result.outputMaxAbs, std::abs(double(actual)));
      if (std::isfinite(expected) && std::isfinite(actual))
        result.maxElementError = std::max(result.maxElementError, std::abs(double(expected) - actual));
    }
  return result;
}
void WriteSnapshot(const std::filesystem::path &path, const std::vector<float> &data) {
  auto snapshotFile = OpenLog(path, true);
  Require(snapshotFile != nullptr, "native upload snapshot create failed");
  auto wrote = fwrite(data.data(), sizeof(float), data.size(), snapshotFile);
  auto closed = fclose(snapshotFile);
  Require(wrote == data.size() && !closed, "native upload snapshot write failed");
}
void After(const Capture &capture) {
  if (!capture.seq) return;
  auto mesh = ReadAt<uintptr_t>(capture.renderer, 0x298);
  Require(mesh == capture.mesh, "native upload Mesh changed inside callback");
  auto meshData = ReadAt<uintptr_t>(mesh, 0x38);
  auto bindposeCount = ReadAt<uint32_t>(meshData, 0x100);
  auto influenceCount = ReadAt<uint32_t>(mesh, 0x1C8);
  auto buffer = ReadAt<uintptr_t>(capture.renderer, 0x388);
  auto current = ReadAt<uint32_t>(capture.renderer, 0x374);
  auto previous = ReadAt<uint32_t>(capture.renderer, 0x370);
  auto skinFrame = ReadAt<uint32_t>(capture.renderer, 0x3B4);
  std::vector<float> packed(size_t(capture.count) * 12), root(12);
  Require(buffer && buffer <= UINTPTR_MAX - 48 &&
              CopyRaw((void *)buffer, root.data(), 48) &&
              CopyRaw((void *)(buffer + 48), packed.data(), packed.size() * sizeof(float)),
          "native upload packed buffer unavailable");
  auto comparison = Compare(capture.input, packed);
  auto stem = "skin-upload-" + std::to_string(capture.seq);
  WriteSnapshot(sessionDirectory / (stem + "-input4x4.bin"), capture.input);
  WriteSnapshot(sessionDirectory / (stem + "-packed3x4.bin"), packed);
  WriteSnapshot(sessionDirectory / (stem + "-root3x4.bin"), root);
  Log("[DEBUG-skin-v14-UPLOAD] seq=%u ordinal=%u renderer=%p mesh=%p part=%s count=%llu expectedBones=%u "
      "expectedVertices=%u nativeBindposes=%u effectiveInfluences=%u currentBase=%u previousBase=%u skinFrame=%u "
      "buffer=%p inputNonFinite=%zu outputNonFinite=%zu packedBitMismatches=%zu maxPackedError=%.9g "
      "inputMaxAbs=%.9g outputMaxAbs=%.9g file=%s GPUActualDrawObserved=0 writes=0",
      capture.seq, capture.ordinal, (void *)capture.renderer, (void *)mesh, capture.part.c_str(),
      (unsigned long long)capture.count, capture.expectedBones, capture.expectedVertices, bindposeCount, influenceCount,
      current, previous, skinFrame, (void *)buffer, comparison.inputNonFinite, comparison.outputNonFinite,
      comparison.packedBitMismatches, comparison.maxElementError, comparison.inputMaxAbs, comparison.outputMaxAbs, stem.c_str());
}
void __fastcall Upload(void *renderer, const float *matrices, uint64_t count) {
  Capture capture;
  Guard([&] { capture = Before(renderer, matrices, count); }, "before");
  originalUpload(renderer, matrices, count);
  Guard([&] { After(capture); }, "after", capture.seq);
}
bool VerifiedImage(HMODULE module) {
  if (!module) return false;
  auto image = EiemDirectory::Read(gameDirectory / "UnityPlayer.dll");
  auto digest = Sha(image.data(), image.size());
  if (Hex(digest.data(), digest.size()) != "BEE7BE52370ADDDD67BA61E4937CA51B7F272656841D187E95E505496DA798D1") return false;
  constexpr unsigned char prefix[]{0x48,0x85,0xD2,0x0F,0x84,0xB2,0,0,0,0x48,0x89,0x5C,0x24,8,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x81,0xEC,0xB0};
  unsigned char bytes[sizeof(prefix)]{};
  return CopyRaw((char *)module + uploadRva, bytes, sizeof(bytes)) && !memcmp(bytes, prefix, sizeof(bytes));
}
void Install(void *objectClass) {
  if (!std::filesystem::exists(gameDirectory / "plugin" / "resource-input-diagnostics" / "native-skin-upload.enabled")) return;
  auto module = GetModuleHandleW(L"UnityPlayer.dll");
  Require(VerifiedImage(module), "native upload exact UnityPlayer image gate");
  void *iterator = nullptr;
  while (auto field = il2cpp_class_get_fields(objectClass, &iterator)) {
    auto name = il2cpp_field_get_name(field);
    if (!name || strcmp(name, "m_CachedPtr")) continue;
    auto fieldType = il2cpp_field_get_type(field);
    auto typeName = fieldType ? il2cpp_type_get_name(fieldType) : nullptr;
    bool valid = typeName && !strcmp(typeName, "System.IntPtr") && il2cpp_field_get_offset(field) >= 16;
    if (typeName) il2cpp_free((void *)typeName);
    Require(valid && !cachedPtrField, "native upload cached pointer metadata gate");
    cachedPtrField = field;
  }
  Require(cachedPtrField, "native upload cached pointer field absent");
  unityBase = (uintptr_t)module;
  auto address = (void *)(unityBase + uploadRva);
  Require(!installed.count(address), "native upload duplicate address");
  auto status = MH_CreateHook(address, (void *)Upload, (void **)&originalUpload);
  if (status == MH_OK) {
    status = MH_EnableHook(address);
    if (status != MH_OK) MH_RemoveHook(address);
  }
  Require(status == MH_OK, "native upload hook install failed");
  installed.insert(address);
  ready = true;
  Log("[DEBUG-skin-v14-READY] boundary=UnityPlayer+0xEE960 maxTargets=%zu samplesPerTarget=%zu "
      "scope=matrix-packing input=column4x4 output=row3x4 GPUActualDrawObserved=0 writes=0", targetLimit, perTargetLimit);
}
} // namespace NativeUpload
