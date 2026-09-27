#pragma once

// Shared Mesh identity executor for world, UI and NPC model owners.
static bool EiemApplyResolvedRenderRule(void *renderer, void *drawRenderer,
                                        void *mesh,
                                        const char *rendererType,
                                        void *methodInfo,
                                        const EiemResolvedRenderRule &resolved,
                                        bool allowMeshReplacement = true) {
  (void)methodInfo;
  if (!renderer || !mesh) return false;
  if (!drawRenderer) drawRenderer = renderer;
  const EiemModRule &rule = resolved.rule;
  const char *source = resolved.source[0] ? resolved.source : "<unknown>";
  const char *asset = resolved.asset[0] ? resolved.asset : "<unknown>";
  const LONG64 rulePerfStarted = EiemPerfNow();
  double materialPrepareMs = 0.0;
  double meshBuildMs = 0.0;
  double boneResolveMs = 0.0;
  double meshCommitMs = 0.0;

  // `mesh` and `handling=skip` are independent directives. A rule with
  // neither directive is a match-only declaration and must pass through the
  // game's setter unchanged.
  const bool skipOriginal = EiemModEquals(rule.handling, "skip");
  const bool applyMesh = allowMeshReplacement && rule.hasMesh;
  if (!applyMesh && !(allowMeshReplacement && rule.hasSkeleton) &&
      !skipOriginal && !rule.materialCount && !rule.submeshCount &&
      !rule.shapeCount)
    return false;

  if (!EiemCaptureOriginal(renderer, drawRenderer, mesh, rendererType, applyMesh,
                           EiemMaterialSourceInitActive(drawRenderer) ? nullptr : &rule,
                           rule.shapeCount != 0)) return false;
  if (!EiemCaptureCommitEnabled(renderer, drawRenderer)) return false;

  std::shared_ptr<EiemSkeletonInstance> skeleton;
  bool skeletonReady=true;
  if (allowMeshReplacement && rule.hasSkeleton) {
    char skeletonError[256]={};
    skeletonReady=EiemModEquals(rendererType,"SkinnedMeshRenderer") &&
        EiemAcquireSkeleton(rule,renderer,skeleton,skeletonError,sizeof(skeletonError));
    if (skeletonReady) {
      AcquireSRWLockExclusive(&s_eiemOverrideLock);
      const auto index=EiemFindOverrideLocked(renderer);
      if (index!=SIZE_MAX) {
        auto &owned=s_eiemOverrides[index].skeleton;
        if (owned && owned!=skeleton) {
          // A later conflicting rule must not drop nodes still referenced by
          // the previously assigned Mesh. Restore its binding first (F10).
          skeletonReady=false;
          Log("[SKELETON] Conflicting Skeleton on renderer=%p; restore before rebinding",renderer);
        } else owned=skeleton;
      } else skeletonReady=false;
      ReleaseSRWLockExclusive(&s_eiemOverrideLock);
    } else Log("[SKELETON] %s / %s: %s",rule.modPath,rule.section,
        skeletonError[0]?skeletonError:"Skeleton requires a skinned source Renderer");
  }

  // `handling=skip` owns only the source Renderer state. It is intentionally
  // independent from every resource mount below.
  bool skipApplied = !skipOriginal;
  if (skipOriginal) {
    skipApplied = EiemCaptureEnabledForSkip(renderer, drawRenderer) &&
                  EiemSetRendererEnabled(drawRenderer, false);
    Log("[MOD] %s resource skip applied: source=%s asset=%s", rendererType,
        source, asset);
  }

  // Build every declared material before changing sharedMesh. A generated
  // multi-submesh Mesh paired with the source's shorter material array makes
  // the remaining submeshes disappear. Resource preparation is therefore a
  // prerequisite for the Mesh write, even though the two Unity assignments
  // remain separate calls.
  char error[256] = {};
  void *preparedMaterials = nullptr;
  const bool prepareMaterials =
      (rule.materialCount || rule.submeshCount) &&
      !EiemMaterialSourceInitActive(drawRenderer);
  bool resourcesReady = skipApplied && skeletonReady;
  const LONG64 materialPrepareStarted = EiemPerfNow();
  if (prepareMaterials &&
      !EiemBuildRendererMaterialsForSource(rule, drawRenderer,
                                           &preparedMaterials, error,
                                           sizeof(error))) {
    resourcesReady = false;
    Log("[MOD] %s resource preparation failed before mutation: source=%s "
        "asset=%s section=%s error=%s",
        rendererType, source, asset, rule.section,
        error[0] ? error : "unknown");
  }
  materialPrepareMs = EiemPerfMilliseconds(
      EiemPerfNow() - materialPrepareStarted);

  // `mesh=` replaces the source Renderer’s shared Mesh in place. It does not
  // create another Renderer or take ownership of game lifecycle state.
  bool meshApplied = false;
  bool rendererDisabledForCommit = false;
  bool rendererEnabledBeforeCommit = true;
  if (applyMesh && skeletonReady && resourcesReady) {
    void *beforeBones = nullptr;
    void *beforeRootBone = nullptr;
    size_t beforeBoneCount = 0;
    uint64_t beforeBoneRefs = 0;
    if (kEiemEnableSkinBindingDiagnostics &&
        EiemModEquals(rendererType, "SkinnedMeshRenderer")) {
      beforeBones = g_smr_get_bones ? Invoke(g_smr_get_bones, renderer) : nullptr;
      beforeRootBone = g_smr_get_rootBone ? Invoke(g_smr_get_rootBone, renderer) : nullptr;
      beforeBoneCount = EiemManagedArrayLength(beforeBones);
      beforeBoneRefs = EiemSkinTimelineBoneRefs(beforeBones);
    }
    EiemBounds sourceBounds = {};
    const bool hasSourceBounds =
        EiemModEquals(rendererType, "SkinnedMeshRenderer") &&
        EiemReadBounds(g_smr_get_localBounds, renderer, &sourceBounds);
    void *assignedMesh = nullptr;
    void *assignedBones = nullptr;
    std::shared_ptr<const EiemSkinIdentity> skin;
    EiemUnityRef assignedBonesRoot;
    EiemUnityRef assignedMeshRoot;
    char meshError[256] = {};
    bool skinPaletteReady = false;
    const LONG64 meshBuildStarted = EiemPerfNow();

    const bool meshBuilt = EiemBuildMeshResource(
        rule, &assignedMesh, meshError, sizeof(meshError), mesh, &skin);
    meshBuildMs = EiemPerfMilliseconds(EiemPerfNow() - meshBuildStarted);
    if (meshBuilt && assignedMesh) {
      skinPaletteReady = true;
      if (skin) {
        // A generated SkinnedMesh carries its own local bone palette.  Unity
        // requires that palette to agree with the Renderer.bones array at the
        // moment sharedMesh is assigned.  Mesh-only replacement still keeps
        // animation and bone ownership in the game: resolve every authored
        // path against this instance's existing Transform hierarchy and assign
        // only that managed Transform[]; no Transform or skeleton is created.
        if (!EiemModEquals(rendererType, "SkinnedMeshRenderer")) {
          skinPaletteReady = false;
          strncpy_s(meshError, sizeof(meshError),
                    "Skinned replacement requires SkinnedMeshRenderer", _TRUNCATE);
        } else {
          // Mesh-only replacements must use the same completed assembly
          // snapshot as the game's skin registration.  The older
          // NativeInstance resolver searched every live Renderer that shared
          // a skinningRoot; in one PFB that mixes body, cloth and shadow
          // branches and can select a valid Transform from the wrong
          // rootBone.  That produces a perfectly valid bones[] array while
          // feeding the renderer the wrong branch (the persistent T-pose /
          // ground-pose symptom).  Keep custom skeleton rules on their
          // explicit path, but never use the cross-Renderer fallback for the
          // ordinary mesh replacement path.
          const LONG64 boneResolveStarted = EiemPerfNow();
          skinPaletteReady = skeleton
              ? EiemSkeletonMeshBones(*skin, *skeleton, &assignedBones,
                                      meshError, sizeof(meshError))
              : EiemResolveMeshBonesFromAssembly(
                    *skin, renderer, &assignedBones, meshError,
                    sizeof(meshError));
          boneResolveMs = EiemPerfMilliseconds(
              EiemPerfNow() - boneResolveStarted);
        }
      }
    }
    const bool meshWillChange =
        assignedMesh && EiemReadSharedMesh(renderer, rendererType) != assignedMesh;
    const bool bonesWillChange =
        assignedBones && g_smr_get_bones &&
        !EiemManagedObjectArraySame(
            assignedBones, Invoke(g_smr_get_bones, renderer));
    if (assignedMesh && skinPaletteReady &&
        (assignedMeshRoot = EiemUnityRef::Capture(assignedMesh, false)) &&
        (!assignedBones || (assignedBonesRoot=EiemUnityRef::Capture(assignedBones,false))) &&
        EiemPrepareRendererShapeBinding(renderer,assignedMesh,rendererType,meshError,sizeof(meshError))) {
      // A SkinnedMeshRenderer observes sharedMesh and bones through separate
      // native setters. Keep it out of the render/skin submission path while
      // those fields are being replaced, otherwise Unity can consume the
      // intermediate source-Mesh/new-bones or new-Mesh/source-bones pair.
      bool quiesced = true;
      if ((meshWillChange || bonesWillChange) &&
          EiemModEquals(rendererType, "SkinnedMeshRenderer")) {
        quiesced = EiemReadRendererEnabled(drawRenderer, &rendererEnabledBeforeCommit) &&
                    (!rendererEnabledBeforeCommit || EiemSetRendererEnabled(drawRenderer, false));
        if (quiesced && rendererEnabledBeforeCommit) {
          rendererDisabledForCommit = true;
          if (kEiemEnableSkinDiagnostics)
            Log("[DEBUG-HR-ATOMIC-v1] disabled renderer=%p section=%s "
                "before mesh/bones commit",
                drawRenderer, rule.section);
        }
      }
      // A failed native setter can already have cleared the field. Own the
      // restoration BEFORE calling it, independently of skip and success.
      const LONG64 meshCommitStarted = EiemPerfNow();
      if (quiesced) {
        EiemBeginMeshWrite(renderer);
        // A failed native setter may still leave this Mesh attached. Retain
        // its wrapper before the call so a pending rollback cannot outlive it.
        const bool retained = EiemRememberReplacement(
            renderer, assignedMesh, rendererType);

        meshApplied = retained && (!meshWillChange ||
                      EiemSetSharedMesh(renderer, assignedMesh, rendererType,
                                        nullptr));
        if (meshApplied)
          meshApplied = EiemInitializeRendererShapeBinding(
              renderer, rendererType, meshError, sizeof(meshError));
        if (meshApplied && assignedBones)
          meshApplied = EiemPreserveSourceSkinning(
              renderer, assignedBones, meshError, sizeof(meshError));
      }
      meshCommitMs = EiemPerfMilliseconds(EiemPerfNow() - meshCommitStarted);
    }
    if (!meshApplied)
      Log("[MOD] %s resource mesh replacement failed: source=%s asset=%s mesh=%s error=%s",
          rendererType, source, asset, rule.mesh,
          meshError[0] ? meshError : "assignment failed");
    if (meshApplied) {

      if (kEiemEnableLifecycleDiagnostics)
      Log("[MOD] %s resource mesh replaced: source=%s asset=%s mesh=%s actual=%p",
          rendererType, source, asset, rule.mesh,
          EiemReadSharedMesh(renderer, rendererType));
      if (kEiemEnableSkinBindingDiagnostics &&
          EiemModEquals(rendererType, "SkinnedMeshRenderer")) {
        void *afterBones = g_smr_get_bones ? Invoke(g_smr_get_bones, renderer) : nullptr;
        void *afterRootBone = g_smr_get_rootBone ? Invoke(g_smr_get_rootBone, renderer) : nullptr;
        const size_t afterBoneCount = EiemManagedArrayLength(afterBones);
        const uint64_t afterBoneRefs = EiemSkinTimelineBoneRefs(afterBones);
        const uint64_t replacementRefs = EiemSkinTimelineBoneRefs(assignedBones);
        Log("[POSE-BIND-v1] owner=%p renderer=%p draw=%p section=%s "
            "sourceMesh=%p replacementMesh=%p beforeBones=%p beforeCount=%zu "
            "beforeRefs=%016llX assignedBones=%p assignedCount=%zu "
            "assignedRefs=%016llX afterBones=%p afterCount=%zu "
            "afterRefs=%016llX beforeRoot=%p afterRoot=%p",
            (void *)s_eiemActivePrefabInstance, renderer, drawRenderer,
            rule.section, mesh, assignedMesh, beforeBones, beforeBoneCount,
            (unsigned long long)beforeBoneRefs, assignedBones,
            EiemManagedArrayLength(assignedBones),
            (unsigned long long)replacementRefs, afterBones, afterBoneCount,
            (unsigned long long)afterBoneRefs, beforeRootBone, afterRootBone);

      }
      if (EiemModEquals(rendererType, "SkinnedMeshRenderer")) {
        InterlockedExchange64(&s_eiemLastSkinCommitTick,
                              (LONG64)GetTickCount64());
      }
      EiemSetReplacementDrawBounds(renderer, rendererType, assignedMesh,
                                   hasSourceBounds ? &sourceBounds : nullptr);
    }
  }

  // A mesh rule is a transaction boundary. Material/submesh and shape edits
  // describe the replacement Mesh's slot layout; committing them to the
  // source Renderer after mesh construction failed would corrupt the source
  // asset and leave a rule that can never be restored consistently. Rules
  // without mesh= remain ordinary source material edits.
  bool resourceCommitted = resourcesReady && (!applyMesh || meshApplied);
  if (!resourceCommitted)
    Log("[MOD] %s resource rule not bound after mesh failure: source=%s "
        "asset=%s mesh=%s",
        rendererType, source, asset, rule.mesh);
  if (prepareMaterials && resourceCommitted && preparedMaterials) {
      const bool captured = EiemCaptureOriginal(
          renderer, drawRenderer, mesh, rendererType, false, &rule);
      const bool assigned = captured && EiemAssignRendererMaterials(
          drawRenderer, preparedMaterials, error, sizeof(error));
      if (!assigned) {
        resourceCommitted = false;
        Log("[MOD] %s material assignment failed: source=%s asset=%s section=%s error=%s",
            rendererType, source, asset, rule.section,
            error[0] ? error : (captured ? "assignment failed" : "capture failed"));
      } else {
        if (kEiemEnableLifecycleDiagnostics)
        Log("[MOD-MATERIAL] applied renderer=%p source=%s asset=%s section=%s slots=%u array=%p stage=resource-rule",
            drawRenderer, source, asset, rule.section, rule.materialCount,
            preparedMaterials);
      }
  } else if (prepareMaterials && !resourceCommitted) {
    Log("[MOD] %s resource dependent edits skipped after mesh failure: "
        "source=%s asset=%s mesh=%s materials=%u submeshes=%u",
        rendererType, source, asset, rule.mesh, rule.materialCount,
        rule.submeshCount);
  }
  if (resourceCommitted) {
    AcquireSRWLockExclusive(&s_eiemOverrideLock);
    const size_t index = EiemFindOverrideLocked(renderer);
    resourceCommitted = index != SIZE_MAX && EiemUpdateRendererShapes(renderer, rendererType, rule, s_eiemOverrides[index].shapes);
    ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  }
  if (resourceCommitted && rendererDisabledForCommit) {
    const bool restored = EiemSetRendererEnabled(drawRenderer,
                                                 rendererEnabledBeforeCommit);
    resourceCommitted = restored;
    if (kEiemEnableSkinDiagnostics) {
      bool actual = false;
      const bool readBack = EiemReadRendererEnabled(drawRenderer, &actual);
      Log("[DEBUG-HR-ATOMIC-v1] restored renderer=%p section=%s setter=%d "
          "readBack=%d enabled=%d",
          drawRenderer, rule.section, restored ? 1 : 0, readBack ? 1 : 0,
          actual ? 1 : 0);
    }
  }
  if (!resourceCommitted) {
    const bool restored = EiemRestoreRenderOverrides(nullptr, renderer);
    Log("[MOD] Render commit rejected renderer=%p section=%s rollback=%s",
        renderer, rule.section, restored ? "complete" : "pending");
    return false;
  }
  EiemFinishRenderCommit(renderer);
  EiemRememberRuleBinding(renderer, rule);
  if (kEiemEnableSkinDiagnostics) {
    bool currentEnabled = true;
    const bool readEnabled = EiemReadRendererEnabled(drawRenderer,
                                                     &currentEnabled);
    bool currentVisible = false;
    const bool readVisible = EiemReadRendererVisible(drawRenderer,
                                                     &currentVisible);
    const int32_t materialCount = EiemReadRendererMaterialCount(drawRenderer);
    char rendererDescription[512] = {};
    TraceDescribeObject(drawRenderer, rendererDescription,
                        sizeof(rendererDescription));
    Log("[MOD] %s resource rule applied: source=%s asset=%s mesh=%s materials=%u skip=%s enabled=%s",
        rendererType, source, asset,
        applyMesh ? rule.mesh : "<none>", rule.materialCount,
        skipOriginal ? "true" : "false",
        readEnabled ? (currentEnabled ? "true" : "false") : "unknown");
    if (applyMesh) {
      Log("[DEBUG-DRAW-STATE] renderer=%s mesh=%p materials=%d enabled=%s visible=%s",
          rendererDescription[0] ? rendererDescription : "<unknown>",
          EiemReadSharedMesh(renderer, rendererType), materialCount,
          readEnabled ? (currentEnabled ? "true" : "false") : "unknown",
          readVisible ? (currentVisible ? "true" : "false") : "unknown");
    }
  }
  const double ruleElapsedMs =
      EiemPerfMilliseconds(EiemPerfNow() - rulePerfStarted);
  if (ruleElapsedMs >= 20.0) {
    Log("[PERF-SLOW-RULE-v1] owner=%p renderer=%p section=%s type=%s "
        "materialPrepareMs=%.2f meshBuildMs=%.2f boneResolveMs=%.2f "
        "meshCommitMs=%.2f totalMs=%.2f",
        (void *)s_eiemActivePrefabInstance, renderer, rule.section,
        rendererType ? rendererType : "<unknown>", materialPrepareMs,
        meshBuildMs, boneResolveMs, meshCommitMs, ruleElapsedMs);
  }
  return resourceCommitted;
}

