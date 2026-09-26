#pragma once

// Completed game assembly snapshots and exact Mesh donor resolution.
static void EiemRememberAssemblyBoneSnapshot(void *renderers, void *rootBones,
                                             int32_t lod, LONG generation) {
  if (!renderers || !rootBones) {
    static LONG loggedNullInput = 0;
    if (InterlockedCompareExchange(&loggedNullInput, 1, 0) == 0)
      Log("[DEBUG-SNAPSHOT] reason=null-input renderers=%p rootBones=%p lod=%d generation=%ld",
          renderers, rootBones, lod, generation);
    return;
  }
  const size_t count = EiemManagedArrayLength(rootBones);
  if (!count || count > 16384) {
    static LONG loggedRootCount = 0;
    if (InterlockedCompareExchange(&loggedRootCount, 1, 0) == 0)
      Log("[DEBUG-SNAPSHOT] reason=root-count-invalid renderers=%p rootBones=%p rootCount=%zu lod=%d generation=%ld",
          renderers, rootBones, count, lod, generation);
    return;
  }
  const size_t rendererCount = EiemManagedArrayLength(renderers);
  if (rendererCount != count) {
    static LONG loggedCountMismatch = 0;
    if (InterlockedCompareExchange(&loggedCountMismatch, 1, 0) == 0)
      Log("[DEBUG-SNAPSHOT] reason=array-count-mismatch renderers=%p rendererCount=%zu rootBones=%p rootCount=%zu lod=%d generation=%ld",
          renderers, rendererCount, rootBones, count, lod, generation);
    return;
  }
  EiemAssemblyBoneSnapshot snapshot;
  snapshot.rendererArray = renderers;
  snapshot.rootBonesArray = rootBones;
  snapshot.generation = generation;
  snapshot.lod = lod;
  const size_t elementSize = EiemManagedArrayValueElementSize(rootBones, nullptr);
  if (elementSize < 16) {
    static LONG loggedElementSize = 0;
    if (InterlockedCompareExchange(&loggedElementSize, 1, 0) == 0)
      Log("[DEBUG-SNAPSHOT] reason=element-size-invalid renderers=%p count=%zu rootBones=%p elementSize=%zu lod=%d generation=%ld",
          renderers, count, rootBones, elementSize, lod, generation);
    return;
  }
  snapshot.rootInfos.resize(count);
  for (size_t index = 0; index < count; ++index)
    EiemReadRootBoneInfoAt(rootBones, index, elementSize, count,
                           &snapshot.rootInfos[index]);
  void **rendererItems = (void **)((char *)renderers + IL2CPP_ARRAY_DATA);
  snapshot.renderers.assign(rendererItems, rendererItems + rendererCount);
  AcquireSRWLockExclusive(&s_eiemAssemblyBoneLock);
  auto found = std::find_if(s_eiemAssemblyBoneSnapshots.begin(),
                            s_eiemAssemblyBoneSnapshots.end(),
                            [&](const EiemAssemblyBoneSnapshot &value) {
                              return value.rendererArray == renderers;
                            });
  if (found == s_eiemAssemblyBoneSnapshots.end())
    s_eiemAssemblyBoneSnapshots.push_back(std::move(snapshot));
  else
    *found = std::move(snapshot);
  if (s_eiemAssemblyBoneSnapshots.size() > 128)
    s_eiemAssemblyBoneSnapshots.erase(s_eiemAssemblyBoneSnapshots.begin());
  ReleaseSRWLockExclusive(&s_eiemAssemblyBoneLock);
  Log("[MOD-SKIN-INSTANCE] array=%p rootBones=%p lod=%d bones=%zu generation=%ld",
      renderers, rootBones, lod, count, generation);

  if (g_smr_get_bones) {
    void **rendererItems = (void **)((char *)renderers + IL2CPP_ARRAY_DATA);
    AcquireSRWLockExclusive(&s_eiemAssemblyBoneLock);
    for (size_t index = 0; index < rendererCount; ++index) {
      void *renderer = rendererItems[index];
      if (!renderer) continue;
      void *bones = EiemBackendInvokeNoThrow(g_smr_get_bones, renderer);
      if (!bones || !EiemManagedArrayLength(bones)) continue;
      auto foundRenderer = std::find_if(
          s_eiemRendererBoneSnapshots.begin(), s_eiemRendererBoneSnapshots.end(),
          [&](const EiemRendererBoneSnapshot &value) {
            return value.renderer == renderer;
          });
      EiemRendererBoneSnapshot value{renderer, bones, generation, lod};
      if (foundRenderer == s_eiemRendererBoneSnapshots.end())
        s_eiemRendererBoneSnapshots.push_back(value);
      else
        *foundRenderer = value;
    }
    if (s_eiemRendererBoneSnapshots.size() > 4096)
      s_eiemRendererBoneSnapshots.erase(
          s_eiemRendererBoneSnapshots.begin(),
          s_eiemRendererBoneSnapshots.begin() + 1024);
    ReleaseSRWLockExclusive(&s_eiemAssemblyBoneLock);
  }
}

