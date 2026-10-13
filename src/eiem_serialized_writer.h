#pragma once
#include "eiem_native_input.h"

// Native serialized bytes only. The caller must separately validate reference
// closure and loader ownership before submitting a rebuilt input to the game.
namespace EiemNativeInput {
inline void WriteInteger(Bytes& bytes, size_t offset, uint64_t value, size_t size,
                         bool big = false) {
  Require(size <= 8 && offset <= bytes.size() && size <= bytes.size() - offset,
          "serialized integer write range");
  if(size<8){auto mask=(uint64_t(1)<<(size*8))-1;
    Require(value<=mask || (value|mask)==UINT64_MAX,"serialized integer field width overflow");}
  for (size_t i = 0; i < size; ++i)
    bytes[offset + i] = (unsigned char)(value >> (8 * (big ? size - i - 1 : i)));
}
inline void AppendInteger(Bytes& bytes, uint64_t value, size_t size) {
  size_t offset = bytes.size();
  bytes.resize(offset + size);
  WriteInteger(bytes, offset, value, size);
}
struct Value {
  size_t node = 0, count = 0;
  // Primitive/string/TypelessData bytes, or packed primitive array elements.
  Bytes raw, padding;
  std::vector<Value> children;
};
inline Value ReadValue(const Type& type, size_t index, Reader& reader,
                       size_t base, unsigned depth = 0) {
  Require(index < type.nodes.size() && depth < type.nodes.size(),
          "serialized value schema cycle");
  const auto& node = type.nodes[index];
  Value value;
  value.node = index;
  size_t primitive = Primitive(node.type), size = primitive;
  if (node.type == "string" || node.type == "TypelessData")
    size = reader.Count(reader.limit - reader.p);
  if (primitive || node.type == "string" || node.type == "TypelessData") {
    size_t start = reader.p;
    reader.Skip(size);
    value.raw.assign(reader.b.begin() + start, reader.b.begin() + reader.p);
    if (node.type == "string") Text(value.raw.data(), value.raw.size());
  } else if (node.type == "Array") {
    Require(node.children.size() == 2 &&
                type.nodes[node.children[0]].name == "size" &&
                Primitive(type.nodes[node.children[0]].type) == 4,
            "serialized array schema");
    value.count = reader.Count(reader.limit - reader.p);
    size_t element = node.children[1], step = Primitive(type.nodes[element].type);
    if (step && !type.nodes[element].align) {
      Require(value.count <= (reader.limit - reader.p) / step, "serialized packed array range");
      size_t start = reader.p;
      reader.Skip(value.count * step);
      value.raw.assign(reader.b.begin() + start, reader.b.begin() + reader.p);
    } else {
      // Append decoded elements; a corrupt count cannot preallocate a huge tree.
      for (size_t i = 0; i < value.count; ++i)
        value.children.push_back(ReadValue(type, element, reader, base, depth + 1));
    }
  } else {
    if (node.children.empty()) Require(false, "unsupported serialized leaf: " + node.type);
    std::set<std::string> names;
    for (size_t child : node.children) {
      if (!node.uniqueFields) Require(names.insert(type.nodes[child].name).second, "duplicate serialized struct field");
      value.children.push_back(ReadValue(type, child, reader, base, depth + 1));
    }
  }
  if (node.align || node.type == "string") {
    size_t start = reader.p;
    reader.Skip((size_t(0) - (reader.p - base)) & 3);
    value.padding.assign(reader.b.begin() + start, reader.b.begin() + reader.p);
  }
  return value;
}
inline Value DecodeValue(const Type& type, size_t node, const Bytes& bytes) {
  Reader reader(bytes);
  auto value = ReadValue(type, node, reader, 0);
  Require(reader.p == bytes.size(), "serialized TypeTree does not consume complete value");
  return value;
}
inline void WriteValue(const Type& type, const Value& value, Bytes& output,
                       unsigned depth = 0) {
  Require(value.node < type.nodes.size() && depth < type.nodes.size(),
          "serialized output schema recursion");
  const auto& node = type.nodes[value.node];
  auto primitive = Primitive(node.type);
  if (primitive) {
    Require(value.raw.size() == primitive && value.children.empty(), "serialized primitive width");
    output.insert(output.end(), value.raw.begin(), value.raw.end());
  } else if (node.type == "string" || node.type == "TypelessData") {
    Require(value.raw.size() <= INT_MAX && value.children.empty(), "serialized blob length");
    if (node.type == "string") Text(value.raw.data(), value.raw.size());
    AppendInteger(output, value.raw.size(), 4);
    output.insert(output.end(), value.raw.begin(), value.raw.end());
  } else if (node.type == "Array") {
    Require(node.children.size() == 2, "serialized output array schema");
    auto element = node.children[1];
    auto step = Primitive(type.nodes[element].type);
    bool packed = step && !type.nodes[element].align;
    Require(value.count <= INT_MAX && (packed ? value.children.empty() &&
                value.count <= SIZE_MAX / step && value.raw.size() == value.count * step
                : value.raw.empty() && value.count == value.children.size()),
            "serialized output array count");
    AppendInteger(output, value.count, 4);
    if (packed)
      output.insert(output.end(), value.raw.begin(), value.raw.end());
    else
      for (const auto& child : value.children) {
        Require(child.node == element, "serialized array element schema");
        WriteValue(type, child, output, depth + 1);
      }
  } else {
    Require(value.raw.empty() && value.children.size() == node.children.size() &&
                !node.children.empty(), "serialized output struct fields");
    for (size_t i = 0; i < node.children.size(); ++i) {
      Require(value.children[i].node == node.children[i], "serialized struct field order");
      WriteValue(type, value.children[i], output, depth + 1);
    }
  }
  if (node.align || node.type == "string") {
    size_t size = (size_t(0) - output.size()) & 3;
    if (size == value.padding.size())
      output.insert(output.end(), value.padding.begin(), value.padding.end());
    else
      output.resize(output.size() + size);
  }
}
inline Bytes EncodeValue(const Type& type, const Value& value) {
  Bytes bytes;
  WriteValue(type, value, bytes);
  return bytes;
}
inline Value& Child(const Type& type, Value& value, const std::string& name) {
  Require(type.nodes.at(value.node).type != "Array", "array accessed as a struct");
  for (auto& child : value.children)
    if (type.nodes.at(child.node).name == name) return child;
  throw std::runtime_error("absent serialized field: " + name);
}
inline const Value& Child(const Type& type, const Value& value, const std::string& name) {
  Require(type.nodes.at(value.node).type != "Array", "array accessed as a struct");
  for (const auto& child : value.children)
    if (type.nodes.at(child.node).name == name) return child;
  throw std::runtime_error("absent serialized field: " + name);
}
inline Value& ArrayValue(const Type& type, Value& value) {
  if (type.nodes.at(value.node).type == "Array") return value;
  Require(value.children.size() == 1 &&
              type.nodes.at(value.children[0].node).type == "Array", "not a serialized array");
  return value.children[0];
}
inline const Value& ArrayValue(const Type& type, const Value& value) {
  if (type.nodes.at(value.node).type == "Array") return value;
  Require(value.children.size() == 1 &&
              type.nodes.at(value.children[0].node).type == "Array", "not a serialized array");
  return value.children[0];
}
inline uint64_t Integer(const Value& value) {
  Require(!value.raw.empty() && value.raw.size() <= 8 && value.children.empty(),
          "not a serialized integer");
  return Le(value.raw, 0, value.raw.size());
}
inline void SetInteger(const Type& type, Value& value, uint64_t data) {
  auto size = Primitive(type.nodes.at(value.node).type);
  Require(size && type.nodes[value.node].type != "float" &&
              type.nodes[value.node].type != "double", "not a serialized integer schema");
  value.raw.resize(size);
  WriteInteger(value.raw, 0, data, size);
}
inline void SetString(const Type& type, Value& value, const std::string& text) {
  Require(type.nodes.at(value.node).type == "string", "not a serialized string schema");
  Text((const unsigned char*)text.data(), text.size());
  value.raw.assign(text.begin(), text.end());
}
using NativeReference = std::pair<std::string, int64_t>;
inline bool ExternalPrefix(const std::vector<std::string>& before,const std::vector<std::string>& after) {
  return after.size()>=before.size()&&std::equal(before.begin(),before.end(),after.begin());
}
inline void SetReference(const Type& type,Value& value,const NativeReference& reference,
                         SerializedLayout& layout,const std::string& cab) {
  size_t file=0;
  if(reference.first!=cab){
    auto found=std::find(layout.externals.begin(),layout.externals.end(),reference.first);
    if(found==layout.externals.end()){
      Require(!reference.first.empty()&&reference.first.find_first_of("/\\\r\n")==std::string::npos&&reference.first.find('\0')==std::string::npos,
              "invalid serialized external member name");
      Require(layout.externals.size()<INT_MAX,"serialized FileID field overflow");
      layout.externals.push_back(reference.first);file=layout.externals.size();
    }else file=size_t(found-layout.externals.begin())+1;
  }
  SetInteger(type,Child(type,value,"m_FileID"),file);
  SetInteger(type,Child(type,value,"m_PathID"),uint64_t(reference.second));
}
inline Value ObjectValue(const Bytes& source, const SerializedLayout& layout, int64_t id) {
  const auto& object = layout.objects.at(id);
  Bytes bytes(source.begin() + object.offset, source.begin() + object.offset + object.size);
  const auto& type = layout.types.at(object.type);
  auto value = DecodeValue(type, 0, bytes);
  Require(EncodeValue(type, value) == bytes, "source serialized value roundtrip changed bytes");
  return value;
}
// Read-only projection. Retained fields keep their original schema nodes;
// omitted fields are still range/schema checked but never allocated. This
// value must not be serialized as a replacement for the complete object.
inline Value ObjectFields(const Bytes& source,const SerializedLayout& layout,int64_t id,
                          const std::set<std::string>& wanted) {
  const auto& object=layout.objects.at(id);const auto& type=layout.types.at(object.type);
  Reader reader(source,object.offset,false,AddSize(object.offset,object.size));
  Value value;value.node=0;std::set<std::string> names;
  for(auto child:type.nodes.at(0).children){const auto& name=type.nodes.at(child).name;
    Require(type.nodes.at(0).uniqueFields||names.insert(name).second,"duplicate projected object field");
    if(wanted.count(name))value.children.push_back(ReadValue(type,child,reader,object.offset,1));
    else SkipValue(type,child,reader,object.offset,1);
  }
  if(type.nodes.at(0).align)reader.Skip((size_t(0)-(reader.p-object.offset))&3);
  Require(reader.p==object.offset+object.size,"projected object extent differs");return value;
}
struct AddedObject {
  int64_t id = 0;
  size_t type = 0;
  Bytes bytes;
};
// Generic TypeTree value construction belongs to the codec. Independent
// author assets must not include a Renderer/Prefab builder for these helpers.
inline Value EmptyValue(const Type& type, size_t index, unsigned depth = 0) {
  Require(index < type.nodes.size() && depth < type.nodes.size(), "new serialized value schema limit");
  Value value;value.node=index;const auto& node=type.nodes[index];auto width=Primitive(node.type);
  if(width)value.raw.resize(width);
  else if(node.type!="string"&&node.type!="TypelessData"&&node.type!="Array"){
    Require(!node.children.empty(),"unsupported new serialized leaf");
    for(auto child:node.children)value.children.push_back(EmptyValue(type,child,depth+1));
  }return value;
}
inline Value ArrayElement(const Type& type,const Value& array){
  Require(type.nodes.at(array.node).type=="Array"&&type.nodes[array.node].children.size()==2,"new serialized array element schema");
  return EmptyValue(type,type.nodes[array.node].children[1]);
}
inline const Value& ConstArray(const Type& type,const Value& value){return ArrayValue(type,value);}
inline Bytes BuildSerialized(const Bytes& source, const SerializedLayout& layout,
                              const std::map<int64_t, Bytes>& replacements,
                              const std::vector<AddedObject>& added = {},
                              const std::set<int64_t>& removed = {},
                              const std::vector<Bytes>& addedTypes = {},
                              const std::vector<Bytes>& addedScripts = {}) {
  Reader externalReader(source,layout.tableEnd,false,48+layout.metadataSize);
  auto scripts=externalReader.Count();externalReader.Skip(MultiplySize(scripts,12));
  auto externalCountOffset=externalReader.p;auto externalCount=externalReader.Count();
  std::vector<std::string> originalExternals;
  for(size_t i=0;i<externalCount;++i){externalReader.Z();externalReader.Skip(20);auto path=externalReader.Z();auto slash=path.find_last_of("/\\");originalExternals.push_back(slash==path.npos?path:path.substr(slash+1));}
  Require(ExternalPrefix(originalExternals,layout.externals),"serialized original external order changed");
  if (replacements.empty() && added.empty() && removed.empty() && addedTypes.empty() && addedScripts.empty() && originalExternals==layout.externals) return source;
  Require(layout.countOffset >= 48 && layout.tableEnd <= 48 + layout.metadataSize &&
              layout.tableStart >= layout.countOffset + 4, "serialized writer metadata range");
  for (const auto& change : replacements)
    Require(layout.objects.count(change.first)&&!removed.count(change.first), "serialized replacement PathID absent/deleted");
  for(auto id:removed)Require(layout.objects.count(id),"serialized deleted PathID absent");
  size_t count = AddSize(layout.order.size()-removed.size(),added.size());
  Require(count <= UINT32_MAX, "serialized object count field overflow");
  Bytes metadata(source.begin() + 48, source.begin() + layout.countOffset);
  size_t typeShift=0;for(const auto& record:addedTypes){typeShift=AddSize(typeShift,record.size());metadata.insert(metadata.end(),record.begin(),record.end());}
  if(!addedTypes.empty()){
    Require(layout.typeCountOffset>=48&&layout.typeCountOffset+4<=layout.countOffset&&layout.types.size()<=INT_MAX,"serialized appended type extent differs");
    WriteInteger(metadata,layout.typeCountOffset-48,layout.types.size(),4);
  }
  metadata.insert(metadata.end(),source.begin()+layout.countOffset,source.begin()+layout.countOffset+4);
  WriteInteger(metadata, layout.countOffset - 48 + typeShift, count, 4);
  if(count)metadata.resize((metadata.size() + 3) & ~size_t(3));
  size_t table = metadata.size();
  metadata.resize(AddSize(table,MultiplySize(count,24)));
  auto tail=metadata.size();
  auto scriptEnd=layout.tableEnd+4+MultiplySize(scripts,12);
  metadata.insert(metadata.end(),source.begin()+layout.tableEnd,source.begin()+scriptEnd);
  Require(AddSize(scripts,addedScripts.size())<=INT_MAX,"serialized script count overflow");
  WriteInteger(metadata,tail,scripts+addedScripts.size(),4);
  size_t scriptShift=0;for(const auto& record:addedScripts){Require(record.size()==12,"serialized script record width differs");metadata.insert(metadata.end(),record.begin(),record.end());scriptShift=AddSize(scriptShift,record.size());}
  metadata.insert(metadata.end(),source.begin()+scriptEnd,source.begin()+externalReader.p);
  Require(layout.externals.size()<=INT_MAX,"serialized external count field overflow");
  WriteInteger(metadata,tail+externalCountOffset-layout.tableEnd+scriptShift,layout.externals.size(),4);
  for(size_t i=externalCount;i<layout.externals.size();++i){
    // Preserve all existing GUID/type/path records. New bundle members use
    // Unity's archive reference grammar and the native zero GUID/type form.
    metadata.resize(AddSize(metadata.size(),21));
    auto path="archive:/"+layout.externals[i]+"/"+layout.externals[i];
    metadata.insert(metadata.end(),path.begin(),path.end());metadata.push_back(0);
  }
  metadata.insert(metadata.end(),source.begin()+externalReader.p,source.begin()+48+layout.metadataSize);
  size_t dataOffset = (48 + metadata.size() + 15) & ~size_t(15);
  Bytes output(source.begin(), source.begin() + 48);
  output.insert(output.end(), metadata.begin(), metadata.end());
  output.resize(dataOffset);
  std::map<int64_t, size_t> ids;
  auto append = [&](size_t index, int64_t id, size_t type, const Bytes& bytes,
                    size_t begin, size_t size) {
    Require(id && ids.emplace(id, index).second && type < layout.types.size() && size <= UINT_MAX,
            "serialized output identity/type/size");
    Require(begin <= bytes.size() && size <= bytes.size() - begin,
            "serialized output payload range");
    output.resize((output.size() + 7) & ~size_t(7));
    size_t offset = output.size() - dataOffset, row = 48 + table + index * 24;
    WriteInteger(output, row, (uint64_t)id, 8);
    WriteInteger(output, row + 8, offset, 8);
    WriteInteger(output, row + 16, size, 4);
    WriteInteger(output, row + 20, type, 4);
    AddSize(output.size(),size);
    output.insert(output.end(), bytes.begin() + begin, bytes.begin() + begin + size);
  };
  size_t index = 0;
  for (auto id : layout.order) {
    if(removed.count(id))continue;
    const auto& object = layout.objects.at(id);
    auto change = replacements.find(id);
    if (change == replacements.end())
      append(index++, id, object.type, source, object.offset, object.size);
    else
      append(index++, id, object.type, change->second, 0, change->second.size());
  }
  for (const auto& object : added)
    append(index++, object.id, object.type, object.bytes, 0, object.bytes.size());
  // Native loading requires signed PathID order. Keep every payload offset intact.
  Bytes records(output.begin() + 48 + table, output.begin() + 48 + table + count * 24);
  size_t sortedIndex = 0;
  for (const auto& row : ids)
    std::copy_n(records.begin() + row.second * 24, 24,
                output.begin() + 48 + table + sortedIndex++ * 24);
  WriteInteger(output, 20, metadata.size(), 4, true);
  WriteInteger(output, 24, output.size(), 8, true);
  WriteInteger(output, 32, dataOffset, 8, true);
  // Original external records, ref types and user info remain byte-for-byte;
  // only append registered dependencies after the existing external prefix.
  auto checked = ParseSerialized(output);
  Require(checked.order.size() == count && checked.externals == layout.externals &&
              std::is_sorted(checked.order.begin(), checked.order.end()),
          "serialized output validation failed");
  return output;
}
}  // namespace EiemNativeInput
