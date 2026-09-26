#pragma once
#include "eiem_runtime_features.h"

// Per-model execution of validated Physics author resources. Matching and
// immutable resource preparation stay in the Mod program; this adapter owns
// only Unity objects and native processes for one concrete model generation.
static constexpr const char *EiemPhysicsRuntimeTag = "[PHYSICS-RUNTIME-v93-ground-diagnostic]";

struct EiemPhysicsRuntimeApi {
  void *clothClass = nullptr, *processClass = nullptr;
  void *gameObjectClass = nullptr, *transformClass = nullptr;
  void *gameObjectCtor = nullptr, *gameObjectTransform = nullptr;
  void *gameObjectActive = nullptr, *addComponent = nullptr;
  void *transformSetParent = nullptr, *setLocalPosition = nullptr;
  void *setLocalRotation = nullptr, *setLocalScale = nullptr;
  void *disableAutoBuild = nullptr, *setSerializeData = nullptr;
  void *getSerializeData = nullptr, *getSerializeData2 = nullptr;
  void *buildAndRun = nullptr, *processField = nullptr;
  void *processValid = nullptr, *processRunning = nullptr;
  void *processTeamId = nullptr, *processAnimatorField = nullptr;
  void *colliderClass = nullptr, *sphereColliderClass = nullptr;
  void *capsuleColliderClass = nullptr;
  void *colliderCenterField = nullptr, *colliderSizeField = nullptr;
  void *sphereSetSize = nullptr, *capsuleSetSize = nullptr;
  void *capsuleDirectionField = nullptr;
  void *capsuleReverseField = nullptr, *capsuleRadiusSeparationField = nullptr;
  void *capsuleAlignedField = nullptr;
  void *collisionConstraintField = nullptr, *collisionConstraintClass = nullptr;
  void *colliderListField = nullptr, *colliderListClass = nullptr;
  void *colliderListAdd = nullptr, *colliderListCount = nullptr;
  void *colliderListItem = nullptr;
  EiemPhysicsConfigApi config;

