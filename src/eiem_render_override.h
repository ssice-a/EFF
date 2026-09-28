#pragma once

// Renderer override ledger, ownership, commit baselines and restore.
// Included after Unity/shape adapters by the IL2CPP host.

struct EiemRenderOverrideState {
  // `renderer` is the component that owns the Mesh: SkinnedMeshRenderer or
  // MeshFilter. `drawRenderer` owns materials and enabled state. They are the
  // same object for skinned meshes and sibling components for static meshes.
  void *renderer = nullptr;
  void *drawRenderer = nullptr;
  void *originalMesh = nullptr;
  void *replacementMesh = nullptr;
  EiemUnityRef rendererRef, drawRendererRef, sourceMeshRef,
      replacementMeshRef;
  std::shared_ptr<EiemSkeletonInstance> skeleton;
  bool restorePending = false;
  uint32_t originalMaterialsHandle = 0;
  uint32_t originalBonesHandle = 0;
  uint32_t replacementBonesHandle = 0;
  uint32_t originalRootBoneHandle = 0;
  bool originalEnabled = true;
  bool hasEnabled = false;
  // Temporary enabled ownership ends on successful commit.
  bool hasCommitEnabled = false;
  bool commitEnabled = true;
  bool hasMaterials = false;
  bool hasSkinning = false;
  bool ownsMesh = false;
  std::vector<size_t> materialSlots;
  EiemShapeState shapes;
  std::vector<EiemShapeBaseline> sourceShapeWeights;
  bool hasSourceShapeWeights = false;
  char rendererType[32] = {};
  uintptr_t ownerPrefabInstance = 0;
  char modPath[MAX_PATH] = {};
  char renderSection[96] = {};
};
struct EiemBounds {
  Vector3 center;
  Vector3 extents;
};
static SRWLOCK s_eiemOverrideLock = SRWLOCK_INIT;
static std::vector<EiemRenderOverrideState> s_eiemOverrides;

// A game controller must capture the game's materials, not our current output.
// Suppress material reapplication only for the Renderer being initialized;
// nested initialization of another model remains independent.
static thread_local std::vector<void *> s_eiemMaterialSourceInitRenderers;
static bool EiemMaterialSourceInitActive(void *renderer) {
  return std::find(s_eiemMaterialSourceInitRenderers.begin(),
                   s_eiemMaterialSourceInitRenderers.end(), renderer) !=
         s_eiemMaterialSourceInitRenderers.end();
}
struct EiemMaterialSourceInitScope {
  explicit EiemMaterialSourceInitScope(void *renderer) {
    s_eiemMaterialSourceInitRenderers.push_back(renderer);
  }
  ~EiemMaterialSourceInitScope() { s_eiemMaterialSourceInitRenderers.pop_back(); }
  EiemMaterialSourceInitScope(const EiemMaterialSourceInitScope &) = delete;
  EiemMaterialSourceInitScope &operator=(const EiemMaterialSourceInitScope &) = delete;
};

static bool EiemReadRendererEnabled(void *renderer, bool *enabled) {
  if (enabled) *enabled = true;
  if (!renderer || !enabled || !g_renderer_get_enabled) return false;
  __try {
    void *boxed = Invoke(g_renderer_get_enabled, renderer);
    if (!boxed) return false;
    *enabled = *(bool *)((char *)boxed + 16);
    return true;
  } __except (1) {
    return false;
  }
}

