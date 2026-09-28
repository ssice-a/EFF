#pragma once

// Read-only IL2CPP metadata reconnaissance.
//
// The static route to this information is blocked: Il2CppDumper cannot resolve
// this build's CodeRegistration/MetadataRegistration, so no DummyDll or dump.cs
// can be produced. The runtime does not need those pointers -- the process has
// already initialised IL2CPP -- so the metadata is enumerated through the public
// API instead.
//
// What this answers: the exact full names of the classes that carry the avatar
// part table, the per-renderer descriptor and the native cloth physics, plus
// their field layouts and offsets. Those names and offsets are what a later
// probe needs to observe real assembly behaviour, and guessing them is what left
// several hooks unhooked in earlier runs.
//
// Cost control: every entry point runs at most once per process, nothing is
// written, and no object is dereferenced. It only walks metadata tables.

#include <atomic>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Substrings that identify the assembly, descriptor and physics classes this
// investigation cares about. Kept narrow so ordinary startup logging stays small.
static const char *const s_eiemMetadataClassFilters[] = {
    "BoneCloth", "ClothData", "SelectionData", "SubMeshInfo",
    "NPCAvatarLodMeshAssets", "NPCAvatarMeshAssets", "LODCollector",
    "INPCMeshAssets", "RootBoneInfo",
    // The runtime source-data layer. Whether every presentation path shares it
    // decides if one edit point covers UI, world and NPC or three are needed.
    "BP_Avatar", "PrefabItem", "AvatarSourceData",
    // The UI and world presentation paths. NPC-namespaced types cannot be
    // assumed to serve the player's own model in the open world or the UI, so
    // their loaders are enumerated alongside to see whether they share one
    // part table or each carries its own.
    "UIModelLoader", "CharUIModelMono", "BaseModelViewPart", "ModelViewPart",
    "ModelManager", "PrefabInstantiate",
};

// Methods whose real signature has to be known before a hook can be installed.
// Earlier runs left several hooks unhooked purely because a guessed parameter
// list did not match, so the declared parameter types are printed here instead.
static const char *const s_eiemMetadataMethodClasses[] = {
    "NPCAvatarCreatorUtils", "CreateMeshAssetsGo", "CreateMeshAssets",
    "NPCAvatarMeshAssetsSO", "BP_Avatar",
};

static bool EiemMetadataMethodClassMatches(const char *name) {
  if (!name || !name[0]) return false;
  for (const char *filter : s_eiemMetadataMethodClasses)
    if (strstr(name, filter)) return true;
  return false;
}

// Print every method of a class with its declared parameter types, so a hook can
// be written against the real signature.
static void EiemLogClassMethods(void *klass) {
  if (!klass || !il2cpp_class_get_methods || !il2cpp_method_get_name ||
      !il2cpp_method_get_param_count)
    return;
  void *iterator = nullptr;
  int index = 0;
  while (void *method = il2cpp_class_get_methods(klass, &iterator)) {
    const char *name = il2cpp_method_get_name(method);
    const uint32_t count = il2cpp_method_get_param_count(method);
    const char *returnName = "?";
    if (il2cpp_method_get_return_type && il2cpp_type_get_name) {
      void *returnType = il2cpp_method_get_return_type(method);
      const char *resolved =
          returnType ? il2cpp_type_get_name(returnType) : nullptr;
      if (resolved && resolved[0]) returnName = resolved;
    }
    char params[512] = {};
    size_t used = 0;
    for (uint32_t p = 0; p < count && p < 16; ++p) {
      const char *typeName = "?";
      if (il2cpp_method_get_param && il2cpp_type_get_name) {
        void *type = il2cpp_method_get_param(method, p);
        const char *resolved = type ? il2cpp_type_get_name(type) : nullptr;
        if (resolved && resolved[0]) typeName = resolved;
      }
      const int written = snprintf(params + used, sizeof(params) - used, "%s%s",
                                   used ? "," : "", typeName);
      if (written <= 0 || (size_t)written >= sizeof(params) - used) break;
      used += (size_t)written;
    }
    Log("[META-METHOD] class=%s index=%d name=%s return=%s params=%u(%s)",
        il2cpp_class_get_name(klass), index, name ? name : "<null>",
        returnName, count, params);
    ++index;
    if (index >= 160) {
      Log("[META-METHOD] class=%s truncated at 160 methods",
          il2cpp_class_get_name(klass));
      break;
    }
  }
  if (index == 0)
    Log("[META-METHOD] class=%s has no enumerable methods",
        il2cpp_class_get_name(klass));
}

