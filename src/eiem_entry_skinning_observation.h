#pragma once

// Temporary, opt-in readback at game-owned registration boundaries. No Mesh,
// Renderer, Transform or animation state is written. Not a replacement path.
namespace SkinningObservation {
struct Api {
  void *name = nullptr, *transform = nullptr, *parent = nullptr;
  void *mesh = nullptr, *bones = nullptr, *root = nullptr;
  void *vertices = nullptr, *readable = nullptr, *bindposes = nullptr;
  void *attributeCount = nullptr, *attributeDescriptor = nullptr;
  void *weights = nullptr, *channelArray = nullptr, *indices = nullptr;
} api;
bool enabled = false;
constexpr unsigned sampleLimit = 256;
std::atomic<unsigned> samples{0};
std::mutex sampleMutex;
std::set<std::pair<void *, std::string>> seen;
std::set<void *> geometrySeen;
using RendererInitFn = void(__fastcall *)(void *, void *, void *, void *);
using ControllerInitFn = void(__fastcall *)(void *, void *, void *, void *, bool, void *);
using BonesFn = void(__fastcall *)(void *, void *, void *);
RendererInitFn rendererInit = nullptr;
ControllerInitFn controllerInit = nullptr;
BonesFn setBones = nullptr;

template <class F> void StartOptionalReadback(F install) {
  try { install(); }
  catch (const std::exception &e) {
    enabled = false;
    Log("[DEBUG-skin-v11-DISABLED] reason=%s replacementStartupContinues=1", e.what());
  } catch (...) {
    enabled = false;
    Log("[DEBUG-skin-v11-DISABLED] reason=unknown-observation-error replacementStartupContinues=1");
  }
}

void *Invoke(void *method, void *self, void **arguments = nullptr) {
  Require(method && self && il2cpp_runtime_invoke, "readback method/object absent");
  void *exception = nullptr;
  auto result = il2cpp_runtime_invoke(method, self, arguments, &exception);
  Require(!exception, "managed readback exception");
  return result;
}
int Scalar(void *method, void *self, int argument = 0, bool hasArgument = false) {
  void *args[] = {&argument};
  auto boxed = Invoke(method, self, hasArgument ? args : nullptr);
  int value = 0;
  Require(boxed && il2cpp_object_unbox && CopyRaw(il2cpp_object_unbox(boxed), &value, sizeof(value)),
          "readback scalar result");
  return value;
}
bool Boolean(void *method, void *self) {
  auto boxed = Invoke(method, self);
  unsigned char value = 0;
  Require(boxed && il2cpp_object_unbox && CopyRaw(il2cpp_object_unbox(boxed), &value, 1),
          "readback boolean result");
  return value != 0;
}
std::string Name(void *object) { return String(Invoke(api.name, object)); }
std::string Path(void *transform) {
  std::vector<std::string> names;
  std::set<void *> visited;
  for (unsigned depth = 0; transform && depth < 128; ++depth) {
    Require(visited.insert(transform).second, "readback Transform cycle");
    names.push_back(Name(transform));
    transform = Invoke(api.parent, transform);
  }
  Require(!transform, "readback Transform depth limit");
  std::string path;
  for (auto i = names.rbegin(); i != names.rend(); ++i) path += "/" + *i;
  return path;
}
bool SuffixPath(const std::string &actual, const std::string &author) {
  auto suffix = "/" + author;
  return actual.size() >= suffix.size() && actual.compare(actual.size() - suffix.size(), suffix.size(), suffix) == 0;
}
struct Expected {
  const Package *package = nullptr;
  const Mesh *mesh = nullptr;
  std::string part;
};
Expected Match(const std::vector<Package> &packages, const std::string &name) {
  Expected found;
  for (const auto &package : packages) for (const auto &entry : package.rules) {
    const auto &rule = entry.second;
    if (rule.skip) continue;
    for (const auto &target : rule.targets) for (size_t i = 0; i < rule.parts.size(); ++i) {
      const auto &part = rule.parts[i];
      auto expectedName = target.second.asset;
      if (i) expectedName += "__EFF_" + package.id + "_" + part;
      if (name != expectedName) continue;
      Require(!found.mesh, "ambiguous diagnostic Mesh name");
      found = {&package, &package.meshes.at(package.parts.at(part).mesh), part};
    }
  }
  return found;
}
struct NativeWeight {
  float weights[4];
  int32_t bones[4];
};
static_assert(sizeof(NativeWeight) == 32, "managed BoneWeight layout");
struct VertexInput {
  unsigned count = 0;
  Bytes data;
  size_t skinStart = 0, positionStride = 16;
  std::array<float, 3> Position(size_t vertex) const {
    Require(vertex < count, "diagnostic position index");
    std::array<float, 3> result;
    memcpy(result.data(), data.data() + vertex * positionStride, 12);
    return result;
  }
  NativeWeight Skin(size_t vertex) const {
    Require(vertex < count, "diagnostic weight index");
    NativeWeight result{};
    for (size_t i = 0; i < 4; ++i) {
      result.weights[i] = float(Le(data, skinStart + vertex * 12 + i * 2, 2)) / 65535.f;
      result.bones[i] = data.at(skinStart + vertex * 12 + 8 + i);
    }
    return result;
  }
};
VertexInput AuthorVertices(const Resource &resource) {
  const auto &block = resource.blocks.at("m_VertexData");
  Require(block.offset <= resource.bytes.size() && block.size >= 68 &&
              block.size <= resource.bytes.size() - block.offset, "diagnostic vertex block range");
  Bytes raw(resource.bytes.begin() + block.offset, resource.bytes.begin() + block.offset + block.size);
  VertexInput result;
  result.count = unsigned(Le(raw, 0, 4));
  Require(result.count && result.count <= 1000000 && Le(raw, 4, 4) == 14,
          "diagnostic vertex channel/count limit");
  size_t strides[3]{};
  for (size_t i = 0; i < 14; ++i) {
    const unsigned char *c = raw.data() + 8 + i * 4;
    auto dimension = c[3] & 15;
    if (!dimension) continue;
    Require(c[0] < 3 && (c[2] == 0 || c[2] == 4 || c[2] == 6), "diagnostic vertex format");
    size_t width = c[2] == 0 ? 4 : c[2] == 4 ? 2 : 1;
    strides[c[0]] = std::max(strides[c[0]], size_t(c[1]) + dimension * width);
  }
  const bool packed = strides[0] == 16 && raw[12] == 0 && raw[13] == 12 && raw[14] == 0 && raw[15] == 0x31;
  const bool shaped = strides[0] == 40 && raw[12] == 0 && raw[13] == 12 && raw[14] == 0 && raw[15] == 3 &&
                      raw[16] == 0 && raw[17] == 24 && raw[18] == 0 && raw[19] == 4;
  Require(raw[8] == 0 && raw[9] == 0 && raw[10] == 0 && raw[11] == 3 && (packed || shaped) &&
              raw[56] == 2 && raw[57] == 0 && raw[58] == 4 && raw[59] == 4 &&
              raw[60] == 2 && raw[61] == 8 && raw[62] == 6 && raw[63] == 4 && strides[2] == 12,
          "diagnostic packed position/skin layout");
  result.positionStride = strides[0];
  result.skinStart = (result.count * strides[0] + 15) & ~size_t(15);
  result.skinStart = (result.skinStart + result.count * strides[1] + 15) & ~size_t(15);
  size_t size = size_t(Le(raw, 64, 4));
  Require(size <= raw.size() - 68 && result.skinStart + result.count * 12 <= size,
          "diagnostic stream extent");
  result.data.assign(raw.begin() + 68, raw.begin() + 68 + size);
  return result;
}
// Unity may reorder influences. Compare the weight assigned to each bone,
// ignoring zero-weight padding slots rather than requiring tuple order.
double WeightError(const NativeWeight &expected, const NativeWeight &actual, size_t palette) {
  std::map<int, double> difference;
  for (size_t i = 0; i < 4; ++i) {
    Require(std::isfinite(actual.weights[i]) && actual.weights[i] >= 0 && actual.weights[i] <= 1,
            "native diagnostic weight range");
    if (actual.weights[i]) {
      Require(actual.bones[i] >= 0 && size_t(actual.bones[i]) < palette, "native diagnostic bone index range");
      difference[actual.bones[i]] += actual.weights[i];
    }
    if (expected.weights[i]) difference[expected.bones[i]] -= expected.weights[i];
  }
  double error = 0;
  for (const auto &value : difference) error = std::max(error, std::abs(value.second));
  return error;
}
void ArrayElementSize(void *array, int expected) {
  Require(il2cpp_class_get_element_class, "diagnostic array element API unavailable");
  auto element = il2cpp_class_get_element_class(il2cpp_object_get_class(array));
  uint32_t alignment = 0;
  Require(element && il2cpp_class_value_size(element, &alignment) == expected,
          "diagnostic native array element layout");
}
template <class F> void GeometryRead(unsigned seq, const char *capability, F read) {
  try { read(); }
  catch (const std::exception &e) {
    Log("[DEBUG-skin-v12-GAP] seq=%u capability=%s reason=%s", seq, capability, e.what());
  } catch (...) {
    Log("[DEBUG-skin-v12-GAP] seq=%u capability=%s reason=unknown-readback-error", seq, capability);
  }
}
void ReadGeometry(unsigned seq, void *mesh, const Expected &expected, const VertexInput &input) {
  if (api.weights) {
    GeometryRead(seq, "decoded-weights", [&] {
      auto array = Invoke(api.weights, mesh);
      size_t count = 0;
      Require(ArrayLength(array, &count) && count == input.count, "native weight readback count");
      ArrayElementSize(array, sizeof(NativeWeight));
      std::vector<NativeWeight> values(count);
      Require(CopyRaw((char *)array + 32, values.data(), count * sizeof(NativeWeight)), "native weight readback bytes");
      double maxError = 0, maxSumError = 0;
      size_t mismatches = 0;
      for (size_t v = 0; v < count; ++v) {
        auto author = input.Skin(v);
        const auto &actual = values[v];
        double error = WeightError(author, actual, expected.mesh->bones.size());
        maxError = std::max(maxError, error);
        double sum = 0;
        for (float w : actual.weights) sum += w;
        maxSumError = std::max(maxSumError, std::abs(sum - 1));
        if (error > 2e-5 && mismatches++ < 8)
          Log("[DEBUG-skin-v12-WEIGHT-MISMATCH] seq=%u vertex=%zu expectedWeights=%.7g,%.7g,%.7g,%.7g "
              "expectedBones=%d,%d,%d,%d actualWeights=%.7g,%.7g,%.7g,%.7g actualBones=%d,%d,%d,%d error=%.9g",
              seq, v, author.weights[0], author.weights[1], author.weights[2], author.weights[3],
              author.bones[0], author.bones[1], author.bones[2], author.bones[3],
              actual.weights[0], actual.weights[1], actual.weights[2], actual.weights[3],
              actual.bones[0], actual.bones[1], actual.bones[2], actual.bones[3], error);
      }
      Log("[DEBUG-skin-v12-WEIGHTS] seq=%u vertices=%zu mismatches=%zu maxWeightError=%.9g maxSumError=%.9g",
          seq, count, mismatches, maxError, maxSumError);
    });
  } else Log("[DEBUG-skin-v12-GAP] seq=%u capability=decoded-weights reason=method-unavailable", seq);
  if (api.channelArray) {
    GeometryRead(seq, "decoded-positions", [&] {
      int attribute = 0, format = 0, dimension = 3;
      void *args[] = {&attribute, &format, &dimension};
      auto array = Invoke(api.channelArray, mesh, args);
      size_t count = 0;
      Require(ArrayLength(array, &count) && count == input.count, "native position readback count");
      ArrayElementSize(array, 12);
      std::vector<std::array<float, 3>> values(count);
      Require(CopyRaw((char *)array + 32, values.data(), count * 12), "native position readback bytes");
      double maxError = 0;
      size_t mismatches = 0;
      for (size_t v = 0; v < count; ++v) {
        auto author = input.Position(v);
        double error = 0;
        for (size_t axis = 0; axis < 3; ++axis) {
          Require(std::isfinite(values[v][axis]), "nonfinite native position");
          error = std::max(error, std::abs(double(values[v][axis]) - author[axis]));
        }
        maxError = std::max(maxError, error);
        mismatches += error > 1e-6;
      }
      Log("[DEBUG-skin-v12-POSITIONS] seq=%u vertices=%zu mismatches=%zu maxElementError=%.9g", seq, count, mismatches, maxError);
    });
  } else Log("[DEBUG-skin-v12-GAP] seq=%u capability=decoded-positions reason=method-unavailable", seq);
  if (api.indices) {
    GeometryRead(seq, "decoded-indices", [&] {
      int submesh = 0;
      bool baseVertex = true;
      void *args[] = {&submesh, &baseVertex};
      auto array = Invoke(api.indices, mesh, args);
      size_t count = 0;
      Require(ArrayLength(array, &count) && count == expected.mesh->indices, "native index readback count");
      ArrayElementSize(array, 4);
      std::vector<int32_t> values(count);
      Require(CopyRaw((char *)array + 32, values.data(), count * 4), "native index readback bytes");
      const auto &resource = *expected.package->resources.at(expected.mesh->resource);
      const auto &format = resource.blocks.at("m_IndexFormat"), &indices = resource.blocks.at("m_IndexBuffer");
      size_t width = Le(resource.bytes, format.offset, 4) ? 4 : 2;
      Require(indices.size == ((4 + count * width + 3) & ~size_t(3)) && Le(resource.bytes, indices.offset, 4) == count * width,
              "diagnostic author index buffer extent");
      size_t mismatches = 0, outOfRange = 0;
      for (size_t i = 0; i < count; ++i) {
        auto author = Le(resource.bytes, indices.offset + 4 + i * width, width);
        mismatches += uint64_t(values[i]) != author;
        outOfRange += values[i] < 0 || uint64_t(values[i]) >= input.count;
      }
      Log("[DEBUG-skin-v12-INDICES] seq=%u indices=%zu mismatches=%zu outOfRange=%zu", seq, count, mismatches, outOfRange);
    });
  } else Log("[DEBUG-skin-v12-GAP] seq=%u capability=decoded-indices reason=method-unavailable", seq);
}
#include "eiem_entry_native_skin_upload_observation.h"
#include "eiem_entry_native_render_observation.h"
void ReadRenderer(void *renderer, const char *phase) {
  if (!enabled || !renderer || samples.load() >= sampleLimit) return;
  // A Renderer passed to _Init may be an ordinary MeshRenderer. Inspect its
  // declared type before invoking any SkinnedMeshRenderer-only methods.
  auto klass = il2cpp_object_get_class(renderer);
  bool skinned = false;
  for (unsigned depth = 0; klass && depth < 32; ++depth) {
    const char *name = il2cpp_class_get_name(klass), *ns = il2cpp_class_get_namespace(klass);
    if (name && ns && !strcmp(name, "SkinnedMeshRenderer") && !strcmp(ns, "UnityEngine")) { skinned = true; break; }
    klass = il2cpp_class_get_parent(klass);
  }
  if (!skinned) return;
  auto mesh = Invoke(api.mesh, renderer);
  if (!mesh) return;
  auto meshName = Name(mesh);
  auto packages = std::atomic_load(&published);
  if (!packages) return;
  auto expected = Match(*packages, meshName);
  if (!expected.mesh) return;
  NativeUpload::Guard([&] { NativeUpload::Remember(renderer, mesh, expected); }, "registration");
  NativeRender::Guard([&] { NativeRender::Remember(mesh, expected); }, "layout-registration");
  unsigned seq = 0;
  {
    std::lock_guard<std::mutex> lock(sampleMutex);
    if (samples.load() >= sampleLimit || !seen.insert({renderer, phase}).second) return;
    seq = ++samples;
  }
  auto palette = Invoke(api.bones, renderer);
  size_t count = 0;
  Require(ArrayLength(palette, &count) && count <= 256, "readback palette range");
  auto root = Invoke(api.root, renderer);
  size_t matches = 0, nulls = 0, rootCount = 0, mismatchLogs = 0;
  std::vector<void *> bones(count);
  Require(!count || CopyRaw((char *)palette + 32, bones.data(), count * sizeof(void *)), "readback palette bytes");
  for (size_t slot = 0; slot < count; ++slot) {
    auto bone = bones[slot];
    if (!bone) { ++nulls; continue; }
    rootCount += bone == root;
    auto path = Path(bone);
    bool match = slot < expected.mesh->bones.size() && SuffixPath(path, expected.mesh->bones[slot]);
    matches += match;
    if (!match && mismatchLogs++ < 12)
      Log("[DEBUG-skin-v10-SLOT] seq=%u slot=%zu actual=%s expected=%s", seq, slot, path.c_str(),
          slot < expected.mesh->bones.size() ? expected.mesh->bones[slot].c_str() : "<out-of-range>");
  }
  auto owner = Invoke(api.transform, renderer);
  Log("[DEBUG-skin-v10-RENDERER] seq=%u phase=%s renderer=%p mesh=%p meshName=%s part=%s "
      "vertices=%d expectedVertices=%u bones=%zu expectedBones=%zu pathsMatched=%zu nullBones=%zu rootOccurrences=%zu owner=%s",
      seq, phase, renderer, mesh, meshName.c_str(), expected.part.c_str(), Scalar(api.vertices, mesh),
      expected.mesh->vertices, count, expected.mesh->bones.size(), matches, nulls, rootCount, Path(owner).c_str());
  // Attribute getters are legal for non-readable Meshes and describe the
  // native layout after deserialization, instead of trusting input channels.
  if (api.attributeCount && api.attributeDescriptor) {
    int attributes = Scalar(api.attributeCount, mesh);
    Require(attributes >= 0 && attributes <= 32, "native attribute count range");
    for (int index = 0; index < attributes; ++index) {
      void *args[] = {&index};
      auto boxed = Invoke(api.attributeDescriptor, mesh, args);
      auto descriptorClass = boxed ? il2cpp_object_get_class(boxed) : nullptr;
      uint32_t alignment = 0;
      Require(descriptorClass && il2cpp_class_value_size(descriptorClass, &alignment) == 16,
              "native attribute descriptor layout");
      int fields[4] = {};
      Require(CopyRaw(il2cpp_object_unbox(boxed), fields, sizeof(fields)), "native attribute descriptor read");
      Log("[DEBUG-skin-v11-CHANNEL] seq=%u index=%d attr=%d format=%d dimension=%d stream=%d",
          seq, index, fields[0], fields[1], fields[2], fields[3]);
    }
  } else {
    Log("[DEBUG-skin-v11-GAP] seq=%u capability=vertex-attributes reason=method-unavailable", seq);
  }
  // Bindpose readback can be disallowed by native readability policy. Record
  // that gap; never toggle readability or write a substitute runtime Mesh.
  void *exception = nullptr;
  auto poses = api.bindposes ? il2cpp_runtime_invoke(api.bindposes, mesh, nullptr, &exception) : nullptr;
  size_t poseCount = 0;
  bool posesAvailable = !exception && ArrayLength(poses, &poseCount) && poseCount <= 256;
  double maxError = -1;
  if (posesAvailable && poseCount == expected.mesh->bones.size()) {
    const auto &resource = *expected.package->resources.at(expected.mesh->resource);
    const auto &block = resource.blocks.at("m_BindPose");
    Require(block.size == 4 + poseCount * 64 && Le(resource.bytes, block.offset, 4) == poseCount,
            "readback expected bindpose block range");
    std::vector<Matrix4x4> matrices(poseCount);
    Require(!poseCount || CopyRaw((char *)poses + 32, matrices.data(), poseCount * sizeof(Matrix4x4)),
            "readback bindpose bytes");
    maxError = 0;
    for (size_t slot = 0; slot < poseCount; ++slot) for (size_t row = 0; row < 4; ++row) for (size_t col = 0; col < 4; ++col) {
      float expectedValue = 0;
      // Serialized Matrix4x4f fields are row ordered; the managed Unity
      // Matrix4x4 array is column ordered. Compare matrix elements by meaning.
      memcpy(&expectedValue, resource.bytes.data() + block.offset + 4 + slot * 64 + (row * 4 + col) * 4, 4);
      float actual = matrices[slot].m[row + col * 4];
      Require(std::isfinite(actual) && std::isfinite(expectedValue), "nonfinite native bindpose readback");
      maxError = std::max(maxError, std::abs(double(actual) - expectedValue));
    }
  }
  Log("[DEBUG-skin-v10-BINDPOSE] seq=%u readable=%d available=%d count=%zu expected=%zu maxElementError=%.9g",
      seq, api.readable ? int(Boolean(api.readable, mesh)) : -1, posesAvailable,
      posesAvailable ? poseCount : 0, expected.mesh->bones.size(), maxError);
  if (!strcmp(phase, "Controller.after")) {
    bool first = false;
    { std::lock_guard<std::mutex> lock(sampleMutex); first = geometrySeen.insert(mesh).second; }
    if (first) Observe([&] {
      auto input = AuthorVertices(*expected.package->resources.at(expected.mesh->resource));
      ReadGeometry(seq, mesh, expected, input);
    });
  }
}
void ReadList(void *list, const char *phase) {
  if (!enabled || !list) return;
  auto klass = il2cpp_object_get_class(list);
  void *itemsField = nullptr, *sizeField = nullptr, *iterator = nullptr;
  while (auto field = il2cpp_class_get_fields(klass, &iterator)) {
    auto name = il2cpp_field_get_name(field);
    if (name && !strcmp(name, "_items")) itemsField = field;
    if (name && !strcmp(name, "_size")) sizeField = field;
  }
  Require(itemsField && sizeField, "readback Renderer List fields absent");
  void *items = nullptr; int size = 0;
  il2cpp_field_get_value(list, itemsField, &items);
  il2cpp_field_get_value(list, sizeField, &size);
  size_t capacity = 0;
  Require(size >= 0 && size <= 2048 && ArrayLength(items, &capacity) && size_t(size) <= capacity,
          "readback Renderer List range");
  for (int i = 0; i < size && samples.load() < sampleLimit; ++i) {
    void *renderer = nullptr;
    Require(CopyRaw((char *)items + 32 + i * sizeof(void *), &renderer, sizeof(renderer)), "Renderer List element");
    Observe([&] { ReadRenderer(renderer, phase); });
  }
}
void __fastcall RendererInit(void *self, void *renderer, void *configs, void *info) {
  Observe([&] { ReadRenderer(renderer, "RendererInfo.before"); });
  rendererInit(self, renderer, configs, info);
  Observe([&] { ReadRenderer(renderer, "RendererInfo.after"); });
}
void __fastcall ControllerInit(void *self, void *renderers, void *configs, void *properties, bool bounds, void *info) {
  Observe([&] { ReadList(renderers, "Controller.before"); });
  controllerInit(self, renderers, configs, properties, bounds, info);
  Observe([&] { ReadList(renderers, "Controller.after"); });
}
void __fastcall SetBones(void *self, void *bones, void *info) {
  setBones(self, bones, info);
  Observe([&] { ReadRenderer(self, "Game.set_bones.after"); });
}
void InstallReadback(void **assemblies, size_t count) {
  if (!std::filesystem::exists(gameDirectory / "plugin" / "resource-input-diagnostics" / "skinning-observation.enabled")) return;
  auto object = FindClass("UnityEngine", "Object", assemblies, count);
  auto component = FindClass("UnityEngine", "Component", assemblies, count);
  auto transform = FindClass("UnityEngine", "Transform", assemblies, count);
  auto renderer = FindClass("UnityEngine", "SkinnedMeshRenderer", assemblies, count);
  auto mesh = FindClass("UnityEngine", "Mesh", assemblies, count);
  api.name = Exact(object, "get_name", {}, "System.String", false);
  api.transform = Exact(component, "get_transform", {}, "UnityEngine.Transform", false);
  api.parent = Exact(transform, "get_parent", {}, "UnityEngine.Transform", false);
  api.mesh = Exact(renderer, "get_sharedMesh", {}, "UnityEngine.Mesh", false);
  api.bones = Exact(renderer, "get_bones", {}, "UnityEngine.Transform[]", false);
  api.root = Exact(renderer, "get_rootBone", {}, "UnityEngine.Transform", false);
  api.vertices = Exact(mesh, "get_vertexCount", {}, "System.Int32", false);
  api.readable = Exact(mesh, "get_isReadable", {}, "System.Boolean", false);
  api.bindposes = Exact(mesh, "get_bindposes", {}, "UnityEngine.Matrix4x4[]", false);
  api.attributeCount = Exact(mesh, "GetVertexAttributeCountImpl", {}, "System.Int32", false);
  api.attributeDescriptor = Exact(mesh, "GetVertexAttribute", {"System.Int32"},
      "UnityEngine.Rendering.VertexAttributeDescriptor", false);
  api.weights = Exact(mesh, "GetBoneWeightsImpl", {}, "UnityEngine.BoneWeight[]", false);
  api.channelArray = Exact(mesh, "GetAllocArrayFromChannelImpl", {"UnityEngine.Rendering.VertexAttribute",
      "UnityEngine.Rendering.VertexAttributeFormat", "System.Int32"}, "System.Array", false);
  api.indices = Exact(mesh, "GetIndicesImpl", {"System.Int32", "System.Boolean"}, "System.Int32[]", false);
  for (auto field : {std::make_pair("name", api.name), {"transform", api.transform}, {"parent", api.parent},
      {"mesh", api.mesh}, {"bones", api.bones}, {"root", api.root}, {"vertices", api.vertices},
      {"readable", api.readable}, {"bindposes", api.bindposes}, {"attributeCount", api.attributeCount},
      {"attributeDescriptor", api.attributeDescriptor}, {"decodedWeights", api.weights},
      {"decodedPositions", api.channelArray}, {"decodedIndices", api.indices}})
    Log("[DEBUG-skin-v11-API] capability=%s available=%d", field.first, field.second != nullptr);
  Require(api.name && api.transform && api.parent && api.mesh && api.bones && api.root && api.vertices,
          "essential skinning observation API incomplete");
  void *infoClass = nullptr;
  for (size_t i = 0; i < count; ++i) {
    auto image = il2cpp_assembly_get_image(assemblies[i]);
    for (size_t j = 0; image && j < il2cpp_image_get_class_count(image); ++j) {
      auto klass = il2cpp_image_get_class(image, j);
      auto name = klass ? il2cpp_class_get_name(klass) : nullptr;
      if (name && !strcmp(name, "RendererInfo") && Exact(klass, "_Init",
          {"UnityEngine.Renderer", "System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>"}, "System.Void", false)) {
        Require(!infoClass, "ambiguous RendererInfo readback boundary"); infoClass = klass;
      }
    }
  }
  enabled = true;
  Require(Install(infoClass, "_Init", {"UnityEngine.Renderer", "System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>"},
      "System.Void", false, (void *)RendererInit, (void **)&rendererInit), "RendererInfo observation hook absent");
  auto controller = FindClass("Beyond.Rendering", "EntityRenderHelperMaterialController", assemblies, count);
  Require(Install(controller, "Init", {"System.Collections.Generic.List<UnityEngine.Renderer>",
      "System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>",
      "Beyond.Rendering.EntityCustomizeRendererPropertyConfig", "System.Boolean"}, "System.Void", false,
      (void *)ControllerInit, (void **)&controllerInit), "Renderer controller observation hook absent");
  Require(Install(renderer, "set_bones", {"UnityEngine.Transform[]"}, "System.Void", false,
      (void *)SetBones, (void **)&setBones), "bone setter observation hook absent");
  Log("[DEBUG-skin-v10-READY] maxSamples=%u writes=0 nativeReplacementRoute=serialized-inputs", sampleLimit);
  NativeUpload::Guard([&] { NativeUpload::Install(object); }, "installation");
  NativeRender::Guard([&] { NativeRender::Install(); }, "installation");
}
} // namespace SkinningObservation
