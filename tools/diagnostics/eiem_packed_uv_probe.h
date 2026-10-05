#pragma once
#include <cmath>
#include "../../src/eiem_serialized_writer.h"

// Temporary offline representation experiment. This is deliberately separate
// from the production writer until the original visual failure is retested.
namespace EiemNativeInput::PackedUvProbe {
struct Layout {
  size_t count = 0, stride[3]{}, start[3]{}, size = 0;
};
inline size_t Width(size_t format) {
  Require(format < 12, "UV probe unsupported vertex format");
  static const size_t widths[]{4, 2, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4};
  return widths[format];
}
inline Layout Describe(const Type &type, const Value &vertex) {
  Layout result;
  result.count = Integer(Child(type, vertex, "m_VertexCount"));
  auto channelsValue = Child(type, vertex, "m_Channels");
  const auto &channels = ArrayValue(type, channelsValue).children;
  Require(result.count && result.count <= 1000000 && channels.size() == 14,
          "UV probe vertex/channel count limit");
  bool occupied[3][256]{};
  for (const auto &channel : channels) {
    size_t dim = Integer(Child(type, channel, "dimension")) & 15;
    if (!dim) continue;
    size_t stream = Integer(Child(type, channel, "stream"));
    size_t offset = Integer(Child(type, channel, "offset"));
    size_t size = dim * Width(Integer(Child(type, channel, "format")));
    Require(dim <= 4 && stream < 3 && offset + size <= 256,
            "UV probe channel extent");
    for (size_t b = offset; b < offset + size; ++b) {
      Require(!occupied[stream][b], "UV probe overlapping channels");
      occupied[stream][b] = true;
    }
    result.stride[stream] = std::max(result.stride[stream], offset + size);
  }
  for (size_t s = 0; s < 3; ++s) {
    Require(result.stride[s] && !(result.stride[s] & 3), "UV probe stream stride");
    if (s) result.start[s] = (result.start[s - 1] + result.count * result.stride[s - 1] + 15) & ~size_t(15);
  }
  result.size = (result.start[2] + result.count * result.stride[2] + 15) & ~size_t(15);
  Require(Child(type, vertex, "m_DataSize").raw.size() == result.size,
          "UV probe stream data extent");
  return result;
}
inline bool Repack(const Type &type, Value &mesh) {
  auto &vertex = Child(type, mesh, "m_VertexData");
  auto &channels = ArrayValue(type, Child(type, vertex, "m_Channels")).children;
  Require(channels.size() == 14, "UV probe channel count");
  auto &uv = channels[6];
  if (Integer(Child(type, uv, "dimension")) != 0x24) return false;
  Require(Integer(Child(type, uv, "stream")) == 1 &&
              Integer(Child(type, uv, "format")) == 0, "UV probe requires flagged Float32 UV2");
  const auto before = Describe(type, vertex);
  size_t offset = Integer(Child(type, uv, "offset"));
  Require(offset + 16 <= before.stride[1], "UV probe Float32 UV2 extent");
  const Bytes source = Child(type, vertex, "m_DataSize").raw;
  SetInteger(type, Child(type, uv, "format"), 3); // native SNorm8, not UNorm8
  for (size_t i = 0; i < channels.size(); ++i) {
    auto &channel = channels[i];
    if (i == 6 || !(Integer(Child(type, channel, "dimension")) & 15) ||
        Integer(Child(type, channel, "stream")) != 1) continue;
    auto position = Integer(Child(type, channel, "offset"));
    if (position >= offset + 16) SetInteger(type, Child(type, channel, "offset"), position - 12);
  }
  // Describe validates the exact byte extent. Resize a placeholder first;
  // this does not touch any geometry outside this offline Value copy.
  size_t newStride = before.stride[1] - 12;
  size_t skinStart = (before.start[1] + before.count * newStride + 15) & ~size_t(15);
  Bytes result((skinStart + before.count * before.stride[2] + 15) & ~size_t(15));
  std::copy_n(source.begin(), before.start[1], result.begin());
  for (size_t v = 0; v < before.count; ++v) {
    size_t from = before.start[1] + v * before.stride[1];
    size_t to = before.start[1] + v * newStride;
    std::copy_n(source.begin() + from, offset, result.begin() + to);
    for (size_t c = 0; c < 4; ++c) {
      float value = 0;
      std::memcpy(&value, source.data() + from + offset + c * 4, sizeof(value));
      Require(std::isfinite(value) && value >= -1 && value <= 1, "UV probe SNorm8 range");
      int quantized = int(std::round(double(value) * 127));
      Require(std::abs(double(value) - double(quantized) / 127) <= 1.0 / 254 + 1e-7,
              "UV probe SNorm8 quantization exceeds half step");
      result[to + offset + c] = static_cast<unsigned char>(quantized);
    }
    std::copy_n(source.begin() + from + offset + 16, before.stride[1] - offset - 16,
                result.begin() + to + offset + 4);
  }
  std::copy(source.begin() + before.start[2], source.end(), result.begin() + skinStart);
  Child(type, vertex, "m_DataSize").raw = std::move(result);
  const auto after = Describe(type, vertex);
  Require(after.stride[0] == before.stride[0] && after.stride[1] == newStride &&
              after.stride[2] == before.stride[2] && after.start[2] == skinStart,
          "UV probe repacked layout mismatch");
  return true;
}
} // namespace EiemNativeInput::PackedUvProbe
