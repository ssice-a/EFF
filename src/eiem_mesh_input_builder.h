#pragma once
#include "eiem_serialized_writer.h"
#include "eiem_mesh_frame_storage.h"
#include "eiem_runtime_shape_identity.h"

namespace EiemNativeInput {
struct BuiltMeshPart {
  Identity target;
  std::string rule, part, cab;
  int64_t sourceId = 0, id = 0;
  unsigned vertices = 0, indices = 0;
  std::vector<uint32_t> sourceBoneHashes;
};
struct MeshInputBuild {
  Bytes bytes;
  size_t matches = 0, skipped = 0, changedObjects = 0, addedObjects = 0;
  std::vector<BuiltMeshPart> parts;
};
inline int64_t AllocateMeshId(const std::string& cab, const Package& package,
                               const Identity& target, const std::string& part) {
  auto label = cab + ":" + package.id + ":mesh:" + target.path + ":" + target.asset + ":" + part;
  auto hash = Sha((const unsigned char*)label.data(), label.size());
  uint64_t value = 0;
  for (size_t i = 0; i < 8; ++i) value |= uint64_t(hash[i]) << (8 * i);
  Require(value != 0, "allocated Mesh PathID is zero");
  return (int64_t)value;
}
inline Value ApplyMesh(const Type& type, const Value& source, const Resource& resource,
                         const Mesh& mesh, const std::string& name, bool runtimeShapes = false) {
  Require(type.classId == 43 && resource.kind == 1 && type.hash == resource.type,
          "Mesh serialized class/schema mismatch");
  Value result = source;
  // Author UV dimensions retain the source flags for channels 4 through 11.
  // Source meshes may have packed frames or ordinary Float32 normal/tangent channels.
  auto sourceVertex = Child(type, source, "m_VertexData");
  auto& sourceChannels = ArrayValue(type, Child(type, sourceVertex, "m_Channels"));
  Require(sourceChannels.children.size() == 14, "unsupported source Mesh channel count");
  const auto& normal = sourceChannels.children[1];
  const auto& tangent = sourceChannels.children[2];
  auto normalDimension = Integer(Child(type, normal, "dimension"));
  Require(Integer(Child(type, normal, "format")) == 0 &&
              ((normalDimension == 0x31 && Integer(Child(type, tangent, "dimension")) == 0) ||
               (normalDimension == 3 && Integer(Child(type, tangent, "dimension")) == 4 &&
                Integer(Child(type, tangent, "format")) == 0)),
          "unsupported source Mesh frame layout");
  for (const auto& block : resource.blocks) {
    if (block.first[0] == '@') continue;
    auto& field = Child(type, result, block.first);
    Bytes bytes(resource.bytes.begin() + block.second.offset,
                resource.bytes.begin() + block.second.offset + block.second.size);
    field = DecodeValue(type, field.node, bytes);
    Require(EncodeValue(type, field) == bytes, "Mesh field representation changed on decode");
  }
  auto& vertex = Child(type, result, "m_VertexData");
  auto& channels = ArrayValue(type, Child(type, vertex, "m_Channels"));
  Require(channels.children.size() == 14, "author Mesh channel count");
  for (size_t i = 4; i < 12; ++i) {
    auto& dimension = Child(type, channels.children[i], "dimension");
    auto low = Integer(dimension) & 15;
    if (low) SetInteger(type, dimension,
        (Integer(Child(type, sourceChannels.children[i], "dimension")) & 0xf0) | low);
  }
  Require(Integer(Child(type, vertex, "m_VertexCount")) == mesh.vertices,
          "Mesh vertex directory count differs from native field");
  auto submeshes = Child(type, result, "m_SubMeshes");
  auto& submeshArray = ArrayValue(type, submeshes);
  Require(submeshArray.count == 1 && submeshArray.children.size() == 1,
          "each Mesh part must have one submesh");
  auto& submesh = submeshArray.children[0];
  Require(Integer(Child(type, submesh, "indexCount")) == mesh.indices &&
              Integer(Child(type, submesh, "vertexCount")) == mesh.vertices &&
              Integer(Child(type, submesh, "firstByte")) == 0 &&
              Integer(Child(type, submesh, "firstVertex")) == 0 &&
              Integer(Child(type, submesh, "baseVertex")) == 0 &&
              Integer(Child(type, submesh, "topology")) == 0,
          "author Mesh must contain one complete triangle submesh");
  auto indexFormat = Integer(Child(type, result, "m_IndexFormat"));
  auto& indexBuffer = ArrayValue(type, Child(type, result, "m_IndexBuffer"));
  Require(indexFormat <= 1 && indexBuffer.raw.size() ==
              uint64_t(mesh.indices) * (indexFormat ? 4 : 2), "Mesh index directory count");
  size_t indexSize = indexFormat ? 4 : 2;
  for (size_t i = 0; i < indexBuffer.raw.size(); i += indexSize)
    Require(Le(indexBuffer.raw, i, indexSize) < mesh.vertices, "author Mesh index exceeds vertex count");
  auto bindposes = Child(type, result, "m_BindPose");
  auto boneHashes = Child(type, result, "m_BoneNameHashes");
  Require(ArrayValue(type, bindposes).count == mesh.bones.size() &&
              ArrayValue(type, boneHashes).count == mesh.bones.size(), "Mesh bone binding count");
  if(mesh.bones.empty())Require(Integer(Child(type,result,"m_BonesPerVertex"))==0&&
      (Integer(Child(type,channels.children[12],"dimension"))&15)==0&&
      (Integer(Child(type,channels.children[13],"dimension"))&15)==0&&
      ArrayValue(type,Child(type,result,"m_BonesAABB")).count==0,
      "unskinned Mesh contains native skinning data");
  auto shapes = Child(type, result, "m_Shapes");
  auto& shapeChannels = ArrayValue(type, Child(type, shapes, "channels"));
  Require(shapeChannels.children.size() == mesh.shapes.size(), "Mesh shape binding count");
  for (size_t i = 0; i < mesh.shapes.size(); ++i) {
    auto& shapeName = Child(type, shapeChannels.children[i], "name");
    Require(Text(shapeName.raw.data(), shapeName.raw.size()) == mesh.shapes[i],
            "Mesh shape binding name differs from native field");
  }
  ExpandShapedMeshFrames(type, result);
  // Shape capture snapshots use public CPU readback. Keep authored channels
  // intact; readability does not change shape count or the skinning layout.
  if (runtimeShapes) SetInteger(type, Child(type, result, "m_IsReadable"), 1);
  auto& stream = Child(type, result, "m_StreamData");
  Require(Integer(Child(type, stream, "offset")) == 0 &&
              Integer(Child(type, stream, "size")) == 0 &&
              Child(type, stream, "path").raw.empty() &&
              Integer(Child(type, result, "m_MeshCompression")) == 0,
          "author Mesh must contain its own uncompressed geometry");
  SetString(type, Child(type, result, "m_Name"), name);
  return result;
}
inline bool ExpandMeshPreloads(const Type& type, Value& bundle,
                                const std::map<int64_t, std::vector<int64_t>>& expansion) {
  auto& preloads = ArrayValue(type, Child(type, bundle, "m_PreloadTable"));
  std::vector<Value> output;
  std::vector<size_t> offsets{0};
  std::set<int64_t> referenced;
  for (const auto& pointer : preloads.children) {
    output.push_back(pointer);
    if (Integer(Child(type, pointer, "m_FileID")) == 0) {
      auto id = (int64_t)Integer(Child(type, pointer, "m_PathID"));
      auto extras = expansion.find(id);
      if (extras != expansion.end())
        for (auto added : extras->second) {
          auto reference = pointer;
          SetInteger(type, Child(type, reference, "m_PathID"), (uint64_t)added);
          output.push_back(std::move(reference));
          referenced.insert(added);
        }
    }
    offsets.push_back(output.size());
  }
  for (const auto& row : expansion)
    for (auto id : row.second)
      Require(referenced.count(id), "additional Mesh absent from current AssetBundle preload closure");
  if (output.size() == preloads.children.size()) return false;
  auto update = [&](Value& info) {
    auto& start = Child(type, info, "preloadIndex");
    auto& size = Child(type, info, "preloadSize");
    auto first = Integer(start), count = Integer(size);
    Require(first < offsets.size() && count <= offsets.size() - 1 - first,
            "AssetBundle source preload interval");
    SetInteger(type, start, offsets[(size_t)first]);
    SetInteger(type, size, offsets[(size_t)(first + count)] - offsets[(size_t)first]);
  };
  for (const char* name : {"m_Container", "m_HashContainer"}) {
    auto& container = ArrayValue(type, Child(type, bundle, name));
    for (auto& pair : container.children) update(Child(type, pair, "second"));
  }
  update(Child(type, bundle, "m_MainAsset"));
  preloads.children = std::move(output);
  preloads.count = preloads.children.size();
  return true;
}
inline MeshInputBuild BuildMeshMember(const Bytes& source, const std::string& cab,
                                       const Package& package) {
  auto layout = ParseSerialized(source);
  auto assets = SerializedAssets(source, cab, layout);
  MeshInputBuild result;
  std::map<int64_t, Bytes> changes;
  std::vector<AddedObject> added;
  std::map<int64_t, std::vector<int64_t>> expansion;
  std::set<int64_t> matched, allocated;
  for (const auto& asset : assets) {
    Identity identity{asset.path, asset.asset};
    auto selected = package.targetIndex.find(identity);
    if (selected == package.targetIndex.end()) continue;
    Require(asset.cab == cab && asset.classId == 43 &&
                matched.insert(asset.pathId).second, "ambiguous/nonlocal Mesh target in current input");
    ++result.matches;
    const auto& rule = package.rules.at(selected->second);
    if (rule.skip) {
      ++result.skipped;
      continue; // Skip changes the consumer Renderer, never deletes a shared Mesh.
    }
    const auto& object = layout.objects.at(asset.pathId);
    const auto& type = layout.types.at(object.type);
    auto templateValue = ObjectValue(source, layout, asset.pathId);
    for (size_t i = 0; i < rule.parts.size(); ++i) {
      const auto& partId = rule.parts[i];
      const auto& part = package.parts.at(partId);
      const auto& mesh = package.meshes.at(part.mesh);
      const auto& resource = *package.resources.at(mesh.resource);
      int64_t id = i == 0 ? asset.pathId : AllocateMeshId(cab, package, identity, partId);
      Require(i == 0 || (!layout.objects.count(id) && allocated.insert(id).second),
              "additional Mesh PathID collision");
      auto name = i == 0 ? asset.asset : asset.asset + "__EFF_" + package.id + "_" + partId;
      const bool runtimeShapes = !mesh.shapes.empty();
      name = RuntimePartName(package, partId, identity);
      auto value = ApplyMesh(type, templateValue, resource, mesh, name, runtimeShapes);
      auto bytes = EncodeValue(type, value);
      Require(EncodeValue(type, DecodeValue(type, 0, bytes)) == bytes,
              "rebuilt Mesh TypeTree roundtrip differs");
      if (i == 0) changes.emplace(id, std::move(bytes));
      else {
        added.push_back({id, object.type, std::move(bytes)});
        expansion[asset.pathId].push_back(id);
      }
      result.parts.push_back({identity, selected->second, partId, cab, asset.pathId,
                              id, mesh.vertices, mesh.indices});
    }
  }
  if (!expansion.empty()) {
    size_t bundles = 0;
    for (auto id : layout.order) {
      const auto& object = layout.objects.at(id);
      const auto& type = layout.types.at(object.type);
      if (type.classId != 142) continue;
      ++bundles;
      auto value = ObjectValue(source, layout, id);
      if (ExpandMeshPreloads(type, value, expansion))
        changes.emplace(id, EncodeValue(type, value));
    }
    Require(bundles == 1, "Mesh expansion requires one current AssetBundle object");
  }
  result.changedObjects = changes.size();
  result.addedObjects = added.size();
  result.bytes = BuildSerialized(source, layout, changes, added);
  return result;
}
inline MeshInputBuild BuildMeshInput(EndfieldInput::Bundle& bundle, const Package& package) {
  MeshInputBuild result;
  for (auto& member : bundle.members) {
    if (!SerializedHeader(member.bytes)) continue;
    auto rebuilt = BuildMeshMember(member.bytes, member.name, package);
    result.matches += rebuilt.matches;
    result.skipped += rebuilt.skipped;
    result.changedObjects += rebuilt.changedObjects;
    result.addedObjects += rebuilt.addedObjects;
    result.parts.insert(result.parts.end(), rebuilt.parts.begin(), rebuilt.parts.end());
    member.bytes = std::move(rebuilt.bytes);
  }
  return result;
}
inline MeshInputBuild BuildMeshInput(const Bytes& source, const Package& package) {
  Require(EndfieldInput::Recognized(source), "Mesh input writer requires current Endfield format");
  auto bundle = EndfieldInput::Decode(source);
  auto result = BuildMeshInput(bundle, package);
  result.bytes = result.changedObjects ? EndfieldInput::Encode(source, bundle.members) : source;
  auto checked = EndfieldInput::Decode(result.bytes);
  Require(checked.members.size() == bundle.members.size(), "rebuilt Mesh input member count");
  for (size_t i = 0; i < bundle.members.size(); ++i)
    Require(checked.members[i].name == bundle.members[i].name &&
                checked.members[i].flags == bundle.members[i].flags &&
                checked.members[i].bytes == bundle.members[i].bytes,
            "rebuilt Mesh input changed member identity/content");
  return result;
}
}  // namespace EiemNativeInput
