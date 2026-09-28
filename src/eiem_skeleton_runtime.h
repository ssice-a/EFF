#pragma once
#include <cctype>
#include <cmath>
#include <mutex>
#include <unordered_set>
#include "eiem_skeleton_document.h"
#include "eiem_registration_trace.h"

static bool EiemSkeletonPathSuffix(const std::string &full,
                                    const std::string &path) {
  return !path.empty() &&
         (full == path ||
          (full.size() > path.size() &&
           full[full.size() - path.size() - 1] == '/' &&
           full.compare(full.size() - path.size(), path.size(), path) == 0));
}

// Only instantiated on the Unity thread. Consumers keep leases until their
// source Mesh/bones are restored. The registry owns the final retirement ref.
struct EiemSkeletonInstance {
  std::string key;
  uint64_t stamp = 0;
  bool ready = false;
  EiemUnityRef anchor;
  EiemSkeletonDocument document;
  std::vector<EiemUnityRef> nodes;
  std::vector<EiemUnityRef> createdObjects;
  // Deferred destruction / owner-unload callbacks do not prove the native
  // consumer stopped using these Transforms. Wait for native death.
  std::vector<EiemUnityRef> retiringConsumers;
};
static std::vector<std::shared_ptr<EiemSkeletonInstance>> s_eiemSkeletonInstances;
static std::mutex s_eiemSkeletonConsumerMutex;

// Teardown callbacks may run outside Unity's thread. They only retain an
// existing managed reference here; all native queries/destruction stay below.
static void EiemRetainSkeletonConsumer(EiemSkeletonInstance &instance, const EiemUnityRef &reference) {
  std::lock_guard<std::mutex> guard(s_eiemSkeletonConsumerMutex);
  instance.retiringConsumers.push_back(reference);
}

// A live Partner can be rebound to a newer Skeleton generation during F10.
// Remove its old deferred-retirement lease before releasing that generation;
// otherwise the old instance would stay permanently "consumed" by a Renderer
// that now points at the new node array.
static void EiemReleaseSkeletonConsumer(EiemSkeletonInstance &instance,
                                         void *consumer) {
  std::lock_guard<std::mutex> guard(s_eiemSkeletonConsumerMutex);
  for (size_t index = 0; index < instance.retiringConsumers.size();) {
    if (consumer && instance.retiringConsumers[index].Target() != consumer) {
      ++index;
      continue;
    }
    instance.retiringConsumers.erase(instance.retiringConsumers.begin() + index);
  }
}

static bool EiemSkeletonCall(void *method, void *object, void **args = nullptr, void **out = nullptr) {
  void *result = nullptr;
  const bool ok = method && InvokeChecked(method,object,args,&result);
  if (out) *out = result;
  return ok;
}

static void EiemCollectSkeletonInstances() {
  if (!EiemOnUnityThread()) return;
  for (size_t i = 0; i < s_eiemSkeletonInstances.size();) {
    auto &instance = s_eiemSkeletonInstances[i];
    if (instance.use_count() != 1) { ++i; continue; }
    instance->ready=false;
    std::vector<EiemUnityRef> consumers;
    {
      std::lock_guard<std::mutex> guard(s_eiemSkeletonConsumerMutex);
      consumers=instance->retiringConsumers;
    }
    bool consumed=false;
    for (const auto &consumer:consumers)
      if (consumer.Status()!=0) { consumed=true; break; }
    if (consumed) { ++i; continue; }
    bool complete = true;
    // Reverse topological order; detach before deferred Destroy, so this
    // reconcile cannot see retired children in the game's live hierarchy.
    for (size_t j = instance->createdObjects.size(); j > 0; --j) {
      auto &object = instance->createdObjects[j-1];
      if (!object) continue;
      const int status = object.Status();
      if (status < 0) { complete = false; continue; }
      if (status == 1) {
        void *transform = nullptr; bool keepWorld = false;
        void *detach[] = {nullptr,&keepWorld}; void *destroy[] = {object.Target()};
        if (!EiemSkeletonCall(g_gameObject_get_transform,object.Target(),nullptr,&transform) || !transform ||
            !EiemSkeletonCall(g_transform_set_parent,transform,detach) ||
            !EiemSkeletonCall(g_object_destroy,nullptr,destroy)) { complete = false; continue; }
      }
      object = {};
    }
    if (!complete) {
      Log("[SKELETON] Retirement pending; references retained: %s",instance->key.c_str()); ++i; continue;
    }
    Log("[SKELETON] Retired %zu owned nodes: %s",instance->createdObjects.size(),instance->key.c_str());
    s_eiemSkeletonInstances.erase(s_eiemSkeletonInstances.begin()+i);
  }
}