static bool EiemResolveMeshBonesFromAssembly(
    const EiemSkinIdentity &identity, void *renderer, void **out,
    char *error, size_t errorSize) {
  if (out) *out = nullptr;
  if (!renderer || identity.sourceCandidates.empty() ||
      !g_smr_get_bones ||
      !il2cpp_array_new || !g_transformClass)
    return false;
  auto reject = [&](const char *message) {
    if (error) strncpy_s(error, errorSize, message, _TRUNCATE);
    return false;
  };
  auto sourceMatchesMeshIdentity =
      [](const EiemSkinIdentity::Source &source, const char *meshPath,
         const char *meshAsset) {
        if (!source.meshAsset.empty())
          return meshAsset && meshAsset[0] &&
                 EiemModEquals(source.meshAsset.c_str(), meshAsset);
        return !source.meshPath.empty() && meshPath && meshPath[0] &&
               EiemModSameLogicalPath(source.meshPath.c_str(), meshPath);
      };
  void *targetRootBone = g_smr_get_rootBone
                             ? EiemBackendInvokeNoThrow(g_smr_get_rootBone,
                                                        renderer)
                             : nullptr;
  void *targetSkinningRoot = g_smr_get_skinningRoot
                                 ? EiemBackendInvokeNoThrow(
                                       g_smr_get_skinningRoot, renderer)
                                 : nullptr;
  auto sameSkeletonContext = [&](void *candidateRenderer) {
    if (!candidateRenderer) return false;
    if (g_smr_get_rootBone && targetRootBone) {
      void *candidateRoot = EiemBackendInvokeNoThrow(
          g_smr_get_rootBone, candidateRenderer);
      if (!candidateRoot || candidateRoot != targetRootBone) return false;
    }
    if (g_smr_get_skinningRoot && targetSkinningRoot) {
      void *candidateSkinningRoot = EiemBackendInvokeNoThrow(
          g_smr_get_skinningRoot, candidateRenderer);
      if (!candidateSkinningRoot ||
          candidateSkinningRoot != targetSkinningRoot)
        return false;
    }
    return true;
  };
  std::vector<void *> renderers;
  AcquireSRWLockShared(&s_eiemAssemblyBoneLock);
  for (const auto &snapshot : s_eiemAssemblyBoneSnapshots) {
    if (std::find(snapshot.renderers.begin(), snapshot.renderers.end(), renderer) ==
        snapshot.renderers.end()) continue;
    renderers = snapshot.renderers;
    break;
  }
  // Every v6 slot has exact native Mesh donor candidates.
  if (renderers.empty()) {
    void *directBones = nullptr;
    for (const auto &entry : s_eiemRendererBoneSnapshots) {
      if (entry.renderer == renderer) {
        directBones = entry.bones;
        break;
      }
    }
    if (directBones) {
      std::vector<void *> resolved;
      void **items = (void **)((char *)directBones + IL2CPP_ARRAY_DATA);
      const size_t boneCount = EiemManagedArrayLength(directBones);

        void *mesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
        char sourcePath[768] = {}, asset[192] = {};
        EiemReadLiveMeshIdentity(mesh, sourcePath, sizeof(sourcePath), asset,
                                 sizeof(asset));
        for (const auto &candidates : identity.sourceCandidates) {
          void *selected = nullptr;
          for (const auto &source : candidates) {
            const bool matches =
                sourceMatchesMeshIdentity(source, sourcePath, asset);
            if (!matches || source.slot >= boneCount) continue;
            void *bone = items[source.slot];
            if (!bone || EiemNativeObjectStatus(bone) != 1) continue;
            if (selected && selected != bone) {
              ReleaseSRWLockShared(&s_eiemAssemblyBoneLock);
              return reject("Replacement bone source candidates disagree in direct Renderer");
            }
            selected = bone;
          }
          if (!selected) {
            ReleaseSRWLockShared(&s_eiemAssemblyBoneLock);
            return reject("Replacement bone has no native Mesh donor in direct Renderer");
          }
          resolved.push_back(selected);
        }

      ReleaseSRWLockShared(&s_eiemAssemblyBoneLock);
      void *array = il2cpp_array_new(g_transformClass, resolved.size());
      if (!array) return reject("Unable to allocate direct Renderer bone palette");
      memcpy((char *)array + IL2CPP_ARRAY_DATA, resolved.data(),
             resolved.size() * sizeof(void *));
      if (out) *out = array;
      Log("[MOD-SKIN-%s] renderer=%p binding=direct-renderer slots=%zu",
          "V6",
          renderer, resolved.size());
      return true;
    }
  }
  ReleaseSRWLockShared(&s_eiemAssemblyBoneLock);

  // The assembly snapshot is an observation channel.  Some native creation
  // paths (most notably the world model path) do not pass their Renderer
  // array through the snapshot hook, even though this model transaction has
  // already captured every original SkinnedMeshRenderer and its bones[].
  // Treating the optional snapshot as a prerequisite made every replacement
  // fail with "No assembly snapshot" and exposed the source Mesh.  Resolve
  // from the same model-local transaction instead.  This resolver uses only
  // EIEMESH v6 source Mesh identity + original slot records and never
  // matches Transform names or borrows bones from another model instance.
  if (renderers.empty() && s_eiemLiveSkinSources) {
    char liveError[256] = {};
    if (EiemResolveMeshBonesFromNativeInstance(identity, renderer, out,
                                               liveError, sizeof(liveError))) {
      Log("[MOD-SKIN-ASSEMBLY] renderer=%p binding=model-transaction slots=%zu",
          renderer, identity.sourceCandidates.size());
      return true;
    }
    return reject(liveError[0] ? liveError
                               : "No model-local native skeleton donor");
  }
  if (renderers.empty()) return reject("No assembly snapshot for Renderer instance");

  std::vector<void *> resolved;
  resolved.reserve(identity.sourceCandidates.size());
  auto resolveCandidate = [&](const std::vector<EiemSkinIdentity::Source> &candidates,
                              void **selectedOut) -> bool {
    void *selected = nullptr;
    for (const auto &source : candidates) {
      for (void *candidate : renderers) {
        if (!sameSkeletonContext(candidate)) continue;
        void *mesh = EiemReadSharedMesh(candidate, "SkinnedMeshRenderer");
        char sourcePath[768] = {}, asset[192] = {};
        if (!EiemReadLiveMeshIdentity(mesh, sourcePath, sizeof(sourcePath),
                                      asset, sizeof(asset))) continue;
        if (!sourceMatchesMeshIdentity(source, sourcePath, asset)) continue;
        void *bones = EiemBackendInvokeNoThrow(g_smr_get_bones, candidate);
        const size_t boneCount = EiemManagedArrayLength(bones);
        if (!bones || source.slot >= boneCount) continue;
        void **items = (void **)((char *)bones + IL2CPP_ARRAY_DATA);
        void *bone = items[source.slot];
        if (!bone || EiemNativeObjectStatus(bone) != 1) continue;
        if (selected && selected != bone) return false;
        selected = bone;
      }
    }
    if (selectedOut) *selectedOut = selected;
    return selected != nullptr;
  };

    for (const auto &candidates : identity.sourceCandidates) {
      void *selected = nullptr;
      if (!resolveCandidate(candidates, &selected))
        return reject("Replacement bone has no unique native Mesh donor in assembly instance");
      resolved.push_back(selected);
    }

  void *array = il2cpp_array_new(g_transformClass, resolved.size());
  if (!array) return reject("Unable to allocate assembly bone palette");
  memcpy((char *)array + IL2CPP_ARRAY_DATA, resolved.data(),
         resolved.size() * sizeof(void *));
  if (out) *out = array;
  Log("[MOD-SKIN-%s] renderer=%p binding=assembly-source slots=%zu",
      "V6", renderer, resolved.size());
  return true;
}
