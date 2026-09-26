#pragma once

#include "eiem_runtime_features.h"
#include "eiem_mod_document.h"
#include "eiem_persistent_state.h"
#include <utility>

// Configuration publication and lookups; parsing has no runtime state.
static SRWLOCK s_eiemModLock = SRWLOCK_INIT;
static EiemModProgram s_eiemModProgram;
static volatile LONG s_eiemModGeneration = 0;
static volatile LONG s_eiemModControlGeneration = 0;
static std::string s_eiemSelectedModPath;
static volatile LONG s_eiemModManagerOpen = 0;
static EiemPersistentStore s_eiemPersistentStates;

// The built-in manager controls a game Mod. Its focused overlay uses the
// game's Key scope; a separate authored Lua UI still uses the UI scope.
static bool EiemModUsesUiKeyScope(bool gameWindowFocus,
                                  bool managerWindowFocus,
                                  bool managerOpen) {
  return !gameWindowFocus && !(managerWindowFocus && managerOpen);
}

static bool EiemModAffected(const char *path, const std::vector<std::string> *mods) {
  if (!mods) return true;
  for (const auto &mod : *mods) if (EiemModEquals(mod.c_str(), path)) return true;
  return false;
}

struct EiemModControlSnapshot {
  std::string modPath;
  std::vector<EiemModKey> keys;
  std::vector<EiemModShapeControl> shapeControls;
  EiemVariables variables;
  bool selected = false;
};

static bool EiemSelectableMod(const EiemModProgram &program,
                              size_t stateIndex) {
  return !program.states[stateIndex].keys.empty() ||
         !program.states[stateIndex].shapeControls.empty();
}

// Caller owns s_eiemModLock exclusively.
static void EiemReconcileControlledModLocked() {
  std::string next;
  std::string firstWithKeys, firstWithShapes;
  for (size_t index = 0; index < s_eiemModProgram.states.size(); ++index) {
    const auto &state = s_eiemModProgram.states[index];
    if (!EiemSelectableMod(s_eiemModProgram, index)) continue;
    if (firstWithKeys.empty() && !state.keys.empty()) firstWithKeys = state.path;
    if (firstWithShapes.empty() && !state.shapeControls.empty()) firstWithShapes = state.path;
    if (EiemModEquals(state.path.c_str(), s_eiemSelectedModPath.c_str())) {
      next = state.path;
      break;
    }
  }
  if (next.empty()) next = firstWithKeys.empty() ? firstWithShapes : firstWithKeys;
  const bool changed = !EiemModEquals(next.c_str(), s_eiemSelectedModPath.c_str());
  s_eiemSelectedModPath = std::move(next);
  if (changed) InterlockedIncrement(&s_eiemModControlGeneration);
}

static bool EiemSelectControlledMod(const std::string &modPath) {
  bool found = false, changed = false;
  AcquireSRWLockExclusive(&s_eiemModLock);
  for (size_t index = 0; index < s_eiemModProgram.states.size(); ++index) {
    const auto &state = s_eiemModProgram.states[index];
    if (!EiemSelectableMod(s_eiemModProgram, index) ||
        !EiemModEquals(state.path.c_str(), modPath.c_str()))
      continue;
    found = true;
    changed = !EiemModEquals(state.path.c_str(), s_eiemSelectedModPath.c_str());
    s_eiemSelectedModPath = state.path;
    break;
  }
  if (changed) InterlockedIncrement(&s_eiemModControlGeneration);
  ReleaseSRWLockExclusive(&s_eiemModLock);
  return found;
}

static std::string EiemGetSelectedModPath() {
  AcquireSRWLockShared(&s_eiemModLock);
  std::string result = s_eiemSelectedModPath;
  ReleaseSRWLockShared(&s_eiemModLock);
  return result;
}

