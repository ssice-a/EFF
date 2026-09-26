#pragma once

// One Unity-thread transaction for INI reload, key state and owner replay.
static void EiemRunModReconcile() {
  uint32_t requests = s_eiemModUpdates.Take();
  if (g_shutdownRequested) return;
  // A stale wake-up message must not replay every registered model. The
  // request bitmask is the transaction authority; an empty mask is a no-op.
  if (!requests) return;
  // This function is dispatched from the game's window procedure, which is
  // the safe Unity thread for creating generated Mesh/Material/Texture
  // objects. Startup hooks may run on the plugin worker thread instead.
  if (!s_eiemUnityThreadId) s_eiemUnityThreadId = GetCurrentThreadId();
  if (!g_gameObject_GetComponentsInChildren) {
    if (requests & (uint32_t)EiemModUpdate::Reload) {
      LoadEiemConfig();
      EiemReportCameraFade();
    }
    s_eiemModUpdates.Requeue(requests);
    if (g_gameHwnd)
      SetTimer(g_gameHwnd, kEiemModRetryTimer, 100, nullptr);
    Log("[MOD] Reconcile skipped: renderer APIs are not ready");
    return;
  }
  std::vector<EiemModInputEvent> inputs;
  AcquireSRWLockExclusive(&s_eiemInputLock);
  inputs.swap(s_eiemPendingInputs);
  ReleaseSRWLockExclusive(&s_eiemInputLock);
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  EiemRegistrationTraceReconcile("begin", requests, generation, inputs.size(),
                                 0, 0, 0);
  const bool reload = (requests & (uint32_t)EiemModUpdate::Reload) != 0;
  const LONG timingTransaction =
      reload && (kEiemEnableSkinDiagnostics || kEiemEnableSkinBindingDiagnostics)
          ? InterlockedIncrement(&s_eiemSkinTimingProbeSequence)
          : 0;
  const ULONGLONG reconcileStarted = GetTickCount64();
  if (reload)
    Log("[MOD-RELOAD-TRACE] begin tick=%llu requests=0x%X",
        (unsigned long long)reconcileStarted, requests);
  EiemModProgram reloadProgram;
  if (reload) {
    s_eiemPersistentStates.Flush(true);
    std::string failure;
    if (!EiemPrepareModReload(&reloadProgram, &failure)) {
      Log("[MOD-RELOAD-TRACE] rejected before mutation: %s",
          failure.empty() ? "unknown parse error" : failure.c_str());
      EiemRegistrationTraceReconcile(
          "reload-rejected", requests, generation, inputs.size(), 0, 0,
          GetTickCount64() - reconcileStarted);
      return;
    }
  }
  EiemModProgram next;
  std::vector<std::string> affectedMods;
  const std::vector<std::string> *affected = nullptr;
  bool submeshVisibilityOnly = false;
  std::vector<EiemSubmeshVisibilityChange> visibilityChanges;
  // A control request may arrive in the same batch as native set_bones.
  // Lightweight control updates must not consume the required model replay.
  const bool lifecycleReplayRequested =
      (requests & ((uint32_t)EiemModUpdate::Reconcile |
                   (uint32_t)EiemModUpdate::SkinRefresh)) != 0;
  if (!reload && !inputs.empty()) {
    bool shapesOnly = false;
    if (EiemPrepareInputUpdate(inputs, &next, &affectedMods, &shapesOnly,
                               &submeshVisibilityOnly,
                               &visibilityChanges)) {
      if (shapesOnly && !lifecycleReplayRequested) {
        EiemPublishModState(std::move(next));
        EiemReapplyShapeControls(affectedMods);
        EiemRefreshShapeTransitionTimer();
        EiemRegistrationTraceReconcile(
            "end", requests, generation, inputs.size(), 0, 0,
            GetTickCount64() - reconcileStarted);
        return;
      }
      if (submeshVisibilityOnly && !lifecycleReplayRequested) {
        // The source Renderer and its skinning already own the current Mesh.
        // Rebuild only the visibility variant; do not restore/destroy model
        // objects or replay Physics just because a key changed a submesh mask.
        EiemPublishModState(std::move(next));
        const uint32_t matched =
            EiemReapplySubmeshVisibility(visibilityChanges);
        Log("[MOD] Submesh visibility reconcile: rules=%zu renderers=%u",
            visibilityChanges.size(), matched);
        EiemRefreshShapeTransitionTimer();
        EiemRegistrationTraceReconcile(
            "end", requests, generation, inputs.size(),
            visibilityChanges.size(),
            matched, GetTickCount64() - reconcileStarted);
        return;
      }
      affected = &affectedMods;
    } else if (!lifecycleReplayRequested) {
      EiemRegistrationTraceReconcile(
          "end", requests, generation, inputs.size(), 0, 0,
          GetTickCount64() - reconcileStarted);
      return;
    }
  }
  // Native Physics teardown/collection belongs to this same Unity-thread
  // transaction. There is no background candidate scan. Lightweight shape
  // or submesh visibility keys have already returned above and must not enter
  // this boundary at all.
  EiemPhysicsRuntimeBoundary("mod reconcile begin");
  s_eiemPhysicsLifecycleTransaction = true;
  bool restoreComplete = true;
  EiemDispatchModUpdate(requests, [&] {
    const ULONGLONG phaseStarted = GetTickCount64();
    if (reload)
      Log("[MOD-RELOAD-TRACE] phase=restore begin");
    // F10 is a full generation rebuild: restore every owned field before
    // publishing and constructing the next generation. This keeps reload
    // semantics independent of cache/file timestamp behaviour.
    restoreComplete = EiemRestoreRenderOverrides(affected);
    EiemCollectSkeletonInstances();
    if (!restoreComplete)
      Log("[MOD] Update rejected: Renderer restore incomplete; retaining generation=%ld", generation);
    if (reload)
      Log("[MOD-RELOAD-TRACE] phase=restore end elapsed=%llums",
          GetTickCount64() - phaseStarted);
    return restoreComplete;
  }, [&] {
    const ULONGLONG phaseStarted = GetTickCount64();
    Log("[MOD-RELOAD-TRACE] phase=load begin");
    EiemPhysicsRuntimeRetireChangedAssets("mod reload changed Physics");
    Log("[MOD-RELOAD-TRACE] Physics remains owned by model generation; "
        "only changed intents retire during replay");
    LoadEiemConfig();
    EiemReportCameraFade();
    EiemPublishPreparedModReload(std::move(reloadProgram));
    Log("[MOD-RELOAD-TRACE] phase=load end elapsed=%llums",
        GetTickCount64() - phaseStarted);
  }, [&] {
    if (restoreComplete && affected) EiemPublishModState(std::move(next));
    const char *stage = !restoreComplete ? "restore rejected; replay old program" : reload ? "global reload" : affected ? "control state change" : "lifecycle reconcile";
   const ULONGLONG started = GetTickCount64();
   EiemPruneModelInstances();
  std::vector<uintptr_t> nativeSkinModels;
  if (requests & (uint32_t)EiemModUpdate::SkinRefresh) {
    AcquireSRWLockExclusive(&s_eiemNativeSkinRefreshLock);
    nativeSkinModels.swap(s_eiemNativeSkinRefreshModels);
    ReleaseSRWLockExclusive(&s_eiemNativeSkinRefreshLock);
  }
  const bool nativeSkinOnly =
      requests == (uint32_t)EiemModUpdate::SkinRefresh;
  const auto instances = EiemSnapshotModelInstances();
  if (reload)
    Log("[MOD-RELOAD-TRACE] phase=replay begin models=%zu", instances.size());
   EiemRenderReplayLedger replayLedger;
   EiemRenderReplayLedger *previousReplay = s_eiemActiveRenderReplay;
   s_eiemActiveRenderReplay = &replayLedger;
   uint32_t matched = 0;
   size_t replayIndex = 0;
   for (const auto &instance : instances) {
       if (!instance.model) continue;
       if (nativeSkinOnly &&
           std::find(nativeSkinModels.begin(), nativeSkinModels.end(),
                     (uintptr_t)instance.model) == nativeSkinModels.end())
         continue;
       if (reload && kEiemEnableLifecycleDiagnostics)
         Log("[MOD-RELOAD-TRACE] model=%zu/%zu begin ptr=%p path=%s",
             replayIndex + 1, instances.size(), instance.model,
             instance.path[0] ? instance.path : "<unknown>");
       if (instance.modelRef.Status() != 1) {
         Log("[MOD-LIFECYCLE] Cannot validate observed model=%p; not applying rules", instance.model);
         ++replayIndex;
         continue;
       }
       std::vector<EiemPhysicsIntent> physicsIntents;
       bool applied = false;
       // F10 is an instance replay, not a second call to the game's creation
       // helper on already registered UI/NPC/world objects. All three owner
       // adapters feed the same Render executor and resource identity rules.
       applied = EiemApplyStandaloneRenderRules(
           instance.model, stage, nullptr, affected, &physicsIntents);
       EiemStoreModelPhysicsIntents(instance.model, std::move(physicsIntents),
                                    stage);
       if (applied) ++matched;
       if (reload && kEiemEnableLifecycleDiagnostics)
         Log("[MOD-RELOAD-TRACE] model=%zu/%zu end ptr=%p matched=%d elapsed=%llums",
             replayIndex + 1, instances.size(), instance.model, applied ? 1 : 0,
             GetTickCount64() - reconcileStarted);
       ++replayIndex;
   }
  s_eiemActiveRenderReplay = previousReplay;
  const ULONGLONG elapsed = GetTickCount64() - started;
  Log("[MOD] Reconcile complete: registeredModels=%zu matchedModels=%u elapsed=%llums",
      instances.size(), matched, elapsed);
  EiemCollectSkeletonInstances();
  EiemRefreshShapeTransitionTimer();
  if (reload)
    Log("[MOD-RELOAD-TRACE] end tick=%llu elapsed=%llums",
        (unsigned long long)GetTickCount64(),
        GetTickCount64() - reconcileStarted);
  EiemRegistrationTraceReconcile(
      "end", requests,
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0), inputs.size(),
      instances.size(), matched, elapsed);
  EiemPhysicsRuntimeBoundary("mod reconcile end");
   });
   if (reload &&
       (kEiemEnableSkinDiagnostics || kEiemEnableSkinBindingDiagnostics)) {
     EiemLogSkinTimingProbe("after-replay", timingTransaction);
     EiemArmSkinTimingProbe(timingTransaction);
   }
   s_eiemPhysicsLifecycleTransaction = false;
}