// Resolve only the requested resource path below an anchored Transform. A
// live hierarchy may contain Mod-owned Partner objects beside the source
// skeleton; walking the entire subtree would make those unrelated objects
// look like duplicate source bones.
static bool EiemSkeletonResolveLivePath(void *root, const std::string &relative,
                                        const std::string &display,
                                        void **out, std::string &error) {
  *out = root;
  if (relative.empty()) return true;
  size_t begin = 0;
  while (begin < relative.size()) {
    const size_t end = relative.find('/', begin);
    const std::string segment = relative.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin);
    if (segment.empty()) {
      error = "Invalid live Skeleton path: " + display;
      return false;
    }
    void *boxed = nullptr;
    if (!EiemSkeletonCall(g_transform_get_childCount, *out, nullptr, &boxed) || !boxed) {
      error = "Cannot enumerate live Skeleton path: " + display;
      return false;
    }
    const int count = *(int *)((char *)boxed + 16);
    if (count < 0 || count > 16384) {
      error = "Invalid live Skeleton child count: " + display;
      return false;
    }
    void *match = nullptr;
    for (int i = 0; i < count; ++i) {
      void *child = nullptr, *name = nullptr;
      void *args[] = {&i};
      char text[256] = {};
      if (!EiemSkeletonCall(g_transform_GetChild, *out, args, &child) || !child ||
          !EiemSkeletonCall(g_object_get_name, child, nullptr, &name) || !name) {
        error = "Cannot read live Skeleton child: " + display;
        return false;
      }
      ReadStrUtf8(name, text, sizeof(text));
      if (!text[0]) {
        error = "Unnamed live Skeleton child: " + display;
        return false;
      }
      if (segment == text) {
        if (match) {
          error = "Ambiguous live Skeleton path: " + display;
          return false;
        }
        match = child;
      }
    }
    if (!match) {
      *out = nullptr;
      return true;
    }
    *out = match;
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return true;
}