static std::vector<EiemModControlSnapshot> EiemGetModControls(
    LONG *generation, LONG *controlGeneration) {
  std::vector<EiemModControlSnapshot> result;
  AcquireSRWLockShared(&s_eiemModLock);
  if (generation) *generation = s_eiemModGeneration;
  if (controlGeneration) *controlGeneration = s_eiemModControlGeneration;
  for (size_t index = 0; index < s_eiemModProgram.states.size(); ++index) {
    const auto &state = s_eiemModProgram.states[index];
    auto shapeControls = state.shapeControls;
    if (state.keys.empty() && shapeControls.empty()) continue;
    result.push_back({state.path, state.keys, std::move(shapeControls), state.variables,
                      EiemModEquals(state.path.c_str(),
                                    s_eiemSelectedModPath.c_str())});
  }
  ReleaseSRWLockShared(&s_eiemModLock);
  return result;
}

struct EiemModHotkeyBinding {
  EiemKeyChord chord;
  EiemModKeyBehavior behavior = EiemModKeyBehavior::Cycle;
};

static std::vector<EiemModHotkeyBinding> EiemGetModKeyBindings(
    LONG *generation, LONG *controlGeneration, std::string *modPath,
    bool uiFocus = false) {
  std::vector<EiemModHotkeyBinding> result;
  AcquireSRWLockShared(&s_eiemModLock);
  if (generation) *generation = s_eiemModGeneration;
  if (controlGeneration) *controlGeneration = s_eiemModControlGeneration;
  if (modPath) *modPath = s_eiemSelectedModPath;
  for (const auto &state : s_eiemModProgram.states) {
    if (!EiemModEquals(state.path.c_str(), s_eiemSelectedModPath.c_str()))
      continue;
    for (const auto &key : state.keys)
      if (EiemKeyInScope(key.scope,uiFocus)) {
        auto existing = std::find_if(result.begin(), result.end(),
            [&](const auto &binding) { return binding.chord == key.chord; });
        if (existing == result.end())
          result.push_back({key.chord, key.behavior});
        else if (key.behavior == EiemModKeyBehavior::Hold)
          existing->behavior = EiemModKeyBehavior::Hold;
      }
  }
  ReleaseSRWLockShared(&s_eiemModLock);
  return result;
}

static std::vector<EiemKeyChord> EiemGetModKeyChords(
    LONG *generation, LONG *controlGeneration, std::string *modPath,
    bool uiFocus = false) {
  std::vector<EiemKeyChord> result;
  for (const auto &binding : EiemGetModKeyBindings(
           generation, controlGeneration, modPath, uiFocus))
    result.push_back(binding.chord);
  return result;
}

static std::vector<EiemKeyChord> EiemGetModKeyChords(
    LONG *generation, bool uiFocus = false) {
  LONG controlGeneration = 0;
  std::string modPath;
  return EiemGetModKeyChords(generation, &controlGeneration, &modPath,
                             uiFocus);
}

struct EiemModInputEvent {
  EiemKeyChord chord; LONG generation;
  std::string modPath, uiSection;
  EiemVariables values; // one successful Lua frame, committed atomically
  bool uiFocus = false; // focus at key dispatch; used only by generic Key scope
  std::string keySection; // non-empty when the manager invokes one Key section
  bool holdTick = false; // true for polling ticks; cycle keys ignore these
  double holdSeconds = 0.02;
  bool directValues = false; // built-in manager sliders, even without a Lua UI
};

struct EiemUiSnapshot {
  std::string modPath;
  EiemModUi ui;
  EiemVariables variables;
  LONG generation;
  EiemVariables defaults;
};

// A Physics action is selected by the same Render rule that selected its Mesh
// consumer.  It is nevertheless owned by the completed model instance, not by
// an individual Renderer: several Renderer/LOD hits can reference one prepared
// Physics snapshot and must produce one native instance when that adapter is
// connected.
struct EiemPhysicsIntent {
  char modPath[MAX_PATH] = {};
  char resourceSection[96] = {};
  char firstRenderSection[96] = {};
  EiemModRule firstRule = {};
  std::shared_ptr<const EiemPhysicsAsset> asset;
  // Rebuilt on every model pass. These are observation addresses only; the
  // runtime adapter captures and validates its own weak references before use.
  std::vector<void *> matchedRenderers;
  uint32_t rendererMatches = 0;
};

