#pragma once

// Read-only probe for the game's visibility controller.  This is intentionally
// separate from the stable replacement path: it records the controller calls
// and the tracked Renderer state before/after the original method, but never
// changes a Renderer, Mesh, material or GPU resource.

struct EiemVisibilityControllerProbeState {
  bool tracked = false;
  bool enabled = true;
  bool visible = false;
  bool forceOff = false;
  bool active = true;
  // Mesh replacement and material replacement are independent.  A mesh-only
  // binding must still receive the per-draw mesh reassert/restore, but it must
  // never enter the VFX material clone/fanout path.
  bool materialTarget = false;
  char section[96] = {};
  uintptr_t owner = 0;
  void *replacementMesh = nullptr;
};

static volatile LONG s_eiemVisibilityControllerProbeCalls = 0;

// The per-draw hook only needs the ledger facts that identify the replacement
// target.  Reading Renderer.enabled/isVisible/forceRenderingOff and the active
// GameObject state invokes Unity getters on every draw, so keep that runtime
// snapshot separate from the cheap binding snapshot.
static EiemVisibilityControllerProbeState
EiemReadVisibilityControllerProbeLedger(void *renderer) {
  EiemVisibilityControllerProbeState state;
  if (!renderer) return state;

  AcquireSRWLockShared(&s_eiemOverrideLock);
  const size_t index = EiemFindOverrideLocked(renderer);
  if (index != SIZE_MAX && !s_eiemOverrides[index].restorePending) {
    state.tracked = true;
    state.materialTarget = s_eiemOverrides[index].hasMaterialRule;
    state.owner = s_eiemOverrides[index].ownerPrefabInstance;
    state.replacementMesh = s_eiemOverrides[index].replacementMesh;
    strncpy_s(state.section, sizeof(state.section),
              s_eiemOverrides[index].renderSection, _TRUNCATE);
  }
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  return state;
}

static EiemVisibilityControllerProbeState
EiemReadVisibilityControllerProbeState(void *renderer) {
  auto state = EiemReadVisibilityControllerProbeLedger(renderer);
  if (!state.tracked) return state;

  EiemReadRendererEnabled(renderer, &state.enabled);
  EiemReadRendererVisible(renderer, &state.visible);
  EiemReadRendererForceRenderingOff(renderer, &state.forceOff);
  if (g_component_get_gameObject && g_gameObject_get_activeInHierarchy) {
    void *gameObject = Invoke(g_component_get_gameObject, renderer);
    EiemReadBoxedBool(g_gameObject_get_activeInHierarchy, gameObject,
                      &state.active);
  }
  return state;
}

static EiemVisibilityControllerProbeState
EiemReadVisibilityControllerProbeStateFast(void *renderer) {
  return EiemReadVisibilityControllerProbeLedger(renderer);
}

static void EiemLogVisibilityControllerRenderer(
    const char *operation, const char *phase, void *controller,
    void *renderer, int requested, bool hasRequested) {
  if (!renderer ||
      InterlockedIncrement(&s_eiemVisibilityControllerProbeCalls) > 320)
    return;
  const auto state = EiemReadVisibilityControllerProbeState(renderer);
  if (!state.tracked) return;
  Log("[VIS-CONTROLLER] op=%s phase=%s controller=%p renderer=%p "
      "owner=%p section=%s replacementMesh=%p requested=%s "
      "enabled=%d visible=%d forceRenderingOff=%d active=%d",
      operation ? operation : "?", phase ? phase : "?", controller,
      renderer, (void *)state.owner,
      state.section[0] ? state.section : "<unknown>", state.replacementMesh,
      hasRequested ? (requested ? "true" : "false") : "<reset>",
      state.enabled ? 1 : 0, state.visible ? 1 : 0,
      state.forceOff ? 1 : 0, state.active ? 1 : 0);
}

using EiemSetVisibleByRendererFn = void (__fastcall *)(
    void *self, void *renderer, bool visible, void *methodInfo);
using EiemSetVisibleByNameFn = void (__fastcall *)(
    void *self, void *nameContains, bool visible, void *methodInfo);
using EiemResetVisibleByRendererFn = void (__fastcall *)(
    void *self, void *renderer, void *methodInfo);
using EiemResetVisibleByNameFn = void (__fastcall *)(
    void *self, void *nameContains, void *methodInfo);

static void *s_origVisibilitySetByRenderer = nullptr;
static void *s_origVisibilitySetByName = nullptr;
static void *s_origVisibilityResetByRenderer = nullptr;
static void *s_origVisibilityResetByName = nullptr;

static void TraceVisibilitySetByRenderer(void *self, void *renderer,
                                         bool visible, void *methodInfo) {
  EiemLogVisibilityControllerRenderer("SetVisibleByRenderer", "before", self,
                                      renderer, visible, true);
  auto original = (EiemSetVisibleByRendererFn)s_origVisibilitySetByRenderer;
  if (original) original(self, renderer, visible, methodInfo);
  EiemLogVisibilityControllerRenderer("SetVisibleByRenderer", "after", self,
                                      renderer, visible, true);
}

static void TraceVisibilitySetByName(void *self, void *nameContains,
                                     bool visible, void *methodInfo) {
  char name[192] = {};
  if (nameContains) ReadStrUtf8(nameContains, name, sizeof(name));
  Log("[VIS-CONTROLLER-NAME] op=SetVisibleByNameContainsStr phase=before "
      "controller=%p contains=%s requested=%d",
      self, name[0] ? name : "<empty>", visible ? 1 : 0);
  auto original = (EiemSetVisibleByNameFn)s_origVisibilitySetByName;
  if (original) original(self, nameContains, visible, methodInfo);
  Log("[VIS-CONTROLLER-NAME] op=SetVisibleByNameContainsStr phase=after "
      "controller=%p contains=%s requested=%d",
      self, name[0] ? name : "<empty>", visible ? 1 : 0);
}

static void TraceVisibilityResetByRenderer(void *self, void *renderer,
                                            void *methodInfo) {
  EiemLogVisibilityControllerRenderer("ResetVisibleByRenderer", "before", self,
                                      renderer, 0, false);
  auto original =
      (EiemResetVisibleByRendererFn)s_origVisibilityResetByRenderer;
  if (original) original(self, renderer, methodInfo);
  EiemLogVisibilityControllerRenderer("ResetVisibleByRenderer", "after", self,
                                      renderer, 0, false);
}

static void TraceVisibilityResetByName(void *self, void *nameContains,
                                       void *methodInfo) {
  char name[192] = {};
  if (nameContains) ReadStrUtf8(nameContains, name, sizeof(name));
  Log("[VIS-CONTROLLER-NAME] op=ResetVisibleByNameContainsStr phase=before "
      "controller=%p contains=%s",
      self, name[0] ? name : "<empty>");
  auto original = (EiemResetVisibleByNameFn)s_origVisibilityResetByName;
  if (original) original(self, nameContains, methodInfo);
  Log("[VIS-CONTROLLER-NAME] op=ResetVisibleByNameContainsStr phase=after "
      "controller=%p contains=%s",
      self, name[0] ? name : "<empty>");
}

// The public VisibleController methods above are only a façade. The actual
// per-renderer state lives in its nested RendererInfo (oriVisible/curVisible).
// Sprint can update that record directly, leaving Renderer.enabled untouched.
// Observe the nested methods without changing state.
using EiemVisibleInfoSetFn = bool (__fastcall *)(void *self, bool visible,
                                                  void *methodInfo);
using EiemVisibleInfoResetFn = bool (__fastcall *)(void *self,
                                                    void *methodInfo);
static void *s_origVisibleInfoTrySetVisible = nullptr;
static void *s_origVisibleInfoTrySetVisibleByRenderer = nullptr;
static void *s_origVisibleInfoTryResetVisible = nullptr;

