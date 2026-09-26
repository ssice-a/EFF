#pragma once

// Model registration, owner lifetime, retirement and replay snapshots.
static SRWLOCK s_eiemModelInstanceLock = SRWLOCK_INIT;
static std::vector<EiemModelInstanceState> s_eiemModelInstances;
static bool EiemModelHasActiveOwner(const EiemModelInstanceState &state);

// Find the concrete BaseModelViewPart that owns one model instance. The
// timing probe receives the GameObject/model, while the game's parallel
// renderer caches live on the owning part. This correlation is read-only and
// stays instance-local; it never falls back to a scene-wide search.
static void *EiemFindBaseModelPartForModel(void *model) {
  if (!model) return nullptr;
  void *result = nullptr;
  AcquireSRWLockShared(&s_eiemModelInstanceLock);
  for (const auto &state : s_eiemModelInstances) {
    if (state.model != model) continue;
    for (size_t index = 0; index < state.owners.size(); ++index) {
      const auto &owner = state.owners[index];
      if (owner.kind == EiemModelOwnerKind::BaseModelPart && owner.owner &&
          owner.active) {
        result = owner.owner;
        break;
      }
    }
    if (result) break;
  }
  ReleaseSRWLockShared(&s_eiemModelInstanceLock);
  return result;
}

