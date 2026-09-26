#pragma once

// World, prefab and UI lifecycle adapters; no separate replacement implementation.
static void TracePrefabInstantiateCompleted(void *self, void *methodInfo) {
  // Capture identity before the game completion method is allowed to release
  // or recycle its asset handle. The instantiated GameObject is read after
  // completion, when the hierarchy is ready for Render actions.
  char path[768] = {};
  if (s_prefabInstantiateGetLogName)
    TraceDescribeString(Invoke(s_prefabInstantiateGetLogName, self), path,
                        sizeof(path));
  uint32_t instanceUid = 0;
  if (s_prefabInstantiateGetInstanceUid) {
    void *boxed = Invoke(s_prefabInstantiateGetInstanceUid, self);
    if (boxed) instanceUid = (uint32_t)EiemTraceUnboxInt(boxed);
  }
  auto original = (TracePrefabInstantiateCompletedFn)
      s_origPrefabInstantiateCompleted;
  if (original) original(self, methodInfo);
  const LONG64 perfStarted = EiemPerfNow();
  void *model = s_prefabInstantiateGetGameObject
                    ? Invoke(s_prefabInstantiateGetGameObject, self)
                    : nullptr;
  std::vector<EiemModPrefab> prefabs;
  EiemFindModPrefabs(path, &prefabs);
  const bool configured = !prefabs.empty();
  // Character prefabs only. Every presentation path instantiates its own Prefab
  // for the same character, so the prefab path is what lists them, and the
  // renderers in the freshly instantiated hierarchy are what that prefab
  // declares. Reporting both here answers two things at once: which prefabs a
  // character has, and whether a path's renderers carry the Mesh references
  // before any mod code runs.
  TraceDumpPrefabRenderers(path, model);
  const bool applied =
      EiemRegisterAndApplyModelInstance(
          EiemModelOwnerKind::PrefabProxy, self, model, path, instanceUid,
          "PrefabInstantiateProxy.OnCompleted", false);
  if (configured)
    Log("[MOD-PREFAB] completed proxy=%p uid=%u path=%s model=%p applied=%d",
        self, instanceUid, path, model, applied ? 1 : 0);
  const LONG64 perfCalls =
      EiemPerfRecord(s_eiemPerfPrefabCompletion, perfStarted);
  if ((perfCalls & 255) == 0) EiemLogPerformanceSummary(perfCalls);
}

static void TracePrefabInstantiateUnload(void *self, void *methodInfo) {
  EiemForgetModelOwner(EiemModelOwnerKind::PrefabProxy, self,
                       "PrefabInstantiateProxy.Unload");
  auto original = (TracePrefabInstantiateLifecycleFn)s_origPrefabInstantiateUnload;
  if (original) original(self, methodInfo);
}

static void TracePrefabInstantiateClear(void *self, void *methodInfo) {
  EiemForgetModelOwner(EiemModelOwnerKind::PrefabProxy, self,
                       "PrefabInstantiateProxy.Clear");
  auto original = (TracePrefabInstantiateLifecycleFn)s_origPrefabInstantiateClear;
  if (original) original(self, methodInfo);
}

static void TracePrefabInstantiateDispose(void *self, void *methodInfo) {
  EiemForgetModelOwner(EiemModelOwnerKind::PrefabProxy, self,
                       "PrefabInstantiateProxy.Dispose");
  auto original = (TracePrefabInstantiateLifecycleFn)s_origPrefabInstantiateDispose;
  if (original) original(self, methodInfo);
}

static void *TraceUIModelLoaderLoadModel(void *self, void *path,
                                         void *parent, void *methodInfo) {
  char pathText[768] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  auto original =
      (TraceUIModelLoaderLoadModelFn)s_origUIModelLoaderLoadModel;
  void *model = original ? original(self, path, parent, methodInfo) : nullptr;
  const bool applied = EiemRegisterAndApplyModelInstance(
      EiemModelOwnerKind::UIModelLoader, self, model, pathText, 0,
      // Loading the PFB is not the bone-assembly completion boundary. The
      // common EntityRenderHelper/CharUIModel completion hook applies after
      // the game's AssignSkin work has populated every LOD palette.
      "UIModelLoader.LoadModel", false);
  if (pathText[0]) {
    std::vector<EiemModPrefab> prefabs;
    EiemFindModPrefabs(pathText, &prefabs);
    if (!prefabs.empty())
      Log("[MOD-UI] sync completed loader=%p path=%s model=%p applied=%d",
          self, pathText, model, applied ? 1 : 0);
  }
  return model;
}