static int32_t EiemReadRendererMaterialCount(void *renderer) {
  if (!renderer || !g_renderer_get_sharedMaterials) return -1;
  __try {
    void *array = Invoke(g_renderer_get_sharedMaterials, renderer);
    if (!array) return 0;
    const uintptr_t count = *(uintptr_t *)((char *)array + 24);
    return count <= INT32_MAX ? (int32_t)count : -1;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

static bool EiemReadRendererVisible(void *renderer, bool *visible) {
  if (visible) *visible = false;
  if (!renderer || !visible || !g_renderer_get_isVisible) return false;
  __try {
    void *boxed = Invoke(g_renderer_get_isVisible, renderer);
    if (!boxed) return false;
    *visible = *(bool *)((char *)boxed + 16);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

static bool EiemReadBounds(void *getter, void *object, EiemBounds *out) {
  if (!getter || !object || !out) return false;
  void *boxed = Invoke(getter, object);
  if (!boxed) return false;
  *out = *(EiemBounds *)((char *)boxed + 16);
  return std::isfinite(out->center.x) && std::isfinite(out->center.y) &&
         std::isfinite(out->center.z) && std::isfinite(out->extents.x) &&
         std::isfinite(out->extents.y) && std::isfinite(out->extents.z) &&
         out->extents.x >= 0 && out->extents.y >= 0 && out->extents.z >= 0;
}

static void EiemSetReplacementDrawBounds(void *renderer,
                                         const char *rendererType,
                                         void *replacementMesh,
                                         const EiemBounds *sourceBounds) {
  if (!renderer || !replacementMesh ||
      !EiemModEquals(rendererType, "SkinnedMeshRenderer") ||
      !g_smr_set_localBounds || !s_eiemMeshGetBounds)
    return;
  EiemBounds replacement = {};
  if (!EiemReadBounds(s_eiemMeshGetBounds, replacementMesh, &replacement))
    return;
  if (sourceBounds) {
    Vector3 minimum = {
        (std::min)(sourceBounds->center.x - sourceBounds->extents.x,
                   replacement.center.x - replacement.extents.x),
        (std::min)(sourceBounds->center.y - sourceBounds->extents.y,
                   replacement.center.y - replacement.extents.y),
        (std::min)(sourceBounds->center.z - sourceBounds->extents.z,
                   replacement.center.z - replacement.extents.z)};
    Vector3 maximum = {
        (std::max)(sourceBounds->center.x + sourceBounds->extents.x,
                   replacement.center.x + replacement.extents.x),
        (std::max)(sourceBounds->center.y + sourceBounds->extents.y,
                   replacement.center.y + replacement.extents.y),
        (std::max)(sourceBounds->center.z + sourceBounds->extents.z,
                   replacement.center.z + replacement.extents.z)};
    replacement.center = {(minimum.x + maximum.x) * .5f,
                          (minimum.y + maximum.y) * .5f,
                          (minimum.z + maximum.z) * .5f};
    replacement.extents = {(maximum.x - minimum.x) * .5f,
                           (maximum.y - minimum.y) * .5f,
                           (maximum.z - minimum.z) * .5f};
  }
  void *params[] = {&replacement};
  Invoke(g_smr_set_localBounds, renderer, params);
}

static size_t EiemFindOverrideLocked(void *renderer) {
  for (size_t index = 0; index < s_eiemOverrides.size(); ++index)
    if (s_eiemOverrides[index].renderer == renderer &&
        s_eiemOverrides[index].rendererRef.Target() == renderer) return index;
  return SIZE_MAX;
}

// RendererInfo._Init may submit a direct NPC/UI Renderer before its model root
// has been registered. Claim it when the completed model traversal observes
// the same live Renderer; teardown can then retire its override by model.
static void EiemClaimRendererOverrideOwner(void *renderer, uintptr_t model) {
  if (!renderer || !model) return;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX && !s_eiemOverrides[index].ownerPrefabInstance)
    s_eiemOverrides[index].ownerPrefabInstance = model;
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
}

static void EiemPrepareRenderInput(void *renderer, void *mesh,
                                   const char *rendererType,
                                   void **identityMesh) {
  if (identityMesh) *identityMesh = mesh;
  if (!renderer || !mesh) return;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  size_t index = EiemFindOverrideLocked(renderer);
  // Do not create an entry while merely observing an unrelated Renderer.
  // Entries are created only after a rule has matched and a mutation is about
  // to happen, so reload restores exactly the objects we changed.
  if (index != SIZE_MAX) {
    EiemRenderOverrideState &state = s_eiemOverrides[index];
    if (state.restorePending) {
      if (identityMesh) *identityMesh = state.originalMesh;
      ReleaseSRWLockExclusive(&s_eiemOverrideLock);
      return;
    }
    if (state.replacementMesh && state.replacementMesh != mesh) {
      auto sourceRef = EiemUnityRef::Capture(mesh, false);
      if (!sourceRef) {
        ReleaseSRWLockExclusive(&s_eiemOverrideLock);
        Log("[MOD] Cannot retain changed source Mesh renderer=%p", renderer);
        return;
      }
      if (state.hasSourceShapeWeights) {
        EiemUnityShapes shapes;std::vector<EiemShapeBaseline> baseline;
        if (!shapes.Snapshot(renderer,mesh,baseline)) {
          ReleaseSRWLockExclusive(&s_eiemOverrideLock);
          Log("[SHAPE] Cannot capture changed game Mesh layout renderer=%p",renderer);
          return;
        }
        // A game-assigned Mesh is a new source, not the old replacement's
        // channels. Never carry its control claims or numeric mapping across.
        EiemRetireShapeBinding(state.shapes.binding);state.shapes={};
        state.sourceShapeWeights=std::move(baseline);
      }
      state.sourceMeshRef = std::move(sourceRef);
      state.originalMesh = mesh;
      state.replacementMesh = nullptr;
      state.replacementMeshRef = {};
    }
    if (identityMesh && state.replacementMesh == mesh)
      *identityMesh = state.originalMesh;
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
}

static bool EiemCaptureOriginal(void *renderer, void *drawRenderer, void *mesh,
                                const char *rendererType, bool meshEdit = false,
                                const EiemModRule *materialRule = nullptr, bool shapeEdit = false) {
  if (!renderer || !mesh) return false;
  if (!drawRenderer) drawRenderer = renderer;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  size_t index = EiemFindOverrideLocked(renderer);
  if (index == SIZE_MAX) {
    EiemRenderOverrideState state = {};
    state.rendererRef = EiemUnityRef::Capture(renderer);
    state.drawRendererRef = drawRenderer == renderer ? state.rendererRef : EiemUnityRef::Capture(drawRenderer);
    if (!state.rendererRef || !state.drawRendererRef) {
      ReleaseSRWLockExclusive(&s_eiemOverrideLock);
      Log("[MOD] Cannot observe Renderer lifetime; refusing mutation renderer=%p", renderer);
      return false;
    }
    state.renderer = renderer;
    state.drawRenderer = drawRenderer;
    state.originalMesh = mesh;
    strncpy_s(state.rendererType, sizeof(state.rendererType),
              rendererType ? rendererType : "Renderer", _TRUNCATE);
    state.ownerPrefabInstance = s_eiemActivePrefabInstance;
    index = s_eiemOverrides.size();
    s_eiemOverrides.push_back(state);
  }
  auto &state = s_eiemOverrides[index];
  if (state.restorePending) {
    ReleaseSRWLockExclusive(&s_eiemOverrideLock);
    Log("[MOD] Restore incomplete; refusing new mutation renderer=%p", renderer);
    return false;
  }
  if (!state.ownsMesh) state.originalMesh = mesh;
  if (meshEdit && !state.sourceMeshRef) {
    state.sourceMeshRef = EiemUnityRef::Capture(state.originalMesh, false);
    if (!state.sourceMeshRef) {
      ReleaseSRWLockExclusive(&s_eiemOverrideLock);
      Log("[MOD] Cannot retain source Mesh; refusing assignment renderer=%p", renderer);
      return false;
    }
  }
  if ((meshEdit || shapeEdit) && !state.ownsMesh && !state.hasSourceShapeWeights &&
      EiemModEquals(rendererType, "SkinnedMeshRenderer")) {
    EiemUnityShapes shapes;
    if (!shapes.Snapshot(renderer, mesh, state.sourceShapeWeights)) {
      ReleaseSRWLockExclusive(&s_eiemOverrideLock);
      Log("[SHAPE] Cannot capture source shape weights before mesh replacement renderer=%p", renderer);
      return false;
    }
    state.hasSourceShapeWeights = true;
  }
  if (materialRule && (materialRule->materialCount || materialRule->submeshCount)) {
    if (!state.hasMaterials && g_renderer_get_sharedMaterials && il2cpp_gchandle_new) {
      void *materials = Invoke(g_renderer_get_sharedMaterials, drawRenderer);
      if (materials) {
        state.originalMaterialsHandle = il2cpp_gchandle_new(materials, false);
        state.hasMaterials = state.originalMaterialsHandle != 0;
      }
    }
    if (!state.hasMaterials) {
      ReleaseSRWLockExclusive(&s_eiemOverrideLock);
      Log("[MOD] Cannot capture source material slots; renderer=%p", renderer);
      return false;
    }
    auto ownSlot = [&](size_t slot) {
      if (std::find(state.materialSlots.begin(), state.materialSlots.end(), slot) == state.materialSlots.end())
        state.materialSlots.push_back(slot);
    };
    for (uint32_t i = 0; i < materialRule->materialCount; ++i) ownSlot(materialRule->materialSlots[i]);
    for (uint32_t i = 0; i < materialRule->submeshCount; ++i)
      if (materialRule->submeshSlots[i] >= 0) ownSlot(i);
    // Array padding between the old end and an added slot belongs to us too.
    void *baseline = il2cpp_gchandle_get_target(state.originalMaterialsHandle);
    const size_t baselineCount = EiemManagedArrayLength(baseline);
    size_t required = baselineCount;
    for (size_t slot : state.materialSlots) required = (std::max)(required, slot + 1);
    for (size_t slot = baselineCount; slot < required; ++slot) ownSlot(slot);
  }
  // Mesh assignment may cause game skin setup to change its palette. Materials
  // and skip never own that palette and must not capture/restore it.
  if (meshEdit && !state.hasSkinning &&
      EiemModEquals(rendererType, "SkinnedMeshRenderer") && il2cpp_gchandle_new) {
    if (g_smr_get_bones) {
      void *bones = Invoke(g_smr_get_bones, renderer);
      if (bones) state.originalBonesHandle = il2cpp_gchandle_new(bones, false);
    }
    if (g_smr_get_rootBone) {
      void *rootBone = Invoke(g_smr_get_rootBone, renderer);
      if (rootBone) state.originalRootBoneHandle = il2cpp_gchandle_new(rootBone, false);
    }
    state.hasSkinning = state.originalBonesHandle != 0 || state.originalRootBoneHandle != 0;
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  return true;
}

static void EiemRememberRuleBinding(void *renderer,
                                    const EiemModRule &rule) {
  if (!renderer) return;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX) {
    auto &state = s_eiemOverrides[index];
    // A later resource-assembly refresh has no completed Prefab owner. Do not
    // erase an owner learned by the normal Prefab lifecycle adapter.
    if (s_eiemActivePrefabInstance)
      state.ownerPrefabInstance = s_eiemActivePrefabInstance;
    strncpy_s(state.modPath, sizeof(state.modPath), rule.modPath, _TRUNCATE);
    strncpy_s(state.renderSection, sizeof(state.renderSection), rule.section,
              _TRUNCATE);
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
}

static bool EiemFindBoundRenderRule(void *renderer, EiemModRule *out) {
  if (!renderer || !out) return false;
  char modPath[MAX_PATH] = {};
  char section[96] = {};
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX && !s_eiemOverrides[index].restorePending) {
    strncpy_s(modPath, sizeof(modPath), s_eiemOverrides[index].modPath,
              _TRUNCATE);
    strncpy_s(section, sizeof(section),
              s_eiemOverrides[index].renderSection, _TRUNCATE);
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  return modPath[0] && section[0] &&
         EiemFindRenderRuleBySection(modPath, section, out);
}

static void EiemBeginMeshWrite(void *renderer) {
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX) s_eiemOverrides[index].ownsMesh = true;
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
}

static bool EiemPrepareRendererShapeBinding(void *renderer,void *target,const char *type,char *error,size_t size) {
  if(!EiemModEquals(type,"SkinnedMeshRenderer"))return true;
  bool ok=false;std::string reason="Original shape baseline unavailable";
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  size_t index=EiemFindOverrideLocked(renderer);
  if(index!=SIZE_MAX && s_eiemOverrides[index].hasSourceShapeWeights) {
    auto &state=s_eiemOverrides[index];EiemUnityShapes backend;
    ok=EiemPrepareShapeBinding(state.shapes,renderer,target,state.sourceShapeWeights,backend,reason);
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  if(!ok && error)strncpy_s(error,size,reason.c_str(),_TRUNCATE);return ok;
}
static bool EiemInitializeRendererShapeBinding(void *renderer,const char *type,char *error,size_t size) {
  if(!EiemModEquals(type,"SkinnedMeshRenderer"))return true;
  bool ok=false;std::string reason="Shape state unavailable";
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  size_t index=EiemFindOverrideLocked(renderer);
  if(index!=SIZE_MAX) {EiemUnityShapes backend;ok=EiemInitializeBoundShapes(s_eiemOverrides[index].shapes,renderer,backend,reason);}
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  if(!ok && error)strncpy_s(error,size,reason.c_str(),_TRUNCATE);return ok;
}

static bool EiemRememberReplacement(void *renderer, void *replacementMesh,
                                    const char *rendererType) {
  if (!renderer || !replacementMesh) return false;
  // A changed file evicts the previous cache entry before the live Renderer is
  // rebound. Retain the replacement per Renderer as well, so cache eviction
  // can never leave a still-rendered Mesh wrapper eligible for collection.
  EiemUnityRef replacementRef =
      EiemUnityRef::Capture(replacementMesh, false);
  if (!replacementRef) {
    Log("[MOD] Cannot retain replacement Mesh renderer=%p mesh=%p type=%s",
        renderer, replacementMesh,
        rendererType ? rendererType : "Renderer");
    return false;
  }
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX) {
    s_eiemOverrides[index].replacementMesh = replacementMesh;
    s_eiemOverrides[index].replacementMeshRef = std::move(replacementRef);
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  return index != SIZE_MAX;
}

// Once a Renderer has been bound to a generated resource, the game may
// re-assign its original Mesh during LOD/skin refreshes or during a native VFX
// transaction.  Keep the replacement attached for that source-only write.
// Native VFX paths can hand us a distinct wrapper for the same logical asset,
// so pointer equality alone is insufficient.  Reload restoration sets
// s_eiemApplyingModMeshAssignment and bypasses this guard explicitly.
static bool EiemSameMeshIdentity(void *left, void *right) {
  if (!left || !right) return false;
  if (left == right) return true;

  int64_t leftHash = 0;
  int64_t rightHash = 0;
  char leftPath[768] = {};
  char rightPath[768] = {};
  const bool leftKnown =
      TraceLookupAssetOrigin(left, &leftHash, leftPath, sizeof(leftPath));
  const bool rightKnown =
      TraceLookupAssetOrigin(right, &rightHash, rightPath, sizeof(rightPath));
  if (leftKnown && rightKnown) {
    if (leftHash && rightHash && leftHash == rightHash) return true;
    if (leftPath[0] && rightPath[0] && strcmp(leftPath, rightPath) == 0)
      return true;
  }

  char leftName[192] = {};
  char rightName[192] = {};
  TraceReadUnityObjectName(left, leftName, sizeof(leftName));
  TraceReadUnityObjectName(right, rightName, sizeof(rightName));
  return leftName[0] && rightName[0] && strcmp(leftName, rightName) == 0;
}

static void *EiemReplacementForSourceMesh(void *renderer, void *mesh) {
  if (!renderer || !mesh) return nullptr;
  void *originalMesh = nullptr;
  void *replacement = nullptr;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX && !s_eiemOverrides[index].restorePending &&
      s_eiemOverrides[index].originalMesh &&
      s_eiemOverrides[index].replacementMesh) {
    originalMesh = s_eiemOverrides[index].originalMesh;
    replacement = s_eiemOverrides[index].replacementMesh;
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  if (replacement && EiemSameMeshIdentity(originalMesh, mesh))
    return replacement;

  // A sprint VFX object may be a newly cloned Renderer.  It has the same
  // source Mesh identity but no per-Renderer ledger entry yet, so the exact
  // lookup above cannot see it.  Use a generic source-identity fallback only
  // when every live override for that source agrees on one replacement.  An
  // ambiguous source is left untouched rather than guessing across models.
  void *uniqueReplacement = nullptr;
  size_t candidates = 0;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  for (const auto &state : s_eiemOverrides) {
    if (state.restorePending || !state.originalMesh ||
        !state.replacementMesh ||
        !EiemSameMeshIdentity(state.originalMesh, mesh))
      continue;
    ++candidates;
    if (!uniqueReplacement) {
      uniqueReplacement = state.replacementMesh;
    } else if (!EiemSameMeshIdentity(uniqueReplacement, state.replacementMesh)) {
      uniqueReplacement = nullptr;
      break;
    }
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  if (!uniqueReplacement || !candidates) return nullptr;
  static volatile LONG s_eiemSourceMeshFallbackLogs = 0;
  const LONG logIndex = InterlockedIncrement(&s_eiemSourceMeshFallbackLogs);
  if (logIndex <= 32)
    Log("[MOD-MESH-SOURCE-FALLBACK] renderer=%p source=%p replacement=%p "
        "candidates=%zu",
        renderer, mesh, uniqueReplacement, candidates);
  return uniqueReplacement;
}

// A Mesh/material override must never own Renderer.enabled. Only an explicit
// handling=skip directive changes it, and therefore only skip needs a value
// restored on reload. LOD setup is free to toggle enabled while a mesh-only
// rule is active.
static bool EiemCaptureEnabledForSkip(void *renderer, void *drawRenderer) {
  bool enabled = true;
  if (!renderer || !drawRenderer ||
      !EiemReadRendererEnabled(drawRenderer, &enabled))
    return false;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX && !s_eiemOverrides[index].hasEnabled) {
    s_eiemOverrides[index].originalEnabled = enabled;
    s_eiemOverrides[index].hasEnabled = true;
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  return index != SIZE_MAX;
}

static bool EiemCaptureCommitEnabled(void *renderer, void *drawRenderer) {
  bool enabled = true;
  if (!EiemReadRendererEnabled(drawRenderer, &enabled)) return false;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX && !s_eiemOverrides[index].hasCommitEnabled) {
    s_eiemOverrides[index].commitEnabled = enabled;
    s_eiemOverrides[index].hasCommitEnabled = true;
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  return index != SIZE_MAX;
}

static void EiemFinishRenderCommit(void *renderer) {
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX) s_eiemOverrides[index].hasCommitEnabled = false;
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
}

static bool EiemSetRendererEnabled(void *renderer, bool enabled) {
  if (!renderer || !g_renderer_set_enabled) return false;
  void *params[] = {&enabled};
  __try {
    Invoke(g_renderer_set_enabled, renderer, params);
    bool actual = !enabled;
    return EiemReadRendererEnabled(renderer, &actual) && actual == enabled;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    Log("[MOD] Renderer.enabled assignment failed: renderer=%p exception=0x%08lX",
        renderer, GetExceptionCode());
    return false;
  }
}

static void EiemReleaseOverrideHandles(const EiemRenderOverrideState &state) {
  EiemRetireShapeBinding(state.shapes.binding);
  if (state.originalMaterialsHandle && il2cpp_gchandle_free)
    il2cpp_gchandle_free(state.originalMaterialsHandle);
  if (state.originalBonesHandle && il2cpp_gchandle_free)
    il2cpp_gchandle_free(state.originalBonesHandle);
  if (state.replacementBonesHandle && il2cpp_gchandle_free)
    il2cpp_gchandle_free(state.replacementBonesHandle);
  if (state.originalRootBoneHandle && il2cpp_gchandle_free)
    il2cpp_gchandle_free(state.originalRootBoneHandle);
}

static bool EiemRestoreRenderOverrides(
    const std::vector<std::string> *affected = nullptr,
    void *onlyRenderer = nullptr) {
  std::vector<EiemRenderOverrideState> states;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  for (auto &state : s_eiemOverrides) {
    if (onlyRenderer && state.renderer != onlyRenderer) continue;
    if (!EiemModAffected(state.modPath, affected)) continue;
    state.restorePending = true; // stop re-entrant commits from reinstating old rules
    states.push_back(state);
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  size_t completed = 0;
  for (auto &state : states) {
    auto finish = [&](bool success) {
      bool removed = false;
      AcquireSRWLockExclusive(&s_eiemOverrideLock);
      for (size_t i = 0; i < s_eiemOverrides.size(); ++i) {
        if (s_eiemOverrides[i].renderer != state.renderer) continue;
        if (success) { s_eiemOverrides.erase(s_eiemOverrides.begin() + i); removed = true; }
        else {
          s_eiemOverrides[i].shapes = state.shapes;
          s_eiemOverrides[i].hasCommitEnabled = state.hasCommitEnabled;
          s_eiemOverrides[i].commitEnabled = state.commitEnabled;
        }
        break;
      }
      ReleaseSRWLockExclusive(&s_eiemOverrideLock);
      if (removed) { EiemReleaseOverrideHandles(state); ++completed; }
      if (!success) Log("[MOD] Restore incomplete; baseline retained renderer=%p mod=%s", state.renderer, state.modPath);
    };
    const int rendererStatus = state.rendererRef.Status();
    if (rendererStatus != 1) { finish(rendererStatus == 0); continue; }
    const int drawStatus = state.drawRendererRef.Status();
    bool restoredAll = true;
    EiemModRule noShapes = {};
    if (!EiemUpdateRendererShapes(state.renderer, state.rendererType, noShapes, state.shapes)) restoredAll = false;
    if (!state.shapes.owned.empty()) restoredAll = false;
    if (kEiemEnableLifecycleDiagnostics) Log("[DEBUG-hr1] restore renderer=%p original=%p replacement=%p restoreEnabled=%d originalEnabled=%d",
        state.renderer, state.originalMesh, state.replacementMesh,
        state.hasEnabled ? 1 : 0,
        state.originalEnabled ? 1 : 0);
    bool restoreEnabledBefore = true;
    const bool restoreEnabledRead =
        drawStatus == 1 && EiemReadRendererEnabled(state.drawRenderer,
                                                   &restoreEnabledBefore);
    bool rendererDisabledForRestore = false;
    if (state.ownsMesh && restoreEnabledRead && restoreEnabledBefore &&
        g_renderer_set_enabled) {
      if (!state.hasCommitEnabled) { state.hasCommitEnabled = true; state.commitEnabled = restoreEnabledBefore; }
      rendererDisabledForRestore =
          EiemSetRendererEnabled(state.drawRenderer, false);
      if (kEiemEnableSkinDiagnostics) Log("[DEBUG-HR-ATOMIC-v2] restore-begin renderer=%p section=%s "
          "enabledBefore=%d disabled=%d meshBefore=%p bonesBefore=%zu",
          state.drawRenderer, state.renderSection,
          restoreEnabledBefore ? 1 : 0, rendererDisabledForRestore ? 1 : 0,
          EiemReadSharedMesh(state.renderer, state.rendererType),
          g_smr_get_bones
              ? EiemManagedArrayLength(Invoke(g_smr_get_bones, state.renderer))
              : 0);
      if (!rendererDisabledForRestore) { finish(false); continue; }
    }
    const bool sourceAlive = !state.ownsMesh ||
        (state.sourceMeshRef.Target() == state.originalMesh && state.sourceMeshRef.Status() == 1);
    if (!sourceAlive) restoredAll = false;
    if (state.ownsMesh && sourceAlive &&
        EiemReadSharedMesh(state.renderer, state.rendererType) != state.originalMesh)
      EiemSetSharedMesh(state.renderer, state.originalMesh, state.rendererType, nullptr);
    if (state.ownsMesh && state.renderer) {
      void *restored = EiemReadSharedMesh(state.renderer, state.rendererType);
      if (restored != state.originalMesh) restoredAll = false;
      if (sourceAlive && restored == state.originalMesh && state.hasSourceShapeWeights) {
        EiemUnityShapes shapes;
        for (const auto &entry : EiemShapeSourceWeights(state.shapes,state.sourceShapeWeights)) {
          const int index = shapes.Index(restored, entry.name);
          float actual=0;
          if (index < 0 || !shapes.Write(state.renderer, index, entry.value) ||
              !shapes.Read(state.renderer,index,actual) || std::abs(actual-entry.value)>.0001f) {
            restoredAll = false;
            Log("[SHAPE] Cannot restore source channel renderer=%p name=%s", state.renderer, entry.name.c_str());
          }
        }
      }
      if (kEiemEnableLifecycleDiagnostics) Log("[DEBUG-hr1] restore result renderer=%p actual=%p expected=%p ok=%d",
          state.renderer, restored, state.originalMesh,
          restored == state.originalMesh ? 1 : 0);
    }
    if (drawStatus < 0 && (state.hasMaterials || state.hasEnabled)) restoredAll = false;
    if (drawStatus == 1 && state.hasMaterials &&
        state.originalMaterialsHandle &&
        s_eiemRendererSetSharedMaterials && il2cpp_gchandle_get_target) {
      void *original = il2cpp_gchandle_get_target(state.originalMaterialsHandle);
      void *current = g_renderer_get_sharedMaterials ? Invoke(g_renderer_get_sharedMaterials, state.drawRenderer) : nullptr;
      if (original && current && s_eiemMaterialClass) {
        const size_t originalCount = EiemManagedArrayLength(original), currentCount = EiemManagedArrayLength(current);
        void **originalItems = (void **)((char *)original + IL2CPP_ARRAY_DATA);
        void **currentItems = (void **)((char *)current + IL2CPP_ARRAY_DATA);
        std::vector<void *> live(currentItems, currentItems + currentCount);
        std::vector<void *> baseline(originalItems, originalItems + originalCount);
        auto restored = EiemRestoreOwnedSlots(live, baseline, state.materialSlots);
        bool valid = true;
        for (size_t slot : state.materialSlots)
          if (slot < restored.size() && restored[slot] && EiemNativeObjectStatus(restored[slot]) != 1) valid = false;
        if (!valid) restoredAll = false;
        if (valid && restored != live) {
          void *array = il2cpp_array_new(s_eiemMaterialClass, restored.size());
          if (array) {
            if (!restored.empty()) memcpy((char *)array + IL2CPP_ARRAY_DATA, restored.data(), restored.size() * sizeof(void *));
            void *params[] = {array};
            Invoke(s_eiemRendererSetSharedMaterials, state.drawRenderer, params);
            if (!EiemManagedObjectArraySame(array, Invoke(g_renderer_get_sharedMaterials, state.drawRenderer))) restoredAll = false;
          } else { restoredAll = false; Log("[MOD] Cannot allocate restored material slots renderer=%p", state.drawRenderer); }
        }
      } else restoredAll = false;
    } else if (drawStatus == 1 && state.hasMaterials) restoredAll = false;
    if (sourceAlive && state.ownsMesh &&
        EiemReadSharedMesh(state.renderer, state.rendererType) == state.originalMesh && state.hasSkinning &&
        il2cpp_gchandle_get_target) {
      if (state.originalBonesHandle) {
        void *bones = il2cpp_gchandle_get_target(state.originalBonesHandle);
        if (!bones || !g_smr_get_bones || !g_smr_set_bones) restoredAll = false;
        else if (!EiemManagedObjectArraySame(bones, Invoke(g_smr_get_bones, state.renderer))) {
          void *params[] = {bones};
          Invoke(g_smr_set_bones, state.renderer, params);
          if (!EiemManagedObjectArraySame(bones, Invoke(g_smr_get_bones, state.renderer))) restoredAll = false;
        }
      }
      if (state.originalRootBoneHandle) {
        void *rootBone = il2cpp_gchandle_get_target(state.originalRootBoneHandle);
        if (!rootBone || !g_smr_get_rootBone || !g_smr_set_rootBone || EiemNativeObjectStatus(rootBone) != 1) restoredAll = false;
        else if (rootBone != Invoke(g_smr_get_rootBone, state.renderer)) {
          void *params[] = {rootBone};
          Invoke(g_smr_set_rootBone, state.renderer, params);
          if (rootBone != Invoke(g_smr_get_rootBone, state.renderer)) restoredAll = false;
        }
      }
    }
    if (drawStatus == 1 && (state.hasEnabled || state.hasCommitEnabled || rendererDisabledForRestore) &&
        g_renderer_set_enabled) {
      bool enabled = restoredAll && (state.hasEnabled ? state.originalEnabled
                                      : state.hasCommitEnabled ? state.commitEnabled : restoreEnabledBefore);
      void *params[] = {&enabled};
      Invoke(g_renderer_set_enabled, state.drawRenderer, params);
      bool actual = !enabled;
      if (!EiemReadRendererEnabled(state.drawRenderer, &actual) || actual != enabled) restoredAll = false;
    } else if (drawStatus == 1 && (state.hasEnabled || state.hasCommitEnabled || rendererDisabledForRestore)) {
      restoredAll = false;
    }
    if (kEiemEnableSkinDiagnostics && state.ownsMesh && drawStatus == 1) {
      bool enabledAfter = false;
      const bool readAfter =
          EiemReadRendererEnabled(state.drawRenderer, &enabledAfter);
      if (kEiemEnableSkinDiagnostics) Log("[DEBUG-HR-ATOMIC-v2] restore-end renderer=%p section=%s "
          "meshAfter=%p bonesAfter=%zu enabledAfter=%d readBack=%d ok=%d",
          state.drawRenderer, state.renderSection,
          EiemReadSharedMesh(state.renderer, state.rendererType),
          g_smr_get_bones
              ? EiemManagedArrayLength(Invoke(g_smr_get_bones, state.renderer))
              : 0,
          enabledAfter ? 1 : 0, readAfter ? 1 : 0, restoredAll ? 1 : 0);
    }
    finish(restoredAll);
  }
  if (!states.empty())
    Log("[MOD] Restore complete: released=%zu pending=%zu", completed, states.size() - completed);
  return completed == states.size();
}

// The game is about to unload this Prefab. Its Renderer objects must not be
// called while restoring: their native side may already be entering teardown.
// Drop only EIEM bookkeeping and managed handles owned by this instance.
static void EiemForgetRenderOverrides(uintptr_t ownerPrefabInstance) {
  if (!ownerPrefabInstance) return;
  std::vector<EiemRenderOverrideState> forgotten;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  for (size_t index = 0; index < s_eiemOverrides.size();) {
    if (s_eiemOverrides[index].ownerPrefabInstance != ownerPrefabInstance) {
      ++index;
      continue;
    }
    forgotten.push_back(s_eiemOverrides[index]);
    s_eiemOverrides.erase(s_eiemOverrides.begin() + index);
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  for (const auto &state : forgotten) {
    // No extra dereference of objects already entering teardown.
    // An owner can release a pooled or deferred-destroy Renderer while its
    // native bones array still references our nodes. Preserve that dependency
    // without calling any native method from this teardown callback.
    if (state.skeleton && state.ownsMesh)
      EiemRetainSkeletonConsumer(*state.skeleton,state.rendererRef);
    EiemReleaseOverrideHandles(state);
  }
  if (!forgotten.empty())
    Log("[MOD-PREFAB] forgot %zu Renderer override(s) for instance=%p",
        forgotten.size(), (void *)ownerPrefabInstance);
}