// Snapshot the game's own parallel renderer caches at the same cold/F10
// windows as the public SkinnedMeshRenderer probe. This is deliberately
// observation-only: no HG data getter, renderer setter, array mutation, or
// GPU request is made here. The first question is whether the object the game
// has registered for the draw is the same object we inspect through SMR.
static void EiemLogBaseModelCacheProbe(void *part, LONG transaction,
                                       const char *phase) {
  if (!kEiemEnableCustomSkinPipelineObservation || !part ||
      !EiemOnUnityThread())
    return;

  __try {
    void *model = TraceReadObjectField(part, s_basePartModelOffset);
    void *root = (model && g_gameObject_get_transform)
                     ? Invoke(g_gameObject_get_transform, model)
                     : nullptr;
    void *renderers =
        TraceReadObjectField(part, s_basePartRenderersOffset);
    void *rendererStates =
        TraceReadObjectField(part, s_basePartRenderersInitStateOffset);
    void *hgRenderers =
        TraceReadObjectField(part, s_basePartHgRenderersOffset);
    void *hgRendererStates =
        TraceReadObjectField(part, s_basePartHgRenderersInitStateOffset);
    void *meshes = TraceReadObjectField(part, s_basePartMeshesOffset);
    void *meshStates =
        TraceReadObjectField(part, s_basePartMeshesInitStateOffset);
    void *boneCloths =
        TraceReadObjectField(part, s_basePartBoneClothsOffset);
    void *lodGroups = TraceReadObjectField(part, s_basePartLodGroupsOffset);
    const size_t rendererCount = EiemManagedArrayLength(renderers);
    const size_t rendererStateCount = EiemManagedArrayLength(rendererStates);
    const size_t hgCount = EiemManagedArrayLength(hgRenderers);
    const size_t hgStateCount = EiemManagedArrayLength(hgRendererStates);
    const size_t meshCount = EiemManagedArrayLength(meshes);
    const size_t meshStateCount = EiemManagedArrayLength(meshStates);
    const size_t boneClothCount = EiemManagedArrayLength(boneCloths);
    const size_t lodCount = EiemManagedArrayLength(lodGroups);
    Log("[BASEMODEL-CACHE-v1] tx=%ld phase=%s part=%p model=%p "
        "renderers=%p/%zu rendererStates=%zu hgRenderers=%p/%zu "
        "hgStates=%zu meshes=%p/%zu meshStates=%zu boneCloths=%p/%zu "
        "lodGroups=%p/%zu",
        transaction, phase ? phase : "unknown", part, model, renderers,
        rendererCount, rendererStateCount, hgRenderers, hgCount,
        hgStateCount, meshes, meshCount, meshStateCount, boneCloths,
        boneClothCount, lodGroups, lodCount);

    void **meshItems = meshes
                           ? (void **)((char *)meshes + IL2CPP_ARRAY_DATA)
                           : nullptr;
    const size_t meshLimit = (std::min)(meshCount, (size_t)256);
    for (size_t index = 0; meshItems && index < meshLimit; ++index) {
      void *renderer = meshItems[index];
      if (!renderer) continue;
      char path[768] = {};
      if (root) EiemBuildRelativeRendererPath(root, renderer, path,
                                              sizeof(path));
      const bool target =
          path[0] && (strstr(path, "body_01") || strstr(path, "cloth_01") ||
                      strstr(path, "cloth_02"));
      if (!target) continue;
      void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
      void *bones = g_smr_get_bones ? Invoke(g_smr_get_bones, renderer)
                                    : nullptr;
      bool enabled = false, visible = false;
      const bool enabledRead = EiemReadRendererEnabled(renderer, &enabled);
      const bool visibleRead = EiemReadRendererVisible(renderer, &visible);
      size_t overrideIndex = SIZE_MAX;
      AcquireSRWLockShared(&s_eiemOverrideLock);
      overrideIndex = EiemFindOverrideLocked(renderer);
      ReleaseSRWLockShared(&s_eiemOverrideLock);
      const int init = EiemReadManagedBoolArrayValue(
          meshStates, meshStateCount, index);
      const auto bounds = EiemSkinProbe::ReadRendererBounds(renderer);
      Log("[BASEMODEL-CACHE-v1] tx=%ld phase=%s kind=mesh index=%zu "
          "renderer=%p path=%s mesh=%p bones=%zu override=%s init=%d "
          "enabled=%s visible=%s boundsRead=%d boundsCenterY=%.3f "
          "boundsMaxY=%.3f matrixRefs=%016llX",
          transaction, phase ? phase : "unknown", index, renderer,
          path[0] ? path : "<unknown>", mesh, EiemManagedArrayLength(bones),
          overrideIndex == SIZE_MAX ? "no" : "yes", init,
          enabledRead ? (enabled ? "1" : "0") : "?",
          visibleRead ? (visible ? "1" : "0") : "?", bounds.read ? 1 : 0,
          bounds.read ? bounds.CenterY() : 0.0f,
          bounds.read ? bounds.maxY : 0.0f,
          (unsigned long long)EiemSkinTimingBoneMatrixHash(bones));
    }

    void **hgItems = hgRenderers
                         ? (void **)((char *)hgRenderers + IL2CPP_ARRAY_DATA)
                         : nullptr;
    const size_t hgLimit = (std::min)(hgCount, (size_t)256);
    for (size_t index = 0; hgItems && index < hgLimit; ++index) {
      void *hg = hgItems[index];
      if (!hg) continue;
      char path[768] = {};
      if (root) EiemBuildRelativeRendererPath(root, hg, path, sizeof(path));
      const bool target =
          path[0] && (strstr(path, "body_01") || strstr(path, "cloth_01") ||
                      strstr(path, "cloth_02"));
      if (!target) continue;
      const int init = EiemReadManagedBoolArrayValue(
          hgRendererStates, hgStateCount, index);
      char description[384] = {};
      TraceDescribeObject(hg, description, sizeof(description));
      Log("[BASEMODEL-CACHE-v1] tx=%ld phase=%s kind=hg index=%zu hg=%p "
          "path=%s init=%d description=%s",
          transaction, phase ? phase : "unknown", index, hg,
          path[0] ? path : "<unknown>", init,
          description[0] ? description : "<unknown>");
    }

    void **clothItems = boneCloths
                            ? (void **)((char *)boneCloths + IL2CPP_ARRAY_DATA)
                            : nullptr;
    const size_t clothLimit = (std::min)(boneClothCount, (size_t)128);
    for (size_t index = 0; clothItems && index < clothLimit; ++index) {
      void *cloth = clothItems[index];
      if (!cloth) continue;
      char path[768] = {};
      if (root) EiemBuildRelativeRendererPath(root, cloth, path, sizeof(path));
      Log("[BASEMODEL-CACHE-v1] tx=%ld phase=%s kind=boneCloth index=%zu "
          "cloth=%p path=%s",
          transaction, phase ? phase : "unknown", index, cloth,
          path[0] ? path : "<unknown>");
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    Log("[BASEMODEL-CACHE-v1] tx=%ld phase=%s part=%p read=exception",
        transaction, phase ? phase : "unknown", part);
  }
}

static bool EiemModelHasActiveOwner(const EiemModelInstanceState &state) {
  for (size_t index = 0; index < state.owners.size(); ++index)
    if (state.owners[index].active) return true;
  return false;
}

static void EiemStoreModelPhysicsIntents(
    void *model, std::vector<EiemPhysicsIntent> intents, const char *stage) {
  if (!model) return;
  const auto currentIntents = intents;
  const size_t current = intents.size();
  size_t previous = 0;
  bool found = false;
  bool active = false;
  AcquireSRWLockExclusive(&s_eiemModelInstanceLock);
  for (auto &state : s_eiemModelInstances) {
    if (state.model != model) continue;
    previous = state.physicsIntents.size();
    state.physicsIntents = std::move(intents);
    active = EiemModelHasActiveOwner(state);
    found = true;
    break;
  }
  ReleaseSRWLockExclusive(&s_eiemModelInstanceLock);
  if (found && (previous || current))
    Log("[PHYSICS-PLAN] model=%p previous=%zu current=%zu active=%d stage=%s",
        model, previous, current, active ? 1 : 0,
        stage ? stage : "unknown");
  if (found) {
    // A previous Physics replacement may have been waiting for Unity's
    // deferred Destroy. Collect it before comparing this new intent, then
    // reconcile exactly once for this model boundary.
    if (!s_eiemPhysicsLifecycleTransaction) EiemPhysicsRuntimeBoundary(stage);
    EiemReconcileModelPhysics(model, currentIntents, active, stage);
    if (!s_eiemPhysicsLifecycleTransaction) EiemPhysicsRuntimeBoundary(stage);
  }
}

static void EiemSetModelOwnerActive(EiemModelOwnerKind ownerKind, void *owner,
                                    bool active, const char *stage) {
  if (!owner) return;
  size_t changed = 0;
  size_t matched = 0;
  size_t planned = 0;
  struct PhysicsOwnerState {
    void *model = nullptr;
    std::vector<EiemPhysicsIntent> intents;
    bool active = false;
  };
  std::vector<PhysicsOwnerState> physicsStates;
  AcquireSRWLockExclusive(&s_eiemModelInstanceLock);
  for (auto &state : s_eiemModelInstances) {
    for (size_t index = 0; index < state.owners.size(); ++index) {
      auto &candidate = state.owners[index];
      if (candidate.kind != ownerKind || candidate.owner != owner) continue;
      ++matched;
      if (candidate.active != active) {
        candidate.active = active;
        ++changed;
        if (!state.physicsIntents.empty())
          physicsStates.push_back({state.model, state.physicsIntents, active});
      }
      planned += state.physicsIntents.size();
    }
  }
  ReleaseSRWLockExclusive(&s_eiemModelInstanceLock);
  EiemRegistrationTraceOwnerState(
      EiemModelOwnerKindName(ownerKind), owner, nullptr, active, stage,
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0));
  if (changed || planned)
    Log("[PHYSICS-PLAN] owner=%p active=%d models=%zu changed=%zu intents=%zu stage=%s",
        owner, active ? 1 : 0, matched, changed, planned,
        stage ? stage : "unknown");
  for (const auto &physics : physicsStates)
    EiemReconcileModelPhysics(physics.model, physics.intents, physics.active,
                              stage);
  if (!physicsStates.empty() && !s_eiemPhysicsLifecycleTransaction)
    EiemPhysicsRuntimeBoundary(stage);
}