static int32_t TraceUIModelLoaderLoadModelAsync(
    void *self, void *path, void *parent, void *callback, void *methodInfo) {
  // Preserve the game's managed delegate, including its metadata and lifetime.
  // The removed native-address Action wrapper crashed at the game's invoke_impl
  // call before our completion ran (v29, GameAssembly+0x440d5d8). Completion is
  // observed through PrefabInstantiateProxy/CharUIModelMono instead; request IDs
  // are not GameObjects and are never submitted to the renderer executor.
  auto original =
      (TraceUIModelLoaderLoadModelAsyncFn)s_origUIModelLoaderLoadModelAsync;
  const int32_t requestId =
      original ? original(self, path, parent, callback, methodInfo) : -1;
  char pathText[768] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  Log("[MOD-UI] async request: loader=%p path=%s request=%d callback=%p completion=game-owned",
      self, pathText, requestId, callback);
  return requestId;
}

static void TraceUIModelLoaderUnloadModel(void *self, void *model,
                                          void *methodInfo) {
  EiemForgetModelInstance(model, "UIModelLoader.UnloadModel");
  auto original =
      (TraceUIModelLoaderUnloadModelFn)s_origUIModelLoaderUnloadModel;
  if (original) original(self, model, methodInfo);
}

static void TraceUIModelLoaderClear(void *self, void *methodInfo) {
  EiemForgetModelOwner(EiemModelOwnerKind::UIModelLoader, self,
                       "UIModelLoader._Clear");
  auto original = (TraceUIModelLoaderLifecycleFn)s_origUIModelLoaderClear;
  if (original) original(self, methodInfo);
}

static void TraceUIModelLoaderDispose(void *self, void *methodInfo) {
  EiemForgetModelOwner(EiemModelOwnerKind::UIModelLoader, self,
                       "UIModelLoader.Dispose");
  auto original = (TraceUIModelLoaderLifecycleFn)s_origUIModelLoaderDispose;
  if (original) original(self, methodInfo);
}

static void TraceCharUIModelOnAwake(void *self, void *methodInfo) {
  auto original =
      (TraceCharUIModelLifecycleFn)s_origCharUIModelOnAwake;
  if (original) original(self, methodInfo);
  EiemRegisterCharUIModelInstance(self, "CharUIModelMono.OnAwake", false);
}

static void TraceCharUIModelSetVisible(void *self, bool visible,
                                       void *methodInfo) {
  auto original =
      (TraceCharUIModelSetVisibleFn)s_origCharUIModelSetVisible;
  if (original) original(self, visible, methodInfo);
  if (visible)
    EiemRegisterCharUIModelInstance(self, "CharUIModelMono.SetVisible", true);
  else
    EiemSetModelOwnerActive(EiemModelOwnerKind::CharUIModel, self, false,
                            "CharUIModelMono.SetVisible(false)");
}

static void TraceCharUIModelOnRelease(void *self, void *methodInfo) {
  EiemForgetModelOwner(EiemModelOwnerKind::CharUIModel, self,
                       "CharUIModelMono.OnRelease");
  auto original =
      (TraceCharUIModelLifecycleFn)s_origCharUIModelOnRelease;
  if (original) original(self, methodInfo);
}

// Allocation alone does not prove that a skin palette is complete. The PFB,
// UI and NPC owner adapters register model lifetimes at their own boundaries.
static void TraceModelManagerGameObjectAllocate(void *self, void *model,
                                                 void *methodInfo) {
  auto original = (TraceModelManagerGameObjectFn)
      s_origModelManagerGameObjectAllocate;
  if (original) original(self, model, methodInfo);
}

