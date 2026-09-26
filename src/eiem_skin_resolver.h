#pragma once

// Resolve v6 donor slots inside the current model instance; own replacement bones.
static bool EiemResolveMeshBonesFromNativeInstance(
    const EiemSkinIdentity &identity, void *renderer, void **out,
    char *error, size_t errorSize) {
  if (out) *out = nullptr;
  auto reject = [&](const std::string &message) {
    if (error) strncpy_s(error, errorSize, message.c_str(), _TRUNCATE);
    return false;
  };
  if (!renderer || identity.sourceCandidates.empty() || !g_smr_get_bones ||
      !il2cpp_array_new || !g_transformClass)
    return reject("Native instance skeleton APIs are unavailable");

  // A slot number is local to one Mesh sub-asset's bones[] palette.  Several
  // sub-assets may live in the same FBX container, so an equal container path
  // cannot override a differing sub-asset identity. A path-only candidate
  // identifies a source that has no stable Mesh sub-asset name.
  auto sourceMatchesMeshIdentity =
      [](const EiemSkinIdentity::Source &source, const char *meshPath,
         const char *meshAsset) {
        if (!source.meshAsset.empty())
          return meshAsset && meshAsset[0] &&
                 EiemModEquals(source.meshAsset.c_str(), meshAsset);
        return !source.meshPath.empty() && meshPath && meshPath[0] &&
               EiemModSameLogicalPath(source.meshPath.c_str(), meshPath);
      };

  // All LOD Renderers in one PFB instance consume one game skeleton, while
  // each Renderer keeps only a local bones[] palette.  Do not compare local
  // slot numbers or rootBone pointers across LODs: rootBone is a Renderer
  // local anchor and may differ for body, cloth, shadow, and physical parts.
  // The instance boundary is skinningRoot.  A donor slot is therefore allowed
  // to come from any Renderer under the same skinningRoot, with same-root
  // donors preferred when both branches expose the slot.  This uses the
  // completed native hierarchy and never reads a Transform name or falls back
  // to an authored path.
  void *targetRootBone = g_smr_get_rootBone
                             ? EiemBackendInvokeNoThrow(g_smr_get_rootBone,
                                                        renderer)
                             : nullptr;
  void *targetSkinningRoot = g_smr_get_skinningRoot
                                 ? EiemBackendInvokeNoThrow(
                                       g_smr_get_skinningRoot, renderer)
                                 : nullptr;
  if (!targetSkinningRoot || !g_transform_get_parent ||
      !g_transform_get_childCount || !g_transform_GetChild)
    return reject("Native instance has no completed unified skeleton root");

  // The authoring Armature describes one logical skeleton, but a model can
  // expose several native Transform branches for its main, LOD and shadow
  // renderers.  Bone names cannot identify those branches because different
  // PFBs may rename the same logical bone.  Anchor the target branch with the
  // exact original bones[] captured before this model transaction mutates any
  // Renderer, then grow the branch through shared Transform references.
  void *targetNativeBones = EiemBackendInvokeNoThrow(g_smr_get_bones, renderer);
  if (s_eiemLiveSkinSources) {
    for (const auto &source : *s_eiemLiveSkinSources) {
      if (source.renderer == renderer && source.bones) {
        targetNativeBones = source.bones;
        break;
      }
    }
  }
  auto paletteOverlap = [](void *left, void *right) -> size_t {
    const size_t leftCount = EiemManagedArrayLength(left);
    const size_t rightCount = EiemManagedArrayLength(right);
    if (!left || !right || !leftCount || !rightCount) return 0;
    void **leftItems = (void **)((char *)left + IL2CPP_ARRAY_DATA);
    void **rightItems = (void **)((char *)right + IL2CPP_ARRAY_DATA);
    size_t overlap = 0;
    for (size_t leftIndex = 0; leftIndex < leftCount; ++leftIndex) {
      void *bone = leftItems[leftIndex];
      if (!bone) continue;
      for (size_t rightIndex = 0; rightIndex < rightCount; ++rightIndex) {
        if (rightItems[rightIndex] != bone) continue;
        ++overlap;
        break;
      }
    }
    return overlap;
  };
  auto sameSkeletonContext = [&](void *candidateRenderer) -> bool {
    if (!candidateRenderer) return false;
    // rootBone is deliberately not an instance boundary.  The game assigns
    // different local rootBone values to body/cloth/shadow Renderers while
    // their Transform objects still belong to the same skinningRoot.
    if (targetSkinningRoot && g_smr_get_skinningRoot) {
      void *candidateRoot = EiemBackendInvokeNoThrow(
          g_smr_get_skinningRoot, candidateRenderer);
      if (!candidateRoot || candidateRoot != targetSkinningRoot) return false;
    }
    return true;
  };
  std::vector<const EiemLiveSkinSource *> targetBranchSources;
  if (s_eiemLiveSkinSources && targetNativeBones) {
    bool changed = true;
    while (changed) {
      changed = false;
      for (const auto &candidate : *s_eiemLiveSkinSources) {
        if (!candidate.bones) continue;
        if (std::find(targetBranchSources.begin(), targetBranchSources.end(),
                      &candidate) != targetBranchSources.end())
          continue;
        bool connected = candidate.renderer == renderer ||
                         (sameSkeletonContext(candidate.renderer) &&
                          paletteOverlap(targetNativeBones, candidate.bones) != 0);
        if (!connected) {
          for (const auto *member : targetBranchSources) {
            if (member && sameSkeletonContext(candidate.renderer) &&
                paletteOverlap(member->bones, candidate.bones) != 0) {
              connected = true;
              break;
            }
          }
        }
        // A lower LOD may omit every bone owned by a donor Renderer.  It is
        // still a valid donor when the game's completed root context proves
        // that both palettes belong to this same model instance.
        if (!connected && sameSkeletonContext(candidate.renderer))
          connected = true;
        if (!connected) continue;
        targetBranchSources.push_back(&candidate);
        changed = true;
      }
    }
  }
  auto sourceBranchScore = [&](const EiemLiveSkinSource &candidate) -> size_t {
    if (!candidate.bones || !targetNativeBones) return 0;
    if (candidate.renderer == renderer)
      return (size_t)1 << (sizeof(size_t) * 8 - 2);
    if (std::find(targetBranchSources.begin(), targetBranchSources.end(),
                  &candidate) == targetBranchSources.end())
      return 0;
    // Direct overlap selects the closest native palette inside the connected
    // branch.  The +1 keeps a transitively connected donor usable when the
    // target LOD omits every bone owned by that specialised source Mesh.
    size_t score = paletteOverlap(targetNativeBones, candidate.bones) + 1;
    if (targetRootBone && g_smr_get_rootBone) {
      void *candidateRoot = EiemBackendInvokeNoThrow(
          g_smr_get_rootBone, candidate.renderer);
      // Prefer a donor from the target Renderer branch, but keep a lower
      // scoring cross-branch donor available for bones only exposed by cloth,
      // physics, or another specialised Mesh.
      if (candidateRoot == targetRootBone)
        score += (size_t)1 << (sizeof(size_t) * 8 - 3);
    }
    return score;
  };

  auto childIndexPath = [&](void *root, void *node,
                            std::vector<uint32_t> *path) -> bool {
    if (!root || !node || !path) return false;
    path->clear();
    void *current = node;
    for (size_t depth = 0; current && current != root && depth < 256;
         ++depth) {
      void *parent = EiemBackendInvokeNoThrow(g_transform_get_parent, current);
      if (!parent || parent == current) return false;
      void *boxed = EiemBackendInvokeNoThrow(g_transform_get_childCount, parent);
      if (!boxed) return false;
      const int childCount = *(int *)((char *)boxed + 16);
      if (childCount < 0 || childCount > 16384) return false;
      size_t found = SIZE_MAX;
      for (int index = 0; index < childCount; ++index) {
        int childIndex = index;
        void *params[] = {&childIndex};
        void *child = Invoke(g_transform_GetChild, parent, params);
        if (child == current) {
          found = (size_t)index;
          break;
        }
      }
      if (found == SIZE_MAX || found > UINT32_MAX) return false;
      path->push_back((uint32_t)found);
      current = parent;
    }
    if (current != root) return false;
    std::reverse(path->begin(), path->end());
    return true;
  };
  auto resolveChildIndexPath = [&](void *root,
                                   const std::vector<uint32_t> &path) -> void * {
    if (!root) return nullptr;
    void *current = root;
    for (uint32_t index : path) {
      void *boxed = EiemBackendInvokeNoThrow(g_transform_get_childCount,
                                             current);
      if (!boxed) return nullptr;
      const int childCount = *(int *)((char *)boxed + 16);
      if (childCount < 0 || index >= (uint32_t)childCount) return nullptr;
      int childIndex = (int)index;
      void *params[] = {&childIndex};
      current = Invoke(g_transform_GetChild, current, params);
      if (!current || EiemNativeObjectStatus(current) != 1) return nullptr;
    }
    return current;
  };
  auto mapDonorToTargetSkeleton = [&](void *candidateRenderer,
                                      void *candidateBone) -> void * {
    if (!candidateRenderer || !candidateBone) return nullptr;
    // The target Renderer is already part of the completed native table; its
    // own slot is authoritative and needs no cross-palette conversion.
    if (candidateRenderer == renderer) return candidateBone;
    void *candidateRoot = g_smr_get_skinningRoot
                              ? EiemBackendInvokeNoThrow(
                                    g_smr_get_skinningRoot,
                                    candidateRenderer)
                              : nullptr;
    if (!candidateRoot) return nullptr;
    if (candidateRoot == targetSkinningRoot) return candidateBone;
    std::vector<uint32_t> path;
    if (!childIndexPath(candidateRoot, candidateBone, &path)) return nullptr;
    return resolveChildIndexPath(targetSkinningRoot, path);
  };

  auto resolveSourceSlot = [&](const EiemSkinIdentity::Source &source,
                               bool *ambiguous,
                               const EiemLiveSkinSource **donorOut,
                               size_t *scoreOut) -> void * {
    if (ambiguous) *ambiguous = false;
    if (donorOut) *donorOut = nullptr;
    if (scoreOut) *scoreOut = 0;
    if (!s_eiemLiveSkinSources ||
        (source.meshAsset.empty() && source.meshPath.empty()))
      return nullptr;
    static volatile LONG s_candidateConflictLogCount = 0;
    void *firstRenderer = nullptr;
    void *firstBone = nullptr;
    void *firstRootBone = nullptr;
    void *firstSkinningRoot = nullptr;
    void *resolved = nullptr;
    size_t resolvedScore = 0;
    for (const auto &candidate : *s_eiemLiveSkinSources) {
      if (!sourceMatchesMeshIdentity(source, candidate.source.c_str(),
                                     candidate.asset.c_str()))
        continue;
      const size_t branchScore = sourceBranchScore(candidate);
      if (!branchScore) continue;
      const size_t boneCount = EiemManagedArrayLength(candidate.bones);
      if (!candidate.bones || source.slot >= boneCount) continue;
      void **sourceBones =
          (void **)((char *)candidate.bones + IL2CPP_ARRAY_DATA);
      void *bone = sourceBones[source.slot];
      if (!bone || EiemNativeObjectStatus(bone) != 1) continue;
      void *candidateSkinningRoot =
          g_smr_get_skinningRoot
              ? EiemBackendInvokeNoThrow(g_smr_get_skinningRoot,
                                         candidate.renderer)
              : nullptr;
      void *candidateRootBone =
          g_smr_get_rootBone
              ? EiemBackendInvokeNoThrow(g_smr_get_rootBone,
                                         candidate.renderer)
              : nullptr;
      void *mappedBone = mapDonorToTargetSkeleton(candidate.renderer, bone);
      if (!mappedBone) continue;
      if (!resolved || branchScore > resolvedScore) {
        firstRenderer = candidate.renderer;
        firstBone = bone;
        firstRootBone = candidateRootBone;
        firstSkinningRoot = candidateSkinningRoot;
        resolved = mappedBone;
        resolvedScore = branchScore;
        if (donorOut) *donorOut = &candidate;
        continue;
      }
      if (branchScore == resolvedScore && resolved != mappedBone) {
        const LONG sample = InterlockedIncrement(&s_candidateConflictLogCount);
        if (sample <= 48) {
          Log("[MOD-SKIN-CANDIDATE-v1] model=%p target=%p targetRoot=%p "
              "targetSkinningRoot=%p targetBranch=%p targetBranchKey=%s "
              "sourceAsset=%s "
              "sourcePath=%s slot=%u "
              "firstRenderer=%p firstBone=%p firstRoot=%p firstSkinningRoot=%p "
              "conflictRenderer=%p conflictBone=%p conflictRoot=%p "
              "conflictSkinningRoot=%p conflictBranch=%p "
              "conflictBranchKey=%s",
              (void *)s_eiemActivePrefabInstance, renderer, targetRootBone,
              targetSkinningRoot, nullptr, "native-root-context",
              source.meshAsset.c_str(), source.meshPath.c_str(), source.slot,
              firstRenderer, firstBone, firstRootBone, firstSkinningRoot,
              candidate.renderer, bone, candidateRootBone, candidateSkinningRoot,
              nullptr, "unified-skeleton");
        }
        if (ambiguous) *ambiguous = true;
        return nullptr;
      }
    }
    if (scoreOut) *scoreOut = resolvedScore;
    return resolved;
  };

  auto resolveSourceCandidates =
      [&](const std::vector<EiemSkinIdentity::Source> &candidates,
          bool *ambiguous) -> void * {
    if (ambiguous) *ambiguous = false;
    static volatile LONG s_candidateDonorMergeLogCount = 0;
    void *resolved = nullptr;
    const EiemLiveSkinSource *resolvedDonor = nullptr;
    size_t resolvedScore = 0;
    for (const auto &candidate : candidates) {
      bool candidateAmbiguous = false;
      const EiemLiveSkinSource *donor = nullptr;
      size_t candidateScore = 0;
      void *bone = resolveSourceSlot(candidate, &candidateAmbiguous, &donor,
                                     &candidateScore);
      if (candidateAmbiguous) {
        if (ambiguous) *ambiguous = true;
        return nullptr;
      }
      if (!bone) continue;
      if (!resolved || candidateScore > resolvedScore) {
        resolved = bone;
        resolvedDonor = donor;
        resolvedScore = candidateScore;
        continue;
      }
      if (candidateScore == resolvedScore && resolved != bone) {
        const LONG sample =
            InterlockedIncrement(&s_candidateDonorMergeLogCount);
        if (sample <= 48) {
          void *conflictRootBone =
              donor && g_smr_get_rootBone
                  ? EiemBackendInvokeNoThrow(g_smr_get_rootBone,
                                             donor->renderer)
                  : nullptr;
          void *conflictSkinningRoot =
              donor && g_smr_get_skinningRoot
                  ? EiemBackendInvokeNoThrow(g_smr_get_skinningRoot,
                                             donor->renderer)
                  : nullptr;
          Log("[MOD-SKIN-CANDIDATE-v1] model=%p target=%p "
              "targetRoot=%p targetSkinningRoot=%p targetBranch=%p "
              "targetBranchKey=%s slotPath=%s "
              "resolvedRenderer=%p resolvedBone=%p resolvedRoot=%p "
              "resolvedSkinningRoot=%p conflictRenderer=%p conflictBone=%p "
              "conflictRoot=%p conflictSkinningRoot=%p conflictBranch=%p "
              "conflictBranchKey=%s",
              (void *)s_eiemActivePrefabInstance, renderer, targetRootBone,
              targetSkinningRoot, nullptr, "native-root-context",
              candidate.meshPath.c_str(),
              resolvedDonor ? resolvedDonor->renderer : nullptr, resolved,
              targetRootBone, targetSkinningRoot,
              donor ? donor->renderer : nullptr, bone, conflictRootBone,
              conflictSkinningRoot, nullptr, "native-root-context");
        }
        if (ambiguous) *ambiguous = true;
        return nullptr;
      }
    }
    return resolved;
  };

  // Prefer the palette that belongs to this exact Renderer.  A model can
  // contain several LOD and shadow renderers for the same logical Mesh, and
  // their local palettes may be different subsets of the same native
  // skeleton.  Comparing all of those donors before the renderer has a
  // completed assembly snapshot can reject a valid replacement.  The source
  // Mesh/slot record is authoritative for the current renderer, so use its
  // own bones[] whenever it contains every replacement slot we need.
  void *currentBones = targetNativeBones;
  const size_t currentBoneCount = EiemManagedArrayLength(currentBones);
  void **currentBoneItems =
      currentBones && currentBoneCount
          ? (void **)((char *)currentBones + IL2CPP_ARRAY_DATA)
          : nullptr;
  char currentSource[768] = {}, currentAsset[192] = {};
  void *currentMesh = EiemReadSharedMesh(renderer, "SkinnedMeshRenderer");
  void *currentIdentityMesh = currentMesh;
  if (currentMesh)
    EiemPrepareRenderInput(renderer, currentMesh, "SkinnedMeshRenderer",
                           &currentIdentityMesh);
  const bool currentIdentityKnown =
      currentIdentityMesh &&
      EiemReadLiveMeshIdentity(currentIdentityMesh, currentSource,
                               sizeof(currentSource), currentAsset,
                               sizeof(currentAsset));
  auto sourceMatchesCurrentRenderer =
      [&](const EiemSkinIdentity::Source &source) {
        return currentIdentityKnown && sourceMatchesMeshIdentity(
                                           source, currentSource, currentAsset);
      };
  auto allocateCurrentPalette = [&](const std::vector<void *> &resolved,
                                    const char *binding) -> bool {
    if (resolved.empty() || !il2cpp_array_new || !g_transformClass)
      return false;
    void *array = il2cpp_array_new(g_transformClass, resolved.size());
    if (!array) return false;
    memcpy((char *)array + IL2CPP_ARRAY_DATA, resolved.data(),
           resolved.size() * sizeof(void *));
    if (out) *out = array;
    Log("[MOD-SKIN-NATIVE] renderer=%p binding=%s slots=%zu", renderer,
        binding, resolved.size());
    return true;
  };

  if (currentBoneItems && currentIdentityKnown &&
      !identity.sourceCandidates.empty()) {
    std::vector<void *> resolved;
    resolved.reserve(identity.sourceCandidates.size());
    bool complete = true;
    for (const auto &candidates : identity.sourceCandidates) {
      void *selected = nullptr;
      for (const auto &source : candidates) {
        if (!sourceMatchesCurrentRenderer(source) ||
            source.slot >= currentBoneCount)
          continue;
        void *bone = currentBoneItems[source.slot];
        if (bone && EiemNativeObjectStatus(bone) == 1)
          selected = bone;
      }
      if (!selected) {
        complete = false;
        break;
      }
      resolved.push_back(selected);
    }
    if (complete && resolved.size() == identity.sourceCandidates.size() &&
        allocateCurrentPalette(resolved, "renderer-source-slots"))
      return true;
  }


  // EIEMESH v6 is strict by design.  Every replacement slot is backed by one
  // or more original Mesh/slot donors.  Resolve only donors present in this
  // model instance; if none exists, or existing donors disagree, fail instead
  // of guessing with a renamed hierarchy or a local LOD array index.
  if (!identity.sourceCandidates.empty()) {
    std::vector<void *> sourceResolved;
    sourceResolved.reserve(identity.sourceCandidates.size());
    for (size_t slot = 0; slot < identity.sourceCandidates.size(); ++slot) {
      bool ambiguous = false;
      void *bone = resolveSourceCandidates(identity.sourceCandidates[slot],
                                            &ambiguous);
      if (ambiguous)
        return reject("Replacement bone source candidates disagree in model instance: " +
                      std::to_string(slot));
      if (!bone)
        return reject("Replacement bone has no native Mesh donor in model instance: " +
                      std::to_string(slot));
      sourceResolved.push_back(bone);
    }
    void *array = il2cpp_array_new(g_transformClass, sourceResolved.size());
    if (!array) return reject("Unable to allocate donor-slot bone palette");
    memcpy((char *)array + IL2CPP_ARRAY_DATA, sourceResolved.data(),
           sourceResolved.size() * sizeof(void *));
    if (out) *out = array;
    Log("[MOD-SKIN-NATIVE] renderer=%p binding=instance-donor-candidates "
        "slots=%zu sourceResolved=%zu",
        renderer, sourceResolved.size(), sourceResolved.size());
    return true;
  }

  return reject("EIEMESH has no native source-Mesh slot records");
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
  Log("[MOD-SKIN] renderer=%p slots=%zu changed=%d applied=%d",renderer,EiemManagedArrayLength(bones),!same,ok);
  return ok;
}

// The game can finish or rebuild a Renderer skin after EIEM first replaced
// its Mesh. The incoming game-owned palette/root are the baseline that F10
// must restore. Keep that baseline separate from EIEM's replacement palette;
// otherwise a reload restores the early construction snapshot and the affected
// Renderer can fall back to an unanimated/T-pose binding.
