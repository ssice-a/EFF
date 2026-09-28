#pragma once

#include <cstring>

// Native Mesh replacement binding.
//
// A generated Mesh does not carry the Transform objects that animated the
// source Renderer. The source Renderer does. At each model assembly boundary
// EiemCaptureSourceSkinPalette snapshots every original
// SkinnedMeshRenderer.bones array. A replacement slot is then resolved from
// the exported source candidates (asset/path + slot) against that snapshot.
// This keeps the binding instance-local, works across LODs, and avoids using
// unstable runtime child-index paths or Transform names.

static bool EiemSourcePaletteMatches(
    const EiemSkinPaletteCache::SourcePalette &palette,
    const EiemSkinSourceCandidate &candidate) {
  if (!palette.meshAsset.empty() && !candidate.meshAsset.empty() &&
      _stricmp(palette.meshAsset.c_str(), candidate.meshAsset.c_str()) != 0)
    return false;
  if (!candidate.meshPath.empty() && !palette.meshPath.empty() &&
      !EiemModSameLogicalPath(palette.meshPath.c_str(),
                              candidate.meshPath.c_str()))
    return false;
  // An exporter may not have an origin path for an engine-generated asset;
  // asset identity is still sufficient in that case. If both are absent the
  // candidate cannot safely identify a donor palette.
  return (!palette.meshAsset.empty() && !candidate.meshAsset.empty()) ||
         (!palette.meshPath.empty() && !candidate.meshPath.empty());
}

static bool EiemCaptureSourceSkinPalette(
    const std::vector<void *> &renderers,
    EiemSkinPaletteCache *cache) {
  if (!cache || !g_smr_get_bones) return false;
  cache->sourcePalettes.clear();
  for (void *renderer : renderers) {
    if (!renderer || EiemNativeObjectStatus(renderer) != 1) continue;
    void *mesh = g_smr_get_sharedMesh
                     ? EiemBackendInvokeNoThrow(g_smr_get_sharedMesh, renderer)
                     : nullptr;
    void *bones = EiemBackendInvokeNoThrow(g_smr_get_bones, renderer);
    // F10/reconcile can enter with an earlier EIEM generation still attached
    // to a Renderer. Prefer the ledger's original Mesh and palette in that
    // case, so a replacement never becomes a donor for the next generation.
    AcquireSRWLockShared(&s_eiemOverrideLock);
    const size_t overrideIndex = EiemFindOverrideLocked(renderer);
    if (overrideIndex != SIZE_MAX) {
      const auto &state = s_eiemOverrides[overrideIndex];
      if (state.originalMesh && state.originalMesh != mesh)
        mesh = state.originalMesh;
      if (state.originalBonesHandle && il2cpp_gchandle_get_target)
        bones = il2cpp_gchandle_get_target(state.originalBonesHandle);
    }
    ReleaseSRWLockShared(&s_eiemOverrideLock);
    if (!mesh || EiemNativeObjectStatus(mesh) != 1) continue;
    const size_t count = EiemManagedArrayLength(bones);
    if (!bones || !count || count > 4096) continue;
    void **items = (void **)((char *)bones + IL2CPP_ARRAY_DATA);
    EiemSkinPaletteCache::SourcePalette palette;
    palette.renderer = renderer;
    palette.mesh = mesh;
    char source[768] = {};
    char asset[192] = {};
    if (!EiemReadLiveMeshIdentity(mesh, source, sizeof(source), asset,
                                  sizeof(asset)))
      continue;
    palette.meshPath = source;
    palette.meshAsset = asset;
    palette.bones.assign(items, items + count);
    bool valid = true;
    for (void *bone : palette.bones) {
      if (!bone || EiemNativeObjectStatus(bone) != 1) {
        valid = false;
        break;
      }
    }
    if (!valid) continue;
    // Avoid duplicate palettes when the same original Renderer is included
    // through more than one ownership list, but keep distinct LOD sources.
    bool duplicate = false;
    for (const auto &existing : cache->sourcePalettes) {
      if (existing.renderer == palette.renderer) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) cache->sourcePalettes.push_back(std::move(palette));
  }
  if (kEiemEnableSkinBindingDiagnostics)
    Log("[MOD-SKIN-SOURCE-v2] model=%p palettes=%zu", cache->model,
        cache->sourcePalettes.size());
  return !cache->sourcePalettes.empty();
}

