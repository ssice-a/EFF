from runtime_source import read_runtime_source
"""Executable tests of global config, state publication and instance registration."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        if source[end] == "{": depth += 1
        elif source[end] == "}": depth -= 1
        end += 1
    return source[start:end]


PREFIX = r'''
#include <windows.h>
#include <sstream>
#include <cstdio>
#include <vector>
static int g_guiToggleVK = VK_INSERT, g_modReloadVK = VK_F10;
static void Log(const char *, ...) {}
#include "eiem_config.h"
#include "eiem_mods.h"
#include "eiem_render_state.h"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)
enum class EiemModelOwnerKind { PrefabProxy, UIModelLoader, BaseModelPart, CharUIModel, NpcAvatar };
struct EiemModelOwnerRef { EiemModelOwnerKind kind; void *owner; bool active=true; };
struct EiemUnityRef {
  void *p=nullptr;
  static EiemUnityRef Capture(void *p,bool=true) { return {p}; }
  explicit operator bool() const { return p!=nullptr; }
  void *Target() const { return p; }
  int Status() const { return p?1:0; }
};
struct EiemModelInstanceState {
  EiemUnityRef modelRef;
  void *model = nullptr; uint32_t instanceUid = 0; char path[768] = {};
  std::vector<EiemModelOwnerRef> owners;
  std::vector<EiemPhysicsIntent> physicsIntents;
};
static std::vector<EiemModelInstanceState> s_eiemModelInstances;
static SRWLOCK s_eiemModelInstanceLock = SRWLOCK_INIT;
static bool simulateMatch = true;
static bool EiemApplyStandaloneRenderRules(
    void *, const char *, bool *matched=nullptr,
    const std::vector<std::string> * = nullptr,
    std::vector<EiemPhysicsIntent> * = nullptr,
    bool = true) {
  if (matched) *matched = simulateMatch;
  return false; // selector matches, but the conditional body is OFF
}
static void EiemStoreModelPhysicsIntents(
    void *, std::vector<EiemPhysicsIntent>, const char *) {}
static void EiemForgetRenderOverrides(uintptr_t) {}
static int observedModels = 0;
static void EiemProbeObserveModel(void *, const char *) { ++observedModels; }
static const char *EiemModelOwnerKindName(EiemModelOwnerKind kind) {
  switch (kind) {
    case EiemModelOwnerKind::PrefabProxy: return "PrefabProxy";
    case EiemModelOwnerKind::UIModelLoader: return "UIModelLoader";
    case EiemModelOwnerKind::BaseModelPart: return "BaseModelPart";
    case EiemModelOwnerKind::CharUIModel: return "CharUIModel";
    case EiemModelOwnerKind::NpcAvatar: return "NpcAvatar";
  }
  return "Unknown";
}
static void EiemReleaseModelPhysics(void *, const char *) {}
static bool EiemModelHasActiveOwner(const EiemModelInstanceState &state) {
  for (size_t index = 0; index < state.owners.size(); ++index)
    if (state.owners[index].active) return true;
  return false;
}
static void EiemRegistrationTraceModel(
    const char *, void *, void *, const char *, LONG, size_t, bool, bool,
    int, int, const char *) {}
static void EiemRegistrationTraceRelease(
    const char *, void *, void *, const char *, LONG) {}
static void EiemRegistrationTraceReconcile(
    const char *, uint32_t, LONG, size_t, size_t, uint32_t, ULONGLONG) {}
struct EiemPerfCounter {};
struct EiemPerfScope { explicit EiemPerfScope(EiemPerfCounter &) {} };
static EiemPerfCounter s_eiemPerfModelRegistration;
'''

MAIN = r'''
int main(int argc, char **argv) {
  CHECK(argc == 2);
  std::string scenario = argv[1], error;
  if (scenario == "empty_rules_observed") {
    CHECK(s_eiemModelInstances.empty());
    CHECK(!EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart, (void *)1, (void *)2, nullptr, 0, "empty-rules", true));
    CHECK(observedModels == 0 && s_eiemModelInstances.size() == 1);
  } else if (scenario == "global") {
    EiemGlobalConfig config;
    std::istringstream valid("\xEF\xBB\xBF[Hotkeys]\r\nreload=Ctrl+F8\r\ngui=INSERT\r\n");
    CHECK(EiemParseGlobalConfig(valid, &config, error));
    CHECK(config.reload.vk == VK_F8 && config.reload.modifiers == MOD_CONTROL);
    for (const auto &text : {"[Hotkeys]\nreload=bogus", "[Hotkeys]\nreload=INSERT", "[Hotkeys]\nreload=F8\nreload=F9", "reload=F7"}) {
      std::istringstream invalid(text);
      CHECK(!EiemParseGlobalConfig(invalid, &config, error));
      CHECK(config.reload.vk == VK_F8); // failed edits do not remove the working key
    }
    CHECK(std::string(kEiemGlobalConfigPath) == "plugin\\eiem.ini");
    CHECK(LoadEiemConfig()); // create default, not legacy-file search
    CHECK(EiemGetGlobalConfig().reload.vk == VK_F10);
    { std::ofstream file(kEiemGlobalConfigPath); file << "[Hotkeys]\nreload=F8\n"; }
    CHECK(LoadEiemConfig());
    CHECK(EiemGetGlobalConfig().reload.vk == VK_F8);
    CHECK(s_eiemGlobalConfigGeneration == 2);
    { std::ofstream file(kEiemGlobalConfigPath); file << "[Hotkeys]\nreload=invalid\n"; }
    CHECK(!LoadEiemConfig());
    CHECK(EiemGetGlobalConfig().reload.vk == VK_F8 && s_eiemGlobalConfigGeneration == 2);
  } else if (scenario == "slots") {
    CHECK((EiemRestoreOwnedSlots<int>({9,8}, {1,2}, {0}) == std::vector<int>{1,8}));
    CHECK((EiemRestoreOwnedSlots<int>({1,9}, {1}, {1}) == std::vector<int>{1}));
    CHECK((EiemRestoreOwnedSlots<int>({9,8,7}, {1,2}, {0}) == std::vector<int>{1,8,7}));
    CHECK((EiemRestoreOwnedSlots<int>({1,9,7}, {1}, {1}) == std::vector<int>{1,0,7}));
    CHECK((EiemRestoreOwnedSlots<int>({1,0,0,9}, {1}, {1,2,3}) == std::vector<int>{1}));
    CHECK((EiemRestoreOwnedSlots<int>({9,8}, {1,2}, {}) == std::vector<int>{9,8}));
  } else if (scenario == "events") {
    EiemModProgram program;
    std::istringstream input("[Constants]\n$a=0\n[KeyA]\nkey=F6\ntype=cycle\n$a=0,1,2\n"
      "[RenderMain]\nasset=Body\nif $a == 1\nhandling=skip\nendif\n");
    CHECK(EiemModParseStream(input, "a/mod.ini", program, &error));
    EiemPublishModState(program); s_eiemModGeneration = 7;
    CHECK(EiemGetSelectedModPath() == "a/mod.ini");
    CHECK(EiemSelectControlledMod("a/mod.ini"));
    EiemModProgram next; std::vector<std::string> affected;
    CHECK(!EiemPrepareInputUpdate({{{VK_F6,0},6,"a/mod.ini"}}, &next, &affected));
    CHECK(EiemPrepareInputUpdate({{{VK_F6,0},7,"a/mod.ini"},
                                  {{VK_F6,0},7,"a/mod.ini"}},
                                 &next, &affected));
    CHECK(next.states[0].variables.at("$a") == 2);
    CHECK(s_eiemModProgram.states[0].variables.at("$a") == 0); // not published until restore
    EiemPublishModState(next);
    CHECK(s_eiemModProgram.states[0].variables.at("$a") == 2 && s_eiemModGeneration == 7);
    CHECK(EiemPrepareInputUpdate({{{VK_F6,0},7,"a/mod.ini"}}, &next, &affected));
    CHECK(next.states[0].variables.at("$a") == 0 && !next.rules[0].handling[0]);
    EiemPublishModState(program); // F10 replacement resets to parsed defaults
    CHECK(s_eiemModProgram.states[0].variables.at("$a") == 0);
  } else if (scenario == "selected_mod_scope") {
    EiemModProgram first, second, program;
    std::istringstream inputA(
      "[Constants]\n$value=0\n[KeyMode]\nkey=F6\ntype=cycle\n$value=0,1\n"
      "[RenderA]\nasset=BodyA\nif $value == 1\nhandling=skip\nendif\n");
    std::istringstream inputB(
      "[Constants]\n$value=0\n[KeyMode]\nkey=F6\ntype=cycle\n$value=0,1\n"
      "[RenderB]\nasset=BodyB\nif $value == 1\nhandling=skip\nendif\n");
    CHECK(EiemModParseStream(inputA, "mods/a/mod.ini", first, &error));
    CHECK(EiemModParseStream(inputB, "mods/b/mod.ini", second, &error));
    EiemAppendModDocument(program, std::move(first));
    EiemAppendModDocument(program, std::move(second));
    EiemPublishModState(program); s_eiemModGeneration = 11;
    CHECK(EiemSelectControlledMod("mods/b/mod.ini"));
    LONG generation = -1, controlGeneration = -1;
    std::string selected;
    auto chords = EiemGetModKeyChords(&generation, &controlGeneration,
                                      &selected, false);
    CHECK(generation == 11 && controlGeneration > 0);
    CHECK(selected == "mods/b/mod.ini");
    CHECK(chords.size() == 1 && chords[0].vk == VK_F6);
    EiemModProgram next; std::vector<std::string> affected;
    EiemModInputEvent event{{VK_F6,0},11};
    event.modPath = selected;
    CHECK(EiemPrepareInputUpdate({event}, &next, &affected));
    CHECK(next.states[0].variables.at("$value") == 0);
    CHECK(next.states[1].variables.at("$value") == 1);
    CHECK(affected.size() == 1 && affected[0] == "mods/b/mod.ini");
    auto controls = EiemGetModControls(&generation, &controlGeneration);
    CHECK(controls.size() == 2 && !controls[0].selected && controls[1].selected);
    CHECK(controls[1].keys.size() == 1 && controls[1].keys[0].section == "KeyMode");
  } else if (scenario == "selection_survives_reload") {
    EiemModProgram first, second, both;
    std::istringstream inputA("[Constants]\n$a=0\n[KeyA]\nkey=F6\ntype=cycle\n$a=0,1\n");
    std::istringstream inputB("[Constants]\n$b=0\n[KeyB]\nkey=F7\ntype=cycle\n$b=0,1\n");
    CHECK(EiemModParseStream(inputA, "mods/a/mod.ini", first, &error));
    CHECK(EiemModParseStream(inputB, "mods/b/mod.ini", second, &error));
    EiemModProgram onlyA = first;
    EiemAppendModDocument(both, std::move(first));
    EiemAppendModDocument(both, std::move(second));
    EiemPublishPreparedModReload(both);
    CHECK(EiemGetSelectedModPath() == "mods/a/mod.ini");
    CHECK(EiemSelectControlledMod("mods/b/mod.ini"));
    EiemPublishPreparedModReload(both);
    CHECK(EiemGetSelectedModPath() == "mods/b/mod.ini");
    EiemPublishPreparedModReload(std::move(onlyA));
    CHECK(EiemGetSelectedModPath() == "mods/a/mod.ini"); // deleted selection falls back
  } else if (scenario == "default_prefers_switchable_mod") {
    EiemModProgram shapes, keys, combined;
    std::istringstream shapeIni(
      "[Constants]\n$shape=0\n[RenderShape]\nasset=Body\nshape.Inflate=$shape\n");
    std::istringstream keyIni(
      "[Constants]\n$state=0\n[KeySwitch]\nkey=F7\ntype=cycle\n$state=0,1\n");
    CHECK(EiemModParseStream(shapeIni, "mods/a-shapes/mod.ini", shapes, &error));
    CHECK(EiemModParseStream(keyIni, "mods/b-switch/mod.ini", keys, &error));
    EiemAppendModDocument(combined, std::move(shapes));
    EiemAppendModDocument(combined, std::move(keys));
    EiemPublishPreparedModReload(combined);
    CHECK(EiemGetSelectedModPath() == "mods/b-switch/mod.ini");
    CHECK(!EiemSelectControlledMod(""));
    CHECK(EiemGetSelectedModPath() == "mods/b-switch/mod.ini");
    CHECK(!EiemModUsesUiKeyScope(true,false,false));
    CHECK(EiemModUsesUiKeyScope(false,true,false));
    CHECK(!EiemModUsesUiKeyScope(false,true,true));
    CHECK(EiemModUsesUiKeyScope(false,false,true));
    CHECK(EiemSelectControlledMod("mods/a-shapes/mod.ini"));
    EiemPublishPreparedModReload(combined);
    CHECK(EiemGetSelectedModPath() == "mods/a-shapes/mod.ini");
  } else if (scenario == "manager_key_section") {
    EiemModProgram program;
    std::istringstream input(
      "[Constants]\n$a=0\n$b=0\n"
      "[KeyA]\nkey=F6\ntype=cycle\n$a=0,1\n"
      "[KeyB]\nkey=F6\ntype=cycle\n$b=0,1\n");
    CHECK(EiemModParseStream(input, "mods/a/mod.ini", program, &error));
    EiemPublishModState(program); s_eiemModGeneration = 12;
    CHECK(EiemSelectControlledMod("mods/a/mod.ini"));
    EiemModInputEvent event{{VK_F6,0},12,"mods/a/mod.ini"};
    event.keySection = "KeyB";
    EiemModProgram next; std::vector<std::string> affected;
    CHECK(EiemPrepareInputUpdate({event}, &next, &affected));
    CHECK(next.states[0].variables.at("$a") == 0);
    CHECK(next.states[0].variables.at("$b") == 1);
  } else if (scenario == "submesh_visibility_only") {
    EiemModProgram program;
    std::istringstream input(
      "[Constants]\n$show=0\n"
      "[KeyA]\nkey=F6\ntype=cycle\n$show=0,1\n"
      "[RenderMain]\nasset=Body\nmesh=MeshBody\n"
      "if $show == 0\nsubmesh_visible.2=true\n"
      "else\nsubmesh_visible.2=false\nendif\n"
      "[MeshBody]\npath=meshes/body.mesh\n");
    CHECK(EiemModParseStream(input, "a/mod.ini", program, &error));
    EiemPublishModState(program); s_eiemModGeneration = 10;
    CHECK(EiemSelectControlledMod("a/mod.ini"));
    CHECK(program.rules.size() == 1 && program.rules[0].hiddenSubmeshMask == 0);
    EiemModProgram next; std::vector<std::string> affected;
    bool shapesOnly = false;
    bool submeshVisibilityOnly = false;
    std::vector<EiemSubmeshVisibilityChange> visibilityChanges;
    CHECK(EiemPrepareInputUpdate({{{VK_F6,0},10,"a/mod.ini"}}, &next, &affected,
                                 &shapesOnly, &submeshVisibilityOnly,
                                 &visibilityChanges));
    CHECK(!shapesOnly);
    CHECK(submeshVisibilityOnly);
    CHECK(next.rules.size() == 1 && next.rules[0].hiddenSubmeshMask == (1u << 2));
    CHECK(affected.size() == 1 && affected[0] == "a/mod.ini");
    CHECK(visibilityChanges.size() == 1);
    CHECK(visibilityChanges[0].modPath == "a/mod.ini");
    CHECK(visibilityChanges[0].section == "RenderMain");
    CHECK(visibilityChanges[0].beforeMask == 0);
    CHECK(visibilityChanges[0].afterMask == (1u << 2));
  } else if (scenario == "default_off_instances") {
    EiemModProgram program;
    std::istringstream input("[RenderMain]\nasset=Body\n");
    CHECK(EiemModParseStream(input, "a/mod.ini", program, &error));
    EiemPublishModState(program);
    void *ownerA = (void *)1, *ownerB = (void *)2, *modelA = (void *)11, *modelB = (void *)12;
    CHECK(!EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart, ownerA, modelA, nullptr, 0, "test", true));
    CHECK(s_eiemModelInstances.size() == 1); // no mutation is still a tracked target
    EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart, ownerB, modelB, nullptr, 0, "test", true);
    CHECK(s_eiemModelInstances.size() == 2);
    EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart, ownerA, modelA, nullptr, 0, "test", true);
    CHECK(s_eiemModelInstances.size() == 2 && s_eiemModelInstances[0].owners.size() == 1);
    EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart, ownerA, (void *)13, nullptr, 0, "test", true);
    CHECK(s_eiemModelInstances.size() == 2); // recycled owner releases previous instance
    simulateMatch = false;
    EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart, (void *)3, (void *)14, nullptr, 0, "test", true);
    CHECK(s_eiemModelInstances.size() == 3); // future rules may target this observed instance
  } else return 2;
  return 0;
}
'''


class ModControlsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("cl"):
            raise unittest.SkipTest("Requires MSVC developer environment")
        cls.temp = tempfile.TemporaryDirectory(prefix="eiem-controls-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        trace = read_runtime_source(ROOT)
        signature = "static bool EiemRegisterAndApplyModelInstance("
        # Skip forward declaration; compile the actual runtime implementation.
        implementation = function(trace[trace.rindex(signature):], signature)
        source = cls.folder / "controls.cpp"
        source.write_text(PREFIX + implementation + MAIN, encoding="utf-8")
        cls.exe = cls.folder / "controls.exe"
        result = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/utf-8",
                                f"/I{ROOT / 'src'}", str(source), f"/Fe{cls.exe}"],
                                cwd=cls.folder, capture_output=True, text=True,
                                encoding="utf-8",
                                errors="replace")
        if result.returncode: raise AssertionError(result.stdout + result.stderr)

    def run_case(self, name):
        result = subprocess.run([str(self.exe), name], cwd=self.folder,
                                capture_output=True, text=True, errors="replace")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_global_config_default_edit_and_invalid_transaction(self): self.run_case("global")
    def test_only_owned_slots_restore_and_only_owned_tail_removed(self): self.run_case("slots")
    def test_press_order_stale_generation_and_publish_after_restore(self): self.run_case("events")
    def test_key_events_only_change_the_selected_mod(self): self.run_case("selected_mod_scope")
    def test_reload_preserves_or_falls_back_selected_mod(self): self.run_case("selection_survives_reload")
    def test_default_prefers_switchable_mod_and_manager_focus_is_game_scope(self): self.run_case("default_prefers_switchable_mod")
    def test_manager_button_targets_one_key_section(self): self.run_case("manager_key_section")
    def test_submesh_visibility_is_a_lightweight_mesh_update(self): self.run_case("submesh_visibility_only")
    def test_actual_runtime_registers_default_off_multi_instances(self): self.run_case("default_off_instances")
    def test_empty_program_still_registers_instances_without_diagnostic_probe(self): self.run_case("empty_rules_observed")


if __name__ == "__main__": unittest.main()