  bool Resolve(void **assemblies, size_t count, std::string &error) {
    *this = {};
    if (!config.Resolve(assemblies, count, error)) return false;
    clothClass = EiemPhysicsClass(assemblies, count, "BeyondDynamicBone.dll",
                                  "BeyondDynamicBone", "BeyondBoneCloth");
    processClass = EiemPhysicsClass(assemblies, count, "BeyondDynamicBone.dll",
                                    "BeyondDynamicBone", "ClothProcess");
    gameObjectClass = EiemPhysicsClass(assemblies, count,
        "UnityEngine.CoreModule.dll", "UnityEngine", "GameObject");
    transformClass = EiemPhysicsClass(assemblies, count,
        "UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
    gameObjectCtor = EiemPhysicsMethod(gameObjectClass, ".ctor", "System.Void",
                                       false, "System.String");
    gameObjectTransform = EiemPhysicsMethod(
        gameObjectClass, "get_transform", "UnityEngine.Transform", false);
    gameObjectActive = EiemPhysicsMethod(gameObjectClass, "SetActive",
        "System.Void", false, "System.Boolean");
    addComponent = EiemPhysicsMethod(gameObjectClass, "AddComponent",
        "UnityEngine.Component", false, "System.Type");
    transformSetParent = EiemPhysicsMethod(transformClass, "SetParent",
        "System.Void", false, "UnityEngine.Transform", "System.Boolean");
    setLocalPosition = EiemPhysicsMethod(transformClass, "set_localPosition",
        "System.Void", false, "UnityEngine.Vector3");
    setLocalRotation = EiemPhysicsMethod(transformClass, "set_localRotation",
        "System.Void", false, "UnityEngine.Quaternion");
    setLocalScale = EiemPhysicsMethod(transformClass, "set_localScale",
        "System.Void", false, "UnityEngine.Vector3");
    disableAutoBuild = EiemPhysicsMethod(clothClass, "DisableAutoBuild",
                                         "System.Void", false);
    setSerializeData = EiemPhysicsMethod(clothClass, "set_SerializeData",
        "System.Void", false, "BeyondDynamicBone.ClothSerializeData");
    getSerializeData = EiemPhysicsMethod(clothClass, "get_SerializeData",
        "BeyondDynamicBone.ClothSerializeData", false);
    getSerializeData2 = EiemPhysicsMethod(clothClass, "GetSerializeData2",
        "BeyondDynamicBone.ClothSerializeData2", false);
    buildAndRun = EiemPhysicsMethod(clothClass, "BuildAndRun",
                                    "System.Boolean", false);
    processField = EiemPhysicsField(clothClass, "process",
                                    "BeyondDynamicBone.ClothProcess");
    processValid = EiemPhysicsMethod(processClass, "IsValid",
                                     "System.Boolean", false);
    processRunning = EiemPhysicsMethod(processClass, "IsRunning",
                                       "System.Boolean", false);
    processTeamId = EiemPhysicsMethod(processClass, "get_TeamId",
                                      "System.Int32", false);
    processAnimatorField = EiemPhysicsField(
        processClass, "interlockingAnimator", "UnityEngine.Animator");
    colliderClass = EiemPhysicsClass(assemblies, count, "BeyondDynamicBone.dll",
                                     "BeyondDynamicBone", "ColliderComponent");
    sphereColliderClass = EiemPhysicsClass(
        assemblies, count, "BeyondDynamicBone.dll", "BeyondDynamicBone",
        "BeyondBoneSphereCollider");
    capsuleColliderClass = EiemPhysicsClass(
        assemblies, count, "BeyondDynamicBone.dll", "BeyondDynamicBone",
        "BeyondBoneCapsuleCollider");
    colliderCenterField = EiemPhysicsField(
        colliderClass, "center", "UnityEngine.Vector3");
    colliderSizeField = EiemPhysicsField(
        colliderClass, "size", "UnityEngine.Vector3");
    sphereSetSize = EiemPhysicsMethod(sphereColliderClass, "SetSize",
        "System.Void", false, "System.Single");
    capsuleSetSize = EiemPhysicsMethod(capsuleColliderClass, "SetSize",
        "System.Void", false, "System.Single", "System.Single",
        "System.Single");
    capsuleDirectionField = EiemPhysicsField(capsuleColliderClass, "direction");
    capsuleReverseField = EiemPhysicsField(
        capsuleColliderClass, "reverseDirection", "System.Boolean");
    capsuleRadiusSeparationField = EiemPhysicsField(
        capsuleColliderClass, "radiusSeparation", "System.Boolean");
    capsuleAlignedField = EiemPhysicsField(
        capsuleColliderClass, "alignedOnCenter", "System.Boolean");
    collisionConstraintField = EiemPhysicsField(
        config.data, "colliderCollisionConstraint");
    void *constraintType = collisionConstraintField
        ? il2cpp_field_get_type(collisionConstraintField) : nullptr;
    collisionConstraintClass = constraintType &&
        il2cpp_type_get_type(constraintType) == 0x12
        ? il2cpp_class_from_type(constraintType) : nullptr;
    colliderListField = EiemPhysicsField(collisionConstraintClass,
                                          "colliderList");
    void *listType = colliderListField
        ? il2cpp_field_get_type(colliderListField) : nullptr;
    colliderListClass = listType && il2cpp_type_get_type(listType) == 0x15
        ? il2cpp_class_from_type(listType) : nullptr;
    colliderListAdd = EiemPhysicsMethod(colliderListClass, "Add",
        "System.Void", false, "BeyondDynamicBone.ColliderComponent");
    colliderListCount = EiemPhysicsMethod(colliderListClass, "get_Count",
                                           "System.Int32", false);
    colliderListItem = EiemPhysicsMethod(colliderListClass, "get_Item",
        "BeyondDynamicBone.ColliderComponent", false, "System.Int32");
    void *directionType = capsuleDirectionField
        ? il2cpp_field_get_type(capsuleDirectionField) : nullptr;
    void *directionClass = directionType && il2cpp_type_get_type(directionType) == 0x11
        ? il2cpp_class_from_type(directionType) : nullptr;
    uint32_t directionAlignment = 0;
    const bool directionIsInt32Enum = directionClass &&
        il2cpp_class_value_size(directionClass, &directionAlignment) == 4;
    void *required[] = {
        clothClass, processClass, gameObjectClass, transformClass,
        gameObjectCtor, gameObjectTransform, gameObjectActive, addComponent,
        transformSetParent, setLocalPosition, setLocalRotation, setLocalScale,
        disableAutoBuild, setSerializeData, getSerializeData, getSerializeData2,
        buildAndRun, processField, processValid, processRunning, processTeamId,
        processAnimatorField, colliderClass, sphereColliderClass,
        capsuleColliderClass, colliderCenterField, colliderSizeField,
        sphereSetSize, capsuleSetSize, capsuleDirectionField,
        capsuleReverseField, capsuleRadiusSeparationField,
        capsuleAlignedField, collisionConstraintField,
        collisionConstraintClass, colliderListField, colliderListClass,
        colliderListAdd, colliderListCount, colliderListItem};
    for (void *entry : required)
      if (!entry) {
        error = "Native Physics runtime metadata contract is incomplete or ambiguous";
        *this = {};
        return false;
      }
    if (!directionIsInt32Enum) {
      error = "Native Physics capsule direction enum layout changed";
      *this = {};
      return false;
    }
    error.clear();
    return true;
  }
};

struct EiemPhysicsRuntimeVector3 { float x, y, z; };
struct EiemPhysicsRuntimeQuaternion { float x, y, z, w; };

static bool EiemPhysicsRuntimeRendererUnderModel(void *renderer, void *model) {
  if (!renderer || !model || !g_component_get_transform ||
      !g_gameObject_get_transform || !g_transform_get_parent)
    return false;
  void *cursor = Invoke(g_component_get_transform, renderer);
  void *root = Invoke(g_gameObject_get_transform, model);
  for (int depth = 0; cursor && root && depth < 256; ++depth) {
    if (cursor == root) return true;
    cursor = Invoke(g_transform_get_parent, cursor);
  }
  return false;
}

static int EiemPhysicsRuntimeRendererDepth(void *renderer, void *component) {
  if (!renderer || !component || !g_component_get_transform ||
      !g_transform_get_parent)
    return -1;
  void *cursor = Invoke(g_component_get_transform, renderer);
  void *root = Invoke(g_component_get_transform, component);
  for (int depth = 0; cursor && root && depth < 256; ++depth) {
    if (cursor == root) return depth;
    cursor = Invoke(g_transform_get_parent, cursor);
  }
  return -1;
}

static void *EiemPhysicsRuntimeComponents(void *model, void *klass) {
  if (!model || !klass || !g_gameObject_GetComponentsInChildren ||
      !il2cpp_class_get_type || !il2cpp_type_get_object)
    return nullptr;
  void *type = il2cpp_class_get_type(klass);
  void *typeObject = type ? il2cpp_type_get_object(type) : nullptr;
  bool includeInactive = true;
  void *args[] = {typeObject, &includeInactive};
  return typeObject
             ? Invoke(g_gameObject_GetComponentsInChildren, model, args)
             : nullptr;
}

struct EiemPhysicsRuntimeComponent {
  void *component = nullptr;
  void *process = nullptr;
  EiemUnityRef componentRef, processRef, componentData2Ref;
  int32_t teamId = -1;
};

struct EiemPhysicsRuntimeCollider {
  std::string id;
  void *gameObject = nullptr, *transform = nullptr, *component = nullptr;
  EiemUnityRef gameObjectRef, transformRef, componentRef;
  bool destroyRequested = false;
};

struct EiemPhysicsRuntimeInstance {
  uint64_t generation = 0;
  std::string key;
  // A freshly parsed INI creates a new shared_ptr even when the on-disk
  // Physics payload is unchanged. Keep the resource identity stable across
  // F10 using both the Physics file and its Skeleton dependency stamps.
  uint64_t assetStamp = 0;
  std::shared_ptr<const EiemPhysicsAsset> asset;
  void *model = nullptr;
  void *renderer = nullptr;
  void *animator = nullptr;
  EiemUnityRef modelRef, rendererRef, animatorRef, hostRef;
  std::shared_ptr<EiemSkeletonInstance> skeleton;
  EiemPhysicsConfigDraft config;
  std::vector<EiemPhysicsRuntimeComponent> components;
  std::vector<EiemPhysicsRuntimeCollider> colliders;
  bool retiring = false;
  bool destroyNeeded = false;
  bool destroyRequested = false;
  bool ready = false;
  bool bindingLogged = false;
};

static std::vector<std::shared_ptr<EiemPhysicsRuntimeInstance>>
    s_eiemPhysicsRuntimeInstances;
// Only instances whose asynchronous BuildAndRun has not yet been observed
// ready enter this list. Boundary checks never rescan every active runtime.
static std::vector<std::shared_ptr<EiemPhysicsRuntimeInstance>>
    s_eiemPhysicsRuntimePendingReady;
struct EiemPhysicsRuntimeFailure {
  void *model = nullptr;
  EiemUnityRef modelRef;
  uint64_t assetStamp = 0;
  std::shared_ptr<const EiemPhysicsAsset> asset;
  std::string key;
};
static std::vector<EiemPhysicsRuntimeFailure> s_eiemPhysicsRuntimeFailures;
static EiemPhysicsRuntimeApi s_eiemPhysicsRuntimeApi;
static bool s_eiemPhysicsRuntimeApiReady = false;
static uint64_t s_eiemPhysicsRuntimeGeneration = 0;
struct EiemPhysicsPendingRelease {
  void *model = nullptr;
  std::string stage;
};
static SRWLOCK s_eiemPhysicsPendingReleaseLock = SRWLOCK_INIT;
static std::vector<EiemPhysicsPendingRelease> s_eiemPhysicsPendingReleases;

static std::string EiemPhysicsRuntimeKey(const EiemPhysicsIntent &intent) {
  std::string key = std::string(intent.modPath) + "|" +
                    (intent.asset ? intent.asset->path.u8string() : "<missing>");
  std::transform(key.begin(), key.end(), key.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return key;
}

static uint64_t EiemPhysicsRuntimeAssetStamp(
    const std::shared_ptr<const EiemPhysicsAsset> &asset) {
  if (!asset) return 0;
  const uint64_t physics =
      EiemMeshResourceFileStamp(asset->path.u8string().c_str());
  const uint64_t skeleton =
      EiemMeshResourceFileStamp(asset->skeletonPath.u8string().c_str());
  return physics ^
         (skeleton + 0x9E3779B97F4A7C15ULL + (physics << 6) + (physics >> 2));
}

static uint64_t EiemPhysicsRuntimeAssetStamp(
    const EiemPhysicsIntent &intent) {
  return EiemPhysicsRuntimeAssetStamp(intent.asset);
}

static bool EiemPhysicsRuntimeFailureMatches(
    const EiemPhysicsRuntimeFailure &failure, void *model,
    const EiemPhysicsIntent &intent, const std::string &key) {
  return failure.model == model && failure.modelRef.Target() == model &&
         failure.modelRef.Status() == 1 &&
         failure.assetStamp == EiemPhysicsRuntimeAssetStamp(intent) &&
         failure.key == key;
}

static void EiemPhysicsRuntimeRememberFailure(
    void *model, const EiemPhysicsIntent &intent, const std::string &key) {
  for (const auto &failure : s_eiemPhysicsRuntimeFailures)
    if (EiemPhysicsRuntimeFailureMatches(failure, model, intent, key)) return;
  EiemPhysicsRuntimeFailure failure;
  failure.model = model;
  failure.modelRef = EiemUnityRef::Capture(model);
  failure.assetStamp = EiemPhysicsRuntimeAssetStamp(intent);
  failure.asset = intent.asset;
  failure.key = key;
  if (failure.modelRef) s_eiemPhysicsRuntimeFailures.push_back(std::move(failure));
}

static void EiemPhysicsRuntimePruneFailures() {
  s_eiemPhysicsRuntimeFailures.erase(
      std::remove_if(s_eiemPhysicsRuntimeFailures.begin(),
                     s_eiemPhysicsRuntimeFailures.end(),
                     [](const EiemPhysicsRuntimeFailure &failure) {
                       return failure.modelRef.Target() != failure.model ||
                              failure.modelRef.Status() != 1;
                     }),
      s_eiemPhysicsRuntimeFailures.end());
}

static bool EiemPhysicsRuntimeResolve(std::string &error) {
  if (s_eiemPhysicsRuntimeApiReady) return true;
  void *domain = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
  size_t count = 0;
  void **assemblies = domain && il2cpp_domain_get_assemblies
                          ? il2cpp_domain_get_assemblies(domain, &count)
                          : nullptr;
  if (!assemblies || !count ||
      !s_eiemPhysicsRuntimeApi.Resolve(assemblies, count, error))
    return false;
  s_eiemPhysicsRuntimeApiReady = true;
  return true;
}

static bool EiemPhysicsRuntimeSelectBinding(
    const EiemPhysicsIntent &intent, void *model, void **rendererOut,
    void **animatorOut, std::string &error) {
  *rendererOut = nullptr;
  *animatorOut = nullptr;
  if (!model || intent.matchedRenderers.empty()) {
    error = "Physics Render matched no live renderer";
    return false;
  }
  void *domain = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
  size_t count = 0;
  void **assemblies = domain && il2cpp_domain_get_assemblies
                          ? il2cpp_domain_get_assemblies(domain, &count)
                          : nullptr;
  void *animatorClass = assemblies
      ? EiemPhysicsClass(assemblies, count, EiemPhysicsAnimatorImage,
                         "UnityEngine", "Animator")
      : nullptr;
  void *animators = EiemPhysicsRuntimeComponents(model, animatorClass);
  const size_t animatorCount = EiemManagedArrayLength(animators);
  if (!animators || !animatorCount || animatorCount > 64) {
    error = "Physics model has no valid Animator set";
    return false;
  }
  void **items = (void **)((char *)animators + IL2CPP_ARRAY_DATA);
  for (void *candidate : intent.matchedRenderers) {
    if (!candidate || EiemNativeObjectStatus(candidate) != 1 ||
        !EiemPhysicsRuntimeRendererUnderModel(candidate, model))
      continue;
    void *nearest = nullptr;
    int nearestDepth = -1;
    size_t ancestors = 0;
    for (size_t index = 0; index < animatorCount; ++index) {
      const int depth = EiemPhysicsRuntimeRendererDepth(candidate, items[index]);
      if (depth < 0) continue;
      ++ancestors;
      if (nearestDepth < 0 || depth < nearestDepth) {
        nearest = items[index];
        nearestDepth = depth;
      }
    }
    if (ancestors != 1 || !nearest) continue;
    if (*animatorOut && *animatorOut != nearest) {
      error = "Physics Render matches span different Animators";
      return false;
    }
    if (!*rendererOut) *rendererOut = candidate;
    *animatorOut = nearest;
  }
  if (!*rendererOut || !*animatorOut) {
    error = "Physics renderer does not have one unique Animator ancestor";
    return false;
  }
  return true;
}

static bool EiemPhysicsRuntimeNewHost(EiemPhysicsRuntimeInstance &instance,
                                      void **hostOut, std::string &error) {
  auto &api = s_eiemPhysicsRuntimeApi;
  void *modelTransform = nullptr;
  if (!InvokeChecked(api.gameObjectTransform, instance.model, nullptr,
                     &modelTransform) ||
      !modelTransform || !il2cpp_object_new || !il2cpp_string_new) {
    error = "Cannot resolve Physics model Transform";
    return false;
  }
  void *host = il2cpp_object_new(api.gameObjectClass);
  instance.hostRef = EiemUnityRef::Capture(host, false);
  const std::string name = "EIEM_Physics_" +
                           std::to_string(instance.generation);
  void *nameObject = il2cpp_string_new(name.c_str());
  void *result = nullptr;
  void *ctorArgs[] = {nameObject};
  if (!host || !instance.hostRef || !nameObject ||
      !InvokeChecked(api.gameObjectCtor, host, ctorArgs, &result)) {
    error = "Cannot construct Physics host";
    return false;
  }
  void *transform = nullptr;
  bool keepWorld = false;
  EiemPhysicsRuntimeVector3 position{0, 0, 0}, scale{1, 1, 1};
  EiemPhysicsRuntimeQuaternion rotation{0, 0, 0, 1};
  void *parentArgs[] = {modelTransform, &keepWorld};
  void *positionArgs[] = {&position};
  void *rotationArgs[] = {&rotation};
  void *scaleArgs[] = {&scale};
  bool active = false;
  void *activeArgs[] = {&active};
  if (!InvokeChecked(api.gameObjectTransform, host, nullptr, &transform) ||
      !transform ||
      !InvokeChecked(api.transformSetParent, transform, parentArgs, &result) ||
      !InvokeChecked(api.setLocalPosition, transform, positionArgs, &result) ||
      !InvokeChecked(api.setLocalRotation, transform, rotationArgs, &result) ||
      !InvokeChecked(api.setLocalScale, transform, scaleArgs, &result) ||
      !InvokeChecked(api.gameObjectActive, host, activeArgs, &result)) {
    error = "Cannot attach inactive Physics host";
    return false;
  }
  *hostOut = host;
  return true;
}

static bool EiemPhysicsRuntimeWriteColliderField(void *object, void *field,
                                                 void *value) {
  if (!object || !field || !value || !il2cpp_field_set_value) return false;
  __try {
    il2cpp_field_set_value(object, field, value);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

static bool EiemPhysicsRuntimeCreateColliders(
    EiemPhysicsRuntimeInstance &instance,
    const std::unordered_map<std::string, void *> &bindings,
    std::string &error) {
  auto &api = s_eiemPhysicsRuntimeApi;
  for (const auto &source : instance.asset->physics.colliders) {
    auto binding = bindings.find(source.bone);
    if (binding == bindings.end() || !binding->second) {
      error = "Physics collider bone is unavailable: " + source.bone;
      return false;
    }
    instance.colliders.emplace_back();
    auto &runtime = instance.colliders.back();
    runtime.id = source.id;
    runtime.gameObject = il2cpp_object_new(api.gameObjectClass);
    runtime.gameObjectRef = EiemUnityRef::Capture(runtime.gameObject, false);
    void *name = il2cpp_string_new(source.name.c_str());
    void *result = nullptr;
    void *ctorArgs[] = {name};
    if (!runtime.gameObject || !runtime.gameObjectRef || !name ||
        !InvokeChecked(api.gameObjectCtor, runtime.gameObject, ctorArgs,
                       &result)) {
      error = "Cannot construct Physics collider GameObject: " + source.name;
      return false;
    }
    bool inactive = false;
    void *inactiveArgs[] = {&inactive};
    EiemPhysicsRuntimeVector3 position{
        source.position[0], source.position[1], source.position[2]};
    EiemPhysicsRuntimeQuaternion rotation{
        source.rotation[0], source.rotation[1], source.rotation[2],
        source.rotation[3]};
    EiemPhysicsRuntimeVector3 scale{1, 1, 1};
    bool keepWorld = false;
    void *parentArgs[] = {binding->second, &keepWorld};
    void *positionArgs[] = {&position};
    void *rotationArgs[] = {&rotation};
    void *scaleArgs[] = {&scale};
    if (!InvokeChecked(api.gameObjectActive, runtime.gameObject, inactiveArgs,
                       &result) ||
        !InvokeChecked(api.gameObjectTransform, runtime.gameObject, nullptr,
                       &runtime.transform) ||
        !runtime.transform ||
        !(runtime.transformRef = EiemUnityRef::Capture(runtime.transform, false)) ||
        !InvokeChecked(api.transformSetParent, runtime.transform, parentArgs,
                       &result) ||
        !InvokeChecked(api.setLocalPosition, runtime.transform, positionArgs,
                       &result) ||
        !InvokeChecked(api.setLocalRotation, runtime.transform, rotationArgs,
                       &result) ||
        !InvokeChecked(api.setLocalScale, runtime.transform, scaleArgs,
                       &result)) {
      error = "Cannot bind Physics collider to bone: " + source.name;
      return false;
    }
    void *componentClass = source.shape == 0
        ? api.sphereColliderClass : api.capsuleColliderClass;
    void *componentType = il2cpp_type_get_object(
        il2cpp_class_get_type(componentClass));
    auto typeRef = EiemUnityRef::Capture(componentType, false);
    void *addArgs[] = {componentType};
    if (!typeRef ||
        !InvokeChecked(api.addComponent, runtime.gameObject, addArgs,
                       &runtime.component) ||
        !runtime.component ||
        il2cpp_object_get_class(runtime.component) != componentClass ||
        !(runtime.componentRef =
              EiemUnityRef::Capture(runtime.component, false))) {
      error = "Cannot add native Physics collider component: " + source.name;
      return false;
    }
    EiemPhysicsRuntimeVector3 center{0, 0, 0};
    if (!EiemPhysicsRuntimeWriteColliderField(
            runtime.component, api.colliderCenterField, &center)) {
      error = "Cannot set Physics collider center: " + source.name;
      return false;
    }
    if (source.shape == 0) {
      float radius = source.radius;
      void *sizeArgs[] = {&radius};
      if (!InvokeChecked(api.sphereSetSize, runtime.component, sizeArgs,
                         &result)) {
        error = "Cannot set Physics sphere size: " + source.name;
        return false;
      }
    } else {
      float endRadius = instance.asset->physics.version >= 5
          ? source.endRadius : source.radius;
      float length = source.span + source.radius + endRadius;
      if (!std::isfinite(length)) {
        error = "Physics capsule length overflow: " + source.name;
        return false;
      }
      float radius = source.radius;
      void *sizeArgs[] = {&radius, &endRadius, &length};
      int32_t direction = 1; // local Y; author rotation carries orientation
      bool reverse = false, separated = endRadius != radius;
      bool aligned = instance.asset->physics.version >= 5
          ? source.alignedOnCenter != 0 : true;
      if (!InvokeChecked(api.capsuleSetSize, runtime.component, sizeArgs,
                         &result) ||
          !EiemPhysicsRuntimeWriteColliderField(
              runtime.component, api.capsuleDirectionField, &direction) ||
          !EiemPhysicsRuntimeWriteColliderField(
              runtime.component, api.capsuleReverseField, &reverse) ||
          !EiemPhysicsRuntimeWriteColliderField(
              runtime.component, api.capsuleRadiusSeparationField,
              &separated) ||
          !EiemPhysicsRuntimeWriteColliderField(
              runtime.component, api.capsuleAlignedField, &aligned)) {
        error = "Cannot configure Physics capsule: " + source.name;
        return false;
      }
    }
    EiemPhysicsRuntimeVector3 actualCenter{}, actualSize{};
    if (!EiemPhysicsReadField(runtime.component, api.colliderCenterField,
                              actualCenter) ||
        !EiemPhysicsReadField(runtime.component, api.colliderSizeField,
                              actualSize) ||
        actualCenter.x != 0 || actualCenter.y != 0 || actualCenter.z != 0 ||
        actualSize.x != source.radius ||
        (source.shape == 0 && (actualSize.y != 0 || actualSize.z != 0)) ||
        (source.shape == 1 &&
         (actualSize.y != (instance.asset->physics.version >= 5
                              ? source.endRadius : source.radius) ||
          actualSize.z != source.span + source.radius +
              (instance.asset->physics.version >= 5
                   ? source.endRadius : source.radius)))) {
      error = "Native Physics collider readback mismatch: " + source.name;
      return false;
    }
    if (source.shape == 1) {
      int32_t actualDirection = -1;
      bool actualReverse = true;
      const bool expectedSeparated =
          (instance.asset->physics.version >= 5 ? source.endRadius
                                                : source.radius) != source.radius;
      bool actualSeparated = !expectedSeparated;
      const bool expectedAligned = instance.asset->physics.version >= 5
          ? source.alignedOnCenter != 0 : true;
      bool actualAligned = !expectedAligned;
      if (!EiemPhysicsReadField(runtime.component, api.capsuleDirectionField,
                                actualDirection) || actualDirection != 1 ||
          !EiemPhysicsReadField(runtime.component, api.capsuleReverseField,
                                actualReverse) || actualReverse ||
          !EiemPhysicsReadField(runtime.component,
                                api.capsuleRadiusSeparationField,
                                actualSeparated) ||
          actualSeparated != expectedSeparated ||
          !EiemPhysicsReadField(runtime.component, api.capsuleAlignedField,
                                actualAligned) ||
          actualAligned != expectedAligned) {
        error = "Native Physics capsule flag readback mismatch: " + source.name;
          return false;
      }
    }
    Vector3 actualPosition{};
    Quaternion actualRotation{0, 0, 0, 1};
    const bool poseRead =
        EiemPhysicsReadValue(runtime.transform, g_transform_get_localPosition,
                             actualPosition) &&
        EiemPhysicsReadValue(runtime.transform, g_transform_get_localRotation,
                             actualRotation);
    void *actualParent = g_transform_get_parent
                             ? Invoke(g_transform_get_parent, runtime.transform)
                             : nullptr;
    const int parentMatch = g_transform_get_parent
                                ? (actualParent == binding->second ? 1 : 0)
                                : -1;
    Log("%s collider generation=%llu id=%s name=%s bone=%s binding=%p "
        "transform=%p parent=%p parentMatch=%d shape=%s "
        "position=%.7g,%.7g,%.7g rotation=%.7g,%.7g,%.7g,%.7g "
        "poseRead=%d radius=%.7g endRadius=%.7g span=%.7g "
        "size=%.7g,%.7g,%.7g",
        EiemPhysicsRuntimeTag, (unsigned long long)instance.generation,
        source.id.c_str(), source.name.c_str(), source.bone.c_str(),
        binding->second, runtime.transform, actualParent, parentMatch,
        source.shape == 0 ? "SPHERE" : "CAPSULE", actualPosition.x,
        actualPosition.y, actualPosition.z, actualRotation.x, actualRotation.y,
        actualRotation.z, actualRotation.w, poseRead ? 1 : 0, source.radius,
        source.endRadius, source.span, actualSize.x, actualSize.y,
        actualSize.z);
  }
  return true;
}

static bool EiemPhysicsRuntimeBindColliders(
    EiemPhysicsRuntimeInstance &instance, std::string &error) {
  auto &api = s_eiemPhysicsRuntimeApi;
  std::unordered_map<std::string, void *> components;
  for (const auto &collider : instance.colliders)
    if (!components.emplace(collider.id, collider.component).second) {
      error = "Duplicate runtime Physics collider identity";
      return false;
    }
  const auto &groups = instance.config.Groups();
  if (groups.size() != instance.asset->physics.groups.size()) {
    error = "Physics collider group mapping changed";
    return false;
  }
  for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
    void *constraint = nullptr, *list = nullptr;
    if (!EiemPhysicsReadField(groups[groupIndex].data.Target(),
                              api.collisionConstraintField, constraint) ||
        !constraint ||
        il2cpp_object_get_class(constraint) != api.collisionConstraintClass ||
        !EiemPhysicsReadField(constraint, api.colliderListField, list) ||
        !list || il2cpp_object_get_class(list) != api.colliderListClass) {
      error = "ClothSerializeData did not provide a collider list";
      return false;
    }
    int32_t initialCount = -1;
    if (!EiemPhysicsReadValue(list, api.colliderListCount, initialCount) ||
        initialCount != 0) {
      error = "New ClothSerializeData collider list is not empty";
      return false;
    }
    void *result = nullptr;
    const auto &ids = instance.asset->physics.groups[groupIndex].colliders;
    for (const auto &id : ids) {
      auto found = components.find(id);
      void *component = found == components.end() ? nullptr : found->second;
      void *args[] = {component};
      if (!component ||
          !InvokeChecked(api.colliderListAdd, list, args, &result)) {
        error = "Cannot bind Physics collider to group: " + id;
        return false;
      }
    }
    int32_t count = -1;
    if (!EiemPhysicsReadValue(list, api.colliderListCount, count) ||
        count != (int32_t)ids.size()) {
      error = "Physics collider list count mismatch";
      return false;
    }
    for (int32_t index = 0; index < count; ++index) {
      void *actual = nullptr;
      void *args[] = {&index};
      if (!InvokeChecked(api.colliderListItem, list, args, &actual) ||
          actual != components.at(ids[(size_t)index])) {
        error = "Physics collider list member mismatch";
        return false;
      }
    }
  }
  return true;
}

static bool EiemPhysicsRuntimeActivateColliders(
    EiemPhysicsRuntimeInstance &instance, std::string &error) {
  bool active = true;
  void *args[] = {&active};
  void *result = nullptr;
  for (auto &collider : instance.colliders)
    if (!InvokeChecked(s_eiemPhysicsRuntimeApi.gameObjectActive,
                       collider.gameObject, args, &result)) {
      error = "Cannot activate Physics collider";
      return false;
    }
  return true;
}

static void EiemPhysicsRuntimeRequestDestroyOwned(
    EiemPhysicsRuntimeInstance &instance) {
  if (!kEiemEnableExperimentalPhysicsRuntime) return;
  if (!instance.destroyNeeded || !EiemOnUnityThread() || !g_object_destroy)
    return;
  if (!instance.destroyRequested && instance.hostRef.Status() == 1) {
    void *host = instance.hostRef.Target();
    void *args[] = {host};
    instance.destroyRequested =
        EiemSkeletonCall(g_object_destroy, nullptr, args);
  }
  for (auto &collider : instance.colliders) {
    if (collider.destroyRequested || collider.gameObjectRef.Status() != 1)
      continue;
    void *gameObject = collider.gameObjectRef.Target();
    void *args[] = {gameObject};
    collider.destroyRequested =
        EiemSkeletonCall(g_object_destroy, nullptr, args);
  }
}

static void EiemPhysicsRuntimeBeginRetire(
    const std::shared_ptr<EiemPhysicsRuntimeInstance> &instance,
    bool destroyHost, const char *stage) {
  if (!kEiemEnableExperimentalPhysicsRuntime) return;
  if (!instance) return;
  instance->destroyNeeded = instance->destroyNeeded || destroyHost;
  if (instance->retiring) return;
  instance->retiring = true;
  instance->ready = false;
  EiemPhysicsRuntimeRequestDestroyOwned(*instance);
  Log("%s retire generation=%llu model=%p host=%p components=%zu colliders=%zu destroy=%d stage=%s",
      EiemPhysicsRuntimeTag,
      (unsigned long long)instance->generation, instance->model,
      instance->hostRef.Target(), instance->components.size(),
      instance->colliders.size(),
      instance->destroyRequested ? 1 : 0, stage ? stage : "unknown");
}

static bool EiemPhysicsRuntimeDead(
    const EiemPhysicsRuntimeInstance &instance) {
  if (instance.hostRef.Status() != 0) return false;
  for (const auto &component : instance.components)
    if (component.componentRef.Status() != 0) return false;
  for (const auto &collider : instance.colliders)
    if (collider.gameObjectRef.Status() != 0 ||
        collider.transformRef.Status() != 0 ||
        collider.componentRef.Status() != 0)
      return false;
  return true;
}

static size_t EiemPhysicsRuntimeCollect() {
  if (!kEiemEnableExperimentalPhysicsRuntime) return 0;
  size_t retiredCount = 0;
  if (!EiemOnUnityThread()) return retiredCount;
  for (size_t index = 0; index < s_eiemPhysicsRuntimeInstances.size();) {
    auto &instance = s_eiemPhysicsRuntimeInstances[index];
    if (!instance->retiring && instance->modelRef.Status() == 0)
      EiemPhysicsRuntimeBeginRetire(instance, false, "model native object expired");
    if (instance->retiring && instance->destroyNeeded)
      EiemPhysicsRuntimeRequestDestroyOwned(*instance);
    if (!instance->retiring || !EiemPhysicsRuntimeDead(*instance)) {
      ++index;
      continue;
    }
    Log("%s retired generation=%llu model=%p key=%s",
        EiemPhysicsRuntimeTag, (unsigned long long)instance->generation,
        instance->model, instance->key.c_str());
    ++retiredCount;
    s_eiemPhysicsRuntimeInstances.erase(
        s_eiemPhysicsRuntimeInstances.begin() + index);
  }
  EiemCollectSkeletonInstances();
  return retiredCount;
}

static bool EiemAcquirePhysicsSkeleton(
    const EiemPhysicsIntent &intent, void *renderer,
    std::shared_ptr<EiemSkeletonInstance> &out, char *message,
    size_t messageSize) {
  out.reset();
  if (!intent.asset) {
    if (message)
      strncpy_s(message, messageSize, "Physics asset is unavailable",
                _TRUNCATE);
    return false;
  }
  // A Physics Skeleton may contain source nodes from a larger PFB that do not
  // exist in a UI/NPC renderer's live hierarchy. Keep that contract
  // resource-driven: the Skeleton adapter creates only the explicitly
  // exported missing anchors, under the nearest resolved source parent.
  std::unordered_set<std::string> virtualSkeletonPaths;
  for (const auto &node : intent.asset->skeleton.nodes)
    if (node.source) virtualSkeletonPaths.insert(node.path);
  const std::string path = intent.asset->skeletonPath.u8string();
  return EiemAcquireSkeletonDocument(
      EiemSkeletonInstanceKey(intent.modPath, path),
      EiemMeshResourceFileStamp(path.c_str()), intent.asset->skeleton, renderer,
      out, message, messageSize, &virtualSkeletonPaths);
}

static bool EiemPhysicsRuntimeBuild(const EiemPhysicsIntent &intent,
                                    void *model, const char *stage,
                                    bool &retryable) {
  retryable = false;
  // Defense in depth: intent collection is normally frozen first, but this
  // adapter must also reject direct or future call sites before it resolves
  // APIs, creates hosts/colliders, AddComponents, or calls BuildAndRun.
  if (!kEiemEnableExperimentalPhysicsRuntime) return false;
  const bool reloadTrace = stage && strcmp(stage, "global reload") == 0;
  const ULONGLONG buildStarted = reloadTrace ? GetTickCount64() : 0;
  if (reloadTrace)
    Log("%s reload-build begin model=%p resource=%s",
        EiemPhysicsRuntimeTag, model, intent.resourceSection);
  std::string error;
  if (!EiemOnUnityThread() || !intent.asset ||
      !EiemPhysicsRuntimeResolve(error)) {
    Log("%s rejected model=%p resource=%s error=%s", EiemPhysicsRuntimeTag,
        model, intent.resourceSection,
        error.empty() ? "runtime APIs unavailable" : error.c_str());
    return false;
  }
  void *renderer = nullptr, *animator = nullptr;
  if (!EiemPhysicsRuntimeSelectBinding(intent, model, &renderer, &animator,
                                       error)) {
    retryable = true;
    Log("%s pending model=%p resource=%s error=%s stage=%s",
        EiemPhysicsRuntimeTag, model, intent.resourceSection, error.c_str(),
        stage ? stage : "unknown");
    return false;
  }
  auto instance = std::make_shared<EiemPhysicsRuntimeInstance>();
  instance->generation = ++s_eiemPhysicsRuntimeGeneration;
  instance->key = EiemPhysicsRuntimeKey(intent);
  instance->assetStamp = EiemPhysicsRuntimeAssetStamp(intent);
  instance->asset = intent.asset;
  instance->model = model;
  instance->renderer = renderer;
  instance->animator = animator;
  instance->modelRef = EiemUnityRef::Capture(model);
  instance->rendererRef = EiemUnityRef::Capture(renderer);
  instance->animatorRef = EiemUnityRef::Capture(animator);
  char skeletonError[256] = {};
  if (!instance->modelRef || !instance->rendererRef || !instance->animatorRef ||
      !EiemAcquirePhysicsSkeleton(intent, renderer, instance->skeleton,
                                  skeletonError, sizeof(skeletonError))) {
    Log("%s rejected generation=%llu model=%p resource=%s error=%s",
        EiemPhysicsRuntimeTag, (unsigned long long)instance->generation,
        model, intent.resourceSection,
        skeletonError[0] ? skeletonError : "Cannot bind Physics Skeleton");
    return false;
  }
  if (reloadTrace)
    Log("%s reload-build skeleton-ready model=%p elapsed=%llums",
        EiemPhysicsRuntimeTag, model, GetTickCount64() - buildStarted);
  std::unordered_map<std::string, void *> bindings;
  if (instance->skeleton->nodes.size() != intent.asset->skeleton.nodes.size()) {
    Log("%s rejected generation=%llu model=%p resource=%s error=Skeleton node count changed",
        EiemPhysicsRuntimeTag, (unsigned long long)instance->generation,
        model, intent.resourceSection);
    return false;
  }
  for (size_t index = 0; index < intent.asset->skeleton.nodes.size(); ++index) {
    void *node = instance->skeleton->nodes[index].Target();
    if (!node || instance->skeleton->nodes[index].Status() != 1) {
      Log("%s rejected generation=%llu model=%p resource=%s error=Skeleton node expired",
          EiemPhysicsRuntimeTag, (unsigned long long)instance->generation,
          model, intent.resourceSection);
      return false;
    }
    bindings.emplace(intent.asset->skeleton.nodes[index].path, node);
  }
  if (!instance->config.Prepare(s_eiemPhysicsRuntimeApi.config,
                                intent.asset->physics, bindings, error)) {
    Log("%s rejected generation=%llu model=%p resource=%s error=%s",
        EiemPhysicsRuntimeTag, (unsigned long long)instance->generation,
        model, intent.resourceSection, error.c_str());
    return false;
  }
  size_t paletteCount = 0, selectedCount = 0, paletteHits = 0;
  size_t boundaryIgnores = 0;
  void *palette = nullptr;
  if (g_smr_get_bones &&
      EiemSkeletonCall(g_smr_get_bones, renderer, nullptr, &palette)) {
    paletteCount = EiemManagedArrayLength(palette);
    if (palette && paletteCount <= 16384) {
      void **items = (void **)((char *)palette + IL2CPP_ARRAY_DATA);
      for (const auto &group : instance->config.Groups())
        for (const auto &node : group.transforms) {
          ++selectedCount;
          void *selected = node.Target();
          for (size_t index = 0; index < paletteCount; ++index)
            if (items[index] == selected) {
              ++paletteHits;
              break;
            }
        }
    }
  }
  for (const auto &group : instance->config.Groups())
    boundaryIgnores += group.boundaryIgnores.size();
  Log("%s binding generation=%llu model=%p renderer=%p palette=%zu selected=%zu paletteHits=%zu boundaryIgnores=%zu",
      EiemPhysicsRuntimeTag, (unsigned long long)instance->generation, model,
      renderer, paletteCount, selectedCount, paletteHits, boundaryIgnores);
  s_eiemPhysicsRuntimeInstances.push_back(instance);
  s_eiemPhysicsRuntimePendingReady.push_back(instance);
  void *host = nullptr;
  if (!EiemPhysicsRuntimeNewHost(*instance, &host, error)) {
    EiemPhysicsRuntimeBeginRetire(instance, true, "host construction failed");
    return false;
  }
  if (reloadTrace)
    Log("%s reload-build host-ready model=%p elapsed=%llums",
        EiemPhysicsRuntimeTag, model, GetTickCount64() - buildStarted);
  if (!EiemPhysicsRuntimeCreateColliders(*instance, bindings, error)) {
    EiemPhysicsRuntimeBeginRetire(instance, true, error.c_str());
    return false;
  }
  void *clothType = il2cpp_type_get_object(
      il2cpp_class_get_type(s_eiemPhysicsRuntimeApi.clothClass));
  auto typeRef = EiemUnityRef::Capture(clothType, false);
  if (!typeRef) {
    EiemPhysicsRuntimeBeginRetire(instance, true, "component type unavailable");
    return false;
  }
  size_t groupIndex = 0;
  for (const auto &prepared : instance->config.Groups()) {
    if (reloadTrace)
      Log("%s reload-build group=%zu add-component begin model=%p elapsed=%llums",
          EiemPhysicsRuntimeTag, groupIndex, model,
          GetTickCount64() - buildStarted);
    void *component = nullptr, *result = nullptr;
    void *addArgs[] = {clothType};
    if (!InvokeChecked(s_eiemPhysicsRuntimeApi.addComponent, host, addArgs,
                       &component) ||
        !component ||
        il2cpp_object_get_class(component) !=
            s_eiemPhysicsRuntimeApi.clothClass) {
      EiemPhysicsRuntimeBeginRetire(instance, true, "AddComponent failed");
      return false;
    }
    EiemPhysicsRuntimeComponent runtime;
    runtime.component = component;
    runtime.componentRef = EiemUnityRef::Capture(component, false);
    void *data = prepared.data.Target();
    void *dataArgs[] = {data};
    void *actualData = nullptr, *actualData2 = nullptr;
    if (!runtime.componentRef ||
        !InvokeChecked(s_eiemPhysicsRuntimeApi.disableAutoBuild, component,
                       nullptr, &result) ||
        !InvokeChecked(s_eiemPhysicsRuntimeApi.setSerializeData, component,
                       dataArgs, &result) ||
        !InvokeChecked(s_eiemPhysicsRuntimeApi.getSerializeData, component,
                       nullptr, &actualData) ||
        actualData != data ||
        !InvokeChecked(s_eiemPhysicsRuntimeApi.getSerializeData2, component,
                       nullptr, &actualData2) ||
        !actualData2 ||
        il2cpp_object_get_class(actualData2) !=
            s_eiemPhysicsRuntimeApi.config.data2 ||
        !(runtime.componentData2Ref =
              EiemUnityRef::Capture(actualData2, false))) {
      EiemPhysicsRuntimeBeginRetire(instance, true,
                                    "component configuration failed");
      return false;
    }
    EiemPhysicsReadField(component, s_eiemPhysicsRuntimeApi.processField,
                         runtime.process);
    if (runtime.process)
      runtime.processRef = EiemUnityRef::Capture(runtime.process, false);
    instance->components.push_back(std::move(runtime));
    if (reloadTrace)
      Log("%s reload-build group=%zu configured model=%p elapsed=%llums",
          EiemPhysicsRuntimeTag, groupIndex, model,
          GetTickCount64() - buildStarted);
    ++groupIndex;
  }
  if (!EiemPhysicsRuntimeBindColliders(*instance, error)) {
    EiemPhysicsRuntimeBeginRetire(instance, true, error.c_str());
    return false;
  }
  if (reloadTrace)
    Log("%s reload-build colliders-ready model=%p count=%zu elapsed=%llums",
        EiemPhysicsRuntimeTag, model, instance->colliders.size(),
        GetTickCount64() - buildStarted);
  if (!EiemPhysicsRuntimeActivateColliders(*instance, error)) {
    EiemPhysicsRuntimeBeginRetire(instance, true, error.c_str());
    return false;
  }
  bool active = true;
  void *activeArgs[] = {&active};
  void *result = nullptr;
  if (!InvokeChecked(s_eiemPhysicsRuntimeApi.gameObjectActive, host, activeArgs,
                     &result)) {
    EiemPhysicsRuntimeBeginRetire(instance, true, "host activation failed");
    return false;
  }
  for (auto &runtime : instance->components) {
    void *boxed = nullptr;
    bool started = false;
    if (reloadTrace)
      Log("%s reload-build build-and-run begin model=%p elapsed=%llums",
          EiemPhysicsRuntimeTag, model, GetTickCount64() - buildStarted);
    if (!InvokeChecked(s_eiemPhysicsRuntimeApi.buildAndRun, runtime.component,
                       nullptr, &boxed) ||
        !EiemPhysicsUnbox(boxed, started) || !started) {
      EiemPhysicsRuntimeBeginRetire(instance, true, "BuildAndRun failed");
      return false;
    }
    EiemPhysicsReadField(runtime.component,
                         s_eiemPhysicsRuntimeApi.processField,
                         runtime.process);
    if (runtime.process)
      runtime.processRef = EiemUnityRef::Capture(runtime.process, false);
    if (reloadTrace)
      Log("%s reload-build build-and-run end model=%p elapsed=%llums",
          EiemPhysicsRuntimeTag, model, GetTickCount64() - buildStarted);
  }
  if (reloadTrace)
    Log("%s reload-build end model=%p elapsed=%llums",
        EiemPhysicsRuntimeTag, model, GetTickCount64() - buildStarted);
  Log("%s build-started generation=%llu model=%p renderer=%p animator=%p groups=%zu colliders=%zu resource=%s matches=%u stage=%s",
      EiemPhysicsRuntimeTag, (unsigned long long)instance->generation, model,
      renderer, animator, instance->components.size(), instance->colliders.size(), intent.resourceSection,
      intent.rendererMatches, stage ? stage : "unknown");
  return true;
}

static void EiemReconcileModelPhysics(
    void *model, const std::vector<EiemPhysicsIntent> &intents, bool active,
    const char *stage) {
  if (!kEiemEnableExperimentalPhysicsRuntime) return;
  if (!model || !EiemOnUnityThread()) return;
  s_eiemPhysicsRuntimeFailures.erase(
      std::remove_if(s_eiemPhysicsRuntimeFailures.begin(),
                     s_eiemPhysicsRuntimeFailures.end(),
                     [&](const EiemPhysicsRuntimeFailure &failure) {
                       if (failure.model != model) return false;
                       for (const auto &intent : intents)
                         if (failure.assetStamp ==
                                 EiemPhysicsRuntimeAssetStamp(intent) &&
                             failure.key == EiemPhysicsRuntimeKey(intent))
                           return false;
                       return true;
      }),
      s_eiemPhysicsRuntimeFailures.end());
  if (!active) {
    // Visibility is an ownership boundary for UI/NPC model instances. Retire
    // all native hosts and owned colliders while the model is inactive. A later
    // owner activation or F10 transaction builds them again.
    for (const auto &instance : s_eiemPhysicsRuntimeInstances)
      if (instance->model == model && !instance->retiring)
        EiemPhysicsRuntimeBeginRetire(instance, true, stage);
    return;
  }
  for (const auto &instance : s_eiemPhysicsRuntimeInstances) {
    if (instance->model != model || instance->retiring) continue;
    bool keep = false;
    for (const auto &intent : intents)
      if (EiemPhysicsRuntimeAssetStamp(intent) == instance->assetStamp &&
          EiemPhysicsRuntimeKey(intent) == instance->key) {
        keep = true;
        break;
      }
    if (!keep) EiemPhysicsRuntimeBeginRetire(instance, true, stage);
  }
  for (const auto &instance : s_eiemPhysicsRuntimeInstances)
    if (instance->model == model && instance->retiring) return;
  for (const auto &intent : intents) {
    if (!intent.asset) continue;
    const std::string key = EiemPhysicsRuntimeKey(intent);
    bool exists = false;
    for (const auto &instance : s_eiemPhysicsRuntimeInstances)
      if (!instance->retiring && instance->model == model &&
          instance->assetStamp == EiemPhysicsRuntimeAssetStamp(intent) &&
          instance->key == key) {
        exists = true;
        break;
      }
    if (exists) continue;
    bool failed = false;
    for (const auto &failure : s_eiemPhysicsRuntimeFailures)
      if (EiemPhysicsRuntimeFailureMatches(failure, model, intent, key)) {
        failed = true;
        break;
      }
    bool retryable = false;
    if (!failed && !EiemPhysicsRuntimeBuild(intent, model, stage, retryable) &&
        !retryable) {
      EiemPhysicsRuntimeRememberFailure(model, intent, key);
      Log("%s retry-suppressed model=%p resource=%s until model or Physics asset changes",
          EiemPhysicsRuntimeTag, model, intent.resourceSection);
    }
  }
}

// F10 is the only resource-change boundary. Retire instances whose on-disk
// Physics or Skeleton dependency changed before Renderer restoration starts;
// unchanged instances continue to share the game's live model generation.
static size_t EiemPhysicsRuntimeRetireChangedAssets(const char *stage) {
  if (!kEiemEnableExperimentalPhysicsRuntime) return 0;
  if (!EiemOnUnityThread()) return 0;
  size_t changed = 0;
  for (const auto &instance : s_eiemPhysicsRuntimeInstances) {
    if (instance->retiring ||
        EiemPhysicsRuntimeAssetStamp(instance->asset) == instance->assetStamp)
      continue;
    EiemPhysicsRuntimeBeginRetire(instance, true, stage);
    ++changed;
  }
  if (changed)
    Log("%s reload asset changes retired=%zu stage=%s", EiemPhysicsRuntimeTag,
        changed, stage ? stage : "unknown");
  return changed;
}

static void EiemPhysicsRuntimeReleaseOnUnityThread(void *model,
                                                   const char *stage) {
  if (!kEiemEnableExperimentalPhysicsRuntime) return;
  if (!model) return;
  s_eiemPhysicsRuntimeFailures.erase(
      std::remove_if(s_eiemPhysicsRuntimeFailures.begin(),
                     s_eiemPhysicsRuntimeFailures.end(),
                     [model](const EiemPhysicsRuntimeFailure &failure) {
                       return failure.model == model;
                     }),
      s_eiemPhysicsRuntimeFailures.end());
  for (const auto &instance : s_eiemPhysicsRuntimeInstances)
    if (instance->model == model && !instance->retiring)
      EiemPhysicsRuntimeBeginRetire(instance, true, stage);
}

static void EiemReleaseModelPhysics(void *model, const char *stage) {
  if (!kEiemEnableExperimentalPhysicsRuntime) return;
  if (!model) return;
  if (EiemOnUnityThread()) {
    EiemPhysicsRuntimeReleaseOnUnityThread(model, stage);
    return;
  }
  AcquireSRWLockExclusive(&s_eiemPhysicsPendingReleaseLock);
  s_eiemPhysicsPendingReleases.push_back(
      {model, stage ? stage : "off-thread model release"});
  ReleaseSRWLockExclusive(&s_eiemPhysicsPendingReleaseLock);
}

static void EiemPhysicsRuntimeDrainReleases() {
  if (!kEiemEnableExperimentalPhysicsRuntime) return;
  std::vector<EiemPhysicsPendingRelease> pending;
  AcquireSRWLockExclusive(&s_eiemPhysicsPendingReleaseLock);
  pending.swap(s_eiemPhysicsPendingReleases);
  ReleaseSRWLockExclusive(&s_eiemPhysicsPendingReleaseLock);
  for (const auto &release : pending)
    EiemPhysicsRuntimeReleaseOnUnityThread(release.model, release.stage.c_str());
}



// BuildAndRun is asynchronous. Check its accepted component once at the next
// explicit lifecycle boundary (initial model attach, owner change, or F10),
// rather than polling every window message. A false result is diagnostic only;
// it is never used as a destruction/completion fence.
static void EiemPhysicsRuntimeCheckReady() {
  if (!kEiemEnableExperimentalPhysicsRuntime) return;
  for (size_t index = 0; index < s_eiemPhysicsRuntimePendingReady.size();) {
    const auto &instance = s_eiemPhysicsRuntimePendingReady[index];
    if (instance->retiring || instance->ready) {
      s_eiemPhysicsRuntimePendingReady.erase(
          s_eiemPhysicsRuntimePendingReady.begin() + index);
      continue;
    }
    bool ready = !instance->components.empty();
    for (auto &runtime : instance->components) {
      if (runtime.componentRef.Status() != 1) {
        ready = false;
        continue;
      }
      void *process = nullptr;
      if (!EiemPhysicsReadField(runtime.component,
                                s_eiemPhysicsRuntimeApi.processField,
                                process) ||
          !process) {
        ready = false;
        continue;
      }
      runtime.process = process;
      if (!runtime.processRef)
        runtime.processRef = EiemUnityRef::Capture(process, false);
      bool valid = false, running = false;
      int32_t team = -1;
      void *animator = nullptr;
      if (!EiemPhysicsReadValue(process,
                                s_eiemPhysicsRuntimeApi.processValid, valid) ||
          !EiemPhysicsReadValue(process,
                                s_eiemPhysicsRuntimeApi.processRunning,
                                running) ||
          !EiemPhysicsReadValue(process,
                                s_eiemPhysicsRuntimeApi.processTeamId, team) ||
          !EiemPhysicsReadField(process,
                                s_eiemPhysicsRuntimeApi.processAnimatorField,
                                animator) ||
          !valid || !running || team < 0 || animator != instance->animator) {
        ready = false;
        continue;
      }
      runtime.teamId = team;
    }
    if (!ready) {
      ++index;
      continue;
    }
    instance->ready = true;

    std::string teams;
    for (const auto &component : instance->components) {
      if (!teams.empty()) teams += ',';
      teams += std::to_string(component.teamId);
    }
    Log("%s ready generation=%llu model=%p animator=%p teams=%s key=%s",
        EiemPhysicsRuntimeTag, (unsigned long long)instance->generation,
        instance->model, instance->animator, teams.c_str(),
        instance->key.c_str());
    s_eiemPhysicsRuntimePendingReady.erase(
        s_eiemPhysicsRuntimePendingReady.begin() + index);
  }
}

// This is deliberately event driven. The caller has just crossed a model or
// configuration lifecycle boundary, so it is safe to drain off-thread release
// notices, collect objects whose Unity destruction has completed, and perform
// one readiness observation. No candidate scan or timer is involved.
static void EiemPhysicsRuntimeBoundary(const char *stage) {
  if (!kEiemEnableExperimentalPhysicsRuntime) return;
  if (!EiemOnUnityThread()) return;
  const ULONGLONG started = GetTickCount64();
  EiemPhysicsRuntimeDrainReleases();
  EiemPhysicsRuntimePruneFailures();
  const size_t retiredCount = EiemPhysicsRuntimeCollect();
  EiemPhysicsRuntimeCheckReady();
  const ULONGLONG elapsed = GetTickCount64() - started;
  if (elapsed >= 4)
    Log("[PERF] physics-boundary elapsed=%llums retired=%zu runtimes=%zu stage=%s",
        elapsed, retiredCount, s_eiemPhysicsRuntimeInstances.size(),
        stage ? stage : "unknown");
}