// Anchor a document to ONE existing rig through original bone paths. This
// also supports the empty prefab-root path exported by AnimeStudio. Physics
// colliders may name a source path that is absent from a UI/NPC hierarchy;
// those explicitly declared paths are returned as null so the caller can
// create an owned anchor with the exported local TRS.
static bool EiemSkeletonSourceNodes(
    const EiemSkeletonDocument &document, void *renderer,
    const std::unordered_set<std::string> *virtualPaths,
    std::vector<void *> &nodes, std::string &error,
    size_t *ancestryFallbacks = nullptr) {
  nodes.assign(document.nodes.size(),nullptr);
  if (ancestryFallbacks) *ancestryFallbacks = 0;
  void *palette = nullptr;
  if (!EiemSkeletonCall(g_smr_get_bones,renderer,nullptr,&palette)) { error="Cannot read source bone palette"; return false; }
  const size_t count = EiemManagedArrayLength(palette);
  if (!palette || !count || count>16384) { error="Source bone palette is empty or invalid"; return false; }
  const auto paletteRoot=EiemUnityRef::Capture(palette,false);
  if (!paletteRoot) { error="Cannot retain source bone palette"; return false; }
  void **items = (void **)((char *)palette+IL2CPP_ARRAY_DATA);
  std::unordered_map<std::string,void *> ancestors;
  std::string prefix; bool anchored = false;
  for (size_t i=0;i<count;++i) {
    std::vector<std::pair<std::string,void *>> chain;
    for (void *node=items[i];node;) {
      if (chain.size()>=128 || EiemNativeObjectStatus(node)!=1) { error="Invalid source skeleton ancestry"; return false; }
      char name[256]={}; void *value=nullptr;
      if (!EiemSkeletonCall(g_object_get_name,node,nullptr,&value) || !value) return false;
      ReadStrUtf8(value,name,sizeof(name));
      if (!name[0]) { error="Unnamed source bone"; return false; }
      chain.emplace_back(name,node);
      if (!EiemSkeletonCall(g_transform_get_parent,node,nullptr,&node)) return false;
    }
    std::string full;
    for (auto it=chain.rbegin();it!=chain.rend();++it) {
      if (!full.empty()) full+='/';
      full+=it->first;
      auto inserted=ancestors.emplace(full,it->second);
      if (!inserted.second && inserted.first->second!=it->second) { error="Ambiguous source bone ancestry"; return false; }
    }
    for (const auto &node:document.nodes) {
      if (!node.source || node.path.empty() ||
          !EiemSkeletonPathSuffix(full, node.path))
        continue;
      const auto candidate=full.substr(0,full.size()-node.path.size());
      if (anchored && prefix!=candidate) { error="Skeleton document spans multiple instances"; return false; }
      prefix=candidate; anchored=true;
    }
  }
  if (!anchored) { error="No source bone anchors this Skeleton resource"; return false; }
  std::string rootPath=prefix+document.nodes.front().path;
  if (!rootPath.empty() && rootPath.back()=='/') rootPath.pop_back();
  const auto root=ancestors.find(rootPath);
  if (root==ancestors.end()) { error="Skeleton source root is absent"; return false; }
  const std::string rootDocumentPath = document.nodes.front().path;
  if (EiemNativeObjectStatus(root->second) != 1) {
    error = "Invalid live Skeleton root";
    return false;
  }
  for (size_t i = 0; i < document.nodes.size(); ++i) {
    const auto &node = document.nodes[i];
    if (!node.source) continue;
    std::string relative = node.path;
    if (!rootDocumentPath.empty()) {
      if (node.path == rootDocumentPath) relative.clear();
      else if (node.path.rfind(rootDocumentPath + '/', 0) == 0)
        relative = node.path.substr(rootDocumentPath.size() + 1);
      else {
        error = "Skeleton source path is outside its root: " + node.path;
        return false;
      }
    }
    void *resolved = nullptr;
    if (!EiemSkeletonResolveLivePath(
            root->second, relative, prefix + node.path, &resolved, error))
      return false;
    // Some model variants expose a bone in SkinnedMeshRenderer.bones and its
    // ancestry, but the Transform child walk is not a complete view of that
    // hierarchy (the game can insert a runtime wrapper or reorder children).
    // The palette ancestry is still an exact path identity, so reuse that
    // Animator-owned Transform before considering a private replacement.
    if (!resolved) {
      const std::string fullPath = prefix + node.path;
      const auto exact = ancestors.find(fullPath);
      if (exact != ancestors.end() &&
          EiemNativeObjectStatus(exact->second) == 1) {
        resolved = exact->second;
        if (ancestryFallbacks) ++*ancestryFallbacks;
      }
    }
    // Physics Skeleton files can be authored from a sibling PFB variant. A
    // source node may then have the same parent and exported local TRS but a
    // different variant-specific name (for example a generated collider or a
    // renamed skirt segment). Do not immediately replace that source with a
    // Mod-owned Transform: first resolve a unique live child under the already
    // resolved parent. This keeps the slot in the Animator-owned hierarchy and
    // avoids a static private bone putting the whole weighted section in a
    // bind/T-pose. The match is data-driven and never contains character names.
    if (!resolved && virtualPaths && virtualPaths->count(node.path) &&
        node.parent >= 0 && (size_t)node.parent < nodes.size() &&
        nodes[node.parent]) {
      void *parentTransform = nodes[node.parent];
      void *boxedCount = nullptr;
      int childCount = -1;
      if (EiemSkeletonCall(g_transform_get_childCount, parentTransform,
                           nullptr, &boxedCount) && boxedCount)
        childCount = *(int *)((char *)boxedCount + 16);
      const size_t slash = node.path.find_last_of('/');
      const std::string leaf = slash == std::string::npos
                                   ? node.path
                                   : node.path.substr(slash + 1);
      void *nameMatch = nullptr;
      void *trsMatch = nullptr;
      size_t nameMatches = 0;
      size_t trsMatches = 0;
      auto readVector3 = [](void *getter, void *object, float out[3]) {
        if (!getter || !object) return false;
        void *boxed = nullptr;
        if (!EiemSkeletonCall(getter, object, nullptr, &boxed) || !boxed)
          return false;
        const float *value = (const float *)((char *)boxed + 16);
        out[0] = value[0]; out[1] = value[1]; out[2] = value[2];
        return true;
      };
      auto readQuaternion = [](void *getter, void *object, float out[4]) {
        if (!getter || !object) return false;
        void *boxed = nullptr;
        if (!EiemSkeletonCall(getter, object, nullptr, &boxed) || !boxed)
          return false;
        const float *value = (const float *)((char *)boxed + 16);
        out[0] = value[0]; out[1] = value[1];
        out[2] = value[2]; out[3] = value[3];
        return true;
      };
      auto close = [](float left, float right, float tolerance) {
        return std::fabs(left - right) <= tolerance;
      };
      if (childCount >= 0 && childCount <= 16384 &&
          g_transform_GetChild && g_object_get_name) {
        for (int child = 0; child < childCount; ++child) {
          void *params[] = {&child};
          void *candidate = nullptr;
          if (!EiemSkeletonCall(g_transform_GetChild, parentTransform, params,
                                &candidate) ||
              !candidate || EiemNativeObjectStatus(candidate) != 1)
            continue;
          void *boxedName = nullptr;
          if (!EiemSkeletonCall(g_object_get_name, candidate, nullptr,
                                &boxedName) || !boxedName)
            continue;
          char candidateName[256] = {};
          ReadStrUtf8(boxedName, candidateName, sizeof(candidateName));
          if (leaf == candidateName) {
            nameMatch = candidate;
            ++nameMatches;
          }

          float position[3] = {}, rotation[4] = {}, scale[3] = {};
          const bool hasTrs =
              readVector3(g_transform_get_localPosition, candidate, position) &&
              readQuaternion(g_transform_get_localRotation, candidate,
                             rotation) &&
              readVector3(g_transform_get_localScale, candidate, scale);
          bool sameTrs = hasTrs;
          for (size_t axis = 0; sameTrs && axis < 3; ++axis)
            sameTrs = close(position[axis], node.position[axis], 0.001f) &&
                      close(scale[axis], node.scale[axis], 0.001f);
          for (size_t axis = 0; sameTrs && axis < 4; ++axis)
            sameTrs = close(rotation[axis], node.rotation[axis], 0.002f);
          if (sameTrs) {
            trsMatch = candidate;
            ++trsMatches;
          }
        }
      }
      if (nameMatches == 1)
        resolved = nameMatch;
      else if (!resolved && nameMatches == 0 && trsMatches == 1)
        resolved = trsMatch;
      if (resolved) {
        if (ancestryFallbacks) ++*ancestryFallbacks;
        Log("[SKELETON-BIND-FALLBACK] source=%s parent=%p resolved=%p "
            "nameMatches=%zu trsMatches=%zu",
            node.path.c_str(), parentTransform, resolved, nameMatches,
            trsMatches);
      }
    }
    if (!resolved) {
      if (i != 0 && virtualPaths && virtualPaths->count(node.path)) continue;
      error = "Source bone is missing (not a new bone): " + node.path;
      return false;
    }
    nodes[i] = resolved;
  }
  return true;
}

