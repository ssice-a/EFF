#pragma once
#include "eiem_serialized_writer.h"

// Offline, read-only graph inspection. No native loads or identity rewriting.
namespace EiemLoadingGraphAudit {
using namespace EiemNativeInput;
struct External {
  size_t fileId = 0, recordOffset = 0, guidOffset = 0, pathOffset = 0, endOffset = 0;
  std::string assetPath, guid, path, member;
  int32_t type = 0;
};
inline std::string Basename(const std::string& path) {
  auto slash = path.find_last_of("/\\");
  return slash == path.npos ? path : path.substr(slash + 1);
}
inline std::vector<External> ReadExternals(const Bytes& bytes, const SerializedLayout& layout) {
  Reader reader(bytes, layout.tableEnd, false, 48 + layout.metadataSize);
  auto scripts = reader.Count();
  reader.Skip(scripts * 12);
  auto count = reader.Count();
  Require(count == layout.externals.size(), "audit external count differs");
  std::vector<External> result;
  for (size_t i = 0; i < count; ++i) {
    External item;
    item.fileId = i + 1; item.recordOffset = reader.p;
    item.assetPath = reader.Z(); item.guidOffset = reader.p;
    reader.Skip(16);
    item.guid = Hex(bytes.data() + item.guidOffset, 16);
    item.type = (int32_t)reader.Get(4); item.pathOffset = reader.p;
    item.path = reader.Z(); item.endOffset = reader.p;
    item.member = Basename(item.path);
    Require(item.member == layout.externals[i], "audit external identity differs");
    result.push_back(std::move(item));
  }
  return result;
}
struct Pointer {
  int64_t objectId = 0, pathId = 0;
  int32_t classId = 0, fileId = 0;
  std::string field;
};
struct Stream {
  int64_t objectId = 0;
  int32_t classId = 0;
  std::string field, path;
  uint64_t offset = 0, size = 0;
  size_t pathDataOffset = 0;
};
struct UnparsedObject { int64_t objectId = 0; int32_t classId = 0; std::string error; };
struct References {
  std::vector<UnparsedObject> unparsed;
  std::vector<Pointer> pointers;
  std::vector<Stream> streams;
};
inline bool ReferenceNode(const Node& node) {
  return node.type.rfind("PPtr<", 0) == 0 || node.type == "StreamingInfo" ||
         node.type == "StreamedResource";
}
inline const Value* Find(const Type& type, const Value& value, const std::string& name) {
  for (const auto& child : value.children)
    if (type.nodes.at(child.node).name == name) return &child;
  return nullptr;
}
inline void Walk(const Type& type, size_t index, Reader& reader, size_t base,
                 const std::vector<bool>& interesting, References& result,
                 int64_t objectId, int32_t classId, const std::string& field,
                 unsigned depth = 0) {
  Require(depth < type.nodes.size(), "audit reference schema cycle");
  const auto& node = type.nodes.at(index);
  if (!interesting.at(index)) { SkipValue(type, index, reader, base, depth); return; }
  if (ReferenceNode(node)) {
    auto start = reader.p;
    auto value = ReadValue(type, index, reader, base, depth);
    if (node.type.rfind("PPtr<", 0) == 0) {
      auto file = Find(type, value, "m_FileID"), id = Find(type, value, "m_PathID");
      Require(file && id && file->raw.size() == 4 && id->raw.size() == 8,
              "audit PPtr schema differs");
      if (Integer(*id)) result.pointers.push_back({objectId, (int64_t)Integer(*id),
                                                 classId, (int32_t)Integer(*file), field});
    } else {
      auto path = Find(type, value, "path");
      if (!path) path = Find(type, value, "m_Source");
      auto offset = Find(type, value, "offset");
      if (!offset) offset = Find(type, value, "m_Offset");
      auto size = Find(type, value, "size");
      if (!size) size = Find(type, value, "m_Size");
      Require(path && offset && size && type.nodes.at(path->node).type == "string",
              "audit stream schema differs");
      auto text = Text(path->raw.data(), path->raw.size());
      if (!text.empty() || Integer(*size))
        {
        auto fields = Fields(type, index, reader.b, start, reader.p, base);
        auto pathName = type.nodes.at(path->node).name;
        const auto& pathField = fields.at(pathName);
        result.streams.push_back({objectId, classId, field, text, Integer(*offset), Integer(*size), pathField.offset + 4});
      }
    }
    return;
  }
  if (node.type == "Array") {
    Require(node.children.size() == 2 && type.nodes[node.children[0]].name == "size",
            "audit array schema differs");
    auto count = reader.Count(reader.limit - reader.p);
    for (size_t i = 0; i < count; ++i)
      Walk(type, node.children[1], reader, base, interesting, result, objectId, classId,
           field + "[" + std::to_string(i) + "]", depth + 1);
  } else {
    Require(!node.children.empty(), "audit interesting leaf lacks schema");
    for (auto child : node.children)
      Walk(type, child, reader, base, interesting, result, objectId, classId,
           field + "." + type.nodes[child].name, depth + 1);
  }
  if (node.align || node.type == "string")
    reader.Skip((size_t(0) - (reader.p - base)) & 3);
}
inline References ReadReferences(const Bytes& bytes, const SerializedLayout& layout) {
  References result;
  std::vector<std::vector<bool>> masks;
  for (const auto& type : layout.types) {
    std::vector<bool> mask(type.nodes.size(), false);
    for (size_t i = type.nodes.size(); i-- > 0;) {
      mask[i] = ReferenceNode(type.nodes[i]);
      for (auto child : type.nodes[i].children) mask[i] = mask[i] || mask.at(child);
    }
    masks.push_back(std::move(mask));
  }
  for (const auto& entry : layout.objects) {
    const auto& object = entry.second;
    const auto& type = layout.types.at(object.type);
    Reader reader(bytes, object.offset, false, object.offset + object.size);
    References objectReferences;
    try {
      Walk(type, 0, reader, object.offset, masks.at(object.type), objectReferences, object.id,
           type.classId, type.nodes.at(0).name);
      Require(reader.p == reader.limit, "audit object payload suffix");
      result.pointers.insert(result.pointers.end(), objectReferences.pointers.begin(), objectReferences.pointers.end());
      result.streams.insert(result.streams.end(), objectReferences.streams.begin(), objectReferences.streams.end());
    } catch (const std::exception& error) {
      result.unparsed.push_back({object.id, type.classId, error.what()});
    }
  }
  return result;
}
}  // namespace EiemLoadingGraphAudit