static bool EiemSameRelativePath(const char *left, const char *right) {
  const bool leftEmpty = !left || !left[0];
  const bool rightEmpty = !right || !right[0];
  return leftEmpty || rightEmpty ? leftEmpty == rightEmpty
                                 : EiemModSameLogicalPath(left, right);
}

static bool EiemBuildRelativeRendererPath(void *rootTransform, void *renderer,
                                          char *out, size_t outSize) {
  if (!rootTransform || !renderer || !out || !outSize ||
      !g_component_get_transform || !g_transform_get_parent ||
      !g_object_get_name)
    return false;
  out[0] = '\0';
  char names[64][96] = {};
  size_t count = 0;
  void *transform = Invoke(g_component_get_transform, renderer);
  while (transform && transform != rootTransform && count < _countof(names)) {
    void *name = Invoke(g_object_get_name, transform);
    if (name) ReadStrUtf8(name, names[count], sizeof(names[count]));
    if (!names[count][0]) return false;
    ++count;
    transform = Invoke(g_transform_get_parent, transform);
  }
  if (transform != rootTransform) return false;
  size_t used = 0;
  for (size_t index = count; index > 0; --index) {
    const char *name = names[index - 1];
    const size_t length = strlen(name);
    if (used + (used ? 1 : 0) + length + 1 > outSize) return false;
    if (used) out[used++] = '/';
    memcpy(out + used, name, length);
    used += length;
    out[used] = '\0';
  }
  return true;
}

