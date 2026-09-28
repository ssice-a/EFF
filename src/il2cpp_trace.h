#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "eiem_mods.h"
#include "eiem_mod_update.h"
static bool EiemOnUnityThread();
#include "eiem_unity_lifetime.h"
#include "eiem_native_physics_parameters.h"
#include "eiem_native_physics_config.h"
#include "eiem_native_physics_events.h"
#include "eiem_native_physics_order_probe.h"
#include "eiem_model_lifecycle.h"
#include "eiem_render_replay.h"
#include "eiem_resource_backend.h"
#include "eiem_performance.h"
static size_t EiemManagedArrayLength(void *array);
#include "eiem_skeleton_runtime.h"
#include "eiem_registration_trace.h"
#include "eiem_skin_probe.h"

// Resource-loading hooks preserve the game's VFS/decryption pipeline and
// observe logical identities. Only the Render executor applies declared
// replacement resources to the matching game instances.

static volatile LONG s_traceSharedMeshCount = 0;
static volatile LONG s_traceMeshFilterCount = 0;
static volatile LONG s_traceHashLoadCount = 0;
static volatile LONG s_traceHashSubAssetCount = 0;
static volatile LONG s_traceAssetLoaderTryLoadCount = 0;
static volatile LONG s_traceCachedLoaderCount = 0;
static volatile LONG s_tracePathHashCount = 0;
static volatile LONG s_traceAssetCompleteCount = 0;
static volatile LONG s_traceStreamCaptureCount = 0;
static thread_local bool s_traceReentrant = false;
static volatile LONG s_traceSetterThreadLogged = 0;
// One bounded timeline for the intermittent ground-pose investigation. This
// records only tracked replacement Renderers; it does not mutate Unity state.
static volatile LONG s_eiemSkinTimelineCount = 0;
static volatile LONG64 s_eiemLastSkinCommitTick = 0;
// F10 skin timing probe. This is observation-only; it never rebinds a
// Renderer. A transaction id lets a post-window sample be matched to the
// replay that scheduled it.
static volatile LONG s_eiemSkinTimingProbeSequence = 0;
static volatile LONG s_eiemSkinTimingProbePending = 0;
static volatile LONG s_eiemSkinTimingProbeStage = 0;
static volatile LONG s_eiemSkinTimingProbeTicks = 0;
// Bounded calls from Unity's internal skin submission API during one probe
// window. The callbacks below are observation-only and deliberately stop
// after a small number of tracked/untracked calls.
static volatile LONG s_eiemSkinNativeTrackedCalls = 0;
static volatile LONG s_eiemSkinNativeUntrackedCalls = 0;
static volatile LONG s_eiemSkinCaptureRequestCalls = 0;
// One cold-start observation window.  It is armed only after the first
// complete model replay has produced the clothing targets, so the startup
// sample can be compared with the later F10 transactions without changing
// their behavior.
static volatile LONG s_eiemSkinColdProbeArmed = 0;
// Bounded descriptor-call census. This is enabled only for the temporary
// descriptor investigation and records the first calls even when the
// descriptor's name field is not the one expected by our metadata guess.
static volatile LONG s_eiemDescriptorInfoCallCount = 0;
static volatile LONG s_eiemDescriptorAssetsCallCount = 0;
// Bounded one-shot census of the native bones available at the resource
// submission boundary. This is separate from legacy skin diagnostics so a
// noisy unrelated probe cannot hide the assembly evidence.
static volatile LONG s_eiemSkinCaptureProbeCount = 0;
static volatile LONG s_eiemMaterialBoundarySkinProbeCount = 0;
// The CPU evidence window follows one owner containing body, cloth_01 and
// cloth_02.  Keeping the owner stable across A/B/C/D samples prevents a
// scene-wide override table from turning the probe into a cold-start scan.
static volatile LONG s_eiemSkinTargetTransaction = 0;
static void *s_eiemSkinTargetOwner = nullptr;

// The manager often returns the same cached proxy repeatedly. Keep the first
// sighting of each hash so completion records retain their own log budget.
static SRWLOCK s_traceSeenHashLock = SRWLOCK_INIT;
static int64_t s_traceSeenHashes[1024] = {};
static size_t s_traceSeenHashCount = 0;

// Runtime bundle manifest. Unlike diagnostic trace counters, this collection
// is intentionally not capped: it is the input manifest for the offline VFS
// extractor. Paths are kept as logical game paths and written periodically by
// the hotkey worker, so resource hooks never perform disk I/O.
struct TraceBundleManifestEntry {
  std::string path;
  ULONGLONG firstSeenMs;
};
struct TraceActiveBundleEntry {
  void *bundle;
  std::string path;
};
struct TracePendingBundleRequestEntry {
  void *request;
  std::string path;
};
static SRWLOCK s_bundleManifestLock = SRWLOCK_INIT;
static std::vector<TraceBundleManifestEntry> s_bundleManifestEntries;
static std::vector<TraceActiveBundleEntry> s_activeBundleEntries;
static std::vector<TracePendingBundleRequestEntry> s_pendingBundleRequests;

// Connect the game's asynchronous proxy/load lifecycle to the final Unity
// object. This is process-local evidence only, but it lets a scene Dump carry
// the original logical asset path and hash instead of guessing between
// same-named objects in different serialized files.
struct TraceProxyOriginEntry {
  void *proxy;
  int64_t pathHash;
  uint64_t stamp;
  char path[768];
};
struct TraceAssetOriginEntry {
  void *asset;
  int64_t pathHash;
  uint64_t stamp;
  char path[768];
};
static SRWLOCK s_assetOriginLock = SRWLOCK_INIT;
static TraceProxyOriginEntry s_proxyOrigins[8192] = {};
static TraceAssetOriginEntry s_assetOrigins[16384] = {};
static volatile LONG64 s_assetOriginStamp = 0;

static void TraceRememberBundlePathText(const char *pathText);
static void TraceRememberBundlePath(void *path);
static void TraceRememberActiveBundle(void *bundle, void *path);
static void TraceForgetActiveBundle(void *bundle);
static void TraceRememberPendingBundleRequest(void *request, void *path);
static void TraceResolvePendingBundleRequest(void *request, void *bundle);
static void TraceDescribeObject(void *object, char *out, int outSize);
static void TraceDescribeString(void *stringObject, char *out, int outSize);
static bool TraceIdentityTextMatchesConfiguredRule(const char *text);
static bool TraceTakeTargetBudget(volatile LONG *counter, LONG limit,
                                  const char *primary,
                                  const char *secondary = nullptr);
static void TraceRememberAssetOrigin(void *asset, int64_t pathHash,
                                     const char *path);
static void TraceBuildRendererHierarchy(void *renderer, char *out,
                                        size_t outSize);
static void TraceReadUnityObjectName(void *object, char *out, int outSize);
static bool TraceLookupAssetOrigin(void *asset, int64_t *pathHash, char *path,
                                   size_t pathSize);

static void EiemReconcileModelPhysics(
    void *model, const std::vector<EiemPhysicsIntent> &intents, bool active,
    const char *stage);
static void EiemPhysicsRuntimeBoundary(const char *stage);
static size_t EiemPhysicsRuntimeRetireChangedAssets(const char *stage);
// Nested model callbacks during one F10 replay share the outer transaction;
// they must not each repeat Physics collection/readiness work.
static thread_local bool s_eiemPhysicsLifecycleTransaction = false;
static void EiemReleaseModelPhysics(void *model, const char *stage);
static void TraceLookupHashPath(int64_t hash, char *out, int outSize);
static bool TraceResolveStringPathHashPath(int64_t hash, char *out,
                                           size_t outSize);
static bool TraceLookupAssetOrigin(void *asset, int64_t *pathHash, char *path,
                                   size_t pathSize);
static void EiemExtractObjectName(const char *description, char *out,
                                   size_t outSize);
typedef void (__fastcall *TraceSetSharedMeshFn)(void *self, void *mesh,
                                                 void *methodInfo);
static void *s_origSkinnedMeshSetSharedMesh = nullptr;
static void *s_origMeshFilterSetSharedMesh = nullptr;
typedef bool (__fastcall *TraceRendererInfoMaterialCommitFn)(
    void *self, void *materialOrMaterials, void *methodInfo);
static void *s_origRendererInfoTrySetSharedMaterial = nullptr;
static void *s_origRendererInfoTrySetSharedMaterials = nullptr;
static void *s_origRendererInfoTryReplaceSharedMaterials = nullptr;
#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD)
static bool EiemInvokeAdaptedNativeVfxCommit(
    void *self, void *input, bool inputIsArray, void *methodInfo,
    TraceRendererInfoMaterialCommitFn original, bool *adapted);
#endif
static int s_materialRendererInfoRendererOffset = -1;
static void *s_origMaterialInfoInit = nullptr;
typedef void (__fastcall *TraceSetBonesFn)(void *self, void *bones,
                                           void *methodInfo);
static void *s_origSkinnedMeshSetBones = nullptr;
typedef bool (__fastcall *TraceRequestCurrentFrameSkinMatricesFn)(
    void *self, void *skinMatrices, int32_t count, void *methodInfo);
typedef bool (__fastcall *TraceSkinMatricesRequestFinishedFn)(
    void *self, void *methodInfo);
typedef void *(__fastcall *TraceSkinGraphicsBufferFn)(void *self,
                                                       void *methodInfo);
static void *s_origSkinnedMeshRequestCurrentFrameSkinMatrices = nullptr;
static void *s_origSkinnedMeshSkinMatricesRequestFinished = nullptr;
static void *s_origSkinnedMeshGetVertexBuffer = nullptr;
static void *s_origSkinnedMeshGetPreviousVertexBuffer = nullptr;
// HG's custom skin-capture path is a candidate boundary between the managed
// Renderer state and the game's GPU-side skin buffer. The probe below is
// observation-only and is enabled only while the bounded cold/F10 window is
// active; it never changes the capture request or its property block.
typedef void (__fastcall *TraceSkinCaptureRequestFn)(
    void *self, void *meshRenderer, void *skinnedMeshRenderer,
    void *propertyBlock, void *methodInfo);
static void *s_origSkinnedMeshCaptureRequest = nullptr;
// MaterialPropertyBlock buffer bindings are the next read-only boundary after
// the renderer/bones state.  The custom pipeline may bind its skin palette
// through SetBuffer/SetConstantBuffer without entering Unity's public skin
// request APIs.  Keep this probe bounded to the existing cold/F10 timing
// window; it records the binding descriptor only and never changes it.
typedef void (__fastcall *TraceMaterialPropertyBlockSetBufferFn)(
    void *self, int32_t propertyId, void *buffer, int32_t offset,
    int32_t size, void *methodInfo);
static void *s_origMaterialPropertyBlockSetBuffer = nullptr;
static void *s_origMaterialPropertyBlockSetConstantBuffer = nullptr;
static void *s_origMaterialSetConstantBuffer = nullptr;
typedef void *(__fastcall *TraceRenderGraphGetComputeBufferFn)(
    void *self, void *handle, void *methodInfo);
static void *s_origRenderGraphGetComputeBuffer = nullptr;
static volatile LONG s_eiemSkinBufferBindingCalls = 0;
static volatile LONG s_eiemGpuDrivenCalls = 0;
// Read-only observation of the game's custom GPU-cloth path.  These counters
// are reset for each cold/F10 skin timing window and bound the amount of log
// data produced by high-frequency Tick/SetPerDrawData calls.
static volatile LONG s_eiemGpuClothObservationCalls = 0;
static volatile LONG s_eiemGpuClothEventSequence = 0;
static volatile LONG s_eiemGpuClothStartupCalls = 0;
typedef void (__fastcall *TraceGpuClothTickFn)(void *self, float deltaTime,
                                               void *methodInfo);
typedef void (__fastcall *TraceGpuClothPipelineUpdateV2Fn)(
    void *self, void *transform, void *methodInfo);
typedef void (__fastcall *TraceGpuClothPipelineUpdateV2StaticFn)(
    void *transform, void *methodInfo);
typedef void (__fastcall *TraceGpuClothRegisterGroupFn)(
    void *self, void *clothGroupData, void *methodInfo);
typedef void (__fastcall *TraceGpuClothSetCharacterProxyMeshFn)(
    void *self, void *mesh, void *methodInfo);
typedef void *(__fastcall *TraceGpuClothGetSkeletonBufferFn)(
    void *self, void *methodInfo);
typedef bool (__fastcall *TraceGpuClothBoolFn)(void *self, void *methodInfo);
static void *s_origGpuClothTick = nullptr;
static void *s_origGpuClothSetPerDrawData = nullptr;
static void *s_origGpuClothPipelineUpdateV2 = nullptr;
static void *s_origGpuClothPipelineUpdateV2Static = nullptr;
static void *s_origGpuClothRegisterGroup = nullptr;
static void *s_origGpuClothSetCharacterProxyMesh = nullptr;
static void *s_origGpuClothGetSkeletonBuffer = nullptr;
static void *s_origGpuClothIsSkeletonValid = nullptr;
static void *s_origGpuClothIsSkeletonFlipped = nullptr;
static void *s_origGpuClothFlipSkeletonFlag = nullptr;
// The custom HG renderer records skin resources directly on CommandBuffer.
// These hooks observe the command descriptor (native buffer id, property id,
// offset and size) without touching the command or draw state.
typedef void (__fastcall *TraceCommandBufferSetGlobalConstantBuffer0Fn)(
    void *self, uint32_t bufferId, int32_t propertyId, int32_t offset,
    int32_t size, void *methodInfo);
typedef void (__fastcall *TraceCommandBufferSetGlobalBufferIdFn)(
    void *self, int32_t propertyId, uint32_t bufferId, void *methodInfo);
typedef void (__fastcall *TraceCommandBufferSetGlobalConstantBufferFn)(
    void *self, void *buffer, int32_t propertyId, int32_t offset,
    int32_t size, void *methodInfo);
typedef void (__fastcall *TraceCommandBufferSetGlobalBufferFn)(
    void *self, int32_t propertyId, void *buffer, void *methodInfo);
static void *s_origCommandBufferSetGlobalConstantBuffer0 = nullptr;
static void *s_origCommandBufferSetGlobalBufferId = nullptr;
static void *s_origCommandBufferSetGlobalConstantBuffer = nullptr;
static void *s_origCommandBufferSetGlobalBuffer = nullptr;
// HG GPU-driven renderer is the game's custom render submission path.  These
// hooks only identify the command buffer/list submitted during a bounded
// skin transaction; they do not alter renderer state or GPU resources.
typedef void (__fastcall *TraceGpuDrivenBindBuffersForRenderingFn)(
    void *self, void *commandBuffer, void *methodInfo);
typedef void (__fastcall *TraceGpuDrivenPopulatePerFrameDataFn)(
    void *self, void *commandBuffer, uint32_t frameDataId,
    uint32_t rendererDataId, bool flag, void *methodInfo);
typedef void (__fastcall *TraceGpuDrivenDrawRendererListFn)(
    void *self, void *commandBuffer, uint32_t rendererListId, bool flag,
    void *methodInfo);
static void *s_origGpuDrivenV2BindBuffersForCulling = nullptr;
static void *s_origGpuDrivenV2BindBuffersForRendering = nullptr;
static void *s_origGpuDrivenV2PopulatePerFrameData = nullptr;
static void *s_origGpuDrivenV2DrawRendererList = nullptr;
static void *s_origGpuDrivenV1PopulatePerFrameData = nullptr;
static void *s_origGpuDrivenV1DrawRendererList = nullptr;
static void *s_origGpuDrivenV1BindBuffersForCulling = nullptr;
typedef void (__fastcall *TraceGpuDrivenBindBuffersForCullingFn)(
    void *self, void *commandBuffer, void *computeShader, uint32_t bufferId,
    void *methodInfo);
typedef void (__fastcall *TraceGpuDrivenDispatchComputeFn)(
    void *self, void *commandBuffer, void *computeShader, uint32_t dispatchId,
    void *methodInfo);
typedef void (__fastcall *TraceGpuDrivenBindFrameConstantsFn)(
    void *self, void *commandBuffer, void *computeShader, uint32_t bufferId,
    void *methodInfo);
typedef void (__fastcall *TraceGpuDrivenBindFrameConstantsGlobalFn)(
    void *self, void *commandBuffer, void *methodInfo);
typedef void (__fastcall *TraceGpuDrivenAdvanceFrameFn)(
    void *self, void *methodInfo);
static void *s_origGpuDrivenV1BindFrameConstants = nullptr;
static void *s_origGpuDrivenV1BindFrameConstantsGlobal = nullptr;
static void *s_origGpuDrivenV1DispatchMeshletInstanceCount = nullptr;
static void *s_origGpuDrivenV1DispatchDrawBucketCount = nullptr;
static void *s_origGpuDrivenV1BindBuffersForRendering = nullptr;
static void *s_origGpuDrivenV1AdvanceFrame = nullptr;
static void *s_origGpuDrivenV2BindFrameConstants = nullptr;
static void *s_origGpuDrivenV2BindFrameConstantsGlobal = nullptr;
static void *s_origGpuDrivenV2DispatchMeshletInstanceCount = nullptr;
static void *s_origGpuDrivenV2DispatchDrawBucketCount = nullptr;
static void *s_origGpuDrivenV2AdvanceFrame = nullptr;
typedef void(__fastcall *TraceCreateSmsGoFn)(
    void *assetLoader, void *meshAssets, int32_t lod, void *goPool,
    void *parent, void *stringList, void *intList, void **renderers,
    void **rootBones, bool flag, void *handleMap, bool deferred,
    void *methodInfo);
typedef void(__fastcall *TraceCreateSmsPostFn)(
    void *meshAssets, int32_t lod, void *goPool, void *parent,
    void *stringList, void *intList, void **renderers, void **rootBones,
    bool flag, void *methodInfo);
typedef void(__fastcall *TraceLodGroupSetLodsFn)(
    void *self, void *lods, void *methodInfo);
// The post-model helper is a compiler-generated static local function. Its
// explicit parameters are (lod, renderer array, root-bone array, closure).
// It is the last point before the game's AssignSkin code consumes the array.
typedef void(__fastcall *TraceAssignSkinPostFn)(
    int32_t lod, void *renderers, void *rootBones, void *closure,
    void *methodInfo);
static void *s_origCreateSmsGo = nullptr;
static void *s_origCreateSmsPost = nullptr;
static void *s_origAssignSkinGo = nullptr;
static void *s_origAssignSkinPost = nullptr;
// NPCAvatarCreatorUtils assigns the final Animator bone palette after the
// renderer array has been created. This remains an ordering observation; the
// concrete NPC Renderer is handled at RendererInfo._Init.
typedef void(__fastcall *TraceSetSmrRootBoneFn)(
    void *animator, void *renderers, void *rootBoneInfos, void *methodInfo);
static void *s_origSetSmrRootBone = nullptr;
typedef void (__fastcall *TraceModelManagerGameObjectFn)(void *self,
                                                          void *model,
                                                          void *methodInfo);
typedef void *(__fastcall *TraceModelManagerLoadHashFn)(void *self,
                                                         int64_t pathHash,
                                                         void *methodInfo);
typedef void (__fastcall *TracePrefabInstantiateCompletedFn)(void *self,
                                                              void *methodInfo);
typedef void (__fastcall *TracePrefabInstantiateLifecycleFn)(void *self,
                                                              void *methodInfo);
typedef void *(__fastcall *TraceUIModelLoaderLoadModelFn)(
    void *self, void *path, void *parent, void *methodInfo);
typedef int32_t (__fastcall *TraceUIModelLoaderLoadModelAsyncFn)(
    void *self, void *path, void *parent, void *callback, void *methodInfo);
typedef void (__fastcall *TraceUIModelLoaderUnloadModelFn)(void *self,
                                                            void *model,
                                                            void *methodInfo);
typedef void (__fastcall *TraceUIModelLoaderLifecycleFn)(void *self,
                                                          void *methodInfo);
typedef void (__fastcall *TraceCharUIModelLifecycleFn)(void *self,
                                                        void *methodInfo);
typedef void (__fastcall *TraceCharUIModelSetVisibleFn)(void *self,
                                                         bool visible,
                                                         void *methodInfo);
typedef void (__fastcall *TraceBasePartFinishFn)(void *self, bool success,
                                                  void *methodInfo);
typedef void (__fastcall *TraceBasePartPostDealFn)(void *self,
                                                    void *methodInfo);
typedef void (__fastcall *TraceEntityRenderHelperInitFn)(
    void *self, void *methodInfo);
// Read-only observation of the game's material/renderer registry commit.  The
// controller receives the complete Renderer list and builds RendererInfo
// entries used by the custom visibility/material path.
typedef void (__fastcall *TraceEntityRenderHelperMaterialControllerInitFn)(
    void *self, void *renderers, void *rendererTypeConfigs,
    void *customizeRendererPropertyConfig, bool calculateBoundsWithTransform,
    void *methodInfo);
typedef void (__fastcall *TraceBasePartLoadFinishCallbackFn)(
    void *self, int32_t requestId, int64_t pathHash, void *model,
    void *methodInfo);
typedef bool (__fastcall *TraceBasePartLoadFinishResultFn)(
    void *self, int32_t requestId, int64_t pathHash, void *model,
    void *methodInfo);
typedef void (__fastcall *TraceBasePartLoadUseHandleFinishCallbackFn)(
    void *self, bool success, void *handle, void *methodInfo);
typedef bool (__fastcall *TraceBasePartLoadUseHandleFinishResultFn)(
    void *self, bool success, void *handle, void *methodInfo);
static void *s_origModelManagerGameObjectAllocate = nullptr;
static void *s_origModelManagerLoadFromPersistentPool = nullptr;
static void *s_origPrefabInstantiateCompleted = nullptr;
static void *s_origPrefabInstantiateUnload = nullptr;
static void *s_origPrefabInstantiateClear = nullptr;
static void *s_origPrefabInstantiateDispose = nullptr;
static void *s_prefabInstantiateGetGameObject = nullptr;
static void *s_prefabInstantiateGetLogName = nullptr;
static void *s_prefabInstantiateGetInstanceUid = nullptr;
static void *s_origUIModelLoaderLoadModel = nullptr;
static void *s_origUIModelLoaderLoadModelAsync = nullptr;
static void *s_origUIModelLoaderUnloadModel = nullptr;
static void *s_origUIModelLoaderClear = nullptr;
static void *s_origUIModelLoaderDispose = nullptr;
static void *s_origCharUIModelOnAwake = nullptr;
static void *s_origCharUIModelSetVisible = nullptr;
static void *s_origCharUIModelOnRelease = nullptr;
static void *s_origBasePartFinish = nullptr;
static void *s_origBasePartLoadFinishCallback = nullptr;
static void *s_origBasePartLoadFinishResult = nullptr;
static void *s_origBasePartLoadUseHandleFinishCallback = nullptr;
static void *s_origBasePartLoadUseHandleFinishResult = nullptr;
static void *s_origBasePartReleaseModel = nullptr;
static void *s_origBasePartOnRelease = nullptr;
static void *s_origBasePartPostDeal = nullptr;
static void *s_origComplexPartPostDeal = nullptr;
static void *s_origEntityRenderHelperInitRenderAndMaterial = nullptr;
static void *s_origEntityRenderHelperMaterialControllerInit = nullptr;
static void *s_entityRenderHelperClass = nullptr;
static thread_local bool s_eiemEntityRenderHelperInitGuard = false;
static thread_local bool s_eiemEntityRenderHelperMaterialInitGuard = false;
// The outer helper owns the model transaction, while MaterialController.Init
// is the first native boundary at which the game has populated each
// SkinnedMeshRenderer's local bones[] palette.  Carry the concrete model into
// that nested call so the replacement is committed against the same instance
// (never by a scene-wide search or a cross-instance bone lookup).
static thread_local void *s_eiemEntityRenderHelperActiveModel = nullptr;
static thread_local bool s_eiemEntityRenderHelperMaterialApplied = false;
// RendererInfo._Init samples original materials during an enclosing helper.
// The helper must finish that sampling before EIEM restores its owned slots.
// RendererInfo callbacks may occur anywhere inside the outer helper, not only
// inside MaterialController.Init. Drain this queue after the complete helper
// returns so no raw Renderer pointer survives into a later assembly pass.
static thread_local std::vector<void *> s_eiemMaterialsToReapplyAfterHelper;
static thread_local EiemRenderReplayLedger *s_eiemActiveRenderReplay = nullptr;
static SRWLOCK s_eiemNativeSkinRefreshLock = SRWLOCK_INIT;
static std::vector<uintptr_t> s_eiemNativeSkinRefreshModels;
// Character assembly hooks are nested. Inner hooks only observe native state;
// the outermost completed boundary commits one replacement generation.
static thread_local uint32_t s_eiemEnclosingModelAssemblyDepth = 0;
static int s_basePartModelOffset = -1;
static int s_basePartConfigOffset = -1;
static int s_basePartConfigPathOffset = -1;
static int s_basePartRenderersOffset = -1;
static int s_basePartRenderersInitStateOffset = -1;
static int s_basePartHgRenderersOffset = -1;
static int s_basePartHgRenderersInitStateOffset = -1;
static int s_basePartMeshesOffset = -1;
static int s_basePartMeshesInitStateOffset = -1;
static int s_basePartBoneClothsOffset = -1;
static int s_basePartLodGroupsOffset = -1;
struct TraceLoadedModelPathEntry {
  void *model = nullptr;
  int64_t pathHash = 0;
  char path[768] = {};
};
static SRWLOCK s_traceLoadedModelPathLock = SRWLOCK_INIT;
static TraceLoadedModelPathEntry s_traceLoadedModelPaths[256] = {};
static size_t s_traceLoadedModelPathCount = 0;
// Set while the reconciliation pass calls Unity's managed setter. The setter
// hooks then forward to their original trampoline instead of recursively
// resolving the replacement that is already being assigned.
static thread_local bool s_eiemApplyingModMeshAssignment = false;
static void TraceSkinnedMeshSetSharedMesh(void *self, void *mesh,
                                           void *methodInfo);
static void TraceSkinnedMeshSetBones(void *self, void *bones,
                                     void *methodInfo);
static void EiemLogSkinSetterTimeline(const char *event, void *renderer,
                                      void *requestedMesh, void *appliedMesh,
                                      void *incomingBones, void *afterBones);
static void TraceMeshFilterSetSharedMesh(void *self, void *mesh,
                                          void *methodInfo);
static size_t EiemApplyStandaloneRenderRulesToSkinArray(
    void *renderers, const char *stage);
static void *EiemReadSharedMesh(void *renderer, const char *rendererType);
static void TraceModelManagerGameObjectAllocate(void *self, void *model,
                                                 void *methodInfo);
static void *TraceModelManagerLoadFromPersistentPool(void *self,
                                                      int64_t pathHash,
                                                      void *methodInfo);
static void TracePrefabInstantiateCompleted(void *self, void *methodInfo);
static void TracePrefabInstantiateUnload(void *self, void *methodInfo);
static void TracePrefabInstantiateClear(void *self, void *methodInfo);
static void TracePrefabInstantiateDispose(void *self, void *methodInfo);
static void *TraceUIModelLoaderLoadModel(void *self, void *path,
                                         void *parent, void *methodInfo);
static int32_t TraceUIModelLoaderLoadModelAsync(void *self, void *path,
                                                void *parent, void *callback,
                                                void *methodInfo);
static void TraceUIModelLoaderUnloadModel(void *self, void *model,
                                          void *methodInfo);
static void TraceUIModelLoaderClear(void *self, void *methodInfo);
static void TraceUIModelLoaderDispose(void *self, void *methodInfo);
static void TraceCharUIModelOnAwake(void *self, void *methodInfo);
static void TraceCharUIModelSetVisible(void *self, bool visible,
                                       void *methodInfo);
static void TraceCharUIModelOnRelease(void *self, void *methodInfo);
static void TraceBasePartFinish(void *self, bool success, void *methodInfo);
static void TraceBasePartPostDeal(void *self, void *methodInfo);
static void TraceComplexPartPostDeal(void *self, void *methodInfo);
static void TraceEntityRenderHelperInitRenderAndMaterial(void *self,
                                                           void *methodInfo);
static void TraceEntityRenderHelperMaterialControllerInit(
    void *self, void *renderers, void *rendererTypeConfigs,
    void *customizeRendererPropertyConfig, bool calculateBoundsWithTransform,
    void *methodInfo);
static int TraceManagedListCount(void *list);
static void EiemLogMaterialControllerRegistry(void *controller,
                                               void *rendererList,
                                               const char *phase);
static void TraceBasePartLoadFinishCallback(void *self, int32_t requestId,
                                            int64_t pathHash, void *model,
                                            void *methodInfo);
static bool TraceBasePartLoadFinishResult(void *self, int32_t requestId,
                                          int64_t pathHash, void *model,
                                          void *methodInfo);
static void TraceBasePartLoadUseHandleFinishCallback(void *self, bool success,
                                                      void *handle,
                                                      void *methodInfo);
static bool TraceBasePartLoadUseHandleFinish(void *self, bool success,
                                             void *handle,
                                             void *methodInfo);
static void TraceBasePartReleaseModel(void *self, void *methodInfo);
static void TraceBasePartOnRelease(void *self, void *methodInfo);
static bool EiemRegisterAndApplyModelInstance(
    EiemModelOwnerKind ownerKind, void *owner, void *model,
    const char *prefabPath, uint32_t instanceUid, const char *stage,
    bool applyResources);
static bool EiemRegisterBaseModelViewPartInstance(void *part,
                                                   const char *stage,
                                                   bool applyResources);
static void *EiemFindBaseModelPartForModel(void *model);
static void EiemLogBaseModelCacheProbe(void *part, LONG transaction,
                                       const char *phase);
static bool EiemRegisterCharUIModelInstance(void *component,
                                             const char *stage,
                                             bool applyResources);
static bool EiemReapplyRegisteredModelInstance(void *model,
                                                const char *stage);

static EiemPerfCounter s_eiemPerfPrefabCompletion;
static EiemPerfCounter s_eiemPerfModelRegistration;
static EiemPerfCounter s_eiemPerfRuleApplication;
static EiemPerfCounter s_eiemPerfComponentSnapshot;
static EiemPerfCounter s_eiemPerfRendererVisit;

static void EiemLogPerformanceSummary(LONG64 prefabCalls) {
  const LONG64 registrationCalls = EiemPerfRead(s_eiemPerfModelRegistration.calls);
  const LONG64 applicationCalls = EiemPerfRead(s_eiemPerfRuleApplication.calls);
  const LONG64 snapshotCalls = EiemPerfRead(s_eiemPerfComponentSnapshot.calls);
  const LONG64 visitCalls = EiemPerfRead(s_eiemPerfRendererVisit.calls);
  Log("[PERF-STARTUP-v1] prefabs=%lld prefabMs=%.2f registerCalls=%lld "
      "registerMs=%.2f applyCalls=%lld applyMs=%.2f snapshotCalls=%lld "
      "snapshotMs=%.2f visitCalls=%lld visitMs=%.2f maxApplyMs=%.2f",
      prefabCalls,
      EiemPerfMilliseconds(EiemPerfRead(s_eiemPerfPrefabCompletion.ticks)),
      registrationCalls,
      EiemPerfMilliseconds(EiemPerfRead(s_eiemPerfModelRegistration.ticks)),
      applicationCalls,
      EiemPerfMilliseconds(EiemPerfRead(s_eiemPerfRuleApplication.ticks)),
      snapshotCalls,
      EiemPerfMilliseconds(EiemPerfRead(s_eiemPerfComponentSnapshot.ticks)),
      visitCalls,
      EiemPerfMilliseconds(EiemPerfRead(s_eiemPerfRendererVisit.ticks)),
      EiemPerfMilliseconds(EiemPerfRead(s_eiemPerfRuleApplication.maximum)));
}
static bool EiemApplyStandaloneRenderRules(void *model, const char *stage,
                                           bool *matched = nullptr,
                                           const std::vector<std::string> *affected = nullptr,
                                           std::vector<EiemPhysicsIntent> *physicsIntents = nullptr,
                                           const std::vector<EiemModRule> *preparedRules = nullptr);
static bool EiemApplyStandaloneRenderRulesToRenderer(
    void *meshOwner, void *drawRenderer, void *mesh,
    const char *rendererType, void *methodInfo, const char *stage);
static bool EiemReapplyRendererMaterialsAfterCommit(void *renderer,
                                                     const char *stage);
static bool EiemResolveMeshBonesFromAssembly(
    const EiemSkinIdentity &identity, void *renderer, void **out,
    char *error, size_t errorSize);
static bool EiemBuildRelativeRendererPath(void *rootTransform, void *renderer,
                                          char *out, size_t outSize);
static void *EiemReadLodRendererMesh(void *renderer,
                                     const char **rendererTypeOut);
static bool EiemRenderRuleMatches(const EiemModRule &rule,
                                  const char *relativePath, void *mesh,
                                  const char *asset);
static void EiemForgetModelOwner(EiemModelOwnerKind ownerKind, void *owner,
                                 const char *stage);
static void EiemForgetModelInstance(void *model, const char *stage);
static void EiemSetModelOwnerActive(EiemModelOwnerKind ownerKind, void *owner,
                                    bool active, const char *stage);
static void EiemQueueModReconcile(const char *reason);
static void EiemRequestModUpdate(EiemModUpdate request, const char *reason);
static void EiemQueueNativeSkinRefresh(uintptr_t ownerModel);


static bool EiemOnUnityThread() {
  const DWORD current = GetCurrentThreadId();
  if (s_eiemUnityThreadId) return current == s_eiemUnityThreadId;
  if (!g_gameHwnd) return false;
  DWORD windowProcess = 0;
  const DWORD windowThread = GetWindowThreadProcessId(g_gameHwnd, &windowProcess);
  if (!windowThread || windowProcess != GetCurrentProcessId() ||
      windowThread != current)
    return false;
  InterlockedCompareExchange((volatile LONG *)&s_eiemUnityThreadId,
                             (LONG)current, 0);
  return true;
}

// These assembly callbacks execute on the game's Unity thread before the
// window-procedure path necessarily observes its first message.  Pin the
// thread from the callback that is about to invoke Unity setters; otherwise
// the startup resource pass is rejected as `unityThread=0` and no replacement
// can be built until a later F10 reconcile.
static void EiemAdoptUnityThreadFromAssemblyHook(const char *hook) {
  const DWORD current = GetCurrentThreadId();
  const LONG previous = InterlockedCompareExchange(
      (volatile LONG *)&s_eiemUnityThreadId, (LONG)current, 0);
  if (!previous)
    Log("[MOD-THREAD] Unity thread adopted from %s: %lu",
        hook ? hook : "assembly hook", (unsigned long)current);
}

static int32_t EiemTraceUnboxInt(void *boxed) {
  __try {
    return boxed ? *(int32_t *)((char *)boxed + 16) : -1;
  } __except (1) {
    return -1;
  }
}


static void EiemSetOriginalSharedMesh(void *renderer, void *mesh,
                                      const char *rendererType,
                                      void *methodInfo) {
  if (EiemModEquals(rendererType, "SkinnedMeshRenderer")) {
    auto original = (TraceSetSharedMeshFn)s_origSkinnedMeshSetSharedMesh;
    if (original) original(renderer, mesh, methodInfo);
  } else {
    auto original = (TraceSetSharedMeshFn)s_origMeshFilterSetSharedMesh;
    if (original) original(renderer, mesh, methodInfo);
  }
}

static bool EiemSetSharedMesh(void *renderer, void *mesh,
                              const char *rendererType, void *methodInfo) {
  if (EiemReadSharedMesh(renderer, rendererType) == mesh) return true;
  if (methodInfo) {
    EiemSetOriginalSharedMesh(renderer, mesh, rendererType, methodInfo);
    return EiemReadSharedMesh(renderer, rendererType) == mesh;
  }
  void *setter = EiemModEquals(rendererType, "SkinnedMeshRenderer")
                     ? g_smr_set_sharedMesh
                     : g_meshFilter_set_sharedMesh;
  if (!setter) {
    Log("[MOD] %s setter is unavailable; reconciliation cannot assign mesh",
        rendererType ? rendererType : "Renderer");
    return false;
  }
  void *params[] = {mesh};
  s_eiemApplyingModMeshAssignment = true;
  Invoke(setter, renderer, params);
  s_eiemApplyingModMeshAssignment = false;
  void *actual = EiemReadSharedMesh(renderer, rendererType);
  if (actual != mesh)
    Log("[MOD] %s sharedMesh verification failed: renderer=%p expected=%p actual=%p",
        rendererType ? rendererType : "Renderer", renderer, mesh, actual);
  return actual == mesh;
}

static void *EiemReadSharedMesh(void *renderer, const char *rendererType) {
  if (!renderer) return nullptr;
  void *getter = EiemModEquals(rendererType, "SkinnedMeshRenderer")
                     ? g_smr_get_sharedMesh
                     : g_meshFilter_get_sharedMesh;
  return getter ? Invoke(getter, renderer) : nullptr;
}

static bool EiemReadLiveMeshIdentity(void *mesh, char *source, size_t sourceSize,
                                     char *asset, size_t assetSize) {
  if (!mesh || !asset || assetSize == 0) return false;
  char description[512] = {};
  TraceDescribeObject(mesh, description, sizeof(description));
  EiemExtractObjectName(description, asset, assetSize);
  if (!asset[0]) return false;
  if (source && sourceSize) {
    int64_t hash = 0;
    TraceLookupAssetOrigin(mesh, &hash, source, sourceSize);
  }
  return true;
}

static void EiemReadLiveMeshShape(void *mesh, int32_t *vertices,
                                  int32_t *indices, int32_t *subMeshes) {
  if (vertices) *vertices = -1;
  if (indices) *indices = -1;
  if (subMeshes) *subMeshes = -1;
  if (!mesh) return;
  if (vertices)
    *vertices = g_mesh_get_vertexCount ? EiemTraceUnboxInt(Invoke(g_mesh_get_vertexCount, mesh)) : -1;
  if (subMeshes)
    *subMeshes = g_mesh_get_subMeshCount ? EiemTraceUnboxInt(Invoke(g_mesh_get_subMeshCount, mesh)) : -1;
  if (indices) {
    *indices = -1;
    if (g_mesh_GetIndexCount && *subMeshes >= 0 && *subMeshes <= 64) {
      int32_t total = 0;
      bool valid = true;
      for (int32_t index = 0; index < *subMeshes; ++index) {
        void *params[] = {&index};
        const int32_t count = EiemTraceUnboxInt(Invoke(g_mesh_GetIndexCount, mesh, params));
        if (count < 0 || total > INT32_MAX - count) { valid = false; break; }
        total += count;
      }
      if (valid) *indices = total;
    }
  }
}

// Set only while a matched Prefab declaration is applying its Render actions.
// It associates every mutation with one concrete Prefab instance so unload
// cleanup and hot reload never need a scene-wide identity guess.
static thread_local uintptr_t s_eiemActivePrefabInstance = 0;

#include "eiem_render_state.h"
static bool EiemManagedObjectArraySame(void *left, void *right);
static size_t EiemManagedArrayLength(void *array);


#include "eiem_shape_state.h"
#include "eiem_shape_guard.h"

// Runtime shape ownership is per Renderer, never per shared Mesh. No Unity
// calls on ImGui's thread; mutation failures are reported through the adapter.
struct EiemUnityShapes {
  bool Names(void *mesh,std::vector<std::string> &names) {
    void *boxed=nullptr;
    if(!mesh || !InvokeChecked(g_mesh_get_blendShapeCount,mesh,nullptr,&boxed) || !boxed)return false;
    const int count=*(int *)((char *)boxed+16);if(count<0)return false;
    std::vector<std::string> result;
    for(int i=0;i<count;++i){void *text=nullptr;void *args[]={&i};char name[192]={};
      if(!InvokeChecked(g_mesh_GetBlendShapeName,mesh,args,&text) || !text)return false;
      ReadStrUtf8(text,name,sizeof(name));if(strlen(name)>=sizeof(name)-1)return false;
      result.emplace_back(name);
    }
    names=std::move(result);return true;
  }
  bool Snapshot(void *renderer, void *mesh, std::vector<EiemShapeBaseline> &weights) {
    std::vector<std::string> names;
    if (!Names(mesh,names)) return false;
    std::vector<EiemShapeBaseline> values;
    for (int i = 0; i < (int)names.size(); ++i) {
      float value = 0;
      if (!Read(renderer, i, value)) return false;
      values.push_back({names[i], value});
    }
    weights = std::move(values);
    return true;
  }
  int Index(void *mesh, const std::string &name) {
    void *boxed = nullptr;
    if (!InvokeChecked(g_mesh_get_blendShapeCount, mesh, nullptr, &boxed) || !boxed) return -1;
    int count = *(int *)((char *)boxed + 16);
    for (int i = 0; i < count; ++i) {
      void *text = nullptr; void *args[] = {&i};
      if (!InvokeChecked(g_mesh_GetBlendShapeName, mesh, args, &text) || !text) return -1;
      char actual[192] = {}; ReadStrUtf8(text, actual, sizeof(actual));
      if (name == actual) return i;
    }
    return -1;
  }
  bool Read(void *renderer, int index, float &value) {
    EiemShapeAuthorScope origin;
    void *boxed = nullptr; void *args[] = {&index};
    if (!InvokeChecked(g_smr_GetBlendShapeWeight, renderer, args, &boxed) || !boxed) return false;
    value = *(float *)((char *)boxed + 16);
    return std::isfinite(value);
  }
  bool Write(void *renderer, int index, float value) {
    EiemShapeAuthorScope origin;
    void *result = nullptr; void *args[] = {&index, &value};
    return InvokeChecked(g_smr_SetBlendShapeWeight, renderer, args, &result);
  }
};
static SRWLOCK s_eiemShapeMessageLock = SRWLOCK_INIT;
static std::string s_eiemShapeMessage;
static void EiemShapeMessage(const EiemModRule &rule, const std::string &error) {
  const std::string text = std::string(rule.modPath) + " / " + rule.section + ": " + error;
  AcquireSRWLockExclusive(&s_eiemShapeMessageLock);
  if (text != s_eiemShapeMessage) Log("[SHAPE] %s", text.c_str());
  s_eiemShapeMessage = text;
  ReleaseSRWLockExclusive(&s_eiemShapeMessageLock);
}
static bool EiemUpdateRendererShapes(void *renderer, const char *type,
                                      const EiemModRule &rule, EiemShapeState &state,
                                      float elapsedSeconds = -1.0f) {
  if (!rule.shapeCount && state.owned.empty()) return true;
  if (!EiemOnUnityThread()) { EiemShapeMessage(rule, "Shape update requires Unity thread"); return false; }
  if (!EiemModEquals(type, "SkinnedMeshRenderer")) {
    EiemShapeMessage(rule, "Shape weights require SkinnedMeshRenderer"); return false;
  }
  EiemUnityShapes backend; std::string error;
  EiemShapeAuthorScope author;
  void *mesh=EiemReadSharedMesh(renderer,type);
  if(rule.shapeCount && state.binding && !EiemShapeBindingMatches(state,renderer,mesh)) {
    EiemRetireShapeBinding(state.binding);state={};
  }
  if(!state.binding && rule.shapeCount) {
    std::vector<EiemShapeBaseline> baseline;
    if(!backend.Snapshot(renderer,mesh,baseline) || !EiemPrepareShapeBinding(state,renderer,mesh,baseline,backend,error)) {
      EiemShapeMessage(rule,error.empty()?"Cannot capture native shape state":error);return false;
    }
    if(state.binding)state.binding->initialized=true; // No mesh assignment, keep current weights.
  }
  EiemSyncShapeClaims(state,rule);
  const bool applied = EiemApplyShapeWeights(renderer, mesh, rule, state, backend, error, elapsedSeconds);
  if (!applied)
    EiemShapeMessage(rule, error);
  EiemModRule none={};EiemSyncShapeClaims(state,none);
  return applied;
}

#include "eiem_render_override.h"

struct EiemResolvedRenderRule {
  EiemModRule rule = {};
  char source[768] = {};
  char asset[192] = {};
};


static size_t EiemManagedArrayValueElementSize(void *array,
                                               bool *valueTypeOut) {
  if (valueTypeOut) *valueTypeOut = false;
  if (!array || !il2cpp_object_get_class || !il2cpp_class_get_element_class ||
      !il2cpp_class_get_type || !il2cpp_type_get_type ||
      !il2cpp_class_value_size)
    return 0;
  __try {
    void *arrayClass = il2cpp_object_get_class(array);
    void *elementClass = arrayClass
                             ? il2cpp_class_get_element_class(arrayClass)
                             : nullptr;
    void *elementType = elementClass ? il2cpp_class_get_type(elementClass)
                                     : nullptr;
    const int typeCode = elementType ? il2cpp_type_get_type(elementType) : -1;
    // IL2CPP_TYPE_VALUETYPE is 0x11. Reference arrays store one object
    // pointer per element and need no class-size query.
    if (typeCode != 0x11) return sizeof(void *);
    uint32_t alignment = 0;
    const int32_t size = il2cpp_class_value_size(elementClass, &alignment);
    if (size <= 0 || size > 4096) return 0;
    if (valueTypeOut) *valueTypeOut = true;
    return (size_t)size;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}


struct EiemRootBoneInfoValue {
  bool readable = false;
  char name[192] = {};
  int32_t id = -1;
  int deferEnableRestore = -1;
  int rendererEnabled = -1;
};

static bool EiemReadRootBoneInfoAt(void *rootBones, size_t index,
                                   size_t elementSize, size_t count,
                                   EiemRootBoneInfoValue *out) {
  if (!out) return false;
  *out = {};
  // RootBoneInfo is a 16-byte value type on the current IL2CPP build:
  // pointer (8) + bone id (4) + two boolean flags (1 each) + padding.
  if (!rootBones || index >= count || elementSize < 16) return false;
  __try {
    const char *slot = (const char *)rootBones + IL2CPP_ARRAY_DATA +
                       index * elementSize;
    void *name = *(void **)slot;
    out->id = *(int32_t *)(slot + 8);
    out->deferEnableRestore = *(uint8_t *)(slot + 12) ? 1 : 0;
    out->rendererEnabled = *(uint8_t *)(slot + 13) ? 1 : 0;
    if (name) ReadStrUtf8(name, out->name, sizeof(out->name));
    out->readable = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    out->readable = false;
  }
  return out->readable;
}

static bool EiemReadRendererForceRenderingOff(void *renderer, bool *off) {
  if (off) *off = false;
  if (!renderer || !off || !g_renderer_get_forceRenderingOff) return false;
  __try {
    void *boxed = Invoke(g_renderer_get_forceRenderingOff, renderer);
    if (!boxed) return false;
    *off = *(bool *)((char *)boxed + 16);
    return true;
  } __except (1) {
    return false;
  }
}


static size_t EiemManagedArrayLength(void *array) {
  if (!array) return 0;
  __try {
    const uintptr_t length = *(uintptr_t *)((char *)array + 24);
    return length > 100000 ? 0 : (size_t)length;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

static bool EiemManagedObjectArraySame(void *left, void *right) {
  if (left == right) return true;
  const size_t leftCount = EiemManagedArrayLength(left);
  const size_t rightCount = EiemManagedArrayLength(right);
  if (leftCount != rightCount) return false;
  if (!left || !right) return left == right;
  __try {
    void **leftItems = (void **)((char *)left + 32);
    void **rightItems = (void **)((char *)right + 32);
    for (size_t index = 0; index < leftCount; ++index)
      if (leftItems[index] != rightItems[index]) return false;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

static bool EiemAssignRendererMaterials(void *renderer, void *materials,
                                        char *error, size_t errorSize) {
  if (!renderer || !materials || !s_eiemRendererSetSharedMaterials ||
      !g_renderer_get_sharedMaterials) {
    if (error)
      strncpy_s(error, errorSize,
                "Unity Renderer material assignment APIs are unavailable",
                _TRUNCATE);
    return false;
  }
  void *current = Invoke(g_renderer_get_sharedMaterials, renderer);
  if (EiemManagedObjectArraySame(materials, current)) return true;
  void *params[] = {materials};
  Invoke(s_eiemRendererSetSharedMaterials, renderer, params);
  void *actual = Invoke(g_renderer_get_sharedMaterials, renderer);
  if (!EiemManagedObjectArraySame(materials, actual)) {
    if (error)
      strncpy_s(error, errorSize,
                "Unity Renderer material assignment read-back failed",
                _TRUNCATE);
    return false;
  }
  return true;
}

// Expose the existing per-instance baseline while RendererInfo._Init reads
// Renderer.sharedMaterials into its sourceMaterials table. No controller field
// offsets are written and no baseline/ownership record is released here.
static bool EiemExposeSourceMaterialsForInit(void *renderer) {
  if (!renderer || !EiemOnUnityThread()) return false;
  EiemUnityRef originalRef;
  std::vector<size_t> owned;
  bool hasBaseline = false;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  for (const auto &state : s_eiemOverrides) {
    if (state.drawRenderer != renderer || state.drawRendererRef.Target() != renderer ||
        !state.hasMaterials) continue;
    hasBaseline = true;
    void *original = state.originalMaterialsHandle && il2cpp_gchandle_get_target
                         ? il2cpp_gchandle_get_target(state.originalMaterialsHandle) : nullptr;
    originalRef = EiemUnityRef::Capture(original, false);
    owned = state.materialSlots;
    break;
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  if (!hasBaseline) return true; // not modified by EIEM
  if (!originalRef || EiemNativeObjectStatus(renderer) != 1 ||
      !g_renderer_get_sharedMaterials || !s_eiemMaterialClass) {
    Log("[MOD-MATERIAL-SOURCE] cannot expose source slots before init renderer=%p", renderer);
    return false;
  }
  void *original = originalRef.Target();
  void *current = Invoke(g_renderer_get_sharedMaterials, renderer);
  if (!original || !current) {
    Log("[MOD-MATERIAL-SOURCE] source/current slots unavailable renderer=%p", renderer);
    return false;
  }
  const size_t originalCount = EiemManagedArrayLength(original);
  const size_t currentCount = EiemManagedArrayLength(current);
  void **originalItems = (void **)((char *)original + IL2CPP_ARRAY_DATA);
  void **currentItems = (void **)((char *)current + IL2CPP_ARRAY_DATA);
  std::vector<void *> baseline(originalItems, originalItems + originalCount);
  std::vector<void *> live(currentItems, currentItems + currentCount);
  auto restored = EiemRestoreOwnedSlots(live, baseline, owned);
  for (size_t slot : owned) {
    if (slot < restored.size() && restored[slot] && EiemNativeObjectStatus(restored[slot]) != 1) {
      Log("[MOD-MATERIAL-SOURCE] source material unavailable renderer=%p slot=%zu", renderer, slot);
      return false;
    }
  }
  if (restored == live) return true;
  void *array = il2cpp_array_new(s_eiemMaterialClass, restored.size());
  char error[256] = {};
  if (!array) {
    Log("[MOD-MATERIAL-SOURCE] cannot allocate source slots renderer=%p", renderer);
    return false;
  }
  if (!restored.empty())
    memcpy((char *)array + IL2CPP_ARRAY_DATA, restored.data(), restored.size() * sizeof(void *));
  if (!EiemAssignRendererMaterials(renderer, array, error, sizeof(error))) {
    Log("[MOD-MATERIAL-SOURCE] source exposure failed renderer=%p error=%s", renderer, error);
    return false;
  }
  Log("[MOD-MATERIAL-SOURCE] exposed source before controller init renderer=%p ownedSlots=%zu", renderer, owned.size());
  return true;
}

// Resolve resource-local slots against the current instance's shared skeleton.
// This does not change any Renderer; callers prepare everything before writing.


// Resolve a replacement Mesh against the concrete native skeleton instance.
// EIEMESH structural child-index paths are authoritative.  The same path
// table is used for world, UI and NPC renderers; names, source Mesh donors and
// LOD-local slot order are not runtime inputs.
#include "eiem_skin_resolver.h"

static void EiemRememberGameSourceBones(void *renderer, void *bones,
                                        const char *stage) {
  if (!renderer || !bones || !il2cpp_gchandle_new ||
      !il2cpp_gchandle_get_target || !il2cpp_gchandle_free)
    return;

  bool tracked = false;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t observed = EiemFindOverrideLocked(renderer);
  if (observed != SIZE_MAX) {
    const auto &state = s_eiemOverrides[observed];
    void *replacement = state.replacementBonesHandle
                            ? il2cpp_gchandle_get_target(
                                  state.replacementBonesHandle)
                            : nullptr;
    void *baseline = state.originalBonesHandle
                         ? il2cpp_gchandle_get_target(
                               state.originalBonesHandle)
                         : nullptr;
    const bool replacementBinding =
        replacement && EiemManagedObjectArraySame(bones, replacement);
    const bool currentBaseline =
        baseline && EiemManagedObjectArraySame(bones, baseline);
    tracked = !state.restorePending && state.ownsMesh &&
              state.replacementMesh && !replacementBinding &&
              !currentBaseline;
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  if (!tracked) return;

  uint32_t handle = il2cpp_gchandle_new(bones, false);
  if (!handle) return;
  uint32_t previous = 0;
  bool committed = false;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX) {
    auto &state = s_eiemOverrides[index];
    void *replacement = state.replacementBonesHandle
                            ? il2cpp_gchandle_get_target(
                                  state.replacementBonesHandle)
                            : nullptr;
    const bool replacementBinding =
        replacement && EiemManagedObjectArraySame(bones, replacement);
    if (!state.restorePending && state.ownsMesh && state.replacementMesh &&
        !replacementBinding) {
      previous = state.originalBonesHandle;
      state.originalBonesHandle = handle;
      state.hasSkinning = true;
      committed = true;
    }
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  if (!committed) il2cpp_gchandle_free(handle);
  if (previous) il2cpp_gchandle_free(previous);
  if (committed)
    Log("[MOD-SKIN-BASELINE] bones renderer=%p array=%p count=%zu stage=%s",
        renderer, bones, EiemManagedArrayLength(bones),
        stage ? stage : "unknown");
}

static void EiemRememberGameSourceRootBone(void *renderer,
                                           const char *stage) {
  if (!renderer || !g_smr_get_rootBone || !il2cpp_gchandle_new ||
      !il2cpp_gchandle_get_target || !il2cpp_gchandle_free)
    return;
  void *rootBone = Invoke(g_smr_get_rootBone, renderer);
  if (!rootBone) return;

  bool tracked = false;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t observed = EiemFindOverrideLocked(renderer);
  if (observed != SIZE_MAX) {
    const auto &state = s_eiemOverrides[observed];
    void *baseline = state.originalRootBoneHandle
                         ? il2cpp_gchandle_get_target(
                               state.originalRootBoneHandle)
                         : nullptr;
    tracked = !state.restorePending && state.ownsMesh &&
              state.replacementMesh && rootBone != baseline;
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  if (!tracked) return;

  uint32_t handle = il2cpp_gchandle_new(rootBone, false);
  if (!handle) return;
  uint32_t previous = 0;
  bool committed = false;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX) {
    auto &state = s_eiemOverrides[index];
    if (!state.restorePending && state.ownsMesh && state.replacementMesh) {
      previous = state.originalRootBoneHandle;
      state.originalRootBoneHandle = handle;
      state.hasSkinning = true;
      committed = true;
    }
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  if (!committed) il2cpp_gchandle_free(handle);
  if (previous) il2cpp_gchandle_free(previous);
  if (committed)
    Log("[MOD-SKIN-BASELINE] root renderer=%p rootBone=%p stage=%s",
        renderer, rootBone, stage ? stage : "unknown");
}

static void EiemRememberGameSourceSkinningFromArray(void *renderers,
                                                     bool rememberBones,
                                                     bool rememberRoot,
                                                     const char *stage) {
  const size_t count = EiemManagedArrayLength(renderers);
  if (!renderers || count > 8192) return;
  void **items = (void **)((char *)renderers + IL2CPP_ARRAY_DATA);
  for (size_t index = 0; index < count; ++index) {
    void *renderer = items[index];
    if (!renderer) continue;
    if (rememberBones && g_smr_get_bones) {
      void *bones = Invoke(g_smr_get_bones, renderer);
      if (bones) EiemRememberGameSourceBones(renderer, bones, stage);
    }
    if (rememberRoot)
      EiemRememberGameSourceRootBone(renderer, stage);
  }
}

// A later game skin refresh must not shrink an extended palette back to the
// source slots. Reassert only this instance's owned replacement binding.
static void TraceSkinnedMeshSetBones(void *self, void *bones,
                                     void *methodInfo) {
  EiemRegistrationTraceNativeStackContext(
      "SkinnedMeshRenderer.set_bones.entry", self, bones, nullptr,
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0));
  auto original = (TraceSetBonesFn)s_origSkinnedMeshSetBones;
  if (original) original(self, bones, methodInfo);
  if (!self || s_eiemApplyingModMeshAssignment) {
    return;
  }
  void *gameAfterBones = g_smr_get_bones ? Invoke(g_smr_get_bones, self) : nullptr;
  EiemLogSkinSetterTimeline("bones-game", self, nullptr, nullptr, bones,
                            gameAfterBones);

  bool tracked = false;
  uint32_t binding = 0;
  uintptr_t ownerModel = 0;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(self);
  if (index != SIZE_MAX && !s_eiemOverrides[index].restorePending && s_eiemOverrides[index].replacementMesh) {
    tracked = true;
    binding = s_eiemOverrides[index].replacementBonesHandle;
    ownerModel = s_eiemOverrides[index].ownerPrefabInstance;
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  if (tracked && bones)
    EiemRememberGameSourceBones(self, bones, "SkinnedMeshRenderer.set_bones");
  if (tracked && binding && EiemOnUnityThread() && il2cpp_gchandle_get_target) {
    void *expected = il2cpp_gchandle_get_target(binding);
    if (expected && !EiemManagedObjectArraySame(bones, expected)) {
      // The native caller may still be constructing cloth/physics state.
      // Never recurse into set_bones here; rebind after this call stack exits.
      if (ownerModel)
        EiemQueueNativeSkinRefresh(ownerModel);
      else
        EiemQueueModReconcile("native skin refresh without model owner");
    }
  }
  void *finalBones = g_smr_get_bones ? Invoke(g_smr_get_bones, self) : nullptr;
  EiemLogSkinSetterTimeline("bones-final", self, nullptr, nullptr, bones,
                            finalBones);

}


static bool EiemReadBoxedBool(void *getter, void *object, bool *value) {
  if (!getter || !object || !value) return false;
  void *boxed = Invoke(getter, object);
  if (!boxed) return false;
  __try {
    *value = *(bool *)((char *)boxed + 16);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

static bool EiemReadBoxedInt(void *getter, void *object, int32_t *value) {
  if (!getter || !object || !value) return false;
  void *boxed = Invoke(getter, object);
  if (!boxed) return false;
  __try {
    *value = *(int32_t *)((char *)boxed + 16);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

static uint64_t EiemSkinTimelineBoneRefs(void *bones) {
  const size_t count = EiemManagedArrayLength(bones);
  if (!bones || count > 512) return 0;
  void **items = (void **)((char *)bones + IL2CPP_ARRAY_DATA);
  uint64_t hash = 1469598103934665603ull;
  for (size_t index = 0; index < count; ++index) {
    hash ^= (uint64_t)(uintptr_t)items[index];
    hash *= 1099511628211ull;
  }
  return hash;
}

// This probe answers the timing question directly: a later game-owned setter
// is evidence of a post-commit overwrite; no later setter shifts suspicion to
// Unity/game skin-cache invalidation. Keep it small enough for a real run.
static void EiemLogSkinSetterTimeline(const char *event, void *renderer,
                                      void *requestedMesh, void *appliedMesh,
                                      void *incomingBones, void *afterBones) {
  if (!renderer || InterlockedIncrement(&s_eiemSkinTimelineCount) > 240)
    return;
  bool tracked = false;
  void *replacement = nullptr;
  char section[96] = {};
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX) {
    const auto &state = s_eiemOverrides[index];
    tracked = !state.restorePending && state.ownsMesh && state.replacementMesh;
    replacement = state.replacementMesh;
    strncpy_s(section, sizeof(section), state.renderSection, _TRUNCATE);
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  if (!tracked) return;

  const size_t incomingCount = EiemManagedArrayLength(incomingBones);
  const size_t afterCount = EiemManagedArrayLength(afterBones);
  const ULONGLONG now = GetTickCount64();
  const LONG64 commitTick = InterlockedCompareExchange64(
      &s_eiemLastSkinCommitTick, 0, 0);
  const ULONGLONG sinceCommit =
      commitTick > 0 && now >= (ULONGLONG)commitTick
          ? now - (ULONGLONG)commitTick
          : 0;
  const LONG generation = InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  EiemRegistrationTraceNativeStackContext(
      event ? event : "skin-setter", renderer, requestedMesh, incomingBones,
      generation);
  Log("[DEBUG-SKIN-TIMELINE-v1] event=%s tick=%llu gen=%ld tid=%lu "
      "renderer=%p section=%s requestedMesh=%p appliedMesh=%p currentMesh=%p "
      "replacement=%p incomingBones=%p incomingCount=%zu incomingRefs=%016llX "
      "afterBones=%p afterCount=%zu afterRefs=%016llX sinceCommitMs=%llu",
      event ? event : "unknown", (unsigned long long)now, generation,
      (unsigned long)GetCurrentThreadId(), renderer,
      section[0] ? section : "<unknown>", requestedMesh, appliedMesh,
      EiemReadSharedMesh(renderer, "SkinnedMeshRenderer"), replacement,
      incomingBones, incomingCount,
      (unsigned long long)EiemSkinTimelineBoneRefs(incomingBones), afterBones,
      afterCount, (unsigned long long)EiemSkinTimelineBoneRefs(afterBones),
      (unsigned long long)sinceCommit);
}

static uint64_t EiemSkinTimingBoneMatrixHash(void *bones) {
  if (!bones || !g_transform_get_localToWorldMatrix) return 0;
  const size_t count = EiemManagedArrayLength(bones);
  if (count > 512) return 0;
  void **items = (void **)((char *)bones + IL2CPP_ARRAY_DATA);
  uint64_t hash = 1469598103934665603ULL;
  for (size_t index = 0; index < count; ++index) {
    const uintptr_t identity = (uintptr_t)items[index];
    hash ^= (uint64_t)identity;
    hash *= 1099511628211ULL;
    if (!items[index]) continue;
    __try {
      void *boxed = Invoke(g_transform_get_localToWorldMatrix, items[index]);
      if (!boxed) continue;
      const unsigned char *bytes = (const unsigned char *)boxed + 16;
      for (size_t byte = 0; byte < sizeof(float) * 16; ++byte) {
        hash ^= bytes[byte];
        hash *= 1099511628211ULL;
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      hash ^= 0xA5;
      hash *= 1099511628211ULL;
    }
  }
  return hash;
}

static uint64_t EiemSkinTimingTransformMatrixHash(void *transform) {
  if (!transform || !g_transform_get_localToWorldMatrix) return 0;
  __try {
    void *boxed = Invoke(g_transform_get_localToWorldMatrix, transform);
    if (!boxed) return 0;
    const unsigned char *bytes = (const unsigned char *)boxed + 16;
    uint64_t hash = 1469598103934665603ULL;
    for (size_t byte = 0; byte < sizeof(float) * 16; ++byte) {
      hash ^= bytes[byte];
      hash *= 1099511628211ULL;
    }
    return hash;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

// Compare only Transform objects shared by two Renderer palettes.  A clothing
// Mesh may contain slots supplied by several source Meshes, so comparing the
// whole palette hash would mix its extra/physical slots with the torso slots.
// This observation uses Transform identity, never names or target-Lod indices.
static uint64_t EiemSkinTimingSharedBoneMatrixHash(void *left, void *right,
                                                   size_t *sharedCount) {
  if (sharedCount) *sharedCount = 0;
  if (!left || !right || !g_transform_get_localToWorldMatrix) return 0;
  const size_t leftCount = EiemManagedArrayLength(left);
  const size_t rightCount = EiemManagedArrayLength(right);
  if (!leftCount || !rightCount || leftCount > 512 || rightCount > 512)
    return 0;
  void **leftItems = (void **)((char *)left + IL2CPP_ARRAY_DATA);
  void **rightItems = (void **)((char *)right + IL2CPP_ARRAY_DATA);
  uint64_t hash = 1469598103934665603ULL;
  size_t matches = 0;
  for (size_t rightIndex = 0; rightIndex < rightCount; ++rightIndex) {
    void *transform = rightItems[rightIndex];
    if (!transform) continue;
    bool found = false;
    for (size_t leftIndex = 0; leftIndex < leftCount; ++leftIndex) {
      if (leftItems[leftIndex] == transform) {
        found = true;
        break;
      }
    }
    if (!found) continue;
    ++matches;
    const uintptr_t identity = (uintptr_t)transform;
    hash ^= (uint64_t)identity;
    hash *= 1099511628211ULL;
    __try {
      void *boxed = Invoke(g_transform_get_localToWorldMatrix, transform);
      if (!boxed) continue;
      const unsigned char *bytes = (const unsigned char *)boxed + 16;
      for (size_t byte = 0; byte < sizeof(float) * 16; ++byte) {
        hash ^= bytes[byte];
        hash *= 1099511628211ULL;
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      hash ^= 0xA5;
      hash *= 1099511628211ULL;
    }
  }
  if (sharedCount) *sharedCount = matches;
  return matches ? hash : 0;
}

// Hash only the Transform objects present in the clothing palette but absent
// from the body's palette. These are the cloth/skirt/other clothing-specific
// slots that the shared-body probe intentionally excludes. This observation
// uses Transform identity, never names or target-LOD indices, and never writes
// to Unity state.
static uint64_t EiemSkinTimingClothOnlyBoneMatrixHash(void *body,
                                                      void *cloth,
                                                      size_t *clothOnlyCount) {
  if (clothOnlyCount) *clothOnlyCount = 0;
  if (!body || !cloth || !g_transform_get_localToWorldMatrix) return 0;
  const size_t bodyCount = EiemManagedArrayLength(body);
  const size_t clothCount = EiemManagedArrayLength(cloth);
  if (!bodyCount || !clothCount || bodyCount > 512 || clothCount > 512)
    return 0;
  void **bodyItems = (void **)((char *)body + IL2CPP_ARRAY_DATA);
  void **clothItems = (void **)((char *)cloth + IL2CPP_ARRAY_DATA);
  uint64_t hash = 1469598103934665603ULL;
  size_t matches = 0;
  for (size_t clothIndex = 0; clothIndex < clothCount; ++clothIndex) {
    void *transform = clothItems[clothIndex];
    if (!transform) continue;
    bool shared = false;
    for (size_t bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex) {
      if (bodyItems[bodyIndex] == transform) {
        shared = true;
        break;
      }
    }
    if (shared) continue;
    ++matches;
    const uintptr_t identity = (uintptr_t)transform;
    hash ^= (uint64_t)identity;
    hash *= 1099511628211ULL;
    __try {
      void *boxed = Invoke(g_transform_get_localToWorldMatrix, transform);
      if (!boxed) {
        hash ^= 0xD1;
        hash *= 1099511628211ULL;
        continue;
      }
      const unsigned char *bytes = (const unsigned char *)boxed + 16;
      for (size_t byte = 0; byte < sizeof(float) * 16; ++byte) {
        hash ^= bytes[byte];
        hash *= 1099511628211ULL;
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      hash ^= 0xA7;
      hash *= 1099511628211ULL;
    }
  }
  if (clothOnlyCount) *clothOnlyCount = matches;
  return matches ? hash : 0;
}

// Correlate Endfield's parallel HG component path with the ordinary
// SkinnedMeshRenderer path on the exact same GameObject. This is limited to
// the existing cold/F10 probe windows. HGMeshRendererData has a native-facing
// value-type ABI, so this pass intentionally avoids get_data until its layout
// has been collected by the metadata probe.
static void EiemLogHgRendererCensus(void *model, void *root, LONG transaction,
                                    const char *phase) {
  if (!kEiemEnableCustomSkinPipelineObservation || !model || !root ||
      !g_hgMeshRendererClass || !g_skinnedMeshRendererClass ||
      !g_gameObject_GetComponentsInChildren || !g_gameObject_GetComponent ||
      !g_component_get_gameObject || !il2cpp_class_get_type ||
      !il2cpp_type_get_object)
    return;

  void *hgType = il2cpp_class_get_type(g_hgMeshRendererClass);
  void *hgTypeObject = hgType ? il2cpp_type_get_object(hgType) : nullptr;
  void *smrType = il2cpp_class_get_type(g_skinnedMeshRendererClass);
  void *smrTypeObject = smrType ? il2cpp_type_get_object(smrType) : nullptr;
  if (!hgTypeObject || !smrTypeObject) return;

  bool includeInactive = true;
  void *childrenParams[] = {hgTypeObject, &includeInactive};
  void *array = Invoke(g_gameObject_GetComponentsInChildren, model,
                       childrenParams);
  const size_t count = EiemManagedArrayLength(array);
  if (!array || count > 4096) {
    Log("[HG-CENSUS-v1] transaction=%ld phase=%s owner=%p count=invalid",
        transaction, phase ? phase : "unknown", model);
    return;
  }

  void **items = (void **)((char *)array + IL2CPP_ARRAY_DATA);
  size_t paired = 0;
  size_t targets = 0;
  for (size_t index = 0; index < count && index < 256; ++index) {
    void *hg = items[index];
    if (!hg) continue;
    char path[768] = {};
    EiemBuildRelativeRendererPath(root, hg, path, sizeof(path));
    void *gameObject = Invoke(g_component_get_gameObject, hg);
    void *pairedSmr = nullptr;
    if (gameObject) {
      void *componentParams[] = {smrTypeObject};
      pairedSmr = Invoke(g_gameObject_GetComponent, gameObject,
                         componentParams);
    }
    if (pairedSmr) ++paired;
    const bool target =
        path[0] && (strstr(path, "body_01") || strstr(path, "cloth_01") ||
                    strstr(path, "cloth_02"));
    if (!target) continue;
    ++targets;
    void *mesh = pairedSmr
                     ? EiemReadSharedMesh(pairedSmr, "SkinnedMeshRenderer")
                     : nullptr;
    void *bones = pairedSmr && g_smr_get_bones
                      ? Invoke(g_smr_get_bones, pairedSmr)
                      : nullptr;
    bool tracked = false;
    if (pairedSmr) {
      AcquireSRWLockShared(&s_eiemOverrideLock);
      tracked = EiemFindOverrideLocked(pairedSmr) != SIZE_MAX;
      ReleaseSRWLockShared(&s_eiemOverrideLock);
    }
    Log("[HG-CENSUS-v1] transaction=%ld phase=%s owner=%p hg=%p "
        "path=%s gameObject=%p pairedSmr=%p tracked=%d mesh=%p bones=%zu",
        transaction, phase ? phase : "unknown", model, hg, path, gameObject,
        pairedSmr, tracked ? 1 : 0, mesh, EiemManagedArrayLength(bones));
  }
  Log("[HG-CENSUS-v1] transaction=%ld phase=%s owner=%p total=%zu "
      "paired=%zu targetPaths=%zu",
      transaction, phase ? phase : "unknown", model, count, paired, targets);
}

// Enumerate the complete Unity Renderer hierarchy for the model owner.  The
// existing POSE-CENSUS only asks for SkinnedMeshRenderer, so it cannot rule out
// a parallel MeshRenderer/custom Renderer being the object actually submitted
// for a visible clothing draw.  This pass is read-only and bounded; it does
// not register, replace, enable, disable, or otherwise touch any component.
static void EiemLogAllRendererCensus(void *model, void *root, LONG transaction,
                                     const char *phase) {
  if (!kEiemEnableCustomSkinPipelineObservation || !model || !root ||
      !g_rendererClass || !g_gameObject_GetComponentsInChildren ||
      !il2cpp_class_get_type || !il2cpp_type_get_object)
    return;
  __try {
    void *type = il2cpp_class_get_type(g_rendererClass);
    void *typeObject = type ? il2cpp_type_get_object(type) : nullptr;
    if (!typeObject) return;
    bool includeInactive = true;
    void *params[] = {typeObject, &includeInactive};
    void *array = Invoke(g_gameObject_GetComponentsInChildren, model, params);
    const size_t count = EiemManagedArrayLength(array);
    if (!array || count > 8192) {
      Log("[POSE-RENDERER-CENSUS-v1] transaction=%ld phase=%s owner=%p "
          "root=%p count=invalid",
          transaction, phase ? phase : "unknown", model, root);
      return;
    }
    void **items = (void **)((char *)array + IL2CPP_ARRAY_DATA);
    size_t logged = 0;
    size_t targetCount = 0;
    size_t visibleCount = 0;
    for (size_t index = 0; index < count && logged < 512; ++index) {
      void *renderer = items[index];
      if (!renderer) continue;
      char path[768] = {};
      EiemBuildRelativeRendererPath(root, renderer, path, sizeof(path));
      const char *rendererType = "Renderer";
      void *mesh = EiemReadLodRendererMesh(renderer, &rendererType);
      char rendererClass[128] = {};
      if (il2cpp_object_get_class && il2cpp_class_get_name) {
        void *klass = il2cpp_object_get_class(renderer);
        const char *name = klass ? il2cpp_class_get_name(klass) : nullptr;
        if (name) strncpy_s(rendererClass, sizeof(rendererClass), name,
                            _TRUNCATE);
      }
      bool enabled = false;
      bool visible = false;
      const bool enabledRead = EiemReadRendererEnabled(renderer, &enabled);
      const bool visibleRead = EiemReadRendererVisible(renderer, &visible);
      if (visibleRead && visible) ++visibleCount;
      const bool target = path[0] &&
                          (strstr(path, "body_01") ||
                           strstr(path, "cloth_01") ||
                           strstr(path, "cloth_02"));
      if (target) ++targetCount;
      // Keep the output focused on the model's clothing/body path and any
      // Renderer that is currently visible.  Inactive unrelated effects are
      // still represented by the summary count above.
      if (!target && !(visibleRead && visible)) continue;
      bool tracked = false;
      AcquireSRWLockShared(&s_eiemOverrideLock);
      tracked = EiemFindOverrideLocked(renderer) != SIZE_MAX;
      ReleaseSRWLockShared(&s_eiemOverrideLock);
      char meshDescription[384] = {};
      TraceDescribeObject(mesh, meshDescription, sizeof(meshDescription));
      Log("[POSE-RENDERER-CENSUS-v1] transaction=%ld phase=%s owner=%p "
          "index=%zu renderer=%p class=%s type=%s tracked=%d path=%s "
          "mesh=%p meshDesc=%s enabled=%s visible=%s",
          transaction, phase ? phase : "unknown", model, index, renderer,
          rendererClass[0] ? rendererClass : "<unknown>", rendererType,
          tracked ? 1 : 0, path[0] ? path : "<root>", mesh,
          meshDescription[0] ? meshDescription : "<unknown>",
          enabledRead ? (enabled ? "1" : "0") : "?",
          visibleRead ? (visible ? "1" : "0") : "?");
      ++logged;
    }
    Log("[POSE-RENDERER-CENSUS-v1] transaction=%ld phase=%s owner=%p "
        "root=%p count=%zu targetCount=%zu visibleCount=%zu logged=%zu",
        transaction, phase ? phase : "unknown", model, root, count,
        targetCount, visibleCount, logged);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    Log("[POSE-RENDERER-CENSUS-v1] transaction=%ld phase=%s owner=%p "
        "read=exception=0x%08lX",
        transaction, phase ? phase : "unknown", model, GetExceptionCode());
  }
}

static bool EiemSkinTargetFamily(const char *section, const char *family) {
  return section && family && strstr(section, family) != nullptr;
}

static int EiemSkinTargetRank(const char *section) {
  if (!section) return 99;
  if (strstr(section, "_lod0") != nullptr) return 0;
  if (strstr(section, "_lod1") != nullptr) return 1;
  if (strstr(section, "_lod2") != nullptr) return 2;
  if (strstr(section, "_lod3") != nullptr) return 3;
  return 10;
}

// Select one model owner and one authored family entry per body/cloth pair.
// Replacement state can contain many NPCs and four LOD sections; observing
// all of them was the source of the previous startup/frame spikes.  The
// selected pointers are evidence-only and never used for binding decisions.
static bool EiemSelectSkinTargetStates(
    LONG transaction, std::vector<EiemRenderOverrideState> *out,
    void **selectedOwner) {
  if (out) out->clear();
  if (selectedOwner) *selectedOwner = nullptr;
  if (!out) return false;
  struct Candidate {
    uintptr_t owner = 0;
    int families = 0;
    int bestRank = 99;
    bool body = false, cloth01 = false, cloth02 = false;
  };
  std::vector<Candidate> candidates;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  for (const auto &state : s_eiemOverrides) {
    if (state.restorePending || !state.renderer ||
        !EiemModEquals(state.rendererType, "SkinnedMeshRenderer") ||
        !state.ownerPrefabInstance)
      continue;
    const bool body = EiemSkinTargetFamily(state.renderSection, "body_01");
    const bool cloth01 = EiemSkinTargetFamily(state.renderSection, "cloth_01");
    const bool cloth02 = EiemSkinTargetFamily(state.renderSection, "cloth_02");
    if (!body && !cloth01 && !cloth02) continue;
    auto it = std::find_if(candidates.begin(), candidates.end(),
                           [&](const Candidate &c) {
                             return c.owner == state.ownerPrefabInstance;
                           });
    if (it == candidates.end()) {
      candidates.push_back({state.ownerPrefabInstance, 0, 99, body, cloth01,
                            cloth02});
    } else {
      it->body = it->body || body;
      it->cloth01 = it->cloth01 || cloth01;
      it->cloth02 = it->cloth02 || cloth02;
    }
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  if (candidates.empty()) return false;
  for (auto &candidate : candidates) {
    candidate.families = (candidate.body ? 1 : 0) +
                         (candidate.cloth01 ? 1 : 0) +
                         (candidate.cloth02 ? 1 : 0);
  }
  void *preferred = s_eiemSkinTargetOwner;
  auto chosen = std::find_if(candidates.begin(), candidates.end(),
                             [&](const Candidate &c) {
                               return preferred &&
                                      c.owner == (uintptr_t)preferred &&
                                      c.families == 3;
                             });
  if (chosen == candidates.end()) {
    chosen = std::max_element(
        candidates.begin(), candidates.end(),
        [](const Candidate &left, const Candidate &right) {
          return left.families < right.families;
        });
  }
  if (chosen == candidates.end() || chosen->families < 3) {
    Log("[CPU-SKIN-TARGET-v1] transaction=%ld selected=0 owners=%zu "
        "reason=no-owner-with-body-cloth01-cloth02",
        transaction, candidates.size());
    return false;
  }
  s_eiemSkinTargetOwner = (void *)chosen->owner;
  if (selectedOwner) *selectedOwner = s_eiemSkinTargetOwner;

  // Choose the lowest authored LOD for each family.  This is a stable
  // observation key, not a LOD policy and does not affect the game.
  const char *families[] = {"body_01", "cloth_01", "cloth_02"};
  for (const char *family : families) {
    EiemRenderOverrideState best = {};
    int bestRank = 99;
    bool found = false;
    AcquireSRWLockShared(&s_eiemOverrideLock);
    for (const auto &state : s_eiemOverrides) {
      if (state.restorePending || !state.renderer ||
          state.ownerPrefabInstance != chosen->owner ||
          !EiemModEquals(state.rendererType, "SkinnedMeshRenderer") ||
          !EiemSkinTargetFamily(state.renderSection, family))
        continue;
      const int rank = EiemSkinTargetRank(state.renderSection);
      if (!found || rank < bestRank) {
        best = state;
        bestRank = rank;
        found = true;
      }
    }
    ReleaseSRWLockShared(&s_eiemOverrideLock);
    if (found) out->push_back(best);
  }
  Log("[CPU-SKIN-TARGET-v1] transaction=%ld selected=1 owner=%p "
      "families=%zu body=%d cloth01=%d cloth02=%d",
      transaction, s_eiemSkinTargetOwner, out->size(),
      chosen->body ? 1 : 0, chosen->cloth01 ? 1 : 0,
      chosen->cloth02 ? 1 : 0);
  return out->size() == 3;
}

// Compare the exact same cloth_02 Renderers immediately after replay and on
// the next Unity window cycle. If the first sample is healthy and the second
// one is ground-bound, a later animation/physics/LOD write is the cause. If
// both samples are already wrong, the commit boundary itself is too early or
// its source transform state is not complete. This function only reads Unity
// state and is intentionally capped to avoid turning diagnostics into a
// frame-time spike.
static void EiemLogSkinTimingProbe(const char *phase, LONG transaction) {
  // The full probe reads vertices, weights and bindposes. The binding-only
  // mode below reads only palette/root identities and matrix fingerprints.
  // Both modes are evidence-only and opt in independently.
  if (!kEiemEnableSkinDiagnostics && !kEiemEnableSkinBindingDiagnostics)
    return;
  if (!EiemOnUnityThread()) {
    Log("[SKIN-TIMING] transaction=%ld phase=%s skipped=not-unity-thread "
        "tid=%lu unityTid=%lu",
        transaction, phase ? phase : "unknown",
        (unsigned long)GetCurrentThreadId(),
        (unsigned long)s_eiemUnityThreadId);
    return;
  }
  std::vector<EiemRenderOverrideState> targets;
  void *targetOwner = nullptr;
  EiemSelectSkinTargetStates(transaction, &targets, &targetOwner);

  if (targets.empty()) {
    Log("[SKIN-TIMING] transaction=%ld phase=%s samples=0 tid=%lu",
        transaction, phase ? phase : "unknown",
        (unsigned long)GetCurrentThreadId());
    return;
  }
  if (kEiemEnableNativePhysicsObservation)
    EiemPhysicsOrderProbeLogSinceRender(transaction, phase);
  if (!kEiemEnableSkinDiagnostics && kEiemEnableSkinBindingDiagnostics) {
    size_t sampled = 0;
    for (const auto &state : targets) {
      bool enabled = false;
      bool visible = false;
      const bool enabledRead = EiemReadRendererEnabled(state.drawRenderer,
                                                        &enabled);
      const bool visibleRead = EiemReadRendererVisible(state.drawRenderer,
                                                        &visible);
      void *mesh = EiemReadSharedMesh(state.renderer, "SkinnedMeshRenderer");
      void *bones = g_smr_get_bones ? Invoke(g_smr_get_bones, state.renderer)
                                    : nullptr;
      void *rootBone = g_smr_get_rootBone
                           ? Invoke(g_smr_get_rootBone, state.renderer)
                           : nullptr;
      void *skinningRoot = g_smr_get_skinningRoot
                               ? Invoke(g_smr_get_skinningRoot, state.renderer)
                               : nullptr;
      bool updateWhenOffscreen = false;
      bool forceMatrixPerRender = false;
      bool skinnedMotionVectors = false;
      const bool updateWhenOffscreenRead = EiemReadBoxedBool(
          g_smr_get_updateWhenOffscreen, state.renderer,
          &updateWhenOffscreen);
      const bool forceMatrixPerRenderRead = EiemReadBoxedBool(
          g_smr_get_forceMatrixRecalculationPerRender, state.renderer,
          &forceMatrixPerRender);
      const bool skinnedMotionVectorsRead = EiemReadBoxedBool(
          g_smr_get_skinnedMotionVectors, state.renderer,
          &skinnedMotionVectors);
      void *bodyBones = nullptr;
      if (strstr(state.renderSection, "cloth_") != nullptr) {
        for (const auto &body : targets) {
          if (body.ownerPrefabInstance != state.ownerPrefabInstance ||
              strstr(body.renderSection, "body_01") == nullptr)
            continue;
          bodyBones = g_smr_get_bones
                          ? Invoke(g_smr_get_bones, body.renderer)
                          : nullptr;
          if (bodyBones) break;
        }
      }
      size_t sharedCount = 0;
      size_t clothOnlyCount = 0;
      const uint64_t sharedMatrix =
          bodyBones ? EiemSkinTimingSharedBoneMatrixHash(
                          bodyBones, bones, &sharedCount)
                    : 0;
      const uint64_t clothOnlyMatrix =
          bodyBones ? EiemSkinTimingClothOnlyBoneMatrixHash(
                          bodyBones, bones, &clothOnlyCount)
                    : 0;
      const EiemSkinProbe::WorldBounds bounds =
          EiemSkinProbe::ReadRendererBounds(state.renderer);
      EiemPhysicsTargetAssociation physicsAssociation;
      if (kEiemEnableNativePhysicsObservation && bones) {
        const size_t count = EiemManagedArrayLength(bones);
        if (count && count <= 16384) {
          void **items = (void **)((char *)bones + IL2CPP_ARRAY_DATA);
          physicsAssociation =
              EiemPhysicsAssociateTargetBones(items, count);
        }
      }
      Log("[POSE-BIND-WINDOW-v1] transaction=%ld phase=%s owner=%p "
          "renderer=%p section=%s enabled=%s visible=%s currentMesh=%p "
          "expectedMesh=%p bones=%p count=%zu refs=%016llX matrixRefs=%016llX "
          "rootBone=%p rootMatrix=%016llX sharedCount=%zu "
          "sharedMatrix=%016llX clothOnlyCount=%zu clothOnlyMatrix=%016llX "
          "skinningRoot=%p updateWhenOffscreen=%s "
          "forceMatrixPerRender=%s skinnedMotionVectors=%s "
          "boundsRead=%d boundsCenterY=%.3f boundsMaxY=%.3f",
          transaction, phase ? phase : "unknown",
          (void *)state.ownerPrefabInstance, state.renderer,
          state.renderSection, enabledRead ? (enabled ? "1" : "0") : "?",
          visibleRead ? (visible ? "1" : "0") : "?", mesh,
          state.replacementMesh, bones, EiemManagedArrayLength(bones),
          (unsigned long long)EiemSkinTimelineBoneRefs(bones),
          (unsigned long long)EiemSkinTimingBoneMatrixHash(bones), rootBone,
          (unsigned long long)EiemSkinTimingTransformMatrixHash(rootBone),
          sharedCount, (unsigned long long)sharedMatrix, clothOnlyCount,
          (unsigned long long)clothOnlyMatrix, skinningRoot,
          updateWhenOffscreenRead
              ? (updateWhenOffscreen ? "1" : "0")
              : "?",
          forceMatrixPerRenderRead
              ? (forceMatrixPerRender ? "1" : "0")
              : "?",
          skinnedMotionVectorsRead
              ? (skinnedMotionVectors ? "1" : "0")
              : "?",
          bounds.read ? 1 : 0,
          bounds.read ? bounds.CenterY() : 0.0f,
          bounds.read ? bounds.maxY : 0.0f);
      Log("[CPU-SKIN-PHYSICS-v1] transaction=%ld phase=%s owner=%p "
          "section=%s bones=%p matchedTransforms=%zu matchedTeams=%zu "
          "latestPhysicsSeq=%llu team0=%d clothProcess0=%p",
          transaction, phase ? phase : "unknown", targetOwner,
          state.renderSection, bones, physicsAssociation.matchedTransforms,
          physicsAssociation.matchedTeams,
          (unsigned long long)physicsAssociation.latestSequence,
          physicsAssociation.matchedTeams ? physicsAssociation.teamIds[0] : -1,
          physicsAssociation.matchedTeams
              ? (void *)physicsAssociation.clothProcesses[0]
              : nullptr);
      // Only the second post-F10 sample performs the expensive vertex/weight
      // calculation.  Cold start and the first window retain the cheap
      // palette/root fingerprints above, so entering the game stays bounded.
      if (transaction > 0 && phase &&
          strcmp(phase, "post-window-2") == 0) {
        const LONG64 started = EiemPerfNow();
        const EiemSkinProbe::Result result =
            EiemSkinProbe::Measure(state.renderer);
        EiemSkinProbe::LogResult("[CPU-SKIN-C-v1]", result);
        Log("[CPU-SKIN-C-v1] transaction=%ld phase=%s owner=%p section=%s "
            "measureMs=%.2f physicsCompletionSeq=%llu",
            transaction, phase, targetOwner, state.renderSection,
            EiemPerfMilliseconds(EiemPerfNow() - started),
            (unsigned long long)EiemPhysicsOrderCompletionSequence());
      }
      ++sampled;
    }
      Log("[POSE-BIND-WINDOW-v1] transaction=%ld phase=%s samples=%zu",
        transaction, phase ? phase : "unknown", sampled);

    // Target selection above already binds body/cloth01/cloth02 to one owner.
    // Do not census the complete hierarchy here: that was the major startup
    // stall and it did not improve the CPU ownership evidence.
    Log("[CPU-SKIN-D-v1] transaction=%ld phase=%s owner=%p targets=%zu "
        "boundary=last-proven-public-renderer-state",
        transaction, phase ? phase : "unknown", targetOwner, targets.size());
    return;
  }
  size_t measured = 0;
  for (const auto &state : targets) {
    bool enabled = false;
    bool visible = false;
    const bool enabledRead = EiemReadRendererEnabled(state.drawRenderer,
                                                      &enabled);
    const bool visibleRead = EiemReadRendererVisible(state.drawRenderer,
                                                      &visible);
    void *mesh = EiemReadSharedMesh(state.renderer, "SkinnedMeshRenderer");
    void *bones = g_smr_get_bones ? Invoke(g_smr_get_bones, state.renderer)
                                  : nullptr;
    void *rootBone = g_smr_get_rootBone
                         ? Invoke(g_smr_get_rootBone, state.renderer)
                         : nullptr;
    void *skinningRoot = g_smr_get_skinningRoot
                             ? Invoke(g_smr_get_skinningRoot, state.renderer)
                             : nullptr;
    bool updateWhenOffscreen = false;
    bool forceMatrixPerRender = false;
    bool skinnedMotionVectors = false;
    const bool updateWhenOffscreenRead = EiemReadBoxedBool(
        g_smr_get_updateWhenOffscreen, state.renderer, &updateWhenOffscreen);
    const bool forceMatrixPerRenderRead = EiemReadBoxedBool(
        g_smr_get_forceMatrixRecalculationPerRender, state.renderer,
        &forceMatrixPerRender);
    const bool skinnedMotionVectorsRead = EiemReadBoxedBool(
        g_smr_get_skinnedMotionVectors, state.renderer,
        &skinnedMotionVectors);
    void *rendererTransform = g_component_get_transform
                                  ? Invoke(g_component_get_transform,
                                           state.renderer)
                                  : nullptr;
    const size_t boneCount = EiemManagedArrayLength(bones);
    const uint64_t matrixHash = EiemSkinTimingBoneMatrixHash(bones);
    const uint64_t rootMatrixHash =
        EiemSkinTimingTransformMatrixHash(rootBone);
    const uint64_t rendererMatrixHash =
        EiemSkinTimingTransformMatrixHash(rendererTransform);
    void *bodyBones = nullptr;
    if (strstr(state.renderSection, "cloth_") != nullptr) {
      for (const auto &body : targets) {
        if (body.ownerPrefabInstance != state.ownerPrefabInstance ||
            strstr(body.renderSection, "body_01") == nullptr)
          continue;
        bodyBones = g_smr_get_bones
                        ? Invoke(g_smr_get_bones, body.renderer)
                        : nullptr;
        if (bodyBones) break;
      }
    }
    size_t sharedCount = 0;
    const uint64_t sharedMatrixHash =
        bodyBones ? EiemSkinTimingSharedBoneMatrixHash(bodyBones, bones,
                                                        &sharedCount)
                  : 0;
    size_t clothOnlyCount = 0;
    const uint64_t clothOnlyMatrixHash =
        bodyBones ? EiemSkinTimingClothOnlyBoneMatrixHash(
                        bodyBones, bones, &clothOnlyCount)
                  : 0;
    char tag[768] = {};
    snprintf(tag, sizeof(tag),
             "[SKIN-TIMING] transaction=%ld phase=%s tid=%lu renderer=%p "
             "owner=%p section=%s enabled=%s visible=%s currentMesh=%p "
             "expectedMesh=%p bones=%p count=%zu refs=%016llX "
             "matrixRefs=%016llX rootBone=%p rootMatrix=%016llX "
             "rendererMatrix=%016llX skinningRoot=%p "
             "updateWhenOffscreen=%s forceMatrixPerRender=%s "
             "skinnedMotionVectors=%s",
             transaction, phase ? phase : "unknown",
             (unsigned long)GetCurrentThreadId(), state.renderer,
             (void *)state.ownerPrefabInstance, state.renderSection,
             enabledRead ? (enabled ? "1" : "0") : "?",
             visibleRead ? (visible ? "1" : "0") : "?", mesh,
             state.replacementMesh, bones, boneCount,
             (unsigned long long)EiemSkinTimelineBoneRefs(bones),
             (unsigned long long)matrixHash, rootBone,
             (unsigned long long)rootMatrixHash,
             (unsigned long long)rendererMatrixHash, skinningRoot,
             updateWhenOffscreenRead
                 ? (updateWhenOffscreen ? "1" : "0")
                 : "?",
             forceMatrixPerRenderRead
                 ? (forceMatrixPerRender ? "1" : "0")
                 : "?",
             skinnedMotionVectorsRead
                 ? (skinnedMotionVectors ? "1" : "0")
                 : "?");
    if (bodyBones) {
      const size_t used = strnlen(tag, sizeof(tag));
      if (used < sizeof(tag))
        snprintf(tag + used, sizeof(tag) - used,
                 " sharedBodyCount=%zu sharedBodyMatrix=%016llX"
                 " clothOnlyCount=%zu clothOnlyMatrix=%016llX",
                 sharedCount, (unsigned long long)sharedMatrixHash,
                 clothOnlyCount, (unsigned long long)clothOnlyMatrixHash);
    }
    const EiemSkinProbe::Result result =
        EiemSkinProbe::Measure(state.renderer);
    EiemSkinProbe::LogResult(tag, result);
    ++measured;
  }
  Log("[SKIN-TIMING] transaction=%ld phase=%s samples=%zu tid=%lu",
      transaction, phase ? phase : "unknown", measured,
      (unsigned long)GetCurrentThreadId());
}

static void EiemArmSkinTimingProbe(LONG transaction) {
  if (!kEiemEnableSkinTimingProbe && !kEiemEnableSkinDiagnostics &&
      !kEiemEnableSkinBindingDiagnostics)
    return;
  InterlockedExchange(&s_eiemSkinTimingProbePending, transaction);
  s_eiemSkinTargetOwner = nullptr;
  InterlockedExchange(&s_eiemSkinTargetTransaction, transaction);
  InterlockedExchange(&s_eiemSkinTimingProbeStage, 0);
  InterlockedExchange(&s_eiemSkinNativeTrackedCalls, 0);
  InterlockedExchange(&s_eiemSkinNativeUntrackedCalls, 0);
  InterlockedExchange(&s_eiemSkinCaptureRequestCalls, 0);
  InterlockedExchange(&s_eiemGpuClothObservationCalls, 0);
  InterlockedExchange(&s_eiemGpuClothEventSequence, 0);
  InterlockedExchange(&s_eiemSkinBufferBindingCalls, 0);
  // Both cold and F10 evidence need two windows. F10 performs its one full
  // CPU skin calculation in window 2; cold start remains fingerprints-only.
  // GPU submission hooks remain disabled.
  InterlockedExchange(&s_eiemSkinTimingProbeTicks, 2);
  if (g_gameHwnd && IsWindow(g_gameHwnd))
    SetTimer(g_gameHwnd, kEiemSkinTimingProbeTimer,
             transaction < 0 ? 250 : 32, nullptr);
}

static void EiemMaybeArmColdSkinTimingProbe() {
  if (!kEiemEnableSkinDiagnostics && !kEiemEnableSkinBindingDiagnostics) return;
  if (InterlockedCompareExchange(&s_eiemSkinTimingProbeSequence, 0, 0) != 0)
    return;
  if (InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0) != 0)
    return;
  std::vector<EiemRenderOverrideState> targets;
  void *owner = nullptr;
  if (!EiemSelectSkinTargetStates(-1, &targets, &owner) || !owner) return;
  if (InterlockedCompareExchange(&s_eiemSkinColdProbeArmed, 1, 0) != 0)
    return;
  // A negative transaction is reserved for the one cold-start window.  It
  // shares the same bounded timer path as F10, but is labelled separately.
  EiemArmSkinTimingProbe(-1);
  Log("[SKIN-TIMING] cold-start probe armed");
}

static void EiemRunSkinTimingProbe() {
  if (!kEiemEnableSkinTimingProbe && !kEiemEnableSkinDiagnostics &&
      !kEiemEnableSkinBindingDiagnostics)
    return;
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!transaction) return;
  const LONG stage = InterlockedIncrement(&s_eiemSkinTimingProbeStage);
  char phase[64] = {};
  if (transaction < 0)
    snprintf(phase, sizeof(phase), "cold-start-post-window-%ld", stage);
  else
    snprintf(phase, sizeof(phase), "post-window-%ld", stage);
  EiemLogSkinTimingProbe(phase, transaction);
  const LONG remaining = InterlockedDecrement(&s_eiemSkinTimingProbeTicks);
  if (remaining > 0 && g_gameHwnd && IsWindow(g_gameHwnd)) {
    SetTimer(g_gameHwnd, kEiemSkinTimingProbeTimer,
             transaction < 0 ? 500 : 32, nullptr);
  } else {
    InterlockedExchange(&s_eiemSkinTimingProbePending, 0);
  }
}


// Stable only within one process. This is an evidence fingerprint for the
// order of a Renderer bone palette; it is never used as an identity or as a
// binding decision.


static bool EiemRendererEligibleForRule(void *renderer, void *drawRenderer,
                                        bool includeGameHidden) {
  if (!drawRenderer) return false;

  bool active = true;
  const LONG generation = InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  const bool activeRead =
      g_component_get_gameObject && g_gameObject_get_activeInHierarchy &&
      EiemReadBoxedBool(
           g_gameObject_get_activeInHierarchy,
           Invoke(g_component_get_gameObject, drawRenderer), &active);
  if (activeRead && !active) {
    if (includeGameHidden) {
      EiemRegistrationTraceEligibility(
          renderer, drawRenderer, generation, active, true, false, false,
          false, true, "inactive-authored-lod");
      return true;
    }
    EiemRegistrationTraceEligibility(
        renderer, drawRenderer, generation, active, true, false, false, false,
        false, "inactive");
    return false;
  }

  bool enabled = true;
  const bool enabledRead = EiemReadRendererEnabled(drawRenderer, &enabled);
  bool forceRenderingOff = false;
  const bool forceRenderingOffRead =
      EiemReadRendererForceRenderingOff(drawRenderer, &forceRenderingOff);

  // Unity's LODGroup may leave Renderer.enabled true and set only
  // forceRenderingOff while preparing or selecting a different LOD. Such a
  // Renderer is never a current source hit and must not acquire a Mesh rule.
  // This state is owned by the game, so handling=skip does not override it.
  if (forceRenderingOffRead && forceRenderingOff) {
    if (includeGameHidden) {
      EiemRegistrationTraceEligibility(
          renderer, drawRenderer, generation, active, enabled, enabledRead,
          forceRenderingOff, forceRenderingOffRead, true,
          "force-off-authored-lod");
      return true;
    }
    EiemRegistrationTraceEligibility(
        renderer, drawRenderer, generation, active, enabled, enabledRead,
        forceRenderingOff, forceRenderingOffRead, false, "force-off");
    return false;
  }
  if (!enabledRead || enabled) {
    EiemRegistrationTraceEligibility(
        renderer, drawRenderer, generation, active, enabled, enabledRead,
        forceRenderingOff, forceRenderingOffRead, true, "enabled-or-unread");
    return true;
  }

  if (includeGameHidden) {
    EiemRegistrationTraceEligibility(
        renderer, drawRenderer, generation, active, enabled, enabledRead,
        forceRenderingOff, forceRenderingOffRead, true,
        "disabled-authored-lod");
    return true;
  }

  bool ownedSkip = false;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX) {
    const EiemRenderOverrideState &state = s_eiemOverrides[index];
    ownedSkip = !state.restorePending && state.hasEnabled;
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  if (ownedSkip) {
    EiemRegistrationTraceEligibility(
        renderer, drawRenderer, generation, active, enabled, enabledRead,
        forceRenderingOff, forceRenderingOffRead, true, "owned-disabled");
    return true;
  }

  EiemRegistrationTraceEligibility(
      renderer, drawRenderer, generation, active, enabled, enabledRead,
      forceRenderingOff, forceRenderingOffRead, false, "disabled-unowned");
  return false;
}


static void TraceReadStringField(void *object, int offset, char *out,
                                 size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (!object || offset < 0) return;
  __try {
    void *value = *(void **)((char *)object + offset);
    if (value) ReadStrUtf8(value, out, (int)outSize);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    out[0] = '\0';
  }
}

static void *TraceReadObjectField(void *object, int offset) {
  if (!object || offset < 0) return nullptr;
  __try { return *(void **)((char *)object + offset); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static int EiemReadManagedBoolArrayValue(void *array, size_t count,
                                         size_t index) {
  if (!array || index >= count || count > 8192) return -1;
  __try {
    return *((uint8_t *)array + IL2CPP_ARRAY_DATA + index) ? 1 : 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}


static void TraceRememberLoadedModelPath(void *model, int64_t pathHash) {
  if (!model || !pathHash) return;
  char path[768] = {};
  TraceLookupHashPath(pathHash, path, sizeof(path));
  if (!path[0]) TraceResolveStringPathHashPath(pathHash, path, sizeof(path));
  if (!path[0]) return;
  AcquireSRWLockExclusive(&s_traceLoadedModelPathLock);
  size_t slot = s_traceLoadedModelPathCount;
  for (size_t index = 0; index < s_traceLoadedModelPathCount; ++index) {
    if (s_traceLoadedModelPaths[index].model == model) {
      slot = index;
      break;
    }
  }
  if (slot == s_traceLoadedModelPathCount) {
    if (slot >= _countof(s_traceLoadedModelPaths)) slot = slot % _countof(s_traceLoadedModelPaths);
    else ++s_traceLoadedModelPathCount;
  }
  s_traceLoadedModelPaths[slot].model = model;
  s_traceLoadedModelPaths[slot].pathHash = pathHash;
  strncpy_s(s_traceLoadedModelPaths[slot].path,
            sizeof(s_traceLoadedModelPaths[slot].path), path, _TRUNCATE);
  ReleaseSRWLockExclusive(&s_traceLoadedModelPathLock);
}

static bool TraceLookupLoadedModelPath(void *model, char *out, size_t outSize) {
  if (!model || !out || outSize == 0) return false;
  out[0] = '\0';
  AcquireSRWLockShared(&s_traceLoadedModelPathLock);
  for (size_t index = 0; index < s_traceLoadedModelPathCount; ++index) {
    if (s_traceLoadedModelPaths[index].model == model) {
      strncpy_s(out, outSize, s_traceLoadedModelPaths[index].path, _TRUNCATE);
      ReleaseSRWLockShared(&s_traceLoadedModelPathLock);
      return out[0] != '\0';
    }
  }
  ReleaseSRWLockShared(&s_traceLoadedModelPathLock);
  return false;
}

// PrefabInstantiateProxy is the normal world-model lifecycle adapter. Other
// lifecycle owners below feed the same instance registry and Render path.
// Diagnostic only: list the renderers a freshly instantiated character prefab
// carries, and whether they already hold Mesh references. Every presentation
// path instantiates its own Prefab for the same character, so the prefab path
// is what lists them. Kept out of the completion hook itself: that hook has a
// size-bounded contract, and inlining this scan pushed the model-registration
// call out of the window that verifies it.
static void TraceDumpPrefabRenderers(const char *path, void *model) {
  if (!kEiemValidationIdentityProbe) return;
  std::vector<EiemModPrefab> configuredPrefabs;
  if (path && path[0]) EiemFindModPrefabs(path, &configuredPrefabs);
  if (!model || !path || !path[0] || configuredPrefabs.empty() ||
      !g_gameObject_GetComponentsInChildren || !g_skinnedMeshRendererClass ||
      !il2cpp_class_get_type || !il2cpp_type_get_object)
    return;
  void *type = il2cpp_class_get_type(g_skinnedMeshRendererClass);
  void *typeObject = type ? il2cpp_type_get_object(type) : nullptr;
  if (!typeObject) return;
  bool includeInactive = true;
  void *params[] = {typeObject, &includeInactive};
  void *array = Invoke(g_gameObject_GetComponentsInChildren, model, params);
  const size_t count = EiemManagedArrayLength(array);
  Log("[PREFAB-RENDERERS] path=%s model=%p skinnedRenderers=%zu", path, model,
      count);
  if (!array || count > 256) return;
  void **items = (void **)((char *)array + IL2CPP_ARRAY_DATA);
  for (size_t index = 0; index < count; ++index) {
    void *renderer = items[index];
    if (!renderer) continue;
    char rendererName[160] = {};
    TraceReadUnityObjectName(renderer, rendererName, sizeof(rendererName));
    void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
    char meshName[192] = {};
    if (mesh) TraceReadUnityObjectName(mesh, meshName, sizeof(meshName));
    void *rootBone =
        g_smr_get_rootBone ? Invoke(g_smr_get_rootBone, renderer) : nullptr;
    char rootBoneName[160] = {};
    if (rootBone)
      TraceReadUnityObjectName(rootBone, rootBoneName, sizeof(rootBoneName));
    Log("[PREFAB-RENDERER] path=%s index=%zu name=%s mesh=%p meshName=%s "
        "rootBoneName=%s",
        path, index, rendererName[0] ? rendererName : "<unnamed>", mesh,
        meshName[0] ? meshName : "<empty>",
        rootBoneName[0] ? rootBoneName : "<empty>");
  }
}

#include "eiem_render_executor.h"

// The controller owns the game's RendererInfo cache.  This is observation only:
// it records the list passed to Init and the RendererInfo objects created by
// the original method, without changing either list or any Renderer state.
static void EiemLogMaterialControllerRegistry(void *controller,
                                               void *rendererList,
                                               const char *phase) {
  if (!controller) return;
  EiemRegistrationTraceNativeStackContext(
      phase ? phase : "material-controller", controller, rendererList,
      nullptr, InterlockedCompareExchange(&s_eiemModGeneration, 0, 0));
  if (!kEiemEnableCustomSkinPipelineObservation) return;
  __try {
    const int listCount = TraceManagedListCount(rendererList);
    void *items = rendererList ? *(void **)((char *)rendererList + 0x10)
                                : nullptr;
    const size_t itemCount = EiemManagedArrayLength(items);
    Log("[RENDER-REG-v1] phase=%s controller=%p inputList=%p inputCount=%d "
        "inputArrayCount=%zu", phase ? phase : "unknown", controller,
        rendererList, listCount, itemCount);

    // Compare the exact native skin state immediately before and after the
    // game builds RendererInfo.  This is read-only and uses configured Mesh
    // identities only to keep the bounded probe relevant; bone resolution
    // itself never depends on Transform names.
    if (items && listCount > 0 && listCount <= 8192) {
      void **renderers = (void **)((char *)items + IL2CPP_ARRAY_DATA);
      const size_t rendererLimit =
          (std::min)((size_t)listCount, itemCount);
      for (size_t index = 0; index < rendererLimit; ++index) {
        void *renderer = renderers[index];
        if (!renderer) continue;
        const char *rendererType = nullptr;
        void *mesh = EiemReadLodRendererMesh(renderer, &rendererType);
        if (!EiemModEquals(rendererType, "SkinnedMeshRenderer")) continue;
        char source[768] = {};
        char asset[192] = {};
        if (!mesh ||
            !EiemReadLiveMeshIdentity(mesh, source, sizeof(source), asset,
                                      sizeof(asset)) ||
            !TraceIdentityTextMatchesConfiguredRule(asset))
          continue;
        if (InterlockedIncrement(&s_eiemMaterialBoundarySkinProbeCount) > 192)
          break;
        void *bones = g_smr_get_bones
                          ? EiemBackendInvokeNoThrow(g_smr_get_bones, renderer)
                          : nullptr;
        void *rootBone = g_smr_get_rootBone
                             ? EiemBackendInvokeNoThrow(g_smr_get_rootBone,
                                                        renderer)
                             : nullptr;
        void *skinningRoot =
            g_smr_get_skinningRoot
                ? EiemBackendInvokeNoThrow(g_smr_get_skinningRoot, renderer)
                : nullptr;
        Log("[MOD-SKIN-REGISTRY-BOUNDARY-v1] phase=%s controller=%p "
            "index=%zu renderer=%p mesh=%p asset=%s bones=%p count=%zu "
            "rootBone=%p skinningRoot=%p",
            phase ? phase : "unknown", controller, index, renderer, mesh,
            asset, bones, EiemManagedArrayLength(bones), rootBone,
            skinningRoot);
      }
    }

    // EntityRenderHelperMaterialController.m_rendererInfos is a List<RendererInfo>
    // at 0x10 in the current metadata dump.  Do not use it as a mutation point;
    // this snapshot only answers whether the game's cache sees our Renderer and
    // which Mesh/material arrays it retained after Init.
    void *infos = *(void **)((char *)controller + 0x10);
    const int infoCount = TraceManagedListCount(infos);
    void *infoItems = infos ? *(void **)((char *)infos + 0x10) : nullptr;
    const size_t infoArrayCount = EiemManagedArrayLength(infoItems);
    Log("[RENDER-REG-v1] phase=%s controller=%p infoList=%p infoCount=%d "
        "infoArrayCount=%zu", phase ? phase : "unknown", controller, infos,
        infoCount, infoArrayCount);

    if (!infoItems || infoCount <= 0 || infoCount > 512) return;
    void **entries = (void **)((char *)infoItems + IL2CPP_ARRAY_DATA);
    const size_t limit = (std::min)((size_t)infoCount, infoArrayCount);
    size_t logged = 0;
    for (size_t index = 0; index < limit; ++index) {
      void *info = entries[index];
      if (!info) continue;
      __try {
        // RendererInfo field layout is runtime-verified in the resource dump:
        // m_renderer=0x10, materialReplacing=0x38,
        // sourceMaterials=0x30, replacingMaterials=0x40.
        void *renderer = *(void **)((char *)info + 0x10);
        if (!renderer) continue;
        char path[768] = {};
        TraceBuildRendererHierarchy(renderer, path, sizeof(path));
        if (!path[0] ||
            (!strstr(path, "body_01") && !strstr(path, "cloth_01") &&
             !strstr(path, "cloth_02")))
          continue;
        void *currentMesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
        void *sourceMaterials = *(void **)((char *)info + 0x30);
        void *replacingMaterials = *(void **)((char *)info + 0x40);
        const bool materialReplacing =
            *(bool *)((char *)info + 0x38);
        bool enabled = true;
        bool visible = false;
        EiemReadRendererEnabled(renderer, &enabled);
        EiemReadRendererVisible(renderer, &visible);
        Log("[RENDER-REG-v1] phase=%s index=%zu info=%p renderer=%p "
            "path=%s mesh=%p enabled=%d visible=%d sourceMaterials=%zu "
            "replacingMaterials=%zu materialReplacing=%d", phase ? phase :
            "unknown", index, info, renderer, path, currentMesh,
            enabled ? 1 : 0, visible ? 1 : 0,
            EiemManagedArrayLength(sourceMaterials),
            EiemManagedArrayLength(replacingMaterials),
            materialReplacing ? 1 : 0);
        if (++logged >= 96) break;
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[RENDER-REG-v1] phase=%s index=%zu info=%p read=exception",
            phase ? phase : "unknown", index, info);
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    Log("[RENDER-REG-v1] phase=%s controller=%p read=exception",
        phase ? phase : "unknown", controller);
  }
}

static void TraceEntityRenderHelperMaterialControllerInit(
    void *self, void *renderers, void *rendererTypeConfigs,
    void *customizeRendererPropertyConfig, bool calculateBoundsWithTransform,
    void *methodInfo) {
  auto original = (TraceEntityRenderHelperMaterialControllerInitFn)
      s_origEntityRenderHelperMaterialControllerInit;
  if (!self || s_eiemEntityRenderHelperMaterialInitGuard) {
    if (original)
      original(self, renderers, rendererTypeConfigs,
               customizeRendererPropertyConfig, calculateBoundsWithTransform,
               methodInfo);
    return;
  }
  s_eiemEntityRenderHelperMaterialInitGuard = true;
  // At this point the renderer list has been assembled and every native
  // SkinnedMeshRenderer observed in the list already has its instance-local
  // bones[] palette.  This is the common game-owned registration boundary:
  // commit the complete replacement transaction here, then let the original
  // method build RendererInfo/material/LOD/native skin state from it.  The
  // optional deferred branch remains source-compatible for evidence builds,
  // but production creation-boundary mode never queues a second live pass.
  EiemLogMaterialControllerRegistry(self, renderers, "before-original");
  if (s_eiemEntityRenderHelperActiveModel &&
      !s_eiemEntityRenderHelperMaterialApplied) {
    s_eiemEntityRenderHelperMaterialApplied =
        EiemApplyStandaloneRenderRules(
            s_eiemEntityRenderHelperActiveModel,
            "EntityRenderHelper.MaterialController.Init-before", nullptr,
            nullptr, nullptr);
    if (s_eiemEntityRenderHelperMaterialApplied)
      Log("[MOD-SKIN-COMMIT-v1] model=%p boundary=MaterialController.Init "
          "phase=before-original resourcesApplied=1",
          s_eiemEntityRenderHelperActiveModel);
  }
  if (original)
    original(self, renderers, rendererTypeConfigs,
             customizeRendererPropertyConfig, calculateBoundsWithTransform,
             methodInfo);
  EiemLogMaterialControllerRegistry(self, renderers, "after-original");
  s_eiemEntityRenderHelperMaterialInitGuard = false;
}

// EntityRenderHelper is the common game-owned registration boundary for the
// world, NPC and character-preview hierarchies. Runtime ordering shows that
// the game reaches it after PostDealLoadedModel (world) or SetSMRRootBone
// (NPC), while its original implementation has not yet constructed the
// RendererInfo/material/visibility/LOD registries. Commit one complete Mesh +
// bones transaction before that walk, then let the game build every downstream
// cache from the replacement generation. The guard only prevents callbacks
// caused by our own Unity setters; it does not suppress a helper merely because
// it is nested inside OnLoadFinish.
static void TraceEntityRenderHelperInitRenderAndMaterial(void *self,
                                                           void *methodInfo) {
  auto original = (TraceEntityRenderHelperInitFn)
      s_origEntityRenderHelperInitRenderAndMaterial;
  if (!self) {
    if (original) original(self, methodInfo);
    return;
  }
  // Setters issued by EIEM can re-enter the helper. Preserve the game's call,
  // but never start another replacement transaction from our own write.
  if (s_eiemApplyingModMeshAssignment) {
    if (original) original(self, methodInfo);
    return;
  }

  const bool outermost = !s_eiemEntityRenderHelperInitGuard;
  s_eiemEntityRenderHelperInitGuard = true;
  EiemAdoptUnityThreadFromAssemblyHook(
      "EntityRenderHelper._InitRenderAndMaterial");
  void *model = nullptr;
  if (g_component_get_gameObject)
    model = Invoke(g_component_get_gameObject, self);
  // The outer helper is only the transaction owner.  Its pre-original state
  // has empty bones[] (captured by MOD-SKIN-CAPTURE-v1), so applying there is
  // intentionally forbidden.  The nested MaterialController.Init hook above
  // performs the one commit after the native palettes exist.
  void *previousActiveModel = s_eiemEntityRenderHelperActiveModel;
  const bool previousMaterialApplied =
      s_eiemEntityRenderHelperMaterialApplied;
  const size_t firstMaterialReapply =
      s_eiemMaterialsToReapplyAfterHelper.size();
  s_eiemEntityRenderHelperActiveModel = model;
  s_eiemEntityRenderHelperMaterialApplied = false;
  if (original) original(self, methodInfo);
  const bool applied = s_eiemEntityRenderHelperMaterialApplied;
  // RendererInfo._Init can run before, inside, or after MaterialController.Init.
  // The outer helper is the only boundary that covers all three cases.
  size_t materialReapplied = 0;
  if (outermost) {
    for (size_t index = firstMaterialReapply;
         index < s_eiemMaterialsToReapplyAfterHelper.size(); ++index) {
      void *renderer = s_eiemMaterialsToReapplyAfterHelper[index];
      if (renderer && EiemNativeObjectStatus(renderer) == 1 &&
          EiemReapplyRendererMaterialsAfterCommit(
              renderer, "EntityRenderHelper.Init-after"))
        ++materialReapplied;
    }
    s_eiemMaterialsToReapplyAfterHelper.resize(firstMaterialReapply);
  }
  if (materialReapplied)
    Log("[MOD-MATERIAL] helper post-init restored renderers=%zu",
        materialReapplied);
  s_eiemEntityRenderHelperActiveModel = previousActiveModel;
  s_eiemEntityRenderHelperMaterialApplied = previousMaterialApplied;
  if (applied ||
      kEiemValidationIdentityProbe &&
      EiemRegistrationTraceFirst("entity-helper-init", "after", self,
                                 model, nullptr,
                                 InterlockedCompareExchange(
                                     &s_eiemModGeneration, 0, 0)))
    Log("[MOD-ASSEMBLY-v113] boundary=EntityRenderHelper._InitRenderAndMaterial "
        "helper=%p model=%p resourcesApplied=%d", self, model,
        applied ? 1 : 0);
  s_eiemEntityRenderHelperInitGuard = !outermost;
}

static bool EiemApplyStandaloneRenderRulesToRenderer(
    void *meshOwner, void *drawRenderer, void *mesh,
    const char *rendererType, void *methodInfo, const char *stage) {
  (void)stage;
  if (!EiemOnUnityThread()) return false;
  std::vector<EiemModRule> rules;
  EiemFindStandaloneRenderRules(&rules);
  return EiemApplyRenderRuleSetToRenderer(
      nullptr, meshOwner, drawRenderer, mesh, rendererType, methodInfo, rules,
      "<mesh setter>", nullptr, nullptr, nullptr, nullptr);
}

#include "eiem_assembly_binding.h"

#include "eiem_model_registry.h"
#include "eiem_world_ui_owner.h"

static bool EiemIsSkinnedRenderer(void *renderer) {
  if (!renderer || !il2cpp_object_get_class || !g_skinnedMeshRendererClass)
    return false;
  void *klass = nullptr;
  __try { klass = il2cpp_object_get_class(renderer); }
  __except (EXCEPTION_EXECUTE_HANDLER) { klass = nullptr; }
  for (int depth = 0; klass && depth < 10; ++depth) {
    if (klass == g_skinnedMeshRendererClass) return true;
    klass = il2cpp_class_get_parent ? il2cpp_class_get_parent(klass) : nullptr;
  }
  return false;
}

// LODGroup stores Renderer references, while a MeshRenderer's Mesh lives on
// the sibling MeshFilter. Resolve that identity only for the diagnostic dump;
// this helper never writes either component.
static void *EiemReadLodRendererMesh(void *renderer,
                                     const char **rendererTypeOut) {
  if (rendererTypeOut) *rendererTypeOut = "Renderer";
  if (!renderer) return nullptr;
  if (EiemIsSkinnedRenderer(renderer)) {
    if (rendererTypeOut) *rendererTypeOut = "SkinnedMeshRenderer";
    return EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
  }
  if (rendererTypeOut) *rendererTypeOut = "MeshRenderer";
  if (!g_component_get_gameObject || !g_gameObject_GetComponent ||
      !g_meshFilterClass || !il2cpp_class_get_type ||
      !il2cpp_type_get_object)
    return nullptr;
  __try {
    void *gameObject = Invoke(g_component_get_gameObject, renderer);
    void *type = il2cpp_class_get_type(g_meshFilterClass);
    void *typeObject = type ? il2cpp_type_get_object(type) : nullptr;
    if (!gameObject || !typeObject) return nullptr;
    void *params[] = {typeObject};
    void *meshFilter = Invoke(g_gameObject_GetComponent, gameObject, params);
    return EiemReadSharedMesh(meshFilter, "MeshFilter");
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

static bool EiemReapplyRendererMaterialsAfterCommit(void *renderer,
                                                     const char *stage) {
  if (!kEiemEnableMaterialLifecycle) return false;
  if (!renderer || EiemMaterialSourceInitActive(renderer) ||
      !EiemOnUnityThread() || !EiemIsSkinnedRenderer(renderer))
    return false;

  void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
  if (!mesh) return false;
  EiemResolvedRenderRule resolved = {};
  if (!EiemFindBoundRenderRule(renderer, &resolved.rule) ||
      (!resolved.rule.materialCount && !resolved.rule.submeshCount))
    return false;
  TraceReadUnityObjectName(mesh, resolved.asset, sizeof(resolved.asset));

  char error[256] = {};
  void *materials = nullptr;
  if (!EiemBuildRendererMaterialsForSource(resolved.rule, renderer, &materials,
                                           error, sizeof(error)) ||
      !materials) {
    Log("[MOD-MATERIAL-COMMIT] rebuild failed: stage=%s renderer=%p asset=%s error=%s",
        stage ? stage : "unknown", renderer, resolved.asset,
        error[0] ? error : "unknown");
    return false;
  }

  void *committed = g_renderer_get_sharedMaterials
                        ? Invoke(g_renderer_get_sharedMaterials, renderer)
                        : nullptr;
  if (!EiemManagedObjectArraySame(materials, committed) &&
      (!EiemCaptureOriginal(renderer, renderer, mesh, "SkinnedMeshRenderer", false, &resolved.rule) ||
       !EiemAssignRendererMaterials(renderer, materials, error, sizeof(error)))) {
    Log("[MOD-MATERIAL-COMMIT] assignment failed: stage=%s renderer=%p asset=%s error=%s",
        stage ? stage : "unknown", renderer, resolved.asset,
        error[0] ? error : "unknown");
    return false;
  }

  return true;
}

static void *EiemReadRendererFromMaterialInfo(void *rendererInfo) {
  if (!rendererInfo || s_materialRendererInfoRendererOffset < 0)
    return nullptr;
  __try {
    return *(void **)((char *)rendererInfo +
                      s_materialRendererInfoRendererOffset);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

static void TraceMaterialInfoInit(void *self, void *renderer, void *configs, void *methodInfo) {
  using InitFn = void (*)(void *, void *, void *, void *);
  auto original = (InitFn)s_origMaterialInfoInit;
  if (!kEiemEnableMaterialLifecycle) {
    if (original) original(self, renderer, configs, methodInfo);
    return;
  }
  bool sourceReady = false;
  {
    EiemMaterialSourceInitScope scope(renderer);
    sourceReady = EiemExposeSourceMaterialsForInit(renderer);
    // Never skip game initialization, including when source exposure failed.
    if (original) original(self, renderer, configs, methodInfo);
  }
  if (sourceReady) {
    // EntityRenderHelper owns a complete renderer-registration pass.  Do not
    // replace one Mesh while that pass is still iterating: later game systems
    // can otherwise cache a mixture of source and replacement generations.
    // The enclosing hook commits all rules once the original pass returns.
    // Standalone RendererInfo initialization remains the verified fallback for
    // NPC/UI paths which have no enclosing EntityRenderHelper boundary.
    if (s_eiemEntityRenderHelperInitGuard) {
      if (std::find(s_eiemMaterialsToReapplyAfterHelper.begin(),
                    s_eiemMaterialsToReapplyAfterHelper.end(), renderer) ==
          s_eiemMaterialsToReapplyAfterHelper.end())
        s_eiemMaterialsToReapplyAfterHelper.push_back(renderer);
      return;
    }
    // Without an enclosing helper this per-Renderer callback is the completed
    // boundary available to direct NPC/UI construction. Apply in-place
    // resource/material actions here.
    bool applied = false;
    if (EiemIsSkinnedRenderer(renderer)) {
      void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
      if (mesh)
        applied = EiemApplyStandaloneRenderRulesToRenderer(
            renderer, renderer, mesh, "SkinnedMeshRenderer", methodInfo,
            "RendererInfo._Init");
    }
    // Path-qualified rules cannot be newly resolved without a model root, but
    // a previously bound Renderer still needs its material slots restored
    // after the controller has refreshed them.
    if (!applied)
      EiemReapplyRendererMaterialsAfterCommit(renderer, "RendererInfo._Init");
    else {
      Log("[MOD-RENDERER-INIT] global rule applied renderer=%p", renderer);
      EiemRegistrationTraceRenderer(
          renderer, "RendererInfo._Init",
          InterlockedCompareExchange(&s_eiemModGeneration, 0, 0));
    }
  } else
    Log("[MOD-MATERIAL-SOURCE] init source unresolved; mod reapply not attempted renderer=%p", renderer);
}

#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
static volatile LONG s_eiemMaterialLifecycleProbeCalls = 0;

static void EiemLogMaterialLifecycleProbe(const char *method, const char *phase,
                                         void *info, void *input,
                                         bool inputIsArray, void *renderer,
                                         LONG callIndex) {
  if (callIndex < 0 || callIndex >= 500 || !renderer) return;
  char rendererName[160] = {};
  TraceReadUnityObjectName(renderer, rendererName, sizeof(rendererName));
  if (!strstr(rendererName, "lizhiyan_body_01_lod0") &&
      !strstr(rendererName, "lizhiyan_cloth_01_lod0") &&
      !strstr(rendererName, "lizhiyan_cloth_03_lod0"))
    return;
  auto item = [](void *array, size_t index) -> void * {
    if (!array || index >= EiemManagedArrayLength(array)) return nullptr;
    __try { return *(void **)((char *)array + 32 + index * sizeof(void *)); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
  };
  void *current = g_renderer_get_sharedMaterials
                      ? Invoke(g_renderer_get_sharedMaterials, renderer) : nullptr;
  void *replacing = nullptr;
  bool replacingActive = false;
  __try {
    replacing = *(void **)((char *)info + 0x40);
    replacingActive = *(unsigned char *)((char *)info + 0x38) != 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {}
  void *input0 = inputIsArray ? item(input, 0) : input;
  void *current0 = item(current, 0);
  void *currentLast = item(current, EiemManagedArrayLength(current)
                                      ? EiemManagedArrayLength(current) - 1 : 0);
  void *replacing0 = item(replacing, 0);
  char inputName[128] = {}, currentName[128] = {}, lastName[128] = {};
  char replacingName[128] = {};
  if (input0) TraceReadUnityObjectName(input0, inputName, sizeof(inputName));
  if (current0) TraceReadUnityObjectName(current0, currentName, sizeof(currentName));
  if (currentLast) TraceReadUnityObjectName(currentLast, lastName, sizeof(lastName));
  if (replacing0) TraceReadUnityObjectName(replacing0, replacingName,
                                            sizeof(replacingName));
  Log("[MATERIAL-LIFECYCLE-PROBE] call=%ld method=%s phase=%s info=%p renderer=%p "
      "name=%s input=%p/%zu input0=%p:%s current=%p/%zu current0=%p:%s "
      "last=%p:%s replacing=%p/%zu replacing0=%p:%s active=%d",
      callIndex, method, phase, info, renderer, rendererName,
      input, inputIsArray ? EiemManagedArrayLength(input) : (input ? 1u : 0u),
      input0, inputName, current, EiemManagedArrayLength(current),
      current0, currentName, currentLast, lastName, replacing,
      EiemManagedArrayLength(replacing), replacing0, replacingName,
      replacingActive ? 1 : 0);
}
#endif

#if defined(EIEM_PRESERVE_NATIVE_VFX_MATERIALS_BUILD)
static bool EiemNativeSprintMaterial(void *material) {
  if (!material) return false;
  char name[160] = {};
  TraceReadUnityObjectName(material, name, sizeof(name));
  // The character's dissolve material is the *_VFXInstance object.  The
  // M_fxbat_* object is a separate dash effect slot and must remain visible
  // while the character's ordinary materials are rebound to the mod output.
  // Treating both as one transaction left the replacement material in the
  // native array after sprint recovery.
  return strstr(name, "_VFXInstance") != nullptr;
}

static bool EiemNativeSprintMaterialArray(void *materials) {
  const size_t count = EiemManagedArrayLength(materials);
  if (!materials || !count || count > 64) return false;
  __try {
    void **items = (void **)((char *)materials + 32);
    for (size_t index = 0; index < count; ++index)
      if (EiemNativeSprintMaterial(items[index])) return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return false;
}

static bool EiemPreserveNativeSprintCommit(void *materials, bool array) {
  return array ? EiemNativeSprintMaterialArray(materials)
               : EiemNativeSprintMaterial(materials);
}
#else
static bool EiemPreserveNativeSprintCommit(void *, bool) { return false; }
#endif

#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD) && \
    defined(EIEM_PERDRAW_END_RESTORE_BUILD)
// RendererInfo creates/replaces its native *_VFXInstance immediately inside
// TrySet/ TryReplaceSharedMaterials.  The implementation is defined in the
// per-draw controller header below; this declaration lets the lifecycle hook
// prewarm the expanded LZY submesh array at that creation boundary instead of
// delaying all allocations until the first per-draw frame.
static bool EiemPrewarmNativeVfxSlotMaterials(void *info, void *renderer,
                                              void *nativeInput,
                                              bool inputIsArray);
#endif

static bool TraceRendererInfoTrySetSharedMaterial(void *self, void *material,
                                                  void *methodInfo) {
#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
  void *renderer = EiemReadRendererFromMaterialInfo(self);
  const LONG probeCall = InterlockedIncrement(&s_eiemMaterialLifecycleProbeCalls) - 1;
  EiemLogMaterialLifecycleProbe("TrySetSharedMaterial", "before", self,
                                material, false, renderer, probeCall);
#endif
  auto original = (TraceRendererInfoMaterialCommitFn)
      s_origRendererInfoTrySetSharedMaterial;
  bool adapted = false;
#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD)
  const bool result = EiemInvokeAdaptedNativeVfxCommit(
      self, material, false, methodInfo, original, &adapted);
#else
  const bool result = original ? original(self, material, methodInfo) : false;
#endif
#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
  EiemLogMaterialLifecycleProbe("TrySetSharedMaterial", "native", self,
                                material, false, renderer, probeCall);
#endif
  void *commitRenderer = EiemReadRendererFromMaterialInfo(self);
  if (adapted) {
    Log("[MOD-MATERIAL-VFX-SLOT-ADAPTER] method=TrySetSharedMaterial renderer=%p",
        commitRenderer);
#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD) && \
    defined(EIEM_PERDRAW_END_RESTORE_BUILD)
    EiemPrewarmNativeVfxSlotMaterials(self, commitRenderer, material, false);
#endif
  } else if (EiemPreserveNativeSprintCommit(material, false)) {
    Log("[MOD-MATERIAL-SKIP-NATIVE-VFX] method=TrySetSharedMaterial renderer=%p",
        commitRenderer);
  } else {
    EiemReapplyRendererMaterialsAfterCommit(commitRenderer,
                                            "TrySetSharedMaterial");
  }
#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
  EiemLogMaterialLifecycleProbe("TrySetSharedMaterial", "eiem", self,
                                material, false, renderer, probeCall);
#endif
  return result;
}

static bool TraceRendererInfoTrySetSharedMaterials(void *self,
                                                   void *materials,
                                                   void *methodInfo) {
#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
  void *renderer = EiemReadRendererFromMaterialInfo(self);
  const LONG probeCall = InterlockedIncrement(&s_eiemMaterialLifecycleProbeCalls) - 1;
  EiemLogMaterialLifecycleProbe("TrySetSharedMaterials", "before", self,
                                materials, true, renderer, probeCall);
#endif
  auto original = (TraceRendererInfoMaterialCommitFn)
      s_origRendererInfoTrySetSharedMaterials;
  bool adapted = false;
#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD)
  const bool result = EiemInvokeAdaptedNativeVfxCommit(
      self, materials, true, methodInfo, original, &adapted);
#else
  const bool result = original ? original(self, materials, methodInfo) : false;
#endif
#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
  EiemLogMaterialLifecycleProbe("TrySetSharedMaterials", "native", self,
                                materials, true, renderer, probeCall);
#endif
  void *commitRenderer = EiemReadRendererFromMaterialInfo(self);
  if (adapted) {
    Log("[MOD-MATERIAL-VFX-SLOT-ADAPTER] method=TrySetSharedMaterials renderer=%p",
        commitRenderer);
#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD) && \
    defined(EIEM_PERDRAW_END_RESTORE_BUILD)
    EiemPrewarmNativeVfxSlotMaterials(self, commitRenderer, materials, true);
#endif
  } else if (EiemPreserveNativeSprintCommit(materials, true)) {
    Log("[MOD-MATERIAL-SKIP-NATIVE-VFX] method=TrySetSharedMaterials renderer=%p",
        commitRenderer);
  } else {
    EiemReapplyRendererMaterialsAfterCommit(commitRenderer,
                                            "TrySetSharedMaterials");
  }
#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
  EiemLogMaterialLifecycleProbe("TrySetSharedMaterials", "eiem", self,
                                materials, true, renderer, probeCall);
#endif
  return result;
}

static bool TraceRendererInfoTryReplaceSharedMaterials(void *self,
                                                       void *materials,
                                                       void *methodInfo) {
#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
  void *renderer = EiemReadRendererFromMaterialInfo(self);
  const LONG probeCall = InterlockedIncrement(&s_eiemMaterialLifecycleProbeCalls) - 1;
  EiemLogMaterialLifecycleProbe("TryReplaceSharedMaterials", "before", self,
                                materials, true, renderer, probeCall);
#endif
  auto original = (TraceRendererInfoMaterialCommitFn)
      s_origRendererInfoTryReplaceSharedMaterials;
  bool adapted = false;
#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD)
  const bool result = EiemInvokeAdaptedNativeVfxCommit(
      self, materials, true, methodInfo, original, &adapted);
#else
  const bool result = original ? original(self, materials, methodInfo) : false;
#endif
#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
  EiemLogMaterialLifecycleProbe("TryReplaceSharedMaterials", "native", self,
                                materials, true, renderer, probeCall);
#endif
  void *commitRenderer = EiemReadRendererFromMaterialInfo(self);
  if (adapted) {
    Log("[MOD-MATERIAL-VFX-SLOT-ADAPTER] method=TryReplaceSharedMaterials renderer=%p",
        commitRenderer);
#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD) && \
    defined(EIEM_PERDRAW_END_RESTORE_BUILD)
    EiemPrewarmNativeVfxSlotMaterials(self, commitRenderer, materials, true);
#endif
  } else if (EiemPreserveNativeSprintCommit(materials, true)) {
    Log("[MOD-MATERIAL-SKIP-NATIVE-VFX] method=TryReplaceSharedMaterials renderer=%p",
        commitRenderer);
  } else {
    EiemReapplyRendererMaterialsAfterCommit(commitRenderer,
                                            "TryReplaceSharedMaterials");
  }
#if defined(EIEM_MATERIAL_LIFECYCLE_PROBE_BUILD)
  EiemLogMaterialLifecycleProbe("TryReplaceSharedMaterials", "eiem", self,
                                materials, true, renderer, probeCall);
#endif
  return result;
}

// Legacy LOD-group observation retained only for archived diagnostics. The
// production hook is no longer installed.


// Read-only evidence for the concrete Renderer objects that the world skin
// assembly passes to CreateSMSGO/AssignSkinGo. Keep this validation-only,
// bounded and de-duplicated so normal world traffic cannot become a per-frame
// logger.


// These methods expose the separate NPC avatar construction order for
// diagnostics. Character model replacement is owned by the model/PFB
// lifecycle above; NPC activity alone is not evidence that a UI character
// presentation uses this pipeline.
static void TraceAssignSkinGo(int32_t lod, void *renderers,
                              void *rootBones, void *closure,
                              void *methodInfo) {
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  EiemRegistrationTraceArrayBoundary(
      "AssignSkinGoPre", nullptr, renderers, EiemManagedArrayLength(renderers),
      generation, lod);
  auto original = (TraceAssignSkinPostFn)s_origAssignSkinGo;
  if (original)
    original(lod, renderers, rootBones, closure, methodInfo);
  const LONG afterGeneration =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  EiemRegistrationTraceArrayBoundary(
      "AssignSkinGoPost", nullptr, renderers,
      EiemManagedArrayLength(renderers), afterGeneration, lod);

}

static void TraceAssignSkinPost(int32_t lod, void *renderers,
                                void *rootBones, void *closure,
                                void *methodInfo) {
  auto original = (TraceAssignSkinPostFn)s_origAssignSkinPost;
  if (original)
    original(lod, renderers, rootBones, closure, methodInfo);
  EiemRegistrationTraceArrayBoundary(
      "AssignSkinPost", nullptr, renderers, EiemManagedArrayLength(renderers),
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0), lod);
  EiemRememberGameSourceSkinningFromArray(
      renderers, true, true, "AssignSkinPost");
}

static void TraceSetSmrRootBone(void *animator, void *renderers,
                                void *rootBoneInfos, void *methodInfo) {
  auto original = (TraceSetSmrRootBoneFn)s_origSetSmrRootBone;
  if (original) original(animator, renderers, rootBoneInfos, methodInfo);
  EiemRegistrationTraceArrayBoundary(
      "SetSMRRootBone", animator, renderers, EiemManagedArrayLength(renderers),
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0), -1);
  EiemRememberGameSourceSkinningFromArray(
      renderers, false, true, "SetSMRRootBone");
}

// CreateSMS returns the exact SkinnedMeshRenderer array for one NPC model and
// one LOD. Resource writes wait for AssignSkin to finish the native palette.
static size_t EiemApplyStandaloneRenderRulesToSkinArray(
    void *renderers, const char *stage) {
  const size_t count = EiemManagedArrayLength(renderers);
  if (!renderers || !count || count > 8192) return 0;
  void **items = (void **)((char *)renderers + IL2CPP_ARRAY_DATA);
  size_t applied = 0;
  for (size_t index = 0; index < count; ++index) {
    void *renderer = items[index];
    if (!renderer) continue;
    void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
    if (mesh && EiemApplyStandaloneRenderRulesToRenderer(
                    renderer, renderer, mesh, "SkinnedMeshRenderer", nullptr,
                    stage))
      ++applied;
  }
  if (applied)
    Log("[MOD-ASSEMBLY-v111] stage=%s sources=%zu applied=%zu",
        stage ? stage : "unknown", count, applied);
  return applied;
}

static void TraceCreateSmsGo(void *assetLoader, void *meshAssets, int32_t lod,
                             void *goPool, void *parent, void *stringList,
                             void *intList, void **renderers,
                             void **rootBones, bool flag, void *handleMap,
                             bool deferred, void *methodInfo) {
  EiemAdoptUnityThreadFromAssemblyHook("NPCAvatarCreatorUtils.CreateSMSGO");
  auto original = (TraceCreateSmsGoFn)s_origCreateSmsGo;
  if (original)
    original(assetLoader, meshAssets, lod, goPool, parent, stringList,
             intList, renderers, rootBones, flag, handleMap, deferred,
             methodInfo);
  void *array = renderers ? *renderers : nullptr;
  // CreateSMS has produced the Renderer objects, but AssignSkin has not yet
  // supplied their final bones/root state.  Keep this boundary observational;
  // the resource transaction runs after AssignSkin returns.
  EiemRegistrationTraceArrayBoundary(
      "CreateSMSGO", meshAssets, array, EiemManagedArrayLength(array),
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0), lod);

}

static void TraceCreateSmsPost(void *meshAssets, int32_t lod, void *goPool,
                               void *parent, void *stringList, void *intList,
                               void **renderers, void **rootBones, bool flag,
                               void *methodInfo) {
  EiemAdoptUnityThreadFromAssemblyHook(
      "NPCAvatarCreatorUtils.CreateSMSInfoForPostModel");
  auto original = (TraceCreateSmsPostFn)s_origCreateSmsPost;
  if (original)
    original(meshAssets, lod, goPool, parent, stringList, intList, renderers,
             rootBones, flag, methodInfo);
  void *array = renderers ? *renderers : nullptr;
  // Defer resource writes until the game's AssignSkin boundary completes.
  EiemRegistrationTraceArrayBoundary(
      "CreateSMSInfoForPostModel", meshAssets, array,
      EiemManagedArrayLength(array),
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0), lod);
}

// Mesh observations are fixed-size metadata records. They intentionally keep
// native object pointers only for the lifetime of the process; the Dump UI
// uses them as selection keys and never persists them as replacement IDs.
struct EiemMeshObservation {
  void *mesh;
  void *renderer;
  char rendererType[32];
  char rendererName[192];
  char meshName[192];
  char hierarchyPath[512];
  void *renderers[16];
  uint32_t rendererCount;
  uint32_t instanceCount;
  ULONGLONG firstSeenMs;
  ULONGLONG lastSeenMs;
};
static SRWLOCK s_meshObservationLock = SRWLOCK_INIT;
static EiemMeshObservation s_meshObservations[4096] = {};
static size_t s_meshObservationCount = 0;

static void TraceBuildRendererHierarchy(void *renderer, char *out,
                                        size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (!renderer || !g_component_get_transform || !g_transform_get_parent ||
      !g_object_get_name)
    return;

  char names[32][96] = {};
  size_t count = 0;
  void *transform = Invoke(g_component_get_transform, renderer);
  while (transform && count < _countof(names)) {
    void *nameString = Invoke(g_object_get_name, transform);
    if (nameString)
      ReadStrUtf8(nameString, names[count], sizeof(names[count]));
    if (!names[count][0]) strncpy_s(names[count], sizeof(names[count]),
                                    "<unnamed>", _TRUNCATE);
    ++count;
    transform = Invoke(g_transform_get_parent, transform);
  }

  size_t used = 0;
  for (size_t i = count; i > 0; --i) {
    const char *name = names[i - 1];
    const size_t nameLen = strlen(name);
    const size_t separator = used ? 1 : 0;
    if (used + separator + nameLen + 1 >= outSize) break;
    if (separator) out[used++] = '/';
    memcpy(out + used, name, nameLen);
    used += nameLen;
    out[used] = '\0';
  }
}

static void TraceFillMeshObservationDetails(EiemMeshObservation &entry) {
  char rendererText[512] = {};
  char meshText[512] = {};
  TraceDescribeObject(entry.renderer, rendererText, sizeof(rendererText));
  TraceDescribeObject(entry.mesh, meshText, sizeof(meshText));
  strncpy_s(entry.rendererName, sizeof(entry.rendererName), rendererText,
            _TRUNCATE);
  strncpy_s(entry.meshName, sizeof(entry.meshName), meshText, _TRUNCATE);
  TraceBuildRendererHierarchy(entry.renderer, entry.hierarchyPath,
                              sizeof(entry.hierarchyPath));
}

static void TraceRememberMeshObservation(void *renderer, void *mesh,
                                         const char *rendererType) {
  if (!renderer || !mesh || g_shutdownRequested) return;
  AcquireSRWLockExclusive(&s_meshObservationLock);
  for (size_t i = 0; i < s_meshObservationCount; ++i) {
    EiemMeshObservation &entry = s_meshObservations[i];
    if (entry.mesh == mesh) {
      entry.lastSeenMs = GetTickCount64();
      bool knownRenderer = false;
      for (uint32_t r = 0; r < entry.rendererCount; ++r) {
        if (entry.renderers[r] == renderer) {
          knownRenderer = true;
          break;
        }
      }
      if (!knownRenderer) {
        if (entry.rendererCount < _countof(entry.renderers))
          entry.renderers[entry.rendererCount++] = renderer;
        ++entry.instanceCount;
      }
      if (!entry.hierarchyPath[0] || !entry.meshName[0])
        TraceFillMeshObservationDetails(entry);
      ReleaseSRWLockExclusive(&s_meshObservationLock);
      return;
    }
  }
  if (s_meshObservationCount >= _countof(s_meshObservations)) {
    ReleaseSRWLockExclusive(&s_meshObservationLock);
    return;
  }
  EiemMeshObservation &entry = s_meshObservations[s_meshObservationCount++];
  memset(&entry, 0, sizeof(entry));
  entry.renderer = renderer;
  entry.mesh = mesh;
  entry.renderers[0] = renderer;
  entry.rendererCount = 1;
  entry.instanceCount = 1;
  strncpy_s(entry.rendererType, sizeof(entry.rendererType),
            rendererType ? rendererType : "Renderer", _TRUNCATE);
  entry.firstSeenMs = entry.lastSeenMs = GetTickCount64();
  // Object description is protected by SEH and runs on Unity's calling
  // thread. Keep it out of the GUI thread, which cannot invoke IL2CPP safely.
  TraceFillMeshObservationDetails(entry);
  ReleaseSRWLockExclusive(&s_meshObservationLock);
}

static size_t TraceCopyMeshObservations(EiemMeshObservation *out,
                                        size_t capacity) {
  if (!out || capacity == 0) return 0;
  AcquireSRWLockShared(&s_meshObservationLock);
  const size_t count = s_meshObservationCount < capacity
                           ? s_meshObservationCount
                           : capacity;
  if (count) memcpy(out, s_meshObservations,
                    count * sizeof(EiemMeshObservation));
  ReleaseSRWLockShared(&s_meshObservationLock);
  return count;
}

static void TraceClearMeshObservations() {
  AcquireSRWLockExclusive(&s_meshObservationLock);
  s_meshObservationCount = 0;
  memset(s_meshObservations, 0, sizeof(s_meshObservations));
  ReleaseSRWLockExclusive(&s_meshObservationLock);
}

typedef void (*TraceRendererVisitor)(void *renderer, void *mesh,
                                     const char *rendererType, void *context);

static void TraceVisitRendererType(void *rendererClass,
                                   const char *rendererType,
                                   TraceRendererVisitor visitor,
                                   void *context) {
  if (!rendererClass || !rendererType ||
      (!g_object_find_objects_of_type &&
       !g_resources_find_objects_of_type_all) ||
      !il2cpp_class_get_type || !il2cpp_type_get_object || !visitor)
    return;
  __try {
    void *type = il2cpp_class_get_type(rendererClass);
    void *typeObject = type ? il2cpp_type_get_object(type) : nullptr;
    if (!typeObject) return;
    void *params[] = {typeObject};
    // FindObjectsOfType omits inactive and persistent objects, including many
    // UI preview models. F10 is an explicit one-shot reconciliation, so use
    // Resources.FindObjectsOfTypeAll when available and keep the scene-only
    // API solely as a compatibility fallback.
    void *enumerator = g_resources_find_objects_of_type_all
                           ? g_resources_find_objects_of_type_all
                           : g_object_find_objects_of_type;
    void *array = Invoke(enumerator, nullptr, params);
    if (!array) return;
    int count = *(int *)((char *)array + 24);
    if (count < 0) return;
    if (count > 100000) count = 100000;
    void **data = (void **)((char *)array + 32);
    void *getter = strcmp(rendererType, "SkinnedMeshRenderer") == 0
                       ? g_smr_get_sharedMesh
                       : g_meshFilter_get_sharedMesh;
    for (int i = 0; i < count; ++i) {
      if (data[i] && getter)
        visitor(data[i], Invoke(getter, data[i]), rendererType, context);
    }
  } __except (1) {
    Log("[SCENE] Renderer enumeration failed for %s", rendererType);
  }
}

static void TraceObserveRendererVisitor(void *renderer, void *mesh,
                                        const char *rendererType,
                                        void *) {
  TraceRememberMeshObservation(renderer, mesh, rendererType);
}

static void TraceEnumerateRendererType(void *rendererClass,
                                       const char *rendererType) {
  TraceVisitRendererType(rendererClass, rendererType, TraceObserveRendererVisitor,
                         nullptr);
}

// Refresh performs a real scene-wide Unity enumeration instead of relying on
// sharedMesh setter events, which may have occurred before the Dump page was
// opened. It runs only in the game's window procedure.
static void TraceRefreshMeshObservations() {
  TraceClearMeshObservations();
  TraceEnumerateRendererType(g_skinnedMeshRendererClass,
                             "SkinnedMeshRenderer");
  TraceEnumerateRendererType(g_meshFilterClass, "MeshFilter");
}

static void EiemReapplyShapeControls(const std::vector<std::string> &affected) {
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  for (auto &state : s_eiemOverrides) {
    EiemModRule rule = {};
    if (EiemModAffected(state.modPath, &affected) &&
        EiemFindRenderRuleBySection(state.modPath, state.renderSection, &rule))
      EiemUpdateRendererShapes(state.renderer, state.rendererType, rule, state.shapes);
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
}

static constexpr UINT_PTR kEiemShapeTransitionTimer = 0xE153;
static ULONGLONG s_eiemShapeTransitionTick = 0;

static bool EiemAnyShapeTransitions() {
  bool active = false;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  for (const auto &state : s_eiemOverrides)
    if (EiemShapeStateAnimating(state.shapes)) { active = true; break; }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  return active;
}

static void EiemRefreshShapeTransitionTimer() {
  if (!g_gameHwnd || !IsWindow(g_gameHwnd)) return;
  if (EiemAnyShapeTransitions()) {
    if (!s_eiemShapeTransitionTick) s_eiemShapeTransitionTick = GetTickCount64();
    SetTimer(g_gameHwnd, kEiemShapeTransitionTimer, 16, nullptr);
  } else {
    KillTimer(g_gameHwnd, kEiemShapeTransitionTimer);
    s_eiemShapeTransitionTick = 0;
  }
}

static void EiemRunShapeTransitions() {
  const ULONGLONG now = GetTickCount64();
  const float elapsed = s_eiemShapeTransitionTick
      ? (float)(now - s_eiemShapeTransitionTick) / 1000.0f : 0.0f;
  s_eiemShapeTransitionTick = now;
  AcquireSRWLockExclusive(&s_eiemOverrideLock);
  for (auto &state : s_eiemOverrides) {
    if (!EiemShapeStateAnimating(state.shapes)) continue;
    EiemModRule rule = {};
    if (EiemFindRenderRuleBySection(state.modPath, state.renderSection, &rule))
      EiemUpdateRendererShapes(state.renderer, state.rendererType, rule,
                               state.shapes, elapsed);
  }
  ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  EiemRefreshShapeTransitionTimer();
}


static EiemModUpdateQueue s_eiemModUpdates;
// Models whose first native renderer registration was intentionally allowed
// to complete with the game's source Mesh.  A model is queued once and
// replayed through the same EntityRenderHelper boundary after the cache has
// settled; this keeps cold start on the same path as a stable F10 rebuild.
// At most one WM_EIEM_MOD_RECONCILE may be queued at a time.  Requests are
// already coalesced by EiemModUpdateQueue; posting one message per lifecycle
// callback would otherwise flood the game's window queue while a scene is
// assembling.
static volatile LONG s_eiemModUpdateMessagePosted = 0;
// One ordered queue for window-thread keys and render-thread ImGui controls.
static SRWLOCK s_eiemInputLock = SRWLOCK_INIT;
static std::vector<EiemModInputEvent> s_eiemPendingInputs;

static void EiemQueueModInput(EiemModInputEvent event) {
  AcquireSRWLockExclusive(&s_eiemInputLock);
  // Adjacent UI and manager-slider frames merge variable writes; never move
  // one past a key press or a different control source.
  if ((event.directValues || !event.uiSection.empty()) &&
      !s_eiemPendingInputs.empty() &&
      s_eiemPendingInputs.back().generation == event.generation &&
      s_eiemPendingInputs.back().modPath == event.modPath &&
      s_eiemPendingInputs.back().directValues == event.directValues &&
      s_eiemPendingInputs.back().uiSection == event.uiSection) {
    for (const auto &value : event.values) s_eiemPendingInputs.back().values[value.first] = value.second;
  }
  else if (event.holdTick && !s_eiemPendingInputs.empty() &&
           s_eiemPendingInputs.back().holdTick &&
           s_eiemPendingInputs.back().generation == event.generation &&
           s_eiemPendingInputs.back().modPath == event.modPath &&
           s_eiemPendingInputs.back().chord == event.chord &&
           s_eiemPendingInputs.back().keySection == event.keySection &&
           s_eiemPendingInputs.back().uiFocus == event.uiFocus) {
    // A busy Unity thread may leave several 20 ms polls queued. Merge their
    // elapsed time into one transaction so the input queue stays bounded.
    s_eiemPendingInputs.back().holdSeconds += event.holdSeconds;
  }
  else s_eiemPendingInputs.push_back(std::move(event));
  ReleaseSRWLockExclusive(&s_eiemInputLock);
  EiemRequestModUpdate(EiemModUpdate::Reapply, "mod control");
}

static void EiemQueueModKey(EiemKeyChord chord, LONG generation,
                            bool holdTick = false,
                            double holdSeconds = 0.02) {
  if (!EiemOnUnityThread() || !g_pluginActive) return;
  HWND foreground = GetForegroundWindow();
  if (foreground != g_gameHwnd && foreground != g_guiHwnd && foreground != g_modUiHwnd) return;
  EiemModInputEvent event{chord,generation};
  event.modPath = EiemGetSelectedModPath();
  if (event.modPath.empty()) return;
  event.uiFocus = EiemModUsesUiKeyScope(
      foreground == g_gameHwnd, foreground == g_modUiHwnd,
      InterlockedCompareExchange(&s_eiemModManagerOpen, 0, 0) != 0);
  event.holdTick = holdTick;
  event.holdSeconds = holdSeconds;
  EiemRegistrationTraceInput(chord.vk, chord.modifiers, generation,
                             event.uiFocus, event.modPath.c_str(), nullptr, 0);
  EiemQueueModInput(std::move(event));
}

static bool EiemPostPendingModUpdate(const char *reason) {
  if (g_shutdownRequested || !g_gameHwnd || !IsWindow(g_gameHwnd) ||
      !s_eiemModUpdates.HasPending())
    return false;
  // A posted message is only a wake-up.  The atomic queue carries the full
  // bitmask, so a later caller can merge Reconcile/Reapply/Reload without
  // posting another wake-up.
  if (InterlockedCompareExchange(&s_eiemModUpdateMessagePosted, 1, 0) != 0)
    return true;
  if (PostMessageW(g_gameHwnd, WM_EIEM_MOD_RECONCILE, 0, 0)) return true;
  InterlockedExchange(&s_eiemModUpdateMessagePosted, 0);
  SetTimer(g_gameHwnd, kEiemModRetryTimer, 100, nullptr);
  Log("[MOD] Pending reconcile post failed (%s): err=%lu",
      reason ? reason : "unknown", GetLastError());
  return false;
}

static void EiemRequestModUpdate(EiemModUpdate request, const char *reason) {
  if (g_shutdownRequested) {
    Log("[MOD] Reconcile not queued (%s): game window is unavailable",
        reason ? reason : "unknown");
    return;
  }
  const bool first = s_eiemModUpdates.Request(request);
  if (!g_gameHwnd || !IsWindow(g_gameHwnd)) {
    Log("[MOD] Reconcile pending (%s): game window is unavailable",
        reason ? reason : "unknown");
    return;
  }
  if (!EiemPostPendingModUpdate(reason)) return;
  if (first)
    Log("[MOD] Reconcile queued tick=%llu: %s",
        (unsigned long long)GetTickCount64(), reason ? reason : "unknown");
}


static void EiemQueueNativeSkinRefresh(uintptr_t ownerModel) {
  if (!ownerModel) return;
  AcquireSRWLockExclusive(&s_eiemNativeSkinRefreshLock);
  if (std::find(s_eiemNativeSkinRefreshModels.begin(),
                s_eiemNativeSkinRefreshModels.end(), ownerModel) ==
      s_eiemNativeSkinRefreshModels.end())
    s_eiemNativeSkinRefreshModels.push_back(ownerModel);
  ReleaseSRWLockExclusive(&s_eiemNativeSkinRefreshLock);
  EiemRequestModUpdate(EiemModUpdate::SkinRefresh, "native skin refresh");
}

static void EiemQueueModReconcile(const char *reason) {
  EiemRequestModUpdate(EiemModUpdate::Reconcile, reason);
}

static bool EiemSubmeshVisibilityTargets(
    const EiemRenderOverrideState &state,
    const std::vector<EiemSubmeshVisibilityChange> &changes) {
  for (const auto &change : changes)
    if (EiemModEquals(state.modPath, change.modPath.c_str()) &&
        EiemModEquals(state.renderSection, change.section.c_str()))
      return true;
  return false;
}

// A visibility key changes only the generated Mesh index buffers. Existing
// Renderer skin/material/physics state already belongs to the game instance;
// keep the registered Mesh identity and every Renderer field unchanged.
static uint32_t EiemReapplySubmeshVisibility(
    const std::vector<EiemSubmeshVisibilityChange> &changes) {
  if (changes.empty()) return 0;
  std::vector<EiemRenderOverrideState> targets;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  for (const auto &state : s_eiemOverrides)
    if (!state.restorePending && state.ownsMesh &&
        EiemSubmeshVisibilityTargets(state, changes))
      targets.push_back(state);
  ReleaseSRWLockShared(&s_eiemOverrideLock);

  uint32_t applied = 0;
  for (const auto &state : targets) {
    if (!state.renderer || state.rendererRef.Status() != 1 ||
        state.sourceMeshRef.Status() != 1)
      continue;
    void *current = EiemReadSharedMesh(state.renderer, state.rendererType);
    if (!current || current != state.replacementMesh) {
      Log("[MOD] Submesh visibility skipped renderer=%p section=%s "
          "reason=game mesh changed current=%p expected=%p",
          state.renderer, state.renderSection, current,
          state.replacementMesh);
      continue;
    }
    EiemModRule rule = {};
    if (!EiemFindRenderRuleBySection(state.modPath, state.renderSection,
                                     &rule) ||
        !rule.hasMesh)
      continue;
    void *resourceMesh = nullptr;
    char error[256] = {};
    if (!EiemBuildMeshResource(rule, &resourceMesh, error, sizeof(error),
                               state.originalMesh) ||
        !resourceMesh) {
      Log("[MOD] Submesh visibility update failed renderer=%p section=%s "
          "mask=0x%08X error=%s",
          state.renderer, state.renderSection, rule.hiddenSubmeshMask,
          error[0] ? error : "unknown");
      continue;
    }
    if (resourceMesh != current) {
      Log("[MOD] Submesh visibility skipped renderer=%p section=%s "
          "reason=resource identity changed current=%p resource=%p; use F10",
          state.renderer, state.renderSection, current, resourceMesh);
      continue;
    }
    ++applied;
    Log("[MOD] Submesh visibility applied renderer=%p section=%s "
        "mask=0x%08X mesh=%p",
        state.renderer, state.renderSection, rule.hiddenSubmeshMask,
        resourceMesh);
  }
  return applied;
}

// Runs only from MmdWndProc. F10 restores the previous generation, then
// replays configuration against instances registered by either supported
// model lifecycle adapter. No scene-wide Mesh scan exists.
#if defined(EIEM_PERDRAW_END_RESTORE_BUILD)
// The per-draw source/VFX caches are defined by the visibility probe included
// later in this translation unit.  Reconcile runs first, so expose the
// Unity-thread generation-boundary cleanup here.
static void EiemResetPerDrawMaterialCaches();
#endif
#include "eiem_mod_reconcile.h"

static void *s_origAssetBundleLoadAsset1 = nullptr;
static void *s_origAssetBundleLoadAsset2 = nullptr;
static void *s_origAssetBundleLoadAssetAsync1 = nullptr;
static void *s_origAssetBundleLoadAssetAsync2 = nullptr;
static void *s_origAssetBundleUnload = nullptr;
static void *s_origAssetBundleCreateRequestGetAssetBundle = nullptr;
static void *s_origBundleLoadAssetBundle = nullptr;
static void *s_origBundleLoadAssetBundleAsync = nullptr;
static void *s_origBundleSetAssetBundle = nullptr;
static void *s_origBundleFinishWithBundle = nullptr;
static void *s_origBundleGetFullPath = nullptr;
static void *s_origBundleOnEndUnload = nullptr;
static void *s_origResourceLoadAssetInternal = nullptr;
static void *s_origResourceLoadSubAssetInternal = nullptr;
static void *s_origAssetGetAssetName = nullptr;
static void *s_origAssetFinishWithAsset = nullptr;
static void *s_origAssetOnComplete = nullptr;
static void *s_origGetAssetPathHash = nullptr;
static void *s_origGetAssetPathHashWithoutBurst = nullptr;
static void *s_origResourceLoadAssetInternalHash = nullptr;
static void *s_origResourceLoadSubAssetInternalHash = nullptr;
static void *s_origResourceLoadAsyncString = nullptr;
static void *s_origResourceLoadSubAssetAsyncString = nullptr;
static void *s_origResourceLoadAsyncHash = nullptr;
static void *s_origResourceLoadSubAssetAsyncHash = nullptr;
static void *s_origSimpleAssetLoaderLoadAsync = nullptr;
static void *s_origMonoEntitySimpleAssetLoaderLoadAsync = nullptr;
static void *s_origSimpleAssetLoaderTryLoad = nullptr;
static void *s_origMonoEntitySimpleAssetLoaderTryLoad = nullptr;
static void *s_origCachedPathAssetLoaderLoadDirect = nullptr;
static void *s_origCachedPathAssetLoaderTryLoad = nullptr;
static void *s_origPreloadAutoHash = nullptr;
static void *s_origAssetProxyHandlePath = nullptr;
static void *s_origAssetProxyHandleGet = nullptr;
static void *s_origAssetProxyHandleGetAssetProxy = nullptr;
static void *s_origAssetProxyLoaderHandlePath = nullptr;
static void *s_origAssetProxyLoaderHandleGet = nullptr;
static void *s_origAssetProxyLoaderHandleLoadImmediate = nullptr;
static void *s_origAssetProxyLoaderHandleAddOnProxyCompleted = nullptr;
static void *s_origAssetProxyUntrackedPath = nullptr;
static void *s_origAssetProxyUntrackedGet = nullptr;
static void *s_assetProxyUntrackedGetAssetProxy = nullptr;
static void *s_stringPathHashGetPath = nullptr;
static void *s_origVfsLoadBundleFromFile = nullptr;
static void *s_origVfsLoadBundleFromFileAsync = nullptr;
static void *s_origVfsLoadBundleFromFilePos = nullptr;
static void *s_origVfsLoadBundleFromFileAsyncPos = nullptr;
static void *s_origVfsGetAssetStream = nullptr;
static void *s_origVfsGetAssetStreamHash = nullptr;
static void *s_origVfsFileRead = nullptr;
static void *s_origVfsFileReadSpan = nullptr;
static SRWLOCK s_seenStreamLock = SRWLOCK_INIT;
static void *s_seenStreams[128] = {};
static char s_seenStreamPaths[128][768] = {};
static size_t s_seenStreamCount = 0;
static SRWLOCK s_traceHashPathLock = SRWLOCK_INIT;
static int64_t s_traceHashPaths[1024] = {};
static char s_traceHashPathText[1024][768] = {};
static size_t s_traceHashPathCount = 0;
static void *s_origStringPathHashGetMapping = nullptr;
static void *s_origSubMeshInfoGetMesh = nullptr;
static void *s_origSubMeshInfoSetMesh = nullptr;
static void *s_origLodMeshAssetsGetSubMeshInfo = nullptr;
static void *s_origMeshAssetsGetAvatarSlotMeshAssets = nullptr;
static void *s_origMeshAssetsGetAllAvatarSlotMeshAssets = nullptr;


static bool TraceMarkHashFirstSeen(int64_t hash) {
  AcquireSRWLockExclusive(&s_traceSeenHashLock);
  for (size_t i = 0; i < s_traceSeenHashCount; ++i) {
    if (s_traceSeenHashes[i] == hash) {
      ReleaseSRWLockExclusive(&s_traceSeenHashLock);
      return false;
    }
  }
  if (s_traceSeenHashCount < _countof(s_traceSeenHashes))
    s_traceSeenHashes[s_traceSeenHashCount++] = hash;
  ReleaseSRWLockExclusive(&s_traceSeenHashLock);
  return true;
}

static int64_t TraceReadLoadableHash(void *loader) {
  if (!loader) return 0;
  __try {
    return *(int64_t *)((char *)loader + 0x20);
  } __except (1) {
    return 0;
  }
}

static void TraceDescribeObject(void *object, char *out, int outSize) {
  if (!out || outSize <= 0) return;
  out[0] = '\0';
  if (!object) {
    snprintf(out, outSize, "<null>");
    return;
  }

  __try {
    void *klass = il2cpp_object_get_class ? il2cpp_object_get_class(object)
                                          : nullptr;
    const char *ns = (klass && il2cpp_class_get_namespace)
                         ? il2cpp_class_get_namespace(klass)
                         : "";
    const char *name = (klass && il2cpp_class_get_name)
                           ? il2cpp_class_get_name(klass)
                           : "?";
    char objectName[192] = {};
    if (g_object_get_name) {
      void *nameString = Invoke(g_object_get_name, object);
      if (nameString) ReadStrUtf8(nameString, objectName, sizeof(objectName));
    }
    snprintf(out, outSize, "%s.%s @%p name=\"%s\"", ns ? ns : "",
             name ? name : "?", object, objectName[0] ? objectName : "?");
  } __except (1) {
    snprintf(out, outSize, "<invalid @%p>", object);
  }
}

static void TraceDescribeString(void *stringObject, char *out, int outSize) {
  if (!out || outSize <= 0) return;
  out[0] = '\0';
  if (!stringObject) {
    snprintf(out, outSize, "<null>");
    return;
  }
  if (ReadStrUtf8(stringObject, out, outSize) <= 0)
    snprintf(out, outSize, "<invalid string @%p>", stringObject);
}

static void TraceReadUnityObjectName(void *object, char *out, int outSize) {
  if (!out || outSize <= 0) return;
  out[0] = '\0';
  if (!object || !g_object_get_name) return;
  __try {
    void *name = Invoke(g_object_get_name, object);
    if (name) ReadStrUtf8(name, out, outSize);
  } __except (1) {
    out[0] = '\0';
  }
}

static void TraceRememberBundlePathText(const char *pathText) {
  if (!pathText || !pathText[0] || g_shutdownRequested)
    return;

  char normalized[768] = {};
  strncpy_s(normalized, sizeof(normalized), pathText, _TRUNCATE);
  for (char *p = normalized; *p; ++p) {
    if (*p == '\\') *p = '/';
  }

  // Only record logical AssetBundle paths. This keeps accidental diagnostics
  // or absolute filesystem paths out of the extraction manifest.
  size_t length = strlen(normalized);
  if (length < 3 || normalized[0] == '<' ||
      _stricmp(normalized + length - 3, ".ab") != 0)
    return;

  AcquireSRWLockExclusive(&s_bundleManifestLock);
  for (const auto &entry : s_bundleManifestEntries) {
    if (entry.path == normalized) {
      ReleaseSRWLockExclusive(&s_bundleManifestLock);
      return;
    }
  }

  try {
    TraceBundleManifestEntry entry;
    entry.path = normalized;
    entry.firstSeenMs = GetTickCount64();
    s_bundleManifestEntries.push_back(std::move(entry));
  } catch (...) {
    // A manifest is diagnostic/ offline tooling state. Never let an
    // allocation failure affect the game's resource-loading path.
  }
  ReleaseSRWLockExclusive(&s_bundleManifestLock);
}

static void TraceRememberBundlePath(void *path) {
  if (!path || g_shutdownRequested) return;
  char pathText[768] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  TraceRememberBundlePathText(pathText);
}

static bool TraceNormalizeBundlePath(void *path, char *out, int outSize) {
  if (!path || !out || outSize <= 0) return false;
  TraceDescribeString(path, out, outSize);
  if (!out[0] || out[0] == '<') return false;
  for (char *p = out; *p; ++p) {
    if (*p == '\\') *p = '/';
  }
  const size_t length = strlen(out);
  return length >= 3 && _stricmp(out + length - 3, ".ab") == 0;
}

static void TraceRememberActiveBundle(void *bundle, void *path) {
  if (!bundle || g_shutdownRequested) return;
  char normalized[768] = {};
  if (!TraceNormalizeBundlePath(path, normalized, sizeof(normalized))) return;

  AcquireSRWLockExclusive(&s_bundleManifestLock);
  try {
    for (auto &entry : s_activeBundleEntries) {
      if (entry.bundle == bundle) {
        entry.path = normalized;
        ReleaseSRWLockExclusive(&s_bundleManifestLock);
        return;
      }
    }
    TraceActiveBundleEntry entry = {};
    entry.bundle = bundle;
    entry.path = normalized;
    s_activeBundleEntries.push_back(std::move(entry));
  } catch (...) {
  }
  ReleaseSRWLockExclusive(&s_bundleManifestLock);
}

static void TraceForgetActiveBundle(void *bundle) {
  if (!bundle) return;
  AcquireSRWLockExclusive(&s_bundleManifestLock);
  for (size_t i = 0; i < s_activeBundleEntries.size(); ++i) {
    if (s_activeBundleEntries[i].bundle == bundle) {
      s_activeBundleEntries.erase(s_activeBundleEntries.begin() + i);
      break;
    }
  }
  ReleaseSRWLockExclusive(&s_bundleManifestLock);
}

static void TraceRememberPendingBundleRequest(void *request, void *path) {
  if (!request || g_shutdownRequested) return;
  char normalized[768] = {};
  if (!TraceNormalizeBundlePath(path, normalized, sizeof(normalized))) return;
  AcquireSRWLockExclusive(&s_bundleManifestLock);
  try {
    for (auto &entry : s_pendingBundleRequests) {
      if (entry.request == request) {
        entry.path = normalized;
        ReleaseSRWLockExclusive(&s_bundleManifestLock);
        return;
      }
    }
    TracePendingBundleRequestEntry entry = {};
    entry.request = request;
    entry.path = normalized;
    s_pendingBundleRequests.push_back(std::move(entry));
  } catch (...) {
  }
  ReleaseSRWLockExclusive(&s_bundleManifestLock);
}

static void TraceResolvePendingBundleRequest(void *request, void *bundle) {
  if (!request || !bundle) return;
  std::string path;
  AcquireSRWLockExclusive(&s_bundleManifestLock);
  for (size_t i = 0; i < s_pendingBundleRequests.size(); ++i) {
    if (s_pendingBundleRequests[i].request == request) {
      path = s_pendingBundleRequests[i].path;
      s_pendingBundleRequests.erase(s_pendingBundleRequests.begin() + i);
      break;
    }
  }
  if (!path.empty()) {
    bool found = false;
    for (auto &entry : s_activeBundleEntries) {
      if (entry.bundle == bundle) {
        entry.path = path;
        found = true;
        break;
      }
    }
    if (!found) {
      TraceActiveBundleEntry entry = {};
      entry.bundle = bundle;
      entry.path = std::move(path);
      try { s_activeBundleEntries.push_back(std::move(entry)); } catch (...) {}
    }
  }
  ReleaseSRWLockExclusive(&s_bundleManifestLock);
}

static void TraceWriteJsonString(FILE *file, const std::string &value) {
  if (!file) return;
  fputc('"', file);
  for (unsigned char c : value) {
    switch (c) {
    case '"': fputs("\\\"", file); break;
    case '\\': fputs("\\\\", file); break;
    case '\b': fputs("\\b", file); break;
    case '\f': fputs("\\f", file); break;
    case '\n': fputs("\\n", file); break;
    case '\r': fputs("\\r", file); break;
    case '\t': fputs("\\t", file); break;
    default:
      if (c < 0x20)
        fprintf(file, "\\u%04x", (unsigned)c);
      else
        fputc(c, file);
      break;
    }
  }
  fputc('"', file);
}

// Capture only small metadata byte arrays returned by the VFS. This is a
// diagnostic sample, not a resource replacement path: the managed return
// value is left untouched and no managed API is invoked from the hook.
static bool TraceReadByteArrayLength(void *array, uint64_t *length) {
  if (!array || !length) return false;
  __try {
    *length = *(uint64_t *)((char *)array + 24);
    return true;
  } __except (1) {
    return false;
  }
}

static bool TraceMarkStreamFirstRead(void *stream) {
  if (!stream) return false;
  AcquireSRWLockExclusive(&s_seenStreamLock);
  for (size_t i = 0; i < s_seenStreamCount; ++i) {
    if (s_seenStreams[i] == stream) {
      ReleaseSRWLockExclusive(&s_seenStreamLock);
      return false;
    }
  }
  if (s_seenStreamCount >= _countof(s_seenStreams)) {
    ReleaseSRWLockExclusive(&s_seenStreamLock);
    return false;
  }
  s_seenStreams[s_seenStreamCount++] = stream;
  ReleaseSRWLockExclusive(&s_seenStreamLock);
  return true;
}

static void TraceRememberStreamPath(void *stream, void *path) {
  if (!stream || !path) return;
  char pathText[768] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  if (!pathText[0]) return;
  AcquireSRWLockExclusive(&s_seenStreamLock);
  for (size_t i = 0; i < s_seenStreamCount; ++i) {
    if (s_seenStreams[i] == stream) {
      strncpy_s(s_seenStreamPaths[i], sizeof(s_seenStreamPaths[i]), pathText,
                _TRUNCATE);
      ReleaseSRWLockExclusive(&s_seenStreamLock);
      return;
    }
  }
  if (s_seenStreamCount < _countof(s_seenStreams)) {
    size_t i = s_seenStreamCount++;
    s_seenStreams[i] = stream;
    strncpy_s(s_seenStreamPaths[i], sizeof(s_seenStreamPaths[i]), pathText,
              _TRUNCATE);
  }
  ReleaseSRWLockExclusive(&s_seenStreamLock);
}

static void TraceRememberStreamPathText(void *stream, const char *pathText) {
  if (!stream || !pathText || !pathText[0]) return;
  AcquireSRWLockExclusive(&s_seenStreamLock);
  for (size_t i = 0; i < s_seenStreamCount; ++i) {
    if (s_seenStreams[i] == stream) {
      strncpy_s(s_seenStreamPaths[i], sizeof(s_seenStreamPaths[i]), pathText,
                _TRUNCATE);
      ReleaseSRWLockExclusive(&s_seenStreamLock);
      return;
    }
  }
  if (s_seenStreamCount < _countof(s_seenStreams)) {
    size_t i = s_seenStreamCount++;
    s_seenStreams[i] = stream;
    strncpy_s(s_seenStreamPaths[i], sizeof(s_seenStreamPaths[i]), pathText,
              _TRUNCATE);
  }
  ReleaseSRWLockExclusive(&s_seenStreamLock);
}

static void TraceRememberHashPath(int64_t hash, const char *pathText) {
  if (!pathText || !pathText[0]) return;
  AcquireSRWLockExclusive(&s_traceHashPathLock);
  for (size_t i = 0; i < s_traceHashPathCount; ++i) {
    if (s_traceHashPaths[i] == hash) {
      strncpy_s(s_traceHashPathText[i], sizeof(s_traceHashPathText[i]),
                pathText, _TRUNCATE);
      ReleaseSRWLockExclusive(&s_traceHashPathLock);
      return;
    }
  }
  if (s_traceHashPathCount < _countof(s_traceHashPaths)) {
    size_t i = s_traceHashPathCount++;
    s_traceHashPaths[i] = hash;
    strncpy_s(s_traceHashPathText[i], sizeof(s_traceHashPathText[i]), pathText,
              _TRUNCATE);
  }
  ReleaseSRWLockExclusive(&s_traceHashPathLock);
}

static void TraceLookupHashPath(int64_t hash, char *out, int outSize) {
  if (!out || outSize <= 0) return;
  out[0] = '\0';
  AcquireSRWLockShared(&s_traceHashPathLock);
  for (size_t i = 0; i < s_traceHashPathCount; ++i) {
    if (s_traceHashPaths[i] == hash) {
      strncpy_s(out, (size_t)outSize, s_traceHashPathText[i], _TRUNCATE);
      break;
    }
  }
  ReleaseSRWLockShared(&s_traceHashPathLock);
}

// The hash overload of BundleResourceManager receives StringPathHash after
// its implicit conversion to Int64.  Reconstruct the value type locally and
// ask the game's own getter for the logical path.  This keeps path identity
// in the game's hashing/mapping implementation instead of duplicating it in
// the plugin.  The call is best-effort: a missing mapping leaves the hash in
// the origin table and does not alter the original load result.
static bool TraceResolveStringPathHashPath(int64_t hash, char *out,
                                           size_t outSize) {
  if (!out || outSize == 0) return false;
  out[0] = '\0';
  if (!hash || !s_stringPathHashGetPath || !il2cpp_runtime_invoke) return false;
  struct StringPathHashValue {
    int64_t hash;
  } value = {hash};
  __try {
    void *pathObject = Invoke(s_stringPathHashGetPath, &value, nullptr);
    TraceDescribeString(pathObject, out, (int)outSize);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    out[0] = '\0';
  }
  if (!out[0] || out[0] == '<') return false;
  for (char *p = out; *p; ++p)
    if (*p == '\\') *p = '/';
  TraceRememberHashPath(hash, out);
  return true;
}

static void TraceRememberProxyOrigin(void *proxy, int64_t pathHash,
                                     const char *path) {
  if (!proxy || g_shutdownRequested) return;
  AcquireSRWLockExclusive(&s_assetOriginLock);
  constexpr size_t ways = 4;
  const size_t bucketCount = _countof(s_proxyOrigins) / ways;
  const size_t base = (((uintptr_t)proxy >> 4) & (bucketCount - 1)) * ways;
  size_t chosen = base;
  for (size_t way = 0; way < ways; ++way) {
    const size_t index = base + way;
    if (s_proxyOrigins[index].proxy == proxy) { chosen = index; break; }
    if (!s_proxyOrigins[index].proxy) { chosen = index; break; }
    if (s_proxyOrigins[index].stamp < s_proxyOrigins[chosen].stamp)
      chosen = index;
  }
  TraceProxyOriginEntry &entry = s_proxyOrigins[chosen];
  if (entry.proxy != proxy) entry = {};
  entry.proxy = proxy;
  entry.stamp = (uint64_t)InterlockedIncrement64(&s_assetOriginStamp);
  if (pathHash) entry.pathHash = pathHash;
  if (path && path[0])
    strncpy_s(entry.path, sizeof(entry.path), path, _TRUNCATE);
  ReleaseSRWLockExclusive(&s_assetOriginLock);
}

static bool TraceLookupProxyOrigin(void *proxy, int64_t *pathHash,
                                   char *path, size_t pathSize) {
  if (pathHash) *pathHash = 0;
  if (path && pathSize) path[0] = '\0';
  if (!proxy) return false;
  bool found = false;
  AcquireSRWLockShared(&s_assetOriginLock);
  constexpr size_t ways = 4;
  const size_t bucketCount = _countof(s_proxyOrigins) / ways;
  const size_t base = (((uintptr_t)proxy >> 4) & (bucketCount - 1)) * ways;
  for (size_t way = 0; way < ways; ++way) {
    const TraceProxyOriginEntry &entry = s_proxyOrigins[base + way];
    if (entry.proxy != proxy) continue;
    if (pathHash) *pathHash = entry.pathHash;
    if (path && pathSize)
      strncpy_s(path, pathSize, entry.path, _TRUNCATE);
    found = true;
    break;
  }
  ReleaseSRWLockShared(&s_assetOriginLock);
  if (found && path && pathSize && !path[0] && pathHash && *pathHash)
    TraceLookupHashPath(*pathHash, path, (int)pathSize);
  return found && ((path && pathSize && path[0]) ||
                   (pathHash && *pathHash));
}

static void TraceRememberAssetOrigin(void *asset, int64_t pathHash,
                                     const char *path) {
  if (!asset || g_shutdownRequested) return;
  AcquireSRWLockExclusive(&s_assetOriginLock);
  constexpr size_t ways = 4;
  const size_t bucketCount = _countof(s_assetOrigins) / ways;
  const size_t base = (((uintptr_t)asset >> 4) & (bucketCount - 1)) * ways;
  size_t chosen = base;
  for (size_t way = 0; way < ways; ++way) {
    const size_t index = base + way;
    if (s_assetOrigins[index].asset == asset) { chosen = index; break; }
    if (!s_assetOrigins[index].asset) { chosen = index; break; }
    if (s_assetOrigins[index].stamp < s_assetOrigins[chosen].stamp)
      chosen = index;
  }
  TraceAssetOriginEntry &entry = s_assetOrigins[chosen];
  if (entry.asset != asset) entry = {};
  entry.asset = asset;
  entry.stamp = (uint64_t)InterlockedIncrement64(&s_assetOriginStamp);
  if (pathHash) entry.pathHash = pathHash;
  if (path && path[0] && path[0] != '<')
    strncpy_s(entry.path, sizeof(entry.path), path, _TRUNCATE);
  ReleaseSRWLockExclusive(&s_assetOriginLock);
}

static bool TraceBindAssetFromProxy(void *proxy, void *asset) {
  if (!proxy || !asset) return false;
  int64_t pathHash = 0;
  char path[768] = {};
  AcquireSRWLockShared(&s_assetOriginLock);
  constexpr size_t ways = 4;
  const size_t bucketCount = _countof(s_proxyOrigins) / ways;
  const size_t base = (((uintptr_t)proxy >> 4) & (bucketCount - 1)) * ways;
  for (size_t way = 0; way < ways; ++way) {
    const TraceProxyOriginEntry &entry = s_proxyOrigins[base + way];
    if (entry.proxy == proxy) {
      pathHash = entry.pathHash;
      strncpy_s(path, sizeof(path), entry.path, _TRUNCATE);
      break;
    }
  }
  ReleaseSRWLockShared(&s_assetOriginLock);
  if (!path[0] && pathHash)
    TraceLookupHashPath(pathHash, path, (int)sizeof(path));
  if (!pathHash && !path[0]) return false;
  TraceRememberAssetOrigin(asset, pathHash, path);
  return true;
}

static bool TraceLookupAssetOrigin(void *asset, int64_t *pathHash, char *path,
                                   size_t pathSize) {
  if (pathHash) *pathHash = 0;
  if (path && pathSize) path[0] = '\0';
  if (!asset) return false;
  bool found = false;
  AcquireSRWLockShared(&s_assetOriginLock);
  constexpr size_t ways = 4;
  const size_t bucketCount = _countof(s_assetOrigins) / ways;
  const size_t base = (((uintptr_t)asset >> 4) & (bucketCount - 1)) * ways;
  for (size_t way = 0; way < ways; ++way) {
    const TraceAssetOriginEntry &entry = s_assetOrigins[base + way];
    if (entry.asset == asset) {
      if (pathHash) *pathHash = entry.pathHash;
      if (path && pathSize)
        strncpy_s(path, pathSize, entry.path, _TRUNCATE);
      found = true;
      break;
    }
  }
  ReleaseSRWLockShared(&s_assetOriginLock);
  return found;
}

static void TraceLookupStreamPath(void *stream, char *out, int outSize) {
  if (!out || outSize <= 0) return;
  out[0] = '\0';
  AcquireSRWLockShared(&s_seenStreamLock);
  for (size_t i = 0; i < s_seenStreamCount; ++i) {
    if (s_seenStreams[i] == stream) {
      strncpy_s(out, (size_t)outSize, s_seenStreamPaths[i], _TRUNCATE);
      break;
    }
  }
  ReleaseSRWLockShared(&s_seenStreamLock);
}

typedef int (__fastcall *TraceVfsReadFn)(void *self, void *buffer, int offset,
                                         int count, void *methodInfo);

// VFSFileReadStream.Read is the first managed boundary where decrypted bytes
// are copied into a caller-owned byte[]. Capture one initial chunk per stream
// for format identification; the original buffer and return value are never
// changed.
static int TraceVfsFileRead(void *self, void *buffer, int offset, int count,
                            void *methodInfo) {
  auto original = (TraceVfsReadFn)s_origVfsFileRead;
  int result = original ? original(self, buffer, offset, count, methodInfo) : 0;
  if (!kEiemValidationVfsCapture) return result;
  if (result <= 0 || !buffer || !TraceMarkStreamFirstRead(self)) return result;

  LONG slot = InterlockedIncrement(&s_traceStreamCaptureCount);
  if (slot > 32) return result;
  uint64_t length = 0;
  if (!TraceReadByteArrayLength(buffer, &length) || length < 32 ||
      offset < 0 || (uint64_t)offset >= length)
    return result;
  uint64_t available = length - (uint64_t)offset;
  uint64_t bytes = (uint64_t)result;
  if (bytes > available) bytes = available;
  if (bytes > 1024ull * 1024ull) bytes = 1024ull * 1024ull;
  if (bytes == 0) return result;

  std::vector<uint8_t> copy((size_t)bytes);
  memcpy(copy.data(), (char *)buffer + 32 + offset, (size_t)bytes);
  CreateDirectoryA("plugin", nullptr);
  CreateDirectoryA("plugin\\captures", nullptr);
  char outPath[256] = {};
  snprintf(outPath, sizeof(outPath), "plugin\\captures\\stream_%03ld.bin",
           (long)slot);
  HANDLE file = CreateFileA(outPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return result;
  DWORD written = 0;
  BOOL ok = WriteFile(file, copy.data(), (DWORD)copy.size(), &written, nullptr);
  CloseHandle(file);
  if (ok && written == copy.size()) {
    unsigned char *h = copy.data();
    char sourcePath[768] = {};
    TraceLookupStreamPath(self, sourcePath, sizeof(sourcePath));
    Log("[RES-CAPTURE] VFS stream=%p path=\"%s\" read=%d "
        "head=%02X%02X%02X%02X file=%s",
        self, sourcePath[0] ? sourcePath : "?", result, h[0], h[1], h[2], h[3],
        outPath);
  }
  return result;
}

// Span<T> is a 16-byte value type in the Windows x64 IL2CPP ABI.  IL2CPP
// passes this aggregate by reference, with the data pointer at +0 and the
// logical length at +8.  Validate both objects before touching them because
// this hook runs on VFS worker threads and must never turn a probe into a
// crash.
static bool TraceReadByteSpan(void *span, void **data, int32_t *length) {
  if (!span || !data || !length) return false;
  MEMORY_BASIC_INFORMATION spanInfo = {};
  if (VirtualQuery(span, &spanInfo, sizeof(spanInfo)) != sizeof(spanInfo) ||
      spanInfo.State != MEM_COMMIT ||
      (spanInfo.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0 ||
      (uintptr_t)span + 16 < (uintptr_t)span)
    return false;
  uintptr_t spanEnd = (uintptr_t)span + 16;
  uintptr_t regionEnd = (uintptr_t)spanInfo.BaseAddress +
                        spanInfo.RegionSize;
  if (spanEnd > regionEnd) return false;

  void *pointer = nullptr;
  int32_t size = 0;
  __try {
    pointer = *(void **)span;
    size = *(int32_t *)((char *)span + 8);
  } __except (1) {
    return false;
  }
  if (size < 0 || size > 64 * 1024 * 1024) return false;
  if (size > 0) {
    MEMORY_BASIC_INFORMATION dataInfo = {};
    if (!pointer || VirtualQuery(pointer, &dataInfo, sizeof(dataInfo)) !=
                        sizeof(dataInfo) ||
        dataInfo.State != MEM_COMMIT ||
        (dataInfo.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
      return false;
    uintptr_t dataEnd = (uintptr_t)pointer + (uintptr_t)size;
    uintptr_t dataRegionEnd = (uintptr_t)dataInfo.BaseAddress +
                              dataInfo.RegionSize;
    if (dataEnd < (uintptr_t)pointer || dataEnd > dataRegionEnd) return false;
  }
  *data = pointer;
  *length = size;
  return true;
}

typedef int (__fastcall *TraceVfsReadSpanFn)(void *self, void *span,
                                              void *methodInfo);

static int TraceVfsFileReadSpan(void *self, void *span, void *methodInfo) {
  auto original = (TraceVfsReadSpanFn)s_origVfsFileReadSpan;
  int result = original ? original(self, span, methodInfo) : 0;
  if (!kEiemValidationVfsCapture) return result;
  if (result <= 0 || !span) return result;

  void *data = nullptr;
  int32_t length = 0;
  if (!TraceReadByteSpan(span, &data, &length) || !data || length <= 0)
    return result;
  if (!TraceMarkStreamFirstRead(self)) return result;
  uint64_t bytes = (uint64_t)result;
  if (bytes > (uint64_t)length) bytes = (uint64_t)length;
  if (bytes > 1024ull * 1024ull) bytes = 1024ull * 1024ull;
  if (bytes == 0) return result;

  LONG slot = InterlockedIncrement(&s_traceStreamCaptureCount);
  if (slot > 32) return result;
  CreateDirectoryA("plugin", nullptr);
  CreateDirectoryA("plugin\\captures", nullptr);
  char outPath[256] = {};
  snprintf(outPath, sizeof(outPath), "plugin\\captures\\span_%03ld.bin",
           (long)slot);
  HANDLE file = CreateFileA(outPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return result;
  DWORD written = 0;
  BOOL ok = WriteFile(file, data, (DWORD)bytes, &written, nullptr);
  CloseHandle(file);
  if (ok && written == bytes) {
    unsigned char *head = (unsigned char *)data;
    char sourcePath[768] = {};
    TraceLookupStreamPath(self, sourcePath, sizeof(sourcePath));
    Log("[RES-CAPTURE] VFS span stream=%p path=\"%s\" read=%d "
        "span=%d head=%02X%02X%02X%02X file=%s",
        self, sourcePath[0] ? sourcePath : "?", result, length, head[0],
        head[1], head[2], head[3], outPath);
  }
  return result;
}

typedef void *(__fastcall *TraceLoadAsset1Fn)(void *self, void *path,
                                               void *methodInfo);
typedef void *(__fastcall *TraceLoadAsset2Fn)(void *self, void *path,
                                               void *type, void *methodInfo);
typedef void *(__fastcall *TraceBundleLoadFn)(void *self, void *path,
                                               void *methodInfo);
typedef void (__fastcall *TraceBundleSetFn)(void *self, void *bundle,
                                             void *methodInfo);
typedef void *(__fastcall *TraceBundleGetPathFn)(void *self, void *path,
                                                 void *methodInfo);
typedef void (__fastcall *TraceBundleFinishFn)(void *self, void *bundle,
                                                void *methodInfo);
typedef void *(__fastcall *TraceAssetGetNameFn)(void *self, void *methodInfo);
typedef void (__fastcall *TraceAssetFinishFn)(void *self, void *asset,
                                               void *methodInfo);
typedef void (__fastcall *TraceVoidMethodFn)(void *self, void *methodInfo);
typedef void *(__fastcall *TraceResourceLoadAssetFn)(
    void *self, void *path, void *type, int category, bool immediate,
    int priority, void *methodInfo);
typedef void *(__fastcall *TraceResourceLoadSubAssetFn)(
    void *self, void *path, void *subAsset, void *type, int category,
    bool immediate, int priority, void *methodInfo);
typedef int64_t (__fastcall *TracePathHashFn)(void *path, void *methodInfo);
typedef void *(__fastcall *TraceResourceLoadAssetHashFn)(
    void *self, int64_t pathHash, void *type, int category, bool immediate,
    int priority, void *methodInfo);
typedef void *(__fastcall *TraceResourceLoadSubAssetHashFn)(
    void *self, int64_t pathHash, void *subAsset, void *type, int category,
    bool immediate, int priority, void *methodInfo);
// The callback overloads return void and are therefore safe observation
// boundaries even when FAssetProxyHandle is an IL2CPP value type.  Keep the
// callback opaque: invoking or wrapping it would change the game's loader
// ordering, so these probes only record the request and pass through.
typedef void (__fastcall *TraceResourceLoadAsyncStringFn)(
    void *self, int32_t logChannel, void *path, void *type, int category,
    void *callback, int priority, void *methodInfo);
typedef void (__fastcall *TraceResourceLoadSubAssetAsyncStringFn)(
    void *self, int32_t logChannel, void *path, void *subAsset, void *type,
    int category, void *callback, int priority, void *methodInfo);
typedef void (__fastcall *TraceResourceLoadAsyncHashFn)(
    void *self, int32_t logChannel, int64_t pathHash, void *type, int category,
    void *callback, int priority, void *methodInfo);
typedef void (__fastcall *TraceResourceLoadSubAssetAsyncHashFn)(
    void *self, int32_t logChannel, int64_t pathHash, void *subAsset,
    void *type, int category, void *callback, int priority, void *methodInfo);
typedef void (__fastcall *TraceAssetLoaderAsyncHashFn)(
    void *self, int64_t pathHash, void *type, void *callback, int priority,
    void *methodInfo);
// TryLoad writes the value-type handle through an explicit out pointer and
// returns bool, so it is safe to observe without guessing the value-return ABI
// used by Load(...)->FAssetProxyLoaderHandle.
typedef bool (__fastcall *TraceAssetLoaderTryLoadHashFn)(
    void *self, int64_t pathHash, void *type, void *outHandle,
    void *methodInfo);
typedef void *(__fastcall *TraceCachedLoaderLoadDirectFn)(
    void *self, void *path, void *type, void *methodInfo);
typedef bool (__fastcall *TraceCachedLoaderTryLoadStringFn)(
    void *self, void *path, void *type, void *outHandle, void *methodInfo);
typedef void (__fastcall *TracePreloadAutoHashFn)(void *self, int64_t pathHash,
                                                   void *methodInfo);
typedef void *(__fastcall *TraceProxyObjectFn)(void *self, void *methodInfo);
typedef void (__fastcall *TraceProxyLoaderAddCompletedFn)(
    void *self, int32_t logChannel, void *callback, void *methodInfo);
typedef void *(__fastcall *TraceV11DescriptorGetMeshFn)(void *self,
                                                         void *methodInfo);
typedef void (__fastcall *TraceV11DescriptorSetMeshFn)(void *self, void *mesh,
                                                         void *methodInfo);
typedef void *(__fastcall *TraceLodMeshAssetsGetSubMeshInfoFn)(
    void *self, int32_t lod, bool gpu, void *methodInfo);
typedef void *(__fastcall *TraceVfsPathFn)(void *self, void *path,
                                           void *methodInfo);
typedef void *(__fastcall *TraceVfsPathPosFn)(void *self, void *path,
                                              void *loaderPos, uint32_t crc,
                                              void *methodInfo);
typedef void (__fastcall *TraceAssetBundleUnloadFn)(void *self, bool unloadAll,
                                                    void *methodInfo);
typedef void *(__fastcall *TraceAssetBundleCreateRequestGetAssetBundleFn)(
    void *self, void *methodInfo);
typedef bool (__fastcall *TraceHashMappingFn)(void *self, int64_t pathHash,
                                               void *outPath,
                                               void *methodInfo);

static bool TraceStringPathHashGetMapping(void *self, int64_t pathHash,
                                          void *outPath, void *methodInfo) {
  auto original = (TraceHashMappingFn)s_origStringPathHashGetMapping;
  bool result = original ? original(self, pathHash, outPath, methodInfo) : false;
  if (!result || !outPath || g_shutdownRequested) return result;

  void *pathObject = nullptr;
  __try { pathObject = *(void **)outPath; }
  __except (1) { pathObject = nullptr; }
  char pathText[768] = {};
  TraceDescribeString(pathObject, pathText, sizeof(pathText));
  if (pathText[0] && pathText[0] != '<') {
    TraceRememberHashPath(pathHash, pathText);

  }
  return result;
}

static void TraceReadAssetName(void *loader, char *out, int outSize) {
  if (!out || outSize <= 0) return;
  out[0] = '\0';
  auto original = (TraceAssetGetNameFn)s_origAssetGetAssetName;
  if (!original || !loader) return;
  __try {
    TraceDescribeString(original(loader, nullptr), out, outSize);
  } __except (1) {
    snprintf(out, outSize, "<unavailable>");
  }
}

static int64_t TraceGetAssetPathHash(void *path, void *methodInfo) {
  auto original = (TracePathHashFn)s_origGetAssetPathHash;
  int64_t hash = original ? original(path, methodInfo) : 0;

  return hash;
}

static int64_t TraceGetAssetPathHashWithoutBurst(void *path, void *methodInfo) {
  auto original = (TracePathHashFn)s_origGetAssetPathHashWithoutBurst;
  int64_t hash = original ? original(path, methodInfo) : 0;

  return hash;
}

static void *TraceAssetProxyHandlePath(void *self, void *methodInfo) {
  auto original = (TraceProxyObjectFn)s_origAssetProxyHandlePath;
  void *result = original ? original(self, methodInfo) : nullptr;
  char resolvedPath[768] = {};
  TraceDescribeString(result, resolvedPath, sizeof(resolvedPath));
  if (resolvedPath[0] && resolvedPath[0] != '<')
    TraceRememberProxyOrigin(self, 0, resolvedPath);

  return result;
}

static bool EiemUpstreamMeshObject(void *object) {
  if (!object || !s_eiemMeshClass || !il2cpp_object_get_class) return false;
  void *klass = nullptr;
  __try { klass = il2cpp_object_get_class(object); }
  __except (EXCEPTION_EXECUTE_HANDLER) { klass = nullptr; }
  for (int depth = 0; klass && depth < 12; ++depth) {
    if (klass == s_eiemMeshClass) return true;
    klass = il2cpp_class_get_parent ? il2cpp_class_get_parent(klass) : nullptr;
  }
  return false;
}


// Replace a Mesh at the resource return boundary.  This is deliberately
// keyed by the game's logical asset identity (proxy path or Mesh name), not by
// vertex counts or a Renderer address.  The game therefore continues to own
// PFB/UI/world construction, LOD selection, material registration and skin
// submission; EIEM only changes the Mesh object that crosses the boundary.


static void *TraceAssetProxyHandleGet(void *self, void *methodInfo) {
  auto original = (TraceProxyObjectFn)s_origAssetProxyHandleGet;
  void *result = original ? original(self, methodInfo) : nullptr;
  char pathText[768] = {};
  int64_t pathHash = 0;
  auto pathGetter = (TraceProxyObjectFn)s_origAssetProxyHandlePath;
  if (pathGetter)
    TraceDescribeString(pathGetter(self, nullptr), pathText,
                        sizeof(pathText));
  if (!pathText[0] || pathText[0] == '<')
    TraceLookupProxyOrigin(self, &pathHash, pathText, sizeof(pathText));
  // FAssetProxyHandle is a value type.  The address passed to Get() is a
  // short-lived handle copy, while the resource manager recorded the origin
  // on its heap-allocated AssetProxy object.  Resolve that real proxy before
  // attempting the redirect; otherwise every cached handle is indistinguish-
  // able from an unrelated request.
  if ((!pathText[0] || pathText[0] == '<') &&
      s_origAssetProxyHandleGetAssetProxy) {
    void *proxy = ((TraceProxyObjectFn)s_origAssetProxyHandleGetAssetProxy)(
        self, nullptr);
    if (proxy)
      TraceLookupProxyOrigin(proxy, &pathHash, pathText, sizeof(pathText));
  }
  if ((!pathText[0] || pathText[0] == '<') && pathHash)
    TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if ((!pathText[0] || pathText[0] == '<') && pathHash &&
      EiemOnUnityThread())
    TraceResolveStringPathHashPath(pathHash, pathText, sizeof(pathText));
  if (result) {
    if (!TraceBindAssetFromProxy(self, result)) {
      if (pathText[0] && pathText[0] != '<')
        TraceRememberProxyOrigin(self, 0, pathText);
      TraceBindAssetFromProxy(self, result);
    }

  }

  return result;
}

static void *TraceAssetProxyHandleGetAssetProxy(void *self, void *methodInfo) {
  auto original = (TraceProxyObjectFn)s_origAssetProxyHandleGetAssetProxy;
  void *result = original ? original(self, methodInfo) : nullptr;

  return result;
}

static bool TraceIdentityTextMatchesConfiguredRule(const char *text) {
  if (!text || !text[0]) return false;
  std::string identity(text);
  std::transform(identity.begin(), identity.end(), identity.begin(),
                 [](unsigned char value) { return (char)std::tolower(value); });
  std::vector<EiemModRule> rules;
  EiemFindStandaloneRenderRules(&rules);
  for (const auto &rule : rules) {
    for (const char *selector : {rule.asset, rule.path}) {
      if (!selector || !selector[0]) continue;
      std::string candidate(selector);
      std::transform(candidate.begin(), candidate.end(), candidate.begin(),
                     [](unsigned char value) {
                       return (char)std::tolower(value);
                     });
      if (identity.find(candidate) != std::string::npos) return true;
    }
  }
  return false;
}

// Resource containers use a prefab/part naming family (for example P_*),
// while the replacement rule usually names the rendered S_* asset. Reuse the
// configured rule family for diagnostics without hard-coding a character.
static bool TraceDescriptorTextMatchesConfiguredFamily(const char *text) {
  if (!text || !text[0]) return false;
  std::string identity(text);
  std::transform(identity.begin(), identity.end(), identity.begin(),
                 [](unsigned char value) { return (char)std::tolower(value); });
  std::vector<EiemModRule> rules;
  EiemFindStandaloneRenderRules(&rules);
  for (const auto &rule : rules) {
    const char *selectors[] = {rule.asset, rule.path};
    for (const char *selector : selectors) {
      if (!selector || !selector[0]) continue;
      std::string candidate(selector);
      std::transform(candidate.begin(), candidate.end(), candidate.begin(),
                     [](unsigned char value) {
                       return (char)std::tolower(value);
                     });
      const size_t familyStart = candidate.find("actor_");
      if (familyStart == std::string::npos) continue;
      size_t familyEnd = candidate.find("_lod", familyStart);
      if (familyEnd == std::string::npos) familyEnd = candidate.size();
      const std::string family = candidate.substr(familyStart,
                                                   familyEnd - familyStart);
      if (family.size() > 6 && identity.find(family) != std::string::npos)
        return true;
    }
  }
  return false;
}

static bool TraceDescriptorTextMatchesTarget(const char *text) {
  return TraceIdentityTextMatchesConfiguredRule(text) ||
         TraceDescriptorTextMatchesConfiguredFamily(text);
}

static bool TraceTakeTargetBudget(volatile LONG *counter, LONG limit,
                                  const char *primary,
                                  const char *secondary) {
  if (!kEiemValidationIdentityProbe ||
      (!TraceIdentityTextMatchesConfiguredRule(primary) &&
       !TraceIdentityTextMatchesConfiguredRule(secondary)))
    return false;
  return InterlockedIncrement(counter) <= limit;
}

static void TraceSubMeshInfoIdentity(void *info, const char *event,
                                     void *meshOverride) {
  if (!info || !kEiemEnableDescriptorDiagnostics) return;
  __try {
    const LONG sample = InterlockedIncrement(&s_eiemDescriptorInfoCallCount);
    void *nameObject = *(void **)((char *)info + 0x30);
    char name[192] = {};
    if (nameObject) ReadStrUtf8(nameObject, name, sizeof(name));
    bool targeted = TraceDescriptorTextMatchesTarget(name);
    if (!targeted && sample > 80) return;
    void *mesh = meshOverride ? meshOverride
                              : *(void **)((char *)info + 0x10);
    char meshObjectName[192] = {};
    if (mesh) TraceReadUnityObjectName(mesh, meshObjectName,
                                       sizeof(meshObjectName));
    if (!targeted &&
        TraceDescriptorTextMatchesTarget(meshObjectName))
      targeted = true;
    const int64_t pathHash = *(int64_t *)((char *)info + 0x28);
    const int active = *(bool *)((char *)info + 0x58) ? 1 : 0;
    const int disabled = *(bool *)((char *)info + 0x6D) ? 1 : 0;
    const int32_t rootBoneId = *(int32_t *)((char *)info + 0x68);
    Log("[V1.1-DESCRIPTOR] sample=%ld targeted=%d event=%s info=%p "
        "name=%s mesh=%p meshObjectName=%s meshPathHash=%lld active=%d "
        "rendererDisabled=%d rootBoneID=%d",
        sample, targeted ? 1 : 0,
        event ? event : "unknown", info, name[0] ? name : "<empty>", mesh,
        meshObjectName[0] ? meshObjectName : "<empty>", (long long)pathHash,
        active, disabled, rootBoneId);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

static void *TraceV11DescriptorGetMesh(void *self, void *methodInfo) {
  auto original = (TraceV11DescriptorGetMeshFn)s_origSubMeshInfoGetMesh;
  void *result = original ? original(self, methodInfo) : nullptr;

  TraceSubMeshInfoIdentity(self, "SubMeshInfo.get_mesh", result);
  return result;
}

static void TraceV11DescriptorSetMesh(void *self, void *mesh, void *methodInfo) {
  auto original = (TraceV11DescriptorSetMeshFn)s_origSubMeshInfoSetMesh;
  if (original) original(self, mesh, methodInfo);
  TraceSubMeshInfoIdentity(self, "SubMeshInfo.set_mesh", mesh);
}

static void *TraceLodMeshAssetsGetSubMeshInfo(void *self, int32_t lod, bool gpu,
                                              void *methodInfo) {
  auto original = (TraceLodMeshAssetsGetSubMeshInfoFn)
      s_origLodMeshAssetsGetSubMeshInfo;
  void *result = original ? original(self, lod, gpu, methodInfo) : nullptr;
  if (!self || !kEiemEnableDescriptorDiagnostics) return result;
  __try {
    const LONG sample = InterlockedIncrement(&s_eiemDescriptorAssetsCallCount);
    void *nameObject = *(void **)((char *)self + 0x10);
    char ownerName[192] = {};
    if (nameObject) ReadStrUtf8(nameObject, ownerName, sizeof(ownerName));
    const bool targeted = TraceDescriptorTextMatchesTarget(ownerName);
    if (!targeted && sample > 80) return result;
    const size_t count = EiemManagedArrayLength(result);
    Log("[V1.1-DESCRIPTOR] sample=%ld targeted=%d "
        "event=NPCAvatarLodMeshAssets.GetSubMeshInfo owner=%p "
        "ownerName=%s lod=%d gpu=%d array=%p count=%zu",
        sample, targeted ? 1 : 0, self,
        ownerName[0] ? ownerName : "<empty>", lod, gpu ? 1 : 0, result,
        count);
    if (!result || count > 128) return result;
    void **items = (void **)((char *)result + IL2CPP_ARRAY_DATA);
    for (size_t index = 0; index < count; ++index)
      TraceSubMeshInfoIdentity(items[index], "GetSubMeshInfo.item", nullptr);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  return result;
}

static int TraceManagedListCount(void *list) {
  if (!list) return 0;
  __try { return *(int *)((char *)list + 0x18); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static void *TraceMeshAssetsListGetter(void *self, void *methodInfo,
                                       void *originalPtr,
                                       const char *event) {
  auto original = (TraceProxyObjectFn)originalPtr;
  void *result = original ? original(self, methodInfo) : nullptr;
  if (!self || !kEiemValidationIdentityProbe) return result;
  __try {
    void *pathObject = *(void **)((char *)self + 0x20);
    char path[768] = {};
    if (pathObject) ReadStrUtf8(pathObject, path, sizeof(path));
    if (!TraceIdentityTextMatchesConfiguredRule(path)) {
      void *nameObject = *(void **)((char *)self + 0x50);
      if (nameObject) ReadStrUtf8(nameObject, path, sizeof(path));
    }
    if (TraceIdentityTextMatchesConfiguredRule(path))
      Log("[V1.1-DESCRIPTOR] event=%s assets=%p identity=%s list=%p count=%d",
          event ? event : "mesh-assets", self, path[0] ? path : "<empty>",
          result, TraceManagedListCount(result));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  return result;
}

static void *TraceMeshAssetsGetAvatarSlotMeshAssets(void *self,
                                                    void *methodInfo) {
  return TraceMeshAssetsListGetter(self, methodInfo,
      s_origMeshAssetsGetAvatarSlotMeshAssets,
      "NPCAvatarMeshAssetsSO.GetAvatarSlotMeshAssets");
}

static void *TraceMeshAssetsGetAllAvatarSlotMeshAssets(void *self,
                                                       void *methodInfo) {
  return TraceMeshAssetsListGetter(self, methodInfo,
      s_origMeshAssetsGetAllAvatarSlotMeshAssets,
      "NPCAvatarMeshAssetsSO.GetAllAvatarSlotMeshAssets");
}

static void *TraceAssetProxyLoaderHandlePath(void *self, void *methodInfo) {
  auto original = (TraceProxyObjectFn)s_origAssetProxyLoaderHandlePath;
  return original ? original(self, methodInfo) : nullptr;
}

static void *TraceAssetProxyLoaderHandleGet(void *self, void *methodInfo) {
  auto original = (TraceProxyObjectFn)s_origAssetProxyLoaderHandleGet;
  void *result = original ? original(self, methodInfo) : nullptr;
  char path[768] = {};
  auto pathGetter = (TraceProxyObjectFn)s_origAssetProxyLoaderHandlePath;
  if (pathGetter) TraceDescribeString(pathGetter(self, nullptr), path,
                                       sizeof(path));

  if (!kEiemValidationIdentityProbe) return result;
  char objectName[192] = {};
  TraceReadUnityObjectName(result, objectName, sizeof(objectName));
  if (TraceIdentityTextMatchesConfiguredRule(path) ||
      TraceIdentityTextMatchesConfiguredRule(objectName))
    Log("[V1.1-LOADER] event=FAssetProxyLoaderHandle.Get handle=%p "
        "path=%s object=%p objectName=%s",
        self, path[0] ? path : "<none>", result,
        objectName[0] ? objectName : "<empty>");
  return result;
}

static void TraceAssetProxyLoaderHandleLoadImmediate(void *self,
                                                     void *methodInfo) {
  auto original = (TraceVoidMethodFn)s_origAssetProxyLoaderHandleLoadImmediate;
  if (original) original(self, methodInfo);
  if (!kEiemValidationIdentityProbe) return;
  auto pathGetter = (TraceProxyObjectFn)s_origAssetProxyLoaderHandlePath;
  char path[768] = {};
  if (pathGetter) TraceDescribeString(pathGetter(self, nullptr), path,
                                      sizeof(path));
  if (TraceIdentityTextMatchesConfiguredRule(path))
    Log("[V1.1-LOADER] event=FAssetProxyLoaderHandle.LoadImmediate "
        "handle=%p path=%s", self, path);
}

static void TraceAssetProxyLoaderHandleAddOnProxyCompleted(
    void *self, int32_t logChannel, void *callback, void *methodInfo) {
  auto original = (TraceProxyLoaderAddCompletedFn)
      s_origAssetProxyLoaderHandleAddOnProxyCompleted;
  if (original) original(self, logChannel, callback, methodInfo);
  if (!kEiemValidationIdentityProbe) return;
  auto pathGetter = (TraceProxyObjectFn)s_origAssetProxyLoaderHandlePath;
  char path[768] = {};
  if (pathGetter) TraceDescribeString(pathGetter(self, nullptr), path,
                                      sizeof(path));
  if (TraceIdentityTextMatchesConfiguredRule(path))
    Log("[V1.1-LOADER] event=FAssetProxyLoaderHandle.AddOnProxyCompleted "
        "handle=%p path=%s callback=%p", self, path, callback);
}

static void *TraceAssetProxyUntrackedGet(void *self, void *methodInfo) {
  auto original = (TraceProxyObjectFn)s_origAssetProxyUntrackedGet;
  void *result = original ? original(self, methodInfo) : nullptr;
  char pathText[768] = {};
  auto pathGetter = (TraceProxyObjectFn)s_origAssetProxyUntrackedPath;
  if (!pathGetter)
    pathGetter = (TraceProxyObjectFn)s_origAssetProxyHandlePath;
  if (pathGetter)
    TraceDescribeString(pathGetter(self, nullptr), pathText,
                        sizeof(pathText));
  int64_t pathHash = 0;
  if (!pathText[0] || pathText[0] == '<')
    TraceLookupProxyOrigin(self, &pathHash, pathText, sizeof(pathText));
  if ((!pathText[0] || pathText[0] == '<') &&
      s_assetProxyUntrackedGetAssetProxy) {
    void *proxy = ((TraceProxyObjectFn)s_assetProxyUntrackedGetAssetProxy)(
        self, nullptr);
    if (proxy)
      TraceLookupProxyOrigin(proxy, &pathHash, pathText, sizeof(pathText));
  }
  if ((!pathText[0] || pathText[0] == '<') && pathHash)
    TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if ((!pathText[0] || pathText[0] == '<') && pathHash &&
      EiemOnUnityThread())
    TraceResolveStringPathHashPath(pathHash, pathText, sizeof(pathText));
  if (result) {
    // Untracked handles do not expose the tracked proxy's origin table. Their
    // path getter is still a managed, read-only identity source, so bind it
    // before the object reaches a Renderer.
    if (pathText[0] && pathText[0] != '<') {
      TraceRememberProxyOrigin(self, 0, pathText);
      TraceBindAssetFromProxy(self, result);
    }

  }

  return result;
}

// VFS observation deliberately treats the path as an opaque string object and
// never invokes a managed getter. These methods may run on worker threads.
static void *TraceVfsLoadBundleFromFile(void *self, void *path,
                                        void *methodInfo) {
  auto original = (TraceVfsPathFn)s_origVfsLoadBundleFromFile;
  TraceRememberBundlePath(path);
  void *result = original ? original(self, path, methodInfo) : nullptr;
  if (result) TraceRememberActiveBundle(result, path);

  return result;
}

static void *TraceVfsLoadBundleFromFileAsync(void *self, void *path,
                                              void *methodInfo) {
  auto original = (TraceVfsPathFn)s_origVfsLoadBundleFromFileAsync;
  TraceRememberBundlePath(path);
  void *result = original ? original(self, path, methodInfo) : nullptr;
  if (result) TraceRememberPendingBundleRequest(result, path);

  return result;
}

static void *TraceVfsLoadBundleFromFilePos(void *self, void *path,
                                            void *loaderPos, uint32_t crc,
                                            void *methodInfo) {
  auto original = (TraceVfsPathPosFn)s_origVfsLoadBundleFromFilePos;
  TraceRememberBundlePath(path);
  void *result = original ? original(self, path, loaderPos, crc, methodInfo)
                          : nullptr;
  if (result) TraceRememberActiveBundle(result, path);
  return result;
}

static void *TraceVfsLoadBundleFromFileAsyncPos(void *self, void *path,
                                                 void *loaderPos, uint32_t crc,
                                                 void *methodInfo) {
  auto original = (TraceVfsPathPosFn)s_origVfsLoadBundleFromFileAsyncPos;
  TraceRememberBundlePath(path);
  void *result = original ? original(self, path, loaderPos, crc, methodInfo)
                          : nullptr;
  if (result) TraceRememberPendingBundleRequest(result, path);
  return result;
}

static void TraceAssetBundleUnload(void *self, bool unloadAll,
                                   void *methodInfo) {
  auto original = (TraceAssetBundleUnloadFn)s_origAssetBundleUnload;
  if (original) original(self, unloadAll, methodInfo);
  TraceForgetActiveBundle(self);
}

static void *TraceAssetBundleCreateRequestGetAssetBundle(void *self,
                                                          void *methodInfo) {
  auto original =
      (TraceAssetBundleCreateRequestGetAssetBundleFn)
          s_origAssetBundleCreateRequestGetAssetBundle;
  void *result = original ? original(self, methodInfo) : nullptr;
  if (result) TraceResolvePendingBundleRequest(self, result);
  return result;
}

static void *TraceVfsGetAssetStream(void *self, void *path,
                                    void *methodInfo) {
  auto original = (TraceVfsPathFn)s_origVfsGetAssetStream;
  void *result = original ? original(self, path, methodInfo) : nullptr;

  if (result) TraceRememberStreamPath(result, path);
  return result;
}

typedef void *(__fastcall *TraceVfsGetAssetStreamHashFn)(void *self,
                                                          int64_t pathHash,
                                                          void *methodInfo);

static void *TraceVfsGetAssetStreamHash(void *self, int64_t pathHash,
                                        void *methodInfo) {
  auto original = (TraceVfsGetAssetStreamHashFn)s_origVfsGetAssetStreamHash;
  void *result = original ? original(self, pathHash, methodInfo) : nullptr;
  char pathText[768] = {};
  TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if (result) {
    if (!pathText[0])
      snprintf(pathText, sizeof(pathText), "#hash:%lld", (long long)pathHash);
    TraceRememberStreamPathText(result, pathText);
  }

  return result;
}

static void *TraceResourceLoadAssetInternalHash(
    void *self, int64_t pathHash, void *type, int category, bool immediate,
    int priority, void *methodInfo) {
  if (self) s_eiemResourceManagerInstance = self;
  auto original =
      (TraceResourceLoadAssetHashFn)s_origResourceLoadAssetInternalHash;
  void *result = original
                     ? original(self, pathHash, type, category, immediate,
                                priority, methodInfo)
                     : nullptr;
  char pathText[768] = {};
  TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if (!pathText[0] && EiemOnUnityThread())
    TraceResolveStringPathHashPath(pathHash, pathText, sizeof(pathText));
  if (result) {
    TraceRememberProxyOrigin(result, pathHash, pathText);
  }
  if (!s_traceReentrant && TraceMarkHashFirstSeen(pathHash) &&
      TraceTakeTargetBudget(&s_traceHashLoadCount, 120, pathText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] BundleResourceManager._LoadAssetInternal(hash): "
        "hash=%lld path=\"%s\" type=%s category=%d immediate=%d "
        "priority=%d result=%p",
        (long long)pathHash, pathText[0] ? pathText : "?", typeText,
        category, immediate ? 1 : 0, priority, result);
    s_traceReentrant = false;
  }
  return result;
}

static void *TraceResourceLoadSubAssetInternalHash(
    void *self, int64_t pathHash, void *subAsset, void *type, int category,
    bool immediate, int priority, void *methodInfo) {
  auto original =
      (TraceResourceLoadSubAssetHashFn)s_origResourceLoadSubAssetInternalHash;
  void *result = original
                     ? original(self, pathHash, subAsset, type, category,
                                immediate, priority, methodInfo)
                     : nullptr;
  char pathText[768] = {};
  char subAssetText[512] = {};
  TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if (!pathText[0] && EiemOnUnityThread())
    TraceResolveStringPathHashPath(pathHash, pathText, sizeof(pathText));
  TraceDescribeString(subAsset, subAssetText, sizeof(subAssetText));
  if (result) {
    TraceRememberProxyOrigin(result, pathHash, pathText);
  }
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_traceHashSubAssetCount, 120, pathText,
                            subAssetText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] BundleResourceManager._LoadSubAssetInternal(hash): "
        "hash=%lld path=\"%s\" subAsset=\"%s\" type=%s category=%d "
        "immediate=%d priority=%d result=%p",
        (long long)pathHash, pathText[0] ? pathText : "?", subAssetText,
        typeText, category,
        immediate ? 1 : 0, priority, result);
    s_traceReentrant = false;
  }
  return result;
}

static void TracePreloadAutoHash(void *self, int64_t pathHash,
                                 void *methodInfo) {
  auto original = (TracePreloadAutoHashFn)s_origPreloadAutoHash;
  if (original) original(self, pathHash, methodInfo);

}

static void *TraceBundleLoadAssetBundle(void *self, void *path,
                                        void *methodInfo) {
  auto original = (TraceBundleLoadFn)s_origBundleLoadAssetBundle;
  void *result = original ? original(self, path, methodInfo) : nullptr;
  if (result) TraceRememberActiveBundle(self, path);

  return result;
}

static void *TraceBundleLoadAssetBundleAsync(void *self, void *path,
                                             void *methodInfo) {
  auto original = (TraceBundleLoadFn)s_origBundleLoadAssetBundleAsync;
  void *result = original ? original(self, path, methodInfo) : nullptr;
  if (result) TraceRememberActiveBundle(self, path);

  return result;
}

static void TraceBundleSetAssetBundle(void *self, void *bundle,
                                      void *methodInfo) {
  auto original = (TraceBundleSetFn)s_origBundleSetAssetBundle;
  if (original) original(self, bundle, methodInfo);

}

static void *TraceBundleGetFullPath(void *self, void *path,
                                    void *methodInfo) {
  auto original = (TraceBundleGetPathFn)s_origBundleGetFullPath;
  void *result = original ? original(self, path, methodInfo) : nullptr;

  return result;
}

static void TraceBundleFinishWithBundle(void *self, void *bundle,
                                        void *methodInfo) {
  auto original = (TraceBundleFinishFn)s_origBundleFinishWithBundle;
  if (original) original(self, bundle, methodInfo);

}

static void TraceBundleOnEndUnload(void *self, void *methodInfo) {
  auto original = (TraceVoidMethodFn)s_origBundleOnEndUnload;
  if (original) original(self, methodInfo);
  TraceForgetActiveBundle(self);
}

static void *TraceAssetGetAssetName(void *self, void *methodInfo) {
  auto original = (TraceAssetGetNameFn)s_origAssetGetAssetName;
  void *result = original ? original(self, methodInfo) : nullptr;

  return result;
}

static void TraceAssetFinishWithAsset(void *self, void *asset,
                                      void *methodInfo) {
  auto original = (TraceAssetFinishFn)s_origAssetFinishWithAsset;
  const int64_t pathHash = TraceReadLoadableHash(self);
  char logicalPath[768] = {};
  if (pathHash) {
    TraceLookupHashPath(pathHash, logicalPath, sizeof(logicalPath));
    if (!logicalPath[0] && EiemOnUnityThread())
      TraceResolveStringPathHashPath(pathHash, logicalPath,
                                     sizeof(logicalPath));
  }

  if (asset) {
    TraceRememberAssetOrigin(
        asset, pathHash,
        logicalPath[0] && logicalPath[0] != '<' ? logicalPath : nullptr);
  }
  // Completion hooks observe identity only. Resource declarations do not
  // replace cached Unity objects; Render rules own all live mutations.
  if (original) original(self, asset, methodInfo);

  char assetName[768] = {};
  TraceReadAssetName(self, assetName, sizeof(assetName));
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_traceAssetCompleteCount, 160, logicalPath,
                            assetName)) {
    s_traceReentrant = true;
    char sourceText[512] = {};
    TraceDescribeObject(asset, sourceText, sizeof(sourceText));
    Log("[RES-TRACE] Asset._FinishWithAsset: hash=%lld loader=%p "
        "path=\"%s\" assetName=\"%s\" asset=%s",
        (long long)pathHash, self, logicalPath[0] ? logicalPath : "?",
        assetName[0] ? assetName : "?", sourceText);
    s_traceReentrant = false;
  }
}

static void TraceAssetOnComplete(void *self, void *methodInfo) {
  auto original = (TraceVoidMethodFn)s_origAssetOnComplete;
  if (original) original(self, methodInfo);
  void *completedAsset = nullptr;
  __try { completedAsset = *(void **)((char *)self + 0xA0); }
  __except (1) { completedAsset = nullptr; }
  char assetName[768] = {};
  const int64_t pathHash = TraceReadLoadableHash(self);
  if (pathHash)
    TraceLookupHashPath(pathHash, assetName, sizeof(assetName));
  if (!assetName[0]) TraceReadAssetName(self, assetName, sizeof(assetName));
  if (completedAsset) {
    TraceRememberAssetOrigin(completedAsset, pathHash, assetName);
  }
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_traceAssetCompleteCount, 160, assetName)) {
    s_traceReentrant = true;
    void *asset = nullptr;
    __try { asset = *(void **)((char *)self + 0xA0); }
    __except (1) { asset = nullptr; }
    char assetText[512] = {};
    TraceDescribeObject(asset, assetText, sizeof(assetText));
    Log("[RES-TRACE] Asset.OnComplete: hash=%lld loader=%p assetName=\"%s\" "
        "asset=%s",
        (long long)pathHash, self, assetName[0] ? assetName : "?", assetText);
    s_traceReentrant = false;
  }
}

static void *TraceResourceLoadAssetInternal(
    void *self, void *path, void *type, int category, bool immediate,
    int priority, void *methodInfo) {
  if (self) s_eiemResourceManagerInstance = self;
  auto original = (TraceResourceLoadAssetFn)s_origResourceLoadAssetInternal;
  void *result = original
                     ? original(self, path, type, category, immediate,
                                priority, methodInfo)
                     : nullptr;
  char pathText[768] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  if (result) {
    TraceRememberProxyOrigin(result, 0, pathText);
  }
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_tracePathHashCount, 120, pathText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] BundleResourceManager._LoadAssetInternal: path=\"%s\" "
        "type=%s category=%d immediate=%d priority=%d result=%p",
        pathText, typeText, category, immediate ? 1 : 0, priority, result);
    s_traceReentrant = false;
  }
  return result;
}

static void *TraceResourceLoadSubAssetInternal(
    void *self, void *path, void *subAsset, void *type, int category,
    bool immediate, int priority, void *methodInfo) {
  auto original =
      (TraceResourceLoadSubAssetFn)s_origResourceLoadSubAssetInternal;
  void *result = original
                     ? original(self, path, subAsset, type, category,
                                immediate, priority, methodInfo)
                     : nullptr;
  char pathText[768] = {};
  char subAssetText[512] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  TraceDescribeString(subAsset, subAssetText, sizeof(subAssetText));
  if (result) {
    TraceRememberProxyOrigin(result, 0, pathText);
  }
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_tracePathHashCount, 120, pathText,
                            subAssetText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] BundleResourceManager._LoadSubAssetInternal: path=\"%s\" "
        "subAsset=\"%s\" type=%s category=%d immediate=%d priority=%d "
        "result=%p",
        pathText, subAssetText, typeText, category, immediate ? 1 : 0,
        priority, result);
    s_traceReentrant = false;
  }
  return result;
}

static void TraceResourceLoadAsyncString(
    void *self, int32_t logChannel, void *path, void *type, int category,
    void *callback, int priority, void *methodInfo) {
  auto original =
      (TraceResourceLoadAsyncStringFn)s_origResourceLoadAsyncString;
  char pathText[768] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_tracePathHashCount, 120, pathText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] BundleResourceManager.LoadAsync(string callback): "
        "channel=%d path=\"%s\" type=%s category=%d priority=%d "
        "callback=%p", logChannel, pathText[0] ? pathText : "?", typeText,
        category, priority, callback);
    s_traceReentrant = false;
  }
  if (self) s_eiemResourceManagerInstance = self;
  if (original)
    original(self, logChannel, path, type, category, callback, priority,
             methodInfo);
}

static void TraceResourceLoadSubAssetAsyncString(
    void *self, int32_t logChannel, void *path, void *subAsset, void *type,
    int category, void *callback, int priority, void *methodInfo) {
  auto original = (TraceResourceLoadSubAssetAsyncStringFn)
      s_origResourceLoadSubAssetAsyncString;
  char pathText[768] = {};
  char subAssetText[512] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  TraceDescribeString(subAsset, subAssetText, sizeof(subAssetText));
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_tracePathHashCount, 120, pathText,
                            subAssetText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] BundleResourceManager.LoadSubAssetAsync(string "
        "callback): channel=%d path=\"%s\" subAsset=\"%s\" type=%s "
        "category=%d priority=%d callback=%p", logChannel,
        pathText[0] ? pathText : "?", subAssetText[0] ? subAssetText : "?",
        typeText, category, priority, callback);
    s_traceReentrant = false;
  }
  if (self) s_eiemResourceManagerInstance = self;
  if (original)
    original(self, logChannel, path, subAsset, type, category, callback,
             priority, methodInfo);
}

static void TraceResourceLoadAsyncHash(
    void *self, int32_t logChannel, int64_t pathHash, void *type, int category,
    void *callback, int priority, void *methodInfo) {
  auto original = (TraceResourceLoadAsyncHashFn)s_origResourceLoadAsyncHash;
  char pathText[768] = {};
  TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if (!pathText[0] && EiemOnUnityThread())
    TraceResolveStringPathHashPath(pathHash, pathText, sizeof(pathText));
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_traceHashLoadCount, 120, pathText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] BundleResourceManager.LoadAsync(hash callback): "
        "channel=%d hash=%lld path=\"%s\" type=%s category=%d "
        "priority=%d callback=%p", logChannel, (long long)pathHash,
        pathText[0] ? pathText : "?", typeText, category, priority, callback);
    s_traceReentrant = false;
  }
  if (self) s_eiemResourceManagerInstance = self;
  if (original)
    original(self, logChannel, pathHash, type, category, callback, priority,
             methodInfo);
}

static void TraceResourceLoadSubAssetAsyncHash(
    void *self, int32_t logChannel, int64_t pathHash, void *subAsset,
    void *type, int category, void *callback, int priority, void *methodInfo) {
  auto original = (TraceResourceLoadSubAssetAsyncHashFn)
      s_origResourceLoadSubAssetAsyncHash;
  char pathText[768] = {};
  char subAssetText[512] = {};
  TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if (!pathText[0] && EiemOnUnityThread())
    TraceResolveStringPathHashPath(pathHash, pathText, sizeof(pathText));
  TraceDescribeString(subAsset, subAssetText, sizeof(subAssetText));
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_traceHashSubAssetCount, 120, pathText,
                            subAssetText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] BundleResourceManager.LoadSubAssetAsync(hash "
        "callback): channel=%d hash=%lld path=\"%s\" subAsset=\"%s\" "
        "type=%s category=%d priority=%d callback=%p", logChannel,
        (long long)pathHash, pathText[0] ? pathText : "?",
        subAssetText[0] ? subAssetText : "?", typeText, category, priority,
        callback);
    s_traceReentrant = false;
  }
  if (self) s_eiemResourceManagerInstance = self;
  if (original)
    original(self, logChannel, pathHash, subAsset, type, category, callback,
             priority, methodInfo);
}

static void TraceSimpleAssetLoaderLoadAsync(
    void *self, int64_t pathHash, void *type, void *callback, int priority,
    void *methodInfo) {
  auto original = (TraceAssetLoaderAsyncHashFn)s_origSimpleAssetLoaderLoadAsync;
  char pathText[768] = {};
  TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if (!pathText[0] && EiemOnUnityThread())
    TraceResolveStringPathHashPath(pathHash, pathText, sizeof(pathText));
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_traceHashLoadCount, 120, pathText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] SimpleAssetLoader.LoadAsync(hash callback): "
        "loader=%p hash=%lld path=\"%s\" type=%s priority=%d callback=%p",
        self, (long long)pathHash, pathText[0] ? pathText : "?", typeText,
        priority, callback);
    s_traceReentrant = false;
  }
  if (original)
    original(self, pathHash, type, callback, priority, methodInfo);
}

static void TraceMonoEntitySimpleAssetLoaderLoadAsync(
    void *self, int64_t pathHash, void *type, void *callback, int priority,
    void *methodInfo) {
  auto original = (TraceAssetLoaderAsyncHashFn)
      s_origMonoEntitySimpleAssetLoaderLoadAsync;
  char pathText[768] = {};
  TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if (!pathText[0] && EiemOnUnityThread())
    TraceResolveStringPathHashPath(pathHash, pathText, sizeof(pathText));
  if (!s_traceReentrant &&
      TraceTakeTargetBudget(&s_traceHashLoadCount, 120, pathText)) {
    s_traceReentrant = true;
    char typeText[512] = {};
    TraceDescribeObject(type, typeText, sizeof(typeText));
    Log("[RES-TRACE] MonoEntitySimpleAssetLoader.LoadAsync(hash "
        "callback): loader=%p hash=%lld path=\"%s\" type=%s priority=%d "
        "callback=%p", self, (long long)pathHash,
        pathText[0] ? pathText : "?", typeText, priority, callback);
    s_traceReentrant = false;
  }
  if (original)
    original(self, pathHash, type, callback, priority, methodInfo);
}

static bool TraceAssetLoaderTryLoadHash(void *self, int64_t pathHash,
                                        void *type, void *outHandle,
                                        void *methodInfo, void *originalPtr,
                                        const char *label) {
  auto original = (TraceAssetLoaderTryLoadHashFn)originalPtr;
  const bool result = original
                          ? original(self, pathHash, type, outHandle,
                                     methodInfo)
                          : false;
  if (!kEiemValidationIdentityProbe || s_traceReentrant ||
      InterlockedIncrement(&s_traceAssetLoaderTryLoadCount) > 320)
    return result;

  char pathText[768] = {};
  TraceLookupHashPath(pathHash, pathText, sizeof(pathText));
  if (!pathText[0] && EiemOnUnityThread())
    TraceResolveStringPathHashPath(pathHash, pathText, sizeof(pathText));

  // The output handle is a value type stored at the caller-provided address.
  // Calling its getter after the original resolves the concrete cache path
  // without reading the value type's fields or changing ownership.
  char handlePath[768] = {};
  auto pathGetter = (TraceProxyObjectFn)s_origAssetProxyLoaderHandlePath;
  if (result && pathGetter && outHandle) {
    __try {
      TraceDescribeString(pathGetter(outHandle, nullptr), handlePath,
                          sizeof(handlePath));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      handlePath[0] = '\0';
    }
  }
  char typeText[512] = {};
  TraceDescribeObject(type, typeText, sizeof(typeText));

  Log("[RES-TRACE] %s: loader=%p hash=%lld path=\"%s\" type=%s "
      "result=%d outHandle=%p handlePath=\"%s\"",
      label ? label : "AssetLoader.TryLoad(hash,type,out)", self,
      (long long)pathHash, pathText[0] ? pathText : "?",
      typeText[0] ? typeText : "?", result ? 1 : 0, outHandle,
      handlePath[0] ? handlePath : "?");
  return result;
}

static bool TraceSimpleAssetLoaderTryLoad(void *self, int64_t pathHash,
                                          void *type, void *outHandle,
                                          void *methodInfo) {
  return TraceAssetLoaderTryLoadHash(
      self, pathHash, type, outHandle, methodInfo,
      s_origSimpleAssetLoaderTryLoad,
      "SimpleAssetLoader.TryLoad(hash,type,out)");
}

static bool TraceMonoEntitySimpleAssetLoaderTryLoad(
    void *self, int64_t pathHash, void *type, void *outHandle,
    void *methodInfo) {
  return TraceAssetLoaderTryLoadHash(
      self, pathHash, type, outHandle, methodInfo,
      s_origMonoEntitySimpleAssetLoaderTryLoad,
      "MonoEntitySimpleAssetLoader.TryLoad(hash,type,out)");
}

static void *TraceCachedPathAssetLoaderLoadDirect(void *self, void *path,
                                                  void *type,
                                                  void *methodInfo) {
  auto original = (TraceCachedLoaderLoadDirectFn)
      s_origCachedPathAssetLoaderLoadDirect;
  void *result = original ? original(self, path, type, methodInfo) : nullptr;
  if (!kEiemValidationIdentityProbe || s_traceReentrant ||
      InterlockedIncrement(&s_traceCachedLoaderCount) > 240)
    return result;
  char pathText[768] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  char typeText[512] = {};
  TraceDescribeObject(type, typeText, sizeof(typeText));
  char resultText[512] = {};
  TraceDescribeObject(result, resultText, sizeof(resultText));
  if (!TraceIdentityTextMatchesConfiguredRule(pathText) &&
      !TraceIdentityTextMatchesConfiguredRule(resultText))
    return result;
  Log("[RES-TRACE] CachedPathAssetLoader.LoadDirect(string,type): "
      "loader=%p path=\"%s\" type=%s result=%s", self,
      pathText[0] ? pathText : "?", typeText[0] ? typeText : "?",
      resultText[0] ? resultText : "<null>");
  return result;
}

static bool TraceCachedPathAssetLoaderTryLoad(void *self, void *path,
                                              void *type, void *outHandle,
                                              void *methodInfo) {
  auto original = (TraceCachedLoaderTryLoadStringFn)
      s_origCachedPathAssetLoaderTryLoad;
  const bool result = original
                          ? original(self, path, type, outHandle, methodInfo)
                          : false;
  if (!kEiemValidationIdentityProbe || s_traceReentrant ||
      InterlockedIncrement(&s_traceCachedLoaderCount) > 240)
    return result;
  char pathText[768] = {};
  TraceDescribeString(path, pathText, sizeof(pathText));
  if (!TraceIdentityTextMatchesConfiguredRule(pathText)) return result;
  char typeText[512] = {};
  TraceDescribeObject(type, typeText, sizeof(typeText));
  Log("[RES-TRACE] CachedPathAssetLoader.TryLoad(string,type,out): "
      "loader=%p path=\"%s\" type=%s result=%d outHandle=%p", self,
      pathText[0] ? pathText : "?", typeText[0] ? typeText : "?",
      result ? 1 : 0, outHandle);
  return result;
}

static void *TraceAssetBundleLoadAsset1(void *self, void *path,
                                         void *methodInfo) {
  auto original = (TraceLoadAsset1Fn)s_origAssetBundleLoadAsset1;
  void *result = original ? original(self, path, methodInfo) : nullptr;

  return result;
}

static void *TraceAssetBundleLoadAsset2(void *self, void *path, void *type,
                                        void *methodInfo) {
  // Capture the caller before the original runs; the frame is still ours here.
  void *caller = _ReturnAddress();
  auto original = (TraceLoadAsset2Fn)s_origAssetBundleLoadAsset2;
  void *result = original ? original(self, path, type, methodInfo) : nullptr;
  // Only the target character's own parts are reported. Every Prefab loads this
  // same set of Meshes, so filtering on the asset is what turns the log into a
  // list of the presentation paths that touch it.

  return result;
}

static void *TraceAssetBundleLoadAssetAsync1(void *self, void *path,
                                              void *methodInfo) {
  auto original = (TraceLoadAsset1Fn)s_origAssetBundleLoadAssetAsync1;
  void *result = original ? original(self, path, methodInfo) : nullptr;

  return result;
}

static void *TraceAssetBundleLoadAssetAsync2(void *self, void *path,
                                              void *type, void *methodInfo) {
  auto original = (TraceLoadAsset2Fn)s_origAssetBundleLoadAssetAsync2;
  void *result = original ? original(self, path, type, methodInfo) : nullptr;

  return result;
}

static void TraceSkinnedMeshSetSharedMesh(void *self, void *mesh,
                                           void *methodInfo) {
  auto original = (TraceSetSharedMeshFn)s_origSkinnedMeshSetSharedMesh;
  if (s_eiemApplyingModMeshAssignment) {
    if (original) original(self, mesh, methodInfo);
    return;
  }
  if (InterlockedCompareExchange(&s_traceSetterThreadLogged, 1, 0) == 0)
    Log("[DEBUG-thread] SkinnedMeshRenderer setter tid=%lu recordedUnityTid=%lu",
        (unsigned long)GetCurrentThreadId(), (unsigned long)s_eiemUnityThreadId);
  EiemRegistrationTraceNativeStackContext(
      "SkinnedMeshRenderer.set_sharedMesh.entry", self, mesh, nullptr,
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0));
  if (kEiemEnableContinuousMeshObservation)
    TraceRememberMeshObservation(self, mesh, "SkinnedMeshRenderer");
  void *sourceMesh = mesh;
  // A later game-side LOD/skin refresh may assign the original Mesh again.
  // Preserve an existing binding; otherwise this assignment is also a precise
  // lifecycle event at which standalone Mesh-identity rules can be evaluated.
  void *retained = EiemReplacementForSourceMesh(self, sourceMesh);
  if (retained) mesh = retained;
  if (original) original(self, mesh, methodInfo);
  EiemLogSkinSetterTimeline("sharedMesh-game", self, sourceMesh, mesh,
                            nullptr, nullptr);
  // This setter is also used while the game's skin/LOD assembly is only
  // partially populated.  It remains observation/reassertion-only; resource
  // rules are committed at the completed assembly boundaries instead.
  if (kEiemValidationIdentityProbe) {
    char identityText[768] = {};
    TraceLookupAssetOrigin(mesh, nullptr, identityText,
                           sizeof(identityText));
    if (!TraceIdentityTextMatchesConfiguredRule(identityText))
      TraceReadUnityObjectName(mesh, identityText, sizeof(identityText));
    if (!s_traceReentrant &&
        TraceTakeTargetBudget(&s_traceSharedMeshCount, 180, identityText)) {
      s_traceReentrant = true;
      char rendererText[512] = {};
      char meshText[512] = {};
      TraceDescribeObject(self, rendererText, sizeof(rendererText));
      TraceDescribeObject(mesh, meshText, sizeof(meshText));
      Log("[RES-TRACE] SkinnedMeshRenderer.set_sharedMesh: renderer=%s "
          "mesh=%s",
          rendererText, meshText);
      s_traceReentrant = false;
    }
  }
}

static void TraceMeshFilterSetSharedMesh(void *self, void *mesh,
                                         void *methodInfo) {
  auto original = (TraceSetSharedMeshFn)s_origMeshFilterSetSharedMesh;
  if (s_eiemApplyingModMeshAssignment) {
    if (original) original(self, mesh, methodInfo);
    return;
  }
  if (kEiemEnableContinuousMeshObservation)
    TraceRememberMeshObservation(self, mesh, "MeshFilter");
  void *sourceMesh = mesh;
  void *retained = EiemReplacementForSourceMesh(self, sourceMesh);
  if (retained) mesh = retained;
  if (original) original(self, mesh, methodInfo);
  // MeshFilter follows the same rule as SkinnedMeshRenderer: do not mutate a
  // resource from a low-level setter before the owning game assembly returns;
  // commit only at completed assembly boundaries.
  if (kEiemValidationIdentityProbe) {
    char identityText[768] = {};
    TraceLookupAssetOrigin(mesh, nullptr, identityText,
                           sizeof(identityText));
    if (!TraceIdentityTextMatchesConfiguredRule(identityText))
      TraceReadUnityObjectName(mesh, identityText, sizeof(identityText));
    if (!s_traceReentrant &&
        TraceTakeTargetBudget(&s_traceMeshFilterCount, 120, identityText)) {
      s_traceReentrant = true;
      char rendererText[512] = {};
      char meshText[512] = {};
      TraceDescribeObject(self, rendererText, sizeof(rendererText));
      TraceDescribeObject(mesh, meshText, sizeof(meshText));
      Log("[RES-TRACE] MeshFilter.set_sharedMesh: renderer=%s mesh=%s",
          rendererText, meshText);
      s_traceReentrant = false;
    }
  }
}

// Unity's public Mesh/bones properties can remain valid while the internal
// skin submission path is still waiting for the current-frame matrices. This
// probe records that boundary for the exact F10/cold-start window already
// used by EiemLogSkinTimingProbe. It never calls a setter or asks Unity to
// recalculate anything.
static void EiemLogSkinNativeSubmission(const char *eventName, void *renderer,
                                        bool result, void *arg0,
                                        int32_t arg1, void *buffer) {
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!transaction || !renderer) return;

  bool tracked = false;
  char section[96] = "<untracked>";
  void *owner = nullptr;
  void *expectedMesh = nullptr;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX) {
    tracked = true;
    const auto &state = s_eiemOverrides[index];
    owner = (void *)state.ownerPrefabInstance;
    expectedMesh = state.replacementMesh;
    strncpy_s(section, sizeof(section), state.renderSection, _TRUNCATE);
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);

  if (tracked) {
    if (InterlockedIncrement(&s_eiemSkinNativeTrackedCalls) > 256) return;
  } else {
    // A small untracked sample helps detect a different draw branch without
    // turning a busy render loop into a log flood.
    if (InterlockedIncrement(&s_eiemSkinNativeUntrackedCalls) > 64) return;
  }

  void *currentMesh = nullptr;
  void *bones = nullptr;
  void *rootBone = nullptr;
  void *skinningRoot = nullptr;
  size_t boneCount = 0;
  uint64_t boneRefs = 0;
  uint64_t matrixRefs = 0;
  uint64_t rootMatrix = 0;
  if (tracked) {
    currentMesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
    bones = g_smr_get_bones ? Invoke(g_smr_get_bones, renderer) : nullptr;
    rootBone = g_smr_get_rootBone ? Invoke(g_smr_get_rootBone, renderer)
                                  : nullptr;
    skinningRoot = g_smr_get_skinningRoot
                       ? Invoke(g_smr_get_skinningRoot, renderer)
                       : nullptr;
    boneCount = EiemManagedArrayLength(bones);
    boneRefs = EiemSkinTimelineBoneRefs(bones);
    // Transform matrix reads are only performed on Unity's thread.  The
    // submission hook can be called from a render worker on some builds;
    // logging the managed palette there remains safe without dereferencing
    // Unity Transform state from the wrong thread.
    if (EiemOnUnityThread()) {
      matrixRefs = EiemSkinTimingBoneMatrixHash(bones);
      rootMatrix = EiemSkinTimingTransformMatrixHash(rootBone);
    }
  }
  Log("[SKIN-NATIVE-v1] tx=%ld event=%s tid=%lu tracked=%d owner=%p "
      "renderer=%p section=%s result=%d arg0=%p arg1=%d buffer=%p tick=%llu "
      "currentMesh=%p expectedMesh=%p bones=%p boneCount=%zu "
      "boneRefs=%016llX matrixRefs=%016llX rootBone=%p rootMatrix=%016llX "
      "skinningRoot=%p",
      transaction, eventName ? eventName : "unknown",
      (unsigned long)GetCurrentThreadId(), tracked ? 1 : 0, owner, renderer,
      section, result ? 1 : 0, arg0, arg1, buffer,
      (unsigned long long)GetTickCount64(), currentMesh, expectedMesh, bones,
      boneCount, (unsigned long long)boneRefs, (unsigned long long)matrixRefs,
      rootBone, (unsigned long long)rootMatrix, skinningRoot);
}

static bool TraceSkinnedMeshRequestCurrentFrameSkinMatrices(
    void *self, void *skinMatrices, int32_t count, void *methodInfo) {
  auto original = (TraceRequestCurrentFrameSkinMatricesFn)
      s_origSkinnedMeshRequestCurrentFrameSkinMatrices;
  const bool result = original ? original(self, skinMatrices, count, methodInfo)
                               : false;
  EiemLogSkinNativeSubmission("request-current-frame", self, result,
                              skinMatrices, count, nullptr);
  return result;
}

static bool TraceSkinnedMeshSkinMatricesRequestFinished(void *self,
                                                         void *methodInfo) {
  auto original = (TraceSkinMatricesRequestFinishedFn)
      s_origSkinnedMeshSkinMatricesRequestFinished;
  const bool result = original ? original(self, methodInfo) : false;
  EiemLogSkinNativeSubmission("request-finished", self, result, nullptr, 0,
                              nullptr);
  return result;
}

static void *TraceSkinnedMeshGetVertexBuffer(void *self, void *methodInfo) {
  auto original =
      (TraceSkinGraphicsBufferFn)s_origSkinnedMeshGetVertexBuffer;
  void *buffer = original ? original(self, methodInfo) : nullptr;
  EiemLogSkinNativeSubmission("get-current-vertex-buffer", self, buffer != nullptr,
                              nullptr, 0, buffer);
  return buffer;
}

static void *TraceSkinnedMeshGetPreviousVertexBuffer(void *self,
                                                     void *methodInfo) {
  auto original =
      (TraceSkinGraphicsBufferFn)s_origSkinnedMeshGetPreviousVertexBuffer;
  void *buffer = original ? original(self, methodInfo) : nullptr;
  EiemLogSkinNativeSubmission("get-previous-vertex-buffer", self,
                              buffer != nullptr, nullptr, 0, buffer);
  return buffer;
}

// HG.Rendering.Runtime.SkinnedMeshCaptureManager.RequestCapture is the first
// known custom-pipeline boundary that receives both the ordinary MeshRenderer
// and the SkinnedMeshRenderer. It does not expose the eventual ring-buffer
// offset in its managed signature, but the call is still valuable evidence:
// it tells us whether a tracked cloth Renderer enters this path at all and
// gives us the manager's frame counter to correlate with later native traces.
static uint32_t EiemReadSkinCaptureFrame(void *manager) {
  if (!manager) return 0;
  __try { return *(const uint32_t *)((const char *)manager + 0x20); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static void TraceSkinnedMeshCaptureRequest(void *self, void *meshRenderer,
                                           void *skinnedMeshRenderer,
                                           void *propertyBlock,
                                           void *methodInfo) {
  auto original =
      (TraceSkinCaptureRequestFn)s_origSkinnedMeshCaptureRequest;
  if (original)
    original(self, meshRenderer, skinnedMeshRenderer, propertyBlock,
             methodInfo);

  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!transaction || !skinnedMeshRenderer ||
      InterlockedIncrement(&s_eiemSkinCaptureRequestCalls) > 64)
    return;

  bool tracked = false;
  char section[96] = "<untracked>";
  void *owner = nullptr;
  void *expectedMesh = nullptr;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(skinnedMeshRenderer);
  if (index != SIZE_MAX) {
    tracked = true;
    const auto &state = s_eiemOverrides[index];
    owner = (void *)state.ownerPrefabInstance;
    expectedMesh = state.replacementMesh;
    strncpy_s(section, sizeof(section), state.renderSection, _TRUNCATE);
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);

  void *currentMesh = nullptr;
  size_t boneCount = 0;
  if (tracked && EiemOnUnityThread()) {
    currentMesh = EiemReadSharedMesh(skinnedMeshRenderer,
                                     "SkinnedMeshRenderer");
    void *bones = g_smr_get_bones
                      ? Invoke(g_smr_get_bones, skinnedMeshRenderer)
                      : nullptr;
    boneCount = EiemManagedArrayLength(bones);
  }
  Log("[SKIN-CAPTURE-v1] tx=%ld manager=%p managerFrame=%u "
      "meshRenderer=%p skinnedRenderer=%p propertyBlock=%p tracked=%d "
      "owner=%p section=%s currentMesh=%p expectedMesh=%p boneCount=%zu "
      "tick=%llu tid=%lu",
      transaction, self, EiemReadSkinCaptureFrame(self), meshRenderer,
      skinnedMeshRenderer, propertyBlock, tracked ? 1 : 0, owner, section,
      currentMesh, expectedMesh, boneCount,
      (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentThreadId());
}

// GpuClothManager is the first managed object we found whose fields directly
// name the custom cloth skeleton ComputeBuffer and whose methods feed the
// render graph.  The game keeps these fields in an IL2CPP object; reading the
// already-resolved metadata offsets is observation-only and is guarded so a
// stale object cannot affect the game.  We deliberately do not call
// ComputeBuffer.GetData here: that would synchronize the GPU and could change
// the timing that produces the intermittent ground pose.
struct EiemGpuClothState {
  void *characterMesh;
  void *skeletonBuffer;
  bool isStreamingMode;
  float skeletonFlipped;
  int32_t runtimeClothNum;
  int32_t runtimeClothGroupNum;
};

static EiemGpuClothState EiemReadGpuClothState(void *self) {
  EiemGpuClothState state = {};
  if (!self) return state;
  __try {
    const char *base = (const char *)self;
    state.characterMesh = *(void **)(base + 0x110);
    state.skeletonBuffer = *(void **)(base + 0x148);
    state.isStreamingMode = *(const bool *)(base + 0x290);
    state.skeletonFlipped = *(const float *)(base + 0x294);
    state.runtimeClothNum = *(const int32_t *)(base + 0x2B8);
    state.runtimeClothGroupNum = *(const int32_t *)(base + 0x2BC);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    state = {};
  }
  return state;
}

static void EiemLogGpuClothEvent(const char *event, void *self,
                                 float deltaTime, void *argument,
                                 int result, void *returnedBuffer) {
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (transaction) {
    if (InterlockedIncrement(&s_eiemGpuClothObservationCalls) > 512)
      return;
  } else if (InterlockedIncrement(&s_eiemGpuClothStartupCalls) > 128) {
    // Manager creation/registration often happens before the cold/F10 skin
    // window is armed. Keep a small process-start census for that phase.
    return;
  }

  const LONG sequence = InterlockedIncrement(&s_eiemGpuClothEventSequence);
  const EiemGpuClothState state = EiemReadGpuClothState(self);
  char meshName[192] = {};
  if (state.characterMesh && EiemOnUnityThread())
    TraceReadUnityObjectName(state.characterMesh, meshName,
                             (int)sizeof(meshName));
  char selfDescription[256] = {};
  TraceDescribeObject(self, selfDescription, (int)sizeof(selfDescription));
  Log("[GPU-CLOTH-BOUNDARY-v1] tx=%ld seq=%ld event=%s self=%p "
      "selfType=\"%s\" mesh=%p meshName=\"%s\" skeletonBuffer=%p returned=%p "
      "result=%d dt=%.6f streaming=%d flipped=%.3f clothNum=%d "
      "groupNum=%d argument=%p caller=%p tid=%lu tick=%llu",
      transaction, sequence, event ? event : "unknown", self,
      selfDescription[0] ? selfDescription : "?", state.characterMesh,
      meshName[0] ? meshName : "?",
      state.skeletonBuffer, returnedBuffer, result, (double)deltaTime,
      state.isStreamingMode ? 1 : 0, (double)state.skeletonFlipped,
      state.runtimeClothNum, state.runtimeClothGroupNum, argument,
      _ReturnAddress(), (unsigned long)GetCurrentThreadId(),
      (unsigned long long)GetTickCount64());
}

static void TraceGpuClothTick(void *self, float deltaTime, void *methodInfo) {
  auto original = (TraceGpuClothTickFn)s_origGpuClothTick;
  if (original) original(self, deltaTime, methodInfo);
  EiemLogGpuClothEvent("Tick", self, deltaTime, nullptr, 0, nullptr);
}

static void TraceGpuClothSetPerDrawData(void *self, void *methodInfo) {
  auto original = (TraceVoidMethodFn)s_origGpuClothSetPerDrawData;
  if (original) original(self, methodInfo);
  EiemLogGpuClothEvent("SetPerDrawData", self, 0.0f, nullptr, 0, nullptr);
}

static void TraceGpuClothPipelineUpdateV2(void *self, void *transform,
                                          void *methodInfo) {
  auto original = (TraceGpuClothPipelineUpdateV2Fn)s_origGpuClothPipelineUpdateV2;
  if (original) original(self, transform, methodInfo);
  EiemLogGpuClothEvent("PipelineUpdateV2", self, 0.0f, transform, 0,
                       nullptr);
}

// Some builds expose PipelineUpdateV2 as a static helper.  Keeping a separate
// ABI for that case avoids treating its first Transform argument as a
// GpuClothManager object and reading unrelated memory as manager fields.
static void TraceGpuClothPipelineUpdateV2Static(void *transform,
                                                void *methodInfo) {
  auto original = (TraceGpuClothPipelineUpdateV2StaticFn)
      s_origGpuClothPipelineUpdateV2Static;
  if (original) original(transform, methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (transaction) {
    if (InterlockedIncrement(&s_eiemGpuClothObservationCalls) > 512)
      return;
  } else if (InterlockedIncrement(&s_eiemGpuClothStartupCalls) > 128) {
    return;
  }
  const LONG sequence = InterlockedIncrement(&s_eiemGpuClothEventSequence);
  char transformDescription[256] = {};
  TraceDescribeObject(transform, transformDescription,
                      (int)sizeof(transformDescription));
  Log("[GPU-CLOTH-BOUNDARY-v1] tx=%ld seq=%ld event=PipelineUpdateV2.static "
      "transform=%p transformType=\"%s\" caller=%p tid=%lu tick=%llu",
      transaction, sequence, transform,
      transformDescription[0] ? transformDescription : "?", _ReturnAddress(),
      (unsigned long)GetCurrentThreadId(), (unsigned long long)GetTickCount64());
}

static void TraceGpuClothRegisterGroup(void *self, void *clothGroupData,
                                       void *methodInfo) {
  auto original =
      (TraceGpuClothRegisterGroupFn)s_origGpuClothRegisterGroup;
  if (original) original(self, clothGroupData, methodInfo);
  EiemLogGpuClothEvent("RegisterClothGroup", self, 0.0f, clothGroupData, 0,
                       nullptr);
}

static void TraceGpuClothSetCharacterProxyMesh(void *self, void *mesh,
                                               void *methodInfo) {
  auto original = (TraceGpuClothSetCharacterProxyMeshFn)
      s_origGpuClothSetCharacterProxyMesh;
  if (original) original(self, mesh, methodInfo);
  EiemLogGpuClothEvent("_SetCharacterProxyMesh", self, 0.0f, mesh,
                       mesh ? 1 : 0, mesh);
}

static void TraceGpuClothFlipSkeletonFlag(void *self, void *methodInfo) {
  auto original = (TraceVoidMethodFn)s_origGpuClothFlipSkeletonFlag;
  if (original) original(self, methodInfo);
  EiemLogGpuClothEvent("FlipSkeletonFlag", self, 0.0f, nullptr, 0, nullptr);
}

static void *TraceGpuClothGetSkeletonBuffer(void *self, void *methodInfo) {
  auto original =
      (TraceGpuClothGetSkeletonBufferFn)s_origGpuClothGetSkeletonBuffer;
  void *result = original ? original(self, methodInfo) : nullptr;
  EiemLogGpuClothEvent("GetSkeletonBuffer", self, 0.0f, nullptr,
                       result ? 1 : 0, result);
  return result;
}

static bool TraceGpuClothIsSkeletonValid(void *self, void *methodInfo) {
  auto original = (TraceGpuClothBoolFn)s_origGpuClothIsSkeletonValid;
  const bool result = original ? original(self, methodInfo) : false;
  EiemLogGpuClothEvent("IsClothSkeletonValid", self, 0.0f, nullptr,
                       result ? 1 : 0, nullptr);
  return result;
}

static bool TraceGpuClothIsSkeletonFlipped(void *self, void *methodInfo) {
  auto original = (TraceGpuClothBoolFn)s_origGpuClothIsSkeletonFlipped;
  const bool result = original ? original(self, methodInfo) : false;
  EiemLogGpuClothEvent("IsClothSkeletonFlipped", self, 0.0f, nullptr,
                       result ? 1 : 0, nullptr);
  return result;
}

static void TraceMaterialPropertyBlockSetBuffer(
    void *self, int32_t propertyId, void *buffer, int32_t offset,
    int32_t size, void *methodInfo) {
  auto original = (TraceMaterialPropertyBlockSetBufferFn)
      s_origMaterialPropertyBlockSetBuffer;
  if (original)
    original(self, propertyId, buffer, offset, size, methodInfo);

  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!transaction ||
      InterlockedIncrement(&s_eiemSkinBufferBindingCalls) > 256)
    return;

  Log("[SKIN-BUFFER-BIND-v1] tx=%ld kind=buffer propertyId=%d block=%p "
      "buffer=%p offset=%d size=%d caller=%p tick=%llu tid=%lu",
      transaction, propertyId, self, buffer, offset, size,
      _ReturnAddress(), (unsigned long long)GetTickCount64(),
      (unsigned long)GetCurrentThreadId());
}

static void TraceMaterialPropertyBlockSetConstantBuffer(
    void *self, int32_t propertyId, void *buffer, int32_t offset,
    int32_t size, void *methodInfo) {
  auto original = (TraceMaterialPropertyBlockSetBufferFn)
      s_origMaterialPropertyBlockSetConstantBuffer;
  if (original)
    original(self, propertyId, buffer, offset, size, methodInfo);

  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!transaction ||
      InterlockedIncrement(&s_eiemSkinBufferBindingCalls) > 256)
    return;

  Log("[SKIN-BUFFER-BIND-v1] tx=%ld kind=constant propertyId=%d block=%p "
      "buffer=%p offset=%d size=%d caller=%p tick=%llu tid=%lu",
      transaction, propertyId, self, buffer, offset, size,
      _ReturnAddress(), (unsigned long long)GetTickCount64(),
      (unsigned long)GetCurrentThreadId());
}

static void TraceMaterialSetConstantBuffer(
    void *self, int32_t propertyId, void *buffer, int32_t offset,
    int32_t size, void *methodInfo) {
  auto original = (TraceMaterialPropertyBlockSetBufferFn)
      s_origMaterialSetConstantBuffer;
  if (original)
    original(self, propertyId, buffer, offset, size, methodInfo);

  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!transaction ||
      InterlockedIncrement(&s_eiemSkinBufferBindingCalls) > 256)
    return;

  Log("[SKIN-BUFFER-BIND-v1] tx=%ld kind=material-constant propertyId=%d "
      "material=%p buffer=%p offset=%d size=%d caller=%p tick=%llu tid=%lu",
      transaction, propertyId, self, buffer, offset, size,
      _ReturnAddress(), (unsigned long long)GetTickCount64(),
      (unsigned long)GetCurrentThreadId());
}

static void *TraceRenderGraphGetComputeBuffer(void *self, void *handle,
                                              void *methodInfo) {
  auto original = (TraceRenderGraphGetComputeBufferFn)
      s_origRenderGraphGetComputeBuffer;
  void *buffer = original ? original(self, handle, methodInfo) : nullptr;

  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!transaction ||
      InterlockedIncrement(&s_eiemSkinBufferBindingCalls) > 256)
    return buffer;

  uint64_t raw0 = 0;
  uint64_t raw1 = 0;
  if (handle) {
    __try {
      raw0 = *(const uint64_t *)handle;
      raw1 = *((const uint64_t *)handle + 1);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
      raw0 = 0;
      raw1 = 0;
    }
  }
  Log("[SKIN-BUFFER-RESOURCE-v1] tx=%ld registry=%p handle=%p "
      "raw0=%016llX raw1=%016llX buffer=%p caller=%p tick=%llu tid=%lu",
      transaction, self, handle, (unsigned long long)raw0,
      (unsigned long long)raw1, buffer, _ReturnAddress(),
      (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentThreadId());
  return buffer;
}

static bool EiemTraceCommandBufferBudget(LONG transaction) {
  return transaction &&
         InterlockedIncrement(&s_eiemSkinBufferBindingCalls) <= 512;
}

static void TraceCommandBufferSetGlobalConstantBuffer0(
    void *self, uint32_t bufferId, int32_t propertyId, int32_t offset,
    int32_t size, void *methodInfo) {
  auto original = (TraceCommandBufferSetGlobalConstantBuffer0Fn)
      s_origCommandBufferSetGlobalConstantBuffer0;
  if (original)
    original(self, bufferId, propertyId, offset, size, methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!EiemTraceCommandBufferBudget(transaction)) return;
  Log("[SKIN-CMD-BUFFER-v1] tx=%ld kind=global-constant-id cmd=%p "
      "bufferId=%u propertyId=%d offset=%d size=%d caller=%p tick=%llu tid=%lu",
      transaction, self, bufferId, propertyId, offset, size, _ReturnAddress(),
      (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentThreadId());
}

static void TraceCommandBufferSetGlobalBufferId(
    void *self, int32_t propertyId, uint32_t bufferId, void *methodInfo) {
  auto original = (TraceCommandBufferSetGlobalBufferIdFn)
      s_origCommandBufferSetGlobalBufferId;
  if (original)
    original(self, propertyId, bufferId, methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!EiemTraceCommandBufferBudget(transaction)) return;
  Log("[SKIN-CMD-BUFFER-v1] tx=%ld kind=global-buffer-id cmd=%p "
      "propertyId=%d bufferId=%u caller=%p tick=%llu tid=%lu",
      transaction, self, propertyId, bufferId, _ReturnAddress(),
      (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentThreadId());
}

static void TraceCommandBufferSetGlobalConstantBuffer(
    void *self, void *buffer, int32_t propertyId, int32_t offset,
    int32_t size, void *methodInfo) {
  auto original = (TraceCommandBufferSetGlobalConstantBufferFn)
      s_origCommandBufferSetGlobalConstantBuffer;
  if (original)
    original(self, buffer, propertyId, offset, size, methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!EiemTraceCommandBufferBudget(transaction)) return;
  Log("[SKIN-CMD-BUFFER-v1] tx=%ld kind=global-constant cmd=%p buffer=%p "
      "propertyId=%d offset=%d size=%d caller=%p tick=%llu tid=%lu",
      transaction, self, buffer, propertyId, offset, size, _ReturnAddress(),
      (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentThreadId());
}

static void TraceCommandBufferSetGlobalBuffer(
    void *self, int32_t propertyId, void *buffer, void *methodInfo) {
  auto original = (TraceCommandBufferSetGlobalBufferFn)
      s_origCommandBufferSetGlobalBuffer;
  if (original)
    original(self, propertyId, buffer, methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  if (!EiemTraceCommandBufferBudget(transaction)) return;
  Log("[SKIN-CMD-BUFFER-v1] tx=%ld kind=global-buffer cmd=%p "
      "propertyId=%d buffer=%p caller=%p tick=%llu tid=%lu",
      transaction, self, propertyId, buffer, _ReturnAddress(),
      (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentThreadId());
}

static void EiemTraceGpuDrivenSubmit(const char *kind, void *self,
                                     void *commandBuffer, uint32_t id,
                                     bool flag, LONG transaction) {
  const LONG call = InterlockedIncrement(&s_eiemGpuDrivenCalls);
  if (call > 512) return;
  Log("[SKIN-GPU-SUBMIT-v1] tx=%ld call=%ld kind=%s renderer=%p cmd=%p id=%u flag=%d "
      "caller=%p tick=%llu tid=%lu",
      transaction, call, kind, self, commandBuffer, id, flag ? 1 : 0,
      _ReturnAddress(), (unsigned long long)GetTickCount64(),
      (unsigned long)GetCurrentThreadId());
}

static void TraceGpuV1BindBuffersForRendering(
    void *self, void *commandBuffer, void *methodInfo) {
  auto original = (TraceGpuDrivenBindBuffersForRenderingFn)
      s_origGpuDrivenV1BindBuffersForRendering;
  if (original) original(self, commandBuffer, methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  EiemTraceGpuDrivenSubmit("v1-bind-render", self, commandBuffer, 0, false,
                           transaction);
}

static void TraceGpuV1PopulatePerFrameData(
    void *self, void *commandBuffer, uint32_t frameDataId,
    uint32_t rendererDataId, bool flag, void *methodInfo) {
  auto original = (TraceGpuDrivenPopulatePerFrameDataFn)
      s_origGpuDrivenV1PopulatePerFrameData;
  if (original)
    original(self, commandBuffer, frameDataId, rendererDataId, flag,
             methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  EiemTraceGpuDrivenSubmit("v1-populate-frame", self, commandBuffer,
                           frameDataId, flag, transaction);
  if (transaction && InterlockedCompareExchange(&s_eiemSkinBufferBindingCalls,
                                                 0, 0) <= 512) {
    Log("[SKIN-GPU-FRAME-v1] tx=%ld renderer=%p frameDataId=%u "
        "rendererDataId=%u cmd=%p flag=%d",
        transaction, self, frameDataId, rendererDataId, commandBuffer,
        flag ? 1 : 0);
  }
}

static void TraceGpuV1DrawRendererList(
    void *self, void *commandBuffer, uint32_t rendererListId, bool flag,
    void *methodInfo) {
  auto original = (TraceGpuDrivenDrawRendererListFn)
      s_origGpuDrivenV1DrawRendererList;
  if (original)
    original(self, commandBuffer, rendererListId, flag, methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  EiemTraceGpuDrivenSubmit("v1-draw-list", self, commandBuffer,
                           rendererListId, flag, transaction);
}

static void TraceGpuV2BindBuffersForRendering(
    void *self, void *commandBuffer, void *methodInfo) {
  auto original = (TraceGpuDrivenBindBuffersForRenderingFn)
      s_origGpuDrivenV2BindBuffersForRendering;
  if (original) original(self, commandBuffer, methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  EiemTraceGpuDrivenSubmit("v2-bind-render", self, commandBuffer, 0, false,
                           transaction);
}

static void TraceGpuV2PopulatePerFrameData(
    void *self, void *commandBuffer, uint32_t frameDataId,
    uint32_t rendererDataId, bool flag, void *methodInfo) {
  auto original = (TraceGpuDrivenPopulatePerFrameDataFn)
      s_origGpuDrivenV2PopulatePerFrameData;
  if (original)
    original(self, commandBuffer, frameDataId, rendererDataId, flag,
             methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  EiemTraceGpuDrivenSubmit("v2-populate-frame", self, commandBuffer,
                           frameDataId, flag, transaction);
  if (transaction && InterlockedCompareExchange(&s_eiemSkinBufferBindingCalls,
                                                 0, 0) <= 512) {
    Log("[SKIN-GPU-FRAME-v1] tx=%ld renderer=%p frameDataId=%u "
        "rendererDataId=%u cmd=%p flag=%d",
        transaction, self, frameDataId, rendererDataId, commandBuffer,
        flag ? 1 : 0);
  }
}

static void TraceGpuV2DrawRendererList(
    void *self, void *commandBuffer, uint32_t rendererListId, bool flag,
    void *methodInfo) {
  auto original = (TraceGpuDrivenDrawRendererListFn)
      s_origGpuDrivenV2DrawRendererList;
  if (original)
    original(self, commandBuffer, rendererListId, flag, methodInfo);
  const LONG transaction =
      InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);
  EiemTraceGpuDrivenSubmit("v2-draw-list", self, commandBuffer,
                           rendererListId, flag, transaction);
}

#define EIEM_DEFINE_GPU_AUX_WRAPPERS(PREFIX, ORIG_PREFIX, TAG)                 \
  static void PREFIX##BindBuffersForCulling(                                  \
      void *self, void *commandBuffer, void *computeShader,                  \
      uint32_t bufferId, void *methodInfo) {                                  \
    auto original = (TraceGpuDrivenBindBuffersForCullingFn)                  \
        ORIG_PREFIX##BindBuffersForCulling;                                   \
    if (original) original(self, commandBuffer, computeShader, bufferId,     \
                           methodInfo);                                      \
    const LONG transaction =                                                    \
        InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);      \
    EiemTraceGpuDrivenSubmit(TAG "-bind-cull", self, commandBuffer,          \
                             bufferId, false, transaction);                   \
  }                                                                            \
  static void PREFIX##BindFrameConstants(                                      \
      void *self, void *commandBuffer, void *computeShader,                   \
      uint32_t bufferId, void *methodInfo) {                                  \
    auto original = (TraceGpuDrivenBindFrameConstantsFn)                     \
        ORIG_PREFIX##BindFrameConstants;                                      \
    if (original) original(self, commandBuffer, computeShader, bufferId,     \
                           methodInfo);                                      \
    const LONG transaction =                                                    \
        InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);      \
    EiemTraceGpuDrivenSubmit(TAG "-bind-frame-constants", self,             \
                             commandBuffer, bufferId, false, transaction);    \
  }                                                                            \
  static void PREFIX##BindFrameConstantsGlobal(                                \
      void *self, void *commandBuffer, void *methodInfo) {                    \
    auto original = (TraceGpuDrivenBindFrameConstantsGlobalFn)                \
        ORIG_PREFIX##BindFrameConstantsGlobal;                                 \
    if (original) original(self, commandBuffer, methodInfo);                  \
    const LONG transaction =                                                    \
        InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);      \
    EiemTraceGpuDrivenSubmit(TAG "-bind-frame-global", self,                 \
                             commandBuffer, 0, false, transaction);           \
  }                                                                            \
  static void PREFIX##DispatchMeshletInstanceCount(                           \
      void *self, void *commandBuffer, void *computeShader,                   \
      uint32_t dispatchId, void *methodInfo) {                                \
    auto original = (TraceGpuDrivenDispatchComputeFn)                         \
        ORIG_PREFIX##DispatchMeshletInstanceCount;                             \
    if (original) original(self, commandBuffer, computeShader, dispatchId,   \
                            methodInfo);                                      \
    const LONG transaction =                                                    \
        InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);      \
    EiemTraceGpuDrivenSubmit(TAG "-dispatch-meshlet", self, commandBuffer,  \
                             dispatchId, false, transaction);                  \
  }                                                                            \
  static void PREFIX##DispatchDrawBucketCount(                                \
      void *self, void *commandBuffer, void *computeShader,                   \
      uint32_t dispatchId, void *methodInfo) {                                \
    auto original = (TraceGpuDrivenDispatchComputeFn)                         \
        ORIG_PREFIX##DispatchDrawBucketCount;                                  \
    if (original) original(self, commandBuffer, computeShader, dispatchId,    \
                            methodInfo);                                      \
    const LONG transaction =                                                    \
        InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);      \
    EiemTraceGpuDrivenSubmit(TAG "-dispatch-bucket", self, commandBuffer,   \
                             dispatchId, false, transaction);                  \
  }                                                                            \
  static void PREFIX##AdvanceFrame(void *self, void *methodInfo) {             \
    auto original = (TraceGpuDrivenAdvanceFrameFn)                           \
        ORIG_PREFIX##AdvanceFrame;                                             \
    if (original) original(self, methodInfo);                                  \
    const LONG transaction =                                                    \
        InterlockedCompareExchange(&s_eiemSkinTimingProbePending, 0, 0);      \
    if (transaction &&                                                         \
        InterlockedIncrement(&s_eiemSkinBufferBindingCalls) <= 512) {         \
      Log("[SKIN-GPU-SUBMIT-v1] tx=%ld kind=" TAG "-advance-frame "         \
          "renderer=%p cmd=%p id=0 flag=0 caller=%p tick=%llu tid=%lu",       \
          transaction, self, nullptr, _ReturnAddress(),                       \
          (unsigned long long)GetTickCount64(),                                \
          (unsigned long)GetCurrentThreadId());                                \
    }                                                                            \
  }

EIEM_DEFINE_GPU_AUX_WRAPPERS(TraceGpuV1, s_origGpuDrivenV1, "v1")
EIEM_DEFINE_GPU_AUX_WRAPPERS(TraceGpuV2, s_origGpuDrivenV2, "v2")
#undef EIEM_DEFINE_GPU_AUX_WRAPPERS

static void HookTraceMethod(void *klass, const char *methodName, int paramCount,
                            const char *label, void *detour, void **original) {
  if (!klass) return;
  void *method = FindMethodInHierarchy(klass, methodName, paramCount);
  if (!method) {
    Log("[RES-TRACE] %s not found", label);
    return;
  }
  if (Hook(method, label, detour, original))
    Log("[RES-TRACE] %s observation hook installed", label);
  else
    Log("[RES-TRACE] %s hook failed", label);
}

static void *FindMethodWithParamTypes(void *klass, const char *methodName,
                                      const char *const *paramTypes,
                                      int paramCount) {
  for (void *current = klass; current && il2cpp_class_get_methods;
       current = il2cpp_class_get_parent
                     ? il2cpp_class_get_parent(current)
                     : nullptr) {
    void *iterator = nullptr;
    void *method = nullptr;
    while ((method = il2cpp_class_get_methods(current, &iterator))) {
      const char *name = il2cpp_method_get_name(method);
      if (!name || strcmp(name, methodName) != 0 ||
          (int)il2cpp_method_get_param_count(method) != paramCount)
        continue;
      bool matches = true;
      for (int i = 0; i < paramCount; ++i) {
        void *type = il2cpp_method_get_param(method, (uint32_t)i);
        const char *typeName = type && il2cpp_type_get_name
                                   ? il2cpp_type_get_name(type)
                                   : nullptr;
        if (!typeName || strcmp(typeName, paramTypes[i]) != 0) {
          matches = false;
          break;
        }
      }
      if (matches)
        return method;
    }
  }
  return nullptr;
}

// Select the concrete overload when IL2CPP also exposes a stripped generic
// placeholder with the same name and parameter count.
static void *FindMethodWithReturnType(void *klass, const char *methodName,
                                      const char *returnType, int paramCount) {
  for (void *current = klass; current && il2cpp_class_get_methods;
       current = il2cpp_class_get_parent
                     ? il2cpp_class_get_parent(current)
                     : nullptr) {
    void *iterator = nullptr;
    void *method = nullptr;
    while ((method = il2cpp_class_get_methods(current, &iterator))) {
      const char *name = il2cpp_method_get_name(method);
      if (!name || strcmp(name, methodName) != 0 ||
          (int)il2cpp_method_get_param_count(method) != paramCount)
        continue;
      void *retType = il2cpp_method_get_return_type
                          ? il2cpp_method_get_return_type(method)
                          : nullptr;
      const char *retName = retType && il2cpp_type_get_name
                                ? il2cpp_type_get_name(retType)
                                : nullptr;
      if (retName && strcmp(retName, returnType) == 0 &&
          ((MInfo *)method)->mp)
        return method;
    }
  }
  return nullptr;
}

static void *FindMethodWithParamTypesAndReturnType(
    void *klass, const char *methodName, const char *const *paramTypes,
    int paramCount, const char *returnType) {  for (void *current = klass; current && il2cpp_class_get_methods;
       current = il2cpp_class_get_parent
                     ? il2cpp_class_get_parent(current)
                     : nullptr) {
    void *iterator = nullptr;
    void *method = nullptr;
    while ((method = il2cpp_class_get_methods(current, &iterator))) {
      const char *name = il2cpp_method_get_name(method);
      if (!name || strcmp(name, methodName) != 0 ||
          (int)il2cpp_method_get_param_count(method) != paramCount)
        continue;
      void *retType = il2cpp_method_get_return_type
                          ? il2cpp_method_get_return_type(method)
                          : nullptr;
      const char *retName = retType && il2cpp_type_get_name
                                ? il2cpp_type_get_name(retType)
                                : nullptr;
      if (!retName || !returnType || strcmp(retName, returnType) != 0)
        continue;
      bool matches = true;
      for (int index = 0; index < paramCount; ++index) {
        void *paramType = il2cpp_method_get_param(
            method, (uint32_t)index);
        const char *paramName = paramType && il2cpp_type_get_name
                                    ? il2cpp_type_get_name(paramType)
                                    : nullptr;
        if (!paramName || !paramTypes || !paramTypes[index] ||
            strcmp(paramName, paramTypes[index]) != 0) {
          matches = false;
          break;
        }
      }
      if (matches && ((MInfo *)method)->mp) return method;
    }
  }
  return nullptr;
}

static void HookTraceMethodWithParamTypes(
    void *klass, const char *methodName, const char *const *paramTypes,
    int paramCount, const char *label, void *detour, void **original) {
  if (!klass) return;
  void *method =
      FindMethodWithParamTypes(klass, methodName, paramTypes, paramCount);
  if (!method) {
    Log("[RES-TRACE] %s not found", label);
    return;
  }
  if (Hook(method, label, detour, original))
    Log("[RES-TRACE] %s observation hook installed", label);
  else
    Log("[RES-TRACE] %s hook failed", label);
}

static void HookTraceMethodWithParamTypesAndReturnType(
    void *klass, const char *methodName, const char *const *paramTypes,
    int paramCount, const char *label, const char *returnType, void *detour,
    void **original) {
  if (!klass) return;
  void *method = FindMethodWithParamTypesAndReturnType(
      klass, methodName, paramTypes, paramCount, returnType);
  if (!method) {
    Log("[RES-TRACE] %s not found", label);
    return;
  }
  if (Hook(method, label, detour, original))
    Log("[RES-TRACE] %s observation hook installed", label);
  else
    Log("[RES-TRACE] %s hook failed", label);
}

static void HookTraceGpuClothPipelineUpdateV2(void *klass,
                                              const char *const *paramTypes,
                                              int paramCount,
                                              const char *label,
                                              void *instanceDetour,
                                              void **instanceOriginal,
                                              void *staticDetour,
                                              void **staticOriginal) {
  if (!klass) return;
  void *method = FindMethodWithParamTypesAndReturnType(
      klass, "PipelineUpdateV2", paramTypes, paramCount, "System.Void");
  if (!method) {
    Log("[RES-TRACE] %s not found", label);
    return;
  }
  uint32_t impl = 0;
  const uint32_t flags = il2cpp_method_get_flags
                             ? il2cpp_method_get_flags(method, &impl)
                             : 0;
  const bool isStatic = (flags & 0x10u) != 0;
  Log("[RES-TRACE] %s flags=0x%X static=%d impl=0x%X", label, flags,
      isStatic ? 1 : 0, impl);
  void *detour = isStatic ? staticDetour : instanceDetour;
  void **original = isStatic ? staticOriginal : instanceOriginal;
  if (Hook(method, label, detour, original))
    Log("[RES-TRACE] %s observation hook installed", label);
  else
    Log("[RES-TRACE] %s hook failed", label);
}

static void *FindMaterialRendererInfoClass(void **assemblies,
                                           size_t assemblyCount) {
  if (!assemblies || !assemblyCount) return nullptr;
  for (size_t assemblyIndex = 0; assemblyIndex < assemblyCount;
       ++assemblyIndex) {
    void *image = il2cpp_assembly_get_image(assemblies[assemblyIndex]);
    if (!image) continue;
    const size_t classCount = il2cpp_image_get_class_count(image);
    for (size_t classIndex = 0; classIndex < classCount; ++classIndex) {
      void *klass = il2cpp_image_get_class(image, classIndex);
      if (!klass) continue;
      const char *name = il2cpp_class_get_name(klass);
      if (!name || strcmp(name, "RendererInfo") != 0 ||
          !FindMethodInHierarchy(klass, "TrySetSharedMaterial", 1) ||
          !FindMethodInHierarchy(klass, "TrySetSharedMaterials", 1) ||
          !FindMethodInHierarchy(klass, "TryReplaceSharedMaterials", 1))
        continue;

      const char *rendererFields[] = {"m_renderer",
                                      "<renderer>k__BackingField"};
      int rendererOffset = FindFieldInHierarchy(
          klass, rendererFields, _countof(rendererFields), nullptr);
      if (rendererOffset < 0)
        rendererOffset = FindFieldByTypeInHierarchy(
            klass, "UnityEngine.Renderer", nullptr);
      if (rendererOffset < 0) continue;

      s_materialRendererInfoRendererOffset = rendererOffset;
      return klass;
    }
  }
  return nullptr;
}

#include "eiem_native_physics_runtime.h"
#include "eiem_npc_model_owner.h"
#include "eiem_metadata_probe.h"
#include "eiem_visibility_controller_probe.h"

static void InitIl2CppResourceTrace(void **assemblies, size_t assemblyCount) {
  if (!assemblies || assemblyCount == 0) return;
  // Read-only metadata reconnaissance. The static route to these names is
  // blocked (Il2CppDumper cannot resolve this build's registration pointers), and
  // several earlier hooks were guessed wrong, so the exact class names and field
  // offsets are enumerated once here instead.
  // Metadata enumeration and the part-table mutation are research-only paths.
  // The former floods the startup log; the latter writes SubMeshInfo.isActive.
  // Re-enable them only in a dedicated evidence build.
  Log("[VALIDATION] mode=static-resource-baseline metadata-enumeration=%s "
      "part-table-mutation=off",
      kEiemEnableCustomSkinPipelineMetadata ? "custom-skin-only" : "off");
  Log("[PHYSICS-MODE] experimentalRuntime=%s nativeObservation=%s",
      kEiemEnableExperimentalPhysicsRuntime ? "enabled" : "disabled",
      kEiemEnableNativePhysicsObservation ? "enabled" : "disabled");
  EiemInitUnityLifetime(assemblies, assemblyCount);
  EiemInstallNpcModelOwner(assemblies, assemblyCount);
  if (kEiemEnableCustomSkinPipelineMetadata)
    EiemDumpCustomSkinPipelineMetadata(assemblies, assemblyCount);
  if (kEiemEnableVisibilityMetadataProbe)
    EiemDumpVisibilityPipelineMetadata(assemblies, assemblyCount);
  EiemInstallVisibilityControllerProbe(assemblies, assemblyCount);
  EiemInstallDitherProbe(assemblies, assemblyCount);

  // This manager is a candidate custom-pipeline boundary. The hook is
  // observation-only and bounded by the existing cold/F10 timing window; it
  // is intentionally installed only with the custom-pipeline observation
  // switch enabled, so production rendering pays no per-frame logging cost.
  if (kEiemEnableCustomSkinPipelineObservation) {
    void *captureManagerClass = FindClass(
        "HG.Rendering.Runtime", "SkinnedMeshCaptureManager", assemblies,
        assemblyCount);
    if (captureManagerClass) {
      static const char *const captureRequestTypes[] = {
          "UnityEngine.MeshRenderer", "UnityEngine.SkinnedMeshRenderer",
          "UnityEngine.MaterialPropertyBlock"};
      HookTraceMethodWithParamTypesAndReturnType(
          captureManagerClass, "RequestCapture", captureRequestTypes, 3,
          "SkinnedMeshCaptureManager.RequestCapture", "System.Void",
          (void *)TraceSkinnedMeshCaptureRequest,
          &s_origSkinnedMeshCaptureRequest);
    } else {
      Log("[RES-TRACE] HG.Rendering.Runtime.SkinnedMeshCaptureManager class not found");
    }

    // This is the game's custom cloth simulation/upload owner.  Unlike the
    // public Unity skin APIs, its metadata exposes the actual cloth skeleton
    // ComputeBuffer and the render-graph handoff methods.  Keep every hook
    // observation-only and resolve overloads by both parameter and return
    // type; value-type cloth data is passed through opaquely.
    void *gpuClothManagerClass = FindClass(
        "HG.Rendering.Runtime", "GpuClothManager", assemblies,
        assemblyCount);
    if (gpuClothManagerClass) {
      static const char *const gpuClothTickTypes[] = {"System.Single"};
      HookTraceMethodWithParamTypesAndReturnType(
          gpuClothManagerClass, "Tick", gpuClothTickTypes, 1,
          "GpuClothManager.Tick", "System.Void",
          (void *)TraceGpuClothTick, &s_origGpuClothTick);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuClothManagerClass, "_SetPerDrawData", nullptr, 0,
          "GpuClothManager._SetPerDrawData", "System.Void",
          (void *)TraceGpuClothSetPerDrawData,
          &s_origGpuClothSetPerDrawData);
      static const char *const gpuClothRegisterTypes[] = {
          "HG.Rendering.Runtime.ClothGroupData&"};
      HookTraceMethodWithParamTypesAndReturnType(
          gpuClothManagerClass, "RegisterClothGroup", gpuClothRegisterTypes,
          1, "GpuClothManager.RegisterClothGroup", "System.Void",
          (void *)TraceGpuClothRegisterGroup, &s_origGpuClothRegisterGroup);
      static const char *const gpuClothMeshTypes[] = {"UnityEngine.Mesh"};
      HookTraceMethodWithParamTypesAndReturnType(
          gpuClothManagerClass, "_SetCharacterProxyMesh", gpuClothMeshTypes,
          1, "GpuClothManager._SetCharacterProxyMesh", "System.Void",
          (void *)TraceGpuClothSetCharacterProxyMesh,
          &s_origGpuClothSetCharacterProxyMesh);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuClothManagerClass, "GetSkeletonBuffer", nullptr, 0,
          "GpuClothManager.GetSkeletonBuffer",
          "UnityEngine.ComputeBuffer",
          (void *)TraceGpuClothGetSkeletonBuffer,
          &s_origGpuClothGetSkeletonBuffer);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuClothManagerClass, "IsClothSkeletonValid", nullptr, 0,
          "GpuClothManager.IsClothSkeletonValid", "System.Boolean",
          (void *)TraceGpuClothIsSkeletonValid,
          &s_origGpuClothIsSkeletonValid);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuClothManagerClass, "IsClothSkeletonFlipped", nullptr, 0,
          "GpuClothManager.IsClothSkeletonFlipped", "System.Boolean",
          (void *)TraceGpuClothIsSkeletonFlipped,
          &s_origGpuClothIsSkeletonFlipped);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuClothManagerClass, "FlipSkeletonFlag", nullptr, 0,
          "GpuClothManager.FlipSkeletonFlag", "System.Void",
          (void *)TraceGpuClothFlipSkeletonFlag,
          &s_origGpuClothFlipSkeletonFlag);
      static const char *const gpuClothPipelineTypes[] = {
          "UnityEngine.Transform"};
      HookTraceGpuClothPipelineUpdateV2(
          gpuClothManagerClass, gpuClothPipelineTypes, 1,
          "GpuClothManager.PipelineUpdateV2",
          (void *)TraceGpuClothPipelineUpdateV2,
          &s_origGpuClothPipelineUpdateV2,
          (void *)TraceGpuClothPipelineUpdateV2Static,
          &s_origGpuClothPipelineUpdateV2Static);
    } else {
      Log("[RES-TRACE] HG.Rendering.Runtime.GpuClothManager class not found");
    }

    // The target may bypass SkinnedMeshCaptureManager entirely and bind the
    // palette through a MaterialPropertyBlock.  Observe both regular and
    // constant-buffer bindings so the next run can distinguish "no upload"
    // from "upload to a wrong ring-buffer segment" without touching the draw.
    void *propertyBlockClass = FindClass(
        "UnityEngine", "MaterialPropertyBlock", assemblies, assemblyCount);
    if (propertyBlockClass) {
      static const char *const bufferTypes[] = {
          "System.Int32", "UnityEngine.ComputeBuffer", "System.Int32",
          "System.Int32"};
      HookTraceMethodWithParamTypesAndReturnType(
          propertyBlockClass, "SetBufferImpl", bufferTypes, 4,
          "MaterialPropertyBlock.SetBufferImpl", "System.Void",
          (void *)TraceMaterialPropertyBlockSetBuffer,
          &s_origMaterialPropertyBlockSetBuffer);
      HookTraceMethodWithParamTypesAndReturnType(
          propertyBlockClass, "SetConstantBufferImpl", bufferTypes, 4,
          "MaterialPropertyBlock.SetConstantBufferImpl", "System.Void",
          (void *)TraceMaterialPropertyBlockSetConstantBuffer,
          &s_origMaterialPropertyBlockSetConstantBuffer);
    } else {
      Log("[RES-TRACE] UnityEngine.MaterialPropertyBlock class not found");
    }

    void *materialClass =
        FindClass("UnityEngine", "Material", assemblies, assemblyCount);
    if (materialClass) {
      static const char *const materialBufferTypes[] = {
          "System.Int32", "UnityEngine.ComputeBuffer", "System.Int32",
          "System.Int32"};
      HookTraceMethodWithParamTypesAndReturnType(
          materialClass, "SetConstantBufferImpl", materialBufferTypes, 4,
          "Material.SetConstantBufferImpl", "System.Void",
          (void *)TraceMaterialSetConstantBuffer,
          &s_origMaterialSetConstantBuffer);
    } else {
      Log("[RES-TRACE] UnityEngine.Material class not found");
    }

    void *renderGraphRegistryClass = FindClass(
        "HG.Rendering.RenderGraphModule", "HGRenderGraphResourceRegistry",
        assemblies, assemblyCount);
    if (renderGraphRegistryClass) {
      static const char *const computeBufferHandleTypes[] = {
          "HG.Rendering.RenderGraphModule.ComputeBufferHandle&"};
      HookTraceMethodWithParamTypesAndReturnType(
          renderGraphRegistryClass, "GetComputeBuffer",
          computeBufferHandleTypes, 1,
          "HGRenderGraphResourceRegistry.GetComputeBuffer",
          "UnityEngine.ComputeBuffer",
          (void *)TraceRenderGraphGetComputeBuffer,
          &s_origRenderGraphGetComputeBuffer);
    } else {
      Log("[RES-TRACE] HGRenderGraphResourceRegistry class not found");
    }

    // HG's renderer can bypass Material/MaterialPropertyBlock and record
    // the palette directly on UnityEngine.Rendering.CommandBuffer.  These
    // descriptors are the last managed command-recording boundary before
    // the native backend sees the buffer segment.
    void *commandBufferClass = FindClass(
        "UnityEngine.Rendering", "CommandBuffer", assemblies, assemblyCount);
    if (commandBufferClass) {
      static const char *const globalConstantIdTypes[] = {
          "System.UInt32", "System.Int32", "System.Int32", "System.Int32"};
      HookTraceMethodWithParamTypesAndReturnType(
          commandBufferClass, "SetGlobalConstantBufferInternal0",
          globalConstantIdTypes, 4,
          "CommandBuffer.SetGlobalConstantBufferInternal0", "System.Void",
          (void *)TraceCommandBufferSetGlobalConstantBuffer0,
          &s_origCommandBufferSetGlobalConstantBuffer0);

      static const char *const globalBufferIdTypes[] = {
          "System.Int32", "System.UInt32"};
      HookTraceMethodWithParamTypesAndReturnType(
          commandBufferClass, "SetGlobalBufferIDInternal", globalBufferIdTypes,
          2, "CommandBuffer.SetGlobalBufferIDInternal", "System.Void",
          (void *)TraceCommandBufferSetGlobalBufferId,
          &s_origCommandBufferSetGlobalBufferId);

      static const char *const globalConstantTypes[] = {
          "UnityEngine.ComputeBuffer", "System.Int32", "System.Int32",
          "System.Int32"};
      HookTraceMethodWithParamTypesAndReturnType(
          commandBufferClass, "SetGlobalConstantBufferInternal",
          globalConstantTypes, 4,
          "CommandBuffer.SetGlobalConstantBufferInternal", "System.Void",
          (void *)TraceCommandBufferSetGlobalConstantBuffer,
          &s_origCommandBufferSetGlobalConstantBuffer);

      static const char *const globalBufferTypes[] = {
          "System.Int32", "UnityEngine.ComputeBuffer"};
      HookTraceMethodWithParamTypesAndReturnType(
          commandBufferClass, "SetGlobalBufferInternal", globalBufferTypes, 2,
          "CommandBuffer.SetGlobalBufferInternal", "System.Void",
          (void *)TraceCommandBufferSetGlobalBuffer,
          &s_origCommandBufferSetGlobalBuffer);
    } else {
      Log("[RES-TRACE] UnityEngine.Rendering.CommandBuffer class not found");
    }

    // Endfield's HG graphics module has a second submission layer above the
    // backend.  Capture both renderer versions in one pass; all three methods
    // carry the command buffer/list identifiers needed to correlate a skin
    // transaction without touching the renderer data.
    const char *const gpuBindTypes[] = {"UnityEngine.Rendering.CommandBuffer"};
    const char *const gpuPopulateTypes[] = {
        "UnityEngine.Rendering.CommandBuffer", "System.UInt32",
        "System.UInt32", "System.Boolean"};
    const char *const gpuDrawTypes[] = {
        "UnityEngine.Rendering.CommandBuffer", "System.UInt32",
        "System.Boolean"};
    const char *const gpuComputeTypes[] = {
        "UnityEngine.Rendering.CommandBuffer", "UnityEngine.ComputeShader",
        "System.UInt32"};
    const char *const gpuFrameGlobalTypes[] = {
        "UnityEngine.Rendering.CommandBuffer"};
    void *gpuV1Class = FindClass("UnityEngine.HyperGryph",
                                 "GPUDrivenRendererV1", assemblies,
                                 assemblyCount);
    if (gpuV1Class) {
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV1Class, "BindBuffersForRendering", gpuBindTypes, 1,
          "GPUDrivenRendererV1.BindBuffersForRendering", "System.Void",
          (void *)TraceGpuV1BindBuffersForRendering,
          &s_origGpuDrivenV1BindBuffersForRendering);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV1Class, "PopulatePerFrameData", gpuPopulateTypes, 4,
          "GPUDrivenRendererV1.PopulatePerFrameData", "System.Void",
          (void *)TraceGpuV1PopulatePerFrameData,
          &s_origGpuDrivenV1PopulatePerFrameData);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV1Class, "DrawRendererList", gpuDrawTypes, 3,
          "GPUDrivenRendererV1.DrawRendererList", "System.Void",
          (void *)TraceGpuV1DrawRendererList,
          &s_origGpuDrivenV1DrawRendererList);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV1Class, "BindBuffersForCulling", gpuComputeTypes, 3,
          "GPUDrivenRendererV1.BindBuffersForCulling", "System.Void",
          (void *)TraceGpuV1BindBuffersForCulling,
          &s_origGpuDrivenV1BindBuffersForCulling);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV1Class, "BindFrameConstantsBuffer", gpuComputeTypes, 3,
          "GPUDrivenRendererV1.BindFrameConstantsBuffer", "System.Void",
          (void *)TraceGpuV1BindFrameConstants,
          &s_origGpuDrivenV1BindFrameConstants);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV1Class, "BindFrameConstantsBufferGlobal", gpuFrameGlobalTypes,
          1, "GPUDrivenRendererV1.BindFrameConstantsBufferGlobal",
          "System.Void", (void *)TraceGpuV1BindFrameConstantsGlobal,
          &s_origGpuDrivenV1BindFrameConstantsGlobal);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV1Class, "DispatchComputeMeshletInstanceCount", gpuComputeTypes,
          3, "GPUDrivenRendererV1.DispatchComputeMeshletInstanceCount",
          "System.Void", (void *)TraceGpuV1DispatchMeshletInstanceCount,
          &s_origGpuDrivenV1DispatchMeshletInstanceCount);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV1Class, "DispatchComputeDrawBucketCount", gpuComputeTypes, 3,
          "GPUDrivenRendererV1.DispatchComputeDrawBucketCount", "System.Void",
          (void *)TraceGpuV1DispatchDrawBucketCount,
          &s_origGpuDrivenV1DispatchDrawBucketCount);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV1Class, "AdvanceFrame", nullptr, 0,
          "GPUDrivenRendererV1.AdvanceFrame", "System.Void",
          (void *)TraceGpuV1AdvanceFrame, &s_origGpuDrivenV1AdvanceFrame);
    } else {
      Log("[RES-TRACE] GPUDrivenRendererV1 class not found");
    }
    void *gpuV2Class = FindClass("UnityEngine.HyperGryph",
                                 "GPUDrivenRendererV2", assemblies,
                                 assemblyCount);
    if (gpuV2Class) {
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV2Class, "BindBuffersForRendering", gpuBindTypes, 1,
          "GPUDrivenRendererV2.BindBuffersForRendering", "System.Void",
          (void *)TraceGpuV2BindBuffersForRendering,
          &s_origGpuDrivenV2BindBuffersForRendering);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV2Class, "PopulatePerFrameData", gpuPopulateTypes, 4,
          "GPUDrivenRendererV2.PopulatePerFrameData", "System.Void",
          (void *)TraceGpuV2PopulatePerFrameData,
          &s_origGpuDrivenV2PopulatePerFrameData);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV2Class, "DrawRendererList", gpuDrawTypes, 3,
          "GPUDrivenRendererV2.DrawRendererList", "System.Void",
          (void *)TraceGpuV2DrawRendererList,
          &s_origGpuDrivenV2DrawRendererList);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV2Class, "BindBuffersForCulling", gpuComputeTypes, 3,
          "GPUDrivenRendererV2.BindBuffersForCulling", "System.Void",
          (void *)TraceGpuV2BindBuffersForCulling,
          &s_origGpuDrivenV2BindBuffersForCulling);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV2Class, "BindFrameConstantsBuffer", gpuComputeTypes, 3,
          "GPUDrivenRendererV2.BindFrameConstantsBuffer", "System.Void",
          (void *)TraceGpuV2BindFrameConstants,
          &s_origGpuDrivenV2BindFrameConstants);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV2Class, "BindFrameConstantsBufferGlobal", gpuFrameGlobalTypes,
          1, "GPUDrivenRendererV2.BindFrameConstantsBufferGlobal",
          "System.Void", (void *)TraceGpuV2BindFrameConstantsGlobal,
          &s_origGpuDrivenV2BindFrameConstantsGlobal);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV2Class, "DispatchComputeMeshletInstanceCount", gpuComputeTypes,
          3, "GPUDrivenRendererV2.DispatchComputeMeshletInstanceCount",
          "System.Void", (void *)TraceGpuV2DispatchMeshletInstanceCount,
          &s_origGpuDrivenV2DispatchMeshletInstanceCount);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV2Class, "DispatchComputeDrawBucketCount", gpuComputeTypes, 3,
          "GPUDrivenRendererV2.DispatchComputeDrawBucketCount", "System.Void",
          (void *)TraceGpuV2DispatchDrawBucketCount,
          &s_origGpuDrivenV2DispatchDrawBucketCount);
      HookTraceMethodWithParamTypesAndReturnType(
          gpuV2Class, "AdvanceFrame", nullptr, 0,
          "GPUDrivenRendererV2.AdvanceFrame", "System.Void",
          (void *)TraceGpuV2AdvanceFrame, &s_origGpuDrivenV2AdvanceFrame);
    } else {
      Log("[RES-TRACE] GPUDrivenRendererV2 class not found");
    }
  }

  // Install this before the per-Renderer material hook. EntityRenderHelper's
  // original _InitRenderAndMaterial builds the internal renderer registry by
  // scanning its hierarchy.
  // This single boundary is shared by world, NPC and character-preview model
  // paths, so no context-specific array mutation is needed for assembly.
  void *entityRenderHelperClass = FindClass(
      "Beyond.Gameplay.View", "EntityRenderHelper", assemblies,
      assemblyCount);
  if (entityRenderHelperClass) {
    s_entityRenderHelperClass = entityRenderHelperClass;
    HookTraceMethod(
        entityRenderHelperClass, "_InitRenderAndMaterial", 0,
        "EntityRenderHelper._InitRenderAndMaterial",
        (void *)TraceEntityRenderHelperInitRenderAndMaterial,
        &s_origEntityRenderHelperInitRenderAndMaterial);
  } else {
    Log("[RES-TRACE] Beyond.Gameplay.View.EntityRenderHelper class not found");
  }

  // Observe the cache commit made by EntityRenderHelperMaterialController.
  // Its Init signature is resolved by parameter type, so this probe cannot
  // silently bind a same-name overload with a different ABI.
  void *materialControllerClass = FindClass(
      "Beyond.Rendering", "EntityRenderHelperMaterialController", assemblies,
      assemblyCount);
  if (materialControllerClass) {
    const char *controllerInitTypes[] = {
        "System.Collections.Generic.List<UnityEngine.Renderer>",
        "System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>",
        "Beyond.Rendering.EntityCustomizeRendererPropertyConfig",
        "System.Boolean"};
    HookTraceMethodWithParamTypesAndReturnType(
        materialControllerClass, "Init", controllerInitTypes, 4,
        "EntityRenderHelperMaterialController.Init", "System.Void",
        (void *)TraceEntityRenderHelperMaterialControllerInit,
        &s_origEntityRenderHelperMaterialControllerInit);
  } else {
    Log("[RES-TRACE] Beyond.Rendering.EntityRenderHelperMaterialController class not found");
  }

  if (kEiemValidationIdentityProbe) {
    void *assetBundleClass = FindClass("UnityEngine", "AssetBundle", assemblies,
                                       assemblyCount);
    if (assetBundleClass) {
    HookTraceMethod(assetBundleClass, "LoadAsset", 1,
                    "AssetBundle.LoadAsset(string)",
                    (void *)TraceAssetBundleLoadAsset1,
                    &s_origAssetBundleLoadAsset1);
    HookTraceMethod(assetBundleClass, "LoadAsset", 2,
                    "AssetBundle.LoadAsset(string,Type)",
                    (void *)TraceAssetBundleLoadAsset2,
                    &s_origAssetBundleLoadAsset2);
    HookTraceMethod(assetBundleClass, "LoadAssetAsync", 1,
                    "AssetBundle.LoadAssetAsync(string)",
                    (void *)TraceAssetBundleLoadAssetAsync1,
                    &s_origAssetBundleLoadAssetAsync1);
    HookTraceMethod(assetBundleClass, "LoadAssetAsync", 2,
                    "AssetBundle.LoadAssetAsync(string,Type)",
                    (void *)TraceAssetBundleLoadAssetAsync2,
                    &s_origAssetBundleLoadAssetAsync2);
    HookTraceMethod(assetBundleClass, "Unload", 1,
                    "AssetBundle.Unload(bool)",
                    (void *)TraceAssetBundleUnload,
                    &s_origAssetBundleUnload);

    void *bundleRequestClass = FindClass(
        "UnityEngine", "AssetBundleCreateRequest", assemblies, assemblyCount);
    HookTraceMethod(bundleRequestClass, "get_assetBundle", 0,
                    "AssetBundleCreateRequest.get_assetBundle",
                    (void *)TraceAssetBundleCreateRequestGetAssetBundle,
                    &s_origAssetBundleCreateRequestGetAssetBundle);
    } else {
      Log("[RES-TRACE] UnityEngine.AssetBundle not found");
    }
  }

  // Endfield's RendererInfo owns the source/replacement material arrays and
  // is the actual commit boundary used during scene changes. Hook all three
  // commit forms on that one controller type; Unity's public property wrappers
  // are not retained because runtime evidence showed that this path bypasses
  // them.
  void *materialRendererInfoClass =
      FindMaterialRendererInfoClass(assemblies, assemblyCount);
  if (materialRendererInfoClass) {
    const char *infoInitTypes[] = {"UnityEngine.Renderer", "System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>"};
    void *infoInit = FindMethodWithParamTypesAndReturnType(materialRendererInfoClass, "_Init", infoInitTypes, 2, "System.Void");
    if (kEiemEnableMaterialLifecycle) {
      if (!infoInit || !Hook(infoInit, "RendererInfo._Init source isolation", (void *)TraceMaterialInfoInit,
                            &s_origMaterialInfoInit))
        Log("[MOD-MATERIAL-SOURCE] RendererInfo._Init source isolation unavailable");
    } else {
      Log("[VALIDATION] RendererInfo source isolation disabled");
    }
    Log("[RES-TRACE] Material RendererInfo renderer field: 0x%X",
        s_materialRendererInfoRendererOffset);
    HookTraceMethod(materialRendererInfoClass, "TrySetSharedMaterial", 1,
                    "RendererInfo.TrySetSharedMaterial",
                    (void *)TraceRendererInfoTrySetSharedMaterial,
                    &s_origRendererInfoTrySetSharedMaterial);
    HookTraceMethod(materialRendererInfoClass, "TrySetSharedMaterials", 1,
                    "RendererInfo.TrySetSharedMaterials",
                    (void *)TraceRendererInfoTrySetSharedMaterials,
                    &s_origRendererInfoTrySetSharedMaterials);
    HookTraceMethod(materialRendererInfoClass, "TryReplaceSharedMaterials", 1,
                    "RendererInfo.TryReplaceSharedMaterials",
                    (void *)TraceRendererInfoTryReplaceSharedMaterials,
                    &s_origRendererInfoTryReplaceSharedMaterials);
  } else {
    Log("[RES-TRACE] Endfield material RendererInfo not found; material lifecycle hook disabled");
  }

  void *smrClass = FindClass("UnityEngine", "SkinnedMeshRenderer", assemblies,
                            assemblyCount);
  EiemInitShapeGuard(FindClass("Beyond.Gameplay.Core", "SkeletalMorphCore",
                              assemblies, assemblyCount));
  HookTraceMethod(smrClass, "set_sharedMesh", 1,
                  "SkinnedMeshRenderer.set_sharedMesh",
                  (void *)TraceSkinnedMeshSetSharedMesh,
                  &s_origSkinnedMeshSetSharedMesh);
  HookTraceMethod(smrClass, "set_bones", 1,
                  "SkinnedMeshRenderer.set_bones",
                  (void *)TraceSkinnedMeshSetBones,
                  &s_origSkinnedMeshSetBones);
  if (kEiemEnableSkinTimingProbe) {
    // Disabled in the normal build. These hooks belong to the old GPU/skin
    // submission probe and must not be installed during ordinary rendering.
    HookTraceMethod(
        smrClass, "RequestCurrentFrameSkinMatrices", 2,
        "SkinnedMeshRenderer.RequestCurrentFrameSkinMatrices",
        (void *)TraceSkinnedMeshRequestCurrentFrameSkinMatrices,
        &s_origSkinnedMeshRequestCurrentFrameSkinMatrices);
    HookTraceMethod(
        smrClass, "SkinMatricesRequestFinished", 0,
        "SkinnedMeshRenderer.SkinMatricesRequestFinished",
        (void *)TraceSkinnedMeshSkinMatricesRequestFinished,
        &s_origSkinnedMeshSkinMatricesRequestFinished);
    HookTraceMethod(
        smrClass, "GetVertexBuffer", 0,
        "SkinnedMeshRenderer.GetVertexBuffer",
        (void *)TraceSkinnedMeshGetVertexBuffer,
        &s_origSkinnedMeshGetVertexBuffer);
    HookTraceMethod(
        smrClass, "GetPreviousVertexBuffer", 0,
        "SkinnedMeshRenderer.GetPreviousVertexBuffer",
        (void *)TraceSkinnedMeshGetPreviousVertexBuffer,
        &s_origSkinnedMeshGetPreviousVertexBuffer);
  }

  void *prefabInstantiateClass = FindClass(
      "Beyond.Resource.Runtime", "PrefabInstantiateProxy", assemblies,
      assemblyCount);
  if (prefabInstantiateClass) {
    s_prefabInstantiateGetGameObject = FindMethodWithReturnType(
        prefabInstantiateClass, "get_gameObject", "UnityEngine.GameObject",
        0);
    s_prefabInstantiateGetLogName = FindMethodWithReturnType(
        prefabInstantiateClass, "GetLogName", "System.String", 0);
    s_prefabInstantiateGetInstanceUid = FindMethodWithReturnType(
        prefabInstantiateClass, "get_instanceUid", "System.UInt32", 0);
    void *completed = FindMethodWithReturnType(
        prefabInstantiateClass, "OnCompleted", "System.Void", 0);
    if (completed &&
        Hook(completed, "PrefabInstantiateProxy.OnCompleted",
             (void *)TracePrefabInstantiateCompleted,
             &s_origPrefabInstantiateCompleted)) {
      Log("[RES-TRACE] PrefabInstantiateProxy lifecycle anchor installed; gameObject=%p logName=%p instanceUid=%p",
          s_prefabInstantiateGetGameObject, s_prefabInstantiateGetLogName,
          s_prefabInstantiateGetInstanceUid);
    } else {
      Log("[RES-TRACE] PrefabInstantiateProxy.OnCompleted hook failed/not found");
    }
    HookTraceMethod(prefabInstantiateClass, "Unload", 0,
                    "PrefabInstantiateProxy.Unload",
                    (void *)TracePrefabInstantiateUnload,
                    &s_origPrefabInstantiateUnload);
    HookTraceMethod(prefabInstantiateClass, "Clear", 0,
                    "PrefabInstantiateProxy.Clear",
                    (void *)TracePrefabInstantiateClear,
                    &s_origPrefabInstantiateClear);
    HookTraceMethod(prefabInstantiateClass, "Dispose", 0,
                    "PrefabInstantiateProxy.Dispose",
                    (void *)TracePrefabInstantiateDispose,
                    &s_origPrefabInstantiateDispose);
  } else {
    Log("[RES-TRACE] Beyond.Resource.Runtime.PrefabInstantiateProxy class not found");
  }

  void *uiModelLoaderClass = FindClass(
      "Beyond.UI", "UIModelLoader", assemblies, assemblyCount);
  if (uiModelLoaderClass) {
    static const char *const uiLoadTypes[] = {
        "System.String", "UnityEngine.Transform"};
    void *loadModel = FindMethodWithParamTypesAndReturnType(
        uiModelLoaderClass, "LoadModel", uiLoadTypes, 2,
        "UnityEngine.GameObject");
    if (loadModel &&
        Hook(loadModel, "UIModelLoader.LoadModel",
             (void *)TraceUIModelLoaderLoadModel,
             &s_origUIModelLoaderLoadModel))
      Log("[RES-TRACE] UIModelLoader synchronous lifecycle adapter installed");
    else
      Log("[RES-TRACE] UIModelLoader.LoadModel hook failed/not found");

    static const char *const uiLoadAsyncTypes[] = {
        "System.String", "UnityEngine.Transform",
        "System.Action<UnityEngine.GameObject>"};
    void *loadModelAsync = FindMethodWithParamTypesAndReturnType(
        uiModelLoaderClass, "LoadModelAsync", uiLoadAsyncTypes, 3,
        "System.Int32");
    if (loadModelAsync &&
        Hook(loadModelAsync, "UIModelLoader.LoadModelAsync",
             (void *)TraceUIModelLoaderLoadModelAsync,
             &s_origUIModelLoaderLoadModelAsync))
      Log("[RES-TRACE] UIModelLoader async passthrough installed (game callback unchanged)");
    else
      Log("[RES-TRACE] UIModelLoader.LoadModelAsync hook failed/not found");

    static const char *const uiUnloadTypes[] = {"UnityEngine.GameObject"};
    HookTraceMethodWithParamTypes(
        uiModelLoaderClass, "UnloadModel", uiUnloadTypes, 1,
        "UIModelLoader.UnloadModel", (void *)TraceUIModelLoaderUnloadModel,
        &s_origUIModelLoaderUnloadModel);
    HookTraceMethod(uiModelLoaderClass, "_Clear", 0,
                    "UIModelLoader._Clear",
                    (void *)TraceUIModelLoaderClear,
                    &s_origUIModelLoaderClear);
    HookTraceMethod(uiModelLoaderClass, "Dispose", 0,
                    "UIModelLoader.Dispose",
                    (void *)TraceUIModelLoaderDispose,
                    &s_origUIModelLoaderDispose);
  } else {
    Log("[RES-TRACE] Beyond.UI.UIModelLoader class not found");
  }

  // Both UIModelLoader and CharUIModelMono ran in the v29 crash trace. Observe
  // the completed model's own lifecycle without replacing managed callbacks.
  // SetVisible also covers known models reactivated from a persistent pool.
  void *charUIModelClass = FindClass(
      "Beyond.Gameplay.View", "CharUIModelMono", assemblies, assemblyCount);
  if (charUIModelClass) {
    HookTraceMethod(charUIModelClass, "OnAwake", 0,
                    "CharUIModelMono.OnAwake",
                    (void *)TraceCharUIModelOnAwake,
                    &s_origCharUIModelOnAwake);
    static const char *const visibleTypes[] = {"System.Boolean"};
    HookTraceMethodWithParamTypes(
        charUIModelClass, "SetVisible", visibleTypes, 1,
        "CharUIModelMono.SetVisible", (void *)TraceCharUIModelSetVisible,
        &s_origCharUIModelSetVisible);
    HookTraceMethod(charUIModelClass, "OnRelease", 0,
                    "CharUIModelMono.OnRelease",
                    (void *)TraceCharUIModelOnRelease,
                    &s_origCharUIModelOnRelease);
  } else {
    Log("[RES-TRACE] Beyond.Gameplay.View.CharUIModelMono class not found");
  }

  void *meshFilterClass =
      FindClass("UnityEngine", "MeshFilter", assemblies, assemblyCount);
  HookTraceMethod(meshFilterClass, "set_sharedMesh", 1,
                  "MeshFilter.set_sharedMesh",
                  (void *)TraceMeshFilterSetSharedMesh,
                  &s_origMeshFilterSetSharedMesh);

  // Generic character/model lifecycle. BaseModelViewPart is also the explicit
  // completion owner for cached/handle-reused character models.
  void *modelManagerClass = FindClass("Beyond.Gameplay.View", "ModelManager",
                                      assemblies, assemblyCount);
  if (modelManagerClass) {
    static const char *const modelGameObjectType[] = {
        "UnityEngine.GameObject"};
    HookTraceMethodWithParamTypes(
        modelManagerClass, "_OnGameObjectAllocate", modelGameObjectType, 1,
        "ModelManager._OnGameObjectAllocate",
        (void *)TraceModelManagerGameObjectAllocate,
        &s_origModelManagerGameObjectAllocate);

    static const char *const modelPathHashType[] = {
        "Beyond.Resource.StringPathHash"};
    void *loadPersistent = FindMethodWithParamTypesAndReturnType(
        modelManagerClass, "LoadFromPersistentPool", modelPathHashType, 1,
        "UnityEngine.GameObject");
    if (loadPersistent &&
        Hook(loadPersistent, "ModelManager.LoadFromPersistentPool",
             (void *)TraceModelManagerLoadFromPersistentPool,
             &s_origModelManagerLoadFromPersistentPool))
      Log("[RES-TRACE] ModelManager.LoadFromPersistentPool replacement hook installed");
    else
      Log("[RES-TRACE] ModelManager.LoadFromPersistentPool hook failed/not found");
  } else {
    Log("[RES-TRACE] Beyond.Gameplay.View.ModelManager class not found");
  }

  void *basePartClass = FindClass("Beyond.Gameplay.View", "BaseModelViewPart",
                                  assemblies, assemblyCount);
  if (basePartClass) {
    const char *partModelFields[] = {"m_model"};
    const char *partConfigFields[] = {"m_cfg"};
    const char *partRenderersFields[] = {"m_renderers"};
    const char *partRenderersInitStateFields[] = {
        "m_renderersInitState"};
    const char *partHgRenderersFields[] = {"m_hgRenderers"};
    const char *partHgRenderersInitStateFields[] = {
        "m_hgRenderersInitState"};
    const char *partMeshesFields[] = {"m_meshes"};
    const char *partMeshesInitStateFields[] = {"m_meshesInitState"};
    const char *partBoneClothsFields[] = {"m_boneCloths"};
    const char *partLodGroupsFields[] = {"m_lodGroups"};
    s_basePartModelOffset = FindFieldInHierarchy(
        basePartClass, partModelFields, _countof(partModelFields), nullptr);
    s_basePartConfigOffset = FindFieldInHierarchy(
        basePartClass, partConfigFields, _countof(partConfigFields), nullptr);
    s_basePartRenderersOffset = FindFieldInHierarchy(
        basePartClass, partRenderersFields, _countof(partRenderersFields),
        nullptr);
    s_basePartRenderersInitStateOffset = FindFieldInHierarchy(
        basePartClass, partRenderersInitStateFields,
        _countof(partRenderersInitStateFields), nullptr);
    s_basePartHgRenderersOffset = FindFieldInHierarchy(
        basePartClass, partHgRenderersFields,
        _countof(partHgRenderersFields), nullptr);
    s_basePartHgRenderersInitStateOffset = FindFieldInHierarchy(
        basePartClass, partHgRenderersInitStateFields,
        _countof(partHgRenderersInitStateFields), nullptr);
    s_basePartMeshesOffset = FindFieldInHierarchy(
        basePartClass, partMeshesFields, _countof(partMeshesFields), nullptr);
    s_basePartMeshesInitStateOffset = FindFieldInHierarchy(
        basePartClass, partMeshesInitStateFields,
        _countof(partMeshesInitStateFields), nullptr);
    s_basePartBoneClothsOffset = FindFieldInHierarchy(
        basePartClass, partBoneClothsFields,
        _countof(partBoneClothsFields), nullptr);
    s_basePartLodGroupsOffset = FindFieldInHierarchy(
        basePartClass, partLodGroupsFields, _countof(partLodGroupsFields),
        nullptr);
    void *partDataClass = FindClass("Beyond.Gameplay.View",
                                    "BaseModelViewPartData", assemblies,
                                    assemblyCount);
    if (partDataClass) {
      const char *partPathFields[] = {"modelPath"};
      s_basePartConfigPathOffset = FindFieldInHierarchy(
          partDataClass, partPathFields, _countof(partPathFields), nullptr);
    }
    Log("[RES-TRACE] BaseModelViewPart fields: model=0x%X cfg=0x%X "
        "cfg.modelPath=0x%X renderers=0x%X rendererStates=0x%X "
        "hgRenderers=0x%X hgStates=0x%X meshes=0x%X meshStates=0x%X "
        "boneCloths=0x%X lodGroups=0x%X",
        s_basePartModelOffset, s_basePartConfigOffset,
        s_basePartConfigPathOffset, s_basePartRenderersOffset,
        s_basePartRenderersInitStateOffset, s_basePartHgRenderersOffset,
        s_basePartHgRenderersInitStateOffset, s_basePartMeshesOffset,
        s_basePartMeshesInitStateOffset, s_basePartBoneClothsOffset,
        s_basePartLodGroupsOffset);
    HookTraceMethod(basePartClass, "OnLoadFinish", 1,
                    "BaseModelViewPart.OnLoadFinish",
                    (void *)TraceBasePartFinish, &s_origBasePartFinish);
    HookTraceMethod(basePartClass, "PostDealLoadedModel", 0,
                    "BaseModelViewPart.PostDealLoadedModel",
                    (void *)TraceBasePartPostDeal, &s_origBasePartPostDeal);

    static const char *const loadFinishTypes[] = {
        "System.Int32", "Beyond.Resource.StringPathHash",
        "UnityEngine.GameObject"};
    void *loadFinishCallback = FindMethodWithParamTypes(
        basePartClass, "_OnLoadModelFinishCallback", loadFinishTypes,
        _countof(loadFinishTypes));
    if (loadFinishCallback &&
        Hook(loadFinishCallback, "BaseModelViewPart._OnLoadModelFinishCallback",
             (void *)TraceBasePartLoadFinishCallback,
             &s_origBasePartLoadFinishCallback))
      Log("[RES-TRACE] BaseModelViewPart._OnLoadModelFinishCallback path hook installed");
    else
      Log("[RES-TRACE] BaseModelViewPart._OnLoadModelFinishCallback hook failed/not found");

    void *loadFinishResult = FindMethodWithParamTypesAndReturnType(
        basePartClass, "_OnLoadModelFinish", loadFinishTypes,
        _countof(loadFinishTypes), "System.Boolean");
    if (loadFinishResult &&
        Hook(loadFinishResult, "BaseModelViewPart._OnLoadModelFinish",
             (void *)TraceBasePartLoadFinishResult,
             &s_origBasePartLoadFinishResult))
      Log("[RES-TRACE] BaseModelViewPart._OnLoadModelFinish path hook installed");
    else
      Log("[RES-TRACE] BaseModelViewPart._OnLoadModelFinish hook failed/not found");

    static const char *const loadUseHandleTypes[] = {
        "System.Boolean", "Beyond.Resource.FAssetProxyHandle"};
    void *loadUseHandleCallback = FindMethodWithParamTypesAndReturnType(
        basePartClass, "_OnLoadUseHandleFinishCallback", loadUseHandleTypes,
        _countof(loadUseHandleTypes), "System.Void");
    if (loadUseHandleCallback &&
        Hook(loadUseHandleCallback,
             "BaseModelViewPart._OnLoadUseHandleFinishCallback",
             (void *)TraceBasePartLoadUseHandleFinishCallback,
             &s_origBasePartLoadUseHandleFinishCallback))
      Log("[RES-TRACE] BaseModelViewPart._OnLoadUseHandleFinishCallback replacement hook installed");
    else
      Log("[RES-TRACE] BaseModelViewPart._OnLoadUseHandleFinishCallback hook failed/not found");

    void *loadUseHandleResult = FindMethodWithParamTypesAndReturnType(
        basePartClass, "_OnLoadUseHandleFinish", loadUseHandleTypes,
        _countof(loadUseHandleTypes), "System.Boolean");
    if (loadUseHandleResult &&
        Hook(loadUseHandleResult, "BaseModelViewPart._OnLoadUseHandleFinish",
             (void *)TraceBasePartLoadUseHandleFinish,
             &s_origBasePartLoadUseHandleFinishResult))
      Log("[RES-TRACE] BaseModelViewPart._OnLoadUseHandleFinish replacement hook installed");
    else
      Log("[RES-TRACE] BaseModelViewPart._OnLoadUseHandleFinish hook failed/not found");

    HookTraceMethod(basePartClass, "ReleaseModel", 0,
                    "BaseModelViewPart.ReleaseModel",
                    (void *)TraceBasePartReleaseModel,
                    &s_origBasePartReleaseModel);
    HookTraceMethod(basePartClass, "OnRelease", 0,
                    "BaseModelViewPart.OnRelease",
                    (void *)TraceBasePartOnRelease,
                    &s_origBasePartOnRelease);

    void *complexPartClass = FindClass("Beyond.Gameplay.View",
                                       "ComplexModelViewPart", assemblies,
                                       assemblyCount);
    if (complexPartClass) {
      HookTraceMethod(complexPartClass, "PostDealLoadedModel", 0,
                      "ComplexModelViewPart.PostDealLoadedModel",
                      (void *)TraceComplexPartPostDeal,
                      &s_origComplexPartPostDeal);
    } else {
      Log("[RES-TRACE] Beyond.Gameplay.View.ComplexModelViewPart class not found");
    }
  } else {
    Log("[RES-TRACE] Beyond.Gameplay.View.BaseModelViewPart class not found");
  }

  void *npcAvatarCreatorUtils = FindClass(
      "Beyond.NPC.Avatar", "NPCAvatarCreatorUtils", assemblies,
      assemblyCount);

  HookTraceMethod(npcAvatarCreatorUtils, "CreateSMSGO", 12,
                  "NPCAvatarCreatorUtils.CreateSMSGO",
                  (void *)TraceCreateSmsGo, &s_origCreateSmsGo);
  HookTraceMethod(npcAvatarCreatorUtils, "CreateSMSInfoForPostModel", 9,
                  "NPCAvatarCreatorUtils.CreateSMSInfoForPostModel",
                  (void *)TraceCreateSmsPost, &s_origCreateSmsPost);
  HookTraceMethod(
      npcAvatarCreatorUtils, "<CreateMeshAssetsGo>g__AssignSkin|14_0", 4,
      "NPCAvatarCreatorUtils.CreateMeshAssetsGo.AssignSkin",
      (void *)TraceAssignSkinGo, &s_origAssignSkinGo);
  HookTraceMethod(
      npcAvatarCreatorUtils,
      "<CreateMeshAssetsInfoForPostModel>g__AssignSkin|15_0", 4,
      "NPCAvatarCreatorUtils.CreateMeshAssetsInfoForPostModel.AssignSkin",
      (void *)TraceAssignSkinPost, &s_origAssignSkinPost);
  HookTraceMethod(npcAvatarCreatorUtils, "SetSMRRootBone", 3,
                  "NPCAvatarCreatorUtils.SetSMRRootBone",
                  (void *)TraceSetSmrRootBone, &s_origSetSmrRootBone);

  // Descriptor diagnostics are intentionally installable without enabling
  // the legacy high-volume identity probe. The upstream build also installs
  // only the Mesh getter/setter pair so SubMeshInfo consumers receive the
  // same resource-level replacement as proxy consumers.
  if (kEiemEnableDescriptorDiagnostics &&
      !kEiemValidationIdentityProbe) {
    void *subMeshInfoClass =
        FindClass("Beyond.NPC.Avatar", "SubMeshInfo", assemblies,
                 assemblyCount);
    if (subMeshInfoClass) {
      HookTraceMethod(subMeshInfoClass, "get_mesh", 0,
                      "SubMeshInfo.get_mesh",
                      (void *)TraceV11DescriptorGetMesh,
                      &s_origSubMeshInfoGetMesh);
      HookTraceMethod(subMeshInfoClass, "set_mesh", 1,
                      "SubMeshInfo.set_mesh",
                      (void *)TraceV11DescriptorSetMesh,
                      &s_origSubMeshInfoSetMesh);
    } else {
      Log("[V1.1] SubMeshInfo class not found");
    }

    void *lodMeshAssetsClass = FindClass(
        "Beyond.NPC.Avatar", "NPCAvatarLodMeshAssets", assemblies,
        assemblyCount);
    if (lodMeshAssetsClass) {
      static const char *const getSubMeshInfoTypes[] = {
          "Beyond.NPC.Lod.ELODLevel", "System.Boolean"};
      HookTraceMethodWithParamTypes(
          lodMeshAssetsClass, "GetSubMeshInfo", getSubMeshInfoTypes, 2,
          "NPCAvatarLodMeshAssets.GetSubMeshInfo",
          (void *)TraceLodMeshAssetsGetSubMeshInfo,
          &s_origLodMeshAssetsGetSubMeshInfo);
    } else {
      Log("[V1.1] NPCAvatarLodMeshAssets class not found");
    }
    Log("[RES-TRACE] Descriptor/upstream Mesh hooks ready diagnostics=%d upstream=%d",
        kEiemEnableDescriptorDiagnostics ? 1 : 0,
        0);
  }

  if (kEiemValidationIdentityProbe) {
    void *bundleClass =
        FindClass("Beyond.Resource.Runtime", "Bundle", assemblies,
                  assemblyCount);
  HookTraceMethod(bundleClass, "_LoadAssetBundle", 1,
                  "Beyond.Resource.Runtime.Bundle._LoadAssetBundle",
                  (void *)TraceBundleLoadAssetBundle,
                  &s_origBundleLoadAssetBundle);
  HookTraceMethod(bundleClass, "_LoadAssetBundleAsync", 1,
                  "Beyond.Resource.Runtime.Bundle._LoadAssetBundleAsync",
                  (void *)TraceBundleLoadAssetBundleAsync,
                  &s_origBundleLoadAssetBundleAsync);
  HookTraceMethod(bundleClass, "_GetBundleFileFullPath", 1,
                  "Beyond.Resource.Runtime.Bundle._GetBundleFileFullPath",
                  (void *)TraceBundleGetFullPath, &s_origBundleGetFullPath);
  HookTraceMethod(bundleClass, "_FinishWithBundle", 1,
                  "Beyond.Resource.Runtime.Bundle._FinishWithBundle",
                  (void *)TraceBundleFinishWithBundle,
                  &s_origBundleFinishWithBundle);
  HookTraceMethod(bundleClass, "OnEndUnload", 0,
                  "Beyond.Resource.Runtime.Bundle.OnEndUnload",
                  (void *)TraceBundleOnEndUnload, &s_origBundleOnEndUnload);

  void *vfsClass =
      FindClass("Beyond.VFS", "VirtualFileSystem", assemblies, assemblyCount);
  if (vfsClass) {
    static const char *const vfsStringType[] = {"System.String"};
    HookTraceMethodWithParamTypes(
        vfsClass, "LoadBundleFromFile", vfsStringType, 1,
        "VFS.VirtualFileSystem.LoadBundleFromFile(string)",
        (void *)TraceVfsLoadBundleFromFile, &s_origVfsLoadBundleFromFile);
    HookTraceMethodWithParamTypes(
        vfsClass, "LoadBundleFromFileAsync", vfsStringType, 1,
        "VFS.VirtualFileSystem.LoadBundleFromFileAsync(string)",
        (void *)TraceVfsLoadBundleFromFileAsync,
        &s_origVfsLoadBundleFromFileAsync);
    static const char *const vfsBundlePosTypes[] = {
        "System.String", "Beyond.VFS.EFileLoaderPosType&", "System.UInt32"};
    HookTraceMethodWithParamTypes(
        vfsClass, "LoadBundleFromFile", vfsBundlePosTypes, 3,
        "VFS.VirtualFileSystem.LoadBundleFromFile(string,pos,crc)",
        (void *)TraceVfsLoadBundleFromFilePos,
        &s_origVfsLoadBundleFromFilePos);
    HookTraceMethodWithParamTypes(
        vfsClass, "LoadBundleFromFileAsync", vfsBundlePosTypes, 3,
        "VFS.VirtualFileSystem.LoadBundleFromFileAsync(string,pos,crc)",
        (void *)TraceVfsLoadBundleFromFileAsyncPos,
        &s_origVfsLoadBundleFromFileAsyncPos);
    HookTraceMethodWithParamTypes(
        vfsClass, "GetAssetStream", vfsStringType, 1,
        "VFS.VirtualFileSystem.GetAssetStream(string)",
        (void *)TraceVfsGetAssetStream, &s_origVfsGetAssetStream);
    static const char *const vfsHashType[] = {"Beyond.Resource.StringPathHash"};
    HookTraceMethodWithParamTypes(
        vfsClass, "GetAssetStream", vfsHashType, 1,
        "VFS.VirtualFileSystem.GetAssetStream(StringPathHash)",
        (void *)TraceVfsGetAssetStreamHash, &s_origVfsGetAssetStreamHash);
  } else {
    Log("[RES-TRACE] VFS.VirtualFileSystem class not found");
  }

  void *pathHashBinaryClass =
      FindClass("Beyond.Resource", "StringPathHashBinary", assemblies,
                assemblyCount);
  static const char *const hashMappingTypes[] = {"System.Int64",
                                                  "System.String&"};
  HookTraceMethodWithParamTypes(
      pathHashBinaryClass, "GetMappingStrByHash", hashMappingTypes, 2,
      "StringPathHashBinary.GetMappingStrByHash(hash,out string)",
      (void *)TraceStringPathHashGetMapping, &s_origStringPathHashGetMapping);

  void *vfsStreamClass =
      FindClass("Beyond.VFS", "VFSFileReadStream", assemblies, assemblyCount);
  static const char *const vfsReadTypes[] = {"System.Byte[]", "System.Int32",
                                               "System.Int32"};
  HookTraceMethodWithParamTypes(
      vfsStreamClass, "Read", vfsReadTypes, 3,
      "VFS.VFSFileReadStream.Read(byte[],int,int)", (void *)TraceVfsFileRead,
      &s_origVfsFileRead);
  static const char *const vfsSpanReadTypes[] = {"System.Span<System.Byte>"};
  HookTraceMethodWithParamTypes(
      vfsStreamClass, "Read", vfsSpanReadTypes, 1,
      "VFS.VFSFileReadStream.Read(Span<byte>)", (void *)TraceVfsFileReadSpan,
      &s_origVfsFileReadSpan);

  void *resourceManagerClass =
      FindClass("Beyond.Resource.Runtime", "BundleResourceManager",
               assemblies, assemblyCount);
  // Cache StringPathHash.get_path for the hash-based loader boundary.  The
  // overload receives only Int64 in the native detour, so this getter is the
  // authoritative way to recover the same logical path used by the game.
  void *stringPathHashClass =
      FindClass("Beyond.Resource", "StringPathHash", assemblies,
                assemblyCount);
  s_stringPathHashGetPath = FindMethodWithReturnType(
      stringPathHashClass, "get_path", "System.String", 0);
  if (s_stringPathHashGetPath)
    Log("[RES-TRACE] StringPathHash.get_path resolver ready");
  else
    Log("[RES-TRACE] StringPathHash.get_path resolver unavailable");
  static const char *const loadStringTypes[] = {
      "System.String", "System.Type", "Beyond.Resource.RootCategory",
      "System.Boolean", "Beyond.Resource.EResourceRequestPriority"};
  static const char *const loadSubStringTypes[] = {
      "System.String", "System.String", "System.Type",
      "Beyond.Resource.RootCategory", "System.Boolean",
      "Beyond.Resource.EResourceRequestPriority"};
  static const char *const loadHashTypes[] = {
      "Beyond.Resource.StringPathHash", "System.Type",
      "Beyond.Resource.RootCategory", "System.Boolean",
      "Beyond.Resource.EResourceRequestPriority"};
  static const char *const loadSubHashTypes[] = {
      "Beyond.Resource.StringPathHash", "System.String", "System.Type",
      "Beyond.Resource.RootCategory", "System.Boolean",
      "Beyond.Resource.EResourceRequestPriority"};
  HookTraceMethodWithParamTypes(
      resourceManagerClass, "_LoadAssetInternal", loadStringTypes, 5,
      "BundleResourceManager._LoadAssetInternal(string)",
      (void *)TraceResourceLoadAssetInternal, &s_origResourceLoadAssetInternal);
  HookTraceMethodWithParamTypes(
      resourceManagerClass, "_LoadSubAssetInternal", loadSubStringTypes, 6,
      "BundleResourceManager._LoadSubAssetInternal(string)",
      (void *)TraceResourceLoadSubAssetInternal,
      &s_origResourceLoadSubAssetInternal);
  HookTraceMethodWithParamTypes(
      resourceManagerClass, "_LoadAssetInternal", loadHashTypes, 5,
      "BundleResourceManager._LoadAssetInternal(hash)",
      (void *)TraceResourceLoadAssetInternalHash,
      &s_origResourceLoadAssetInternalHash);
  HookTraceMethodWithParamTypes(
      resourceManagerClass, "_LoadSubAssetInternal", loadSubHashTypes, 6,
      "BundleResourceManager._LoadSubAssetInternal(hash)",
      (void *)TraceResourceLoadSubAssetInternalHash,
      &s_origResourceLoadSubAssetInternalHash);

  static const char *const loadAsyncStringCallbackTypes[] = {
      "Beyond.ELogChannel", "System.String", "System.Type",
      "Beyond.Resource.RootCategory",
      "System.Action<System.Boolean,Beyond.Resource.FAssetProxyHandle>",
      "Beyond.Resource.EResourceRequestPriority"};
  static const char *const loadSubAssetAsyncStringCallbackTypes[] = {
      "Beyond.ELogChannel", "System.String", "System.String", "System.Type",
      "Beyond.Resource.RootCategory",
      "System.Action<System.Boolean,Beyond.Resource.FAssetProxyHandle>",
      "Beyond.Resource.EResourceRequestPriority"};
  static const char *const loadAsyncHashCallbackTypes[] = {
      "Beyond.ELogChannel", "Beyond.Resource.StringPathHash", "System.Type",
      "Beyond.Resource.RootCategory",
      "System.Action<System.Boolean,Beyond.Resource.FAssetProxyHandle>",
      "Beyond.Resource.EResourceRequestPriority"};
  static const char *const loadSubAssetAsyncHashCallbackTypes[] = {
      "Beyond.ELogChannel", "Beyond.Resource.StringPathHash", "System.String",
      "System.Type", "Beyond.Resource.RootCategory",
      "System.Action<System.Boolean,Beyond.Resource.FAssetProxyHandle>",
      "Beyond.Resource.EResourceRequestPriority"};
  HookTraceMethodWithParamTypes(
      resourceManagerClass, "LoadAsync", loadAsyncStringCallbackTypes, 6,
      "BundleResourceManager.LoadAsync(string callback)",
      (void *)TraceResourceLoadAsyncString, &s_origResourceLoadAsyncString);
  HookTraceMethodWithParamTypes(
      resourceManagerClass, "LoadSubAssetAsync",
      loadSubAssetAsyncStringCallbackTypes, 7,
      "BundleResourceManager.LoadSubAssetAsync(string callback)",
      (void *)TraceResourceLoadSubAssetAsyncString,
      &s_origResourceLoadSubAssetAsyncString);
  HookTraceMethodWithParamTypes(
      resourceManagerClass, "LoadAsync", loadAsyncHashCallbackTypes, 6,
      "BundleResourceManager.LoadAsync(hash callback)",
      (void *)TraceResourceLoadAsyncHash, &s_origResourceLoadAsyncHash);
  HookTraceMethodWithParamTypes(
      resourceManagerClass, "LoadSubAssetAsync",
      loadSubAssetAsyncHashCallbackTypes, 7,
      "BundleResourceManager.LoadSubAssetAsync(hash callback)",
      (void *)TraceResourceLoadSubAssetAsyncHash,
      &s_origResourceLoadSubAssetAsyncHash);

  static const char *const assetLoaderAsyncHashCallbackTypes[] = {
      "Beyond.Resource.StringPathHash", "System.Type",
      "System.Action<System.Boolean,Beyond.Resource.FAssetProxyLoaderHandle>",
      "Beyond.Resource.EResourceRequestPriority"};
  void *simpleAssetLoaderClass =
      FindClass("Beyond.Resource", "SimpleAssetLoader", assemblies,
                assemblyCount);
  HookTraceMethodWithParamTypes(
      simpleAssetLoaderClass, "LoadAsync", assetLoaderAsyncHashCallbackTypes,
      4, "SimpleAssetLoader.LoadAsync(hash callback)",
      (void *)TraceSimpleAssetLoaderLoadAsync,
      &s_origSimpleAssetLoaderLoadAsync);
  static const char *const assetLoaderTryLoadHashTypes[] = {
      "Beyond.Resource.StringPathHash", "System.Type",
      "Beyond.Resource.FAssetProxyLoaderHandle&"};
  HookTraceMethodWithParamTypesAndReturnType(
      simpleAssetLoaderClass, "TryLoad", assetLoaderTryLoadHashTypes, 3,
      "SimpleAssetLoader.TryLoad(hash,type,out)", "System.Boolean",
      (void *)TraceSimpleAssetLoaderTryLoad, &s_origSimpleAssetLoaderTryLoad);
  void *monoEntitySimpleAssetLoaderClass =
      FindClass("Beyond.Resource", "MonoEntitySimpleAssetLoader", assemblies,
                assemblyCount);
  HookTraceMethodWithParamTypes(
      monoEntitySimpleAssetLoaderClass, "LoadAsync",
      assetLoaderAsyncHashCallbackTypes, 4,
      "MonoEntitySimpleAssetLoader.LoadAsync(hash callback)",
      (void *)TraceMonoEntitySimpleAssetLoaderLoadAsync,
      &s_origMonoEntitySimpleAssetLoaderLoadAsync);
  HookTraceMethodWithParamTypesAndReturnType(
      monoEntitySimpleAssetLoaderClass, "TryLoad",
      assetLoaderTryLoadHashTypes, 3,
      "MonoEntitySimpleAssetLoader.TryLoad(hash,type,out)", "System.Boolean",
      (void *)TraceMonoEntitySimpleAssetLoaderTryLoad,
      &s_origMonoEntitySimpleAssetLoaderTryLoad);

  void *cachedPathAssetLoaderClass =
      FindClass("Beyond.Resource", "CachedPathAssetLoader", assemblies,
                assemblyCount);
  static const char *const cachedLoadDirectTypes[] = {
      "System.String", "System.Type"};
  HookTraceMethodWithParamTypesAndReturnType(
      cachedPathAssetLoaderClass, "LoadDirect", cachedLoadDirectTypes, 2,
      "CachedPathAssetLoader.LoadDirect(string,type)",
      "UnityEngine.Object", (void *)TraceCachedPathAssetLoaderLoadDirect,
      &s_origCachedPathAssetLoaderLoadDirect);
  static const char *const cachedTryLoadTypes[] = {
      "System.String", "System.Type",
      "Beyond.Resource.FAssetProxyLoaderHandle&"};
  HookTraceMethodWithParamTypesAndReturnType(
      cachedPathAssetLoaderClass, "TryLoad", cachedTryLoadTypes, 3,
      "CachedPathAssetLoader.TryLoad(string,type,out)", "System.Boolean",
      (void *)TraceCachedPathAssetLoaderTryLoad,
      &s_origCachedPathAssetLoaderTryLoad);

  void *hashProcessorClass =
      FindClass("Beyond.Resource", "HashStringPathProcessor", assemblies,
                assemblyCount);
  static const char *const stringType[] = {"System.String"};
  HookTraceMethodWithParamTypes(
      hashProcessorClass, "GetABStringPathHash", stringType, 1,
      "HashStringPathProcessor.GetABStringPathHash",
      (void *)TraceGetAssetPathHash, &s_origGetAssetPathHash);
  HookTraceMethodWithParamTypes(
      hashProcessorClass, "GetABStringPathHashWithoutBurst", stringType, 1,
      "HashStringPathProcessor.GetABStringPathHashWithoutBurst",
      (void *)TraceGetAssetPathHashWithoutBurst,
      &s_origGetAssetPathHashWithoutBurst);

  void *preloadManagerClass =
      FindClass("Beyond.Resource.Runtime", "PreloadManager", assemblies,
                assemblyCount);
  static const char *const int64Type[] = {"System.Int64"};
  HookTraceMethodWithParamTypes(
      preloadManagerClass, "PreloadAuto", int64Type, 1,
      "PreloadManager.PreloadAuto(hash)", (void *)TracePreloadAutoHash,
      &s_origPreloadAutoHash);

  void *proxyHandleClass =
      FindClass("Beyond.Resource", "FAssetProxyHandle", assemblies,
                assemblyCount);
  if (proxyHandleClass) {
    void *pathMethod = FindMethodWithReturnType(
        proxyHandleClass, "get_pathOrName", "System.String", 0);
    if (pathMethod && Hook(pathMethod, "FAssetProxyHandle.get_pathOrName",
                           (void *)TraceAssetProxyHandlePath,
                           &s_origAssetProxyHandlePath))
      Log("[RES-TRACE] FAssetProxyHandle.get_pathOrName observation hook installed");

    void *getMethod = FindMethodWithReturnType(
        proxyHandleClass, "Get", "UnityEngine.Object", 0);
    if (getMethod && Hook(getMethod, "FAssetProxyHandle.Get",
                          (void *)TraceAssetProxyHandleGet,
                          &s_origAssetProxyHandleGet))
      Log("[RES-TRACE] FAssetProxyHandle.Get observation hook installed");

    void *proxyMethod = FindMethodWithReturnType(
        proxyHandleClass, "GetAssetProxy", "Beyond.Resource.IAssetProxy", 0);
    if (proxyMethod && Hook(proxyMethod, "FAssetProxyHandle.GetAssetProxy",
                            (void *)TraceAssetProxyHandleGetAssetProxy,
                            &s_origAssetProxyHandleGetAssetProxy))
      Log("[RES-TRACE] FAssetProxyHandle.GetAssetProxy observation hook installed");
  } else {
    Log("[RES-TRACE] FAssetProxyHandle not found");
  }

  void *proxyLoaderHandleClass =
      FindClass("Beyond.Resource", "FAssetProxyLoaderHandle", assemblies,
                assemblyCount);
  if (proxyLoaderHandleClass) {
    void *pathMethod = FindMethodWithReturnType(
        proxyLoaderHandleClass, "get_pathOrName", "System.String", 0);
    if (pathMethod && Hook(pathMethod, "FAssetProxyLoaderHandle.get_pathOrName",
                           (void *)TraceAssetProxyLoaderHandlePath,
                           &s_origAssetProxyLoaderHandlePath))
      Log("[V1.1] FAssetProxyLoaderHandle.get_pathOrName observation hook installed");

    void *getMethod = FindMethodWithReturnType(
        proxyLoaderHandleClass, "Get", "UnityEngine.Object", 0);
    if (getMethod && Hook(getMethod, "FAssetProxyLoaderHandle.Get",
                          (void *)TraceAssetProxyLoaderHandleGet,
                          &s_origAssetProxyLoaderHandleGet))
      Log("[V1.1] FAssetProxyLoaderHandle.Get observation hook installed");

    void *loadImmediate = FindMethodWithReturnType(
        proxyLoaderHandleClass, "LoadImmediate", "System.Void", 0);
    if (loadImmediate && Hook(
            loadImmediate, "FAssetProxyLoaderHandle.LoadImmediate",
            (void *)TraceAssetProxyLoaderHandleLoadImmediate,
            &s_origAssetProxyLoaderHandleLoadImmediate))
      Log("[V1.1] FAssetProxyLoaderHandle.LoadImmediate observation hook installed");

    static const char *const addCompletedTypes[] = {
        "Beyond.ELogChannel",
        "System.Action<System.Boolean,Beyond.Resource.FAssetProxyUntrackedHandle>"};
    HookTraceMethodWithParamTypes(
        proxyLoaderHandleClass, "AddOnProxyCompleted", addCompletedTypes, 2,
        "FAssetProxyLoaderHandle.AddOnProxyCompleted",
        (void *)TraceAssetProxyLoaderHandleAddOnProxyCompleted,
        &s_origAssetProxyLoaderHandleAddOnProxyCompleted);
  } else {
    Log("[V1.1] FAssetProxyLoaderHandle not found");
  }

  void *untrackedClass =
      FindClass("Beyond.Resource", "FAssetProxyUntrackedHandle", assemblies,
                assemblyCount);
  if (untrackedClass) {
    s_origAssetProxyUntrackedPath = FindMethodWithReturnType(
        untrackedClass, "get_pathOrName", "System.String", 0);
    void *getMethod = FindMethodWithReturnType(
        untrackedClass, "Get", "UnityEngine.Object", 0);
    Log("[RES-TRACE] FAssetProxyUntrackedHandle path=%p get=%p",
        s_origAssetProxyUntrackedPath, getMethod);
    if (getMethod && Hook(getMethod, "FAssetProxyUntrackedHandle.Get",
                          (void *)TraceAssetProxyUntrackedGet,
                          &s_origAssetProxyUntrackedGet))
      Log("[RES-TRACE] FAssetProxyUntrackedHandle.Get observation hook installed");
    s_assetProxyUntrackedGetAssetProxy = FindMethodWithReturnType(
        untrackedClass, "get_assetProxy", "Beyond.Resource.IAssetProxy", 0);
  } else {
    Log("[RES-TRACE] FAssetProxyUntrackedHandle not found");
  }

  void *assetClass = FindClass("Beyond.Resource.Runtime", "Asset", assemblies,
                               assemblyCount);
  HookTraceMethod(assetClass, "get_assetName", 0,
                  "Beyond.Resource.Runtime.Asset.get_assetName",
                  (void *)TraceAssetGetAssetName, &s_origAssetGetAssetName);
  HookTraceMethod(assetClass, "_FinishWithAsset", 1,
                  "Beyond.Resource.Runtime.Asset._FinishWithAsset",
                  (void *)TraceAssetFinishWithAsset,
                  &s_origAssetFinishWithAsset);
  HookTraceMethod(assetClass, "OnComplete", 0,
                  "Beyond.Resource.Runtime.Asset.OnComplete",
                  (void *)TraceAssetOnComplete, &s_origAssetOnComplete);

  void *subMeshInfoClass =
      FindClass("Beyond.NPC.Avatar", "SubMeshInfo", assemblies, assemblyCount);
  if (subMeshInfoClass) {
    HookTraceMethod(subMeshInfoClass, "get_mesh", 0,
                    "SubMeshInfo.get_mesh",
                    (void *)TraceV11DescriptorGetMesh,
                    &s_origSubMeshInfoGetMesh);
    HookTraceMethod(subMeshInfoClass, "set_mesh", 1,
                    "SubMeshInfo.set_mesh",
                    (void *)TraceV11DescriptorSetMesh,
                    &s_origSubMeshInfoSetMesh);
  } else {
    Log("[V1.1] SubMeshInfo class not found");
  }

  void *lodMeshAssetsClass = FindClass(
      "Beyond.NPC.Avatar", "NPCAvatarLodMeshAssets", assemblies,
      assemblyCount);
  if (lodMeshAssetsClass) {
    static const char *const getSubMeshInfoTypes[] = {
        "Beyond.NPC.Lod.ELODLevel", "System.Boolean"};
    HookTraceMethodWithParamTypes(
        lodMeshAssetsClass, "GetSubMeshInfo", getSubMeshInfoTypes, 2,
        "NPCAvatarLodMeshAssets.GetSubMeshInfo",
        (void *)TraceLodMeshAssetsGetSubMeshInfo,
        &s_origLodMeshAssetsGetSubMeshInfo);
  } else {
    Log("[V1.1] NPCAvatarLodMeshAssets class not found");
  }

  void *meshAssetsClass = FindClass(
      "Beyond.NPC.Avatar", "NPCAvatarMeshAssetsSO", assemblies,
      assemblyCount);
  if (meshAssetsClass) {
    HookTraceMethod(
        meshAssetsClass, "GetAvatarSlotMeshAssets", 0,
        "NPCAvatarMeshAssetsSO.GetAvatarSlotMeshAssets",
        (void *)TraceMeshAssetsGetAvatarSlotMeshAssets,
        &s_origMeshAssetsGetAvatarSlotMeshAssets);
    HookTraceMethod(
        meshAssetsClass, "GetAllAvatarSlotMeshAssets", 0,
        "NPCAvatarMeshAssetsSO.GetAllAvatarSlotMeshAssets",
        (void *)TraceMeshAssetsGetAllAvatarSlotMeshAssets,
        &s_origMeshAssetsGetAllAvatarSlotMeshAssets);
  } else {
    Log("[V1.1] NPCAvatarMeshAssetsSO class not found");
  }

    Log("[RES-TRACE] Identity observation hooks ready");
  } else {
    Log("[RES-TRACE] Identity observation hooks disabled");
  }
}
