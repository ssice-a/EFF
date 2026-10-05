#pragma once
#include "eiem_native_input.h"

// Native serialized bytes only. The caller must separately validate reference
// closure and loader ownership before submitting a rebuilt input to the game.
namespace EiemNativeInput {
inline void WriteInteger(Bytes& bytes, size_t offset, uint64_t value, size_t size,
                         bool big = false) {
  Require(size <= 8 && offset <= bytes.size() && size <= bytes.size() - offset,
          "serialized integer write range");
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
                       size_t base, size_t& budget, unsigned depth = 0) {
  Require(index < type.nodes.size() && depth < 128 && budget,
          "serialized value recursion/allocation limit");
  --budget;
  const auto& node = type.nodes[index];
  Value value;
  value.node = index;
  size_t size = Primitive(node.type);
  if (node.type == "string" || node.type == "TypelessData")
    size = reader.Count(reader.limit - reader.p);
  if (Primitive(node.type) || node.type == "string" || node.type == "TypelessData") {
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
      Require(value.count <= budget, "serialized structured array allocation limit");
      value.children.reserve(value.count);
      for (size_t i = 0; i < value.count; ++i)
        value.children.push_back(ReadValue(type, element, reader, base, budget, depth + 1));
    }
  } else {
    Require(!node.children.empty(), "unsupported serialized leaf: " + node.type);
    std::set<std::string> names;
    for (size_t child : node.children) {
      Require(names.insert(type.nodes[child].name).second, "duplicate serialized struct field");
      value.children.push_back(ReadValue(type, child, reader, base, budget, depth + 1));
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
  size_t budget = 4000000;
  auto value = ReadValue(type, node, reader, 0, budget);
  Require(reader.p == bytes.size(), "serialized TypeTree does not consume complete value");
  return value;
}
inline void WriteValue(const Type& type, const Value& value, Bytes& output,
                       unsigned depth = 0) {
  Require(value.node < type.nodes.size() && depth < 128,
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
  Require(output.size() <= 64ull * 1024 * 1024, "serialized output value limit");
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
inline Value ObjectValue(const Bytes& source, const SerializedLayout& layout, int64_t id) {
  const auto& object = layout.objects.at(id);
  Bytes bytes(source.begin() + object.offset, source.begin() + object.offset + object.size);
  const auto& type = layout.types.at(object.type);
  auto value = DecodeValue(type, 0, bytes);
  Require(EncodeValue(type, value) == bytes, "source serialized value roundtrip changed bytes");
  return value;
}
struct AddedObject {
  int64_t id = 0;
  size_t type = 0;
  Bytes bytes;
};
inline Bytes BuildSerialized(const Bytes& source, const SerializedLayout& layout,
                              const std::map<int64_t, Bytes>& replacements,
                              const std::vector<AddedObject>& added = {}) {
  if (replacements.empty() && added.empty()) return source;
  Require(layout.countOffset >= 48 && layout.tableEnd <= 48 + layout.metadataSize &&
              layout.tableStart >= layout.countOffset + 4, "serialized writer metadata range");
  for (const auto& change : replacements)
    Require(layout.objects.count(change.first), "serialized replacement PathID absent");
  size_t count = layout.order.size() + added.size();
  Require(count <= 1000000, "serialized output object count");
  Bytes metadata(source.begin() + 48, source.begin() + layout.tableStart);
  WriteInteger(metadata, layout.countOffset - 48, count, 4);
  metadata.resize((metadata.size() + 3) & ~size_t(3));
  size_t table = metadata.size();
  metadata.resize(table + count * 24);
  metadata.insert(metadata.end(), source.begin() + layout.tableEnd,
                  source.begin() + 48 + layout.metadataSize);
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
    Require(output.size() <= 64ull * 1024 * 1024 &&
                size <= 64ull * 1024 * 1024 - output.size(), "serialized output file limit");
    output.insert(output.end(), bytes.begin() + begin, bytes.begin() + begin + size);
  };
  size_t index = 0;
  for (auto id : layout.order) {
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
  // Legacy fields, type table and trailing metadata (externals/ref types/user
  // info) remain the current input's bytes; payload order is retained.
  auto checked = ParseSerialized(output);
  Require(checked.order.size() == count && checked.externals == layout.externals &&
              std::is_sorted(checked.order.begin(), checked.order.end()),
          "serialized output validation failed");
  return output;
}
}  // namespace EiemNativeInput