static std::vector<EiemUiSnapshot> EiemGetModUis(LONG *generation) {
  std::vector<EiemUiSnapshot> result;
  AcquireSRWLockShared(&s_eiemModLock);
  *generation = s_eiemModGeneration;
  for (const auto &state : s_eiemModProgram.states)
    for (const auto &ui : state.uis)
      result.push_back({state.path, ui, state.variables, s_eiemModGeneration,state.defaults});
  ReleaseSRWLockShared(&s_eiemModLock);
  return result;
}

static bool EiemSameRenderAssembly(EiemModRule a, EiemModRule b) {
  // New/removed channels may need to discover a previously match-only consumer.
  if (a.shapeCount != b.shapeCount) return false;
  for (uint32_t i = 0; i < a.shapeCount; ++i)
    if (strcmp(a.shapeNames[i], b.shapeNames[i])) return false;
  memset(a.shapeNames, 0, sizeof(a.shapeNames)); memset(b.shapeNames, 0, sizeof(b.shapeNames));
  memset(a.shapeWeights, 0, sizeof(a.shapeWeights)); memset(b.shapeWeights, 0, sizeof(b.shapeWeights));
  a.shapeCount = b.shapeCount = 0;
  memset(a.shapeSpeedNames, 0, sizeof(a.shapeSpeedNames));
  memset(b.shapeSpeedNames, 0, sizeof(b.shapeSpeedNames));
  memset(a.shapeSpeeds, 0, sizeof(a.shapeSpeeds));
  memset(b.shapeSpeeds, 0, sizeof(b.shapeSpeeds));
  a.shapeSpeedCount = b.shapeSpeedCount = 0;
  return memcmp(&a, &b, sizeof(a)) == 0;
}

static bool EiemSameRenderWithoutSubmeshVisibility(EiemModRule a,
                                                   EiemModRule b) {
  // A submesh visibility key changes only the generated index buffer. Keep
  // this separate from partner-link updates so the source Renderer is
  // refreshed without destroying any model-owned objects.
  a.hiddenSubmeshMask = b.hiddenSubmeshMask = 0;
  return memcmp(&a, &b, sizeof(a)) == 0;
}

struct EiemSubmeshVisibilityChange {
  std::string modPath;
  std::string section;
  uint32_t beforeMask = 0;
  uint32_t afterMask = 0;
};

