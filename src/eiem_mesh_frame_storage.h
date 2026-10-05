#pragma once
#include "eiem_serialized_writer.h"
#include <cmath>

namespace EiemNativeInput {
struct MeshVertexLayout {
  size_t count = 0, stride[4]{}, start[4]{}, bytes = 0;
};
inline MeshVertexLayout DescribeMeshVertexStorage(const Type& type, const Value& vertex) {
  MeshVertexLayout result;
  result.count = Integer(Child(type, vertex, "m_VertexCount"));
  auto channelValue = Child(type, vertex, "m_Channels");
  const auto& channels = ArrayValue(type, channelValue).children;
  Require(result.count && result.count <= 4000000 && channels.size() == 14,
          "Mesh vertex storage count/channel limit");
  static const size_t widths[]{4, 2, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4};
  bool occupied[4][256]{};
  for (const auto& channel : channels) {
    size_t dim = Integer(Child(type, channel, "dimension")) & 15;
    if (!dim) continue;
    size_t stream = Integer(Child(type, channel, "stream"));
    size_t offset = Integer(Child(type, channel, "offset"));
    size_t format = Integer(Child(type, channel, "format"));
    Require(dim <= 4 && stream < 4 && format < 12 && offset + dim * widths[format] <= 256,
            "Mesh vertex storage channel extent");
    size_t end = offset + dim * widths[format];
    for (size_t b = offset; b < end; ++b) {
      Require(!occupied[stream][b], "Mesh vertex storage overlapping channels");
      occupied[stream][b] = true;
    }
    result.stride[stream] = std::max(result.stride[stream], end);
  }
  for (size_t stream = 0; stream < 4; ++stream) {
    if (stream) result.start[stream] = result.bytes;
    result.bytes = (result.start[stream] + result.count * result.stride[stream] + 15) & ~size_t(15);
  }
  Require(Child(type, vertex, "m_DataSize").raw.size() == result.bytes,
          "Mesh vertex storage byte extent");
  return result;
}
inline std::array<float, 7> DecodePackedMeshFrame(uint32_t word) {
  Require(word & 0x40000000, "Mesh packed frame marker missing");
  auto signed10 = [&](unsigned shift) {
    int value = int((word >> shift) & 1023);
    return double(value >= 512 ? value - 1024 : value) / 511.;
  };
  using Vec = std::array<double, 3>;
  auto dot = [](const Vec& a, const Vec& b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; };
  auto unit = [&](Vec v) {
    double length = std::sqrt(dot(v, v));
    Require(std::isfinite(length) && length > 1e-10, "degenerate packed Mesh frame");
    for (auto& component : v) component /= length;
    return v;
  };
  double x = signed10(0), y = signed10(10), angle = signed10(20);
  double z = 1 - std::abs(x) - std::abs(y);
  if (z < 0) {
    double oldX = x;
    x = (1 - std::abs(y)) * (x >= 0 ? 1 : -1);
    y = (1 - std::abs(oldX)) * (y >= 0 ? 1 : -1);
  }
  Vec normal = unit({x, y, z});
  Vec basis{normal[1]-normal[2], normal[2]-normal[0], normal[0]-normal[1]};
  double projection = dot(basis, normal);
  for (size_t i = 0; i < 3; ++i) basis[i] -= projection * normal[i];
  basis = unit(basis);
  Vec bitangent = unit({normal[1]*basis[2]-normal[2]*basis[1],
                       normal[2]*basis[0]-normal[0]*basis[2],
                       normal[0]*basis[1]-normal[1]*basis[0]});
  double dx = 1 - 2 * std::abs(angle), dy = (1 - std::abs(dx)) * (angle < 0 ? -1 : 1);
  Vec tangent{};
  for (size_t i = 0; i < 3; ++i) tangent[i] = dx*basis[i] + dy*bitangent[i];
  tangent = unit(tangent);
  return {float(normal[0]), float(normal[1]), float(normal[2]),
          float(tangent[0]), float(tangent[1]), float(tangent[2]), word >> 31 ? 1.f : -1.f};
}
inline bool ExpandShapedMeshFrames(const Type& type, Value& mesh) {
  auto shapes = Child(type, mesh, "m_Shapes");
  if (!ArrayValue(type, Child(type, shapes, "channels")).count) return false;
  auto& vertex = Child(type, mesh, "m_VertexData");
  auto& channels = ArrayValue(type, Child(type, vertex, "m_Channels")).children;
  const auto before = DescribeMeshVertexStorage(type, vertex);
  auto descriptor = [&](size_t index, size_t offset, size_t dimension) {
    const auto& c = channels.at(index);
    return Integer(Child(type,c,"stream")) == 0 && Integer(Child(type,c,"offset")) == offset &&
           Integer(Child(type,c,"format")) == 0 && Integer(Child(type,c,"dimension")) == dimension;
  };
  Require(descriptor(0, 0, 3), "shaped Mesh Float32 position contract");
  if (descriptor(1, 12, 3) && descriptor(2, 24, 4) && before.stride[0] == 40) return false;
  Require(descriptor(1, 12, 0x31) && !(Integer(Child(type, channels[2], "dimension")) & 15) &&
              before.stride[0] == 16, "unsupported shaped Mesh frame storage");
  const Bytes old = Child(type, vertex, "m_DataSize").raw;
  // Packed frame words are shader bit patterns. The native CPU shape/skin
  // route needs real normal3/tangent4 float channels before deserialization.
  MeshVertexLayout after = before;
  after.stride[0] = 40;
  for (size_t s = 0; s < 4; ++s) {
    if (s) after.start[s] = after.bytes;
    else after.start[s] = after.bytes = 0;
    after.bytes = (after.start[s] + after.count * after.stride[s] + 15) & ~size_t(15);
  }
  Bytes data(after.bytes);
  for (size_t v = 0; v < before.count; ++v) {
    const auto frame = DecodePackedMeshFrame((uint32_t)Le(old, v*16+12, 4));
    std::copy_n(old.begin()+v*16, 12, data.begin()+v*40);
    std::memcpy(data.data()+v*40+12, frame.data(), 28);
  }
  for (size_t s = 1; s < 4; ++s)
    std::copy_n(old.begin()+before.start[s], before.count*before.stride[s], data.begin()+after.start[s]);
  SetInteger(type, Child(type, channels[1], "dimension"), 3);
  SetInteger(type, Child(type, channels[2], "stream"), 0);
  SetInteger(type, Child(type, channels[2], "offset"), 24);
  SetInteger(type, Child(type, channels[2], "format"), 0);
  SetInteger(type, Child(type, channels[2], "dimension"), 4);
  Child(type, vertex, "m_DataSize").raw = std::move(data);
  Require(DescribeMeshVertexStorage(type, vertex).stride[0] == 40, "shaped Mesh expansion stride");
  return true;
}
}  // namespace EiemNativeInput
