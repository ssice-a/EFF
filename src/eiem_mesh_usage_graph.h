#pragma once

// Runtime Mesh usage/lineage evidence.  This is deliberately separate from
// the replacement ledger: the graph observes what the game does and never
// assigns a Mesh or Material by itself.

#if defined(EIEM_MESH_USAGE_GRAPH_BUILD)

static constexpr size_t kEiemMeshGraphMeshCapacity = 4096;
static constexpr size_t kEiemMeshGraphRendererCapacity = 4096;
static constexpr size_t kEiemMeshGraphProbeWays = 8;
static constexpr size_t kEiemMeshGraphMaterialSlots = 32;

struct EiemMeshGraphMeshNode {
  void *mesh = nullptr;
  void *parentMesh = nullptr;
  uint32_t flags = 0; // 1=rule binding, 2=derived, 4=used by tracked renderer
  LONG ruleGeneration = -1;
  uint64_t firstSeen = 0;
  uint64_t lastSeen = 0;
  int64_t originHash = 0;
  char name[192] = {};
  char originPath[768] = {};
};

struct EiemMeshGraphRendererNode {
  void *renderer = nullptr;
  void *currentMesh = nullptr;
  void *materials[kEiemMeshGraphMaterialSlots] = {};
  uint32_t materialCount = 0;
  uint64_t firstSeen = 0;
  uint64_t lastSeen = 0;
  char rendererType[40] = {};
  char name[192] = {};
  char hierarchy[768] = {};
};

static SRWLOCK s_eiemMeshGraphLock = SRWLOCK_INIT;
static EiemMeshGraphMeshNode
    s_eiemMeshGraphMeshes[kEiemMeshGraphMeshCapacity] = {};
static EiemMeshGraphRendererNode
    s_eiemMeshGraphRenderers[kEiemMeshGraphRendererCapacity] = {};
static volatile LONG s_eiemMeshGraphMeshCount = 0;
static volatile LONG s_eiemMeshGraphRendererCount = 0;
static volatile LONG64 s_eiemMeshGraphEventSequence = 0;
static volatile LONG s_eiemMeshGraphLogCount = 0;
static thread_local bool s_eiemMeshGraphReentrant = false;

static size_t EiemMeshGraphHash(void *object, size_t capacity) {
  return (reinterpret_cast<uintptr_t>(object) >> 4) & (capacity - 1);
}

static EiemMeshGraphMeshNode *EiemMeshGraphFindMeshLocked(void *mesh,
                                                           bool create) {
  if (!mesh) return nullptr;
  const size_t start = EiemMeshGraphHash(mesh, kEiemMeshGraphMeshCapacity);
  for (size_t way = 0; way < kEiemMeshGraphProbeWays; ++way) {
    auto &entry = s_eiemMeshGraphMeshes[(start + way) &
                                         (kEiemMeshGraphMeshCapacity - 1)];
    if (entry.mesh == mesh) return &entry;
    if (!entry.mesh) {
      if (!create) return nullptr;
      entry.mesh = mesh;
      InterlockedIncrement(&s_eiemMeshGraphMeshCount);
      return &entry;
    }
  }
  return nullptr;
}

static EiemMeshGraphRendererNode *EiemMeshGraphFindRendererLocked(
    void *renderer, bool create) {
  if (!renderer) return nullptr;
  const size_t start =
      EiemMeshGraphHash(renderer, kEiemMeshGraphRendererCapacity);
  for (size_t way = 0; way < kEiemMeshGraphProbeWays; ++way) {
    auto &entry = s_eiemMeshGraphRenderers[
        (start + way) & (kEiemMeshGraphRendererCapacity - 1)];
    if (entry.renderer == renderer) return &entry;
    if (!entry.renderer) {
      if (!create) return nullptr;
      entry.renderer = renderer;
      InterlockedIncrement(&s_eiemMeshGraphRendererCount);
      return &entry;
    }
  }
  return nullptr;
}

static void EiemMeshGraphCopyMeshIdentity(void *mesh,
                                          EiemMeshGraphMeshNode &node) {
  if (!mesh) return;
  if (EiemOnUnityThread())
    TraceReadUnityObjectName(mesh, node.name, sizeof(node.name));
  TraceLookupAssetOrigin(mesh, &node.originHash, node.originPath,
                         sizeof(node.originPath));
}