static bool EiemPrepareInputUpdate(const std::vector<EiemModInputEvent> &events,
                                   EiemModProgram *next, std::vector<std::string> *affected,
                                   bool *shapesOnly = nullptr,
                                   bool *submeshVisibilityOnly = nullptr,
                                   std::vector<EiemSubmeshVisibilityChange>
                                       *visibilityChanges = nullptr) {
  AcquireSRWLockShared(&s_eiemModLock);
  *next = s_eiemModProgram;
  const LONG generation = s_eiemModGeneration;
  const std::string selectedModPath = s_eiemSelectedModPath;
  ReleaseSRWLockShared(&s_eiemModLock);
  const auto before = next->rules;
  affected->clear();
  for (const auto &event : events) {
    if (event.generation != generation) continue; // queued before an F10 reset
    std::vector<std::string> changed;
    if (event.directValues &&
        EiemModEquals(event.modPath.c_str(), selectedModPath.c_str())) {
      for (size_t i = 0; i < next->states.size(); ++i) {
        auto &state = next->states[i];
        if (!EiemModEquals(state.path.c_str(), event.modPath.c_str())) continue;
        const auto &controls = state.shapeControls;
        bool inRange = !event.values.empty();
        for (const auto &item : event.values) {
          auto control = std::find_if(controls.begin(), controls.end(),
              [&](const auto &candidate) { return candidate.variable == item.first; });
          inRange &= control != controls.end() && std::isfinite(item.second) &&
                     item.second >= control->minimum &&
                     item.second <= control->maximum;
        }
        const auto previous = state.variables;
        std::string error;
        if (!inRange || !EiemApplyUiValues(*next, i, event.values, error))
          Log("[UI] Rejected manager shape values for %s", state.path.c_str());
        else if (previous != state.variables) changed.push_back(state.path);
      }
      if (!changed.empty()) EiemEvaluateModProgram(*next);
    }
    else if (event.uiSection.empty() && !event.modPath.empty() &&
             EiemModEquals(event.modPath.c_str(), selectedModPath.c_str()))
      changed = EiemApplyModKey(*next, event.chord, event.uiFocus,
                                event.modPath.c_str(),
                                event.keySection.empty()
                                    ? nullptr
                                    : event.keySection.c_str(),
                                event.holdTick, event.holdSeconds);
    else {
      for (size_t i = 0; i < next->states.size(); ++i) {
        auto &state = next->states[i];
        if (!EiemModEquals(state.path.c_str(), event.modPath.c_str())) continue;
        for (const auto &ui : state.uis) {
          if (ui.section != event.uiSection) continue;
          const auto previous = state.variables;
          std::string error;
          if (!EiemApplyUiValues(*next, i, event.values, error))
            Log("[UI] Rejected transaction %s/%s: %s", state.path.c_str(), ui.section.c_str(), error.c_str());
          else if (previous != state.variables) changed.push_back(state.path);
        }
      }
      if (!changed.empty()) EiemEvaluateModProgram(*next);
    }
    for (const auto &path : changed)
      if (std::find(affected->begin(), affected->end(), path) == affected->end()) affected->push_back(path);
  }
  if (shapesOnly) {
    *shapesOnly = before.size() == next->rules.size();
    for (size_t i = 0; *shapesOnly && i < before.size(); ++i)
      *shapesOnly = EiemSameRenderAssembly(before[i], next->rules[i]);
  }
  if (submeshVisibilityOnly) {
    *submeshVisibilityOnly = before.size() == next->rules.size();
    for (size_t i = 0; *submeshVisibilityOnly && i < before.size(); ++i)
      *submeshVisibilityOnly =
          EiemSameRenderWithoutSubmeshVisibility(before[i], next->rules[i]);
  }
  if (visibilityChanges) {
    visibilityChanges->clear();
    if (before.size() == next->rules.size()) {
      for (size_t i = 0; i < before.size(); ++i) {
        if (before[i].hiddenSubmeshMask == next->rules[i].hiddenSubmeshMask)
          continue;
        EiemSubmeshVisibilityChange change;
        change.modPath = next->rules[i].modPath;
        change.section = next->rules[i].section;
        change.beforeMask = before[i].hiddenSubmeshMask;
        change.afterMask = next->rules[i].hiddenSubmeshMask;
        visibilityChanges->push_back(std::move(change));
      }
    }
  }
  return !affected->empty();
}

static void EiemPublishModState(EiemModProgram next) {
  s_eiemPersistentStates.Queue(next);
  AcquireSRWLockExclusive(&s_eiemModLock);
  s_eiemModProgram = std::move(next);
  EiemReconcileControlledModLocked();
  ReleaseSRWLockExclusive(&s_eiemModLock);
}

