#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace EiemAuthorBoneAnimation {
struct Node {
  std::string path;
  int32_t parent = -1;
  std::array<float, 3> position{};
  std::array<float, 4> rotation{0, 0, 0, 1};
  std::array<float, 3> scale{1, 1, 1};
};

struct Skeleton {
  std::vector<Node> nodes;
  std::set<std::string> paths;
};

using TransformSample = std::array<float, 10>;

struct Track {
  std::string path;
  std::vector<TransformSample> samples;
};

struct Clip {
  std::string id;
  std::string skeleton;
  float duration = 0;
  uint32_t sampleRate = 0;
  bool loop = true;
  std::vector<Track> tracks;
};

class Reader {
 public:
  Reader(const unsigned char* bytes, size_t size) : bytes_(bytes), size_(size) {
    if (!bytes_ && size_) throw std::runtime_error("author animation input is null");
  }

  void Magic(const char* expected, size_t size) {
    Need(size);
    if (std::memcmp(bytes_ + position_, expected, size) != 0)
      throw std::runtime_error("author animation signature differs");
    position_ += size;
  }

  uint8_t U8() {
    Need(1);
    return bytes_[position_++];
  }

  uint32_t U32() {
    Need(4);
    uint32_t value = 0;
    for (unsigned i = 0; i != 4; ++i)
      value |= uint32_t(bytes_[position_++]) << (i * 8);
    return value;
  }

  int32_t I32() { return static_cast<int32_t>(U32()); }

  float F32() {
    uint32_t bits = U32();
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value)) throw std::runtime_error("author animation contains non-finite TRS");
    return value;
  }

  std::string String16() {
    Need(2);
    size_t count = size_t(bytes_[position_]) | (size_t(bytes_[position_ + 1]) << 8);
    position_ += 2;
    Need(count);
    if (!count || std::memchr(bytes_ + position_, 0, count))
      throw std::runtime_error("author animation string is empty or contains NUL");
    std::string value(reinterpret_cast<const char*>(bytes_ + position_), count);
    position_ += count;
    return value;
  }

  size_t Remaining() const { return size_ - position_; }
  bool Done() const { return position_ == size_; }

 private:
  void Need(size_t count) const {
    if (position_ > size_ || count > size_ - position_)
      throw std::runtime_error("author animation resource is truncated");
  }

  const unsigned char* bytes_ = nullptr;
  size_t size_ = 0;
  size_t position_ = 0;
};

inline bool ValidPath(const std::string& path) {
  if (path.empty() || path.front() == '/' || path.back() == '/' ||
      path.find('\\') != std::string::npos || path.find("//") != std::string::npos)
    return false;
  size_t begin = 0;
  while (begin < path.size()) {
    auto end = path.find('/', begin);
    if (end == std::string::npos) end = path.size();
    auto part = path.substr(begin, end - begin);
    if (part.empty() || part == "." || part == "..") return false;
    begin = end + 1;
  }
  return true;
}

inline void ValidateSample(const TransformSample& sample) {
  double length = 0;
  for (size_t i = 0; i < sample.size(); ++i) {
    if (!std::isfinite(sample[i])) throw std::runtime_error("author animation sample is non-finite");
    if (i >= 3 && i < 7) length += double(sample[i]) * sample[i];
    if (i >= 7 && sample[i] <= 0) throw std::runtime_error("author animation scale is not positive");
  }
  if (std::abs(length - 1.0) > .001)
    throw std::runtime_error("author animation rotation is not normalized");
}

inline Skeleton DecodeSkeleton(const unsigned char* bytes, size_t size) {
  Reader reader(bytes, size);
  reader.Magic("EFFSKA1\0", 8);
  if (reader.U32() != 1) throw std::runtime_error("unsupported author Skeleton version");
  const auto count = reader.U32();
  if (!count || count > 16384) throw std::runtime_error("invalid author Skeleton node count");
  if (reader.String16() != "unity-y-up-left-handed")
    throw std::runtime_error("unsupported author Skeleton coordinate space");

  Skeleton result;
  result.nodes.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    Node node;
    node.path = reader.String16();
    node.parent = reader.I32();
    for (auto& value : node.position) value = reader.F32();
    for (auto& value : node.rotation) value = reader.F32();
    for (auto& value : node.scale) value = reader.F32();

    if (!ValidPath(node.path) || !result.paths.insert(node.path).second)
      throw std::runtime_error("invalid or duplicate author Skeleton path: " + node.path);
    if (i == 0) {
      if (node.parent != -1 || node.path.find('/') != std::string::npos)
        throw std::runtime_error("author Skeleton root must be a path root");
    } else {
      if (node.parent < 0 || uint32_t(node.parent) >= i)
        throw std::runtime_error("author Skeleton parent must precede child");
      const auto slash = node.path.find_last_of('/');
      const auto parentPath = slash == std::string::npos ? std::string() : node.path.substr(0, slash);
      if (parentPath != result.nodes[size_t(node.parent)].path)
        throw std::runtime_error("author Skeleton path and parent differ: " + node.path);
    }
    double rotationLength = 0;
    for (float value : node.rotation) rotationLength += double(value) * value;
    if (std::abs(rotationLength - 1.0) > .001)
      throw std::runtime_error("author Skeleton rotation is not normalized: " + node.path);
    for (float value : node.scale)
      if (value <= 0) throw std::runtime_error("author Skeleton scale is not positive: " + node.path);
    result.nodes.push_back(std::move(node));
  }
  if (!reader.Done()) throw std::runtime_error("author Skeleton has trailing bytes");
  return result;
}

