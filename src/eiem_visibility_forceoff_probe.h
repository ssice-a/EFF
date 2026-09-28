#pragma once

// Temporary, compile-time-gated visibility bridge experiment. This file is
// deliberately outside the normal replacement path so the stable DLL keeps
// the existing Renderer lifecycle untouched.

struct EiemVisibilityForceOffEntry {
  bool forcedOff = false;
  bool loggedUnavailable = false;
  bool hasSample = false;
  bool lastVisible = false;
  bool sawVisible = false;
  bool lastEnabled = true;
  bool lastForceRenderingOff = false;
  bool lastActive = true;
};

static SRWLOCK s_eiemVisibilityForceOffLock = SRWLOCK_INIT;
static std::map<void *, EiemVisibilityForceOffEntry>
    s_eiemVisibilityForceOffEntries;

static void EiemRunVisibilityForceOffProbe() {
  if (!kEiemEnableVisibilityForceOffProbe || !EiemOnUnityThread()) return;

  struct Target {
    void *renderer = nullptr;
    void *drawRenderer = nullptr;
    void *replacementMesh = nullptr;
    char section[96] = {};
  };
  std::vector<Target> targets;

  // Snapshot only raw Unity pointers and labels. Never hold the override lock
  // while invoking Unity methods or changing Renderer.enabled.
  AcquireSRWLockShared(&s_eiemOverrideLock);
  targets.reserve(s_eiemOverrides.size());
  for (const auto &state : s_eiemOverrides) {
    if (state.restorePending || !state.ownsMesh || !state.renderer ||
        !state.drawRenderer || !state.replacementMesh)
      continue;
    Target target;
    target.renderer = state.renderer;
    target.drawRenderer = state.drawRenderer;
    target.replacementMesh = state.replacementMesh;
    strncpy_s(target.section, sizeof(target.section), state.renderSection,
              _TRUNCATE);
    targets.push_back(target);
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);

  for (const auto &target : targets) {
    if (!target.renderer || !target.drawRenderer) continue;

    bool wasForcedOff = false;
    bool loggedUnavailable = false;
    bool hadSample = false;
    bool lastVisible = false;
    bool sawVisible = false;
    bool lastEnabled = true;
    bool lastForceRenderingOff = false;
    bool lastActive = true;
    AcquireSRWLockShared(&s_eiemVisibilityForceOffLock);
    const auto previous = s_eiemVisibilityForceOffEntries.find(target.renderer);
    if (previous != s_eiemVisibilityForceOffEntries.end()) {
      wasForcedOff = previous->second.forcedOff;
      loggedUnavailable = previous->second.loggedUnavailable;
      hadSample = previous->second.hasSample;
      lastVisible = previous->second.lastVisible;
      sawVisible = previous->second.sawVisible;
      lastEnabled = previous->second.lastEnabled;
      lastForceRenderingOff = previous->second.lastForceRenderingOff;
      lastActive = previous->second.lastActive;
    }
    ReleaseSRWLockShared(&s_eiemVisibilityForceOffLock);

    // A forced-off Renderer makes isVisible false by definition. Re-enable it
    // for one read so the next sample can distinguish a real source-visible
    // transition from our own diagnostic mutation.
    if (wasForcedOff) EiemSetRendererEnabled(target.drawRenderer, true);

    bool enabled = true;
    bool visible = false;
    bool forceRenderingOff = false;
    bool active = true;
    const bool enabledRead =
        EiemReadRendererEnabled(target.drawRenderer, &enabled);
    const bool visibleRead =
        EiemReadRendererVisible(target.drawRenderer, &visible);
    const bool forceRenderingOffRead = EiemReadRendererForceRenderingOff(
        target.drawRenderer, &forceRenderingOff);
    if (g_component_get_gameObject && g_gameObject_get_activeInHierarchy) {
      void *gameObject = Invoke(g_component_get_gameObject, target.drawRenderer);
      EiemReadBoxedBool(g_gameObject_get_activeInHierarchy, gameObject, &active);
    }
    if (!visibleRead) {
      if (!loggedUnavailable) {
        Log("[TEMP-VIS-FORCEOFF-v1] renderer=%p section=%s "
            "isVisible=unavailable; no mutation",
            target.drawRenderer,
            target.section[0] ? target.section : "<unknown>");
        AcquireSRWLockExclusive(&s_eiemVisibilityForceOffLock);
        s_eiemVisibilityForceOffEntries[target.renderer].loggedUnavailable =
            true;
        ReleaseSRWLockExclusive(&s_eiemVisibilityForceOffLock);
      }
      continue;
    }

    const bool trueToFalse = hadSample && lastVisible && !visible;
    const bool falseToTrue = hadSample && !lastVisible && visible;
    const bool shouldForceOff = trueToFalse && sawVisible && enabledRead && enabled;

    // Publish the observation before mutating enabled. A first false sample is
    // expected for inactive LODs and is never a hide event.
    AcquireSRWLockExclusive(&s_eiemVisibilityForceOffLock);
    auto &entry = s_eiemVisibilityForceOffEntries[target.renderer];
    entry.hasSample = true;
    entry.lastVisible = visible;
    entry.sawVisible = sawVisible || visible;
    entry.lastEnabled = enabled;
    entry.lastForceRenderingOff = forceRenderingOff;
    entry.lastActive = active;
    entry.loggedUnavailable = loggedUnavailable;
    ReleaseSRWLockExclusive(&s_eiemVisibilityForceOffLock);

    if (wasForcedOff && !visible) {
      // The source-like visibility is still false. Keep the temporary hide in
      // effect after the pre-read re-enable above; otherwise the replacement
      // would flash back on every 100 ms while the source remains hidden.
      const bool disabled = EiemSetRendererEnabled(target.drawRenderer, false);
      AcquireSRWLockExclusive(&s_eiemVisibilityForceOffLock);
      s_eiemVisibilityForceOffEntries[target.renderer].forcedOff = disabled;
      ReleaseSRWLockExclusive(&s_eiemVisibilityForceOffLock);
    } else if (shouldForceOff) {
      // This is the only mutation in v2: a proven true->false transition on a
      // Renderer that was already visible. The Mesh pointer is untouched.
      const bool disabled = EiemSetRendererEnabled(target.drawRenderer, false);
      AcquireSRWLockExclusive(&s_eiemVisibilityForceOffLock);
      auto &updated = s_eiemVisibilityForceOffEntries[target.renderer];
      updated.forcedOff = disabled;
      ReleaseSRWLockExclusive(&s_eiemVisibilityForceOffLock);
      Log("[TEMP-VIS-SYNC-v2] renderer=%p section=%s mesh=%p "
          "transition=true->false enabledBefore=%d action=disable setter=%d",
          target.drawRenderer,
          target.section[0] ? target.section : "<unknown>",
          target.replacementMesh, enabledRead ? (enabled ? 1 : 0) : -1,
          disabled ? 1 : 0);
    } else if (wasForcedOff && falseToTrue) {
      // `wasForcedOff` was temporarily cleared before this sample. Restore the
      // normal enabled state only after the source-like visibility recovers.
      const bool restored = EiemSetRendererEnabled(target.drawRenderer, true);
      AcquireSRWLockExclusive(&s_eiemVisibilityForceOffLock);
      s_eiemVisibilityForceOffEntries.erase(target.renderer);
      ReleaseSRWLockExclusive(&s_eiemVisibilityForceOffLock);
      Log("[TEMP-VIS-SYNC-v2] renderer=%p section=%s mesh=%p "
          "transition=false->true action=restore setter=%d",
          target.drawRenderer,
          target.section[0] ? target.section : "<unknown>",
          target.replacementMesh, restored ? 1 : 0);
    } else if (trueToFalse) {
      Log("[TEMP-VIS-SYNC-v2] renderer=%p section=%s mesh=%p "
          "transition=true->false enabled=%d forceRenderingOff=%d active=%d "
          "action=observe-only",
          target.drawRenderer,
          target.section[0] ? target.section : "<unknown>",
          target.replacementMesh, enabledRead ? (enabled ? 1 : 0) : -1,
          forceRenderingOffRead ? (forceRenderingOff ? 1 : 0) : -1,
          active ? 1 : 0);
    } else if (falseToTrue) {
      Log("[TEMP-VIS-SYNC-v2] renderer=%p section=%s mesh=%p "
          "transition=false->true action=observe-only",
          target.drawRenderer,
          target.section[0] ? target.section : "<unknown>",
          target.replacementMesh);
    } else if (!hadSample && !visible) {
      Log("[TEMP-VIS-SYNC-v2] renderer=%p section=%s mesh=%p "
          "initial isVisible=0 action=observe-only",
          target.drawRenderer,
          target.section[0] ? target.section : "<unknown>",
          target.replacementMesh);
    }

    if (!hadSample || visible != lastVisible || enabled != lastEnabled ||
        forceRenderingOff != lastForceRenderingOff || active != lastActive) {
      Log("[TEMP-VIS-SYNC-v3] renderer=%p section=%s mesh=%p "
          "visible=%d enabled=%d forceRenderingOff=%d active=%d "
          "reads=%d/%d/%d",
          target.drawRenderer,
          target.section[0] ? target.section : "<unknown>",
          target.replacementMesh, visible ? 1 : 0,
          enabledRead ? (enabled ? 1 : 0) : -1,
          forceRenderingOffRead ? (forceRenderingOff ? 1 : 0) : -1,
          active ? 1 : 0, visibleRead ? 1 : 0, enabledRead ? 1 : 0,
          forceRenderingOffRead ? 1 : 0);
    }
  }
}

static void EiemClearVisibilityForceOffProbe() {
  AcquireSRWLockExclusive(&s_eiemVisibilityForceOffLock);
  s_eiemVisibilityForceOffEntries.clear();
  ReleaseSRWLockExclusive(&s_eiemVisibilityForceOffLock);
}