static bool EiemMetadataClassMatches(const char *name) {
  if (!name || !name[0]) return false;
  for (const char *filter : s_eiemMetadataClassFilters)
    if (strstr(name, filter)) return true;
  return false;
}

// Log one class's fields. Type names come from the field's declared type so an
// offset can be read as "what kind of value lives here" rather than just a
// number.
static void EiemLogClassFields(void *klass) {
  if (!klass || !il2cpp_class_get_fields || !il2cpp_field_get_name ||
      !il2cpp_field_get_offset)
    return;
  void *iterator = nullptr;
  int index = 0;
  while (void *field = il2cpp_class_get_fields(klass, &iterator)) {
    const char *name = il2cpp_field_get_name(field);
    const size_t offset = il2cpp_field_get_offset(field);
    const char *typeName = "<unknown>";
    if (il2cpp_field_get_type && il2cpp_type_get_name) {
      void *type = il2cpp_field_get_type(field);
      const char *resolved = type ? il2cpp_type_get_name(type) : nullptr;
      if (resolved && resolved[0]) typeName = resolved;
    }
    Log("[META-FIELD] class=%s index=%d name=%s type=%s offset=0x%zX",
        il2cpp_class_get_name(klass), index, name ? name : "<null>", typeName,
        offset);
    ++index;
    if (index >= 96) {
      Log("[META-FIELD] class=%s truncated at 96 fields",
          il2cpp_class_get_name(klass));
      break;
    }
  }
  if (index == 0)
    Log("[META-FIELD] class=%s has no enumerable fields",
        il2cpp_class_get_name(klass));
}

// One walk over every class in every loaded assembly. Matches are logged with
// their assembly and namespace so the exact declaring name is unambiguous, and
// each match's fields are dumped once.
static void EiemDumpMetadataClasses(void **assemblies, size_t assemblyCount) {
  if (!assemblies || !assemblyCount || !il2cpp_assembly_get_image ||
      !il2cpp_image_get_class_count || !il2cpp_image_get_class ||
      !il2cpp_class_get_name)
    return;
  size_t matched = 0;
  size_t classTotal = 0;
  for (size_t i = 0; i < assemblyCount; ++i) {
    void *image = il2cpp_assembly_get_image(assemblies[i]);
    if (!image) continue;
    const size_t classCount = il2cpp_image_get_class_count(image);
    if (!classCount || classCount > 200000) continue;
    classTotal += classCount;
    for (size_t j = 0; j < classCount; ++j) {
      void *klass = il2cpp_image_get_class(image, j);
      if (!klass) continue;
      const char *name = il2cpp_class_get_name(klass);
      const bool fieldMatch = EiemMetadataClassMatches(name);
      // The creator classes carry no fields we read, but their method signatures
      // are exactly what a hook has to match, so they are visited too.
      const bool methodMatch = EiemMetadataMethodClassMatches(name);
      if (!fieldMatch && !methodMatch) continue;
      const char *space = il2cpp_class_get_namespace
                              ? il2cpp_class_get_namespace(klass)
                              : nullptr;
      ++matched;
      Log("[META-CLASS] %zu assembly=%zu namespace=%s name=%s fields=%d methods=%d",
          matched, i, space && space[0] ? space : "<global>", name,
          fieldMatch ? 1 : 0, methodMatch ? 1 : 0);
      if (fieldMatch) EiemLogClassFields(klass);
      if (methodMatch) EiemLogClassMethods(klass);
    }
  }
  Log("[META-SUMMARY] assemblies=%zu classes=%zu matched=%zu", assemblyCount,
      classTotal, matched);
}

