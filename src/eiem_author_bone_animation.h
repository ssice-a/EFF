#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace EiemAuthorBoneAnimation {
using Rotation = std::array<float, 4>;
using RetargetPaths = std::map<std::string, std::string>; // target -> source

struct Node {
  std::string path;
  int32_t parent = -1;
  std::array<float, 3> position{};
  Rotation rotation{0, 0, 0, 1};
  std::array<float, 3> scale{1, 1, 1};
  bool source = true;
};

struct RotationRetarget {
  size_t ancestor;
  size_t source;
  std::string sourceRelativePath;
  Rotation before;
  Rotation after;
};

struct Skeleton {
  std::vector<Node> nodes;
  std::set<std::string> paths;
  std::map<size_t, RotationRetarget> retargets;
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

  std::string String16(bool allowEmpty = false) {
    Need(2);
    size_t count = size_t(bytes_[position_]) | (size_t(bytes_[position_ + 1]) << 8);
    position_ += 2;
    Need(count);
    if ((!count && !allowEmpty) || std::memchr(bytes_ + position_, 0, count))
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
      path.find('\\') != std::string::npos || path.find("//") != std::string::npos ||
      path.find('\0') != std::string::npos)
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

inline Rotation Multiply(const Rotation& a, const Rotation& b) {
  const auto [x, y, z, w] = a;
  const auto [u, v, s, t] = b;
  return {w*u + x*t + y*s - z*v, w*v - x*s + y*t + z*u,
          w*s + x*v - y*u + z*t, w*t - x*u - y*v - z*s};
}

inline Rotation Conjugate(const Rotation& q) { return {-q[0], -q[1], -q[2], q[3]}; }

inline Rotation Normalize(Rotation q) {
  double length = 0;
  for (auto value : q) length += double(value) * value;
  if (!std::isfinite(length) || length <= 0)
    throw std::runtime_error("invalid live bone rotation");
  const auto inverse = 1.0 / std::sqrt(length);
  for (auto& value : q) value = float(value * inverse);
  return q;
}

inline void SetRetargets(Skeleton& skeleton, const RetargetPaths& paths) {
  if (paths.empty()) return;
  const Rotation identity{0, 0, 0, 1};
  std::map<std::string, size_t> indices;
  std::vector<Rotation> worldRest;
  worldRest.reserve(skeleton.nodes.size());
  const auto parentRest = [&](const Node& node) {
    return node.parent < 0 ? identity : worldRest[size_t(node.parent)];
  };
  for (const auto& node : skeleton.nodes) {
    indices.emplace(node.path, worldRest.size());
    worldRest.push_back(Normalize(Multiply(parentRest(node), node.rotation)));
  }
  for (const auto& [targetPath, sourcePath] : paths) {
    const auto sourceIndex = indices.find(sourcePath), targetIndex = indices.find(targetPath);
    if (sourceIndex == indices.end() || targetIndex == indices.end())
      throw std::runtime_error("retarget path is absent from Skeleton: " + sourcePath + " -> " + targetPath);
    const auto& source = skeleton.nodes[sourceIndex->second];
    const auto& target = skeleton.nodes[targetIndex->second];
    if (!source.source || target.source)
      throw std::runtime_error("retarget needs a native source and an authored target: " + sourcePath + " -> " + targetPath);

    // The shared native ancestor lets the runtime resolve both paths on one
    // character instance, even when the source is outside the mesh palette.
    std::set<size_t> sourceAncestors;
    for (int32_t at = int32_t(sourceIndex->second); at >= 0; at = skeleton.nodes[size_t(at)].parent) {
      if (!skeleton.nodes[size_t(at)].source)
        throw std::runtime_error("retarget source hierarchy is not native: " + sourcePath);
      sourceAncestors.insert(size_t(at));
    }
    auto ancestor = target.parent;
    while (ancestor >= 0 && !sourceAncestors.count(size_t(ancestor)))
      ancestor = skeleton.nodes[size_t(ancestor)].parent;
    if (ancestor < 0)
      throw std::runtime_error("retarget bones have no shared native ancestor: " + targetPath);
    const auto& ancestorPath = skeleton.nodes[size_t(ancestor)].path;
    auto relative = sourcePath == ancestorPath ? std::string() :
        sourcePath.substr(ancestorPath.empty() ? 0 : ancestorPath.size() + 1);

    // Change the source's parent-space rotational delta into the target's
    // parent bind basis. Only the source's current local rotation is sampled.
    const auto before = Multiply(Conjugate(parentRest(target)), parentRest(source));
    const auto after = Multiply(Multiply(Conjugate(source.rotation), Conjugate(before)), target.rotation);
    skeleton.retargets.emplace(targetIndex->second,
        RotationRetarget{size_t(ancestor), sourceIndex->second, std::move(relative), before, after});
  }
}

// Carries local Transform addresses inside the assembled prefab. It contains
// no game bone names, native pointers, or cross-instance object identifiers.
using LocalBoneRoute = std::vector<int>;
using RetargetRoutes = std::map<size_t, LocalBoneRoute>;
inline std::string RetargetRoutePrefix(const std::string& skeletonId) {
  return "__EFF_RETARGET_ROUTES_1_" + skeletonId + ":";
}
inline std::string EncodeRetargetRoutes(const std::string& prefix, const RetargetRoutes& routes) {
  std::string encoded = prefix;
  for (const auto& [target, route] : routes) {
    encoded += std::to_string(target) + '=';
    for (auto step : route) encoded += std::to_string(step) + ',';
    encoded += ';';
  }
  return encoded;
}
inline RetargetRoutes DecodeRetargetRoutes(std::string_view rows, const Skeleton& skeleton) {
  const auto readIndex = [&rows](auto& value, char end) {
    const auto at = rows.find(end);
    if (at == rows.npos || !at) throw std::runtime_error("invalid prefab retarget route field");
    const auto last = rows.data() + at;
    const auto number = std::from_chars(rows.data(), last, value);
    if (number.ec != std::errc() || number.ptr != last)
      throw std::runtime_error("invalid prefab retarget route index");
    rows.remove_prefix(at + 1);
  };
  RetargetRoutes result;
  while (!rows.empty()) {
    size_t target = 0;
    readIndex(target, '=');
    const auto mapping = skeleton.retargets.find(target);
    if (mapping == skeleton.retargets.end())
      throw std::runtime_error("prefab retarget route has no authored target");
    const auto& path = skeleton.nodes.at(mapping->second.source).path;
    const auto depth = path.empty() ? 0 : size_t(std::count(path.begin(), path.end(), '/')) + 1;
    LocalBoneRoute route;
    route.reserve(depth);
    for (size_t i = 0; i < depth; ++i) {
      int child = 0;
      readIndex(child, ',');
      if (child < 0) throw std::runtime_error("negative prefab retarget child ordinal");
      route.push_back(child);
    }
    if (rows.empty() || rows.front() != ';' || !result.emplace(target, std::move(route)).second)
      throw std::runtime_error("invalid or duplicate prefab retarget route");
    rows.remove_prefix(1);
  }
  return result;
}

inline Rotation RetargetRotation(const RotationRetarget& retarget, const Rotation& current) {
  return Normalize(Multiply(Multiply(retarget.before, current), retarget.after));
}

inline void ValidateSample(const TransformSample& sample) {
  double length = 0;
  for (size_t i = 0; i < sample.size(); ++i) {
    if (!std::isfinite(sample[i])) throw std::runtime_error("author animation sample is non-finite");
    if (i >= 3 && i < 7) length += double(sample[i]) * sample[i];
  }
  if (std::abs(length - 1.0) > .001)
    throw std::runtime_error("author animation rotation is not normalized");
}

inline Skeleton DecodeSkeleton(const unsigned char* bytes, size_t size) {
  Reader reader(bytes, size);
  reader.Magic("EFFSKA1\0", 8);
  const auto version = reader.U32();
  if (version != 1 && version != 2) throw std::runtime_error("unsupported author Skeleton version");
  const auto count = reader.U32();
  if (!count || count > 16384) throw std::runtime_error("invalid author Skeleton node count");
  if (reader.String16() != "unity-y-up-left-handed")
    throw std::runtime_error("unsupported author Skeleton coordinate space");

  Skeleton result;
  result.nodes.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    Node node;
    node.path = reader.String16(i == 0);
    node.parent = reader.I32();
    for (auto& value : node.position) value = reader.F32();
    for (auto& value : node.rotation) value = reader.F32();
    for (auto& value : node.scale) value = reader.F32();
    if (version == 2) {
      const auto source = reader.U8();
      if (source > 1) throw std::runtime_error("invalid author Skeleton source flag");
      node.source = source != 0;
    }

    if ((!ValidPath(node.path) && !(i == 0 && node.path.empty())) ||
        !result.paths.insert(node.path).second)
      throw std::runtime_error("invalid or duplicate author Skeleton path: " + node.path);
    if (i == 0) {
      if (node.parent != -1 || node.path.find('/') != std::string::npos ||
          (!node.source && node.path.empty()))
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
      if (value == 0) throw std::runtime_error("author Skeleton bind transform is singular: " + node.path);
    result.nodes.push_back(std::move(node));
  }
  if (!reader.Done()) throw std::runtime_error("author Skeleton has trailing bytes");
  return result;
}

// The palette includes weighted bones only. Their unweighted author parents
// still need to exist and participate in animation.
inline std::vector<bool> AuthorBoneClosure(const Skeleton& skeleton, const std::set<std::string>& palette) {
  std::vector<bool> used(skeleton.nodes.size());
  for (size_t index = used.size(); index-- > 0;) {
    const auto& node = skeleton.nodes[index];
    used[index] = !node.source && (used[index] || palette.count(node.path));
    if (used[index] && node.parent >= 0) used[size_t(node.parent)] = true;
  }
  return used;
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