static bool EiemMeshGraphRendererTracked(void *renderer) {
  if (!renderer) return false;
  AcquireSRWLockShared(&s_eiemOverrideLock);
  const bool tracked = EiemFindOverrideLocked(renderer) != SIZE_MAX;
  ReleaseSRWLockShared(&s_eiemOverrideLock);
  return tracked;
}

static LONG EiemMeshGraphCurrentGeneration() {
  return InterlockedCompareExchange(&s_eiemModGeneration, 0, 0);
}

// A RendererInfo entry already present in the replacement ledger is stronger
// evidence than the live pointer identity walk.  Native VFX clones can carry
// a wrapper Mesh pointer that is created before the replacement ledger has
// finished binding, so keep that evidence on the Mesh node as well.
static void EiemMeshGraphMarkMeshRelevant(void *mesh) {
  if (!mesh) return;
  AcquireSRWLockExclusive(&s_eiemMeshGraphLock);
  auto *node = EiemMeshGraphFindMeshLocked(mesh, false);
  if (node) node->flags |= 4u;
  ReleaseSRWLockExclusive(&s_eiemMeshGraphLock);
}

static bool EiemMeshGraphMeshMatchesRule(void *mesh) {
  if (!mesh || !EiemOnUnityThread()) return false;
  EiemModRule rule = {};
  return EiemFindRenderRuleForMeshBinding(mesh, &rule);
}

static EiemMeshGraphMeshNode *EiemMeshGraphTouchMesh(void *mesh,
                                                     void *parentMesh,
                                                     const char *reason,
                                                     bool *createdOut,
                                                     bool *matchedOut) {
  if (createdOut) *createdOut = false;
  if (matchedOut) *matchedOut = false;
  if (!mesh || g_shutdownRequested || s_eiemMeshGraphReentrant)
    return nullptr;

  const LONG generation = EiemMeshGraphCurrentGeneration();

  // Fast path: once a pointer is known, do not invoke Unity object getters on
  // every setter call. The normal assignment path pays only a shared-lock
  // lookup after the graph is warm.
  AcquireSRWLockShared(&s_eiemMeshGraphLock);
  EiemMeshGraphMeshNode *known = EiemMeshGraphFindMeshLocked(mesh, false);
  if (known && (!parentMesh || parentMesh == mesh || known->parentMesh)) {
    const bool needsRuleCheck = EiemOnUnityThread() &&
                                !(known->flags & 1u) &&
                                known->ruleGeneration != generation;
    bool matched = (known->flags & 7u) != 0;
    if (matchedOut) *matchedOut = matched;
    ReleaseSRWLockShared(&s_eiemMeshGraphLock);
    // Re-evaluate an initially unknown pointer once per mod generation.  The
    // old code permanently cached a negative result from before the renderer
    // entered the override ledger, which produced matched=0 for real mod
    // meshes during sprint material commits.
    if (needsRuleCheck) {
      const bool identityMatch = EiemMeshGraphMeshMatchesRule(mesh);
      AcquireSRWLockExclusive(&s_eiemMeshGraphLock);
      auto *current = EiemMeshGraphFindMeshLocked(mesh, false);
      if (current && current->ruleGeneration != generation) {
        current->ruleGeneration = generation;
        if (identityMatch) current->flags |= 1u;
      }
      matched = current && (current->flags & 7u) != 0;
      ReleaseSRWLockExclusive(&s_eiemMeshGraphLock);
      if (matchedOut) *matchedOut = matched;
    }
    return known;
  }
  ReleaseSRWLockShared(&s_eiemMeshGraphLock);

  // Do the potentially expensive rule-identity walk without holding the
  // graph lock. This keeps the lock order independent from the replacement
  // ledger's lock and avoids a setter/commit deadlock during reconciliation.
  const bool canCheckRule = EiemOnUnityThread();
  const bool identityMatch = canCheckRule && EiemMeshGraphMeshMatchesRule(mesh);

  // Unity object getters are called only for a new pointer or a new lineage
  // edge. The normal assignment path therefore pays no metadata cost after
  // the graph is warm.
  EiemMeshGraphMeshNode identity = {};
  identity.mesh = mesh;
  identity.parentMesh = parentMesh && parentMesh != mesh ? parentMesh : nullptr;
  identity.firstSeen = identity.lastSeen = GetTickCount64();
  s_eiemMeshGraphReentrant = true;
  EiemMeshGraphCopyMeshIdentity(mesh, identity);
  s_eiemMeshGraphReentrant = false;

  AcquireSRWLockExclusive(&s_eiemMeshGraphLock);
  EiemMeshGraphMeshNode *node =
      EiemMeshGraphFindMeshLocked(mesh, true);
  if (!node) {
    ReleaseSRWLockExclusive(&s_eiemMeshGraphLock);
    return nullptr;
  }
  const bool wasNew = node->firstSeen == 0;
  if (wasNew) {
    node->firstSeen = identity.firstSeen;
    node->lastSeen = identity.lastSeen;
    node->originHash = identity.originHash;
    memcpy(node->name, identity.name, sizeof(node->name));
    memcpy(node->originPath, identity.originPath, sizeof(node->originPath));
    node->ruleGeneration = canCheckRule ? generation : -1;
    if (identityMatch) node->flags |= 1u;
  } else {
    node->lastSeen = GetTickCount64();
    if (canCheckRule && node->ruleGeneration != generation) {
      node->ruleGeneration = generation;
      if (identityMatch) node->flags |= 1u;
    }
  }
  if (parentMesh && parentMesh != mesh && !node->parentMesh)
    node->parentMesh = parentMesh;
  if (node->parentMesh) {
    auto *parent = EiemMeshGraphFindMeshLocked(node->parentMesh, false);
    if (parent && (parent->flags & 1u)) node->flags |= 2u;
  }
  const bool matched = (node->flags & 7u) != 0;
  if (createdOut) *createdOut = wasNew;
  if (matchedOut) *matchedOut = matched;
  EiemMeshGraphMeshNode snapshot = *node;
  ReleaseSRWLockExclusive(&s_eiemMeshGraphLock);

  if (wasNew) {
    const LONG logIndex = InterlockedIncrement(&s_eiemMeshGraphLogCount);
    if (logIndex <= 12000) {
      const LONG64 sequence = InterlockedIncrement64(&s_eiemMeshGraphEventSequence);
      Log("[MESH-GRAPH] seq=%lld event=mesh-node mesh=%p parent=%p "
          "matched=%d derived=%d reason=%s name=%s originHash=%lld origin=%s",
          (long long)sequence, snapshot.mesh, snapshot.parentMesh,
          (snapshot.flags & 1u) ? 1 : 0, (snapshot.flags & 2u) ? 1 : 0,
          reason ? reason : "unknown", snapshot.name[0] ? snapshot.name : "<none>",
          (long long)snapshot.originHash,
          snapshot.originPath[0] ? snapshot.originPath : "<none>");
    }
  }
  return node;
}