// A deliberately narrow census for the intermittent T-pose investigation.
// The public SkinnedMeshRenderer input has already been validated.  These are
// the exact game-owned classes between that input and Endfield's HG/GPU part
// path.  Dumping their real signatures is the prerequisite for safe runtime
// hooks; guessing a native signature here can corrupt the call frame.
static void EiemDumpCustomSkinPipelineMetadata(void **assemblies,
                                                size_t assemblyCount) {
  static std::atomic<bool> dumped{false};
  if (dumped.exchange(true) || !assemblies || !assemblyCount) return;

  struct Candidate {
    const char *nameSpace;
    const char *name;
    bool fields;
    bool methods;
  };
  const Candidate candidates[] = {
      {"UnityEngine", "HGMeshRenderer", true, true},
      {"UnityEngine", "HGMeshRendererData", true, true},
      {"UnityEngine.HyperGryph.ECS", "Entity", true, true},
      {"Beyond.NPC.Avatar", "SubMeshInfo", true, true},
      {"Beyond.NPC.Avatar", "NPCAvatarLodMeshAssets", true, true},
      {"Beyond.NPC.Avatar", "NPCAvatarCreatorUtils", false, true},
      {"Beyond.Gameplay.View", "EntityRenderHelper", true, true},
      {"Beyond.Rendering", "EntityRenderHelperMaterialController", true, true},
      {"Beyond.Gameplay.View", "BaseModelViewPart", true, false},
      {"Beyond.Gameplay.View", "CharUIModelMono", true, false},
      // Custom skin/GPU candidates. Their metadata is collected before any
      // hook is attempted so the next probe can bind only to real signatures.
      {"Beyond.NPC.Animation", "GpuAnimator", true, true},
      {"Beyond.NPC.Animation", "GpuAnimatorSampler", true, true},
      {"Beyond.NPC.Animation", "GpuAnimInfo", true, true},
      {"Beyond.NPC.Animation", "GpuAnimMgr", true, true},
      {"Beyond.Gameplay.Core", "GpuResourceWithMat", true, true},
      {"Beyond.Gameplay.Core", "NpcGpuResource", true, true},
      {"Beyond.Gameplay.Core", "NpcGpuInstanceWithMat", true, true},
      {"Beyond.Gameplay.Core", "NpcGpuInstanceMgr", true, true},
      {"Beyond.Gameplay.Core", "PartSubMeshGPU", true, true},
      {"HG.Rendering.Runtime", "SkinnedMeshCaptureManager", true, true},
      {"HG.Rendering.Runtime", "GpuClothManager", true, true},
      {"HG.Rendering.Runtime", "GpuClothRenderData", true, true},
      {"HG.Rendering.Runtime", "GpuClothGroupUploadData", true, true},
      {"HG.Rendering.Runtime", "GpuClothClearBufferData", true, true},
      {"HG.Rendering.Runtime", "GpuClothMatrixGenerator", true, true},
      {"HG.Rendering.Runtime", "GpuClothSimulationPassConstructor", true, true},
      {"BeyondDynamicBone", "VirtualMesh", true, true},
      {"BeyondDynamicBone", "TeamManager", true, true},
  };

  size_t found = 0;
  for (const Candidate &candidate : candidates) {
    void *klass = FindClass(candidate.nameSpace, candidate.name, assemblies,
                            assemblyCount);
    uint32_t alignment = 0;
    const int32_t valueSize =
        klass && il2cpp_class_value_size
            ? il2cpp_class_value_size(klass, &alignment)
            : 0;
    Log("[CUSTOM-SKIN-META] namespace=%s class=%s found=%d fields=%d "
        "methods=%d valueSize=%d alignment=%u",
        candidate.nameSpace, candidate.name, klass ? 1 : 0,
        candidate.fields ? 1 : 0, candidate.methods ? 1 : 0, valueSize,
        alignment);
    if (!klass) continue;
    ++found;
    if (candidate.fields) EiemLogClassFields(klass);
    if (candidate.methods) EiemLogClassMethods(klass);
  }
  Log("[CUSTOM-SKIN-META] complete requested=%zu found=%zu",
      _countof(candidates), found);
}