static bool EiemRenderRuleMatches(const EiemModRule &rule,
                                  const char *relativePath, void *mesh,
                                  const char *asset) {
  // A source Render must identify a node path, a Mesh sub-asset, or both.
  // Selector-free Render sections are valid only as partner declarations.
  if (!rule.path[0] && !rule.asset[0]) return false;
  if (rule.path[0] && !EiemSameRelativePath(rule.path, relativePath))
    return false;
  if (rule.asset[0] && (!asset || !EiemModEquals(rule.asset, asset)))
    return false;
  if (rule.matchVertices >= 0 || rule.matchIndices >= 0 ||
      rule.matchSubMeshes >= 0) {
    int32_t vertices = -1, indices = -1, subMeshes = -1;
    EiemReadLiveMeshShape(mesh, &vertices, &indices, &subMeshes);
    if (rule.matchVertices >= 0 && rule.matchVertices != vertices) return false;
    if (rule.matchIndices >= 0 && rule.matchIndices != indices) return false;
    if (rule.matchSubMeshes >= 0 && rule.matchSubMeshes != subMeshes)
      return false;
  }
  return true;
}

static bool EiemRegisterAndApplyModelInstance(
    EiemModelOwnerKind ownerKind, void *owner, void *model,
    const char *prefabPath, uint32_t instanceUid, const char *stage,
    bool applyResources) {
  if (!model) return false;
  EiemPerfScope perfScope(s_eiemPerfModelRegistration);
  auto modelRef = EiemUnityRef::Capture(model);
  if (!modelRef) {
    Log("[MOD-LIFECYCLE] Cannot observe model lifetime model=%p stage=%s", model, stage);
    return false;
  }

  std::vector<uintptr_t> releasedModels;
  uint32_t ownerCountSnapshot = 0;
  bool ownerActiveSnapshot = false;
  const LONG generationSnapshot =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  AcquireSRWLockExclusive(&s_eiemModelInstanceLock);
  // A PrefabInstantiateProxy or BaseModelViewPart owns one live result at a
  // time. UIModelLoader is intentionally different: one loader can own many
  // preview instances.
  if (owner && ownerKind != EiemModelOwnerKind::UIModelLoader) {
    for (size_t stateIndex = 0; stateIndex < s_eiemModelInstances.size();) {
      auto &entry = s_eiemModelInstances[stateIndex];
      if (entry.model == model) {
        ++stateIndex;
        continue;
      }
      for (size_t ownerIndex = 0; ownerIndex < entry.owners.size();
           ++ownerIndex) {
        if (entry.owners[ownerIndex].kind != ownerKind ||
            entry.owners[ownerIndex].owner != owner)
          continue;
        entry.owners.erase(entry.owners.begin() + ownerIndex);
        break;
      }
      if (entry.owners.empty()) {
        releasedModels.push_back((uintptr_t)entry.model);
        s_eiemModelInstances.erase(s_eiemModelInstances.begin() + stateIndex);
      } else {
        ++stateIndex;
      }
    }
  }
  size_t slot = SIZE_MAX;
  for (size_t index = 0; index < s_eiemModelInstances.size(); ++index) {
    if (s_eiemModelInstances[index].model == model) {
      if (s_eiemModelInstances[index].modelRef.Target() != model) {
        // Wrapper addresses can be reused; do not inherit stale owners.
        releasedModels.push_back((uintptr_t)model);
        s_eiemModelInstances.erase(s_eiemModelInstances.begin() + index);
        break;
      }
      slot = index;
      break;
    }
  }
  if (slot == SIZE_MAX) {
    EiemModelInstanceState state = {};
    state.model = model;
    state.modelRef = modelRef;
    s_eiemModelInstances.push_back(state);
    slot = s_eiemModelInstances.size() - 1;
  }
  auto &state = s_eiemModelInstances[slot];
  if (instanceUid) state.instanceUid = instanceUid;
  if (prefabPath && prefabPath[0])
    strncpy_s(state.path, sizeof(state.path), prefabPath, _TRUNCATE);
  if (owner) {
    bool knownOwner = false;
    for (size_t index = 0; index < state.owners.size(); ++index) {
      if (state.owners[index].kind == ownerKind &&
          state.owners[index].owner == owner) {
        state.owners[index].active = true;
        knownOwner = true;
        break;
      }
    }
    if (!knownOwner) state.owners.push_back({ownerKind, owner, true});
  }
  ownerCountSnapshot = (uint32_t)state.owners.size();
  ownerActiveSnapshot = EiemModelHasActiveOwner(state);
  ReleaseSRWLockExclusive(&s_eiemModelInstanceLock);
  for (uintptr_t released : releasedModels) {
    EiemReleaseModelPhysics((void *)released, "owner moved to another model");
    EiemRegistrationTraceRelease(
        EiemModelOwnerKindName(ownerKind), owner, (void *)released,
        "owner moved to another model", generationSnapshot);
    EiemForgetRenderOverrides(released);
  }
  // Existence is recorded before consulting the current program. An empty
  // INI must not make an already-created model undiscoverable at the next F10.
  if (modelRef.Status() != 1) return false;
  bool applied = false;
  std::vector<EiemPhysicsIntent> physicsIntents;
  if (applyResources && EiemHasStandaloneRenderRules())
    applied = EiemApplyStandaloneRenderRules(model, stage, nullptr, nullptr,
                                             &physicsIntents);
  EiemRegistrationTraceModel(
      EiemModelOwnerKindName(ownerKind), owner, model, stage,
      generationSnapshot, ownerCountSnapshot, ownerActiveSnapshot, applied,
      -1, -1, prefabPath);
  EiemStoreModelPhysicsIntents(model, std::move(physicsIntents), stage);
  return applied;
}