inline Clip DecodeClip(const unsigned char* bytes, size_t size,
                       const Skeleton& skeleton, const std::string& expectedSkeleton) {
  Reader reader(bytes, size);
  reader.Magic("EFFBAN1\0", 8);
  if (reader.U32() != 1) throw std::runtime_error("unsupported BoneAnimation version");
  Clip result;
  result.id = reader.String16();
  result.skeleton = reader.String16();
  if (result.id.empty() || result.skeleton != expectedSkeleton)
    throw std::runtime_error("BoneAnimation identity or Skeleton reference differs");
  result.duration = reader.F32();
  result.sampleRate = reader.U32();
  const auto loop = reader.U8();
  const auto count = reader.U32();
  if (result.duration <= 0 || !result.sampleRate || loop > 1 || !count || count > 16384)
    throw std::runtime_error("invalid BoneAnimation timing or track count");
  result.loop = loop != 0;
  result.tracks.reserve(count);
  std::set<std::string> trackPaths;
  for (uint32_t i = 0; i < count; ++i) {
    Track track;
    track.path = reader.String16();
    const auto samples = reader.U32();
    if (!ValidPath(track.path) || !skeleton.paths.count(track.path) ||
        !trackPaths.insert(track.path).second)
      throw std::runtime_error("BoneAnimation track references an invalid or duplicate bone: " + track.path);
    if (samples < 2 || samples > 1000000 || size_t(samples) > reader.Remaining() / 40)
      throw std::runtime_error("invalid BoneAnimation sample extent: " + track.path);
    track.samples.resize(samples);
    for (auto& sample : track.samples) {
      for (auto& value : sample) value = reader.F32();
      ValidateSample(sample);
    }
    result.tracks.push_back(std::move(track));
  }
  if (!reader.Done()) throw std::runtime_error("BoneAnimation has trailing bytes");
  return result;
}

inline TransformSample Sample(const std::vector<TransformSample>& samples, double phase) {
  if (samples.size() < 2 || !std::isfinite(phase))
    throw std::runtime_error("BoneAnimation sample input differs");
  const double position = std::clamp(phase, 0.0, 1.0) * double(samples.size() - 1);
  const size_t left = std::min(size_t(position), samples.size() - 2);
  const double amount = position - double(left);
  const auto& a = samples[left];
  const auto& b = samples[left + 1];
  TransformSample output{};
  for (size_t i = 0; i < 3; ++i)
    output[i] = float(double(a[i]) + (double(b[i]) - a[i]) * amount);
  double dot = 0;
  for (size_t i = 0; i < 4; ++i) dot += double(a[i + 3]) * b[i + 3];
  const double sign = dot < 0 ? -1.0 : 1.0;
  double length = 0;
  for (size_t i = 0; i < 4; ++i) {
    output[i + 3] = float(double(a[i + 3]) + (sign * double(b[i + 3]) - a[i + 3]) * amount);
    length += double(output[i + 3]) * output[i + 3];
  }
  if (!(length > 0) || !std::isfinite(length))
    throw std::runtime_error("BoneAnimation quaternion interpolation collapsed");
  const double inverseLength = 1.0 / std::sqrt(length);
  for (size_t i = 0; i < 4; ++i) output[i + 3] = float(output[i + 3] * inverseLength);
  for (size_t i = 7; i < 10; ++i)
    output[i] = float(double(a[i]) + (double(b[i]) - a[i]) * amount);
  return output;
}
inline TransformSample Sample(const Track& track, double phase) {
  return Sample(track.samples, phase);
}
}  // namespace EiemAuthorBoneAnimation