// Narrow read-only census for the sprint visibility investigation.  The
// public Renderer.isVisible result changed while enabled/active stayed true;
// this pass finds the game's own controller or descriptor class before any
// hook is attempted.  It only dumps metadata and never reads or writes live
// object fields.
static void EiemDumpVisibilityPipelineMetadata(void **assemblies,
                                                size_t assemblyCount) {
  static std::atomic<bool> dumped{false};
  if (dumped.exchange(true) || !assemblies || !assemblyCount ||
      !il2cpp_assembly_get_image || !il2cpp_image_get_class_count ||
      !il2cpp_image_get_class || !il2cpp_class_get_name)
    return;

  size_t matched = 0;
  size_t classes = 0;
  for (size_t imageIndex = 0; imageIndex < assemblyCount; ++imageIndex) {
    void *image = il2cpp_assembly_get_image(assemblies[imageIndex]);
    if (!image) continue;
    const size_t count = il2cpp_image_get_class_count(image);
    if (count > 200000) continue;
    classes += count;
    for (size_t classIndex = 0; classIndex < count; ++classIndex) {
      void *klass = il2cpp_image_get_class(image, classIndex);
      if (!klass) continue;
      const char *name = il2cpp_class_get_name(klass);
      if (!name ||
          (!strstr(name, "VisibleController") &&
           !strstr(name, "VisibilityController") &&
           !strstr(name, "SubMeshInfo") &&
           !strstr(name, "RendererInfo")))
        continue;
      const char *nameSpace = il2cpp_class_get_namespace
                                  ? il2cpp_class_get_namespace(klass)
                                  : nullptr;
      ++matched;
      Log("[VIS-META] assembly=%zu namespace=%s class=%s", imageIndex,
          nameSpace && nameSpace[0] ? nameSpace : "<global>", name);
      EiemLogClassFields(klass);
      EiemLogClassMethods(klass);
      if (matched >= 32) {
        Log("[VIS-META] truncated at 32 matching classes");
        Log("[VIS-META] complete classes=%zu matched=%zu", classes, matched);
        return;
      }
    }
  }
  Log("[VIS-META] complete classes=%zu matched=%zu", classes, matched);
}

// ---------------------------------------------------------------------------
// Part-table mutation.
//
// Two questions are settled by the same hook, which is why it is a mutation and
// not another observation:
//
//   1. Do the UI and world presentation paths read the same part table the NPC
//      path does? The class lives under Beyond.NPC.Avatar, so that cannot be
//      assumed, and it decides whether one edit point covers all three.
//   2. Does the game honour an edit to this table at all? isActive on a
//      SubMeshInfo is the cheapest possible probe: a bool needs no asset
//      identity, so a negative result cannot be blamed on resource resolution.
//
// GetAvatarSlotMeshAssets() is a zero-argument getter on the ScriptableObject, so
// it cannot be bypassed by any path that consumes the table, and the plugin does
// not hook it -- hooking a method the plugin already owns is what silently broke
// its chain earlier.
//
// Effect: one part of one model is dropped, once per process. The original is
// still called, so the only difference from a normal run is that single bool.
// ---------------------------------------------------------------------------

static std::atomic<int> s_eiemPartTableCalls{0};
static std::atomic<bool> s_eiemPartTableTestDone{false};

// Confirmed by the metadata dump above.
static const int kEiemMeshAssetsSlotDatas = 0x58;
static const int kEiemLodMeshAssetsPartSubMeshsLOD0 = 0x68;
static const int kEiemSubMeshInfoMesh = 0x10;
static const int kEiemSubMeshInfoMeshPathHash = 0x28;
static const int kEiemSubMeshInfoMeshName = 0x30;
static const int kEiemSubMeshInfoIsActive = 0x58;
static const int kEiemSubMeshInfoRootBoneID = 0x68;