// Parse and validate a complete candidate before any live Renderer is changed.
// A malformed mod rejects the whole generation so existing instances never
// observe a partially loaded program.
static bool EiemPrepareModReload(EiemModProgram *prepared,
                                 std::string *failure = nullptr) {
  if (failure) failure->clear();
  if (!prepared) {
    if (failure) *failure = "Missing reload destination";
    return false;
  }
  EiemModProgram next;
  std::vector<std::string> files;
  WIN32_FIND_DATAW data = {};
  HANDLE root = FindFirstFileW(L"plugin\\mods\\*", &data);
  if (root != INVALID_HANDLE_VALUE) {
    do {
      if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || data.cFileName[0] == '.')
        continue;
      files.push_back((std::filesystem::path(L"plugin\\mods") /
                       data.cFileName / L"mod.ini").u8string());
    } while (FindNextFileW(root, &data));
    FindClose(root);
  }
  // File enumeration order is unspecified. Preserve deterministic conflict
  // precedence: sorted mod folders first, optional root mod.ini last.
  std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
    return _stricmp(a.c_str(), b.c_str()) < 0;
  });
  files.push_back("plugin\\mods\\mod.ini");
  for (const auto &file : files) {
    std::string error; EiemModProgram document;
    if (!EiemModParseFile(file.c_str(), document, &error)) {
      // A discovered folder does not have to contain a mod.ini. A present but
      // invalid file supplies an error and rejects the candidate generation.
      if (!error.empty()) {
        if (failure) *failure = file + ":" + error;
        return false;
      }
      continue;
    }
    EiemAppendModDocument(next,std::move(document));
  }
  s_eiemPersistentStates.Load(next);
  EiemCompileModProgram(next);
  *prepared = std::move(next);
  return true;
}

static LONG EiemPublishPreparedModReload(EiemModProgram next) {
  for (const auto &prefab : next.prefabs)
    Log("[MOD] prefab section=%s path=%s renders=%u", prefab.section, prefab.path, prefab.renderCount);
  for (const auto &rule : next.rules)
    Log("[MOD] rule section=%s path=%s asset=%s handling=%s mesh=%s",
        rule.section, rule.path, rule.asset, rule.handling[0] ? rule.handling : "<none>",
        rule.hasMesh ? rule.mesh : "<none>");
  const size_t prefabCount = next.prefabs.size(), ruleCount = next.rules.size();
  const size_t resourceCount = next.resources.size(), standaloneCount = next.standaloneRules.size();
  AcquireSRWLockExclusive(&s_eiemModLock);
  s_eiemModProgram = std::move(next);
  EiemReconcileControlledModLocked();
  const LONG generation = InterlockedIncrement(&s_eiemModGeneration);
  ReleaseSRWLockExclusive(&s_eiemModLock);
  Log("[MOD] Reloaded %zu Prefab declarations, %zu Render rules (%zu standalone), %zu resources, generation=%ld",
      prefabCount, ruleCount, standaloneCount, resourceCount, generation);
  return generation;
}

// Called before hooks at startup. Runtime F10 prepares its candidate before
// entering the Unity-side resource transaction.
static bool EiemReloadMods() {
  s_eiemPersistentStates.Flush(true);
  EiemModProgram next;
  std::string failure;
  if (!EiemPrepareModReload(&next, &failure)) {
    Log("[MOD] Reload rejected; current generation retained: %s",
        failure.empty() ? "unknown parse error" : failure.c_str());
    return false;
  }
  EiemPublishPreparedModReload(std::move(next));
  return true;
}

static void EiemFindModPrefabs(const char *path,
                               std::vector<EiemModPrefab> *out) {
  if (!out) return;
  out->clear();
  if (!path || !path[0]) return;
  AcquireSRWLockShared(&s_eiemModLock);
  for (const auto &prefab : s_eiemModProgram.prefabs) {
    if (EiemModSameLogicalPath(prefab.path, path)) out->push_back(prefab);
  }
  ReleaseSRWLockShared(&s_eiemModLock);
}

// Source Render rules match Mesh identity on every live consumer. A PFB
// reference is only a resource relationship and does not remove a rule from
// this set. Only Render sections used as partner templates are excluded.
static void EiemFindStandaloneRenderRules(std::vector<EiemModRule> *out) {
  if (!out) return;
  out->clear();
  AcquireSRWLockShared(&s_eiemModLock);
  for (size_t index : s_eiemModProgram.standaloneRules)
    out->push_back(s_eiemModProgram.rules[index]);
  ReleaseSRWLockShared(&s_eiemModLock);
}