// Persistent-pool loads return an already constructed GameObject. They may
// re-activate a PFB instance previously registered by OnCompleted, but they do
// not discover new replacement ownership. New ownership comes only from the
// an explicit model owner such as PFB, BaseModelViewPart or CharUIModelMono.
static void *TraceModelManagerLoadFromPersistentPool(void *self,
                                                      int64_t pathHash,
                                                      void *methodInfo) {
  auto original = (TraceModelManagerLoadHashFn)
      s_origModelManagerLoadFromPersistentPool;
  void *model = original ? original(self, pathHash, methodInfo) : nullptr;
  if (model) {
    TraceRememberLoadedModelPath(model, pathHash);
  }
  return model;
}

// Measure where a source Renderer and each of its Partners actually ARE in
// world space, at the moments the game finishes assembling the model.
//
// Every other diagnostic in this plugin reports ownership, registration or
// palette facts, and two runs with opposite visual outcomes produced identical
// values on all of them. World-space bounds are different in kind: they are the
// game's own answer to "where is this thing drawn", so a Partner that has
// collapsed to its bind pose or dropped below the character shows up as a
// centre Y well below its source even though every managed array looks right.
//
// Cost is a few getter calls per (source, partner) pair at a completion
// boundary, deduplicated by stage + pair. It performs no hierarchy walk and no
// per-model loop, so it cannot repeat the v117 ring-buffer load-time regression.


static void TraceBasePartFinish(void *self, bool success, void *methodInfo) {
  if (success)
    EiemRegisterBaseModelViewPartInstance(
        self, "BaseModelViewPart.OnLoadFinish-before-original", false);
  auto original = (TraceBasePartFinishFn)s_origBasePartFinish;
  ++s_eiemEnclosingModelAssemblyDepth;
  if (original) original(self, success, methodInfo);
  --s_eiemEnclosingModelAssemblyDepth;
  if (success)
    EiemRegisterBaseModelViewPartInstance(
        self, "BaseModelViewPart.OnLoadFinish", true);
}

// PostDealLoadedModel is normally nested inside OnLoadFinish. Let the game
// finish its native renderer and physics registration first. If it is invoked
// independently, its return becomes the completed commit boundary.
static void TraceBasePartPostDeal(void *self, void *methodInfo) {
  EiemAdoptUnityThreadFromAssemblyHook("BaseModelViewPart.PostDealLoadedModel");
  const bool enclosed = s_eiemEnclosingModelAssemblyDepth != 0;
  ++s_eiemEnclosingModelAssemblyDepth;
  auto original = (TraceBasePartPostDealFn)s_origBasePartPostDeal;
  if (original) original(self, methodInfo);
  --s_eiemEnclosingModelAssemblyDepth;
  if (!enclosed)
    EiemRegisterBaseModelViewPartInstance(
        self, "BaseModelViewPart.PostDealLoadedModel", true);
}

// ComplexModelViewPart overrides the virtual method, so a base-class hook is
// not sufficient for the concrete character path. Keep a separate trampoline
// and label to make dispatch visible in the runtime log.
static void TraceComplexPartPostDeal(void *self, void *methodInfo) {
  EiemAdoptUnityThreadFromAssemblyHook("ComplexModelViewPart.PostDealLoadedModel");
  const bool enclosed = s_eiemEnclosingModelAssemblyDepth != 0;
  ++s_eiemEnclosingModelAssemblyDepth;
  auto original = (TraceBasePartPostDealFn)s_origComplexPartPostDeal;
  if (original) original(self, methodInfo);
  --s_eiemEnclosingModelAssemblyDepth;
  if (!enclosed)
    EiemRegisterBaseModelViewPartInstance(
        self, "ComplexModelViewPart.PostDealLoadedModel", true);
}

static void TraceBasePartLoadFinishCallback(void *self, int32_t requestId,
                                            int64_t pathHash, void *model,
                                            void *methodInfo) {
  TraceRememberLoadedModelPath(model, pathHash);
  auto original =
      (TraceBasePartLoadFinishCallbackFn)s_origBasePartLoadFinishCallback;
  if (original) original(self, requestId, pathHash, model, methodInfo);
}