static void EiemMeshGraphObserveDerivedMesh(void *parentMesh, void *mesh,
                                             const char *reason) {
  if (!kEiemEnableMeshUsageGraph || !parentMesh || !mesh ||
      parentMesh == mesh)
    return;
  // Register the parent first so the child can inherit a known rule-binding
  // flag even when the clone is the first pointer seen by the graph.
  EiemMeshGraphTouchMesh(parentMesh, nullptr, "lineage-parent", nullptr,
                         nullptr);
  bool created = false;
  bool matched = false;
  EiemMeshGraphTouchMesh(mesh, parentMesh, reason, &created, &matched);
  (void)created;
  (void)matched;
}

static void EiemMeshGraphObserveMeshAssignment(void *renderer,
                                               void *requestedMesh,
                                               void *assignedMesh,
                                               const char *rendererType,
                                               const char *phase) {
  if (!kEiemEnableMeshUsageGraph || !renderer || s_eiemMeshGraphReentrant)
    return;
  if (!requestedMesh && !assignedMesh) return;

  bool requestedCreated = false;
  bool requestedMatched = false;
  if (requestedMesh)
    EiemMeshGraphTouchMesh(requestedMesh, nullptr, "mesh-request",
                           &requestedCreated, &requestedMatched);
  bool assignedCreated = false;
  bool assignedMatched = false;
  if (assignedMesh)
    EiemMeshGraphTouchMesh(assignedMesh, requestedMesh != assignedMesh
                                            ? requestedMesh
                                            : nullptr,
                           phase ? phase : "mesh-assignment", &assignedCreated,
                           &assignedMatched);

  const bool tracked = EiemMeshGraphRendererTracked(renderer);
  if (tracked) {
    if (requestedMesh) EiemMeshGraphMarkMeshRelevant(requestedMesh);
    if (assignedMesh) EiemMeshGraphMarkMeshRelevant(assignedMesh);
    requestedMatched = true;
    assignedMatched = true;
  }

  char rendererName[192] = {};
  char hierarchy[768] = {};
  if ((assignedCreated || requestedCreated) && EiemOnUnityThread()) {
    s_eiemMeshGraphReentrant = true;
    TraceReadUnityObjectName(renderer, rendererName, sizeof(rendererName));
    TraceBuildRendererHierarchy(renderer, hierarchy, sizeof(hierarchy));
    s_eiemMeshGraphReentrant = false;
  }

  AcquireSRWLockExclusive(&s_eiemMeshGraphLock);
  EiemMeshGraphRendererNode *node =
      EiemMeshGraphFindRendererLocked(renderer, true);
  if (!node) {
    ReleaseSRWLockExclusive(&s_eiemMeshGraphLock);
    return;
  }
  const bool rendererWasNew = node->firstSeen == 0;
  void *previousMesh = node->currentMesh;
  node->currentMesh = assignedMesh ? assignedMesh : requestedMesh;
  node->lastSeen = GetTickCount64();
  if (rendererWasNew) {
    node->firstSeen = node->lastSeen;
    strncpy_s(node->rendererType, sizeof(node->rendererType),
              rendererType ? rendererType : "Renderer", _TRUNCATE);
    strncpy_s(node->name, sizeof(node->name), rendererName, _TRUNCATE);
    strncpy_s(node->hierarchy, sizeof(node->hierarchy), hierarchy, _TRUNCATE);
  }
  const bool changed = previousMesh != node->currentMesh;
  EiemMeshGraphRendererNode snapshot = *node;
  ReleaseSRWLockExclusive(&s_eiemMeshGraphLock);
  if (changed || rendererWasNew || requestedMesh != assignedMesh) {
    const LONG logIndex = InterlockedIncrement(&s_eiemMeshGraphLogCount);
    if (logIndex <= 12000) {
      const LONG64 sequence = InterlockedIncrement64(&s_eiemMeshGraphEventSequence);
      Log("[MESH-GRAPH] seq=%lld event=mesh-use renderer=%p type=%s "
          "previous=%p requested=%p assigned=%p tracked=%d "
          "requestedMatched=%d assignedMatched=%d phase=%s name=%s hierarchy=%s",
          (long long)sequence, renderer,
          rendererType ? rendererType : "Renderer", previousMesh,
          requestedMesh, assignedMesh, tracked ? 1 : 0,
          requestedMatched ? 1 : 0, assignedMatched ? 1 : 0,
          phase ? phase : "unknown", snapshot.name[0] ? snapshot.name : "<none>",
          snapshot.hierarchy[0] ? snapshot.hierarchy : "<none>");
    }
  }
}