static std::string EiemSkeletonInstanceKey(const char *modPath,
                                            const std::string &diskPath) {
  std::string key=std::string(modPath?modPath:"")+"|"+diskPath;
  std::transform(key.begin(),key.end(),key.begin(),
                 [](unsigned char c){return (char)std::tolower(c);});
  return key;
}

static bool EiemAcquireSkeletonDocument(
    std::string key,uint64_t stamp,EiemSkeletonDocument document,void *renderer,
    std::shared_ptr<EiemSkeletonInstance> &out,char *message,size_t messageSize,
    const std::unordered_set<std::string> *virtualPaths = nullptr) {
  out.reset(); std::string error;
  auto fail=[&](const std::string &why) {
    if (message) strncpy_s(message,messageSize,why.c_str(),_TRUNCATE);
    return false;
  };
  if (!EiemOnUnityThread()) return fail("Skeleton requires Unity thread");
  std::vector<void *> source;
  size_t ancestryFallbacks = 0;
  if (!EiemSkeletonSourceNodes(document,renderer,virtualPaths,source,error,
                               &ancestryFallbacks))
    return fail(error.empty()?"Cannot resolve source Skeleton":error);
  size_t missingSource = 0, missingNew = 0;
  std::string firstMissingSource, firstMissingNew;
  for (size_t i = 0; i < document.nodes.size(); ++i) {
    if (source[i]) continue;
    if (document.nodes[i].source) {
      ++missingSource;
      if (firstMissingSource.empty()) firstMissingSource = document.nodes[i].path;
    } else {
      ++missingNew;
      if (firstMissingNew.empty()) firstMissingNew = document.nodes[i].path;
    }
  }
  Log("[SKELETON-BIND] key=%s anchor=%p sourceMissing=%zu newMissing=%zu "
      "ancestryFallback=%zu firstSource=%s firstNew=%s",
      key.c_str(), source.empty() ? nullptr : source.front(), missingSource,
      missingNew, ancestryFallbacks,
      firstMissingSource.empty() ? "<none>" : firstMissingSource.c_str(),
      firstMissingNew.empty() ? "<none>" : firstMissingNew.c_str());
  EiemCollectSkeletonInstances();
  for (const auto &cached:s_eiemSkeletonInstances) {
    if (cached->key!=key || cached->anchor.Target()!=source.front() || cached->anchor.Status()!=1) continue;
    // Old generations can be waiting for deferred partner destruction.
    // Their private nodes must not be reused by the new generation.
    if (!cached->ready) continue;
    if (cached->stamp!=stamp) {
      // Added nodes are private to one instance. Keep this generation alive
      // for Renderers/native jobs that already reference it, but stop handing
      // it to new consumers and build the changed file as a separate
      // generation. This makes F10 a real rollover instead of requiring a
      // second control event after deferred Unity destruction.
      cached->ready=false;
      Log("[SKELETON] Superseded generation retained during reload: %s",
          cached->key.c_str());
      continue;
    }
    for (const auto &node:cached->nodes) if (node.Status()!=1) return fail("Skeleton node was destroyed; reload to rebuild");
    if (EiemRegistrationTraceFirst("skeleton-cache", cached->key.c_str(),
                                   cached.get(), source.front(), nullptr, -1))
      Log("%s event=skeleton-cache instance=%p stamp=%llu anchor=%p nodes=%zu",
          EiemRegistrationTraceTag, cached.get(),
          (unsigned long long)cached->stamp, source.front(), cached->nodes.size());
    out=cached; return true;
  }
  auto instance=std::make_shared<EiemSkeletonInstance>();
  instance->key=key; instance->stamp=stamp; instance->document=std::move(document);
  instance->anchor=EiemUnityRef::Capture(source.front());
  if (!instance->anchor) return fail("Cannot retain Skeleton instance identity");
  // Registry also keeps failed builds alive until checked retirement succeeds.
  s_eiemSkeletonInstances.push_back(instance);
  for (size_t i=0;i<instance->document.nodes.size();++i) {
    const auto &node=instance->document.nodes[i]; void *transform=source[i];
    if (!node.source || !source[i]) {
      if (!g_gameObjectClass || !il2cpp_object_new || !il2cpp_string_new ||
          !g_gameObject_ctor || !g_transform_set_parent || !g_transform_set_localPosition ||
          !g_transform_set_localRotation || !g_transform_set_localScale || !g_object_destroy)
        return fail("New Skeleton node APIs are unavailable");
      void *object=il2cpp_object_new(g_gameObjectClass);
      auto hold=EiemUnityRef::Capture(object,false);
      if (!object || !hold) return fail("Cannot allocate Skeleton node");
      // Private runtime names avoid collisions between Mod-owned nodes.
      // Mesh paths resolve through this instance's explicit node map.
      static uint64_t serial=0;
      std::string name="EIEM_Bone_"+std::to_string(++serial);
      void *args[]={il2cpp_string_new(name.c_str())};
      instance->createdObjects.push_back(hold);
      if (!args[0] || !EiemSkeletonCall(g_gameObject_ctor,object,args)) return fail("Cannot construct Skeleton node");
      if (!EiemSkeletonCall(g_gameObject_get_transform,object,nullptr,&transform) || !transform) return fail("Skeleton node has no Transform");
      bool keepWorld=false; void *parent=instance->nodes[node.parent].Target();
      void *parentArgs[]={parent,&keepWorld};
      void *position[]={const_cast<float *>(node.position)};
      void *rotation[]={const_cast<float *>(node.rotation)};
      void *scale[]={const_cast<float *>(node.scale)};
      if (EiemNativeObjectStatus(parent)!=1 || !EiemSkeletonCall(g_transform_set_parent,transform,parentArgs) ||
          !EiemSkeletonCall(g_transform_set_localPosition,transform,position) ||
          !EiemSkeletonCall(g_transform_set_localRotation,transform,rotation) ||
          !EiemSkeletonCall(g_transform_set_localScale,transform,scale)) return fail("Cannot attach Skeleton node: "+node.path);
    }
    auto reference=EiemUnityRef::Capture(transform,false);
    if (!reference || reference.Status()!=1) return fail("Cannot retain Skeleton Transform: "+node.path);
    instance->nodes.push_back(std::move(reference));
  }
  instance->ready=true;
  Log("[SKELETON] Ready resource=%s sourceRoot=%p nodes=%zu added=%zu (GPU validation separate)",
      key.c_str(),source.front(),instance->nodes.size(),instance->createdObjects.size());
  if (EiemRegistrationTraceFirst("skeleton-rebuilt", instance->key.c_str(),
                                 instance.get(), source.front(), nullptr, -1))
    Log("%s event=skeleton-rebuilt instance=%p stamp=%llu anchor=%p nodes=%zu added=%zu",
        EiemRegistrationTraceTag, instance.get(),
        (unsigned long long)instance->stamp, source.front(),
        instance->nodes.size(), instance->createdObjects.size());
  out=std::move(instance); return true;
}

