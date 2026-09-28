#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Immutable resource identity, never instance Transform pointers.
struct EiemSkinIdentity {
  std::vector<std::string> paths;
  std::vector<uint32_t> hashes;
  // Stable structural identity emitted by the exporter. Each path is a
  // slash-separated child-index sequence relative to the skeleton root;
  // names are deliberately excluded so equivalent PFB variants may rename
  // their bones without changing the binding.
  std::vector<std::string> boneIndexPaths;
};

struct EiemSkinPaletteCache {
  void *model = nullptr;
  // One table per native skeleton instance observed during this model
  // transaction. The table owns no Unity objects; it only keeps the resolved
  // Transform pointers until the transaction ends.
  struct BoneTable {
    void *root = nullptr;
    std::unordered_map<std::string, void *> byIndexPath;
  };
  std::vector<BoneTable> boneTables;
};

static thread_local EiemSkinPaletteCache *s_eiemActiveSkinPaletteCache = nullptr;

static bool EiemSkinPathSuffix(const std::string &full, const std::string &path) {
  return !path.empty() && (full == path ||
      (full.size() > path.size() && full[full.size()-path.size()-1] == '/' &&
       full.compare(full.size()-path.size(), path.size(), path) == 0));
}

static bool EiemResolveSkinPathIndices(const std::vector<std::string> &payload,
                                        const std::vector<std::string> &live,
                                        std::vector<size_t> &indices,
                                        std::string &error) {
  indices.clear();
  for (const auto &path : payload) {
    size_t found = SIZE_MAX;
    for (size_t i=0; i<live.size(); ++i) if (live[i] == path) {
      if (found != SIZE_MAX) { error = "Ambiguous skeleton bone path: " + path; indices.clear(); return false; }
      found=i;
    }
    if (found == SIZE_MAX) { error = "Skeleton bone path not found: " + path; indices.clear(); return false; }
    indices.push_back(found);
  }
  return true;
}
