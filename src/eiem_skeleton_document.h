#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Resource-local paths, never Transform pointers or animation-buffer indices.
struct EiemSkeletonNode {
  std::string path;
  int32_t parent = -1;
  float position[3] = {};
  float rotation[4] = {0, 0, 0, 1};
  float scale[3] = {1, 1, 1};
  bool source = true;
};
struct EiemSkeletonDocument {
  std::vector<EiemSkeletonNode> nodes;
};

static bool EiemValidateSkeleton(const EiemSkeletonDocument &document, std::string &error) {
  if (document.nodes.empty() || document.nodes.size() > 16384) {
    error = "Invalid Skeleton node count"; return false;
  }
  std::unordered_map<std::string, size_t> paths;
  for (size_t i = 0; i < document.nodes.size(); ++i) {
    const auto &node = document.nodes[i];
    auto fail = [&](const char *why) { error = std::string(why) + ": " + node.path; return false; };
    if (node.path.size() > 4096) return fail("Skeleton path is too long");
    if (!paths.emplace(node.path, i).second) return fail("Duplicate Skeleton path");
    if ((!node.path.empty() && node.path.front()=='/') ||
        node.path.find('\\') != std::string::npos || node.path.find('\0') != std::string::npos ||
        node.path.find("//") != std::string::npos || node.path == "." || node.path == ".." ||
        node.path.find("/../") != std::string::npos || node.path.find("/./") != std::string::npos)
      return fail("Invalid Skeleton path");
    if (i == 0) {
      if (node.parent != -1 || !node.source || node.path.find('/') != std::string::npos)
        return fail("Skeleton root must reference the source hierarchy");
    } else {
      if (node.parent < 0 || (size_t)node.parent >= i) return fail("Skeleton parent must precede child");
      const auto &parent = document.nodes[node.parent];
      const auto slash = node.path.find_last_of('/');
      const auto prefix = slash == std::string::npos ? std::string() : node.path.substr(0, slash);
      const auto name = slash == std::string::npos ? node.path : node.path.substr(slash + 1);
      if (prefix != parent.path || name.empty() || name == "." || name == "..") return fail("Skeleton path/parent mismatch");
      if (node.source && !parent.source) return fail("Source bone cannot be reparented under a new bone");
    }
    for (float x : node.position) if (!std::isfinite(x)) return fail("Non-finite Skeleton position");
    float length = 0;
    for (float x : node.rotation) { if (!std::isfinite(x)) return fail("Non-finite Skeleton rotation"); length += x*x; }
    if (std::abs(length - 1.f) > .001f) return fail("Skeleton quaternion must be normalized");
    for (float x : node.scale) if (!std::isfinite(x) || x <= 0) return fail("Unsupported Skeleton scale");
  }
  return true;
}

// EIESKEL v2 requires explicit provenance on every node.
template <typename Reader>
static bool EiemReadSkeleton(Reader &reader, EiemSkeletonDocument &out, std::string &error) {
  EiemSkeletonDocument next;
  char magic[8] = {}; int32_t version = 0, root = -1;
  uint32_t count = 0, paletteCount = 0; std::string coordinate;
  auto invalid = [&] { error = "Invalid or truncated EIEM Skeleton resource"; return false; };
  if (!reader.Bytes(magic, 8) || std::string(magic,8) != std::string("EIESKEL\0",8) ||
      !reader.Value(&version) || version != 2 || !reader.String(&coordinate) ||
      coordinate != "unity-y-up-left-handed" || !reader.Count(&count,16384)) return invalid();
  next.nodes.resize(count);
  for (auto &node : next.nodes)
    if (!reader.String(&node.path) || node.path.size() > 4096 || !reader.Value(&node.parent) ||
        !reader.Bytes(node.position,sizeof(node.position)) || !reader.Bytes(node.rotation,sizeof(node.rotation)) ||
        !reader.Bytes(node.scale,sizeof(node.scale))) return invalid();
  // Per-renderer palettes were removed from Skeleton; they live in Mesh.
  if (!reader.Count(&paletteCount,0) || !reader.Value(&root) || root != -1) return invalid();

    uint32_t flags = 0;
    if (!reader.Count(&flags,16384) || flags != count) return invalid();
    for (auto &node : next.nodes) {
      uint8_t source = 0;
      if (!reader.Value(&source) || source > 1) return invalid();
      node.source = source != 0;
    }

  if (!reader.End()) return invalid();
  if (!EiemValidateSkeleton(next,error)) return false;
  out = std::move(next); return true;
}
