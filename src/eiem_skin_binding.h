#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// A replacement Mesh slot is bound to one or more slots in the original
// Renderer palettes.  The candidates are deliberately asset based: the game
// may rename or insert Transform branches between PFBs, while the original
// Renderer already contains the exact Transform pointer Unity uses.
struct EiemSkinSourceCandidate {
  std::string meshPath;
  std::string meshAsset;
  uint32_t slot = 0;
};

// Immutable resource identity, never instance Transform pointers.
struct EiemSkinIdentity {
  std::vector<std::string> paths;
  std::vector<uint32_t> hashes;
  // The normal native path is resolved from source Renderer palettes captured
  // for this model instance. Each replacement slot may have several source
  // candidates (other LODs, clothing slots, or the body) and all candidates
  // that resolve must agree on the same Transform pointer.
  std::vector<std::vector<EiemSkinSourceCandidate>> sourceCandidates;
  // Retained only for an explicit Mod-owned Skeleton resource. Normal Mesh
  // replacement never consults this field because runtime hierarchy child
  // indices are not stable across PFB variants.
  std::vector<std::string> boneIndexPaths;
};

struct EiemSkinPaletteCache {
  void *model = nullptr;
  struct SourcePalette {
    void *renderer = nullptr;
    void *mesh = nullptr;
    std::string meshPath;
    std::string meshAsset;
    // Exact Transform pointers returned by the source Renderer.bones getter.
    std::vector<void *> bones;
  };
  // Captured before any replacement setter runs and discarded at the end of
  // the model transaction. This is instance scoped and never shared between
  // world, UI, NPC, or another character instance.
  std::vector<SourcePalette> sourcePalettes;
};

static thread_local EiemSkinPaletteCache *s_eiemActiveSkinPaletteCache = nullptr;