static void *EiemMeshGraphReadRendererMesh(void *renderer) {
  if (!renderer) return nullptr;
  void *mesh = nullptr;
  __try {
    if (g_smr_get_sharedMesh) mesh = Invoke(g_smr_get_sharedMesh, renderer);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    mesh = nullptr;
  }
  if (mesh) return mesh;
  __try {
    if (g_meshFilter_get_sharedMesh)
      mesh = Invoke(g_meshFilter_get_sharedMesh, renderer);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    mesh = nullptr;
  }
  return mesh;
}

static void *EiemMeshGraphMaterialAt(void *input, bool inputIsArray,
                                      size_t index) {
  if (!input) return nullptr;
  if (!inputIsArray) return index == 0 ? input : nullptr;
  const size_t count = EiemManagedArrayLength(input);
  if (index >= count) return nullptr;
  __try {
    return *(void **)((char *)input + IL2CPP_ARRAY_DATA +
                      index * sizeof(void *));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

static void EiemMeshGraphObserveMaterialCommit(void *renderer, void *input,
                                                bool inputIsArray,
                                                const char *method,
                                                const char *phase) {
  if (!kEiemEnableMeshUsageGraph || !renderer || s_eiemMeshGraphReentrant)
    return;
  const bool tracked = EiemMeshGraphRendererTracked(renderer);
  void *mesh = EiemMeshGraphReadRendererMesh(renderer);
  if (!mesh) return;

  bool created = false;
  bool matched = false;
  EiemMeshGraphTouchMesh(mesh, nullptr, "material-context", &created,
                         &matched);
  if (tracked) {
    EiemMeshGraphMarkMeshRelevant(mesh);
    matched = true;
  }
  if (!tracked && !matched) return;

  const size_t inputCount = inputIsArray
                                ? EiemManagedArrayLength(input)
                                : (input ? 1u : 0u);
  const size_t count = inputCount > kEiemMeshGraphMaterialSlots
                           ? kEiemMeshGraphMaterialSlots
                           : inputCount;
  void *newMaterials[kEiemMeshGraphMaterialSlots] = {};
  for (size_t index = 0; index < count; ++index)
    newMaterials[index] = EiemMeshGraphMaterialAt(input, inputIsArray, index);

  bool changed = false;
  bool rendererWasNew = false;
  void *previousMesh = nullptr;
  void *previousMaterials[kEiemMeshGraphMaterialSlots] = {};
  uint32_t previousCount = 0;
  EiemMeshGraphRendererNode snapshot = {};
  AcquireSRWLockExclusive(&s_eiemMeshGraphLock);
  EiemMeshGraphRendererNode *node =
      EiemMeshGraphFindRendererLocked(renderer, true);
  if (node) {
    rendererWasNew = node->firstSeen == 0;
    previousMesh = node->currentMesh;
    previousCount = node->materialCount;
    memcpy(previousMaterials, node->materials, sizeof(previousMaterials));
    node->currentMesh = mesh;
    node->materialCount = (uint32_t)count;
    memcpy(node->materials, newMaterials, sizeof(newMaterials));
    node->lastSeen = GetTickCount64();
    if (rendererWasNew) node->firstSeen = node->lastSeen;
    if (previousMesh != mesh || previousCount != count) changed = true;
    if (previousCount == count &&
        memcmp(previousMaterials, newMaterials, sizeof(newMaterials)) != 0)
      changed = true;
    snapshot = *node;
  }
  ReleaseSRWLockExclusive(&s_eiemMeshGraphLock);
  if (!node) return;

  if (!changed && !rendererWasNew) return;
  const LONG logIndex = InterlockedIncrement(&s_eiemMeshGraphLogCount);
  if (logIndex > 12000) return;
  char slots[4096] = {};
  size_t used = 0;
  for (size_t index = 0; index < count; ++index) {
    char materialName[128] = {};
    if (newMaterials[index] && EiemOnUnityThread())
      TraceReadUnityObjectName(newMaterials[index], materialName,
                               sizeof(materialName));
    char item[320] = {};
    _snprintf_s(item, sizeof(item), _TRUNCATE, "%zu=%p:%s", index,
                newMaterials[index], materialName[0] ? materialName : "<none>");
    const size_t itemLength = strlen(item);
    if (used && used + 1 < sizeof(slots)) slots[used++] = ',';
    if (used + itemLength >= sizeof(slots)) break;
    memcpy(slots + used, item, itemLength);
    used += itemLength;
    slots[used] = '\0';
  }
  const LONG64 sequence = InterlockedIncrement64(&s_eiemMeshGraphEventSequence);
  Log("[MESH-GRAPH] seq=%lld event=material-state renderer=%p mesh=%p "
      "tracked=%d matched=%d method=%s phase=%s previousMesh=%p "
      "previousMaterialCount=%u materialCount=%zu slots=%s name=%s hierarchy=%s",
      (long long)sequence, renderer, mesh, tracked ? 1 : 0, matched ? 1 : 0,
      method ? method : "unknown", phase ? phase : "unknown", previousMesh,
      previousCount, count, slots[0] ? slots : "<none>",
      snapshot.name[0] ? snapshot.name : "<none>",
      snapshot.hierarchy[0] ? snapshot.hierarchy : "<none>");
}

#else

static void EiemMeshGraphObserveDerivedMesh(void *, void *, const char *) {}
static void EiemMeshGraphObserveMeshAssignment(void *, void *, void *,
                                               const char *, const char *) {}
static void EiemMeshGraphObserveMaterialCommit(void *, void *, bool,
                                                const char *, const char *) {}

#endif
