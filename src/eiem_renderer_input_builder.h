#pragma once
#include "eiem_mesh_input_builder.h"
#include "eiem_texture_input_builder.h"
#include <cfloat>
#include <functional>
#include <chrono>
#include <mutex>

namespace EiemNativeInput {
using NativeReference = std::pair<std::string, int64_t>;
using MeshSchemaIndex = std::map<std::array<unsigned char, 16>, Type>;
inline Value EmptyValue(const Type& type, size_t index, unsigned depth = 0) {
  Require(index < type.nodes.size() && depth < 128, "new serialized value schema limit");
  Value value;
  value.node = index;
  const auto& node = type.nodes[index];
  auto width = Primitive(node.type);
  if (width) value.raw.resize(width);
  else if (node.type != "string" && node.type != "TypelessData" && node.type != "Array") {
    Require(!node.children.empty(), "unsupported new serialized leaf");
    for (auto child : node.children) value.children.push_back(EmptyValue(type, child, depth + 1));
  }
  return value;
}
inline Value ArrayElement(const Type& type, const Value& array) {
  Require(type.nodes.at(array.node).type == "Array" &&
              type.nodes[array.node].children.size() == 2, "new serialized array element schema");
  return EmptyValue(type, type.nodes[array.node].children[1]);
}
inline const Value& ConstArray(const Type& type, const Value& value) {
  if (type.nodes.at(value.node).type == "Array") return value;
  Require(value.children.size() == 1 &&
              type.nodes.at(value.children[0].node).type == "Array", "not a serialized array");
  return value.children[0];
}
inline NativeReference NullableReference(const Type& type, const Value& value,
                                          const SerializedLayout& layout,
                                          const std::string& cab) {
  auto file = Integer(Child(type, value, "m_FileID"));
  Require(file <= layout.externals.size(), "serialized dependency file index");
  return {file ? layout.externals[(size_t)file - 1] : cab,
          (int64_t)Integer(Child(type, value, "m_PathID"))};
}
inline void SetReference(const Type& type, Value& value, const NativeReference& reference,
                           const SerializedLayout& layout, const std::string& cab) {
  size_t file = 0;
  if (reference.first != cab) {
    auto found = std::find(layout.externals.begin(), layout.externals.end(), reference.first);
    Require(found != layout.externals.end(), "new dependency requires serialized external registration: " + reference.first);
    file = size_t(found - layout.externals.begin()) + 1;
  }
  SetInteger(type, Child(type, value, "m_FileID"), file);
  SetInteger(type, Child(type, value, "m_PathID"), (uint64_t)reference.second);
}
inline double FloatValue(const Type& type, const Value& value) {
  Require(type.nodes.at(value.node).type == "float" && value.raw.size() == 4,
          "serialized float schema");
  float number;
  memcpy(&number, value.raw.data(), 4);
  Require(std::isfinite(number), "nonfinite serialized geometry float");
  return number;
}
using Vector3 = std::array<double, 3>;
inline Vector3 ReadVector(const Type& type, const Value& value) {
  return {FloatValue(type, Child(type, value, "x")), FloatValue(type, Child(type, value, "y")),
          FloatValue(type, Child(type, value, "z"))};
}
inline void SetVector(const Type& type, Value& value, const Vector3& vector) {
  for (size_t i = 0; i < 3; ++i) SetFloat(type, Child(type, value, std::string(1, "xyz"[i])), vector[i]);
}
inline Value AuthorField(const Type& type, const Value& source,
                           const Resource& resource, const char* name) {
  const auto& block = resource.blocks.at(name);
  Bytes bytes(resource.bytes.begin() + block.offset, resource.bytes.begin() + block.offset + block.size);
  return DecodeValue(type, Child(type, source, name).node, bytes);
}
inline std::pair<Vector3, Vector3> PartBounds(const Type& type, const Value& source,
                                              const Resource& resource, const Mesh& mesh,
                                              size_t rootSlot) {
  auto vertex = AuthorField(type, source, resource, "m_VertexData");
  const auto& channels = ConstArray(type, Child(type, vertex, "m_Channels"));
  Require(channels.children.size() == 14 && mesh.vertices && rootSlot < mesh.bones.size(),
          "Renderer bounds geometry/palette count");
  const auto& position = channels.children[0];
  Require(Integer(Child(type, position, "stream")) == 0 &&
              Integer(Child(type, position, "offset")) == 0 &&
              Integer(Child(type, position, "format")) == 0 &&
              Integer(Child(type, position, "dimension")) == 3,
          "unsupported Renderer position stream");
  const auto storage = DescribeMeshVertexStorage(type, vertex);
  const auto& data = Child(type, vertex, "m_DataSize").raw;
  Require(storage.count == mesh.vertices && uint64_t(mesh.vertices) * storage.stride[0] <= data.size(),
          "Renderer bounds vertex range");
  std::vector<Vector3> lower(mesh.vertices), upper(mesh.vertices);
  for (size_t i = 0; i < mesh.vertices; ++i)
    for (size_t axis = 0; axis < 3; ++axis) {
      float number;
      memcpy(&number, data.data() + i * storage.stride[0] + axis * 4, 4);
      Require(std::isfinite(number), "Renderer bounds nonfinite position");
      lower[i][axis] = upper[i][axis] = number;
    }
  auto shapes = AuthorField(type, source, resource, "m_Shapes");
  const auto& shapeVertices = ConstArray(type, Child(type, shapes, "vertices"));
  const auto& frames = ConstArray(type, Child(type, shapes, "shapes"));
  const auto& weights = ConstArray(type, Child(type, shapes, "fullWeights"));
  Require(weights.raw.size() == frames.count * 4, "shape bounds frame weights");
  // Add each channel's envelope so simultaneous shape weights remain inside it.
  for (const auto& channel : ConstArray(type, Child(type, shapes, "channels")).children) {
    auto first = Integer(Child(type, channel, "frameIndex")), count = Integer(Child(type, channel, "frameCount"));
    Require(count && first <= frames.count && count <= frames.count - first, "shape bounds channel range");
    std::vector<Vector3> low(mesh.vertices), high(mesh.vertices);
    for (size_t f = (size_t)first; f < first + count; ++f) {
      const auto& frame = frames.children[f];
      auto start = Integer(Child(type, frame, "firstVertex")), length = Integer(Child(type, frame, "vertexCount"));
      Require(start <= shapeVertices.count && length <= shapeVertices.count - start, "shape bounds vertex range");
      float weight;
      memcpy(&weight, weights.raw.data() + f * 4, 4);
      Require(std::isfinite(weight) && weight > 0, "shape bounds positive frame weight required");
      std::set<size_t> visited;
      for (size_t v = (size_t)start; v < start + length; ++v) {
        const auto& item = shapeVertices.children[v];
        auto index = Integer(Child(type, item, "index"));
        Require(index < mesh.vertices && visited.insert((size_t)index).second, "shape bounds duplicate/out-of-range vertex");
        auto delta = ReadVector(type, Child(type, item, "vertex"));
        for (size_t axis = 0; axis < 3; ++axis) {
          low[(size_t)index][axis] = std::min(low[(size_t)index][axis], delta[axis] * (100. / weight));
          high[(size_t)index][axis] = std::max(high[(size_t)index][axis], delta[axis] * (100. / weight));
        }
      }
    }
    for (size_t i = 0; i < mesh.vertices; ++i)
      for (size_t axis = 0; axis < 3; ++axis) { lower[i][axis] += low[i][axis]; upper[i][axis] += high[i][axis]; }
  }
  auto bindposes = AuthorField(type, source, resource, "m_BindPose");
  const auto& palette = ConstArray(type, bindposes);
  Require(palette.count == mesh.bones.size(), "Renderer bounds bindpose count");
  double matrix[4][4];
  for (size_t row = 0; row < 4; ++row)
    for (size_t column = 0; column < 4; ++column)
      matrix[row][column] = FloatValue(type, Child(type, palette.children[rootSlot],
          "e" + std::to_string(row) + std::to_string(column)));
  Require(matrix[3][0] == 0 && matrix[3][1] == 0 && matrix[3][2] == 0 && matrix[3][3] == 1,
          "Renderer bounds requires affine root bindpose");
  Vector3 minimum{DBL_MAX, DBL_MAX, DBL_MAX}, maximum{-DBL_MAX, -DBL_MAX, -DBL_MAX};
  for (size_t i = 0; i < mesh.vertices; ++i)
    for (size_t axis = 0; axis < 3; ++axis) {
      double center = matrix[axis][3], radius = 0;
      for (size_t k = 0; k < 3; ++k) {
        center += matrix[axis][k] * ((lower[i][k] + upper[i][k]) * .5);
        radius += std::abs(matrix[axis][k]) * ((upper[i][k] - lower[i][k]) * .5);
      }
      minimum[axis] = std::min(minimum[axis], center - radius);
      maximum[axis] = std::max(maximum[axis], center + radius);
    }
  Vector3 center, extent;
  for (size_t i = 0; i < 3; ++i) { center[i] = (minimum[i] + maximum[i]) * .5; extent[i] = (maximum[i] - minimum[i]) * .5; }
  return {center, extent};
}
struct RendererPartWrite {
  std::string cab, rule, part, meshCab, materialCab;
  int64_t sourceRenderer = 0, renderer = 0, owner = 0, transform = 0, parent = 0, mesh = 0, material = 0;
  std::vector<int64_t> bones;
};
struct RendererInputBuild {
  Bytes bytes;
  size_t matches = 0, skipped = 0, changedObjects = 0, addedObjects = 0, lodGroups = 0, preloadReferences = 0;
  std::vector<RendererPartWrite> parts;
};
// Scoped to one immutable author Package/build. Skeleton pointers and palettes
// are still resolved for each Renderer; only the authored local AABB is reused.
struct RendererBoundsCache {
  std::mutex mutex;
  const Package* package = nullptr;
  std::map<std::pair<std::string,size_t>, std::pair<Vector3,Vector3>> values;
  size_t hits = 0, misses = 0;
  double computeMs = 0;
};
class RendererMemberBuilder {
  const Bytes& source;
  const std::string cab;
  const Package& package;
  const AssetIndex& assets;
  const std::vector<BuiltMeshPart>& meshes;
  SerializedLayout layout;
  std::map<int64_t, Value> values;
  std::map<int64_t, int64_t> transforms, parents, roots;
  std::map<int64_t, std::string> paths;
  std::map<int64_t, std::map<std::string, int64_t>> hierarchy;
  std::map<int64_t, std::vector<int64_t>> rendererExpansion;
  std::map<int64_t, std::vector<NativeReference>> preloadExpansion;
  std::set<int64_t> changed, allocated;
  std::vector<AddedObject> added;
  RendererInputBuild result;
  RendererBoundsCache localBounds;
  RendererBoundsCache* boundsCache = &localBounds;
  const Type& Schema(int64_t id) const { return layout.types.at(layout.objects.at(id).type); }
  Value& Tree(int64_t id, int expected = 0) {
    Require(layout.objects.count(id) && (!expected || Schema(id).classId == expected), "Renderer local object absent/class mismatch");
    auto found = values.find(id);
    if (found == values.end()) found = values.emplace(id, ObjectValue(source, layout, id)).first;
    return found->second;
  }
  int64_t Local(const Type& type, const Value& pointer, int expected) {
    auto reference = NullableReference(type, pointer, layout, cab);
    Require(reference.first == cab && reference.second, "Renderer hierarchy/bone pointer must be local and nonnull");
    Tree(reference.second, expected);
    return reference.second;
  }
  int64_t Allocate(const char* kind, int64_t renderer, const std::string& part) {
    auto label = cab + ":" + package.id + ":" + kind + ":" + std::to_string(renderer) + ":" + part;
    auto hash = Sha((const unsigned char*)label.data(), label.size());
    auto id = (int64_t)Le(Bytes(hash.begin(), hash.end()), 0, 8);
    Require(id && !layout.objects.count(id) && allocated.insert(id).second, "Renderer added PathID collision");
    return id;
  }
  void Add(int64_t id, int64_t donor, const Value& value) {
    added.push_back({id, layout.objects.at(donor).type, EncodeValue(Schema(donor), value)});
  }
  std::vector<int64_t> ResolveBones(int64_t ownerTransform, int64_t rootBone,
                                      const Mesh& mesh) const {
    std::set<int64_t> ownerAncestors;
    for (auto id = ownerTransform; id; id = parents.at(id)) ownerAncestors.insert(id);
    const auto& index = hierarchy.at(roots.at(ownerTransform));
    std::vector<std::vector<int64_t>> candidates;
    for (auto anchor = rootBone; anchor; anchor = parents.at(anchor)) {
      if (!ownerAncestors.count(anchor)) continue;
      std::vector<int64_t> bones;
      auto prefix = paths.at(anchor);
      for (const auto& path : mesh.bones) {
        auto found = index.find(prefix.empty() ? path : prefix + "/" + path);
        if (found == index.end() || !found->second) { bones.clear(); break; }
        bones.push_back(found->second);
      }
      if (!bones.empty() && std::count(bones.begin(), bones.end(), rootBone) == 1)
        candidates.push_back(std::move(bones));
    }
    Require(candidates.size() == 1, "Renderer bone paths have no unique local hierarchy anchor");
    return candidates.front();
  }
  void Hierarchy() {
    for (auto id : layout.order) {
      if (Schema(id).classId != 4) continue;
      const auto& type = Schema(id);
      const auto& tree = Tree(id);
      auto owner = Local(type, Child(type, tree, "m_GameObject"), 1);
      Require(transforms.emplace(owner, id).second, "GameObject has multiple Transforms");
      auto parent = NullableReference(type, Child(type, tree, "m_Father"), layout, cab);
      Require(parent.first == cab && (!parent.second || Schema(parent.second).classId == 4), "nonlocal/invalid Transform parent");
      parents[id] = parent.second;
    }
    std::set<int64_t> visiting;
    std::function<void(int64_t)> visit = [&](int64_t id) {
      if (roots.count(id)) return;
      Require(visiting.size() < 512 && visiting.insert(id).second, "Transform hierarchy cycle/depth");
      auto parent = parents.at(id);
      if (parent) {
        visit(parent);
        auto owner = Local(Schema(id), Child(Schema(id), Tree(id), "m_GameObject"), 1);
        const auto& name = Child(Schema(owner), Tree(owner), "m_Name");
        auto text = Text(name.raw.data(), name.raw.size());
        Require(!text.empty() && text.find('/') == text.npos, "ambiguous Transform path component");
        paths[id] = paths[parent].empty() ? text : paths[parent] + "/" + text;
        roots[id] = roots[parent];
      } else { roots[id] = id; paths[id] = ""; }
      auto& index = hierarchy[roots[id]];
      auto found = index.find(paths[id]);
      if (found == index.end()) index.emplace(paths[id], id);
      else found->second = 0; // Only selected ambiguous bone paths are rejected.
      visiting.erase(id);
    };
    for (const auto& parent : parents) visit(parent.first);
    for (const auto& row : parents) {
      const auto& type = Schema(row.first);
      for (const auto& child : ConstArray(type, Child(type, Tree(row.first), "m_Children")).children) {
        auto id = Local(type, child, 4);
        Require(parents.at(id) == row.first, "Transform child/father reference mismatch");
      }
      if (row.second) {
        size_t count = 0;
        auto& parentType = Schema(row.second);
        for (const auto& child : ConstArray(parentType, Child(parentType, Tree(row.second), "m_Children")).children)
          if (Local(parentType, child, 4) == row.first) ++count;
        Require(count == 1, "Transform absent/duplicate in parent children");
      }
    }
  }
  bool Preloads(const Type& type, Value& bundle) {
    auto& preloads = ArrayValue(type, Child(type, bundle, "m_PreloadTable"));
    std::vector<Value> output;
    std::vector<size_t> offsets{0};
    std::set<int64_t> referenced;
    for (const auto& pointer : preloads.children) {
      output.push_back(pointer);
      auto reference = NullableReference(type, pointer, layout, cab);
      auto extras = preloadExpansion.find(reference.second);
      if (reference.first == cab && extras != preloadExpansion.end()) {
        referenced.insert(reference.second);
        for (const auto& item : extras->second) {
          auto value = ArrayElement(type, preloads);
          SetReference(type, value, item, layout, cab);
          output.push_back(std::move(value));
          ++result.preloadReferences;
        }
      }
      offsets.push_back(output.size());
    }
    for (const auto& row : preloadExpansion)
      Require(referenced.count(row.first), "Renderer expansion absent from AssetBundle preload closure");
    if (output.size() == preloads.count) return false;
    auto update = [&](Value& info) {
      auto& start = Child(type, info, "preloadIndex"); auto& size = Child(type, info, "preloadSize");
      auto first = Integer(start), count = Integer(size);
      Require(first < offsets.size() && count <= offsets.size() - 1 - first, "Renderer source preload interval");
      SetInteger(type, start, offsets[(size_t)first]);
      SetInteger(type, size, offsets[(size_t)(first + count)] - offsets[(size_t)first]);
    };
    for (const char* name : {"m_Container", "m_HashContainer"})
      for (auto& pair : ArrayValue(type, Child(type, bundle, name)).children) update(Child(type, pair, "second"));
    update(Child(type, bundle, "m_MainAsset"));
    preloads.children = std::move(output); preloads.count = preloads.children.size();
    return true;
  }
public:
  RendererMemberBuilder(const Bytes& bytes, const std::string& name, const Package& mod,
                          const AssetIndex& graph, const std::vector<BuiltMeshPart>& meshParts)
      : source(bytes), cab(name), package(mod), assets(graph), meshes(meshParts), layout(ParseSerialized(bytes)) {}
  RendererInputBuild Build() {
    std::map<NativeReference, Identity> selected;
    for (const auto& target : package.targetIndex) {
      auto found = assets.find(target.first);
      if (found == assets.end()) continue;
      Require(found->second.classId == 43, "Renderer target is not a Mesh");
      Require(selected.emplace(NativeReference{found->second.cab, found->second.pathId}, target.first).second,
              "Renderer target aliases are ambiguous");
    }
    std::vector<std::pair<int64_t, Identity>> matches;
    for (auto id : layout.order) {
      if (Schema(id).classId != 137) continue;
      auto reference = NullableReference(Schema(id), Child(Schema(id), Tree(id), "m_Mesh"), layout, cab);
      auto found = selected.find(reference);
      if (reference.second && found != selected.end()) matches.emplace_back(id, found->second);
    }
    if (matches.empty()) { result.bytes = source; return result; }
    Hierarchy();
    for (const auto& matched : matches) {
      auto rid = matched.first;
      const auto& target = matched.second;
      const auto& ruleId = package.targetIndex.at(target);
      const auto& rule = package.rules.at(ruleId);
      const auto& type = Schema(rid);
      const auto original = Tree(rid);
      ++result.matches;
      if (rule.skip) {
        SetInteger(type, Child(type, Tree(rid), "m_Enabled"), 0);
        changed.insert(rid); rendererExpansion[rid] = {}; ++result.skipped;
        continue;
      }
      auto owner = Local(type, Child(type, original, "m_GameObject"), 1);
      Require(transforms.count(owner), "Renderer owner lacks Transform");
      auto tid = transforms.at(owner);
      auto rootBone = Local(type, Child(type, original, "m_RootBone"), 4);
      Local(type, Child(type, original, "m_SkinningRoot"), 4);
      Require(roots.at(rootBone) == roots.at(tid), "Renderer root bone belongs to another hierarchy");
      size_t rendererComponents = 0, transformComponents = 0;
      const auto& goType = Schema(owner);
      for (const auto& item : ConstArray(goType, Child(goType, Tree(owner), "m_Component")).children) {
        auto reference = NullableReference(goType, Child(goType, item, "component"), layout, cab);
        rendererComponents += reference == NativeReference{cab, rid};
        transformComponents += reference == NativeReference{cab, tid};
      }
      Require(rendererComponents == 1 && transformComponents == 1, "Renderer/GameObject component reference mismatch");
      std::set<NativeReference> dependencies;
      for (size_t i = 0; i < rule.parts.size(); ++i) {
        const auto& pid = rule.parts[i];
        const auto& part = package.parts.at(pid);
        const auto& mesh = package.meshes.at(part.mesh);
        auto nativeMesh = std::find_if(meshes.begin(), meshes.end(), [&](const BuiltMeshPart& item) {
          return item.part == pid && !(item.target < target) && !(target < item.target);
        });
        Require(nativeMesh != meshes.end(), "Renderer Mesh dependency not rebuilt");
        auto sourceMesh = assets.find(target);
        Require(sourceMesh != assets.end() && sourceMesh->second.type == package.resources.at(mesh.resource)->type,
                "Renderer author Mesh schema differs from current target");
        auto material = assets.find(package.materials.at(part.material).source);
        Require(material != assets.end() && material->second.classId == 21, "Renderer Material dependency absent from current graph");
        auto bones = ResolveBones(tid, rootBone, mesh);
        auto renderer = original;
        int64_t newRid = rid, gid = owner, newTid = tid;
        if (i) {
          newRid = Allocate("renderer", rid, pid); gid = Allocate("go", rid, pid); newTid = Allocate("transform", rid, pid);
          auto go = Tree(owner);
          SetString(goType, Child(goType, go, "m_Name"),
              Text(Child(goType, go, "m_Name").raw.data(), Child(goType, go, "m_Name").raw.size()) + "__EFF_" + package.id + "_" + pid);
          auto& components = ArrayValue(goType, Child(goType, go, "m_Component"));
          components.children.clear();
          for (auto component : {newTid, newRid}) {
            auto entry = ArrayElement(goType, components);
            SetReference(goType, Child(goType, entry, "component"), {cab, component}, layout, cab);
            components.children.push_back(std::move(entry));
          }
          components.count = 2;
          Add(gid, owner, go);
          const auto& transformType = Schema(tid);
          auto transform = Tree(tid);
          SetReference(transformType, Child(transformType, transform, "m_GameObject"), {cab, gid}, layout, cab);
          SetReference(transformType, Child(transformType, transform, "m_Father"), {cab, tid}, layout, cab);
          auto& children = ArrayValue(transformType, Child(transformType, transform, "m_Children"));
          children.children.clear(); children.count = 0;
          SetVector(transformType, Child(transformType, transform, "m_LocalPosition"), {0, 0, 0});
          SetVector(transformType, Child(transformType, transform, "m_LocalScale"), {1, 1, 1});
          auto& rotation = Child(transformType, transform, "m_LocalRotation");
          SetVector(transformType, rotation, {0, 0, 0}); SetFloat(transformType, Child(transformType, rotation, "w"), 1);
          auto& parentChildren = ArrayValue(transformType, Child(transformType, Tree(tid), "m_Children"));
          for (auto& field : transform.children)
            if (transformType.nodes[field.node].name == "m_RootOrder") SetInteger(transformType, field, parentChildren.count);
          auto child = ArrayElement(transformType, parentChildren);
          SetReference(transformType, child, {cab, newTid}, layout, cab);
          parentChildren.children.push_back(std::move(child)); parentChildren.count = parentChildren.children.size();
          changed.insert(tid); Add(newTid, tid, transform);
          for (auto id : {gid, newTid, newRid}) dependencies.insert({cab, id});
        }
        SetReference(type, Child(type, renderer, "m_GameObject"), {cab, gid}, layout, cab);
        SetReference(type, Child(type, renderer, "m_Mesh"), {nativeMesh->cab, nativeMesh->id}, layout, cab);
        auto& materials = ArrayValue(type, Child(type, renderer, "m_Materials"));
        auto materialPointer = ArrayElement(type, materials);
        SetReference(type, materialPointer, {material->second.cab, material->second.pathId}, layout, cab);
        materials.children = {materialPointer}; materials.count = 1;
        auto& palette = ArrayValue(type, Child(type, renderer, "m_Bones"));
        palette.children.clear();
        for (auto id : bones) {
          auto pointer = ArrayElement(type, palette); SetReference(type, pointer, {cab, id}, layout, cab);
          palette.children.push_back(std::move(pointer)); dependencies.insert({cab, id});
        }
        palette.count = bones.size();
        auto& weights = ArrayValue(type, Child(type, renderer, "m_BlendShapeWeights"));
        Require(Primitive(type.nodes[type.nodes[weights.node].children[1]].type) == 4, "Renderer shape weight schema");
        weights.children.clear(); weights.raw.clear(); weights.count = mesh.shapes.size();
        for (size_t shape=0;shape<mesh.shapes.size();++shape) {
          auto weight = ArrayElement(type, weights);
          SetFloat(type, weight, 0); // Runtime applies author state after registration.
          weights.raw.insert(weights.raw.end(), weight.raw.begin(), weight.raw.end());
        }
        // Dynamic visibility is combined with the native enabled request at runtime.
        // Preserve the original Renderer flag so inactive native consumers stay inactive.
        // Every part retains the source enabled flag; all visibility expressions,
        // including constants, are applied by the runtime binding.
        SetInteger(type, Child(type, renderer, "m_DirtyAABB"), 0);
        // Mesh schema is part of the validated author input; no original PFB is published in the Mod.
        const auto& resource = *package.resources.at(mesh.resource);
        auto bounds = Bounds(resource, mesh,
            size_t(std::find(bones.begin(), bones.end(), rootBone) - bones.begin()));
        auto& aabb = Child(type, renderer, "m_AABB");
        SetVector(type, Child(type, aabb, "m_Center"), bounds.first);
        SetVector(type, Child(type, aabb, "m_Extent"), bounds.second);
        if (i) Add(newRid, rid, renderer);
        else { Tree(rid) = std::move(renderer); changed.insert(rid); }
        rendererExpansion[rid].push_back(newRid);
        dependencies.insert({nativeMesh->cab, nativeMesh->id}); dependencies.insert({material->second.cab, material->second.pathId});
        result.parts.push_back({cab, ruleId, pid, nativeMesh->cab, material->second.cab,
            rid, newRid, gid, newTid, tid, nativeMesh->id, material->second.pathId, std::move(bones)});
      }
      preloadExpansion[rid] = {dependencies.begin(), dependencies.end()};
    }
    for (auto id : layout.order) {
      // These typed authored lists enumerate Renderers just like LOD arrays.
      // Scalar references keep the native anchor; list entries expand 0/1/N.
      if (Schema(id).classId == 114) {
        const auto& type=Schema(id);bool hasList=false;
        for(auto n:type.nodes.at(0).children)hasList|=type.nodes.at(n).name=="customRenderers";
        if(hasList){auto& list=ArrayValue(type,Child(type,Tree(id),"customRenderers"));std::vector<Value> output;bool modified=false;
          for(const auto& item:list.children){auto reference=NullableReference(type,item,layout,cab);auto expansion=rendererExpansion.find(reference.second);
            if(reference.first!=cab||expansion==rendererExpansion.end())output.push_back(item);
            else{for(auto renderer:expansion->second){auto entry=item;SetReference(type,entry,{cab,renderer},layout,cab);output.push_back(std::move(entry));}modified|=expansion->second!=std::vector<int64_t>{reference.second};}
          }list.children=std::move(output);list.count=list.children.size();if(modified)changed.insert(id);
        }
      }
      if (Schema(id).classId != 205) continue;
      const auto& type = Schema(id); auto& tree = Tree(id); bool modified = false;
      for (auto& lod : ArrayValue(type, Child(type, tree, "m_LODs")).children) {
        auto& renderers = ArrayValue(type, Child(type, lod, "renderers"));
        std::vector<Value> output;
        for (const auto& item : renderers.children) {
          auto reference = NullableReference(type, Child(type, item, "renderer"), layout, cab);
          auto expansion = rendererExpansion.find(reference.second);
          if (reference.first != cab || expansion == rendererExpansion.end()) output.push_back(item);
          else {
            for (auto renderer : expansion->second) {
              auto entry = item; SetReference(type, Child(type, entry, "renderer"), {cab, renderer}, layout, cab);
              output.push_back(std::move(entry));
            }
            modified |= expansion->second != std::vector<int64_t>{reference.second};
          }
        }
        renderers.children = std::move(output); renderers.count = renderers.children.size();
      }
      if (modified) { changed.insert(id); ++result.lodGroups; }
    }
    if (!preloadExpansion.empty()) {
      size_t bundles = 0;
      for (auto id : layout.order) if (Schema(id).classId == 142) {
        ++bundles;
        if (Preloads(Schema(id), Tree(id))) changed.insert(id);
      }
      Require(bundles == 1, "Renderer expansion requires one current AssetBundle");
    }
    std::map<int64_t, Bytes> replacements;
    for (auto id : changed) replacements.emplace(id, EncodeValue(Schema(id), Tree(id)));
    result.changedObjects = replacements.size(); result.addedObjects = added.size();
    result.bytes = BuildSerialized(source, layout, replacements, added);
    return result;
  }
private:
  // Supplied by the caller from the selected current Mesh input schema.
  const MeshSchemaIndex* meshSchemas = nullptr;
  std::pair<Type, Value> EmptyValueForMesh(const Resource& resource) const {
    Require(meshSchemas && meshSchemas->count(resource.type), "Renderer bounds needs current Mesh TypeTree");
    const auto& type = meshSchemas->at(resource.type);
    return {type, EmptyValue(type, 0)};
  }
  std::pair<Vector3,Vector3> Bounds(const Resource& resource, const Mesh& mesh, size_t rootSlot) {
    Require(boundsCache,"Renderer bounds cache absent");std::lock_guard<std::mutex> lock(boundsCache->mutex);
    Require(boundsCache && (!boundsCache->package || boundsCache->package == &package), "Renderer bounds cache author scope differs");boundsCache->package=&package;
    auto key=std::make_pair(mesh.resource,rootSlot);auto found=boundsCache->values.find(key);
    if(found!=boundsCache->values.end()){++boundsCache->hits;return found->second;}
    auto start=std::chrono::steady_clock::now();auto boundsTemplate=EmptyValueForMesh(resource);
    auto bounds=PartBounds(boundsTemplate.first,boundsTemplate.second,resource,mesh,rootSlot);
    boundsCache->values.emplace(std::move(key),bounds);++boundsCache->misses;
    boundsCache->computeMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return bounds;
  }
public:
  void SetMeshSchemas(const MeshSchemaIndex& schemas) { meshSchemas = &schemas; }
  void SetBoundsCache(RendererBoundsCache& cache) {
    std::lock_guard<std::mutex> lock(cache.mutex);
    Require(!cache.package || cache.package == &package, "Renderer bounds cache cannot cross author Packages");
    cache.package=&package;boundsCache=&cache;
  }
};
inline void AddMeshSchemas(MeshSchemaIndex& schemas, const Bytes& input) {
  auto bundle = EndfieldInput::Decode(input);
  for (const auto& member : bundle.members) {
    if (member.bytes.size() < 48 || member.bytes[8] || member.bytes[9] || member.bytes[10] || member.bytes[11] != 22) continue;
    auto layout = ParseSerialized(member.bytes);
    for (const auto& type : layout.types) if (type.classId == 43) schemas.emplace(type.hash, type);
  }
}
inline RendererInputBuild BuildRendererInput(const Bytes& source, const Package& package,
                                               const AssetIndex& assets,
                                               const std::vector<BuiltMeshPart>& meshParts,
                                               const MeshSchemaIndex& schemas,
                                               RendererBoundsCache* sharedBounds = nullptr) {
  auto bundle = EndfieldInput::Decode(source);
  RendererBoundsCache localBounds;auto& bounds=sharedBounds?*sharedBounds:localBounds;
  RendererInputBuild result;
  for (auto& member : bundle.members) {
    if (member.bytes.size() < 48 || member.bytes[8] || member.bytes[9] || member.bytes[10] || member.bytes[11] != 22) continue;
    RendererMemberBuilder builder(member.bytes, member.name, package, assets, meshParts);
    builder.SetMeshSchemas(schemas);
    builder.SetBoundsCache(bounds);
    auto rebuilt = builder.Build();
    result.matches += rebuilt.matches; result.skipped += rebuilt.skipped;
    result.changedObjects += rebuilt.changedObjects; result.addedObjects += rebuilt.addedObjects;
    result.lodGroups += rebuilt.lodGroups; result.preloadReferences += rebuilt.preloadReferences;
    result.parts.insert(result.parts.end(), rebuilt.parts.begin(), rebuilt.parts.end());
    member.bytes = std::move(rebuilt.bytes);
  }
  result.bytes = result.changedObjects ? EndfieldInput::Encode(source, bundle.members) : source;
  auto checked = EndfieldInput::Decode(result.bytes);
  Require(checked.members.size() == bundle.members.size(), "Renderer rebuilt member count");
  for (size_t i = 0; i < bundle.members.size(); ++i)
    Require(checked.members[i].name == bundle.members[i].name && checked.members[i].flags == bundle.members[i].flags &&
                checked.members[i].bytes == bundle.members[i].bytes, "Renderer rebuilt member identity/content differs");
  return result;
}
}  // namespace EiemNativeInput
