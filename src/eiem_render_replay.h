#pragma once

#include <unordered_set>

// One Unity-thread update can visit the same Renderer through several nested
// world, UI or NPC model owners. A failed binding remains eligible for a later
// owner pass; a successful binding is committed only once in that update.
class EiemRenderReplayLedger {
 public:
  bool Committed(void *renderer) const {
    return renderer && committed_.find(renderer) != committed_.end();
  }

  void Commit(void *renderer) {
    if (renderer) committed_.insert(renderer);
  }

 private:
  std::unordered_set<void *> committed_;
};