// BaseModelViewPart is the game's confirmed character-model completion owner.
// In particular, its handle path reuses an already loaded model without
// creating another PrefabInstantiateProxy. Read the exact model and logical
// path held by that part; never infer identity from a scene-wide Mesh scan.


// CharUIModelMono is attached directly to the UI presentation hierarchy. Its
// own GameObject is therefore a sufficient lifecycle root for Mesh-identity
// rules; no PFB name inference or scene-wide search is needed.


static bool EiemReapplyRegisteredModelInstance(void *model,
                                                const char *stage) {
  if (!model) return false;
  EiemModelInstanceState state = {};
  bool found = false;
  AcquireSRWLockShared(&s_eiemModelInstanceLock);
  for (const auto &entry : s_eiemModelInstances) {
    if (entry.model == model) {
      state = entry;
      found = true;
      break;
    }
  }
  ReleaseSRWLockShared(&s_eiemModelInstanceLock);
  if (!found || state.modelRef.Status() != 1) return false;
  std::vector<EiemPhysicsIntent> physicsIntents;
  bool applied = EiemApplyStandaloneRenderRules(model, stage, nullptr, nullptr,
                                                &physicsIntents);
  EiemStoreModelPhysicsIntents(model, std::move(physicsIntents), stage);
  return applied;
}