static void *EiemFindMeshFilterDrawRenderer(void *meshFilter) {
  if (!meshFilter || !g_rendererClass || !g_component_get_gameObject ||
      !g_gameObject_GetComponent || !il2cpp_class_get_type ||
      !il2cpp_type_get_object)
    return nullptr;
  __try {
    void *gameObject = Invoke(g_component_get_gameObject, meshFilter);
    void *type = il2cpp_class_get_type(g_rendererClass);
    void *typeObject = type ? il2cpp_type_get_object(type) : nullptr;
    if (!gameObject || !typeObject) return nullptr;
    void *params[] = {typeObject};
    return Invoke(g_gameObject_GetComponent, gameObject, params);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

struct EiemLiveSkinCaptureContext {
  void *model = nullptr;
  const char *stage = nullptr;
  const std::vector<void *> *renderers = nullptr;
  std::vector<EiemLiveSkinSource> *output = nullptr;
  bool captured = false;
};

// Bone donor capture is needed only for a matched mesh/skeleton rule.  The
// previous implementation captured every SkinnedMeshRenderer in every model
// before checking whether any rule matched, so a global F10 paid that cost for
// hundreds of unrelated models. Keep the same model-local snapshot, but make
// it lazy and invoke it immediately before the first replacement commit.
static bool EiemEnsureLiveSkinSources(void *opaque) {
  auto *context = static_cast<EiemLiveSkinCaptureContext *>(opaque);
  if (!context || context->captured) return context != nullptr;
  context->captured = true;
  if (!context->output || !context->renderers || !g_smr_get_bones) return true;
  context->output->reserve(context->renderers->size());
  for (void *renderer : *context->renderers) {
    if (!renderer) continue;
    void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
    if (!mesh) continue;
    void *identityMesh = mesh;
    EiemPrepareRenderInput(renderer, mesh, "SkinnedMeshRenderer",
                           &identityMesh);
    char source[768] = {};
    char asset[192] = {};
    if (!identityMesh ||
        !EiemReadLiveMeshIdentity(identityMesh, source, sizeof(source),
                                  asset, sizeof(asset))) {
      if (kEiemEnableSkinBindingDiagnostics &&
          InterlockedIncrement(&s_eiemSkinCaptureProbeCount) <= 96)
        Log("[MOD-SKIN-CAPTURE-v1] model=%p renderer=%p mesh=%p "
            "identity=unavailable stage=%s",
            context->model, renderer, identityMesh,
            context->stage ? context->stage : "unknown");
      continue;
    }
    void *bones = EiemBackendInvokeNoThrow(g_smr_get_bones, renderer);
    AcquireSRWLockShared(&s_eiemOverrideLock);
    const size_t overrideIndex = EiemFindOverrideLocked(renderer);
    if (overrideIndex != SIZE_MAX &&
        s_eiemOverrides[overrideIndex].originalBonesHandle &&
        il2cpp_gchandle_get_target) {
      void *original = il2cpp_gchandle_get_target(
          s_eiemOverrides[overrideIndex].originalBonesHandle);
      if (original) bones = original;
    }
    ReleaseSRWLockShared(&s_eiemOverrideLock);
    const size_t boneCount = EiemManagedArrayLength(bones);
    if (!bones || !boneCount) {
      if (kEiemEnableSkinBindingDiagnostics &&
          InterlockedIncrement(&s_eiemSkinCaptureProbeCount) <= 96) {
        void *rootBone = g_smr_get_rootBone
                             ? EiemBackendInvokeNoThrow(
                                   g_smr_get_rootBone, renderer)
                             : nullptr;
        void *skinningRoot = g_smr_get_skinningRoot
                                 ? EiemBackendInvokeNoThrow(
                                       g_smr_get_skinningRoot, renderer)
                                 : nullptr;
        Log("[MOD-SKIN-CAPTURE-v1] model=%p renderer=%p mesh=%p asset=%s "
            "identity=ok bones=%p count=%zu rootBone=%p skinningRoot=%p "
            "stage=%s",
            context->model, renderer, identityMesh, asset, bones, boneCount,
            rootBone, skinningRoot,
            context->stage ? context->stage : "unknown");
      }
      continue;
    }
    if (kEiemEnableSkinBindingDiagnostics &&
        InterlockedIncrement(&s_eiemSkinCaptureProbeCount) <= 96) {
      void *rootBone = g_smr_get_rootBone
                           ? EiemBackendInvokeNoThrow(g_smr_get_rootBone,
                                                      renderer)
                           : nullptr;
      void *skinningRoot = g_smr_get_skinningRoot
                               ? EiemBackendInvokeNoThrow(
                                     g_smr_get_skinningRoot, renderer)
                               : nullptr;
      Log("[MOD-SKIN-CAPTURE-v1] model=%p renderer=%p mesh=%p asset=%s "
          "identity=ok bones=%p count=%zu rootBone=%p skinningRoot=%p "
          "stage=%s",
          context->model, renderer, identityMesh, asset, bones, boneCount,
          rootBone, skinningRoot,
          context->stage ? context->stage : "unknown");
    }
    context->output->push_back({source, asset, renderer, bones});
  }
  return true;
}

using EiemEnsureLiveSkinSourcesFn = bool (*)(void *);

static bool EiemApplyRenderRuleSetToRenderer(
    void *rootTransform, void *meshOwner, void *drawRenderer, void *mesh,
    const char *rendererType, void *methodInfo,
    const std::vector<EiemModRule> &rules, const char *sourceLabel,
    bool *referenced = nullptr, bool *matched = nullptr,
    const std::vector<std::string> *affected = nullptr,
    std::vector<EiemPhysicsIntent> *physicsIntents = nullptr,
    bool includeGameHidden = false,
    EiemEnsureLiveSkinSourcesFn ensureLiveSkinSources = nullptr,
    void *ensureLiveSkinSourcesContext = nullptr) {
  if (!meshOwner || !drawRenderer || !mesh || !rendererType) return false;
  if (!EiemRendererEligibleForRule(meshOwner, drawRenderer,
                                   includeGameHidden))
    return false;
  void *identityMesh = mesh;
  EiemPrepareRenderInput(meshOwner, mesh, rendererType, &identityMesh);
  if (!identityMesh) return false;

  char source[768] = {};
  char asset[192] = {};
  if (!EiemReadLiveMeshIdentity(identityMesh, source, sizeof(source), asset,
                                sizeof(asset)))
    return false;

  char relativePath[768] = {};
  bool relativePathAttempted = false;
  bool relativePathResolved = false;
  for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex) {
    const EiemModRule &rule = rules[ruleIndex];
    if (rule.path[0]) {
      if (!relativePathAttempted) {
        relativePathAttempted = true;
        relativePathResolved =
            rootTransform && EiemBuildRelativeRendererPath(
                                 rootTransform, drawRenderer, relativePath,
                                 sizeof(relativePath));
      }
      if (!relativePathResolved) continue;
    }
    if (!EiemRenderRuleMatches(rule, relativePath, identityMesh, asset))
      continue;
    if (referenced) referenced[ruleIndex] = true;
    if (matched) *matched = true;
    if (rule.hasPhysics && physicsIntents &&
        !EiemCollectPhysicsIntent(rule, physicsIntents, drawRenderer))
      Log("[PHYSICS] Cannot resolve matched intent: mod=%s render=%s resource=%s",
          rule.modPath, rule.section, rule.physics);
    // Preserve first-match precedence even during a mod-scoped key update.
    // Filtering the rule list before matching would promote a lower-priority mod.
    if (!EiemModAffected(rule.modPath, affected)) return false;
    if ((rule.hasMesh || rule.hasSkeleton) && ensureLiveSkinSources &&
        !ensureLiveSkinSources(ensureLiveSkinSourcesContext))
      return false;
    EiemResolvedRenderRule resolved = {};
    resolved.rule = rule;
    strncpy_s(resolved.source, sizeof(resolved.source),
              source[0] ? source : (sourceLabel ? sourceLabel : "<mesh>"),
              _TRUNCATE);
    strncpy_s(resolved.asset, sizeof(resolved.asset), asset, _TRUNCATE);
    return EiemApplyResolvedRenderRule(meshOwner, drawRenderer, identityMesh,
                                       rendererType, methodInfo, resolved,
                                       true);
  }
  return false;
}

static bool EiemApplyRenderRuleSet(void *model,
                                   const std::vector<EiemModRule> &rules,
                                   const char *sourceLabel,
                                   const char *stage, bool *referenced = nullptr,
                                   bool *matched = nullptr,
                                   const std::vector<std::string> *affected = nullptr,
                                   std::vector<EiemPhysicsIntent> *physicsIntents = nullptr) {
  if (!model || rules.empty() || !EiemOnUnityThread() ||
      !g_gameObject_GetComponentsInChildren || !il2cpp_class_get_type ||
      !il2cpp_type_get_object)
    return false;
  EiemPerfScope perfScope(s_eiemPerfRuleApplication);
  const LONG64 slowApplyStarted = EiemPerfNow();

  void *root = g_gameObject_get_transform
                   ? Invoke(g_gameObject_get_transform, model)
                   : nullptr;
  uint32_t applied = 0;
  size_t visited = 0;
  const uintptr_t previousOwner = s_eiemActivePrefabInstance;
  const auto *previousLiveSkinSources = s_eiemLiveSkinSources;
  std::vector<EiemLiveSkinSource> liveSkinSources;
  std::vector<void *> skinnedRenderers;
  std::vector<void *> meshFilters;
  s_eiemActivePrefabInstance = (uintptr_t)model;

  auto snapshotType = [&](void *componentClass,
                          std::vector<void *> *components) {
    if (!componentClass || !components) return false;
    void *type = il2cpp_class_get_type(componentClass);
    void *typeObject = type ? il2cpp_type_get_object(type) : nullptr;
    if (!typeObject) return false;
    bool includeInactive = true;
    void *params[] = {typeObject, &includeInactive};
    void *array = Invoke(g_gameObject_GetComponentsInChildren, model, params);
    const size_t count = EiemManagedArrayLength(array);
    if (!array || count > 8192) return false;
    void **items = (void **)((char *)array + IL2CPP_ARRAY_DATA);
    components->assign(items, items + count);
    return true;
  };

  // Snapshot each component class once. The source-palette capture and the
  // rule executor consume the same concrete Renderer pointers, so one model
  // transaction cannot observe two different hierarchy states and does not
  // pay for a second GetComponentsInChildren<SkinnedMeshRenderer> traversal.
  const LONG64 snapshotStarted = EiemPerfNow();
  snapshotType(g_skinnedMeshRendererClass, &skinnedRenderers);
  snapshotType(g_meshFilterClass, &meshFilters);
  EiemPerfRecord(s_eiemPerfComponentSnapshot, snapshotStarted);

  EiemLiveSkinCaptureContext liveSkinCapture = {
      model, stage, &skinnedRenderers, &liveSkinSources, false};
  s_eiemLiveSkinSources = &liveSkinSources;

  auto visitType = [&](const std::vector<void *> &components,
                       const char *rendererType) {
    // Custom LOD controllers keep authored Renderer GameObjects inactive until
    // the level is selected.  Replace every authored level after the model's
    // assembly boundary; the game remains responsible only for activating the
    // selected level.  Waiting for an inactive level to become visible misses
    // controllers that toggle GameObjects without calling a Mesh setter.
    visited += components.size();
    for (void *meshOwner : components) {
      if (!meshOwner) continue;
      EiemClaimRendererOverrideOwner(meshOwner, (uintptr_t)model);
      if (s_eiemActiveRenderReplay &&
          s_eiemActiveRenderReplay->Committed(meshOwner))
        continue;
      void *drawRenderer = EiemModEquals(rendererType, "SkinnedMeshRenderer")
                               ? meshOwner
                               : EiemFindMeshFilterDrawRenderer(meshOwner);
      if (!drawRenderer) continue;
      void *mesh = EiemReadSharedMesh(meshOwner, rendererType);
      if (mesh && EiemApplyRenderRuleSetToRenderer(
                      root, meshOwner, drawRenderer, mesh, rendererType,
                      nullptr, rules, sourceLabel, referenced, matched, affected,
                      physicsIntents, true, EiemEnsureLiveSkinSources,
                      &liveSkinCapture)) {
        if (s_eiemActiveRenderReplay)
          s_eiemActiveRenderReplay->Commit(meshOwner);
        ++applied;
      }
    }
  };

  const LONG64 visitStarted = EiemPerfNow();
  visitType(skinnedRenderers, "SkinnedMeshRenderer");
  visitType(meshFilters, "MeshFilter");
  EiemPerfRecord(s_eiemPerfRendererVisit, visitStarted);
  s_eiemLiveSkinSources = previousLiveSkinSources;
  s_eiemActivePrefabInstance = previousOwner;
  const double slowApplyMs =
      EiemPerfMilliseconds(EiemPerfNow() - slowApplyStarted);
  if (slowApplyMs >= 100.0) {
    Log("[PERF-SLOW-APPLY-v1] model=%p stage=%s source=%s rules=%zu "
        "skinned=%zu meshFilters=%zu visited=%zu applied=%u elapsedMs=%.2f",
        model, stage ? stage : "unknown",
        sourceLabel ? sourceLabel : "<none>", rules.size(),
        skinnedRenderers.size(), meshFilters.size(), visited, applied,
        slowApplyMs);
  }
  if (applied)
    Log("[MOD-MESH] applied model=%p components=%zu actions=%u stage=%s",
        model, visited, applied, stage ? stage : "unknown");
  // Cold-start evidence is collected only after one model transaction has
  // produced both the stable body reference and the intermittent clothing
  // target.  This is observation-only; the normal commit path is unchanged.
  if (applied) EiemMaybeArmColdSkinTimingProbe();
  return applied != 0;
}

static bool EiemApplyStandaloneRenderRules(void *model, const char *stage,
                                           bool *matched, const std::vector<std::string> *affected,
                                           std::vector<EiemPhysicsIntent> *physicsIntents) {
  std::vector<EiemModRule> rules;
  EiemFindStandaloneRenderRules(&rules);
  const bool applied = EiemApplyRenderRuleSet(
      model, rules, "<mesh identity>", stage, nullptr, matched, affected,
      physicsIntents);
  if (kEiemEnableNativePhysicsObservation && applied) {
    EiemPhysicsOrderProbeRememberRender(model);
    const uint64_t now = GetTickCount64();
    const uint64_t completionTick = EiemPhysicsOrderLastCompletionTick();
    const int64_t delta = completionTick
                              ? (int64_t)now - (int64_t)completionTick
                              : 0;
    Log("[PHYSICS-ORDER-PROBE] render-commit model=%p stage=%s renderTick=%llu "
        "completionSeq=%llu completionTick=%llu deltaAfterCompletionMs=%s%lld "
        "completionObject=%p completionThread=%lu association=global-candidate",
        model, stage ? stage : "unknown", (unsigned long long)now,
        (unsigned long long)EiemPhysicsOrderCompletionSequence(),
        (unsigned long long)completionTick,
        completionTick ? "" : "unknown/",
        completionTick ? (long long)delta : 0LL,
        (void *)EiemPhysicsOrderLastCompletionObject(),
        (unsigned long)EiemPhysicsOrderLastCompletionThread());
  }
  return applied;
}
