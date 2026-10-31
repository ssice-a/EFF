#pragma once
#include "eiem_resource_directory.h"

// One independently addressable author resource. The descriptor is local to
// this file; it is not a package-wide catalog or a runtime cache index.
namespace EiemAuthorResourceFile {
using namespace EiemDirectory;
inline constexpr size_t headerBytes = 72;
inline constexpr size_t maximumMetadataBytes = 16 * 1024 * 1024;
inline constexpr unsigned version = 3;
inline constexpr unsigned char magic[16] = {
    'E', 'F', 'F', '_', 'A', 'U', 'T', 'H', 'O', 'R', '_', 'F', 'I', 'L', 'E', '3'};

struct Layout {
  unsigned kind = 0;
  size_t metadataOffset = headerBytes;
  size_t metadataBytes = 0;
  size_t payloadOffset = 0;
  size_t payloadBytes = 0;
};

inline void Put(unsigned char* output, uint64_t value, size_t bytes) {
  for (size_t i = 0; i < bytes; ++i)
    output[i] = static_cast<unsigned char>(value >> (i * 8));
}

inline uint64_t Get(const unsigned char* input, size_t bytes) {
  uint64_t value = 0;
  for (size_t i = 0; i < bytes; ++i)
    value |= uint64_t(input[i]) << (i * 8);
  return value;
}

inline Layout Parse(size_t fileBytes,
                    const std::function<void(size_t, unsigned char*, size_t)>& read) {
  Require(bool(read) && fileBytes >= headerBytes, "author resource header truncated");
  Bytes header(headerBytes);
  read(0, header.data(), header.size());
  Require(std::equal(std::begin(magic), std::end(magic), header.begin()),
          "author resource signature differs");
  Require(Get(header.data() + 16, 4) == version, "unsupported author resource version");

  Layout result;
  result.kind = static_cast<unsigned>(Get(header.data() + 20, 4));
  auto metadataBytes = Get(header.data() + 24, 8);
  auto payloadBytes = Get(header.data() + 32, 8);
  Require(result.kind >= 1 && result.kind <= 7, "unknown author resource kind");
  Require(metadataBytes && metadataBytes <= maximumMetadataBytes && metadataBytes <= SIZE_MAX,
          "author resource metadata extent invalid");
  Require(payloadBytes <= SIZE_MAX, "author resource payload extent overflow");

  result.metadataBytes = static_cast<size_t>(metadataBytes);
  result.payloadBytes = static_cast<size_t>(payloadBytes);
  result.payloadOffset = AddSize(result.metadataOffset, result.metadataBytes);
  Require(AddSize(result.payloadOffset, result.payloadBytes) == fileBytes,
          "author resource file extent differs");

  Bytes metadata(result.metadataBytes);
  read(result.metadataOffset, metadata.data(), metadata.size());
  const auto digest = Sha(metadata.data(), metadata.size());
  Require(std::equal(digest.begin(), digest.end(), header.begin() + 40),
          "author resource metadata checksum differs");
  return result;
}

inline Bytes ReadMetadata(const Layout& layout,
                          const std::function<void(size_t, unsigned char*, size_t)>& read) {
  Bytes result(layout.metadataBytes);
  Require(layout.metadataBytes && layout.metadataBytes <= maximumMetadataBytes,
          "author resource metadata extent invalid");
  read(layout.metadataOffset, result.data(), result.size());
  return result;
}

inline Bytes Encode(unsigned kind, const Bytes& metadata, const Bytes& payload) {
  Require(kind >= 1 && kind <= 7, "unknown author resource kind");
  Require(!metadata.empty() && metadata.size() <= maximumMetadataBytes,
          "author resource metadata extent invalid");
  Bytes result(headerBytes, 0);
  std::copy(std::begin(magic), std::end(magic), result.begin());
  Put(result.data() + 16, version, 4);
  Put(result.data() + 20, kind, 4);
  Put(result.data() + 24, metadata.size(), 8);
  Put(result.data() + 32, payload.size(), 8);
  const auto digest = Sha(metadata.data(), metadata.size());
  std::copy(digest.begin(), digest.end(), result.begin() + 40);
  result.insert(result.end(), metadata.begin(), metadata.end());
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}
} // namespace EiemAuthorResourceFile
