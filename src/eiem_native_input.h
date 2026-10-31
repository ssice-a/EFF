#pragma once
#include "eiem_resource_directory.h"
#include "eiem_author_byte_origins.h"
// Serialized loading-input parsing. No native Unity pointers, object setters
// or resource identity derived from a buffer fingerprint.
namespace EiemNativeInput {
using namespace EiemDirectory;
struct Reader {
  const Bytes& b;
  size_t p = 0, limit = 0;
  bool big = false;
  explicit Reader(const Bytes& bytes, size_t pos = 0, bool be = false, size_t stop = SIZE_MAX)
      : b(bytes), p(pos), limit(std::min(stop, bytes.size())), big(be) {
    Require(pos <= limit, "input position exceeds range");
  }
  uint64_t Get(size_t n) {
    Require(n <= 8 && p <= limit && n <= limit - p, "truncated native input");
    uint64_t v = 0;
    if (big) {
      for (size_t i = 0; i < n; ++i) v = (v << 8) | b[p + i];
    } else
      v = Le(b, p, n);
    p += n;
    return v;
  }
  size_t Count(size_t max = SIZE_MAX) {
    auto n = Get(4);
    Require(n <= max && n <= limit-p, "native count exceeds input extent");
    return (size_t)n;
  }
  void Skip(size_t n) {
    Require(p <= limit && n <= limit - p, "native range exceeds input");
    p += n;
  }
  void Align(size_t n = 4) { Skip((size_t(0) - p) & (n - 1)); }
  std::string Z() {
    size_t start = p;
    while (p < limit && b[p]) ++p;
    Require(p < limit, "unterminated native string");
    auto s = Text(b.data() + start, p - start);
    ++p;
    return s;
  }
  std::string String() {
    size_t n = Count(limit - p);
    Require(n <= limit - p, "truncated string");
    auto s = Text(b.data() + p, n);
    p += n;
    Align();
    return s;
  }
};
struct Node {
  std::string type, name;
  bool align = false;
  std::vector<size_t> children;
  size_t fixedWidth = 0; // Derived from the schema; never serialized.
  bool uniqueFields = false;
};
struct Type {
  int classId = 0;
  std::array<unsigned char, 16> hash{};
  std::vector<Node> nodes;
  // Exact native metadata is retained for merging new compiled component types.
  // Portable proofs still describe the schema; runtime reads these bytes from
  // the proven member rather than inventing script IDs or TypeTree records.
  Bytes nativeRecord;
  size_t dependencyOffset = 0;
};
struct Object {
  int64_t id = 0;
  size_t offset = 0, size = 0, type = 0;
};
struct SerializedLayout {
  size_t metadataSize = 0, dataOffset = 0;
  size_t countOffset = 0, tableStart = 0, tableEnd = 0;
  std::vector<Type> types;
  std::map<int64_t, Object> objects;
  std::vector<int64_t> order;
  std::vector<std::string> externals;
  size_t typeCountOffset = 0;
  size_t scriptCount = 0;
};
struct Field {
  size_t node = 0, offset = 0, size = 0;
};
inline size_t Primitive(const std::string& t) {
  if (t == "SInt8" || t == "UInt8" || t == "char" || t == "bool") return 1;
  if (t == "short" || t == "SInt16" || t == "UInt16" || t == "unsigned short") return 2;
  if (t == "int" || t == "SInt32" || t == "UInt32" || t == "unsigned int" || t == "Type*" ||
      t == "float")
    return 4;
  if (t == "long long" || t == "SInt64" || t == "UInt64" || t == "unsigned long long" ||
      t == "FileSize" || t == "double")
    return 8;
  return 0;
}
inline void PrepareFixedWidths(Type& type) {
  for (size_t i = type.nodes.size(); i-- > 0;) {
    auto& node = type.nodes[i];node.fixedWidth = 0;
    std::set<std::string> names;node.uniqueFields = true;
    for (auto child : node.children) {
      Require(child > i && child < type.nodes.size(), "native fixed schema cycle");
      if (!names.insert(type.nodes[child].name).second) node.uniqueFields = false;
    }
    if (node.align || node.type == "string" || node.type == "TypelessData" || node.type == "Array") continue;
    if (auto primitive = Primitive(node.type)) {node.fixedWidth = primitive;continue;}
    size_t width = 0;bool fixed = !node.children.empty();
    for (auto child : node.children) {
      Require(child > i && child < type.nodes.size(), "native fixed schema cycle");
      auto step = type.nodes[child].fixedWidth;
      if (!step || step > SIZE_MAX - width) {fixed = false;break;}width += step;
    }
    if (fixed) node.fixedWidth = width;
  }
}
inline void SkipValue(const Type& t, size_t index, Reader& r, size_t base, unsigned depth = 0) {
  Require(index < t.nodes.size() && depth < t.nodes.size(), "native schema cycle");
  const auto& node = t.nodes[index];
  if (node.fixedWidth) {r.Skip(node.fixedWidth);return;}
  auto primitive = Primitive(node.type);
  if (primitive)
    r.Skip(primitive);
  else if (node.type == "string" || node.type == "TypelessData") {
    size_t n = r.Count(r.limit - r.p);
    r.Skip(n);
  } else if (node.type == "Array") {
    Require(node.children.size() == 2 && t.nodes[node.children[0]].name == "size",
            "native array schema");
    size_t n = r.Count(r.limit - r.p), element = node.children[1];
    const auto& e = t.nodes[element];
    size_t step = e.fixedWidth ? e.fixedWidth : Primitive(e.type);
    if (step && !e.align) {
      Require(n <= (r.limit - r.p) / step, "native array size");
      r.Skip(n * step);
    } else
      for (size_t i = 0; i < n; ++i) SkipValue(t, element, r, base, depth + 1);
  } else {
    if (node.children.empty()) Require(false, "unsupported native leaf: " + node.type);
    for (size_t child : node.children) SkipValue(t, child, r, base, depth + 1);
  }
  if (node.align || node.type == "string") {
    size_t n = (size_t(0) - (r.p - base)) & 3;
    r.Skip(n);
  }
}
inline std::map<std::string, Field> Fields(const Type& t, size_t parent, const Bytes& b,
                                           size_t offset, size_t limit, size_t base) {
  Reader r(b, offset, false, limit);
  std::map<std::string, Field> fields;
  for (auto child : t.nodes.at(parent).children) {
    size_t start = r.p;
    SkipValue(t, child, r, base);
    Require(fields.emplace(t.nodes[child].name, Field{child, start, r.p - start}).second,
            "duplicate native field");
  }
  return fields;
}
inline std::string FieldString(const Bytes& b, const Field& field) {
  Reader r(b, field.offset, false, field.offset + field.size);
  return r.String();
}
struct Asset {
  std::string path, asset, cab;
  int64_t pathId = 0;
  int classId = 0;
  std::array<unsigned char, 16> type{};
};
// AssetBundle container entries may point at an object in another CAB in the
// same Endfield input.  Keep the lookup key independent of the display case
// used by the serialized external table; the Asset identity itself remains
// unchanged so existing source and runtime references keep their spelling.
using SerializedAssetKey = std::pair<std::string, int64_t>;
using SerializedNamedAssets = std::map<SerializedAssetKey, Asset>;
inline std::string SerializedAssetCabKey(std::string cab) {
  std::transform(cab.begin(), cab.end(), cab.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return cab;
}
inline std::string SerializedObjectName(const Bytes& b, const Type& type,
                                        const Object& object) {
  Reader prefix(b, object.offset, false, object.offset + object.size);
  for (auto child : type.nodes.at(0).children) {
    size_t start = prefix.p;
    SkipValue(type, child, prefix, object.offset);
    if (type.nodes[child].name == "m_Name")
      return FieldString(b, {child, start, prefix.p - start});
  }
  return {};
}
inline SerializedNamedAssets NamedSerializedAssets(const Bytes& b,
                                                   const std::string& cab,
                                                   const SerializedLayout& layout) {
  SerializedNamedAssets result;
  for (const auto& pair : layout.objects) {
    const auto& object = pair.second;
    const auto& type = layout.types.at(object.type);
    if (type.classId != 43 && type.classId != 21 && type.classId != 28 &&
        type.classId != 142)
      continue;
    auto name = SerializedObjectName(b, type, object);
    if (name.empty()) continue;
    Asset asset;
    asset.cab = cab;
    asset.pathId = object.id;
    asset.classId = type.classId;
    asset.type = type.hash;
    asset.asset = std::move(name);
    auto key = SerializedAssetKey{SerializedAssetCabKey(cab), object.id};
    auto found = result.find(key);
    Require(found == result.end() ||
                (found->second.classId == asset.classId &&
                 found->second.asset == asset.asset &&
                 found->second.type == asset.type),
            "duplicate named serialized asset identity");
    result.emplace(std::move(key), std::move(asset));
  }
  return result;
}
inline std::string LocalString(const Bytes& b, size_t base, size_t size, uint32_t offset);
inline const std::map<unsigned, std::string>& CommonStrings() {
  static const std::map<unsigned, std::string> values = {
      {0, "AABB"},
      {5, "AnimationClip"},
      {19, "AnimationCurve"},
      {34, "AnimationState"},
      {49, "Array"},
      {55, "Base"},
      {60, "BitField"},
      {69, "bitset"},
      {76, "bool"},
      {81, "char"},
      {86, "ColorRGBA"},
      {96, "Component"},
      {106, "data"},
      {111, "deque"},
      {117, "double"},
      {124, "dynamic_array"},
      {138, "FastPropertyName"},
      {155, "first"},
      {161, "float"},
      {167, "Font"},
      {172, "GameObject"},
      {183, "Generic Mono"},
      {196, "GradientNEW"},
      {208, "GUID"},
      {213, "GUIStyle"},
      {222, "int"},
      {226, "list"},
      {231, "long long"},
      {241, "map"},
      {245, "Matrix4x4f"},
      {256, "MdFour"},
      {263, "MonoBehaviour"},
      {277, "MonoScript"},
      {288, "m_ByteSize"},
      {299, "m_Curve"},
      {307, "m_EditorClassIdentifier"},
      {331, "m_EditorHideFlags"},
      {349, "m_Enabled"},
      {359, "m_ExtensionPtr"},
      {374, "m_GameObject"},
      {387, "m_Index"},
      {395, "m_IsArray"},
      {405, "m_IsStatic"},
      {416, "m_MetaFlag"},
      {427, "m_Name"},
      {434, "m_ObjectHideFlags"},
      {452, "m_PrefabInternal"},
      {469, "m_PrefabParentObject"},
      {490, "m_Script"},
      {499, "m_StaticEditorFlags"},
      {519, "m_Type"},
      {526, "m_Version"},
      {536, "Object"},
      {543, "pair"},
      {548, "PPtr<Component>"},
      {564, "PPtr<GameObject>"},
      {581, "PPtr<Material>"},
      {596, "PPtr<MonoBehaviour>"},
      {616, "PPtr<MonoScript>"},
      {633, "PPtr<Object>"},
      {646, "PPtr<Prefab>"},
      {659, "PPtr<Sprite>"},
      {672, "PPtr<TextAsset>"},
      {688, "PPtr<Texture>"},
      {702, "PPtr<Texture2D>"},
      {718, "PPtr<Transform>"},
      {734, "Prefab"},
      {741, "Quaternionf"},
      {753, "Rectf"},
      {759, "RectInt"},
      {767, "RectOffset"},
      {778, "second"},
      {785, "set"},
      {789, "short"},
      {795, "size"},
      {800, "SInt16"},
      {807, "SInt32"},
      {814, "SInt64"},
      {821, "SInt8"},
      {827, "staticvector"},
      {840, "string"},
      {847, "TextAsset"},
      {857, "TextMesh"},
      {866, "Texture"},
      {874, "Texture2D"},
      {884, "Transform"},
      {894, "TypelessData"},
      {907, "UInt16"},
      {914, "UInt32"},
      {921, "UInt64"},
      {928, "UInt8"},
      {934, "unsigned int"},
      {947, "unsigned long long"},
      {966, "unsigned short"},
      {981, "vector"},
      {988, "Vector2f"},
      {997, "Vector3f"},
      {1006, "Vector4f"},
      {1015, "m_ScriptingClassIdentifier"},
      {1042, "Gradient"},
      {1051, "Type*"},
      {1057, "int2_storage"},
      {1070, "int3_storage"},
      {1083, "BoundsInt"},
      {1093, "m_CorrespondingSourceObject"},
      {1121, "m_PrefabInstance"},
      {1138, "m_PrefabAsset"},
      {1152, "FileSize"},
      {1161, "Hash128"},
      {1188, "fixed_array"},
      {1200, "EntityId"},
  };
  return values;
}
inline std::string LocalString(const Bytes& b, size_t base, size_t size, uint32_t offset) {
  if (offset & 0x80000000) {
    auto found = CommonStrings().find(offset & 0x7fffffff);
    Require(found != CommonStrings().end(), "unknown common TypeTree string");
    return found->second;
  }
  Require(offset < size, "TypeTree string offset");
  Reader r(b, base + offset, false, base + size);
  return r.Z();
}
// Recognise the extended header from its fields. The serialized release label
// at offset 8 is metadata; it does not prove or disprove this layout.
inline bool SerializedHeader(const Bytes& b,size_t offset=0,size_t size=SIZE_MAX) {
  if(offset>b.size())return false;if(size==SIZE_MAX)size=b.size()-offset;
  if(size<48||size>b.size()-offset||b[offset+16])return false;
  Reader h(b,offset+20,true,offset+48);auto metadata=h.Get(4),total=h.Get(8),data=h.Get(8);
  return total==size&&data<=size&&data>=48&&metadata<=data-48;
}
// A metadata prefix is sufficient to read the layout. Payload bounds are
// checked against the independently observed native file extent, not against
// the prefix allocation. Existing callers still require a complete file.
inline SerializedLayout ParseSerialized(const Bytes& b, size_t nativeFileSize = SIZE_MAX) {
  if (nativeFileSize == SIZE_MAX) nativeFileSize = b.size();
  SerializedLayout layout;
  Reader h(b, 0, true);
  Require(b.size() >= 48, "SerializedFile header truncated");
  h.Skip(8);
  h.Skip(4); // Informational format label; validate actual extended fields below.
  h.Skip(4);
  Require(h.Get(1) == 0, "requires little-endian native data");
  h.Skip(3);
  size_t metadata = (size_t)h.Get(4);
  Require(h.Get(8) == nativeFileSize, "SerializedFile size mismatch");
  size_t dataOffset = (size_t)h.Get(8);
  Require(dataOffset <= nativeFileSize && metadata <= dataOffset &&
              metadata <= b.size() - 48 && 48 + metadata <= dataOffset,
          "SerializedFile metadata range");
  layout.metadataSize = metadata;
  layout.dataOffset = dataOffset;
  Reader r(b, 48, false, 48 + metadata);
  r.Z();
  r.Skip(4);
  Require(r.Get(1) == 1, "source TypeTree disabled");
  layout.typeCountOffset = r.p;
  size_t typeCount = r.Count();
  auto& types = layout.types;
  Require(typeCount <= (r.limit-r.p)/35,"native type table truncated");
  types.reserve(typeCount);
  for (size_t i = 0; i < typeCount; ++i) {
    Type t;
    auto recordStart = r.p;
    t.classId = (int32_t)r.Get(4);
    r.Skip(3);
    if (t.classId == 114) r.Skip(16);
    Require(r.limit - r.p >= 16, "native type hash truncated");
    std::copy_n(b.begin() + r.p, 16, t.hash.begin());
    r.Skip(16);
    size_t count = r.Count(), strings = r.Count(r.limit - r.p);
    Require(count <= (r.limit - r.p) / 32, "TypeTree range");
    size_t start = r.p, stringBase = start + count * 32;
    Require(strings <= r.limit - stringBase, "TypeTree string range");
    std::vector<size_t> stack;
    for (size_t j = 0; j < count; ++j) {
      Reader n(b, start + j * 32, false, start + (j + 1) * 32);
      n.Skip(2);
      size_t level = (size_t)n.Get(1);
      n.Skip(1);
      auto type = (uint32_t)n.Get(4), name = (uint32_t)n.Get(4);
      n.Skip(8);
      auto flags = n.Get(4);
      Require(level <= stack.size() && (j != 0 || level == 0) && (j == 0 || level != 0),
              "invalid TypeTree levels");
      stack.resize(level);
      t.nodes.push_back({LocalString(b, stringBase, strings, type),
                         LocalString(b, stringBase, strings, name),
                         (flags & 0x4000) != 0,
                         {}});
      if (level) t.nodes[stack.back()].children.push_back(j);
      stack.push_back(j);
    }
    r.Skip(count * 32 + strings);
    size_t deps = r.Count();t.dependencyOffset=r.p-recordStart;
    r.Skip(deps * 4);
    t.nativeRecord.assign(b.begin()+recordStart,b.begin()+r.p);
    PrepareFixedWidths(t);
    types.push_back(std::move(t));
  }
  layout.countOffset = r.p;
  size_t count = r.Count();
  if (count) r.Align();
  layout.tableStart = r.p;
  Require(count <= (r.limit-r.p)/24,"native object table truncated");
  auto& objects = layout.objects;
  for (size_t i = 0; i < count; ++i) {
    r.Align();
    Object o;
    o.id = (int64_t)r.Get(8);
    uint64_t relative = r.Get(8);
    o.size = (size_t)r.Get(4);
    o.type = (size_t)r.Get(4);
    Require(o.type < types.size() && relative <= nativeFileSize - dataOffset &&
                o.size <= nativeFileSize - dataOffset - relative,
            "invalid native object record");
    o.offset = dataOffset + (size_t)relative;
    Require(objects.emplace(o.id, o).second, "duplicate native object PathID");
    layout.order.push_back(o.id);
  }
  layout.tableEnd = r.p;
  size_t scripts = r.Count();
  layout.scriptCount = scripts;
  r.Skip(scripts * 12);
  size_t externalCount = r.Count();
  auto& externals = layout.externals;
  for (size_t i = 0; i < externalCount; ++i) {
    r.Z();
    r.Skip(20);
    auto path = r.Z();
    auto slash = path.find_last_of("/\\");
    externals.push_back(slash == path.npos ? path : path.substr(slash + 1));
  }
  std::vector<std::pair<size_t, size_t>> ranges;
  for (const auto& pair : objects)
    if (pair.second.size) ranges.emplace_back(pair.second.offset, pair.second.size);
  std::sort(ranges.begin(), ranges.end());
  size_t end = dataOffset;
  for (const auto& range : ranges) {
    Require(range.first >= end, "overlapping native object payloads");
    end = range.first + range.second;
  }
  return layout;
}
inline std::vector<Asset> SerializedAssets(const Bytes& b, const std::string& cab,
                                           const SerializedLayout& layout,
                                           const SerializedNamedAssets* named = nullptr) {
  const auto& types = layout.types;
  const auto& objects = layout.objects;
  const auto& externals = layout.externals;
  std::map<int64_t, std::string> names;
  for (const auto& pair : objects) {
    const auto& o = pair.second;
    const auto& t = types[o.type];
    if (t.classId != 43 && t.classId != 21 && t.classId != 28 && t.classId != 142) continue;
    Require(!t.nodes.empty(), "named asset has no TypeTree");
    // Identity indexing only needs the prefix through m_Name. Walking every
    // Mesh/Texture payload here couples resource lookup to unrelated codecs.
    Reader prefix(b,o.offset,false,o.offset+o.size);
    for(auto child:t.nodes.at(0).children){size_t start=prefix.p;SkipValue(t,child,prefix,o.offset);
      if(t.nodes[child].name=="m_Name"){names[o.id]=FieldString(b,{child,start,prefix.p-start});break;}}
  }
  std::vector<Asset> assets;
  for (const auto& pair : objects) {
    const auto& o = pair.second;
    const auto& t = types[o.type];
    if (t.classId != 142) continue;
    auto fields = Fields(t, 0, b, o.offset, o.offset + o.size, o.offset);
    auto container = fields.find("m_Container");
    Require(container != fields.end(), "AssetBundle has no m_Container");
    size_t node = container->second.node;
    const auto& map = t.nodes[node];
    Require(map.children.size() == 1, "AssetBundle container schema");
    const auto& array = t.nodes[map.children[0]];
    Require(array.type == "Array" && array.children.size() == 2, "AssetBundle container array");
    size_t element = array.children[1];
    Reader entries(b, container->second.offset, false,
                   container->second.offset + container->second.size);
    size_t n = entries.Count();
    for (size_t i = 0; i < n; ++i) {
      size_t start = entries.p;
      auto pairFields = Fields(t, element, b, start, entries.limit, o.offset);
      auto first = pairFields.at("first"), second = pairFields.at("second");
      auto info = Fields(t, second.node, b, second.offset, second.offset + second.size, o.offset);
      auto ptr = info.at("asset");
      auto pointer = Fields(t, ptr.node, b, ptr.offset, ptr.offset + ptr.size, o.offset);
      int file = (int32_t)Le(b, pointer.at("m_FileID").offset, 4);
      int64_t id = (int64_t)Le(b, pointer.at("m_PathID").offset, 8);
      Require(file >= 0 && (size_t)file <= externals.size(), "invalid AssetBundle file reference");
      Asset a;
      a.path = FieldString(b, first);
      std::transform(a.path.begin(),a.path.end(),a.path.begin(),[](unsigned char c){return char(std::tolower(c));});
      a.pathId = id;
      a.cab = file ? externals[file - 1] : cab;
      if (file == 0 && objects.count(id)) {
        const auto& target = objects.at(id);
        const auto& nativeType = types[target.type];
        a.classId = nativeType.classId;
        a.type = nativeType.hash;
        if (names.count(id)) a.asset = names.at(id);
      } else if (file != 0 && named) {
        auto found = named->find({SerializedAssetCabKey(a.cab), id});
        if (found != named->end()) {
          a.classId = found->second.classId;
          a.type = found->second.type;
          a.asset = found->second.asset;
        }
      }
      assets.push_back(std::move(a));
      SkipValue(t, element, entries, o.offset);
    }
  }
  return assets;
}
inline std::vector<Asset> Serialized(const Bytes& b, const std::string& cab) {
  return SerializedAssets(b, cab, ParseSerialized(b));
}
inline Bytes Lz4(const unsigned char* source, size_t size, size_t expected, bool inverted = false) {
  Bytes out;
  out.reserve(std::min(expected,size));
  size_t p = 0;
  auto length = [&](size_t base) {
    size_t n = base;
    if (base == 15) {
      unsigned v = 0;
      do {
        Require(p < size, "truncated LZ4 length");
        v = source[p++];
        Require(n <= expected && v <= expected - n, "LZ4 length overflow");
        n += v;
      } while (v == 255);
    }
    return n;
  };
  while (p < size) {
    unsigned token = source[p++];
    unsigned literalToken = inverted ? (token & 3) | ((token >> 2) & 12) : token >> 4;
    unsigned matchToken = inverted ? ((token >> 2) & 3) | ((token >> 4) & 12) : token & 15;
    size_t literals = length(literalToken);
    Require(literals <= size - p && literals <= expected - out.size(), "LZ4 literal range");
    out.insert(out.end(), source + p, source + p + literals);
    p += literals;
    if (p == size) break;
    Require(size - p >= 2, "LZ4 match offset truncated");
    size_t distance = inverted ? (source[p] << 8) | source[p + 1] : source[p] | (source[p + 1] << 8);
    p += 2;
    Require(distance && distance <= out.size(), "invalid LZ4 match offset");
    size_t match = length(matchToken);
    Require(expected - out.size() >= 4 && match <= expected - out.size() - 4, "LZ4 match range");
    match += 4;
    for (size_t j = 0; j < match; ++j) out.push_back(out[out.size() - distance]);
  }
  Require(out.size() == expected, "LZ4 output size mismatch");
  return out;
}
inline Bytes Decompress(const Bytes& b, size_t p, size_t size, size_t expected, unsigned flags) {
  Require(p <= b.size() && size <= b.size() - p,
          "bundle compressed block range");
  unsigned mode = flags & 63;
  if (mode == 0) {
    Require(size == expected, "uncompressed block size");
    return Bytes(b.begin() + p, b.begin() + p + size);
  }
  Require(mode == 2 || mode == 3, "unsupported bundle compression mode " + std::to_string(mode));
  return Lz4(b.data() + p, size, expected);
}
#include "eiem_endfield_input.h"
inline std::vector<Asset> Inspect(const Bytes& b, const std::string& sourceName = "<input>") {
  if (EndfieldInput::Recognized(b)) {
    auto input = EndfieldInput::Decode(b);
    std::vector<Asset> assets;
    for (const auto& member : input.members) {
      if (SerializedHeader(member.bytes)) {
        auto entries = Serialized(member.bytes, member.name);
        assets.insert(assets.end(), entries.begin(), entries.end());
      }
    }
    return assets;
  }
  if (SerializedHeader(b))
    return Serialized(b, sourceName);
  Require(b.size() >= 8 && memcmp(b.data(), "UnityFS\0", 8) == 0,
          "input is not a plaintext UnityFS/SerializedFile");
  Reader r(b, 8, true);
  r.Get(4); // Informational format label; determine padding from the byte layout.
  r.Z();
  r.Z();
  Require(r.Get(8) == b.size(), "UnityFS input incomplete");
  size_t compressed = (size_t)r.Get(4), uncompressed = (size_t)r.Get(4);
  unsigned flags = (unsigned)r.Get(4);
  Require(!(flags & 0x200) && !(flags & 0x1000), "unsupported UnityFS flags");
  const size_t headerEnd = r.p;
  auto inspectAt = [&](size_t directoryStart) {
  Require(compressed <= b.size() - directoryStart, "bundle directory extent");
  size_t infoOffset = flags & 0x80 ? b.size() - compressed : directoryStart;
  Bytes info = Decompress(b, infoOffset, compressed, uncompressed, flags);
  size_t dataOffset = flags & 0x80 ? directoryStart : directoryStart + compressed;
  Reader ir(info, 16, true);
  size_t blockCount = ir.Count();
  Require(blockCount <= (ir.limit-ir.p)/10, "bundle block table extent");
  struct Chunk {
    size_t decoded, encoded;
    unsigned flags;
  };
  std::vector<Chunk> chunks;
  size_t total = 0;
  for (size_t i = 0; i < blockCount; ++i) {
    Chunk c{(size_t)ir.Get(4), (size_t)ir.Get(4), (unsigned)ir.Get(2)};
    total = AddSize(total,c.decoded);
    chunks.push_back(c);
  }
  size_t fileCount = ir.Count();
  Require(fileCount <= (ir.limit-ir.p)/21, "bundle member table extent");
  struct Member {
    size_t offset, size;
    std::string name;
  };
  std::vector<Member> files;
  for (size_t i = 0; i < fileCount; ++i) {
    Member m;
    m.offset = (size_t)ir.Get(8);
    m.size = (size_t)ir.Get(8);
    ir.Skip(4);
    m.name = ir.Z();
    Require(m.offset <= total && m.size <= total - m.offset, "bundle member range");
    files.push_back(std::move(m));
  }
  Require(ir.p == info.size(), "bundle info suffix");
  const size_t dataEnd = flags & 0x80 ? infoOffset : b.size();
  Require(dataOffset <= dataEnd, "bundle data extent");
  size_t encodedTotal = 0;
  for (const auto& chunk : chunks) {
    encodedTotal = AddSize(encodedTotal, chunk.encoded);
    Require((chunk.flags&63) != 0 || chunk.decoded == chunk.encoded,
            "uncompressed bundle block extent");
  }
  Require(encodedTotal == dataEnd-dataOffset, "bundle encoded data extent");
  Bytes data;
  data.reserve(total);
  for (const auto& chunk : chunks) {
    if (flags & 0x80)
      Require(dataOffset <= infoOffset && chunk.encoded <= infoOffset - dataOffset,
              "bundle block overlaps directory");
    auto out = Decompress(b, dataOffset, chunk.encoded, chunk.decoded, chunk.flags);
    data.insert(data.end(), out.begin(), out.end());
    dataOffset += chunk.encoded;
  }
  Require(dataOffset == (flags & 0x80 ? infoOffset : b.size()), "bundle data suffix");
  std::vector<Asset> assets;
  for (const auto& file : files)
    if (SerializedHeader(data,file.offset,file.size)) {
      Bytes member(data.begin() + file.offset, data.begin() + file.offset + file.size);
      auto entries = Serialized(member, file.name);
      assets.insert(assets.end(), entries.begin(), entries.end());
    }
  return assets;
  };
  // Both packed and 16-byte-padded headers use the same fields. Validate the
  // directory, block/member ranges and payload instead of trusting the label.
  try { return inspectAt(headerEnd); }
  catch (const std::runtime_error& packedError) {
    const size_t padded = AlignSize(headerEnd,16);
    if (padded == headerEnd || padded > b.size()) throw;
    try { return inspectAt(padded); }
    catch (const std::runtime_error& paddedError) {
      throw std::runtime_error(std::string("UnityFS field layout invalid: ")+
          packedError.what()+"; "+paddedError.what());
    }
  }
}
}  // namespace EiemNativeInput