static bool EiemAcquireSkeleton(const EiemModRule &rule, void *renderer,
                                 std::shared_ptr<EiemSkeletonInstance> &out,
                                 char *message, size_t messageSize) {
  out.reset();
  EiemModResource resource={}; char path[kEiemResourceDiskPathCapacity]={};
  if (!EiemFindModResource(rule.modPath,rule.skeleton,"Skeleton",&resource) ||
      !EiemResolveResourceDiskPath(resource,path,sizeof(path))) {
    if (message) strncpy_s(message,messageSize,
        "Skeleton resource is not declared or has no path",_TRUNCATE);
    return false;
  }
  std::string error; EiemNativeReader reader(path); EiemSkeletonDocument document;
  if (!EiemReadSkeleton(reader,document,error)) {
    if (message) strncpy_s(message,messageSize,error.c_str(),_TRUNCATE);
    return false;
  }
  // A Render action explicitly paired with Physics may use a Skeleton
  // exported from a larger PFB than the current UI/NPC hierarchy. Allow the
  // declared source nodes to become owned anchors for that Physics path;
  // Mesh-only Skeleton actions keep strict source matching below.
  std::unordered_set<std::string> virtualPaths;
  if (rule.hasPhysics)
    for (const auto &node : document.nodes)
      if (node.source) virtualPaths.insert(node.path);
  return EiemAcquireSkeletonDocument(
      EiemSkeletonInstanceKey(rule.modPath,path),EiemMeshResourceFileStamp(path),
      std::move(document),renderer,out,message,messageSize,
      rule.hasPhysics ? &virtualPaths : nullptr);
}



