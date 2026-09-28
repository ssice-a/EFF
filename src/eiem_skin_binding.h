#pragma once
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Immutable resource identity, never instance Transform pointers.
struct EiemSkinIdentity {
  struct Source {
    std::string meshPath;
    std::string meshAsset;
    uint32_t slot = 0;
  };
  std::vector<std::string> paths;
  std::vector<uint32_t> hashes;
  // EIEMESH v6 records every valid source-Mesh/slot that can provide this
  // palette entry.  A candidate may be absent from one PFB (for example a
  // lower LOD or a UI model), so the runtime tries the candidates that exist
  // in the current model instance.  All candidates that do exist must point
  // at the same Transform.
  std::vector<std::vector<Source>> sourceCandidates;
};

struct EiemLiveSkinSource {
  std::string source;
  std::string asset;
  void *renderer = nullptr;
  void *bones = nullptr;
  // The native assembly hook records the LOD that produced this Renderer.
  // -1 means that this creation path did not expose an LOD.  The value is
  // only a preference when choosing a complete donor palette; it never lets
  // a source cross the current skinningRoot instance boundary.
  int32_t lod = -1;
};

// One replacement Mesh can be bound to several target LOD Renderers in the
// same model transaction.  Keep one resolved Transform palette per generated
// Mesh identity so lod1/lod2/lod3 reuse the complete source palette selected
// for the replacement Mesh instead of resolving their smaller local palettes
// independently.  The cache is transaction-scoped and never crosses model
// instances or F10 replays.
struct EiemSkinPaletteCacheEntry {
  const EiemSkinIdentity *identity = nullptr;
  std::vector<void *> bones;
  void *providerRenderer = nullptr;
  int32_t providerLod = -1;
  bool failed = false;
  std::string error;
};

struct EiemSkinPaletteCache {
  void *model = nullptr;
  std::vector<EiemSkinPaletteCacheEntry> entries;
};

static thread_local EiemSkinPaletteCache *s_eiemActiveSkinPaletteCache = nullptr;

// LOD renderers use asset names such as `S_actor_lizhiyan_cloth_03_lod1`.
// When a replacement authored from LOD0 is assigned to a lower LOD, the
// complete donor palette must come from the same logical Mesh family.  The
// old resolver ranked every LOD0 donor by palette size, so a larger body or
// cloth02 palette could win over cloth03's own breast bones.
static std::string EiemSkinAssetFamily(const std::string &asset) {
  const size_t marker = asset.rfind("_lod");
  if (marker == std::string::npos || marker + 4 >= asset.size()) return asset;
  for (size_t index = marker + 4; index < asset.size(); ++index)
    if (!std::isdigit(static_cast<unsigned char>(asset[index]))) return asset;
  return asset.substr(0, marker);
}

// A replacement slot is resolved by this resource identity, never by the
// target LOD's local array index. A lower LOD may omit slots present in LOD0;
// the resolver must search the same model instance's native source renderers.
// Missing identities are hard failures, because guessing an index produces a
// valid-looking but incorrectly skinned mesh.

static bool EiemSkinPathSuffix(const std::string &full, const std::string &path) {
  return !path.empty() && (full == path ||
      (full.size() > path.size() && full[full.size()-path.size()-1] == '/' &&
       full.compare(full.size()-path.size(), path.size(), path) == 0));
}

// Find a single skeleton instance from the source palette, not from a scene
// search or character/prefab names. An original slot anchors extended slots.
static bool EiemSkinRootPath(const std::vector<std::string> &sourcePaths,
                              const std::vector<std::string> &payloadPaths,
                              std::string &root, std::string &error) {
  root.clear();
  for (const auto &source : sourcePaths) for (const auto &path : payloadPaths) {
    if (!EiemSkinPathSuffix(source, path)) continue;
    const size_t head = path.find('/');
    const std::string candidate = source.substr(0, source.size()-path.size()) + path.substr(0, head);
    if (!root.empty() && root != candidate) {
      error = "Mesh palette spans different skeleton instances"; return false;
    }
    root = candidate;
  }
  if (root.empty()) { error = "No source bone anchors the Mesh skeleton paths"; return false; }
  return true;
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