static bool TraceBasePartLoadFinishResult(void *self, int32_t requestId,
                                          int64_t pathHash, void *model,
                                          void *methodInfo) {
  TraceRememberLoadedModelPath(model, pathHash);
  auto original =
      (TraceBasePartLoadFinishResultFn)s_origBasePartLoadFinishResult;
  const bool result = original ? original(self, requestId, pathHash, model,
                                           methodInfo)
                               : false;
  return result;
}

// The handle-based path is separate from _OnLoadModelFinish and is used when
// a previously loaded model is reused. The original method populates m_model;
// inspect that exact object afterwards so no handle ABI or proxy assumptions
// leak into the replacement code.
static void TraceBasePartLoadUseHandleFinishCallback(void *self, bool success,
                                                      void *handle,
                                                      void *methodInfo) {
  auto original = (TraceBasePartLoadUseHandleFinishCallbackFn)
      s_origBasePartLoadUseHandleFinishCallback;
  if (original) original(self, success, handle, methodInfo);
  if (success)
    EiemRegisterBaseModelViewPartInstance(
        self, "BaseModelViewPart._OnLoadUseHandleFinishCallback",
        s_eiemEnclosingModelAssemblyDepth == 0);
}

static bool TraceBasePartLoadUseHandleFinish(void *self, bool success,
                                             void *handle,
                                             void *methodInfo) {
  auto original = (TraceBasePartLoadUseHandleFinishResultFn)
      s_origBasePartLoadUseHandleFinishResult;
  const bool result = original ? original(self, success, handle, methodInfo)
                               : false;
  if (result)
    EiemRegisterBaseModelViewPartInstance(
        self, "BaseModelViewPart._OnLoadUseHandleFinish",
        s_eiemEnclosingModelAssemblyDepth == 0);
  return result;
}

static void TraceBasePartReleaseModel(void *self, void *methodInfo) {
  EiemForgetModelOwner(EiemModelOwnerKind::BaseModelPart, self,
                       "BaseModelViewPart.ReleaseModel");
  auto original =
      (TraceBasePartPostDealFn)s_origBasePartReleaseModel;
  if (original) original(self, methodInfo);
}

static void TraceBasePartOnRelease(void *self, void *methodInfo) {
  EiemForgetModelOwner(EiemModelOwnerKind::BaseModelPart, self,
                       "BaseModelViewPart.OnRelease");
  auto original = (TraceBasePartPostDealFn)s_origBasePartOnRelease;
  if (original) original(self, methodInfo);
}


static bool EiemRegisterBaseModelViewPartInstance(void *part,
                                                   const char *stage,
                                                   bool applyResources) {
  if (!part) return false;
  void *model = TraceReadObjectField(part, s_basePartModelOffset);
  if (!model) return false;

  char path[768] = {};
  const int configPathOffset =
      (s_basePartConfigOffset >= 0 && s_basePartConfigPathOffset >= 0)
          ? s_basePartConfigOffset + s_basePartConfigPathOffset
          : -1;
  TraceReadStringField(part, configPathOffset, path, sizeof(path));
  if (!path[0]) TraceLookupLoadedModelPath(model, path, sizeof(path));

  const bool applied = EiemRegisterAndApplyModelInstance(
      EiemModelOwnerKind::BaseModelPart, part, model,
      path[0] ? path : nullptr, 0, stage, applyResources);
  if (applied || kEiemValidationIdentityProbe)
    Log("[MOD-MODEL-PART] completed part=%p path=%s model=%p applied=%d stage=%s",
        part, path, model, applied ? 1 : 0, stage ? stage : "unknown");
  return applied;
}

static bool EiemRegisterCharUIModelInstance(void *component,
                                             const char *stage,
                                             bool applyResources) {
  if (!component || !g_component_get_gameObject)
    return false;
  void *model = Invoke(g_component_get_gameObject, component);
  const bool applied = EiemRegisterAndApplyModelInstance(
      EiemModelOwnerKind::CharUIModel, component, model, nullptr, 0, stage,
      applyResources);
  if (applied || kEiemValidationIdentityProbe)
    Log("[MOD-CHAR-UI] completed component=%p model=%p applied=%d stage=%s",
        component, model, applied ? 1 : 0, stage ? stage : "unknown");
  return applied;
}