static bool EiemReadVisibleInfoFlags(void *info, bool *originalVisible,
                                     bool *currentVisible) {
  if (!info) return false;
  __try {
    if (originalVisible) *originalVisible = *(bool *)((char *)info + 0x20);
    if (currentVisible) *currentVisible = *(bool *)((char *)info + 0x21);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

// Forward declaration: the shared RendererInfo reader is defined below the
// property-block probe, but the nested visibility probe uses it first.
static void *EiemRendererInfoRenderer(void *info);

static void EiemLogVisibleInfoCall(const char *operation, const char *phase,
                                   void *info, int requested,
                                   bool hasRequested, bool resultKnown,
                                   bool result) {
  if (!info) return;
  void *renderer = EiemRendererInfoRenderer(info);
  const auto state = EiemReadVisibilityControllerProbeState(renderer);
  char rendererName[160] = {};
  char hierarchy[768] = {};
  if (renderer) {
    TraceReadUnityObjectName(renderer, rendererName, sizeof(rendererName));
    TraceBuildRendererHierarchy(renderer, hierarchy, sizeof(hierarchy));
  }
  if (!state.tracked) return;
  bool originalVisible = false;
  bool currentVisible = false;
  const bool flagsRead =
      EiemReadVisibleInfoFlags(info, &originalVisible, &currentVisible);
  Log("[VIS-INFO-PROBE] op=%s phase=%s info=%p renderer=%p tracked=%d "
      "section=%s requested=%s result=%s oriVisible=%s curVisible=%s "
      "flagsRead=%d enabled=%d visible=%d forceRenderingOff=%d active=%d "
      "rendererName=%s hierarchy=%s",
      operation ? operation : "?", phase ? phase : "?", info, renderer,
      state.tracked ? 1 : 0, state.section[0] ? state.section : "<none>",
      hasRequested ? (requested ? "true" : "false") : "<reset>",
      resultKnown ? (result ? "true" : "false") : "<none>",
      flagsRead ? (originalVisible ? "true" : "false") : "<unreadable>",
      flagsRead ? (currentVisible ? "true" : "false") : "<unreadable>",
      flagsRead ? 1 : 0, state.enabled ? 1 : 0, state.visible ? 1 : 0,
      state.forceOff ? 1 : 0, state.active ? 1 : 0,
      rendererName[0] ? rendererName : "<none>",
      hierarchy[0] ? hierarchy : "<none>");
}

static bool TraceVisibleInfoTrySetVisible(void *self, bool visible,
                                          void *methodInfo) {
  EiemLogVisibleInfoCall("RendererInfo.TrySetVisible", "before", self,
                         visible ? 1 : 0, true, false, false);
  auto original = (EiemVisibleInfoSetFn)s_origVisibleInfoTrySetVisible;
  const bool result = original ? original(self, visible, methodInfo) : false;
  EiemLogVisibleInfoCall("RendererInfo.TrySetVisible", "after", self,
                         visible ? 1 : 0, true, true, result);
  return result;
}

static bool TraceVisibleInfoTrySetVisibleByRenderer(void *self, bool visible,
                                                    void *methodInfo) {
  EiemLogVisibleInfoCall("RendererInfo.TrySetVisibleByRenderer", "before",
                         self, visible ? 1 : 0, true, false, false);
  auto original =
      (EiemVisibleInfoSetFn)s_origVisibleInfoTrySetVisibleByRenderer;
  const bool result = original ? original(self, visible, methodInfo) : false;
  EiemLogVisibleInfoCall("RendererInfo.TrySetVisibleByRenderer", "after", self,
                         visible ? 1 : 0, true, true, result);
  return result;
}

static bool TraceVisibleInfoTryResetVisible(void *self, void *methodInfo) {
  EiemLogVisibleInfoCall("RendererInfo.TryResetVisible", "before", self, 0,
                         false, false, false);
  auto original = (EiemVisibleInfoResetFn)s_origVisibleInfoTryResetVisible;
  const bool result = original ? original(self, methodInfo) : false;
  EiemLogVisibleInfoCall("RendererInfo.TryResetVisible", "after", self, 0,
                         false, true, result);
  return result;
}

static void *EiemFindNestedClass(void *outer, const char *name) {
  if (!outer || !name || !il2cpp_class_get_nested_types ||
      !il2cpp_class_get_name)
    return nullptr;
  void *iter = nullptr;
  while (void *nested = il2cpp_class_get_nested_types(outer, &iter)) {
    const char *nestedName = il2cpp_class_get_name(nested);
    if (nestedName && strcmp(nestedName, name) == 0)
      return nested;
  }
  return nullptr;
}

// RendererInfo keeps the native replacement transaction in private fields.
// Enumerate its actual methods once so the end-of-dissolve path can use an
// engine-owned reset/clear operation if one exists, instead of guessing at a
// field offset.  This is metadata-only and does not hook or invoke anything.
static void EiemLogRendererInfoMaterialMethods(void *klass) {
  static volatile LONG s_logged = 0;
  if (!klass || InterlockedCompareExchange(&s_logged, 1, 0) != 0 ||
      !il2cpp_class_get_methods || !il2cpp_method_get_name ||
      !il2cpp_method_get_param_count)
    return;
  void *iterator = nullptr;
  int index = 0;
  while (void *method = il2cpp_class_get_methods(klass, &iterator)) {
    const char *name = il2cpp_method_get_name(method);
    if (!name) continue;
    const bool materialName = strstr(name, "Material") != nullptr ||
                              strstr(name, "Replace") != nullptr ||
                              strstr(name, "Reset") != nullptr ||
                              strstr(name, "Clear") != nullptr;
    if (!materialName) continue;
    const uint32_t count = il2cpp_method_get_param_count(method);
    const char *returnName = "?";
    if (il2cpp_method_get_return_type && il2cpp_type_get_name) {
      void *returnType = il2cpp_method_get_return_type(method);
      const char *resolved = returnType ? il2cpp_type_get_name(returnType) : nullptr;
      if (resolved && resolved[0]) returnName = resolved;
    }
    char params[384] = {};
    size_t used = 0;
    for (uint32_t p = 0; p < count && p < 12; ++p) {
      const char *typeName = "?";
      if (il2cpp_method_get_param && il2cpp_type_get_name) {
        void *type = il2cpp_method_get_param(method, p);
        const char *resolved = type ? il2cpp_type_get_name(type) : nullptr;
        if (resolved && resolved[0]) typeName = resolved;
      }
      const int written = snprintf(params + used, sizeof(params) - used,
                                   "%s%s", used ? "," : "", typeName);
      if (written <= 0 || (size_t)written >= sizeof(params) - used) break;
      used += (size_t)written;
    }
    Log("[VIS-MATERIAL-METHOD] class=%s index=%d name=%s return=%s params=%u(%s)",
        il2cpp_class_get_name ? il2cpp_class_get_name(klass) : "RendererInfo",
        index++, name, returnName, count, params);
  }
}

static void EiemInstallVisibleInfoProbe(void **assemblies,
                                        size_t assemblyCount) {
  if (!kEiemEnableVisibilityControllerProbe) return;
  void *outer = FindClass("Beyond.Rendering",
                          "EntityRenderHelperVisibleController", assemblies,
                          assemblyCount);
  void *klass = EiemFindNestedClass(outer, "RendererInfo");
  if (!klass) {
    Log("[VIS-INFO-PROBE] nested RendererInfo class not found outer=%p nestedApi=%p",
        outer, (void *)il2cpp_class_get_nested_types);
    return;
  }
  EiemLogRendererInfoMaterialMethods(klass);
  const char *visibleTypes[] = {"System.Boolean"};
  HookTraceMethodWithParamTypesAndReturnType(
      klass, "TrySetVisible", visibleTypes, 1,
      "EntityRenderHelperVisibleController.RendererInfo.TrySetVisible",
      "System.Boolean", (void *)TraceVisibleInfoTrySetVisible,
      &s_origVisibleInfoTrySetVisible);
  HookTraceMethodWithParamTypesAndReturnType(
      klass, "TrySetVisibleByRenderer", visibleTypes, 1,
      "EntityRenderHelperVisibleController.RendererInfo.TrySetVisibleByRenderer",
      "System.Boolean", (void *)TraceVisibleInfoTrySetVisibleByRenderer,
      &s_origVisibleInfoTrySetVisibleByRenderer);
  HookTraceMethodWithParamTypesAndReturnType(
      klass, "TryResetVisible", nullptr, 0,
      "EntityRenderHelperVisibleController.RendererInfo.TryResetVisible",
      "System.Boolean", (void *)TraceVisibleInfoTryResetVisible,
      &s_origVisibleInfoTryResetVisible);
  Log("[VIS-INFO-PROBE] nested RendererInfo hooks installed class=%p", klass);
}

static void EiemInstallVisibilityControllerProbe(void **assemblies,
                                                  size_t assemblyCount) {
  if (!kEiemEnableVisibilityControllerProbe) return;
  void *klass = FindClass("Beyond.Rendering",
                          "EntityRenderHelperVisibleController", assemblies,
                          assemblyCount);
  if (!klass) {
    Log("[VIS-CONTROLLER] class not found");
    return;
  }
  const char *rendererBool[] = {"UnityEngine.Renderer", "System.Boolean"};
  const char *rendererOnly[] = {"UnityEngine.Renderer"};
  const char *nameBool[] = {"System.String", "System.Boolean"};
  const char *nameOnly[] = {"System.String"};
  HookTraceMethodWithParamTypesAndReturnType(
      klass, "SetVisibleByRenderer", rendererBool, 2,
      "EntityRenderHelperVisibleController.SetVisibleByRenderer", "System.Void",
      (void *)TraceVisibilitySetByRenderer, &s_origVisibilitySetByRenderer);
  HookTraceMethodWithParamTypesAndReturnType(
      klass, "SetVisibleByNameContainsStr", nameBool, 2,
      "EntityRenderHelperVisibleController.SetVisibleByNameContainsStr",
      "System.Void", (void *)TraceVisibilitySetByName,
      &s_origVisibilitySetByName);
  HookTraceMethodWithParamTypesAndReturnType(
      klass, "ResetVisibleByRenderer", rendererOnly, 1,
      "EntityRenderHelperVisibleController.ResetVisibleByRenderer", "System.Void",
      (void *)TraceVisibilityResetByRenderer,
      &s_origVisibilityResetByRenderer);
  HookTraceMethodWithParamTypesAndReturnType(
      klass, "ResetVisibleByNameContainsStr", nameOnly, 1,
      "EntityRenderHelperVisibleController.ResetVisibleByNameContainsStr",
      "System.Void", (void *)TraceVisibilityResetByName,
      &s_origVisibilityResetByName);
  Log("[VIS-CONTROLLER] probe installed");
  EiemInstallVisibleInfoProbe(assemblies, assemblyCount);
}

// RendererInfo owns the per-renderer dissolve/dither controls.  A Mesh swap
// can leave this controller's state attached to the original render path, so
// observe both calls and their underlying Renderer without changing arguments.
using EiemDitherAlphaFn = bool (__fastcall *)(void *self, float alpha,
                                               void *methodInfo);
using EiemDitherEnableFn = bool (__fastcall *)(void *self, bool enabled,
                                                bool *changed,
                                                void *methodInfo);
// Character dissolve is driven through the material controller's per-draw
// Vector4, rather than the manual dither methods above.  Keep this hook
// observation-only so we can compare the source and replacement renderers.
using EiemCharacterPerDrawDataFn = bool (__fastcall *)(void *self,
                                                       Vector4 value,
                                                       void *methodInfo);
static void *s_origRendererInfoDitherAlpha = nullptr;
static void *s_origRendererInfoDitherEnable = nullptr;

static void *s_origRendererInfoCharacterPerDrawData = nullptr;

// The sprint dissolve is not using RendererInfo's manual dither entry in the
// current run.  Keep a second, read-only probe at the next likely boundary:
// MaterialPropertyBlock/Material scalar writes followed by Renderer
// SetPropertyBlock.  This is diagnostic-only and is intentionally bounded.
struct EiemDissolveFloatWrite {
  void *block = nullptr;
  void *material = nullptr;
  int32_t propertyId = 0;
  float value = 0.0f;
  ULONGLONG tick = 0;
};
static EiemDissolveFloatWrite s_eiemDissolveFloatWrites[256] = {};
static volatile LONG s_eiemDissolveFloatWriteCount = 0;
static void *s_origMaterialPropertyBlockSetFloatImpl = nullptr;
static void *s_origMaterialSetFloatImpl = nullptr;
static void *s_origRendererSetPropertyBlock = nullptr;
static void *s_origRendererSetPropertyBlockIndexed = nullptr;

using EiemSetFloatImplFn = void (__fastcall *)(
    void *self, int32_t propertyId, float value, void *methodInfo);
using EiemRendererSetPropertyBlockFn = void (__fastcall *)(
    void *self, void *block, void *methodInfo);
using EiemRendererSetPropertyBlockIndexedFn = void (__fastcall *)(
    void *self, void *block, int32_t materialIndex, void *methodInfo);

static void EiemRememberDissolveFloat(void *block, void *material,
                                      int32_t propertyId, float value) {
  const LONG index = InterlockedIncrement(&s_eiemDissolveFloatWriteCount) - 1;
  if (index < 0 || index >= (LONG)_countof(s_eiemDissolveFloatWrites)) return;
  s_eiemDissolveFloatWrites[index] = {
      block, material, propertyId, value, GetTickCount64()};
}

static void EiemLogRendererPropertyBlock(const char *operation, void *renderer,
                                          void *block, int32_t materialIndex,
                                          bool indexed) {
  if (!renderer) return;
  const auto state = EiemReadVisibilityControllerProbeState(renderer);
  char name[160] = {};
  char hierarchy[768] = {};
  if (renderer) {
    TraceReadUnityObjectName(renderer, name, sizeof(name));
    TraceBuildRendererHierarchy(renderer, hierarchy, sizeof(hierarchy));
  }
  if (!state.tracked) return;
  Log("[DISSOLVE-PROBE] op=%s renderer=%p tracked=%d section=%s block=%p "
      "indexed=%d materialIndex=%d rendererName=%s hierarchy=%s",
      operation ? operation : "?", renderer, state.tracked ? 1 : 0,
      state.section[0] ? state.section : "<none>", block, indexed ? 1 : 0,
      materialIndex, name[0] ? name : "<none>",
      hierarchy[0] ? hierarchy : "<none>");
  if (!block) return;
  const LONG count = (std::min)(InterlockedCompareExchange(
                                   &s_eiemDissolveFloatWriteCount, 0, 0),
                               (LONG)_countof(s_eiemDissolveFloatWrites));
  int logged = 0;
  for (LONG i = count - 1; i >= 0 && logged < 12; --i) {
    const auto &write = s_eiemDissolveFloatWrites[i];
    if (write.block != block) continue;
    Log("[DISSOLVE-PROBE-FLOAT] renderer=%p block=%p propertyId=%d "
        "value=%.6f tick=%llu",
        renderer, block, write.propertyId, (double)write.value,
        (unsigned long long)write.tick);
    ++logged;
  }
}

static void TraceMaterialPropertyBlockSetFloatImpl(void *self,
                                                    int32_t propertyId,
                                                    float value,
                                                    void *methodInfo) {
  auto original = (EiemSetFloatImplFn)s_origMaterialPropertyBlockSetFloatImpl;
  if (original) original(self, propertyId, value, methodInfo);
  EiemRememberDissolveFloat(self, nullptr, propertyId, value);
}

static void TraceMaterialSetFloatImpl(void *self, int32_t propertyId,
                                      float value, void *methodInfo) {
  auto original = (EiemSetFloatImplFn)s_origMaterialSetFloatImpl;
  if (original) original(self, propertyId, value, methodInfo);
  EiemRememberDissolveFloat(nullptr, self, propertyId, value);
}

static void TraceRendererSetPropertyBlock(void *self, void *block,
                                           void *methodInfo) {
  auto original =
      (EiemRendererSetPropertyBlockFn)s_origRendererSetPropertyBlock;
  if (original) original(self, block, methodInfo);
  EiemLogRendererPropertyBlock("Renderer.SetPropertyBlock", self, block, -1,
                               false);
}

static void TraceRendererSetPropertyBlockIndexed(void *self, void *block,
                                                  int32_t materialIndex,
                                                  void *methodInfo) {
  auto original = (EiemRendererSetPropertyBlockIndexedFn)
      s_origRendererSetPropertyBlockIndexed;
  if (original) original(self, block, materialIndex, methodInfo);
  EiemLogRendererPropertyBlock("Renderer.SetPropertyBlock(indexed)", self,
                               block, materialIndex, true);
}
// Startup initializes hundreds of unrelated renderers with alpha=0 and
// enable=true.  Counting those calls made the old probe exhaust its budget
// before the character was even loaded.  Keep a small budget only for
// untracked, non-zero calls; tracked replacement renderers are always kept.
static volatile LONG s_eiemDitherUntrackedInterestingCalls = 0;

static void *EiemRendererInfoRenderer(void *info) {
  if (!info) return nullptr;
  __try { return *(void **)((char *)info + 0x10); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static void EiemLogDitherCall(const char *operation, const char *phase,
                              void *info, float alpha, int hasAlpha,
                              int requested, bool *changed) {
#if !defined(EIEM_HIGH_FREQUENCY_DIAGNOSTICS_BUILD)
  (void)operation;
  (void)phase;
  (void)info;
  (void)alpha;
  (void)hasAlpha;
  (void)requested;
  (void)changed;
  return;
#else
  void *renderer = EiemRendererInfoRenderer(info);
  EiemVisibilityControllerProbeState state =
      EiemReadVisibilityControllerProbeState(renderer);
  const bool nonZeroAlpha = hasAlpha && std::isfinite(alpha) &&
                            std::fabs(alpha) > 0.000001f;
  const bool disableRequest = requested == 0;
  // Keep the untracked budget bounded without depending on a character name.
  // The production replacement path is tracked by renderer identity; this
  // probe only retains interesting native transitions for comparison.
  char rendererName[160] = {};
  char hierarchy[768] = {};
  char meshName[192] = {};
  if (renderer) {
    TraceReadUnityObjectName(renderer, rendererName, sizeof(rendererName));
    TraceBuildRendererHierarchy(renderer, hierarchy, sizeof(hierarchy));
  }
  // Keep all calls for replacement renderers.  For source/untracked renderers
  // retain only calls that can describe the dissolve transition, so the
  // startup initialization does not consume the useful part of the log.
  if (!state.tracked && !nonZeroAlpha && !disableRequest) return;
  if (!state.tracked &&
      InterlockedIncrement(&s_eiemDitherUntrackedInterestingCalls) > 512)
    return;
  if (!state.tracked && renderer) {
    void *mesh = EiemReadSharedMesh(renderer, "DitherProbe");
    if (mesh) TraceReadUnityObjectName(mesh, meshName, sizeof(meshName));
  }
  Log("[DITHER-PROBE] op=%s phase=%s info=%p renderer=%p tracked=%d "
      "section=%s alpha=%s requested=%s changed=%s enabled=%d visible=%d "
      "forceRenderingOff=%d active=%d rendererName=%s hierarchy=%s mesh=%s",
      operation ? operation : "?", phase ? phase : "?", info, renderer,
      state.tracked ? 1 : 0, state.section[0] ? state.section : "<none>",
      hasAlpha ? (std::isfinite(alpha) ? "finite" : "nonfinite") : "<none>",
      hasAlpha ? "<alpha>" : (requested >= 0 ? (requested ? "true" : "false")
                                             : "<none>"),
      changed ? (*changed ? "true" : "false") : "<none>",
      state.enabled ? 1 : 0, state.visible ? 1 : 0, state.forceOff ? 1 : 0,
      state.active ? 1 : 0, rendererName[0] ? rendererName : "<none>",
      hierarchy[0] ? hierarchy : "<none>", meshName[0] ? meshName : "<none>");
  if (hasAlpha)
    Log("[DITHER-PROBE-VALUE] op=%s info=%p renderer=%p alpha=%.6f",
        operation ? operation : "?", info, renderer, alpha);
#endif
}

static bool TraceRendererInfoDitherAlpha(void *self, float alpha,
                                         void *methodInfo) {
  EiemLogDitherCall("TrySetManualDitherAlphaValue", "before", self, alpha, 1,
                    -1, nullptr);
  auto original = (EiemDitherAlphaFn)s_origRendererInfoDitherAlpha;
  const bool result = original ? original(self, alpha, methodInfo) : false;
  EiemLogDitherCall("TrySetManualDitherAlphaValue", "after", self, alpha, 1,
                    -1, nullptr);
#if defined(EIEM_HIGH_FREQUENCY_DIAGNOSTICS_BUILD)
  Log("[DITHER-PROBE-RESULT] op=TrySetManualDitherAlphaValue info=%p result=%d",
      self, result ? 1 : 0);
#endif
  return result;
}

static bool TraceRendererInfoDitherEnable(void *self, bool enabled,
                                          bool *changed, void *methodInfo) {
  EiemLogDitherCall("TrySetManualDitherEnable", "before", self, 0.0f, 0,
                    enabled ? 1 : 0, changed);
  auto original = (EiemDitherEnableFn)s_origRendererInfoDitherEnable;
  const bool result = original ? original(self, enabled, changed, methodInfo)
                               : false;
  EiemLogDitherCall("TrySetManualDitherEnable", "after", self, 0.0f, 0,
                    enabled ? 1 : 0, changed);
#if defined(EIEM_HIGH_FREQUENCY_DIAGNOSTICS_BUILD)
  Log("[DITHER-PROBE-RESULT] op=TrySetManualDitherEnable info=%p result=%d "
      "changed=%d",
      self, result ? 1 : 0, changed ? (*changed ? 1 : 0) : -1);
#endif
  return result;
}

static bool EiemReadRendererInfoPerDrawData(void *info, Vector4 *value) {
  if (!info || !value) return false;
  __try {
    *value = *(Vector4 *)((char *)info + 0x48);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

struct EiemPerDrawMaterialState {
  void *sourceMaterials = nullptr;
  void *replacingMaterials = nullptr;
  void *currentMaterials = nullptr;
  size_t sourceCount = 0;
  size_t replacingCount = 0;
  size_t currentCount = 0;
  bool materialReplacing = false;
  bool fieldsRead = false;
};

// RendererInfo's material fields were verified against the live metadata dump:
// sourceMaterials=0x30, materialReplacing=0x38, replacingMaterials=0x40.
// This is a read-only diagnostic snapshot.  In particular, it does not infer
// visibility from the managed Renderer; it only tells us which material array
// the game's per-draw controller believes it is using.
static EiemPerDrawMaterialState
EiemReadRendererInfoMaterialState(void *info, void *renderer) {
  EiemPerDrawMaterialState state;
  if (!info) return state;
  __try {
    state.sourceMaterials = *(void **)((char *)info + 0x30);
    state.materialReplacing =
        *(unsigned char *)((char *)info + 0x38) != 0;
    state.replacingMaterials = *(void **)((char *)info + 0x40);
    state.sourceCount = EiemManagedArrayLength(state.sourceMaterials);
    state.replacingCount = EiemManagedArrayLength(state.replacingMaterials);
    if (renderer && g_renderer_get_sharedMaterials) {
      state.currentMaterials = Invoke(g_renderer_get_sharedMaterials, renderer);
      state.currentCount = EiemManagedArrayLength(state.currentMaterials);
    }
    state.fieldsRead = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    state = {};
  }
  return state;
}

#if defined(EIEM_PERDRAW_END_RESTORE_BUILD)
// Endfield's RendererInfo keeps the original material array in a private
// sourceMaterials field.  A replacement Mesh may deliberately have more
// submeshes than that source array.  Unity's public sharedMaterials getter can
// therefore report the complete EFF array while the game's custom draw path
// still submits only the original slot range (which makes only submesh 0
// visible).  Keep the native one-slot source table while the native VFX call
// is running, then expose the complete clean array during the idle draw.
//
// This is per RendererInfo state, never a character/slot hardcode.  The
// expanded managed array is GC-rooted for as long as the private field points
// at it; on the next per-draw call we put the exact original source pointer
// back before invoking game code.
struct EiemPerDrawSourceExpansion {
  void *info = nullptr;
  void *originalSource = nullptr;
  void *expandedSource = nullptr;
  uint32_t expandedHandle = 0;
  bool expanded = false;
};
static EiemPerDrawSourceExpansion s_eiemPerDrawSourceExpansions[256] = {};
static volatile LONG s_eiemPerDrawSourceExpansionCount = 0;

static EiemPerDrawSourceExpansion *EiemFindPerDrawSourceExpansion(
    void *info, bool allocate) {
  if (!info) return nullptr;
  for (auto &entry : s_eiemPerDrawSourceExpansions)
    if (entry.info == info) return &entry;
  if (!allocate) return nullptr;
  const LONG index =
      InterlockedIncrement(&s_eiemPerDrawSourceExpansionCount) - 1;
  if (index < 0 || index >= (LONG)_countof(s_eiemPerDrawSourceExpansions))
    return nullptr;
  s_eiemPerDrawSourceExpansions[index].info = info;
  return &s_eiemPerDrawSourceExpansions[index];
}

static void EiemPerDrawRestoreNativeSource(void *info) {
  auto *entry = EiemFindPerDrawSourceExpansion(info, false);
  if (!entry || !entry->expanded || !entry->originalSource) return;
  bool written = false;
  __try {
    *(void **)((char *)info + 0x30) = entry->originalSource;
    written = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    written = false;
  }
  if (written) entry->expanded = false;
}

static bool EiemPerDrawExpandNativeSource(void *info, void *renderer,
                                          void *expandedSource) {
  if (!info || !renderer || !expandedSource || !il2cpp_gchandle_new)
    return false;
  const size_t expandedCount = EiemManagedArrayLength(expandedSource);
  if (expandedCount <= 1 || expandedCount > 64) return false;
  auto *entry = EiemFindPerDrawSourceExpansion(info, true);
  if (!entry) return false;

  if (!entry->originalSource) {
    __try { entry->originalSource = *(void **)((char *)info + 0x30); }
    __except (EXCEPTION_EXECUTE_HANDLER) { entry->originalSource = nullptr; }
  }
  const size_t originalCount = EiemManagedArrayLength(entry->originalSource);
  if (!entry->originalSource || originalCount >= expandedCount) return false;
  if (entry->expanded && entry->expandedSource == expandedSource) return true;

  if (entry->expandedHandle && il2cpp_gchandle_free) {
    il2cpp_gchandle_free(entry->expandedHandle);
    entry->expandedHandle = 0;
  }
  const uint32_t handle = il2cpp_gchandle_new(expandedSource, false);
  if (!handle) return false;
  bool written = false;
  __try {
    *(void **)((char *)info + 0x30) = expandedSource;
    written = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    written = false;
  }
  if (!written) {
    if (il2cpp_gchandle_free) il2cpp_gchandle_free(handle);
    return false;
  }
  entry->expandedSource = expandedSource;
  entry->expandedHandle = handle;
  entry->expanded = true;
  return true;
}
#endif

#if defined(EIEM_PERDRAW_END_RESTORE_BUILD)
// A VFXInstance is a runtime material object, so it cannot be removed by
// changing the Mesh rule.  At the end of the dissolve the per-draw value
// returns to zero while RendererInfo may still hold the temporary replacement
// array.  Restore the exact LZY material array after the native per-draw call
// has completed.  The native replacement methods are not called from this
// callback; after the public Renderer assignment succeeds, the stale private
// replacement fields are retired directly so they cannot overwrite the clean
// array on the next frame.
static bool EiemPerDrawArrayHasVfxInstance(void *array) {
  if (!array) return false;
  const size_t count = EiemManagedArrayLength(array);
  if (!count || count > 64) return false;
  __try {
    void **items = (void **)((char *)array + IL2CPP_ARRAY_DATA);
    for (size_t index = 0; index < count; ++index) {
      if (!items[index]) continue;
      char name[160] = {};
      TraceReadUnityObjectName(items[index], name, sizeof(name));
      if (strstr(name, "_VFXInstance") != nullptr) return true;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return false;
}

// Record the actual GPU-facing state after the end-of-dash restore.  Material
// array length alone is not sufficient: Unity can retain a valid-sized array
// while one of the Mesh index buffers is empty or one of the material objects
// is already destroyed.  This diagnostic is bounded to the restore edge and
// uses only the renderer's live Mesh/material getters.
static void EiemLogPerDrawSubmeshState(const char *phase, void *renderer,
                                       const char *section) {
  if (!renderer || !g_renderer_get_sharedMaterials) return;
  void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
  int32_t submeshCount = -1;
  std::string indexCounts;
  if (mesh && g_mesh_get_subMeshCount) {
    void *boxed = Invoke(g_mesh_get_subMeshCount, mesh);
    if (boxed) submeshCount = *(int32_t *)((char *)boxed + 16);
  }
  if (mesh && g_mesh_GetIndexCount && submeshCount >= 0 && submeshCount <= 64) {
    for (int32_t index = 0; index < submeshCount; ++index) {
      if (index) indexCounts += ",";
      void *params[] = {&index};
      void *boxed = Invoke(g_mesh_GetIndexCount, mesh, params);
      if (!boxed) {
        indexCounts += "?";
      } else {
        indexCounts += std::to_string(*(int32_t *)((char *)boxed + 16));
      }
    }
  }
  void *materials = Invoke(g_renderer_get_sharedMaterials, renderer);
  const size_t materialCount = EiemManagedArrayLength(materials);
  std::string materialState;
  if (materials && materialCount <= 64) {
    void **items = (void **)((char *)materials + IL2CPP_ARRAY_DATA);
    for (size_t index = 0; index < materialCount; ++index) {
      if (index) materialState += ";";
      char name[96] = {};
      if (items[index])
        TraceReadUnityObjectName(items[index], name, sizeof(name));
      materialState += std::to_string(index);
      materialState += "=";
      materialState += items[index] ? name : "<null>";
      materialState += "/";
      materialState += std::to_string(items[index]
                                          ? EiemNativeObjectStatus(items[index])
                                          : 0);
    }
  }
  char meshName[128] = {};
  if (mesh) TraceReadUnityObjectName(mesh, meshName, sizeof(meshName));
  Log("[MOD-SUBMESH-STATE] phase=%s renderer=%p section=%s mesh=%p:%s "
      "meshSubmeshes=%d indexCounts=[%s] materials=%p/%zu slots=[%s]",
      phase ? phase : "?", renderer, section ? section : "<none>", mesh,
      meshName[0] ? meshName : "<none>", submeshCount,
      indexCounts.empty() ? "<none>" : indexCounts.c_str(), materials,
      materialCount, materialState.empty() ? "<none>" : materialState.c_str());
}

static bool EiemPerDrawRestoreLzyMaterials(void *info, void *renderer,
                                           void *methodInfo) {
  (void)methodInfo;
  if (!info || !renderer || !EiemOnUnityThread()) return false;
  const auto before = EiemReadRendererInfoMaterialState(info, renderer);
  if (!before.fieldsRead ||
      (!before.materialReplacing &&
       !EiemPerDrawArrayHasVfxInstance(before.currentMaterials) &&
       !EiemPerDrawArrayHasVfxInstance(before.replacingMaterials)))
    return false;

  EiemResolvedRenderRule resolved = {};
  if (!EiemFindBoundRenderRule(renderer, &resolved.rule) ||
      (!resolved.rule.materialCount && !resolved.rule.submeshCount))
    return false;

  char error[256] = {};
  void *clean = nullptr;
  bool sourceFallback = false;
  if (resolved.rule.materialCount) {
    if (!EiemBuildRendererMaterials(resolved.rule, &clean, error,
                                     sizeof(error)) || !clean) {
      Log("[MOD-PERDRAW-END-RESTORE] build failed info=%p renderer=%p error=%s",
          info, renderer, error[0] ? error : "unknown");
      return false;
    }
  } else {
    // A mesh-only rule still owns the renderer during the dash.  There is no
    // LZY Material section to build for it, so restoring through the normal
    // builder returns a null array and leaves the native one-slot VFX array
    // installed.  Reuse RendererInfo.sourceMaterials (and extend it only when
    // the replacement mesh has more submeshes) so the original material-slot
    // layout is restored without naming a character or renderer.
    const size_t sourceCount = before.sourceCount;
    size_t requiredCount = sourceCount;
    if (resolved.rule.submeshCount > requiredCount)
      requiredCount = resolved.rule.submeshCount;
    if (!before.sourceMaterials || !requiredCount) {
      Log("[MOD-PERDRAW-END-RESTORE] source fallback unavailable info=%p "
          "renderer=%p source=%p/%zu submeshes=%u",
          info, renderer, before.sourceMaterials, sourceCount,
          resolved.rule.submeshCount);
      return false;
    }
    if (requiredCount == sourceCount) {
      clean = before.sourceMaterials;
    } else if (s_eiemMaterialClass) {
      clean = il2cpp_array_new(s_eiemMaterialClass, requiredCount);
      if (clean) {
        void **items = (void **)((char *)clean + IL2CPP_ARRAY_DATA);
        memcpy(items, (char *)before.sourceMaterials + IL2CPP_ARRAY_DATA,
               sourceCount * sizeof(void *));
        for (size_t slot = sourceCount; slot < requiredCount; ++slot)
          items[slot] = items[0];
      }
    }
    if (!clean) {
      strncpy_s(error, sizeof(error),
                "Unable to build source material fallback", _TRUNCATE);
      Log("[MOD-PERDRAW-END-RESTORE] build failed info=%p renderer=%p error=%s",
          info, renderer, error);
      return false;
    }
    sourceFallback = true;
  }

  // Do not call RendererInfo.TryReplaceSharedMaterials or TrySetSharedMaterials
  // from inside TrySetCharacterPerDrawData.  Those methods are the owner of
  // the same transaction that is currently executing and are not re-entrant;
  // the crash dump showed the sprint-end fault at this exact nested call.
  // Restore the visible Renderer array directly, then retire the private
  // replacement fields below after the native per-draw callback has returned.
  const bool assigned = EiemAssignRendererMaterials(
      renderer, clean, error, sizeof(error));
  // This game build can leave RendererInfo's private replacement transaction
  // armed (materialReplacing=1, replacingMaterials=one VFX slot).  On the
  // next frame that stale transaction can overwrite the restored LZY array.
  // The verified idle representation is a null replacing array and a false
  // flag.  Clear it only after the public assignment has completed.
  bool nativeCleared = false;
  if (assigned) {
    __try {
      const auto pending = EiemReadRendererInfoMaterialState(info, renderer);
      if (pending.materialReplacing || pending.replacingMaterials) {
        *(void **)((char *)info + 0x40) = nullptr;
        *(unsigned char *)((char *)info + 0x38) = 0;
        nativeCleared = true;
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      nativeCleared = false;
    }
  }
  const auto after = EiemReadRendererInfoMaterialState(info, renderer);
  EiemLogPerDrawSubmeshState("end-restore", renderer, resolved.rule.section);
  Log("[MOD-PERDRAW-END-RESTORE] info=%p renderer=%p section=%s "
      "nativeSetterSkipped=1 assigned=%d "
      "nativeCleared=%d sourceFallback=%d cleanCount=%zu currentCount=%zu "
      "replacingCount=%zu materialReplacing=%d error=%s",
      info, renderer, resolved.rule.section,
      assigned ? 1 : 0,
      nativeCleared ? 1 : 0,
      sourceFallback ? 1 : 0, EiemManagedArrayLength(clean), after.currentCount,
      after.replacingCount, after.materialReplacing ? 1 : 0,
      error[0] ? error : "<none>");
  return assigned;
}

// RendererInfo can receive the native source Mesh again when the dash VFX
// transaction creates its temporary draw object.  Re-assert the bound EIEM
// replacement immediately before the native per-draw call.  The lookup is
// entirely based on the already tracked renderer rule; no character or asset
// name is embedded here.
static bool EiemPerDrawEnsureReplacementMesh(
    void *renderer, const EiemVisibilityControllerProbeState &visibility) {
  if (!renderer || !EiemOnUnityThread())
    return false;
  const char *rendererType = EiemIsSkinnedRenderer(renderer)
                                 ? "SkinnedMeshRenderer"
                                 : "MeshFilter";
  void *current = EiemReadSharedMesh(renderer, rendererType);
  // A native afterimage can be a draw-only Renderer clone.  It may never pass
  // through RendererInfo._Init or the sharedMesh setter, so it has no override
  // ledger entry even though its source Mesh is the same as a live bound
  // renderer.  Reuse the source-identity fallback only when all existing
  // bindings for that source agree on one replacement; ambiguous sources stay
  // untouched rather than guessing across models.
  void *replacement = visibility.tracked ? visibility.replacementMesh : nullptr;
  if (!replacement && current)
    replacement = EiemReplacementForSourceMesh(renderer, current);
  if (!replacement) {
    // The attack afterimage is a draw-only renderer and may never receive a
    // ledger entry.  Keep a bounded probe for that branch so we can tell
    // whether the missing body is an unrecognised source Mesh or whether the
    // renderer never reaches this per-draw seam at all.
    if (!visibility.tracked) {
      static volatile LONG s_eiemUntrackedMeshProbeLogs = 0;
      const LONG logIndex = InterlockedIncrement(&s_eiemUntrackedMeshProbeLogs);
      if (logIndex <= 128) {
        char rendererName[160] = {};
        char meshName[192] = {};
        char originPath[768] = {};
        char hierarchy[768] = {};
        int64_t originHash = 0;
        if (renderer)
          TraceReadUnityObjectName(renderer, rendererName,
                                   sizeof(rendererName));
        if (current) {
          TraceReadUnityObjectName(current, meshName, sizeof(meshName));
          TraceLookupAssetOrigin(current, &originHash, originPath,
                                 sizeof(originPath));
        }
        if (renderer)
          TraceBuildRendererHierarchy(renderer, hierarchy, sizeof(hierarchy));
        Log("[MOD-PERDRAW-UNTRACKED-MESH] renderer=%p type=%s mesh=%p "
            "meshName=%s originHash=%lld originPath=%s enabled=%d visible=%d "
            "forceOff=%d active=%d rendererName=%s hierarchy=%s",
            renderer, rendererType, current, meshName[0] ? meshName : "<none>",
            (long long)originHash,
            originPath[0] ? originPath : "<none>", visibility.enabled ? 1 : 0,
            visibility.visible ? 1 : 0, visibility.forceOff ? 1 : 0,
            visibility.active ? 1 : 0,
            rendererName[0] ? rendererName : "<none>",
            hierarchy[0] ? hierarchy : "<none>");
      }
    }
    return false;
  }
  if (current == replacement) {
    static volatile LONG s_eiemMeshReassertObserved = 0;
    const LONG observed = InterlockedIncrement(&s_eiemMeshReassertObserved);
    if (observed <= 24)
      Log("[MOD-PERDRAW-MESH-CHECK] renderer=%p tracked=%d type=%s "
          "current=%p replacement=%p same=1",
          renderer, visibility.tracked ? 1 : 0, rendererType, current,
          replacement);
    return true;
  }
  const bool assigned = EiemSetSharedMesh(
      renderer, replacement, rendererType, nullptr);
  void *after = EiemReadSharedMesh(renderer, rendererType);
  if (visibility.tracked) {
    Log("[MOD-PERDRAW-MESH-REASSERT] renderer=%p tracked=1 type=%s "
        "before=%p replacement=%p assigned=%d after=%p",
        renderer, rendererType, current, replacement, assigned ? 1 : 0,
        after);
  } else {
    static volatile LONG s_eiemMeshFallbackLogs = 0;
    const LONG logIndex = InterlockedIncrement(&s_eiemMeshFallbackLogs);
    if (logIndex <= 64)
      Log("[MOD-PERDRAW-MESH-FALLBACK] renderer=%p tracked=0 type=%s "
          "before=%p replacement=%p assigned=%d after=%p",
          renderer, rendererType, current, replacement, assigned ? 1 : 0,
          after);
  }
  return assigned && after == replacement;
}
#endif

#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD)
// Native sprint replacement requests are usually a one-element array even
// when the live Renderer has several Mesh submesh slots.  Build a runtime VFX
// clone for each LZY slot: the clone keeps the native shader/effect parameters,
// while texture properties are copied from the matched LZY material.
static void *EiemVfxArrayItem(void *array, size_t index) {
  if (!array || index >= EiemManagedArrayLength(array)) return nullptr;
  __try {
    return *(void **)((char *)array + IL2CPP_ARRAY_DATA +
                      index * sizeof(void *));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

static bool EiemMaterialLooksLikeVfx(void *material) {
  if (!material) return false;
  struct VfxNameCacheEntry {
    void *material;
    LONG generation;
    bool isVfx;
  };
  static VfxNameCacheEntry s_cache[128] = {};
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  const size_t cacheIndex = (reinterpret_cast<uintptr_t>(material) >> 4) &
                            (_countof(s_cache) - 1);
  if (s_cache[cacheIndex].material == material &&
      s_cache[cacheIndex].generation == generation)
    return s_cache[cacheIndex].isVfx;
  char name[192] = {};
  TraceReadUnityObjectName(material, name, sizeof(name));
  const bool isVfx = strstr(name, "_VFXInstance") != nullptr;
  s_cache[cacheIndex] = {material, generation, isVfx};
  return isVfx;
}

// Native RendererInfo keeps the same VFXInstance alive while its per-draw
// value changes.  Texture/property overlay is static for that instance, so
// remember which targets were already patched and avoid repeating Unity
// material calls on every frame of a dash.
struct EiemVfxPatchSeenEntry {
  void *info = nullptr;
  void *target = nullptr;
  LONG generation = 0;
};
static EiemVfxPatchSeenEntry s_eiemVfxPatchSeen[256] = {};
// A native material commit can synchronously re-enter the per-draw hook.  Do
// not release the cache that the outer VFX patch is reading until that patch
// has returned; otherwise its vector references become use-after-clear.
static thread_local LONG s_eiemVfxPatchDepth = 0;
static void EiemFlushDeferredPerDrawVfxSlotCaches();

struct EiemVfxPatchGuard {
  EiemVfxPatchGuard() { ++s_eiemVfxPatchDepth; }
  ~EiemVfxPatchGuard() {
    if (--s_eiemVfxPatchDepth == 0)
      EiemFlushDeferredPerDrawVfxSlotCaches();
  }
};
#if !defined(EIEM_PERDRAW_END_RESTORE_BUILD)
static void EiemFlushDeferredPerDrawVfxSlotCaches() {}
#endif

static bool EiemVfxPatchWasSeen(void *info, void *target) {
  if (!info || !target) return false;
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  for (const auto &entry : s_eiemVfxPatchSeen)
    if (entry.info == info && entry.target == target &&
        entry.generation == generation)
      return true;
  return false;
}

static void EiemRememberVfxPatch(void *info, void *target) {
  if (!info || !target) return;
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  for (auto &entry : s_eiemVfxPatchSeen) {
    if (entry.info == info && entry.target == target) {
      entry.generation = generation;
      return;
    }
  }
  for (auto &entry : s_eiemVfxPatchSeen) {
    if (!entry.info) {
      entry = {info, target, generation};
      return;
    }
  }
  // The table is only a bounded hot-path guard. Reuse an old slot if a game
  // scene creates more short-lived RendererInfo objects than the table holds.
  s_eiemVfxPatchSeen[(reinterpret_cast<uintptr_t>(info) >> 4) &
                     (_countof(s_eiemVfxPatchSeen) - 1)] =
      {info, target, generation};
}

static bool EiemVfxPatchHasUnseenTarget(void *info,
                                        const EiemPerDrawMaterialState &state,
                                        bool *hasVfxOut = nullptr) {
  void *arrays[] = {state.replacingMaterials, state.currentMaterials};
  bool hasVfx = false;
  void *seen[64] = {};
  size_t seenCount = 0;
  for (void *array : arrays) {
    const size_t count = EiemManagedArrayLength(array);
    if (!array || !count || count > 64) continue;
    for (size_t index = 0; index < count; ++index) {
      void *target = EiemVfxArrayItem(array, index);
      if (!target) continue;
      // A stable native VFXInstance is the common case.  Once it has been
      // patched, avoid asking Unity for its object name on every per-draw
      // callback just to rediscover the same classification.
      if (!EiemVfxPatchWasSeen(info, target) &&
          !EiemMaterialLooksLikeVfx(target))
        continue;
      hasVfx = true;
      bool duplicate = false;
      for (size_t i = 0; i < seenCount; ++i)
        if (seen[i] == target) {
          duplicate = true;
          break;
      }
      if (!duplicate && seenCount < _countof(seen)) seen[seenCount++] = target;
      if (!EiemVfxPatchWasSeen(info, target)) {
        if (hasVfxOut) *hasVfxOut = true;
        return true;
      }
    }
  }
  if (hasVfxOut) *hasVfxOut = hasVfx;
  return false;
}

// Mesh-to-rule resolution is the other expensive part of the draw-only VFX
// path.  A cloned RendererInfo can be different every frame, while its source
// Mesh and the active mod generation stay stable for the whole attack/dash.
// Cache successful resolutions by Mesh so the hot path does not repeatedly
// call TraceDescribeObject or scan the override ledger.
struct EiemVfxRenderRuleCacheEntry {
  void *mesh = nullptr;
  LONG generation = 0;
  EiemResolvedRenderRule resolved = {};
};
static SRWLOCK s_eiemVfxRenderRuleCacheLock = SRWLOCK_INIT;
static EiemVfxRenderRuleCacheEntry s_eiemVfxRenderRuleCache[128];

static bool EiemLookupVfxRenderRuleCache(void *mesh,
                                         EiemResolvedRenderRule *out) {
  if (!mesh || !out) return false;
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  const size_t start = (reinterpret_cast<uintptr_t>(mesh) >> 4) & 127u;
  AcquireSRWLockShared(&s_eiemVfxRenderRuleCacheLock);
  for (size_t step = 0; step < 8; ++step) {
    const auto &entry = s_eiemVfxRenderRuleCache[(start + step) & 127u];
    if (!entry.mesh) break;
    if (entry.mesh == mesh && entry.generation == generation) {
      *out = entry.resolved;
      ReleaseSRWLockShared(&s_eiemVfxRenderRuleCacheLock);
      return true;
    }
  }
  ReleaseSRWLockShared(&s_eiemVfxRenderRuleCacheLock);
  return false;
}

static void EiemStoreVfxRenderRuleCache(void *mesh,
                                        const EiemResolvedRenderRule &resolved) {
  if (!mesh) return;
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  const size_t start = (reinterpret_cast<uintptr_t>(mesh) >> 4) & 127u;
  AcquireSRWLockExclusive(&s_eiemVfxRenderRuleCacheLock);
  size_t slot = start;
  for (size_t step = 0; step < 8; ++step) {
    const size_t candidate = (start + step) & 127u;
    auto &entry = s_eiemVfxRenderRuleCache[candidate];
    if (!entry.mesh || entry.mesh == mesh || entry.generation != generation) {
      slot = candidate;
      break;
    }
  }
  s_eiemVfxRenderRuleCache[slot] = {mesh, generation, resolved};
  ReleaseSRWLockExclusive(&s_eiemVfxRenderRuleCacheLock);
}

// RendererInfo creates short-lived afterimage renderers without running the
// normal Renderer lifecycle hooks.  Resolve the LZY Render rule in the same
// order as the normal path: exact renderer binding, Mesh binding, then a
// standalone source-Mesh match.  The material-slot predicate is evaluated
// separately after native has committed its runtime arrays.
static bool EiemResolveVfxRenderRule(void *renderer,
                                     EiemResolvedRenderRule *out) {
  if (!renderer || !out) return false;
  *out = {};

  if (EiemFindBoundRenderRule(renderer, &out->rule)) {
    strncpy_s(out->source, sizeof(out->source), "renderer-binding",
              _TRUNCATE);
    return out->rule.materialCount || out->rule.submeshCount;
  }

  const char *rendererType = EiemIsSkinnedRenderer(renderer)
                                 ? "SkinnedMeshRenderer"
                                 : "MeshFilter";
  void *mesh = EiemReadSharedMesh(renderer, rendererType);
  if (!mesh) return false;

  if (EiemLookupVfxRenderRuleCache(mesh, out))
    return out->rule.materialCount || out->rule.submeshCount;

  if (EiemFindRenderRuleForMeshBinding(mesh, &out->rule)) {
    strncpy_s(out->source, sizeof(out->source), "mesh-binding", _TRUNCATE);
    EiemStoreVfxRenderRuleCache(mesh, *out);
    return out->rule.materialCount || out->rule.submeshCount;
  }

  char source[768] = {};
  char asset[192] = {};
  if (!EiemReadLiveMeshIdentity(mesh, source, sizeof(source), asset,
                                sizeof(asset)))
    return false;
  std::vector<EiemModRule> standaloneRules;
  EiemFindStandaloneRenderRules(&standaloneRules);
  for (const auto &candidate : standaloneRules) {
    if (candidate.path[0] ||
        !EiemRenderRuleMatches(candidate, "", mesh, asset))
      continue;
    out->rule = candidate;
    strncpy_s(out->source, sizeof(out->source),
              source[0] ? source : "standalone", _TRUNCATE);
    strncpy_s(out->asset, sizeof(out->asset), asset, _TRUNCATE);
    EiemStoreVfxRenderRuleCache(mesh, *out);
    return out->rule.materialCount || out->rule.submeshCount;
  }
  return false;
}

static void *EiemFindVfxTemplate(void *materials, bool inputIsArray) {
  if (!materials) return nullptr;
  if (!inputIsArray) return EiemMaterialLooksLikeVfx(materials) ? materials : nullptr;
  const size_t count = EiemManagedArrayLength(materials);
  if (!count || count > 64) return nullptr;
  for (size_t index = 0; index < count; ++index) {
    void *item = EiemVfxArrayItem(materials, index);
    if (EiemMaterialLooksLikeVfx(item)) return item;
  }
  return nullptr;
}

#if defined(EIEM_SPRINT_TIMING_PROBE_BUILD)
// Observation-only sprint lifecycle probe.  It records ordering and aggregate
// counts without changing any RendererInfo, Renderer, Mesh or Material state.
// The production build does not compile this block.
struct EiemSprintTimingProbeEntry {
  void *info = nullptr;
  void *renderer = nullptr;
  void *lastTemplate = nullptr;
  uint64_t session = 0;
  uint64_t firstCommitMs = 0;
  uint64_t firstTemplateMs = 0;
  uint64_t firstCacheMs = 0;
  uint64_t cacheBuildStartMs = 0;
  uint64_t firstActiveMs = 0;
  uint64_t endMs = 0;
  uint32_t commitCount = 0;
  uint32_t cacheBuildCount = 0;
  uint64_t activeCallCount = 0;
  bool sawActive = false;
  bool ended = false;
};

static EiemSprintTimingProbeEntry s_eiemSprintTimingProbe[128] = {};
static uint64_t s_eiemSprintTimingProbeSession = 0;

static uint64_t EiemSprintTimingProbeNowMs() {
  return static_cast<uint64_t>(GetTickCount64());
}

static EiemSprintTimingProbeEntry *EiemSprintTimingProbeFind(
    void *info, bool allocate) {
  if (!info) return nullptr;
  for (auto &entry : s_eiemSprintTimingProbe)
    if (entry.info == info) return &entry;
  if (!allocate) return nullptr;
  for (auto &entry : s_eiemSprintTimingProbe) {
    if (entry.info) continue;
    entry.info = info;
    entry.session = ++s_eiemSprintTimingProbeSession;
    return &entry;
  }
  // Reuse a completed lifecycle if the fixed evidence table is exhausted.
  for (auto &entry : s_eiemSprintTimingProbe) {
    if (!entry.ended) continue;
    entry = {};
    entry.info = info;
    entry.session = ++s_eiemSprintTimingProbeSession;
    return &entry;
  }
  return nullptr;
}

static EiemSprintTimingProbeEntry *EiemSprintTimingProbeEnsure(
    void *info, void *renderer) {
  auto *entry = EiemSprintTimingProbeFind(info, true);
  if (!entry) return nullptr;
  if (entry->ended) {
    *entry = {};
    entry->info = info;
    entry->session = ++s_eiemSprintTimingProbeSession;
  }
  entry->renderer = renderer;
  return entry;
}

static void EiemSprintTimingProbeOnMaterialCommit(
    void *info, void *renderer, const char *method, bool nativeResult,
    bool adapted) {
  if (!info || !renderer) return;
  const auto visibility = EiemReadVisibilityControllerProbeState(renderer);
  if (!visibility.tracked) return;

  const auto state = EiemReadRendererInfoMaterialState(info, renderer);
  void *templateMaterial =
      EiemFindVfxTemplate(state.replacingMaterials, true);
  if (!templateMaterial)
    templateMaterial = EiemFindVfxTemplate(state.currentMaterials, true);
  if (!templateMaterial) return;

  auto *entry = EiemSprintTimingProbeEnsure(info, renderer);
  if (!entry) return;
  const uint64_t now = EiemSprintTimingProbeNowMs();
  ++entry->commitCount;
  if (!entry->firstCommitMs) {
    entry->firstCommitMs = now;
    entry->firstTemplateMs = now;
    entry->lastTemplate = templateMaterial;
    Log("[SPRINT-TIMING] event=native-vfx-commit session=%llu "
        "info=%p renderer=%p method=%s template=%p nativeResult=%d "
        "adapted=%d tickMs=%llu",
        (unsigned long long)entry->session, info, renderer,
        method ? method : "?", templateMaterial, nativeResult ? 1 : 0,
        adapted ? 1 : 0, (unsigned long long)now);
  } else if (entry->lastTemplate != templateMaterial) {
    entry->lastTemplate = templateMaterial;
    Log("[SPRINT-TIMING] event=native-vfx-template-change session=%llu "
        "info=%p renderer=%p method=%s template=%p tickMs=%llu",
        (unsigned long long)entry->session, info, renderer,
        method ? method : "?", templateMaterial,
        (unsigned long long)now);
  }
}

static void EiemSprintTimingProbeOnVfxTemplate(
    void *info, void *renderer, void *templateMaterial, const char *stage) {
  if (!info || !renderer || !templateMaterial) return;
  auto *entry = EiemSprintTimingProbeEnsure(info, renderer);
  if (!entry) return;
  const uint64_t now = EiemSprintTimingProbeNowMs();
  if (!entry->firstTemplateMs) {
    entry->firstTemplateMs = now;
    entry->lastTemplate = templateMaterial;
    Log("[SPRINT-TIMING] event=vfx-template-seen session=%llu info=%p "
        "renderer=%p stage=%s template=%p tickMs=%llu",
        (unsigned long long)entry->session, info, renderer,
        stage ? stage : "?", templateMaterial, (unsigned long long)now);
  } else if (entry->lastTemplate != templateMaterial) {
    entry->lastTemplate = templateMaterial;
    Log("[SPRINT-TIMING] event=vfx-template-change session=%llu info=%p "
        "renderer=%p stage=%s template=%p tickMs=%llu",
        (unsigned long long)entry->session, info, renderer,
        stage ? stage : "?", templateMaterial, (unsigned long long)now);
  }
}

static void EiemSprintTimingProbeOnCacheBuild(
    void *info, void *renderer, void *templateMaterial, bool begin,
    bool success) {
  if (!info || !renderer) return;
  auto *entry = EiemSprintTimingProbeEnsure(info, renderer);
  if (!entry) return;
  const uint64_t now = EiemSprintTimingProbeNowMs();
  if (begin) {
    ++entry->cacheBuildCount;
    if (!entry->firstCacheMs) entry->firstCacheMs = now;
    entry->cacheBuildStartMs = now;
    Log("[SPRINT-TIMING] event=cache-build-begin session=%llu info=%p "
        "renderer=%p template=%p buildCount=%u tickMs=%llu",
        (unsigned long long)entry->session, info, renderer, templateMaterial,
        (unsigned)entry->cacheBuildCount, (unsigned long long)now);
    return;
  }
  const uint64_t elapsed = entry->cacheBuildStartMs
                               ? now - entry->cacheBuildStartMs
                               : 0;
  Log("[SPRINT-TIMING] event=cache-build-end session=%llu info=%p "
      "renderer=%p template=%p success=%d elapsedMs=%llu tickMs=%llu",
      (unsigned long long)entry->session, info, renderer, templateMaterial,
      success ? 1 : 0, (unsigned long long)elapsed,
      (unsigned long long)now);
  entry->cacheBuildStartMs = 0;
}

static void EiemSprintTimingProbeOnPerDraw(void *info, void *renderer,
                                            float valueW) {
  if (!info || !renderer) return;
  auto *entry = EiemSprintTimingProbeFind(info, valueW > 0.01f);
  if (!entry) return;
  entry->renderer = renderer;
  if (valueW > 0.01f) {
    ++entry->activeCallCount;
    if (!entry->firstActiveMs) {
      entry->firstActiveMs = EiemSprintTimingProbeNowMs();
      entry->sawActive = true;
      Log("[SPRINT-TIMING] event=first-active-perdraw session=%llu "
          "info=%p renderer=%p valueW=%.3f activeCalls=%llu tickMs=%llu",
          (unsigned long long)entry->session, info, renderer,
          (double)valueW, (unsigned long long)entry->activeCallCount,
          (unsigned long long)entry->firstActiveMs);
    }
    return;
  }
  if (!entry->sawActive || entry->ended) return;
  entry->ended = true;
  entry->endMs = EiemSprintTimingProbeNowMs();
  const bool commitBeforeActive =
      entry->firstCommitMs && entry->firstActiveMs &&
      entry->firstCommitMs <= entry->firstActiveMs;
  const bool cacheBeforeActive =
      entry->firstCacheMs && entry->firstActiveMs &&
      entry->firstCacheMs <= entry->firstActiveMs;
  const uint64_t commitToActive =
      commitBeforeActive ? entry->firstActiveMs - entry->firstCommitMs : 0;
  const uint64_t cacheToActive =
      cacheBeforeActive ? entry->firstActiveMs - entry->firstCacheMs : 0;
  Log("[SPRINT-TIMING] event=sprint-end session=%llu info=%p renderer=%p "
      "commitCount=%u cacheBuildCount=%u activeCalls=%llu "
      "commitBeforeActive=%d cacheBeforeActive=%d commitToActiveMs=%llu "
      "cacheToActiveMs=%llu tickMs=%llu",
      (unsigned long long)entry->session, info, renderer,
      (unsigned)entry->commitCount, (unsigned)entry->cacheBuildCount,
      (unsigned long long)entry->activeCallCount,
      commitBeforeActive ? 1 : 0, cacheBeforeActive ? 1 : 0,
      (unsigned long long)commitToActive, (unsigned long long)cacheToActive,
      (unsigned long long)entry->endMs);
}
#endif

static bool EiemCopyLzyTextureProperties(
    void *target, void *source,
    const std::vector<std::string> *configuredProperties = nullptr) {
  if (!target || !source || !s_eiemMaterialSetTexture ||
      !s_eiemMaterialGetTexture || !il2cpp_string_new)
    return false;
  bool copied = false;
  static volatile LONG s_eiemVfxTextureDebugCalls = 0;
  const LONG debugCall = InterlockedIncrement(&s_eiemVfxTextureDebugCalls);
  const bool debug = debugCall <= 24;
  void *sourceShaderForDebug = nullptr;
  if (debug && g_material_get_shader) {
    __try { sourceShaderForDebug = Invoke(g_material_get_shader, source); }
    __except (EXCEPTION_EXECUTE_HANDLER) { sourceShaderForDebug = nullptr; }
  }
  if (debug) {
    char sourceName[160] = {};
    char targetName[160] = {};
    TraceReadUnityObjectName(source, sourceName, sizeof(sourceName));
    TraceReadUnityObjectName(target, targetName, sizeof(targetName));
    Log("[MOD-VFX-TEX-DEBUG] call=%ld target=%p/%s source=%p/%s "
        "sourceShader=%p directNames=%p shaderApis=%p/%p/%p",
        debugCall, target, targetName[0] ? targetName : "<unnamed>",
        source, sourceName[0] ? sourceName : "<unnamed>",
        sourceShaderForDebug, g_material_GetTexturePropertyNames,
        g_eiemShaderGetPropertyCount, g_eiemShaderGetPropertyName,
        g_eiemShaderGetPropertyType);
  }
  auto copyProperty = [&](const char *propertyName) {
    if (!propertyName || !propertyName[0]) return;
    void *name = il2cpp_string_new(propertyName);
    if (!name) return;
    void *getParams[] = {name};
    void *texture = g_material_GetTexture
                        ? Invoke(g_material_GetTexture, source, getParams)
                        : nullptr;
    if (debug)
      Log("[MOD-VFX-TEX-DEBUG] call=%ld property=%s texture=%p",
          debugCall, propertyName, texture);
    if (!texture) return;
    void *setParams[] = {name, texture};
    Invoke(s_eiemMaterialSetTexture, target, setParams);
    void *readbackParams[] = {name};
    void *readback = Invoke(s_eiemMaterialGetTexture, target, readbackParams);
    if (debug)
      Log("[MOD-VFX-TEX-DEBUG] call=%ld property=%s readback=%p set=%d",
          debugCall, propertyName, readback, readback == texture ? 1 : 0);
    if (readback == texture)
      copied = true;
  };

  if (configuredProperties && !configuredProperties->empty()) {
    if (debug)
      Log("[MOD-VFX-TEX-CONFIG] call=%ld propertyCount=%zu", debugCall,
          configuredProperties->size());
    for (const std::string &property : *configuredProperties)
      copyProperty(property.c_str());
  } else {
    // The game fork does not expose Material.GetTexturePropertyNames() in all
    // builds (the pointer is null in the current Endfield player).  Enumerate
    // the source shader instead and keep only ShaderPropertyType.Texture (4).
    // This remains generic: the shader declares the property names; EFF does
    // not contain character, material, or slot names.
    void *properties = g_material_GetTexturePropertyNames
                           ? Invoke(g_material_GetTexturePropertyNames, source)
                           : nullptr;
    const size_t count = EiemManagedArrayLength(properties);
    if (properties && count <= 256) {
      for (size_t index = 0; index < count; ++index) {
        void *property = EiemVfxArrayItem(properties, index);
        char propertyName[192] = {};
        if (property) ReadStrUtf8(property, propertyName, sizeof(propertyName));
        copyProperty(propertyName);
      }
    } else if (g_material_get_shader && g_eiemShaderGetPropertyCount &&
               g_eiemShaderGetPropertyName && g_eiemShaderGetPropertyType) {
      void *shader = Invoke(g_material_get_shader, source);
      const int32_t shaderCount = shader
                                      ? EiemTraceUnboxInt(
                                            Invoke(g_eiemShaderGetPropertyCount,
                                                   shader))
                                      : -1;
      if (debug)
        Log("[MOD-VFX-TEX-DEBUG] call=%ld shader=%p propertyCount=%d",
            debugCall, shader, shaderCount);
      if (shaderCount > 0 && shaderCount <= 512) {
        for (int32_t index = 0; index < shaderCount; ++index) {
          void *indexParams[] = {&index};
          void *typeObject = Invoke(g_eiemShaderGetPropertyType, shader,
                                     indexParams);
          const int32_t propertyType = EiemTraceUnboxInt(typeObject);
          void *property = Invoke(g_eiemShaderGetPropertyName, shader,
                                  indexParams);
          char propertyName[192] = {};
          if (property) ReadStrUtf8(property, propertyName, sizeof(propertyName));
          if (debug && index < 32)
            Log("[MOD-VFX-TEX-DEBUG] call=%ld shaderProperty=%d name=%s type=%d",
                debugCall, index, propertyName[0] ? propertyName : "<empty>",
                propertyType);
          if (propertyType != 4) continue;
          copyProperty(propertyName);
        }
      }
    }
  }
  if (!copied)
    Log("[MOD-VFX-SLOT-ADAPTER] no LZY texture properties copied target=%p source=%p direct=%p shader=%p",
        target, source, g_material_GetTexturePropertyNames,
        g_material_get_shader);
  return copied;
}

static bool EiemBuildLzyMaterialSectionsForRule(
    const EiemModRule &rule, std::vector<std::string> *out, char *error,
    size_t errorSize);
static bool EiemApplyLzyMaterialValueProperties(
    void *target, const EiemModRule &rule, const char *section,
    size_t *appliedOut);
static bool EiemCopyCachedPerDrawVfxLzyData(
    void *info, void **clean, size_t *cleanCount,
    std::vector<std::vector<std::string>> *textureProperties,
    std::vector<std::string> *materialSections);

// Let RendererInfo create and own the native VFXInstance.  Once the native
// commit has completed, replace only the texture properties on that runtime
// material with the matched LZY material's textures.  This preserves the
// game's shader, dissolve parameters, per-draw identity and one-slot
// replacement transaction while making only the VFX instance that occupies an
// EFF material slot use the mod textures.  Extra effect-only entries are left
// native.  The mapping is array-index based when native supplies several VFX
// instances; a single native instance falls back to LZY slot zero because that
// is the material the game's one-slot renderer contract can actually sample.
static bool EiemPatchNativeVfxTexturesForRenderer(void *info,
                                                  void *renderer) {
  if (!info || !renderer || !EiemOnUnityThread()) return false;
  EiemVfxPatchGuard patchGuard;
  const auto state = EiemReadRendererInfoMaterialState(info, renderer);
  // The native VFX instance normally survives for the whole dash.  Once all
  // visible VFX targets have been overlaid, the per-draw hook only needs to
  // keep the native animation running; rebuilding mod arrays and reading .mat
  // files here would turn every frame into a resource preparation pass.
  bool hasVfx = false;
  const bool hasUnseenVfx =
      EiemVfxPatchHasUnseenTarget(info, state, &hasVfx);
  // Most native material commits are ordinary clean arrays.  Do not resolve
  // rules, copy cache vectors, or touch .mat data until a VFXInstance is
  // actually present.  This is both the hot-path fast exit and the boundary
  // that keeps unrelated material commits out of the sprint adapter.
  if (!hasVfx) return false;
  if (!hasUnseenVfx)
    return true;
  EiemResolvedRenderRule resolved = {};
  char error[256] = {};
  void *clean = nullptr;
  size_t cleanCount = 0;
  std::vector<std::vector<std::string>> textureProperties;
  std::vector<std::string> materialSections;
  if (!EiemResolveVfxRenderRule(renderer, &resolved) ||
      !resolved.rule.materialCount)
    return false;

  // Prewarm creates this cache at the native material commit boundary.  Reuse
  // its parsed LZY data when a new native VFXInstance arrives on a later draw;
  // otherwise every frame would rebuild arrays and reread .mat resources.
  if (!EiemCopyCachedPerDrawVfxLzyData(
          info, &clean, &cleanCount, &textureProperties,
          &materialSections)) {
    if (!EiemBuildRendererMaterials(resolved.rule, &clean, error,
                                    sizeof(error)) || !clean)
      return false;
    cleanCount = EiemManagedArrayLength(clean);
    if (!cleanCount || cleanCount > 64) return false;
    if (!EiemBuildLzyTexturePropertyNamesForRule(
            resolved.rule, &textureProperties, error, sizeof(error)))
      return false;
    if (textureProperties.size() < cleanCount)
      textureProperties.resize(cleanCount);
    if (!EiemBuildLzyMaterialSectionsForRule(
            resolved.rule, &materialSections, error, sizeof(error)))
      return false;
    if (materialSections.size() < cleanCount)
      materialSections.resize(cleanCount);
  }
  if (!cleanCount || cleanCount > 64) return false;

  const auto runtimeState = EiemReadRendererInfoMaterialState(info, renderer);
  void *arrays[] = {runtimeState.replacingMaterials,
                    runtimeState.currentMaterials};
  void *seen[64] = {};
  size_t seenCount = 0;
  size_t overlappingVfx = 0;
  size_t patched = 0;
  for (void *array : arrays) {
    const size_t count = EiemManagedArrayLength(array);
    if (!array || !count || count > 64) continue;
    for (size_t index = 0; index < count; ++index) {
      // A native VFX array may contain extra effect-only entries after the
      // renderer's declared EFF material slots.  Those entries are owned by
      // the game and must remain untouched.  Only a VFX material occupying an
      // EFF slot is the overlay case this adapter is meant to handle.
      void *target = EiemVfxArrayItem(array, index);
      if (!target || !EiemMaterialLooksLikeVfx(target)) continue;
      if (index >= cleanCount) {
        // Remember effect-only entries too; otherwise an unrelated native
        // VFX slot would defeat the once-per-instance fast path forever.
        EiemRememberVfxPatch(info, target);
        continue;
      }
      ++overlappingVfx;
      bool duplicate = false;
      for (size_t i = 0; i < seenCount; ++i)
        if (seen[i] == target) {
          duplicate = true;
          break;
        }
      if (duplicate) continue;
      if (seenCount < _countof(seen)) seen[seenCount++] = target;
      const size_t sourceIndex = index;
      void *source = EiemVfxArrayItem(clean, sourceIndex);
      const std::vector<std::string> *properties =
          sourceIndex < textureProperties.size()
              ? &textureProperties[sourceIndex]
              : nullptr;
      const bool textures =
          EiemCopyLzyTextureProperties(target, source, properties);
      size_t values = 0;
      const bool materialValues =
          sourceIndex < materialSections.size() &&
          EiemApplyLzyMaterialValueProperties(
              target, resolved.rule, materialSections[sourceIndex].c_str(),
              &values);
      EiemRememberVfxPatch(info, target);
      if (textures || (materialValues && values)) ++patched;
    }
  }
  // Texture copying is on a hot VFX commit path.  Logging every successful
  // patch made the diagnostic DLL itself a frame-time participant during
  // sprint/attack bursts.  Keep only a bounded sample; the counters still
  // record whether the patch occurred.
  static volatile LONG s_logs = 0;
  const LONG logIndex = InterlockedIncrement(&s_logs);
  if (logIndex <= 96) {
    Log("[MOD-NATIVE-VFX-TEX] info=%p renderer=%p section=%s "
        "ruleSource=%s overlap=%zu patched=%zu cleanCount=%zu replacingCount=%zu "
        "currentCount=%zu error=%s",
        info, renderer, resolved.rule.section,
        resolved.source[0] ? resolved.source : "<unknown>", overlappingVfx,
        patched, cleanCount,
        runtimeState.replacingCount, runtimeState.currentCount,
        error[0] ? error : "<none>");
  }
  return patched != 0;
}

// A native renderer uses one VFXInstance for the whole renderer.  Only split
// that visible transaction into per-slot VFX clones when the LZY rule really
// has different material objects in different slots.  Repeated references to
// one material (the common merged-clothes case) need no fanout and retain the
// exact native one-slot path.
static bool EiemRendererLzySlotsNeedVfxFanout(void *renderer) {
  if (!renderer) return false;
  EiemResolvedRenderRule resolved = {};
  if (!EiemFindBoundRenderRule(renderer, &resolved.rule) ||
      !resolved.rule.materialCount)
    return false;
  char error[256] = {};
  void *clean = nullptr;
  if (!EiemBuildRendererMaterials(resolved.rule, &clean, error,
                                  sizeof(error)) || !clean)
    return false;
  const size_t count = EiemManagedArrayLength(clean);
  if (count <= 1 || count > 64) return false;
  void *first = EiemVfxArrayItem(clean, 0);
  if (!first) return false;
  for (size_t index = 1; index < count; ++index)
    if (EiemVfxArrayItem(clean, index) != first) return true;
  return false;
}

// Resolve the material section that owns each logical LZY slot.  This mirrors
// EiemBuildRendererMaterials, including slot-0 inheritance and submesh maps,
// so VFX clones receive the same per-slot material definition as the clean
// renderer.  No character, asset, or material name is embedded here.
static bool EiemBuildLzyMaterialSectionsForRule(
    const EiemModRule &rule, std::vector<std::string> *out, char *error,
    size_t errorSize) {
  if (out) out->clear();
  if (!out || rule.materialCount == 0) return true;
  uint32_t arrayCount = rule.materialCount;
  for (uint32_t index = 0; index < rule.materialCount; ++index) {
    if (rule.materialSlots[index] >= 0 &&
        (uint32_t)(rule.materialSlots[index] + 1) > arrayCount)
      arrayCount = (uint32_t)rule.materialSlots[index] + 1;
  }
  if (!arrayCount || arrayCount > 64) {
    if (error) strncpy_s(error, errorSize, "Invalid LZY material slot count", _TRUNCATE);
    return false;
  }
  out->resize(arrayCount);
  for (uint32_t index = 0; index < rule.materialCount; ++index) {
    const int32_t slot = rule.materialSlots[index] >= 0
                             ? rule.materialSlots[index]
                             : (int32_t)index;
    if (slot >= 0 && (uint32_t)slot < arrayCount)
      (*out)[(size_t)slot] = rule.materials[index];
  }
  for (uint32_t slot = 1; slot < arrayCount; ++slot)
    if ((*out)[slot].empty()) (*out)[slot] = (*out)[0];
  if (rule.submeshCount) {
    std::vector<std::string> original = *out;
    const uint32_t limit = rule.submeshCount < arrayCount
                               ? rule.submeshCount
                               : arrayCount;
    for (uint32_t submesh = 0; submesh < limit; ++submesh) {
      const int32_t sourceSlot = rule.submeshSlots[submesh];
      if (sourceSlot >= 0 && (uint32_t)sourceSlot < arrayCount)
        (*out)[submesh] = original[(size_t)sourceSlot];
    }
  }
  return true;
}

// Apply only properties explicitly declared by an EFF material resource.
// The clone already contains the game's native VFX material, so undeclared
// dissolve/Fresnel/runtime values remain intact.  This is the material-side
// equivalent of the texture patch and is intentionally data-driven.
struct EiemLzyMaterialValueCacheEntry {
  char modPath[MAX_PATH] = {};
  char section[96] = {};
  LONG generation = 0;
  bool loaded = false;
  bool valid = false;
  std::vector<std::pair<std::string, std::string>> values;
};
static EiemLzyMaterialValueCacheEntry s_eiemLzyMaterialValueCache[64] = {};

static const std::vector<std::pair<std::string, std::string>> *
EiemGetCachedLzyMaterialValueProperties(const EiemModRule &rule,
                                         const char *section) {
  if (!section || !section[0] || !rule.modPath[0]) return nullptr;
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  for (auto &entry : s_eiemLzyMaterialValueCache) {
    if (entry.loaded && entry.generation == generation &&
        strcmp(entry.modPath, rule.modPath) == 0 &&
        strcmp(entry.section, section) == 0)
      return entry.valid ? &entry.values : nullptr;
  }

  EiemModResource resource = {};
  char materialPath[kEiemResourceDiskPathCapacity] = {};
  std::vector<std::pair<std::string, std::string>> values;
  char error[256] = {};
  const bool valid =
      EiemFindModResource(rule.modPath, section, "Material", &resource) &&
      EiemResolveResourceDiskPath(resource, materialPath,
                                  sizeof(materialPath)) &&
      EiemReadMaterialFile(materialPath, &values, error, sizeof(error));

  EiemLzyMaterialValueCacheEntry *slot = nullptr;
  for (auto &entry : s_eiemLzyMaterialValueCache) {
    if (!entry.loaded || entry.generation != generation) {
      slot = &entry;
      break;
    }
    if (strcmp(entry.modPath, rule.modPath) == 0 &&
        strcmp(entry.section, section) == 0) {
      slot = &entry;
      break;
    }
  }
  if (!slot) {
    slot = &s_eiemLzyMaterialValueCache[
        (reinterpret_cast<uintptr_t>(section) >> 4) &
        (_countof(s_eiemLzyMaterialValueCache) - 1)];
  }
  *slot = {};
  strncpy_s(slot->modPath, sizeof(slot->modPath), rule.modPath, _TRUNCATE);
  strncpy_s(slot->section, sizeof(slot->section), section, _TRUNCATE);
  slot->generation = generation;
  slot->loaded = true;
  slot->valid = valid;
  if (valid) {
    for (auto &pair : values) {
      const char *key = pair.first.c_str();
      if (_strnicmp(key, "float.", 6) == 0 ||
          _strnicmp(key, "int.", 4) == 0 ||
          _strnicmp(key, "value4.", 7) == 0 ||
          _strnicmp(key, "texture_scale.", 14) == 0 ||
          _strnicmp(key, "texture_offset.", 15) == 0)
        slot->values.push_back(std::move(pair));
    }
  }
  return slot->valid ? &slot->values : nullptr;
}

static bool EiemApplyLzyMaterialValueProperties(
    void *target, const EiemModRule &rule, const char *section,
    size_t *appliedOut = nullptr) {
  if (appliedOut) *appliedOut = 0;
  if (!target || !section || !section[0]) return false;
  const auto *values = EiemGetCachedLzyMaterialValueProperties(rule, section);
  if (!values) return false;
  size_t applied = 0;
  for (const auto &pair : *values) {
    const char *key = pair.first.c_str();
    if (_strnicmp(key, "float.", 6) == 0 && s_eiemMaterialSetFloat) {
      float value = 0.0f;
      if (!EiemParseFloat(pair.second, &value)) continue;
      void *name = il2cpp_string_new(key + 6);
      void *params[] = {name, &value};
      Invoke(s_eiemMaterialSetFloat, target, params);
      ++applied;
    } else if (_strnicmp(key, "int.", 4) == 0 && s_eiemMaterialSetInt) {
      int32_t value = 0;
      if (!EiemParseInt32(pair.second, &value)) continue;
      void *name = il2cpp_string_new(key + 4);
      void *params[] = {name, &value};
      Invoke(s_eiemMaterialSetInt, target, params);
      ++applied;
    } else if (_strnicmp(key, "value4.", 7) == 0 && s_eiemMaterialSetColor) {
      Color value = {};
      if (!EiemParseColor(pair.second, &value)) continue;
      void *name = il2cpp_string_new(key + 7);
      void *params[] = {name, &value};
      Invoke(s_eiemMaterialSetColor, target, params);
      ++applied;
    } else if (_strnicmp(key, "texture_scale.", 14) == 0 &&
               s_eiemMaterialSetTextureScale) {
      Vector2 value = {};
      if (!EiemParseVector2(pair.second, &value)) continue;
      void *name = il2cpp_string_new(key + 14);
      void *params[] = {name, &value};
      Invoke(s_eiemMaterialSetTextureScale, target, params);
      ++applied;
    } else if (_strnicmp(key, "texture_offset.", 15) == 0 &&
               s_eiemMaterialSetTextureOffset) {
      Vector2 value = {};
      if (!EiemParseVector2(pair.second, &value)) continue;
      void *name = il2cpp_string_new(key + 15);
      void *params[] = {name, &value};
      Invoke(s_eiemMaterialSetTextureOffset, target, params);
      ++applied;
    }
  }
  if (appliedOut) *appliedOut = applied;
  return true;
}

static void *EiemCloneVfxWithLzyTextures(
    void *vfxTemplate, void *lzyMaterial,
    const std::vector<std::string> *configuredProperties = nullptr) {
  if (!vfxTemplate || !s_eiemMaterialClass || !s_eiemMaterialCtorCopy)
    return nullptr;
  void *clone = il2cpp_object_new(s_eiemMaterialClass);
  if (!clone) return nullptr;
  void *copyParams[] = {vfxTemplate};
  Invoke(s_eiemMaterialCtorCopy, clone, copyParams);
  if (!EiemCopyLzyTextureProperties(clone, lzyMaterial,
                                    configuredProperties))
    return nullptr;
  return clone;
}

struct EiemVfxSlotAdapterState {
  void *renderer = nullptr;
  void *cleanMaterials = nullptr;
  void *templateMaterial = nullptr;
  void *mappedSingle = nullptr;
  void *mappedInput = nullptr;
  void *mappedMaterials = nullptr;
  std::vector<std::vector<std::string>> textureProperties;
  std::vector<std::string> materialSections;
  bool inputIsArray = false;
  bool valid = false;
};

static bool EiemPrepareNativeVfxSlotAdapter(
    void *info, void *renderer, void *input, bool inputIsArray,
    EiemVfxSlotAdapterState *out) {
  if (out) *out = {};
  auto fail = [&](const char *reason) {
    Log("[MOD-VFX-SLOT-ADAPTER] prepare skipped info=%p renderer=%p input=%p "
        "array=%d reason=%s",
        info, renderer, input, inputIsArray ? 1 : 0,
        reason ? reason : "unknown");
    return false;
  };
  if (!info || !renderer || !input || !out) return fail("invalid-argument");
  if (!EiemOnUnityThread()) return fail("not-unity-thread");
  void *vfxTemplate = EiemFindVfxTemplate(input, inputIsArray);
  if (!vfxTemplate) return fail("no-vfx-template");
  EiemResolvedRenderRule resolved = {};
  if (!EiemFindBoundRenderRule(renderer, &resolved.rule) ||
      (!resolved.rule.materialCount && !resolved.rule.submeshCount))
    return fail("no-bound-rule");
  char error[256] = {};
  void *clean = nullptr;
  if (!EiemBuildRendererMaterials(resolved.rule, &clean, error,
                                  sizeof(error)) || !clean)
    return fail(error[0] ? error : "build-lzy-materials-failed");
  const size_t cleanCount = EiemManagedArrayLength(clean);
  if (!cleanCount || cleanCount > 64) return fail("invalid-clean-count");
  std::vector<std::vector<std::string>> textureProperties;
  if (!EiemBuildLzyTexturePropertyNamesForRule(
          resolved.rule, &textureProperties, error, sizeof(error)))
    return fail(error[0] ? error : "read-lzy-texture-properties-failed");
  if (textureProperties.size() < cleanCount)
    textureProperties.resize(cleanCount);
  if (!s_eiemMaterialClass) return fail("material-class-unavailable");
  void *single = il2cpp_array_new(s_eiemMaterialClass, 1);
  if (!single) return fail("mapped-array-allocation-failed");
  void *mapped = il2cpp_array_new(s_eiemMaterialClass, cleanCount);
  if (!mapped) return fail("mapped-vfx-array-allocation-failed");
  void **mappedItems = (void **)((char *)mapped + IL2CPP_ARRAY_DATA);
  for (size_t index = 0; index < cleanCount; ++index) {
    void *clone = EiemCloneVfxWithLzyTextures(
        vfxTemplate, EiemVfxArrayItem(clean, index),
        index < textureProperties.size() ? &textureProperties[index] : nullptr);
    if (!clone) return fail("clone-vfx-with-lzy-textures-failed");
    mappedItems[index] = clone;
  }
  void *first = mappedItems[0];
  *(void **)((char *)single + IL2CPP_ARRAY_DATA) = first;
  out->renderer = renderer;
  out->cleanMaterials = clean;
  out->templateMaterial = vfxTemplate;
  out->mappedSingle = single;
  // Keep the native controller's original one-element transaction intact: its
  // dissolve code expects one VFX instance and drives that instance's timing
  // and per-draw values.  The expanded array is installed after the native
  // call by the per-draw adapter below, so multi-submesh clothes render with
  // their own LZY slot materials without changing the game's VFX contract.
  out->mappedInput = inputIsArray ? single : first;
  out->mappedMaterials = mapped;
  out->textureProperties = std::move(textureProperties);
  std::vector<std::string> materialSections;
  if (!EiemBuildLzyMaterialSectionsForRule(
          resolved.rule, &materialSections, error, sizeof(error)))
    return fail(error[0] ? error : "read-lzy-material-sections-failed");
  if (materialSections.size() < cleanCount)
    materialSections.resize(cleanCount);
  out->materialSections = std::move(materialSections);
  out->inputIsArray = inputIsArray;
  out->valid = true;
  Log("[MOD-VFX-SLOT-ADAPTER] prepare renderer=%p section=%s "
      "inputCount=%zu cleanCount=%zu",
      renderer, resolved.rule.section,
      inputIsArray ? EiemManagedArrayLength(input) : 1u, cleanCount);
  return true;
}

static bool EiemCommitNativeVfxSlotAdapter(
    const EiemVfxSlotAdapterState &state) {
  if (!state.valid || !state.renderer || !state.cleanMaterials) return false;
  const size_t cleanCount = EiemManagedArrayLength(state.cleanMaterials);
  void *mapped = state.mappedMaterials;
  const size_t mappedCount = EiemManagedArrayLength(mapped);
  if (!mapped || mappedCount != cleanCount || mappedCount > 64) return false;
  char error[256] = {};
  const bool assigned = EiemAssignRendererMaterials(
      state.renderer, mapped, error, sizeof(error));
  Log("[MOD-VFX-SLOT-ADAPTER] commit renderer=%p cleanCount=%zu "
      "mappedCount=%zu assigned=%d error=%s",
      state.renderer, cleanCount, mappedCount,
      assigned ? 1 : 0, error[0] ? error : "<none>");
  return assigned;
}

static bool EiemInvokeAdaptedNativeVfxCommit(
    void *self, void *input, bool inputIsArray, void *methodInfo,
    TraceRendererInfoMaterialCommitFn original, bool *adapted) {
  if (adapted) *adapted = false;
#if defined(EIEM_NATIVE_VFX_PASSTHROUGH_TEXTURE_PATCH_BUILD)
  // Native owns the VFXInstance transaction.  Calling it with the exact
  // original argument is essential: it creates/reuses the one-slot runtime
  // material and updates RendererInfo's private replacement fields.  EIEM
  // only patches the native instance after that commit has finished.
  const bool result = original ? original(self, input, methodInfo) : false;
  void *renderer = EiemReadRendererFromMaterialInfo(self);
  // The input to TrySet/TryReplace is the clean material array.  Native VFX
  // can replace RendererInfo's runtime slots inside the original call, so the
  // input name/type is not a reliable discriminator.  The patch helper reads
  // the post-native current/replacing arrays and only succeeds when a VFX
  // material occupies a corresponding LZY material slot.  That result is the
  // adaptation predicate: ordinary attack commits continue through the normal
  // EFF material reapply path, while a matched native VFX slot keeps the
  // game's native shader/dissolve transaction and receives only LZY textures.
  const bool patched = EiemPatchNativeVfxTexturesForRenderer(self, renderer);
  static volatile LONG s_passthroughLogs = 0;
  const LONG logIndex = InterlockedIncrement(&s_passthroughLogs);
  if (logIndex <= 64) {
    Log("[MOD-NATIVE-VFX-PASSTHROUGH] methodArray=%d info=%p renderer=%p "
        "result=%d slotMatched=%d patched=%d inputCount=%zu",
        inputIsArray ? 1 : 0, self, renderer, result ? 1 : 0,
        patched ? 1 : 0, patched ? 1 : 0,
        inputIsArray ? EiemManagedArrayLength(input) : 1u);
  }
  // Keep the native VFX commit on the prewarm path.  The renderer rule, not
  // the effect asset name, decides which LZY materials are copied into the
  // runtime VFX instance.
  if (adapted) *adapted = patched;
  return result;
#else
  void *renderer = EiemReadRendererFromMaterialInfo(self);
  EiemVfxSlotAdapterState state = {};
  if (!EiemPrepareNativeVfxSlotAdapter(self, renderer, input, inputIsArray,
                                       &state))
    return original ? original(self, input, methodInfo) : false;
  const bool result = original
                          ? original(self, state.mappedInput, methodInfo)
                          : false;
#if defined(EIEM_NATIVE_VFX_SINGLE_RENDERER_AB_BUILD)
  // A/B: keep the renderer on the exact one-element VFX array used by the
  // game.  The native dissolve path appears to be renderer-level, so
  // expanding this transaction to one material per LZY submesh can leave
  // clothes visible even though the per-draw value is updated.  The first
  // clone still carries the LZY textures; the full LZY array is restored at
  // the end of the dash.
  if (state.mappedInput) {
    const bool assigned = EiemAssignRendererMaterials(
        renderer, state.mappedInput, nullptr, 0);
    Log("[MOD-VFX-SINGLE-AB] initial renderer=%p cleanCount=%zu "
        "assigned=%d nativeInputCount=%zu",
        renderer, EiemManagedArrayLength(state.cleanMaterials),
        assigned ? 1 : 0, inputIsArray ? EiemManagedArrayLength(input) : 1u);
  }
#else
  EiemCommitNativeVfxSlotAdapter(state);
#endif
  if (adapted) *adapted = true;
  return result;
#endif
}
#endif

#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD) && \
    defined(EIEM_PERDRAW_END_RESTORE_BUILD)
// Native RendererInfo intentionally keeps a one-element VFX replacement. The
// renderer itself can still have several submesh slots, so mirror that one
// runtime VFX material into a data-driven LZY-sized array after the native
// per-draw update. This keeps the game's dissolve state authoritative while
// preventing clothes from falling back to the body/source material in slots
// 1..N.
struct EiemPerDrawVfxSlotCache {
  void *info = nullptr;
  void *templateMaterial = nullptr;
  void *cleanMaterials = nullptr;
  void *mappedMaterials = nullptr;
  // Scratch one-slot array used only while replaying the game's native
  // per-draw update for the extra mapped VFX materials.  It is deliberately
  // separate from RendererInfo's own replacingMaterials field.
  void *fanoutSingle = nullptr;
  uint32_t cleanHandle = 0;
  uint32_t mappedHandle = 0;
  uint32_t fanoutHandle = 0;
  size_t count = 0;
  std::vector<std::vector<std::string>> textureProperties;
  std::vector<std::string> materialSections;
  bool clearPending = false;
};
static EiemPerDrawVfxSlotCache s_eiemPerDrawVfxSlotCaches[256] = {};

static EiemPerDrawVfxSlotCache *EiemFindPerDrawVfxSlotCache(void *info,
                                                            bool allocate) {
  if (!info) return nullptr;
  for (auto &entry : s_eiemPerDrawVfxSlotCaches)
    if (entry.info == info) return &entry;
  if (!allocate) return nullptr;
  // EiemClearPerDrawVfxSlotCache intentionally drops the managed handles and
  // zeroes the entry. Reuse that released slot for the next VFX transaction;
  // otherwise repeated attack/dodge cycles eventually hit the fixed 256-entry
  // table and the expanded submesh material array silently stops being built.
  for (auto &entry : s_eiemPerDrawVfxSlotCaches)
    if (!entry.info) {
      entry.info = info;
      return &entry;
    }
  return nullptr;
}

// Copy the parsed cache while still at the cache boundary.  Callers must not
// retain pointers into EiemPerDrawVfxSlotCache: a nested Unity material call
// can request cleanup as soon as the next managed call begins.  Returning
// value-owned vectors makes the later native-array walk independent of that
// cache entry's lifetime.
static bool EiemCopyCachedPerDrawVfxLzyData(
    void *info, void **clean, size_t *cleanCount,
    std::vector<std::vector<std::string>> *textureProperties,
    std::vector<std::string> *materialSections) {
  if (clean) *clean = nullptr;
  if (cleanCount) *cleanCount = 0;
  if (textureProperties) textureProperties->clear();
  if (materialSections) materialSections->clear();
  auto *cache = EiemFindPerDrawVfxSlotCache(info, false);
  if (!cache || !cache->cleanMaterials || !cache->count ||
      !textureProperties || !materialSections)
    return false;
  try {
    *textureProperties = cache->textureProperties;
    *materialSections = cache->materialSections;
  } catch (...) {
    return false;
  }
  if (clean) *clean = cache->cleanMaterials;
  if (cleanCount) *cleanCount = cache->count;
  return true;
}

static void EiemClearPerDrawVfxSlotCacheNow(void *info) {
  auto *entry = EiemFindPerDrawVfxSlotCache(info, false);
  if (!entry) return;
  if (entry->cleanHandle && il2cpp_gchandle_free)
    il2cpp_gchandle_free(entry->cleanHandle);
  if (entry->mappedHandle && il2cpp_gchandle_free)
    il2cpp_gchandle_free(entry->mappedHandle);
  if (entry->fanoutHandle && il2cpp_gchandle_free)
    il2cpp_gchandle_free(entry->fanoutHandle);
  *entry = {};
  // RendererInfo addresses can be reused by the next dash.  Do not let the
  // once-per-native-VFX patch guard mistake a new instance for the old one.
  for (auto &seen : s_eiemVfxPatchSeen)
    if (seen.info == info) seen = {};
}

static void EiemClearPerDrawVfxSlotCache(void *info) {
  auto *entry = EiemFindPerDrawVfxSlotCache(info, false);
  if (!entry) return;
  if (s_eiemVfxPatchDepth > 0) {
    // The current patch may still be using cleanMaterials and the copied
    // property vectors.  Defer handle release and table reset until the outer
    // patch guard has finished.
    entry->clearPending = true;
    return;
  }
  EiemClearPerDrawVfxSlotCacheNow(info);
}

static void EiemFlushDeferredPerDrawVfxSlotCaches() {
  if (s_eiemVfxPatchDepth != 0) return;
  for (auto &entry : s_eiemPerDrawVfxSlotCaches) {
    if (!entry.info || !entry.clearPending) continue;
    void *info = entry.info;
    EiemClearPerDrawVfxSlotCacheNow(info);
  }
}

#if defined(EIEM_PERDRAW_END_RESTORE_BUILD)
// Called only from the Unity-thread reconcile transaction, before the old
// render generation is restored.  RendererInfo pointers can be destroyed and
// reused by a key-driven render switch or by F10, so retaining either the
// expanded source array or the VFX fan-out arrays would bind the next
// generation to stale managed objects.  Put the private source field back to
// its original pointer while it is still valid, release every GC root, then
// reset the fixed-size lookup tables for the next generation.
static void EiemResetPerDrawSourceField(void *info, void *source) {
  if (!info || !source) return;
  __try {
    *(void **)((char *)info + 0x30) = source;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

static void EiemResetPerDrawMaterialCaches() {
  for (auto &entry : s_eiemPerDrawSourceExpansions) {
    if (entry.info && entry.expanded && entry.originalSource)
      EiemResetPerDrawSourceField(entry.info, entry.originalSource);
    if (entry.expandedHandle && il2cpp_gchandle_free)
      il2cpp_gchandle_free(entry.expandedHandle);
    entry = {};
  }
  InterlockedExchange(&s_eiemPerDrawSourceExpansionCount, 0);

#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD)
  for (auto &entry : s_eiemPerDrawVfxSlotCaches) {
    if (entry.cleanHandle && il2cpp_gchandle_free)
      il2cpp_gchandle_free(entry.cleanHandle);
    if (entry.mappedHandle && il2cpp_gchandle_free)
      il2cpp_gchandle_free(entry.mappedHandle);
    if (entry.fanoutHandle && il2cpp_gchandle_free)
      il2cpp_gchandle_free(entry.fanoutHandle);
    entry = {};
  }
#endif
}
#endif

static bool EiemCopyVfxRuntimeMaterialProperties(void *target,
                                                 void *source) {
  if (!target || !source || !g_material_CopyPropertiesFromMaterial)
    return false;
  void *params[] = {source};
  __try {
    Invoke(g_material_CopyPropertiesFromMaterial, target, params);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

static bool EiemBuildPerDrawVfxSlotArray(
    void *renderer, void *templateMaterial, void **outArray,
    size_t *outCount, void **outCleanMaterials,
    std::vector<std::vector<std::string>> *outTextureProperties,
    std::vector<std::string> *outMaterialSections) {
  if (outArray) *outArray = nullptr;
  if (outCount) *outCount = 0;
  if (outCleanMaterials) *outCleanMaterials = nullptr;
  if (outTextureProperties) outTextureProperties->clear();
  if (outMaterialSections) outMaterialSections->clear();
  if (!renderer || !templateMaterial || !outArray || !s_eiemMaterialClass)
    return false;
  EiemResolvedRenderRule resolved = {};
  if (!EiemResolveVfxRenderRule(renderer, &resolved) ||
      (!resolved.rule.materialCount && !resolved.rule.submeshCount))
    return false;
  char error[256] = {};
  void *clean = nullptr;
  if (!EiemBuildRendererMaterials(resolved.rule, &clean, error,
                                  sizeof(error)) || !clean)
    return false;
  const size_t cleanCount = EiemManagedArrayLength(clean);
  if (!cleanCount || cleanCount > 64) return false;
  std::vector<std::vector<std::string>> properties;
  if (!EiemBuildLzyTexturePropertyNamesForRule(
          resolved.rule, &properties, error, sizeof(error)))
    return false;
  if (properties.size() < cleanCount) properties.resize(cleanCount);
  std::vector<std::string> materialSections;
  if (!EiemBuildLzyMaterialSectionsForRule(
          resolved.rule, &materialSections, error, sizeof(error)))
    return false;
  if (materialSections.size() < cleanCount)
    materialSections.resize(cleanCount);
  void *mapped = il2cpp_array_new(s_eiemMaterialClass, cleanCount);
  if (!mapped) return false;
  void **items = (void **)((char *)mapped + IL2CPP_ARRAY_DATA);
  std::vector<void *> uniqueLzyMaterials;
  std::vector<void *> uniqueVfxMaterials;
  void *firstLzyMaterial = nullptr;
  for (size_t index = 0; index < cleanCount; ++index) {
    void *lzyMaterial = EiemVfxArrayItem(clean, index);
    if (!lzyMaterial) return false;

    // The native dash transaction has one VFX material.  Keep that exact
    // instance for slot 0 and for every repeated LZY material reference;
    // this is the common merged-clothes case and avoids needless clones and
    // per-frame property copies.  Only a genuinely different LZY material
    // needs a VFX clone.
    if (index == 0) {
      firstLzyMaterial = lzyMaterial;
      items[index] = templateMaterial;
      continue;
    }
    if (lzyMaterial == firstLzyMaterial) {
      items[index] = templateMaterial;
      continue;
    }
    size_t uniqueIndex = SIZE_MAX;
    for (size_t candidate = 0; candidate < uniqueLzyMaterials.size();
         ++candidate) {
      if (uniqueLzyMaterials[candidate] == lzyMaterial) {
        uniqueIndex = candidate;
        break;
      }
    }
    if (uniqueIndex == SIZE_MAX) {
      const std::vector<std::string> *slotProperties =
          index < properties.size() ? &properties[index] : nullptr;
      void *clone = EiemCloneVfxWithLzyTextures(
          templateMaterial, lzyMaterial, slotProperties);
      if (!clone) return false;
      size_t applied = 0;
      if (index < materialSections.size())
        EiemApplyLzyMaterialValueProperties(
            clone, resolved.rule, materialSections[index].c_str(), &applied);
      uniqueIndex = uniqueLzyMaterials.size();
      uniqueLzyMaterials.push_back(lzyMaterial);
      uniqueVfxMaterials.push_back(clone);
    }
    items[index] = uniqueVfxMaterials[uniqueIndex];
  }
  *outArray = mapped;
  if (outCount) *outCount = cleanCount;
  if (outCleanMaterials) *outCleanMaterials = clean;
  if (outTextureProperties) *outTextureProperties = std::move(properties);
  if (outMaterialSections) *outMaterialSections = std::move(materialSections);
  return true;
}

static size_t EiemSyncPerDrawVfxRuntimeProperties(
    const EiemPerDrawVfxSlotCache &cache, void *templateMaterial) {
  if (!cache.mappedMaterials || !cache.cleanMaterials || !templateMaterial)
    return 0;
  const size_t mappedCount = EiemManagedArrayLength(cache.mappedMaterials);
  const size_t cleanCount = EiemManagedArrayLength(cache.cleanMaterials);
  const size_t count = (std::min)(mappedCount, cleanCount);
  if (!count || count > 64) return 0;
  if (!g_material_CopyPropertiesFromMaterial) return 0;
  EiemResolvedRenderRule resolved = {};
  const bool hasRule = cache.info && EiemResolveVfxRenderRule(
      EiemRendererInfoRenderer(cache.info), &resolved);
  void **mappedItems = (void **)((char *)cache.mappedMaterials + IL2CPP_ARRAY_DATA);
  void *seen[64] = {};
  size_t seenCount = 0;
  size_t copied = 0;
  for (size_t index = 0; index < count; ++index) {
    // Slot 0 and repeated references intentionally point at the native
    // template.  The game already updated that object this frame; copying it
    // back to itself only adds a costly managed call.
    if (!mappedItems[index] || mappedItems[index] == templateMaterial) continue;
    bool duplicate = false;
    for (size_t seenIndex = 0; seenIndex < seenCount; ++seenIndex) {
      if (seen[seenIndex] == mappedItems[index]) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;
    if (seenCount < _countof(seen)) seen[seenCount++] = mappedItems[index];
    if (!EiemCopyVfxRuntimeMaterialProperties(mappedItems[index],
                                               templateMaterial))
      continue;
    void *lzyMaterial = EiemVfxArrayItem(cache.cleanMaterials, index);
    const std::vector<std::string> *properties =
        index < cache.textureProperties.size()
            ? &cache.textureProperties[index]
            : nullptr;
    // CopyPropertiesFromMaterial also copies the native VFX textures. Restore
    // the matched LZY slot textures immediately after the runtime-property
    // copy, so the native dissolve state and the mod's slot identity coexist.
    EiemCopyLzyTextureProperties(mappedItems[index], lzyMaterial, properties);
    if (hasRule && index < cache.materialSections.size())
      EiemApplyLzyMaterialValueProperties(
          mappedItems[index], resolved.rule,
          cache.materialSections[index].c_str(), nullptr);
    ++copied;
  }
  return copied;
}

#if defined(EIEM_PERDRAW_VFX_FANOUT_BUILD)
// The game's native per-draw method updates the material(s) in its private
// one-slot replacement transaction.  Extra clothes slots are our clones, so
// replay that same native method once per clone while temporarily pointing the
// transaction at a one-element array.  The original native array and flag are
// restored before returning; Renderer.sharedMaterials is re-expanded by the
// caller afterwards.  No character, renderer, slot or VFX name is embedded.
static size_t EiemFanoutNativePerDrawToVfxSlots(
    void *info, void *self, Vector4 value, void *methodInfo,
    EiemPerDrawVfxSlotCache *cache) {
  if (!info || !self || !cache || !cache->mappedMaterials ||
      !s_origRendererInfoCharacterPerDrawData || !s_eiemMaterialClass)
    return 0;
  const size_t count = EiemManagedArrayLength(cache->mappedMaterials);
  if (count <= 1 || count > 64) return 0;
  if (!cache->fanoutSingle) {
    cache->fanoutSingle = il2cpp_array_new(s_eiemMaterialClass, 1);
    if (!cache->fanoutSingle) return 0;
    if (il2cpp_gchandle_new)
      cache->fanoutHandle = il2cpp_gchandle_new(cache->fanoutSingle, false);
  }
  void **mappedItems = (void **)((char *)cache->mappedMaterials + IL2CPP_ARRAY_DATA);
  auto *singleItem = (void **)((char *)cache->fanoutSingle + IL2CPP_ARRAY_DATA);
  void *oldReplacing = nullptr;
  unsigned char oldReplacingFlag = 0;
  __try {
    oldReplacing = *(void **)((char *)info + 0x40);
    oldReplacingFlag = *(unsigned char *)((char *)info + 0x38);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
  size_t replayed = 0;
  void *seen[64] = {};
  size_t seenCount = 0;
  if (mappedItems[0]) seen[seenCount++] = mappedItems[0];
  for (size_t index = 1; index < count; ++index) {
    if (!mappedItems[index]) continue;
    bool duplicate = false;
    for (size_t seenIndex = 0; seenIndex < seenCount; ++seenIndex) {
      if (seen[seenIndex] == mappedItems[index]) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;
    if (seenCount < _countof(seen)) seen[seenCount++] = mappedItems[index];
    *singleItem = mappedItems[index];
    bool callOk = false;
    __try {
      *(void **)((char *)info + 0x40) = cache->fanoutSingle;
      *(unsigned char *)((char *)info + 0x38) = 1;
      auto original = (EiemCharacterPerDrawDataFn)
          s_origRendererInfoCharacterPerDrawData;
      callOk = original && original(self, value, methodInfo);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      callOk = false;
    }
    if (callOk) ++replayed;
  }
  __try {
    *(void **)((char *)info + 0x40) = oldReplacing;
    *(unsigned char *)((char *)info + 0x38) = oldReplacingFlag;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  static volatile LONG s_logs = 0;
  if (InterlockedIncrement(&s_logs) <= 32)
    Log("[MOD-PERDRAW-VFX-FANOUT] info=%p count=%zu replayed=%zu valueW=%.3f",
        info, count, replayed, (double)value.w);
  return replayed;
}
#endif

// Assigning the expanded VFX array can synchronously cause another
// RendererInfo material commit.  The outer commit owns the cache and the
// Unity setter; a nested EIEM apply would allocate/assign the same array again
// and can loop indefinitely on the first attack.  Let the nested native commit
// continue through its original hook, but skip a second EIEM apply transaction.
static thread_local LONG s_eiemPerDrawVfxApplyDepth = 0;
struct EiemPerDrawVfxApplyGuard {
  EiemPerDrawVfxApplyGuard() { ++s_eiemPerDrawVfxApplyDepth; }
  ~EiemPerDrawVfxApplyGuard() { --s_eiemPerDrawVfxApplyDepth; }
};

static bool EiemApplyPerDrawVfxSlotMaterials(void *info, void *renderer,
                                             void *templateOverride = nullptr) {
  if (!info || !renderer || !EiemOnUnityThread()) return false;
  const auto visibility = EiemReadVisibilityControllerProbeStateFast(renderer);
  if (!visibility.tracked || !visibility.materialTarget) return false;
  if (s_eiemPerDrawVfxApplyDepth > 0) {
    static volatile LONG s_reentryLogs = 0;
    const LONG logIndex = InterlockedIncrement(&s_reentryLogs);
    if (logIndex <= 32)
      Log("[MOD-VFX-REENTRY-SKIP] info=%p renderer=%p depth=%ld",
          info, renderer, s_eiemPerDrawVfxApplyDepth);
    return false;
  }
  EiemPerDrawVfxApplyGuard applyGuard;
  // This function holds cache-owned managed arrays and material metadata while
  // invoking Unity setters.  A nested end-of-dash callback may request cache
  // cleanup; defer that cleanup until this whole apply transaction returns.
  EiemVfxPatchGuard cacheGuard;
  const auto state = EiemReadRendererInfoMaterialState(info, renderer);
  // During the native end-of-dash cleanup, RendererInfo clears its private
  // replacingMaterials field before the public commit hook returns.  In that
  // narrow window the committed VFX object is still available as the hook's
  // input, so callers may provide it directly instead of relying on the
  // private field.  This keeps the replacement array expanded through the
  // final native material commit as well.
  void *templateMaterial = templateOverride
                               ? templateOverride
                               : EiemFindVfxTemplate(state.replacingMaterials,
                                                     true);
  if (!templateMaterial) return false;
  auto *cache = EiemFindPerDrawVfxSlotCache(info, true);
  if (!cache) return false;
#if defined(EIEM_SPRINT_TIMING_PROBE_BUILD)
  const bool cacheNeedsBuild = !cache->mappedMaterials;
  if (cacheNeedsBuild || cache->templateMaterial != templateMaterial)
    EiemSprintTimingProbeOnVfxTemplate(info, renderer, templateMaterial,
                                       cacheNeedsBuild ? "cache-build"
                                                       : "template-refresh");
  if (cacheNeedsBuild)
    EiemSprintTimingProbeOnCacheBuild(info, renderer, templateMaterial, true,
                                      false);
#endif
  if (!cache->mappedMaterials) {
    void *mapped = nullptr;
    size_t count = 0;
    void *cleanMaterials = nullptr;
    std::vector<std::vector<std::string>> textureProperties;
    std::vector<std::string> materialSections;
    if (!EiemBuildPerDrawVfxSlotArray(renderer, templateMaterial, &mapped,
                                      &count, &cleanMaterials,
                                      &textureProperties,
                                      &materialSections)) {
#if defined(EIEM_SPRINT_TIMING_PROBE_BUILD)
      EiemSprintTimingProbeOnCacheBuild(info, renderer, templateMaterial,
                                        false, false);
#endif
      return false;
    }
    cache->templateMaterial = templateMaterial;
    cache->cleanMaterials = cleanMaterials;
    cache->mappedMaterials = mapped;
    cache->count = count;
    cache->textureProperties = std::move(textureProperties);
    cache->materialSections = std::move(materialSections);
    if (il2cpp_gchandle_new && cleanMaterials)
      cache->cleanHandle = il2cpp_gchandle_new(cleanMaterials, false);
    if (il2cpp_gchandle_new)
      cache->mappedHandle = il2cpp_gchandle_new(mapped, false);
#if defined(EIEM_SPRINT_TIMING_PROBE_BUILD)
    EiemSprintTimingProbeOnCacheBuild(info, renderer, templateMaterial, false,
                                      cache->mappedMaterials != nullptr);
#endif
  } else if (cache->templateMaterial != templateMaterial) {
    // Native may allocate a fresh VFXInstance while the dash is active.  The
    // LZY rule and clone cache are unchanged, so update only references that
    // pointed at the previous native template.  Rebuilding here would reread
    // material files and allocate managed Materials on every frame.
    void *oldTemplate = cache->templateMaterial;
    cache->templateMaterial = templateMaterial;
    if (cache->mappedMaterials) {
      const size_t count = EiemManagedArrayLength(cache->mappedMaterials);
      if (count && count <= 64) {
        void **items =
            (void **)((char *)cache->mappedMaterials + IL2CPP_ARRAY_DATA);
        for (size_t index = 0; index < count; ++index)
          if (items[index] == oldTemplate) items[index] = templateMaterial;
      }
    }
  }
  const size_t synced = EiemSyncPerDrawVfxRuntimeProperties(
      *cache, templateMaterial);
  char error[256] = {};
#if defined(EIEM_NATIVE_VFX_SINGLE_RENDERER_AB_BUILD)
  // Keep the native one-slot VFX contract during the dash.  Assigning the
  // expanded clone array here is what made the body disappear while clothes
  // stayed on: the shader's dissolve state is driven by the native renderer
  // transaction, not by ordinary Material properties on every clone.
  void *single = il2cpp_array_new(s_eiemMaterialClass, 1);
  if (!single) return false;
  *(void **)((char *)single + IL2CPP_ARRAY_DATA) =
      *(void **)((char *)cache->mappedMaterials + IL2CPP_ARRAY_DATA);
  const bool assigned = EiemAssignRendererMaterials(
      renderer, single, error, sizeof(error));
  Log("[MOD-PERDRAW-VFX-SINGLE-AB] info=%p renderer=%p cleanCount=%zu "
      "runtimeSynced=%zu assigned=%d error=%s",
      info, renderer, cache->count, synced, assigned ? 1 : 0,
      error[0] ? error : "<none>");
  return assigned;
#else
  const bool assigned = EiemAssignRendererMaterials(
      renderer, cache->mappedMaterials, error, sizeof(error));
  static volatile LONG s_logs = 0;
  if (InterlockedIncrement(&s_logs) <= 32)
    Log("[MOD-PERDRAW-VFX-SLOTS] info=%p renderer=%p template=%p count=%zu "
        "runtimeSynced=%zu assigned=%d copyApi=%p error=%s",
        info, renderer, cache->templateMaterial, cache->count,
        synced, assigned ? 1 : 0, g_material_CopyPropertiesFromMaterial,
        error[0] ? error : "<none>");
  return assigned;
#endif
}

// Called from the native material-commit hooks, immediately after the game
// has created/replaced its VFXInstance.  This moves clone allocation and the
// first texture/property copy out of the first TrySetCharacterPerDrawData
// callback.  The per-draw path still reuses this cache and refreshes only the
// runtime values that the native controller animates.
static bool EiemPrewarmNativeVfxSlotMaterials(void *info, void *renderer,
                                              void *nativeInput,
                                              bool inputIsArray) {
  if (!info || !renderer || !EiemOnUnityThread()) return false;
  const auto visibility = EiemReadVisibilityControllerProbeStateFast(renderer);
  if (!visibility.tracked || !visibility.materialTarget) return false;
  const auto state = EiemReadRendererInfoMaterialState(info, renderer);
  if (!state.fieldsRead) return false;
  void *templateMaterial =
      EiemFindVfxTemplate(nativeInput, inputIsArray);
  if (!templateMaterial)
    templateMaterial = EiemFindVfxTemplate(state.replacingMaterials, true);
  if (!templateMaterial)
    templateMaterial = EiemFindVfxTemplate(state.currentMaterials, true);
  if (!templateMaterial)
    return false;
  const bool assigned = EiemApplyPerDrawVfxSlotMaterials(
      info, renderer, templateMaterial);
  if (assigned) {
    auto *cache = EiemFindPerDrawVfxSlotCache(info, false);
    static volatile LONG s_prewarmLogs = 0;
    const LONG logIndex = InterlockedIncrement(&s_prewarmLogs);
    if (logIndex <= 64)
      Log("[MOD-VFX-PREWARM] info=%p renderer=%p input=%p inputArray=%d "
          "template=%p cleanCount=%zu assigned=1",
          info, renderer, nativeInput, inputIsArray ? 1 : 0, templateMaterial,
          cache ? cache->count : 0u);
  }
  return assigned;
}
#endif

#if defined(EIEM_PERDRAW_SYNC_PROBE_BUILD)
// Temporary A/B only: give the game's own RendererInfo the material array
// already installed by EIEM.  The old link probe wrote Renderer.sharedMaterials
// directly into the controller's replacement field.  That made the native
// sprint VFX material permanent.  This probe uses the game's own
// TryReplaceSharedMaterials method at sprint start and restores the clean
// replacement array at sprint end.
struct EiemPerDrawSyncEntry {
  void *info = nullptr;
  bool linked = false;
};
static EiemPerDrawSyncEntry s_eiemPerDrawSyncInfos[256] = {};
static volatile LONG s_eiemPerDrawSyncInfoCount = 0;

static bool EiemPerDrawSyncFind(void *info, LONG *slotOut,
                                bool allocate) {
  if (slotOut) *slotOut = -1;
  if (!info) return false;
  for (LONG i = 0; i < (LONG)_countof(s_eiemPerDrawSyncInfos); ++i) {
    if (s_eiemPerDrawSyncInfos[i].info == info) {
      if (slotOut) *slotOut = i;
      return true;
    }
  }
  if (!allocate) return false;
  const LONG index = InterlockedIncrement(&s_eiemPerDrawSyncInfoCount) - 1;
  if (index < 0 || index >= (LONG)_countof(s_eiemPerDrawSyncInfos)) return false;
  s_eiemPerDrawSyncInfos[index].info = info;
  if (slotOut) *slotOut = index;
  return true;
}

static void *EiemPerDrawSyncArrayItem(void *array, size_t index) {
  if (!array || index > 4096) return nullptr;
  __try {
    const size_t count = EiemManagedArrayLength(array);
    if (index >= count) return nullptr;
    return *(void **)((char *)array + 32 + index * sizeof(void *));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

static size_t EiemPerDrawSyncReplacementSubMeshCount(void *renderer) {
  if (!renderer || !g_mesh_get_subMeshCount) return 0;
  void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
  if (!mesh) return 0;
  __try {
    void *boxed = Invoke(g_mesh_get_subMeshCount, mesh);
    if (!boxed) return 0;
    const int32_t count = *(int32_t *)((char *)boxed + 16);
    return count > 0 && count <= 64 ? (size_t)count : 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

static bool EiemPerDrawRestoreCleanMaterials(void *info, void *renderer,
                                             void *methodInfo) {
  if (!info || !renderer) return false;
  const auto before = EiemReadRendererInfoMaterialState(info, renderer);
  void *current = before.currentMaterials;
  const size_t currentCount = before.currentCount;
  size_t cleanCount = EiemPerDrawSyncReplacementSubMeshCount(renderer);
  if (!cleanCount) cleanCount = before.sourceCount;
  if (!current || !cleanCount || currentCount < cleanCount ||
      !s_eiemMaterialClass || !s_eiemRendererSetSharedMaterials)
    return false;

  // The game appends its dash material(s) after the replacement mesh's
  // submesh slots.  Keep only the mesh slots that EFF actually owns.
  void *clean = il2cpp_array_new(s_eiemMaterialClass, cleanCount);
  if (!clean) return false;
  void **items = (void **)((char *)clean + IL2CPP_ARRAY_DATA);
  for (size_t i = 0; i < cleanCount; ++i)
    items[i] = EiemPerDrawSyncArrayItem(current, i);
  if (!items[0]) return false;
  for (size_t i = 1; i < cleanCount; ++i)
    if (!items[i]) items[i] = items[0];

  void *params[] = {clean};
  bool assigned = false;
  __try {
    Invoke(s_eiemRendererSetSharedMaterials, renderer, params);
    assigned = EiemManagedObjectArraySame(
        clean, g_renderer_get_sharedMaterials
                       ? Invoke(g_renderer_get_sharedMaterials, renderer)
                       : nullptr);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    assigned = false;
  }

  // TryReplaceSharedMaterials has no public clear operation in this build.
  // Its verified idle state is materialReplacing=0 and a null replacing array.
  // Restore exactly that state after the native call has consumed the sprint
  // value; this avoids retaining the VFX material in the next frame.
  bool cleared = false;
  __try {
    *(void **)((char *)info + 0x40) = nullptr;
    *(unsigned char *)((char *)info + 0x38) = 0;
    cleared = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    cleared = false;
  }
  const auto after = EiemReadRendererInfoMaterialState(info, renderer);
  Log("[PERDRAW-SYNC-RESTORE] info=%p renderer=%p clean=%p/%zu assigned=%d "
      "cleared=%d beforeCurrent=%p/%zu afterCurrent=%p/%zu "
      "afterReplacing=%p/%zu materialReplacing=%d",
      info, renderer, clean, cleanCount, assigned ? 1 : 0, cleared ? 1 : 0,
      current, currentCount, after.currentMaterials, after.currentCount,
      after.replacingMaterials, after.replacingCount,
      after.materialReplacing ? 1 : 0);
  (void)methodInfo;
  return assigned && cleared;
}

static void EiemPerDrawSyncReplacementMaterials(void *info, Vector4 requested,
                                                void *methodInfo) {
  if (!info || !s_origRendererInfoTryReplaceSharedMaterials)
    return;
  void *renderer = EiemRendererInfoRenderer(info);
  const auto visibility = EiemReadVisibilityControllerProbeState(renderer);
  if (!visibility.tracked) return;
  LONG slot = -1;
  if (!EiemPerDrawSyncFind(info, &slot, requested.w > 0.49f && requested.w < 0.51f))
    return;
  if (slot < 0 || slot >= (LONG)_countof(s_eiemPerDrawSyncInfos)) return;
  if (requested.w <= 0.01f) {
    if (s_eiemPerDrawSyncInfos[slot].linked) {
      EiemPerDrawRestoreCleanMaterials(info, renderer, methodInfo);
      s_eiemPerDrawSyncInfos[slot].linked = false;
    }
    return;
  }
  if (requested.w < 0.49f || requested.w > 0.51f ||
      s_eiemPerDrawSyncInfos[slot].linked)
    return;
  const auto materials = EiemReadRendererInfoMaterialState(info, renderer);
  if (!materials.currentMaterials ||
      EiemManagedObjectArraySame(materials.currentMaterials,
                                 materials.replacingMaterials))
    return;
  auto original = (TraceRendererInfoMaterialCommitFn)
      s_origRendererInfoTryReplaceSharedMaterials;
  const bool result = original(info, materials.currentMaterials, methodInfo);
  const auto after = EiemReadRendererInfoMaterialState(info, renderer);
  Log("[PERDRAW-SYNC-PROBE] info=%p renderer=%p section=%s requestedW=%.3f "
      "result=%d current=%p/%zu replacing=%p/%zu materialReplacing=%d",
      info, renderer, visibility.section[0] ? visibility.section : "<none>",
      (double)requested.w, result ? 1 : 0, after.currentMaterials,
      after.currentCount, after.replacingMaterials, after.replacingCount,
      after.materialReplacing ? 1 : 0);
  if (result) s_eiemPerDrawSyncInfos[slot].linked = true;
}
#endif

#if defined(EIEM_PERDRAW_MATERIAL_PROBE_BUILD)
// Read-only snapshot of the material/shader objects actually attached to a
// tracked replacement renderer at the moment the dissolve per-draw value is
// received.  This answers whether EIEM's material has the same shader as the
// game's source material; it never changes material or renderer state.
static void *s_eiemPerDrawMaterialProbeInfos[256] = {};
static volatile LONG s_eiemPerDrawMaterialProbeCount = 0;

static void *EiemPerDrawMaterialProbeArrayItem(void *array, size_t index) {
  if (!array || index > 4096) return nullptr;
  __try {
    const size_t count = *(size_t *)((char *)array + 24);
    if (index >= count) return nullptr;
    return *(void **)((char *)array + 32 + index * sizeof(void *));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

static bool EiemPerDrawMaterialProbeAlreadyLogged(void *info) {
  if (!info) return true;
  for (size_t i = 0; i < _countof(s_eiemPerDrawMaterialProbeInfos); ++i) {
    if (s_eiemPerDrawMaterialProbeInfos[i] == info) return true;
  }
  const LONG index = InterlockedIncrement(&s_eiemPerDrawMaterialProbeCount) - 1;
  if (index < 0 || index >= (LONG)_countof(s_eiemPerDrawMaterialProbeInfos))
    return true;
  s_eiemPerDrawMaterialProbeInfos[index] = info;
  return false;
}

static void EiemLogPerDrawMaterialProbeArray(const char *section,
                                             const char *arrayName,
                                             void *array) {
  const size_t count = EiemManagedArrayLength(array);
  const size_t bounded = count > 12 ? 12 : count;
  Log("[PERDRAW-MATERIAL] section=%s array=%s ptr=%p count=%zu",
      section ? section : "<none>", arrayName ? arrayName : "<none>",
      array, count);
  for (size_t index = 0; index < bounded; ++index) {
    void *material = EiemPerDrawMaterialProbeArrayItem(array, index);
    char materialName[160] = {};
    char shaderName[160] = {};
    void *shader = nullptr;
    if (material) {
      TraceReadUnityObjectName(material, materialName, sizeof(materialName));
      if (g_material_get_shader) {
        __try { shader = Invoke(g_material_get_shader, material); }
        __except (EXCEPTION_EXECUTE_HANDLER) { shader = nullptr; }
      }
    }
    if (shader) TraceReadUnityObjectName(shader, shaderName, sizeof(shaderName));
    Log("[PERDRAW-MATERIAL] section=%s array=%s slot=%zu material=%p "
        "materialName=%s shader=%p shaderName=%s",
        section ? section : "<none>", arrayName ? arrayName : "<none>",
        index, material, materialName[0] ? materialName : "<unnamed>",
        shader, shaderName[0] ? shaderName : "<unnamed>");
  }
}

static void EiemLogPerDrawMaterialProbe(void *info, const char *section,
                                        const EiemPerDrawMaterialState &state) {
  if (!info || !state.fieldsRead || EiemPerDrawMaterialProbeAlreadyLogged(info))
    return;
  EiemLogPerDrawMaterialProbeArray(section, "source", state.sourceMaterials);
  EiemLogPerDrawMaterialProbeArray(section, "replacing", state.replacingMaterials);
  EiemLogPerDrawMaterialProbeArray(section, "current", state.currentMaterials);
}
#endif

#if defined(EIEM_PERDRAW_REPLACING_LINK_PROBE_BUILD)
// Diagnostic only: RendererInfo keeps a managed reference to the material
// array used by its per-draw path.  EFF replaces Renderer.sharedMaterials
// after RendererInfo has initialized, so the controller can continue to read
// the old one-slot game array while Unity renders EIEM's expanded array.  This
// probe links the controller field to the current Renderer array for tracked
// renderers, without touching sourceMaterials or invoking the native replace
// method (which was observed to truncate the array back to the game's layout).
static void *s_eiemPerDrawReplacingLinkInfos[256] = {};
static uint32_t s_eiemPerDrawReplacingLinkHandles[256] = {};
static volatile LONG s_eiemPerDrawReplacingLinkCount = 0;

static bool EiemPerDrawReplacingLinkFindOrAdd(void *info, LONG *slotOut) {
  if (slotOut) *slotOut = -1;
  if (!info) return false;
  for (LONG i = 0; i < (LONG)_countof(s_eiemPerDrawReplacingLinkInfos); ++i) {
    if (s_eiemPerDrawReplacingLinkInfos[i] == info) {
      if (slotOut) *slotOut = i;
      return true;
    }
  }
  const LONG index = InterlockedIncrement(&s_eiemPerDrawReplacingLinkCount) - 1;
  if (index < 0 || index >= (LONG)_countof(s_eiemPerDrawReplacingLinkInfos))
    return false;
  s_eiemPerDrawReplacingLinkInfos[index] = info;
  if (slotOut) *slotOut = index;
  return true;
}

static void EiemPerDrawLinkReplacingMaterials(void *info, Vector4 requested) {
  if (!info || requested.w < 0.49f || requested.w > 0.51f) return;
  void *renderer = EiemRendererInfoRenderer(info);
  const auto visibility = EiemReadVisibilityControllerProbeState(renderer);
  if (!visibility.tracked) return;

  LONG slot = -1;
  if (!EiemPerDrawReplacingLinkFindOrAdd(info, &slot)) return;
  const auto before = EiemReadRendererInfoMaterialState(info, renderer);
  void *current = before.currentMaterials;
  if (!current || !before.fieldsRead ||
      EiemManagedArrayLength(current) == 0) {
    Log("[PERDRAW-LINK-PROBE] info=%p renderer=%p section=%s action=skip reason=no-current-array",
        info, renderer, visibility.section[0] ? visibility.section : "<none>");
    return;
  }

  // Renderer.sharedMaterials returns a managed array copy on this game build,
  // so comparing the managed-array pointer would relink every frame.  Compare
  // the material contents instead and only replace RendererInfo's field when
  // the game's current array has actually gained/lost/changed a slot.
  if (EiemManagedObjectArraySame(current, before.replacingMaterials)) return;

  const uint32_t previousHandle =
      (slot >= 0 && slot < (LONG)_countof(s_eiemPerDrawReplacingLinkHandles))
          ? s_eiemPerDrawReplacingLinkHandles[slot]
          : 0;

  // Keep the managed array alive while RendererInfo points at it.  The getter
  // may return a managed copy, and this diagnostic direct field write is
  // intentionally outside normal IL2CPP write-barrier APIs.
  uint32_t handle = 0;
  if (il2cpp_gchandle_new)
    handle = il2cpp_gchandle_new(current, false);
  if (slot >= 0 && slot < (LONG)_countof(s_eiemPerDrawReplacingLinkHandles)) {
    s_eiemPerDrawReplacingLinkHandles[slot] = handle;
    if (previousHandle && il2cpp_gchandle_free)
      il2cpp_gchandle_free(previousHandle);
  }

  bool written = false;
  __try {
    *(void **)((char *)info + 0x40) = current;
    *(unsigned char *)((char *)info + 0x38) = 1;
    written = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    written = false;
  }
  const auto after = EiemReadRendererInfoMaterialState(info, renderer);
  Log("[PERDRAW-LINK-PROBE] info=%p renderer=%p section=%s requestedW=%.3f "
      "action=link written=%d handle=%u oldCount=%zu newCount=%zu "
      "currentCount=%zu materialReplacing=%d",
      info, renderer, visibility.section[0] ? visibility.section : "<none>",
      (double)requested.w, written ? 1 : 0, handle, before.replacingCount,
      after.replacingCount, after.currentCount,
      after.materialReplacing ? 1 : 0);
}
#endif

static void EiemLogCharacterPerDrawData(const char *phase, void *info,
                                        Vector4 requested, bool resultKnown,
                                        bool result) {
#if !defined(EIEM_HIGH_FREQUENCY_DIAGNOSTICS_BUILD)
  (void)phase;
  (void)info;
  (void)requested;
  (void)resultKnown;
  (void)result;
  return;
#else
  if (!info) return;
  void *renderer = EiemRendererInfoRenderer(info);
  const auto state = EiemReadVisibilityControllerProbeState(renderer);
  char rendererName[160] = {};
  char hierarchy[768] = {};
  if (renderer) {
    TraceReadUnityObjectName(renderer, rendererName, sizeof(rendererName));
    TraceBuildRendererHierarchy(renderer, hierarchy, sizeof(hierarchy));
  }
  if (!state.tracked) return;
  Vector4 stored = {};
  const bool storedRead = EiemReadRendererInfoPerDrawData(info, &stored);
  const auto materials = EiemReadRendererInfoMaterialState(info, renderer);
  const bool sourceMatchesCurrent =
      materials.fieldsRead &&
      EiemManagedObjectArraySame(materials.sourceMaterials,
                                 materials.currentMaterials);
  const bool replacingMatchesCurrent =
      materials.fieldsRead &&
      EiemManagedObjectArraySame(materials.replacingMaterials,
                                 materials.currentMaterials);
  Log("[PERDRAW-PROBE] phase=%s info=%p renderer=%p tracked=%d section=%s "
      "requested=(%.6f,%.6f,%.6f,%.6f) stored=%s "
      "storedValue=(%.6f,%.6f,%.6f,%.6f) result=%s enabled=%d visible=%d "
      "forceRenderingOff=%d active=%d materialReplacing=%d "
      "sourceMaterials=%p/%zu replacingMaterials=%p/%zu currentMaterials=%p/%zu "
      "sourceMatchesCurrent=%d replacingMatchesCurrent=%d rendererName=%s "
      "hierarchy=%s",
      phase ? phase : "?", info, renderer, state.tracked ? 1 : 0,
      state.section[0] ? state.section : "<none>",
      (double)requested.x, (double)requested.y, (double)requested.z,
      (double)requested.w, storedRead ? "yes" : "no", (double)stored.x,
      (double)stored.y, (double)stored.z, (double)stored.w,
      resultKnown ? (result ? "true" : "false") : "<none>",
      state.enabled ? 1 : 0, state.visible ? 1 : 0,
      state.forceOff ? 1 : 0, state.active ? 1 : 0,
      materials.materialReplacing ? 1 : 0, materials.sourceMaterials,
      materials.sourceCount, materials.replacingMaterials,
      materials.replacingCount, materials.currentMaterials,
      materials.currentCount, sourceMatchesCurrent ? 1 : 0,
      replacingMatchesCurrent ? 1 : 0,
      rendererName[0] ? rendererName : "<none>",
      hierarchy[0] ? hierarchy : "<none>");
#endif
}

#if defined(EIEM_AFTERIMAGE_DIAGNOSTICS_BUILD)
// The general A/B renderer probe intentionally ignores idle values.  A native
// afterimage may use a zero w component while still changing the other
// per-draw channels, so keep a small body/cloth Shadow_Proxy-only probe that
// records both sides of the native callback.
struct EiemAfterimagePerDrawProbeEntry {
  void *info = nullptr;
  LONG generation = -1;
  uint8_t attempts = 0;
};
static EiemAfterimagePerDrawProbeEntry
    s_eiemAfterimagePerDrawProbeEntries[128] = {};

static bool EiemAfterimagePerDrawProbeShouldLog(void *info,
                                                uint8_t *attempts) {
  if (!info || !attempts) return false;
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  constexpr size_t kProbeTableSize =
      _countof(s_eiemAfterimagePerDrawProbeEntries);
  const size_t start =
      (static_cast<size_t>(reinterpret_cast<uintptr_t>(info) >> 4)) &
      (kProbeTableSize - 1);
  for (size_t probe = 0; probe < 16; ++probe) {
    auto &entry = s_eiemAfterimagePerDrawProbeEntries[
        (start + probe) & (kProbeTableSize - 1)];
    if (entry.info == info) {
      if (entry.generation != generation) {
        entry.generation = generation;
        entry.attempts = 0;
      }
      if (entry.attempts >= 16) return false;
      ++entry.attempts;
      *attempts = entry.attempts;
      return true;
    }
    if (!entry.info) {
      entry.info = info;
      entry.generation = generation;
      entry.attempts = 1;
      *attempts = 1;
      return true;
    }
  }
  return false;
}

static void EiemLogAfterimagePerDrawProxyProbe(const char *phase, void *info,
                                               void *renderer,
                                               Vector4 value) {
  if (!info || !renderer || g_shutdownRequested) return;
  char rendererName[160] = {};
  char hierarchy[768] = {};
  TraceReadUnityObjectName(renderer, rendererName, sizeof(rendererName));
  TraceBuildRendererHierarchy(renderer, hierarchy, sizeof(hierarchy));
  const bool shadowProxy = strstr(hierarchy, "Shadow_Proxy") != nullptr ||
                           strstr(rendererName, "shadowProxy") != nullptr;
  const bool bodyOrCloth =
      strstr(rendererName, "body_01") || strstr(rendererName, "cloth_01") ||
      strstr(rendererName, "cloth_02") || strstr(rendererName, "cloth_03") ||
      strstr(rendererName, "cloth_04") || strstr(rendererName, "cloth_05") ||
      strstr(rendererName, "cloth_06") || strstr(hierarchy, "body_01") ||
      strstr(hierarchy, "cloth_01") || strstr(hierarchy, "cloth_02") ||
      strstr(hierarchy, "cloth_03") || strstr(hierarchy, "cloth_04") ||
      strstr(hierarchy, "cloth_05") || strstr(hierarchy, "cloth_06");
  if (!shadowProxy || !bodyOrCloth) return;
  uint8_t attempts = 0;
  if (!EiemAfterimagePerDrawProbeShouldLog(info, &attempts)) return;

  const auto visibility = EiemReadVisibilityControllerProbeState(renderer);
  const auto materials = EiemReadRendererInfoMaterialState(info, renderer);
  void *mesh = EiemReadSharedMesh(
      renderer, EiemIsSkinnedRenderer(renderer) ? "SkinnedMeshRenderer"
                                                  : "MeshFilter");
  char meshName[192] = {};
  if (mesh) TraceReadUnityObjectName(mesh, meshName, sizeof(meshName));
  char currentNames[768] = {};
  char replacingNames[768] = {};
  auto summarize = [](void *array, char *out, size_t outSize) {
    if (!out || !outSize) return;
    out[0] = '\0';
    const size_t count = EiemManagedArrayLength(array);
    const size_t bounded = count > 8 ? 8 : count;
    size_t used = 0;
    for (size_t index = 0; index < bounded; ++index) {
      void *material = EiemVfxArrayItem(array, index);
      char name[128] = {};
      if (material)
        TraceReadUnityObjectName(material, name, sizeof(name));
      const int written = _snprintf_s(
          out + used, outSize - used, _TRUNCATE, "%s%zu:%s",
          index ? "," : "", index, name[0] ? name : "<none>");
      if (written <= 0) break;
      used += static_cast<size_t>(written);
      if (used + 24 >= outSize) break;
    }
  };
  summarize(materials.currentMaterials, currentNames, sizeof(currentNames));
  summarize(materials.replacingMaterials, replacingNames,
            sizeof(replacingNames));
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  Log("[AFTERIMAGE-PERDRAW-PROBE] phase=%s generation=%ld attempt=%u "
      "info=%p renderer=%p tracked=%d section=%s value=(%.4f,%.4f,%.4f,%.4f) "
      "mesh=%p meshName=%s replacement=%p meshSame=%d enabled=%d visible=%d "
      "forceOff=%d active=%d materialReplacing=%d current=%s replacing=%s "
      "rendererName=%s hierarchy=%s",
      phase ? phase : "?", generation, (unsigned)attempts, info, renderer,
      visibility.tracked ? 1 : 0,
      visibility.section[0] ? visibility.section : "<none>",
      (double)value.x, (double)value.y, (double)value.z, (double)value.w,
      mesh, meshName[0] ? meshName : "<none>", visibility.replacementMesh,
      visibility.replacementMesh && mesh == visibility.replacementMesh ? 1 : 0,
      visibility.enabled ? 1 : 0, visibility.visible ? 1 : 0,
      visibility.forceOff ? 1 : 0, visibility.active ? 1 : 0,
      materials.materialReplacing ? 1 : 0,
      currentNames[0] ? currentNames : "<none>",
      replacingNames[0] ? replacingNames : "<none>",
      rendererName[0] ? rendererName : "<none>",
      hierarchy[0] ? hierarchy : "<none>");
}
#endif

// A/B probe for the LZY body/clothes afterimage path.  The normal per-draw
// diagnostic intentionally reports only tracked renderers; that hides the
// original-model side of an A/B run because an unmodified body has no EIEM
// ledger entry.  This probe observes each RendererInfo once (with a small
// retry allowance while Unity finishes assigning its mesh) and records both
// tracked and untracked targets without changing any state.
struct EiemAbRendererProbeEntry {
  void *info = nullptr;
  LONG generation = -1;
  uint8_t attempts = 0;
};
static EiemAbRendererProbeEntry s_eiemAbRendererProbeEntries[512] = {};

static bool EiemAbRendererProbeShouldInspect(void *info, uint8_t *attempts) {
  if (!info || !attempts) return false;
  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  constexpr size_t kProbeTableSize = _countof(s_eiemAbRendererProbeEntries);
  const size_t start =
      (static_cast<size_t>(reinterpret_cast<uintptr_t>(info) >> 4)) &
      (kProbeTableSize - 1);
  // This hook is on a draw hot path.  Use a short open-addressed lookup
  // instead of scanning the whole diagnostic table on every frame.
  for (size_t probe = 0; probe < 16; ++probe) {
    auto &entry = s_eiemAbRendererProbeEntries[(start + probe) &
                                                (kProbeTableSize - 1)];
    if (entry.info == info) {
      // F10 publishes a new mod generation while the same short-lived
      // RendererInfo address can be reused.  Start a fresh A/B sample set so
      // the enabled and commented-ini attacks can be compared in one run.
      if (entry.generation != generation) {
        entry.generation = generation;
        entry.attempts = 0;
      }
      if (entry.attempts >= 12) return false;
      ++entry.attempts;
      *attempts = entry.attempts;
      return true;
    }
    if (!entry.info) {
      entry.info = info;
      entry.generation = generation;
      entry.attempts = 1;
      *attempts = 1;
      return true;
    }
  }
  return false;
}

static void EiemLogAbRendererProbe(void *info, void *renderer,
                                   Vector4 value, const char *phase) {
#if !defined(EIEM_AFTERIMAGE_DIAGNOSTICS_BUILD)
  (void)info;
  (void)renderer;
  (void)value;
  (void)phase;
  return;
#else
  // The comparison concerns a visible afterimage.  Skip idle per-draw calls
  // so the probe adds no frame-time work between attacks.
  if (value.w <= 0.01f) return;
  uint8_t attempts = 0;
  if (!EiemAbRendererProbeShouldInspect(info, &attempts) || !renderer) return;

  char rendererName[160] = {};
  char hierarchy[768] = {};
  char meshName[192] = {};
  char originPath[768] = {};
  int64_t originHash = 0;
  TraceReadUnityObjectName(renderer, rendererName, sizeof(rendererName));
  TraceBuildRendererHierarchy(renderer, hierarchy, sizeof(hierarchy));
  void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
  if (!mesh) mesh = EiemReadSharedMesh(renderer, "MeshFilter");
  TraceReadUnityObjectName(mesh, meshName, sizeof(meshName));
  if (mesh)
    TraceLookupAssetOrigin(mesh, &originHash, originPath, sizeof(originPath));
  const bool target = strstr(rendererName, "lizhiyan") != nullptr ||
                      strstr(hierarchy, "lizhiyan") != nullptr ||
                      strstr(meshName, "lizhiyan") != nullptr ||
                      strstr(originPath, "lizhiyan") != nullptr;
  if (!target) return;

  const auto visibility = EiemReadVisibilityControllerProbeState(renderer);
  const auto materials = EiemReadRendererInfoMaterialState(info, renderer);
  EiemModRule rendererRule = {};
  EiemModRule meshRule = {};
  const bool rendererRuleHit = EiemFindBoundRenderRule(renderer, &rendererRule);
  const bool meshRuleHit = mesh && EiemFindRenderRuleForMeshBinding(mesh, &meshRule);
  bool enabled = true;
  bool forceOff = false;
  EiemReadRendererEnabled(renderer, &enabled);
  EiemReadRendererForceRenderingOff(renderer, &forceOff);

  char materialNames[8][128] = {};
  const size_t materialCount =
      std::min<size_t>(materials.currentCount, _countof(materialNames));
  for (size_t index = 0; index < materialCount; ++index) {
    void *material = EiemVfxArrayItem(materials.currentMaterials, index);
    if (material)
      TraceReadUnityObjectName(material, materialNames[index],
                               sizeof(materialNames[index]));
  }
  auto summarizeMaterials = [&](void *array, char *out, size_t outSize) {
    if (!out || !outSize) return;
    out[0] = '\0';
    const size_t count = EiemManagedArrayLength(array);
    const size_t bounded = count > 8 ? 8 : count;
    size_t offset = 0;
    for (size_t index = 0;
         index < bounded && offset + 4 < outSize; ++index) {
      void *material = EiemVfxArrayItem(array, index);
      char name[128] = {};
      if (material)
        TraceReadUnityObjectName(material, name, sizeof(name));
      const char *label = name[0] ? name : "<none>";
      int written = _snprintf_s(out + offset, outSize - offset, _TRUNCATE,
                                "%s%zu:%s", index ? "," : "", index,
                                label);
      if (written <= 0) break;
      offset += (size_t)written;
    }
    if (!out[0] && count) strncpy_s(out, outSize, "<unreadable>", _TRUNCATE);
  };
  char sourceSummary[512] = {};
  char replacingSummary[512] = {};
  char currentSummary[512] = {};
  summarizeMaterials(materials.sourceMaterials, sourceSummary,
                     sizeof(sourceSummary));
  summarizeMaterials(materials.replacingMaterials, replacingSummary,
                     sizeof(replacingSummary));
  summarizeMaterials(materials.currentMaterials, currentSummary,
                     sizeof(currentSummary));

  const LONG generation =
      InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
  Log("[AB-RENDERER-PROBE] phase=%s generation=%ld attempt=%u info=%p renderer=%p "
      "tracked=%d materialTarget=%d section=%s mesh=%p meshName=%s originHash=%lld "
      "originPath=%s replacement=%p meshSame=%d enabled=%d visible=%d "
      "forceOff=%d active=%d value=(%.3f,%.3f,%.3f,%.3f) "
      "rendererRuleHit=%d rendererRule=%s meshRuleHit=%d meshRule=%s "
      "current=%p/%zu replacing=%p/%zu source=%p/%zu materialReplacing=%d "
      "sourceMaterials=%s replacingMaterials=%s currentMaterials=%s "
      "rendererName=%s "
      "hierarchy=%s",
      phase ? phase : "?", generation, (unsigned)attempts, info, renderer,
      visibility.tracked ? 1 : 0, visibility.materialTarget ? 1 : 0,
      visibility.section[0] ? visibility.section : "<none>", mesh,
      meshName[0] ? meshName : "<none>", (long long)originHash,
      originPath[0] ? originPath : "<none>", visibility.replacementMesh,
      visibility.replacementMesh && mesh == visibility.replacementMesh ? 1 : 0,
      enabled ? 1 : 0, visibility.visible ? 1 : 0, forceOff ? 1 : 0,
      visibility.active ? 1 : 0, (double)value.x, (double)value.y,
      (double)value.z, (double)value.w, rendererRuleHit ? 1 : 0,
      rendererRule.section[0] ? rendererRule.section : "<none>",
      meshRuleHit ? 1 : 0, meshRule.section[0] ? meshRule.section : "<none>",
      materials.currentMaterials,
      materials.currentCount, materials.replacingMaterials,
      materials.replacingCount, materials.sourceMaterials, materials.sourceCount,
      materials.materialReplacing ? 1 : 0,
      sourceSummary[0] ? sourceSummary : "<none>",
      replacingSummary[0] ? replacingSummary : "<none>",
      currentSummary[0] ? currentSummary : "<none>",
      rendererName[0] ? rendererName : "<none>",
      hierarchy[0] ? hierarchy : "<none>");
#endif
}

static thread_local bool s_eiemPerDrawHookActive = false;

static bool TraceRendererInfoCharacterPerDrawData(void *self, Vector4 value,
                                                  void *methodInfo) {
  auto original = (EiemCharacterPerDrawDataFn)
      s_origRendererInfoCharacterPerDrawData;
  if (s_eiemPerDrawHookActive)
    return original ? original(self, value, methodInfo) : false;
  s_eiemPerDrawHookActive = true;
  struct EiemPerDrawHookGuard {
    ~EiemPerDrawHookGuard() { s_eiemPerDrawHookActive = false; }
  } guard;
#if defined(EIEM_PERDRAW_END_RESTORE_BUILD)
  void *perDrawRenderer = EiemRendererInfoRenderer(self);
  const auto perDrawVisibility =
      EiemReadVisibilityControllerProbeStateFast(perDrawRenderer);
#if defined(EIEM_SPRINT_TIMING_PROBE_BUILD)
  if (perDrawVisibility.tracked)
    EiemSprintTimingProbeOnPerDraw(self, perDrawRenderer, value.w);
#endif
  // If the preceding idle frame exposed EIEM's expanded source slot table,
  // restore the exact one-slot table before the game's own VFX transaction.
  // Native RendererInfo code must continue to see its original layout.
  if (perDrawVisibility.tracked)
    EiemPerDrawRestoreNativeSource(self);
  // The Mesh guard must run before the game's draw-state update.  The native
  // method is still called unchanged, so its dissolve/Fresnel animation and
  // VFX lifetime remain authoritative.
  if (perDrawVisibility.tracked && value.w > 0.01f)
    EiemPerDrawEnsureReplacementMesh(perDrawRenderer, perDrawVisibility);
#endif
#if defined(EIEM_AFTERIMAGE_DIAGNOSTICS_BUILD)
  EiemLogAfterimagePerDrawProxyProbe("before", self, perDrawRenderer, value);
#endif
#if defined(EIEM_PERDRAW_REPLACING_LINK_PROBE_BUILD)
  EiemPerDrawLinkReplacingMaterials(self, value);
#endif
#if defined(EIEM_PERDRAW_SYNC_PROBE_BUILD)
  EiemPerDrawSyncReplacementMaterials(self, value, methodInfo);
#endif
  EiemLogCharacterPerDrawData("before", self, value, false, false);
  const bool result = original ? original(self, value, methodInfo) : false;
#if defined(EIEM_AFTERIMAGE_DIAGNOSTICS_BUILD)
  EiemLogAfterimagePerDrawProxyProbe("after-native", self, perDrawRenderer,
                                     value);
#endif
#if defined(EIEM_PERDRAW_END_RESTORE_BUILD)
  // The native per-draw transaction can write its source Mesh back while it
  // updates the temporary VFX material.  The pre-call guard alone is not
  // sufficient: re-assert the bound EFF replacement after the native call so
  // every submesh in the replacement Mesh remains present.
  const auto afterNativeVisibility =
      EiemReadVisibilityControllerProbeStateFast(perDrawRenderer);
  if (perDrawVisibility.tracked && value.w > 0.01f)
    EiemPerDrawEnsureReplacementMesh(perDrawRenderer, afterNativeVisibility);
  EiemLogAbRendererProbe(self, perDrawRenderer, value, "after-native");
  // Native has now updated the one VFX template's dissolve/Fresnel state.
  // Mirror that state into every matched LZY material slot only after the
  // native call; doing it before the call lets RendererInfo overwrite the
  // expanded clothes array with its one-slot transaction.
#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD)
#if defined(EIEM_NATIVE_VFX_PASSTHROUGH_TEXTURE_PATCH_BUILD)
  if (perDrawVisibility.tracked && perDrawVisibility.materialTarget &&
      value.w > 0.01f)
    // Native may replace the VFXInstance pointer on every frame.  Re-read the
    // current/replacing arrays after the native per-draw update and patch the
    // newly-created instance instead of caching or expanding it.
    EiemPatchNativeVfxTexturesForRenderer(self, perDrawRenderer);
  if (perDrawVisibility.tracked && perDrawVisibility.materialTarget &&
      value.w > 0.01f) {
    // The native transaction is one VFX material, even when the replacement
    // Mesh has several submeshes.  The renderer must still receive one
    // material entry per replacement submesh; otherwise Unity draws only
    // submesh 0 (the observed `neiyi` piece) and silently drops the remaining
    // merged parts.  The helper reuses one clone for repeated LZY material
    // references and creates distinct clones only where the rule has distinct
    // material objects, so this is generic and does not hardcode a character
    // or a slot count.
    EiemApplyPerDrawVfxSlotMaterials(self, perDrawRenderer);
  }
#else
  if (perDrawVisibility.tracked && perDrawVisibility.materialTarget) {
    // Build/retrieve the expanded clone array first.  The native fanout below
    // temporarily swaps only RendererInfo's private one-slot pointer and then
    // restores it, so this final apply keeps the visible renderer expanded.
    EiemApplyPerDrawVfxSlotMaterials(self, perDrawRenderer);
#if defined(EIEM_PERDRAW_VFX_FANOUT_BUILD)
    auto *slotCache = EiemFindPerDrawVfxSlotCache(self, false);
    if (slotCache && slotCache->mappedMaterials)
      EiemFanoutNativePerDrawToVfxSlots(self, self, value, methodInfo,
                                        slotCache);
    if (value.w > 0.01f && perDrawVisibility.materialTarget)
      EiemApplyPerDrawVfxSlotMaterials(self, perDrawRenderer);
#endif
  }
#endif
#endif
  // Restore only after the native call.  Restoring before it lets the native
  // controller immediately overwrite the clean array with its VFX instance.
  if (value.w <= 0.01f && perDrawVisibility.tracked) {
    const bool restored = EiemPerDrawRestoreLzyMaterials(
        self, perDrawRenderer, methodInfo);
#if defined(EIEM_NATIVE_VFX_SLOT_ADAPTER_BUILD)
    // The cache owns GC handles for the expanded managed arrays.  Never drop
    // those roots when the visible restore failed; RendererInfo or Renderer
    // may still reference the arrays and freeing them would leave Unity with
    // a dangling managed object on the next frame.
    if (restored)
      EiemClearPerDrawVfxSlotCache(self);
#endif
    // Material restoration is a separate native transaction.  Confirm the
    // replacement Mesh once more after it completes; this covers engines that
    // rebuild the draw object during TrySet/TryReplaceSharedMaterials.
    const auto afterRestoreVisibility =
        EiemReadVisibilityControllerProbeStateFast(perDrawRenderer);
    EiemPerDrawEnsureReplacementMesh(perDrawRenderer,
                                     afterRestoreVisibility);
    const auto afterRestoreMaterials =
        EiemReadRendererInfoMaterialState(self, perDrawRenderer);
    EiemPerDrawExpandNativeSource(self, perDrawRenderer,
                                  afterRestoreMaterials.currentMaterials);
    (void)restored;
  }
#endif
#if defined(EIEM_PERDRAW_MATERIAL_PROBE_BUILD)
  if (value.w > 0.49f && value.w < 0.51f) {
    void *renderer = EiemRendererInfoRenderer(self);
    const auto state = EiemReadVisibilityControllerProbeState(renderer);
    if (state.tracked) {
      const auto materials = EiemReadRendererInfoMaterialState(self, renderer);
      EiemLogPerDrawMaterialProbe(self, state.section, materials);
    }
  }
#endif
  EiemLogCharacterPerDrawData("after", self, value, true, result);
  return result;
}

static void EiemInstallDitherProbe(void **assemblies, size_t assemblyCount) {
  if (!kEiemEnableDitherProbe) return;
  void *rendererInfo = FindMaterialRendererInfoClass(assemblies, assemblyCount);
  if (!rendererInfo) {
    Log("[DITHER-PROBE] material RendererInfo class not found");
    return;
  }
#if defined(EIEM_HIGH_FREQUENCY_DIAGNOSTICS_BUILD)
  const char *alphaTypes[] = {"System.Single"};
  const char *enableTypes[] = {"System.Boolean", "System.Boolean&"};
  HookTraceMethodWithParamTypesAndReturnType(
      rendererInfo, "TrySetManualDitherAlphaValue", alphaTypes, 1,
      "RendererInfo.TrySetManualDitherAlphaValue", "System.Boolean",
      (void *)TraceRendererInfoDitherAlpha, &s_origRendererInfoDitherAlpha);
  HookTraceMethodWithParamTypesAndReturnType(
      rendererInfo, "TrySetManualDitherEnable", enableTypes, 2,
      "RendererInfo.TrySetManualDitherEnable", "System.Boolean",
      (void *)TraceRendererInfoDitherEnable, &s_origRendererInfoDitherEnable);
#endif
  const char *perDrawTypes[] = {"UnityEngine.Vector4"};
  HookTraceMethodWithParamTypesAndReturnType(
      rendererInfo, "TrySetCharacterPerDrawData", perDrawTypes, 1,
      "RendererInfo.TrySetCharacterPerDrawData", "System.Boolean",
      (void *)TraceRendererInfoCharacterPerDrawData,
      &s_origRendererInfoCharacterPerDrawData);

#if defined(EIEM_HIGH_FREQUENCY_DIAGNOSTICS_BUILD)
  void *propertyBlockClass =
      FindClass("UnityEngine", "MaterialPropertyBlock", assemblies,
               assemblyCount);
  if (propertyBlockClass) {
    const char *floatTypes[] = {"System.Int32", "System.Single"};
    HookTraceMethodWithParamTypesAndReturnType(
        propertyBlockClass, "SetFloatImpl", floatTypes, 2,
        "MaterialPropertyBlock.SetFloatImpl", "System.Void",
        (void *)TraceMaterialPropertyBlockSetFloatImpl,
        &s_origMaterialPropertyBlockSetFloatImpl);
    Log("[DISSOLVE-PROBE] MaterialPropertyBlock scalar hook installed");
  } else {
    Log("[DISSOLVE-PROBE] MaterialPropertyBlock class not found");
  }

  void *materialClass = FindClass("UnityEngine", "Material", assemblies,
                                  assemblyCount);
  if (materialClass) {
    const char *floatTypes[] = {"System.Int32", "System.Single"};
    HookTraceMethodWithParamTypesAndReturnType(
        materialClass, "SetFloatImpl", floatTypes, 2,
        "Material.SetFloatImpl", "System.Void", (void *)TraceMaterialSetFloatImpl,
        &s_origMaterialSetFloatImpl);
    Log("[DISSOLVE-PROBE] Material scalar hook installed");
  } else {
    Log("[DISSOLVE-PROBE] Material class not found");
  }

  void *rendererClass = FindClass("UnityEngine", "Renderer", assemblies,
                                  assemblyCount);
  if (rendererClass) {
    const char *blockTypes[] = {"UnityEngine.MaterialPropertyBlock"};
    const char *indexedTypes[] = {"UnityEngine.MaterialPropertyBlock",
                                  "System.Int32"};
    HookTraceMethodWithParamTypesAndReturnType(
        rendererClass, "SetPropertyBlock", blockTypes, 1,
        "Renderer.SetPropertyBlock", "System.Void",
        (void *)TraceRendererSetPropertyBlock, &s_origRendererSetPropertyBlock);
    HookTraceMethodWithParamTypesAndReturnType(
        rendererClass, "SetPropertyBlock", indexedTypes, 2,
        "Renderer.SetPropertyBlock(indexed)", "System.Void",
        (void *)TraceRendererSetPropertyBlockIndexed,
        &s_origRendererSetPropertyBlockIndexed);
    Log("[DISSOLVE-PROBE] Renderer property-block hooks installed");
  } else {
    Log("[DISSOLVE-PROBE] Renderer class not found");
  }
#endif
  Log("[DITHER-PROBE] installed rendererInfo=%p", rendererInfo);
}