static bool EiemSkeletonMeshBones(const EiemSkinIdentity &skin, const EiemSkeletonInstance &instance,
                                   void **out, char *message, size_t size) {
  *out=nullptr; std::string error; std::vector<std::string> paths;
  if (!instance.ready || instance.nodes.size()!=instance.document.nodes.size() ||
      !il2cpp_array_new || !g_transformClass) {
    if (message) strncpy_s(message,size,"Skeleton instance or Transform array API is unavailable",_TRUNCATE);
    return false;
  }
  // Mesh paths are relative to the named armature root, while EIESKEL v2
  // may contain an empty serialization root above that node. Build one
  // child-index table per named root used by this Mesh so the same exported
  // keys mean the same Transform for native and Mod-owned Skeleton nodes.
  if (skin.boneIndexPaths.empty() || skin.boneIndexPaths.size() != skin.paths.size()) {
    if (message) strncpy_s(message, size,
                           "Skeleton binding requires a complete canonical bone index table",
                           _TRUNCATE);
    return false;
  }
  std::unordered_map<std::string, size_t> byPath;
  for (size_t index = 0; index < instance.document.nodes.size(); ++index)
    byPath.emplace(instance.document.nodes[index].path, index);
  std::vector<std::vector<size_t>> children(instance.document.nodes.size());
  for (size_t index = 0; index < instance.document.nodes.size(); ++index) {
    if (index == 0 && instance.document.nodes[index].parent < 0) continue;
    const int32_t parent = instance.document.nodes[index].parent;
    if (parent < 0 || (size_t)parent >= children.size()) {
      if (message) strncpy_s(message, size, "Skeleton node parent is invalid", _TRUNCATE);
      return false;
    }
    children[(size_t)parent].push_back(index);
  }
  std::unordered_map<size_t, std::unordered_map<std::string, size_t>> tables;
  auto tableForRoot = [&](size_t root) -> const std::unordered_map<std::string, size_t> * {
    auto existing = tables.find(root);
    if (existing != tables.end()) return &existing->second;
    if (root >= children.size()) return nullptr;
    std::unordered_map<std::string, size_t> table;
    std::vector<std::pair<size_t, std::string>> pending;
    pending.emplace_back(root, std::string());
    while (!pending.empty()) {
      auto current = std::move(pending.back());
      pending.pop_back();
      table.emplace(current.second, current.first);
      for (size_t child = children[current.first].size(); child > 0; --child) {
        const size_t local = child - 1;
        const size_t node = children[current.first][local];
        std::string key = current.second;
        if (!key.empty()) key.push_back('/');
        key += std::to_string(local);
        pending.emplace_back(node, std::move(key));
      }
    }
    auto inserted = tables.emplace(root, std::move(table));
    return &inserted.first->second;
  };
  std::vector<void *> values;
  values.reserve(skin.boneIndexPaths.size());
  for (size_t slot = 0; slot < skin.boneIndexPaths.size(); ++slot) {
    const std::string &authorPath = skin.paths[slot];
    const size_t slash = authorPath.find('/');
    const std::string rootPath = slash == std::string::npos
                                     ? authorPath
                                     : authorPath.substr(0, slash);
    auto root = byPath.find(rootPath);
    if (root == byPath.end() && rootPath.empty()) root = byPath.find("");
    const auto *table = root == byPath.end() ? nullptr : tableForRoot(root->second);
    const auto found = table ? table->find(skin.boneIndexPaths[slot])
                             : std::unordered_map<std::string, size_t>::const_iterator{};
    if (!table || found == table->end() || found->second >= instance.nodes.size() ||
        instance.nodes[found->second].Status() != 1) {
      if (message) strncpy_s(message, size,
                             ("Skeleton canonical bone not found: " +
                              skin.boneIndexPaths[slot]).c_str(), _TRUNCATE);
      return false;
    }
    values.push_back(instance.nodes[found->second].Target());
  }
  void *array = il2cpp_array_new(g_transformClass, values.size());
  if (!array) return false;
  memcpy((char *)array + IL2CPP_ARRAY_DATA, values.data(),
         values.size() * sizeof(void *));
  *out = array;
  return true;
}