static void EiemForgetModelOwner(EiemModelOwnerKind ownerKind, void *owner,
                                 const char *stage) {
  if (!owner) return;
  std::vector<uintptr_t> releasedModels;
  struct PhysicsOwnerState {
    void *model = nullptr;
    std::vector<EiemPhysicsIntent> intents;
    bool active = false;
  };
  std::vector<PhysicsOwnerState> physicsStates;
  AcquireSRWLockExclusive(&s_eiemModelInstanceLock);
  for (size_t stateIndex = 0; stateIndex < s_eiemModelInstances.size();) {
    auto &state = s_eiemModelInstances[stateIndex];
    const bool wasActive = EiemModelHasActiveOwner(state);
    bool removed = false;
    for (size_t ownerIndex = 0; ownerIndex < state.owners.size();
         ++ownerIndex) {
      if (state.owners[ownerIndex].kind != ownerKind ||
          state.owners[ownerIndex].owner != owner)
        continue;
      state.owners.erase(state.owners.begin() + ownerIndex);
      removed = true;
      break;
    }
    if (removed && !state.owners.empty() && wasActive != EiemModelHasActiveOwner(state) &&
        !state.physicsIntents.empty())
      physicsStates.push_back(
          {state.model, state.physicsIntents, EiemModelHasActiveOwner(state)});
    if (state.owners.empty()) {
      releasedModels.push_back((uintptr_t)state.model);
      s_eiemModelInstances.erase(s_eiemModelInstances.begin() + stateIndex);
    } else {
      ++stateIndex;
    }
  }
  ReleaseSRWLockExclusive(&s_eiemModelInstanceLock);
  for (uintptr_t modelOwner : releasedModels) {
    EiemReleaseModelPhysics((void *)modelOwner, stage);
    EiemRegistrationTraceRelease(
        EiemModelOwnerKindName(ownerKind), owner, (void *)modelOwner, stage,
        InterlockedCompareExchange(&s_eiemModGeneration, 0, 0));
    EiemForgetRenderOverrides(modelOwner);
  }
  for (const auto &physics : physicsStates)
    EiemReconcileModelPhysics(physics.model, physics.intents, physics.active,
                              stage);
  if ((!releasedModels.empty() || !physicsStates.empty()) &&
      !s_eiemPhysicsLifecycleTransaction)
    EiemPhysicsRuntimeBoundary(stage);
}

static void EiemForgetModelInstance(void *model, const char *stage) {
  if (!model) return;
  bool removed = false;
  AcquireSRWLockExclusive(&s_eiemModelInstanceLock);
  for (size_t index = 0; index < s_eiemModelInstances.size(); ++index) {
    if (s_eiemModelInstances[index].model != model) continue;
    s_eiemModelInstances.erase(s_eiemModelInstances.begin() + index);
    removed = true;
    break;
  }
  ReleaseSRWLockExclusive(&s_eiemModelInstanceLock);
  if (!removed) return;
  const uintptr_t modelOwner = (uintptr_t)model;
  EiemReleaseModelPhysics(model, stage);
  EiemRegistrationTraceRelease(
      "model", nullptr, model, stage,
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0));
  EiemForgetRenderOverrides(modelOwner);
  Log("[MOD-LIFECYCLE] released model=%p stage=%s", model,
      stage ? stage : "unknown");
  if (!s_eiemPhysicsLifecycleTransaction) EiemPhysicsRuntimeBoundary(stage);
}

static void EiemPruneModelInstances() {
  std::vector<EiemModelInstanceState> observed;
  AcquireSRWLockShared(&s_eiemModelInstanceLock);
  observed = s_eiemModelInstances;
  ReleaseSRWLockShared(&s_eiemModelInstanceLock);
  for (const auto &state : observed)
    if (state.modelRef.Status() == 0)
      EiemForgetModelInstance(state.model, "observed model expired");
}

// Endfield owns source/replacement material arrays in
// EntityRenderHelperMaterialController.RendererInfo.  Scene transitions can
// commit those arrays after a prefab and its EIEM Render rule have completed.
// Mesh identity remains the rule key, so enforce only the material portion at
// that game-owned final commit boundary.  This is not a UI-specific rule and
// does not re-run mesh, skip, or partner actions.

// Consumers receive an immutable snapshot; locking and storage stay here.
static std::vector<EiemModelInstanceState> EiemSnapshotModelInstances() {
  AcquireSRWLockShared(&s_eiemModelInstanceLock);
  auto snapshot = s_eiemModelInstances;
  ReleaseSRWLockShared(&s_eiemModelInstanceLock);
  return snapshot;
}