static bool EiemHasStandaloneRenderRules() {
  AcquireSRWLockShared(&s_eiemModLock);
  const bool found = !s_eiemModProgram.standaloneRules.empty();
  ReleaseSRWLockShared(&s_eiemModLock);
  return found;
}

// Resource names in Render sections are section names, never file paths. Keep
// this lookup here so the Unity backend does not have to parse mod.ini again.
static bool EiemFindModResource(const char *modIni, const char *section,
                                const char *kind, EiemModResource *out) {
  if (!modIni || !section || !out) return false;
  bool found = false;
  AcquireSRWLockShared(&s_eiemModLock);
  for (const auto &resource : s_eiemModProgram.resources) {
    if (_stricmp(resource.modPath, modIni) != 0 ||
        _stricmp(resource.section, section) != 0)
      continue;
    if (kind && kind[0] && _stricmp(resource.kind, kind) != 0) continue;
    *out = resource;
    found = true;
    break;
  }
  ReleaseSRWLockShared(&s_eiemModLock);
  return found;
}

// Called only after a Render rule has won normal first-match precedence for a
// concrete Renderer under a registered model root.  Aliases of the same
// prepared file share a snapshot, so snapshot identity plus Mod identity is the
// stable deduplication key.  The resource name of the first hit is retained for
// diagnostics; it is not the runtime ownership key.
static bool EiemCollectPhysicsIntent(
    const EiemModRule &rule, std::vector<EiemPhysicsIntent> *out,
    void *matchedRenderer = nullptr) {
  if (!rule.hasPhysics) return true;
  // P0 freeze: Physics resources remain part of the parsed authoring
  // document, but the production Render path must not hand them to the
  // historical self-owned runtime adapter.  Returning success preserves the
  // winning Mesh/material/texture replacement.
  if (!kEiemEnableExperimentalPhysicsRuntime) return true;
  if (!out) return false;
  EiemModResource resource = {};
  if (!EiemFindModResource(rule.modPath, rule.physics, "Physics", &resource) ||
      !resource.physicsAsset)
    return false;
  for (auto &intent : *out) {
    if (!EiemModEquals(intent.modPath, rule.modPath) ||
        intent.asset.get() != resource.physicsAsset.get())
      continue;
    ++intent.rendererMatches;
    if (matchedRenderer &&
        std::find(intent.matchedRenderers.begin(), intent.matchedRenderers.end(),
                  matchedRenderer) == intent.matchedRenderers.end())
      intent.matchedRenderers.push_back(matchedRenderer);
    return true;
  }
  EiemPhysicsIntent intent = {};
  strncpy_s(intent.modPath, sizeof(intent.modPath), rule.modPath, _TRUNCATE);
  strncpy_s(intent.resourceSection, sizeof(intent.resourceSection),
            rule.physics, _TRUNCATE);
  strncpy_s(intent.firstRenderSection, sizeof(intent.firstRenderSection),
            rule.section, _TRUNCATE);
  intent.firstRule = rule;
  intent.asset = std::move(resource.physicsAsset);
  if (matchedRenderer) intent.matchedRenderers.push_back(matchedRenderer);
  intent.rendererMatches = 1;
  out->push_back(std::move(intent));
  return true;
}

static bool EiemFindRenderRuleBySection(const char *modIni, const char *section,
                                        EiemModRule *out) {
  if (!modIni || !section || !section[0] || !out) return false;
  bool found = false;
  AcquireSRWLockShared(&s_eiemModLock);
  for (auto it = s_eiemModProgram.rules.rbegin(); it != s_eiemModProgram.rules.rend(); ++it) {
    if (_stricmp(it->modPath, modIni) == 0 &&
        _stricmp(it->section, section) == 0) {
      *out = *it;
      found = true;
      break;
    }
  }
  ReleaseSRWLockShared(&s_eiemModLock);
  return found;
}

// Conditional partner links describe visibility, not component lifetime.
// Return every partner template referenced by the source Render across all
// branches so the Unity adapter can construct each Renderer while the model is
// still being assembled. The currently evaluated source rule decides which of
// these prebuilt Renderers is visible.