static bool EiemResolveMeshBonesFromSourcePalettes(
    const EiemSkinIdentity &identity, void *renderer, void **out,
    char *error, size_t errorSize) {
  if (out) *out = nullptr;
  auto reject = [&](const std::string &message) {
    if (error) strncpy_s(error, errorSize, message.c_str(), _TRUNCATE);
    return false;
  };
  if (!renderer || !il2cpp_array_new || !g_transformClass)
    return reject("Native source Renderer binding APIs are unavailable");
  if (identity.sourceCandidates.empty())
    return reject("Mesh has no source Renderer bone candidate table");
  EiemSkinPaletteCache *cache = s_eiemActiveSkinPaletteCache;
  if (!cache || cache->sourcePalettes.empty())
    return reject("No original Renderer bone palettes were captured");

  std::vector<void *> resolved;
  resolved.reserve(identity.sourceCandidates.size());
  for (size_t slot = 0; slot < identity.sourceCandidates.size(); ++slot) {
    const auto &candidates = identity.sourceCandidates[slot];
    void *selected = nullptr;
    bool found = false;
    // Prefer the original palette belonging to this exact target Renderer.
    // This disambiguates a model that legitimately contains two Renderers
    // using the same source Mesh while still allowing a replacement LOD to
    // borrow a sibling source palette when the target has no direct match.
    auto collect = [&](bool preferredOnly) {
      for (const auto &candidate : candidates) {
        for (const auto &palette : cache->sourcePalettes) {
          if (preferredOnly && palette.renderer != renderer) continue;
          if (!EiemSourcePaletteMatches(palette, candidate) ||
              candidate.slot >= palette.bones.size())
            continue;
          void *bone = palette.bones[candidate.slot];
          if (!bone || EiemNativeObjectStatus(bone) != 1) continue;
          if (found && selected != bone)
            return false;
          selected = bone;
          found = true;
        }
      }
      return true;
    };
    if (!collect(true))
      return reject("Ambiguous source Renderer bone candidates for slot " +
                    std::to_string(slot));
    if (!found && !collect(false))
      return reject("Ambiguous source Renderer bone candidates for slot " +
                    std::to_string(slot));
    if (!found)
      return reject("No source Renderer bone candidate resolved for slot " +
                    std::to_string(slot));
    resolved.push_back(selected);
  }
  void *array = il2cpp_array_new(g_transformClass, resolved.size());
  if (!array) return reject("Unable to allocate source Renderer bone palette");
  memcpy((char *)array + IL2CPP_ARRAY_DATA, resolved.data(),
         resolved.size() * sizeof(void *));
  if (out) *out = array;
  if (kEiemEnableSkinBindingDiagnostics)
    Log("[MOD-SKIN-SOURCE-v2] renderer=%p model=%p slots=%zu", renderer,
        cache->model, resolved.size());
  return true;
}

// Mesh-only replacement uses the source Renderer palette. Explicit
// Mod-owned Skeleton resources take their separate path in
// EiemSkeletonMeshBones and may still use their own serialized node table.
static bool EiemResolveMeshBonesFromNativeInstance(
    const EiemSkinIdentity &identity, void *renderer, void **out,
    char *error, size_t errorSize) {
  return EiemResolveMeshBonesFromSourcePalettes(identity, renderer, out,
                                                 error, errorSize);
}

// Mesh bindings are instance state. Store the exact replacement array for
// later game setter refreshes; the original array remains separate for F10.
static bool EiemPreserveSourceSkinning(void *renderer, void *bones,
                                        char *error, size_t errorSize) {
  if (!renderer || !bones || !g_smr_get_bones || !g_smr_set_bones ||
      !il2cpp_gchandle_new || !il2cpp_gchandle_free) return false;
  const size_t boneCount=EiemManagedArrayLength(bones);
  void **boneItems=(void **)((char *)bones+IL2CPP_ARRAY_DATA);
  for (size_t i=0; i<boneCount; ++i) if (EiemNativeObjectStatus(boneItems[i])!=1) {
    if (error) strncpy_s(error,errorSize,"Replacement skeleton instance is no longer alive",_TRUNCATE);
    return false;
  }
  const uint32_t handle=il2cpp_gchandle_new(bones,false);
  if (!handle) return false;
  bool same=EiemManagedObjectArraySame(bones,Invoke(g_smr_get_bones,renderer));
  bool ok=true;
  if (!same) {
    void *params[]={bones};
    const bool previous=s_eiemApplyingModMeshAssignment;
    s_eiemApplyingModMeshAssignment=true;
    void *result=nullptr;
    ok=InvokeChecked(g_smr_set_bones,renderer,params,&result);
    s_eiemApplyingModMeshAssignment=previous;
    ok=ok && EiemManagedObjectArraySame(bones,Invoke(g_smr_get_bones,renderer));
  }
  uint32_t old=0;
  if (ok) {
    AcquireSRWLockExclusive(&s_eiemOverrideLock);
    const size_t index=EiemFindOverrideLocked(renderer);
    if (index!=SIZE_MAX) {
      old=s_eiemOverrides[index].replacementBonesHandle;
      s_eiemOverrides[index].replacementBonesHandle=handle;
    } else ok=false;
    ReleaseSRWLockExclusive(&s_eiemOverrideLock);
  }
  if (!ok) {
    il2cpp_gchandle_free(handle);
    if (error) strncpy_s(error,errorSize,"Replacement bone palette assignment failed",_TRUNCATE);
  }
  if (old) il2cpp_gchandle_free(old);
  if (kEiemEnableSkinBindingDiagnostics)
    Log("[MOD-SKIN] renderer=%p slots=%zu changed=%d applied=%d", renderer,
        EiemManagedArrayLength(bones), !same, ok);
  return ok;
}

// The game can finish or rebuild a Renderer skin after EIEM first replaced
// its Mesh. The incoming game-owned palette/root are the baseline that F10
// must restore. Keep that baseline separate from EIEM's replacement palette;
// otherwise a reload restores the early construction snapshot and the affected
// Renderer can fall back to an unanimated/T-pose binding.
