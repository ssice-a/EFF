#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Local-space deformation only. Skin indices, bind poses and topology never
// enter this evaluator. A result is an immutable capture revision.
namespace EiemShapeSnapshot {
using Vec3 = std::array<float, 3>;
using Vec4 = std::array<float, 4>;
struct Geometry {
  std::vector<Vec3> positions, normals;
  std::vector<Vec4> tangents;
};
struct Frame {
  float weight = 0;
  std::vector<Vec3> positions, normals, tangents;
};
struct Channel { std::string name; std::vector<Frame> frames; };
inline void Check(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
template <typename V> inline void Finite(const std::vector<V>& data) {
  for (const auto& v : data) for (auto x : v) Check(std::isfinite(x), "nonfinite shape vector");
}
inline void Validate(const Geometry& base, const std::vector<Channel>& channels) {
  const auto count = base.positions.size();
  Check(count && count <= 1000000 && base.normals.size() == count && base.tangents.size() == count,
        "shape geometry channel sizes");
  Finite(base.positions); Finite(base.normals); Finite(base.tangents);
  std::vector<std::string> names;
  for (const auto& channel : channels) {
    Check(!channel.name.empty() && std::find(names.begin(), names.end(), channel.name) == names.end(),
          "duplicate/empty shape channel");
    names.push_back(channel.name);
    Check(!channel.frames.empty() && channel.frames.size() <= 256, "shape frame count");
    float last = -INFINITY;
    for (const auto& frame : channel.frames) {
      Check(std::isfinite(frame.weight) && frame.weight > last, "shape frame weights must increase");
      last = frame.weight;
      Check(frame.positions.size() == count && frame.normals.size() == count && frame.tangents.size() == count,
            "shape delta channel sizes");
      Finite(frame.positions); Finite(frame.normals); Finite(frame.tangents);
      if (frame.weight == 0) for (const auto* data : {&frame.positions, &frame.normals, &frame.tangents})
        for (const auto& v : *data) for (auto x : v) Check(x == 0, "nonzero delta at zero frame weight");
    }
  }
}
inline Geometry Evaluate(const Geometry& base, const std::vector<Channel>& channels,
                         const std::map<std::string, float>& weights) {
  Validate(base, channels);
  Geometry result = base; // Never modify a source or an older capture.
  for (const auto& value : weights) {
    Check(std::isfinite(value.second), "nonfinite shape weight");
    Check(std::find_if(channels.begin(),channels.end(),[&](const auto& c) { return c.name == value.first; }) != channels.end(),
          "unresolved shape weight channel");
  }
  for (const auto& channel : channels) {
    auto found = weights.find(channel.name);
    if (found == weights.end() || found->second == 0) continue;
    // The undeformed zero frame is implicit, including for negative frames.
    std::vector<std::pair<float, const Frame*>> points;
    bool zero = false;
    for (const auto& frame : channel.frames) { points.push_back({frame.weight,&frame}); zero |= frame.weight == 0; }
    if (!zero) points.push_back({0.0f,nullptr});
    std::sort(points.begin(),points.end(),[](const auto& a,const auto& b) { return a.first < b.first; });
    Check(points.size() >= 2, "shape has no nonzero frame");
    auto upper = std::upper_bound(points.begin(),points.end(),found->second,
        [](float w,const auto& p) { return w < p.first; });
    size_t high = upper == points.begin() ? 1 : upper == points.end() ? points.size()-1 : size_t(upper-points.begin());
    auto low = high-1;
    float alpha = (found->second-points[low].first)/(points[high].first-points[low].first);
    for (auto weighted : {std::make_pair(points[low].second,1-alpha),std::make_pair(points[high].second,alpha)}) {
      if (!weighted.first || weighted.second == 0) continue;
      const auto& frame = *weighted.first;
      for (size_t i=0;i<result.positions.size();++i) for (size_t axis=0;axis<3;++axis) {
        result.positions[i][axis] += frame.positions[i][axis]*weighted.second;
        result.normals[i][axis] += frame.normals[i][axis]*weighted.second;
        result.tangents[i][axis] += frame.tangents[i][axis]*weighted.second;
      }
    }
  }
  Finite(result.positions); Finite(result.normals); Finite(result.tangents);
  return result;
}
inline double Advance(double value, double target, double speed, double elapsed) {
  Check(std::isfinite(value) && std::isfinite(target) && std::isfinite(speed) && speed > 0 &&
        std::isfinite(elapsed) && elapsed >= 0, "shape control motion");
  return value < target ? (std::min)(target,value+speed*elapsed) : (std::max)(target,value-speed*elapsed);
}
} // namespace EiemShapeSnapshot
