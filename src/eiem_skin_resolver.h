#pragma once

#include <unordered_set>

static bool EiemParseSkinIndexPath(const std::string &text,
                                   std::vector<uint32_t> &indices,
                                   std::string &error) {
  indices.clear();
  if (text.empty()) return true;
  size_t begin = 0;
  while (begin < text.size()) {
    const size_t end = text.find('/', begin);
    const size_t length = end == std::string::npos ? text.size() - begin
                                                   : end - begin;
    if (!length || length > 6) {
      error = "Invalid canonical bone index path: " + text;
      return false;
    }
    uint64_t value = 0;
    for (size_t i = begin; i < begin + length; ++i) {
      const unsigned char character =
          static_cast<unsigned char>(text[i]);
      if (character < '0' || character > '9') {
        error = "Invalid canonical bone index path: " + text;
        return false;
      }
      value = value * 10u + (character - '0');
      if (value > 16384u) {
        error = "Canonical bone child index is too large: " + text;
        return false;
      }
    }
    indices.push_back(static_cast<uint32_t>(value));
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return true;
}

static void *EiemSkinGetChildByIndex(void *parent, int index) {
  if (!parent || !g_transform_GetChild) return nullptr;
  void *args[] = {&index};
  __try { return Invoke(g_transform_GetChild, parent, args); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static bool EiemBuildSkinBoneTable(
    void *root, EiemSkinPaletteCache::BoneTable &table, std::string &error) {
  table.root = root;
  table.byIndexPath.clear();
  if (!root || !g_transform_get_childCount || !g_transform_GetChild) {
    error = "Native skeleton hierarchy APIs are unavailable";
    return false;
  }
  std::vector<std::pair<void *, std::string>> pending;
  pending.emplace_back(root, std::string());
  size_t visited = 0;
  while (!pending.empty()) {
    auto current = std::move(pending.back());
    pending.pop_back();
    if (!current.first || EiemNativeObjectStatus(current.first) != 1)
      continue;
    if (++visited > 16384) {
      error = "Native skeleton hierarchy is too large";
      return false;
    }
    auto inserted = table.byIndexPath.emplace(current.second, current.first);
    if (!inserted.second && inserted.first->second != current.first) {
      error = "Canonical skeleton index path is ambiguous: " + current.second;
      return false;
    }
    void *boxed = EiemBackendInvokeNoThrow(g_transform_get_childCount,
                                           current.first);
    if (!boxed) {
      error = "Cannot enumerate native skeleton hierarchy";
      return false;
    }
    const int childCount = *(int *)((char *)boxed + 16);
    if (childCount < 0 || childCount > 16384) {
      error = "Invalid native skeleton child count";
      return false;
    }
    // Push in reverse so the table construction is deterministic without
    // changing the child-index identity itself.
    for (int index = childCount - 1; index >= 0; --index) {
      void *child = nullptr;
      child = EiemSkinGetChildByIndex(current.first, index);
      if (!child) {
        error = "Cannot read native skeleton child";
        return false;
      }
      std::string path = current.second;
      if (!path.empty()) path.push_back('/');
      path += std::to_string(index);
      pending.emplace_back(child, std::move(path));
    }
  }
  return true;
}

static bool EiemResolveMeshBonesFromIndexTable(
    const EiemSkinIdentity &identity, void *renderer, void **out,
    char *error, size_t errorSize) {
  if (out) *out = nullptr;
  auto reject = [&](const std::string &message) {
    if (error) strncpy_s(error, errorSize, message.c_str(), _TRUNCATE);
    return false;
  };
  if (!renderer || identity.boneIndexPaths.empty() || !il2cpp_array_new ||
      !g_transformClass)
    return reject("Mesh has no complete canonical bone index table");
  std::unordered_set<std::string> uniquePaths;
  for (const auto &path : identity.boneIndexPaths)
    if (!uniquePaths.emplace(path).second)
      return reject("Mesh canonical bone index table contains duplicates");

  void *skinningRoot = g_smr_get_skinningRoot
                           ? EiemBackendInvokeNoThrow(
                                 g_smr_get_skinningRoot, renderer)
                           : nullptr;
  void *rootBone = g_smr_get_rootBone
                       ? EiemBackendInvokeNoThrow(g_smr_get_rootBone,
                                                  renderer)
                       : nullptr;
  // Blender emits paths relative to the named armature root.  Unity's
  // rootBone is that anchor for the renderer; skinningRoot is only the
  // instance-wide fallback when a renderer does not expose rootBone.
  std::vector<void *> roots;
  if (rootBone) roots.push_back(rootBone);
  if (skinningRoot && skinningRoot != rootBone) roots.push_back(skinningRoot);
  if (roots.empty())
    return reject("Native Renderer has no skeleton root");

  EiemSkinPaletteCache localCache;
  EiemSkinPaletteCache *cache = s_eiemActiveSkinPaletteCache;
  if (!cache) cache = &localCache;
  for (void *root : roots) {
    EiemSkinPaletteCache::BoneTable *table = nullptr;
    for (auto &candidate : cache->boneTables)
      if (candidate.root == root) {
        table = &candidate;
        break;
      }
    if (!table) {
      EiemSkinPaletteCache::BoneTable next;
      std::string buildError;
      if (!EiemBuildSkinBoneTable(root, next, buildError)) continue;
      cache->boneTables.push_back(std::move(next));
      table = &cache->boneTables.back();
    }
    std::vector<void *> resolved;
    resolved.reserve(identity.boneIndexPaths.size());
    bool complete = true;
    for (const auto &path : identity.boneIndexPaths) {
      std::vector<uint32_t> indices;
      std::string parseError;
      if (!EiemParseSkinIndexPath(path, indices, parseError))
        return reject(parseError);
      auto found = table->byIndexPath.find(path);
      if (found == table->byIndexPath.end() ||
          !found->second || EiemNativeObjectStatus(found->second) != 1) {
        complete = false;
        break;
      }
      resolved.push_back(found->second);
    }
    if (!complete) continue;
    void *array = il2cpp_array_new(g_transformClass, resolved.size());
    if (!array) return reject("Unable to allocate canonical bone palette");
    memcpy((char *)array + IL2CPP_ARRAY_DATA, resolved.data(),
           resolved.size() * sizeof(void *));
    if (out) *out = array;
    if (kEiemEnableSkinBindingDiagnostics)
      Log("[MOD-SKIN-TABLE-v1] renderer=%p root=%p slots=%zu",
          renderer, root, resolved.size());
    return true;
  }
  return reject("Canonical bone index paths do not exist in this skeleton instance");
}

// Resolve every replacement slot from the concrete model instance hierarchy.
// The serialized child-index path is authoritative.  No source Mesh donor,
// local LOD slot, Transform name, or cross-instance cache participates.
static bool EiemResolveMeshBonesFromNativeInstance(
    const EiemSkinIdentity &identity, void *renderer, void **out,
    char *error, size_t errorSize) {
  if (out) *out = nullptr;
  return EiemResolveMeshBonesFromIndexTable(identity, renderer, out, error,
                                             errorSize);
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