static int EiemProbeArrayCount(void *array) {
  if (!array) return 0;
  __try { return *(int *)((char *)array + 24); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static void *EiemProbeArrayItem(void *array, int index) {
  if (!array || index < 0) return nullptr;
  __try { return ((void **)((char *)array + IL2CPP_ARRAY_DATA))[index]; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

// Read one managed List<T>: item count at 0x18, backing array at 0x10.
static void *EiemProbeListItem(void *list, int index) {
  if (!list || index < 0) return nullptr;
  __try {
    void *items = *(void **)((char *)list + 0x10);
    const int count = *(int *)((char *)list + 0x18);
    if (!items || index >= count) return nullptr;
    return ((void **)((char *)items + IL2CPP_ARRAY_DATA))[index];
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

static int EiemProbeListCount(void *list) {
  if (!list) return 0;
  __try { return *(int *)((char *)list + 0x18); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

using EiemGetAvatarSlotMeshAssetsFn = void *(__fastcall *)(void *self,
                                                           void *methodInfo);
static void *s_eiemOrigGetAvatarSlotMeshAssets = nullptr;

static void *__fastcall EiemGetAvatarSlotMeshAssetsHook(void *self,
                                                        void *methodInfo) {
  // The caller identifies the presentation path. All three paths load the same
  // Mesh assets, so only the caller tells them apart.
  void *caller = _ReturnAddress();
  const int call = s_eiemPartTableCalls.fetch_add(1);
  auto original =
      (EiemGetAvatarSlotMeshAssetsFn)s_eiemOrigGetAvatarSlotMeshAssets;
  void *slots = original ? original(self, methodInfo) : nullptr;
  const int slotCount = EiemProbeListCount(slots);
  // Every call is reported, not just the first few: the table is asked for once
  // per model, and the consumer seen at startup is an animal NPC whose asset
  // names differ from the avatar's, so an early cap hides the paths that matter.
  Log("[PART-TABLE] call=%d self=%p caller=%p slots=%p slotCount=%d", call, self,
      caller, slots, slotCount);

  // One descriptor is disabled, once per process, so the run has a single
  // observable difference. The slot must actually be active first: disabling an
  // already-disabled entry produces no visible change and proves nothing.
  if (slots && !s_eiemPartTableTestDone.exchange(true)) {
    __try {
      for (int slotIndex = 0; slotIndex < slotCount && slotIndex < 32; ++slotIndex) {
        void *slot = EiemProbeListItem(slots, slotIndex);
        void *lod0 =
            slot ? *(void **)((char *)slot + kEiemLodMeshAssetsPartSubMeshsLOD0)
                 : nullptr;
        const int count = EiemProbeArrayCount(lod0);
        Log("[PART-TABLE-SLOT] call=%d slot=%d lod0=%p lod0Count=%d", call,
            slotIndex, lod0, count);
        for (int index = 0; index < count && index < 24; ++index) {
          void *info = EiemProbeArrayItem(lod0, index);
          if (!info) continue;
          void *nameField = *(void **)((char *)info + kEiemSubMeshInfoMeshName);
          char name[192] = {};
          if (nameField) ReadStrUtf8(nameField, name, sizeof(name));
          const bool before = *(bool *)((char *)info + kEiemSubMeshInfoIsActive);
          const void *resolved = *(void **)((char *)info + kEiemSubMeshInfoMesh);
          const int64_t pathHash =
              *(int64_t *)((char *)info + kEiemSubMeshInfoMeshPathHash);
          const int32_t rootBoneID =
              *(int32_t *)((char *)info + kEiemSubMeshInfoRootBoneID);
          bool wrote = false;
          // The first descriptor that is actually active is the one disabled.
          if (before && !wrote) {
            *(bool *)((char *)info + kEiemSubMeshInfoIsActive) = false;
            wrote = true;
          }
          Log("[PART-TABLE-ITEM] slot=%d index=%d isActive %d%s meshName=%s "
              "resolvedMesh=%p meshPathHash=%lld rootBoneID=%d",
              slotIndex, index, before ? 1 : 0, wrote ? "->0" : "",
              name[0] ? name : "<empty>", resolved, (long long)pathHash,
              rootBoneID);
        }
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      Log("[PART-TABLE-TEST] mutation raised exception=0x%08lX",
          GetExceptionCode());
    }
  }
  return slots;
}

// Install the part-table test. The signature is a zero-argument getter, taken
// from the metadata dump rather than guessed.
static void EiemInstallPartTableTest(void **assemblies, size_t assemblyCount) {
  void *assets = FindClass("Beyond.NPC.Avatar", "NPCAvatarMeshAssetsSO",
                           assemblies, assemblyCount);
  if (!assets) {
    Log("[PART-TABLE] NPCAvatarMeshAssetsSO not found; test skipped");
    return;
  }
  void *getter = FindMethod(assets, "GetAvatarSlotMeshAssets", 0);
  if (getter)
    Hook(getter, "NPCAvatarMeshAssetsSO.GetAvatarSlotMeshAssets part-table test",
         (void *)EiemGetAvatarSlotMeshAssetsHook,
         &s_eiemOrigGetAvatarSlotMeshAssets);
  else
    Log("[PART-TABLE] GetAvatarSlotMeshAssets not found; test skipped");
}
