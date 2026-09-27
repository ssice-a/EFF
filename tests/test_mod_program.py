"""Native tests of the actual INI compiler and main-thread update protocol."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#include <sstream>
#include <string>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include "eiem_mod_document.h"
#include "eiem_mod_update.h"
static void Log(const char *, ...) {}
#include "eiem_mods.h"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)
static bool parse(const std::string &text, EiemModProgram &p, std::string &error,
                  const char *file = "test/mod.ini") {
  std::istringstream stream(text);
  return EiemModParseStream(stream, file, p, &error);
}
int main(int argc, char **argv) {
  CHECK(argc >= 2);
  std::string scenario = argv[1], error;
  EiemModProgram p;
  if (scenario == "file") {
    CHECK(argc == 3);
    if (!EiemModParseFile(argv[2], p, &error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    EiemCompileModProgram(p);
    std::printf("rules=%zu resources=%zu standalone=%zu\n", p.rules.size(), p.resources.size(), p.standaloneRules.size());
  } else if (scenario == "semantics") {
    CHECK(parse("\xEF\xBB\xBF[RenderBody]\r\nasset=Body\r\nhandling=skip\r\n; mesh=MeshNew\r\n"
                "material.0=MaterialOld\nmaterial.0=MaterialNew\n[MeshNew]\npath=body.mesh\ntarget.asset=Body\n"
                "[MaterialOld]\npath=old.mat\n[MaterialNew]\npath=new.mat\n", p, error));
    CHECK(p.rules.size() == 1 && p.resources.size() == 3);
    CHECK(!p.rules[0].hasMesh && EiemModEquals(p.rules[0].handling, "skip"));
    CHECK(p.rules[0].materialCount == 1 && EiemModEquals(p.rules[0].materials[0], "MaterialNew"));
    EiemModProgram mesh;
    CHECK(parse("[RenderBody]\nasset=Body\nmesh=MeshNew\n; handling=skip\n[MeshNew]\npath=new.mesh\n", mesh, error));
    CHECK(mesh.rules[0].hasMesh && !mesh.rules[0].handling[0]);
    CHECK(!parse("[RenderAdd]\npartner.0=RenderExtra\n[RenderExtra]\n", mesh, error));
    CHECK(!error.empty());
    EiemModProgram visibility;
    CHECK(parse("[RenderBody]\nasset=Body\nsubmesh_visible.2=false\n", visibility, error));
    CHECK(visibility.rules[0].hiddenSubmeshMask == (1u << 2));
    CHECK(parse("[RenderBody]\nasset=Body\nsubmesh_visible.2=true\n", visibility, error));
    CHECK(visibility.rules[1].hiddenSubmeshMask == 0);
  } else if (scenario == "invalid") {
    const char *bad[] = {"match.vertices=12garbage", "match.indices=-1", "match.submeshes=99999999999999999",
      "material.0tail=M", "partner.-1=R", "submesh.1x=0", "submesh.0=no", "submesh_visible.32=false", "submesh_visible.0=maybe", "handling=clone",
      "if $toggle", "if $toggle == 1", "if($toggle==1)", "else", "endif"};
    CHECK(parse("[RenderExisting]\nasset=Existing\n", p, error));
    for (const char *line : bad) {
      CHECK(!parse(std::string("[RenderValidFirst]\nasset=Valid\n[RenderBad]\nasset=Bad\n") + line, p, error));
      CHECK(p.rules.size() == 1 && !error.empty()); // no partial file published
    }
    CHECK(!parse("[RenderDuplicate]\nasset=A\n[renderduplicate]\nasset=B", p, error));
    CHECK(p.rules.size() == 1);
    CHECK(!parse("[TextureBad]\npath=x.png\nlinear=perhaps", p, error));
    CHECK(!parse("[TextureBad]\npath=x.png\nmip_bias=nan", p, error));
  } else if (scenario == "compile") {
    CHECK(parse("[PrefabA]\npath=actors/A.prefab\nrender.0=RenderScoped\n"
                "[RenderScoped]\nasset=Body\n[RenderMain]\nasset=Body\n"
                "[RenderUnusedTemplate]\nmesh=MeshBody\n"
                "[MeshBody]\npath=body.mesh\ntarget.asset=Body\n", p, error));
    EiemCompileModProgram(p);
    CHECK(p.standaloneRules.size() == 2);
    CHECK(EiemModEquals(p.rules[p.standaloneRules[0]].section, "RenderScoped"));
    CHECK(EiemModEquals(p.rules[p.standaloneRules[1]].section, "RenderMain"));
    // A PFB reference organizes related declarations; it does not scope Mesh
    // consumers. Same identifiers in another mod remain independent too.
    CHECK(parse("[RenderScoped]\nasset=Other\n", p, error, "other/mod.ini"));
    EiemCompileModProgram(p);
    CHECK(p.standaloneRules.size() == 3);
    EiemCompileModProgram(p);
    CHECK(p.standaloneRules.size() == 3); // recompilation is idempotent
  } else if (scenario == "reload") {
    EiemModUpdateQueue queue;
    std::string order;
    int config = 1, applied = 1;
    // First lifecycle reconcile must not discard effects already applied by startup hooks.
    CHECK(queue.Request(EiemModUpdate::Reconcile));
    EiemDispatchModUpdate(queue.Take(), [&] { order += 'S'; applied = 0; return true; },
                         [&] { order += 'L'; ++config; }, [&] { order += 'A'; applied = config; });
    CHECK(order == "A" && applied == 1);
    order.clear();
    CHECK(queue.Request(EiemModUpdate::Reconcile));
    CHECK(!queue.Request(EiemModUpdate::Reload)); // must merge, not drop F10
    CHECK(!queue.Request(EiemModUpdate::Reapply));
    bool sawOldConfigDuringRestore = false, restoredBeforePublish = false;
    EiemDispatchModUpdate(queue.Take(), [&] { order += 'S'; sawOldConfigDuringRestore = config == 1; applied = 0; return true; },
                         [&] { order += 'L'; restoredBeforePublish = applied == 0; ++config; },
                         [&] { order += 'A'; applied = config; });
    CHECK(order == "SLA" && applied == 2 && sawOldConfigDuringRestore && restoredBeforePublish);
    order.clear();
    CHECK(queue.Request(EiemModUpdate::Reapply));
    EiemDispatchModUpdate(queue.Take(), [&] { order += 'S'; return true; }, [&] { order += 'L'; }, [&] {
      order += 'A';
    });
    CHECK(order == "SA"); // future state changes do not reparse disk
    order.clear();
    const bool restoreAccepted = EiemDispatchModUpdate(
        (uint32_t)EiemModUpdate::Reload,
        [&] { order += 'S'; return false; },
        [&] { order += 'L'; ++config; },
        [&] { order += 'A'; applied = config; });
    CHECK(!restoreAccepted && order == "SA" && config == 2 && applied == 2);
    CHECK(queue.Take() == 0);
    CHECK(queue.Request(EiemModUpdate::Reload)); // requests after drain are schedulable
    const uint32_t retry = queue.Take();
    CHECK(retry == (uint32_t)EiemModUpdate::Reload);
    queue.Requeue(retry); // a failed post/API readiness must not lose F10
    CHECK(queue.HasPending());
    CHECK(queue.Take() == retry);
  } else if (scenario == "publish") {
    EiemReloadMods();
    std::vector<EiemModRule> rules;
    EiemFindStandaloneRenderRules(&rules);
    CHECK(rules.size() == 2);
    CHECK(EiemModEquals(rules[0].asset, "A") && EiemModEquals(rules[1].asset, "Z"));
  } else if (scenario == "independent_invalid") {
    namespace fs = std::filesystem;
    fs::create_directories("plugin/mods/bad");
    fs::create_directories("plugin/mods/good");
    std::ofstream bad("plugin/mods/bad/mod.ini");
    bad << "[RenderBad]\nasset=Bad\nunknown_field=1\n";
    std::ofstream good("plugin/mods/good/mod.ini");
    good << "[RenderGood]\nasset=Good\n";
    bad.close();
    good.close();
    CHECK(EiemReloadMods());
    std::vector<EiemModRule> rules;
    EiemFindStandaloneRenderRules(&rules);
    bool sawGood = false, sawBad = false;
    for (const auto &rule : rules) {
      sawGood = sawGood || EiemModEquals(rule.asset, "Good");
      sawBad = sawBad || EiemModEquals(rule.asset, "Bad");
    }
    CHECK(sawGood && !sawBad);
  } else if (scenario == "conditions") {
    CHECK(parse("[Constants]\n$outfit=0\n$detail=1\n"
      "[KeyOutfit]\nkey=Ctrl+F6\ntype=cycle\n$outfit=0,1,2\n"
      "[MeshNew]\npath=new.mesh\n[MaterialNew]\npath=new.mat\n"
      "[RenderMain]\nasset=Body\nif $outfit == 1\nmesh=MeshNew\n"
      "if $detail && !(false || $outfit < 1)\nmaterial.0=MaterialNew\nendif\n"
      "else if $outfit == 2\nhandling=skip\nelse\nmesh=\nendif\n", p, error));
    CHECK(p.standaloneRules.size() == 1); // default-off still has a selector
    CHECK(!p.rules[0].hasMesh && !p.rules[0].handling[0]);
    EiemKeyChord chord; CHECK(EiemParseKeyChord("Ctrl+F6", &chord));
    CHECK(EiemCycleModKey(p, chord).size() == 1);
    CHECK(p.rules[0].hasMesh && p.rules[0].materialCount == 1 && !p.rules[0].handling[0]);
    EiemCycleModKey(p, chord);
    CHECK(!p.rules[0].hasMesh && p.rules[0].materialCount == 0 && EiemModEquals(p.rules[0].handling, "skip"));
    EiemCycleModKey(p, chord);
    CHECK(!p.rules[0].hasMesh && p.rules[0].materialCount == 0 && !p.rules[0].handling[0]);
    // Two fast presses are two transitions, not one pending-bit toggle.
    EiemCycleModKey(p, chord); EiemCycleModKey(p, chord);
    CHECK(p.states[0].variables.at("$outfit") == 2);
  } else if (scenario == "expressions") {
    EiemVariables vars{{"$a", 2}, {"$b", -1}};
    for (const auto &s : {"1 || 0 && 0", "$a >= 2 && $b < 0", "!false && ($a != $b)", "-1 == $b", "+2 == $a"}) {
      auto expr = EiemExpressionParser(s, error).Parse();
      CHECK(expr && expr->Validate(vars, error) && expr->Evaluate(vars));
    }
    for (const auto &s : {"1 = 1", "$a &&", "(1", "1)tail", "NaN", "1 + 2", "1 & 1"})
      CHECK(!EiemExpressionParser(s, error).Parse());
    auto expr = EiemExpressionParser("true || $missing", error).Parse();
    CHECK(expr && !expr->Validate(vars, error)); // check even inactive references
    CHECK(expr->Evaluate(vars) == 1); // short circuit does not read missing variable
  } else if (scenario == "condition_errors") {
    const char *bodies[] = {"if true\nmesh=Missing\nendif", "if false\nmesh=Missing\nendif",
      "if $missing\nendif", "if true\nasset=Other\nendif", "else", "endif", "if true",
      "if true\nelse\nelse\nendif", "if true\nendif garbage", "if true\n[RenderOther]",
      "if true\nelse wrong\nendif", "if false\nmaterial.0=Missing\nendif"};
    for (const char *body : bodies) {
      CHECK(!parse(std::string("[RenderMain]\nasset=Body\n") + body, p, error));
      CHECK(p.rules.empty() && p.definitions.empty() && p.states.empty());
      CHECK(!error.empty());
    }
    CHECK(!parse("[Constants]\n$a=0\n[KeyA]\nkey=F6\ntype=cycle\n$a=0\n", p, error));
    CHECK(!parse("[Constants]\n$a=0\n[KeyA]\nkey=F6\n$a=0,1\n", p, error));
    CHECK(!parse("[Constants]\n$a=0\n[KeyA]\nkey=F6\ntype=hold\n$a=0,1\n", p, error));
    CHECK(!parse("[Constants]\n$a=0\n[KeyA]\nkey=F6\ntype=hold\n$a=1\n", p, error));
    CHECK(parse("[Constants]\n$a=0\n[KeyA]\nkey=F6\ntype=hold\nspeed=2\n$a=1\n", p, error));
    CHECK(!parse("[Constants]\n$a=0\n[KeyA]\nkey=F6\ntype=cycle\n$b=0,1\n", p, error));
  } else if (scenario == "ordered_and_scope") {
    const std::string text = "[Constants]\n$a=0\n$b=0\n[KeyA]\nkey=F6\ntype=cycle\n$a=0,1\n$b=0,2\n"
      "[MeshNew]\npath=new.mesh\n[MaterialNew]\npath=new.mat\n[RenderMain]\nasset=Body\n"
      "material.0=MaterialNew\nif $a\nmaterial.0=\nendif\n"
      "if $b == 2\nmesh=MeshNew\nendif\n";
    CHECK(parse(text, p, error, "a/mod.ini"));
    CHECK(parse(text, p, error, "b/mod.ini"));
    CHECK(p.standaloneRules.size() == 2);
    p.states[1].keys.clear();
    EiemKeyChord chord; CHECK(EiemParseKeyChord("F6", &chord));
    auto changed = EiemCycleModKey(p, chord);
    CHECK(changed.size() == 1 && changed[0] == "a/mod.ini");
    CHECK(p.rules[0].hasMesh && p.rules[0].materialCount == 0);
    CHECK(!p.rules[1].hasMesh && p.rules[1].materialCount == 1);
    EiemCycleModKey(p, chord);
    CHECK(!p.rules[0].hasMesh && p.rules[0].materialCount == 1);
  } else if (scenario == "hold") {
    CHECK(parse("[Constants]\n$shape=0\n[KeyShape]\nkey=F6\ntype=hold\n"
                "speed=2\n$shape=1\n[RenderShape]\nasset=Body\n"
                "shape.Inflate=$shape\n", p, error));
    EiemKeyChord chord; CHECK(EiemParseKeyChord("F6", &chord));
    CHECK(EiemApplyModKey(p, chord, false, nullptr, nullptr, true, .25).size() == 1);
    CHECK(std::abs(p.states[0].variables.at("$shape") - .5) < 1e-9);
    CHECK(EiemApplyModKey(p, chord, false, nullptr, nullptr, true, .5).size() == 1);
    CHECK(std::abs(p.states[0].variables.at("$shape") - 1.0) < 1e-9);
    // A hold tick must never advance a cycle key sharing the same chord.
    EiemModProgram cycle;
    CHECK(parse("[Constants]\n$x=0\n[KeyCycle]\nkey=F7\ntype=cycle\n$x=0,1\n", cycle, error));
    EiemKeyChord cycleChord; CHECK(EiemParseKeyChord("F7", &cycleChord));
    EiemApplyModKey(cycle, cycleChord, false, nullptr, nullptr, true, .25);
    CHECK(cycle.states[0].variables.at("$x") == 0);
  } else if (scenario == "chords") {
    EiemKeyChord chord;
    CHECK(EiemParseKeyChord("Ctrl+Shift+F12", &chord));
    CHECK(chord.vk == VK_F12 && chord.modifiers == (MOD_CONTROL | MOD_SHIFT));
    CHECK(EiemParseKeyChord("Ctrl+Alt+Numpad7", &chord));
    CHECK(chord.vk == VK_NUMPAD7 && chord.modifiers == (MOD_CONTROL | MOD_ALT));
    CHECK(EiemParseKeyChord("Shift+NumpadPlus", &chord));
    CHECK(chord.vk == VK_ADD && chord.modifiers == MOD_SHIFT);
    CHECK(EiemParseKeyChord("NumpadMinus", &chord) && chord.vk == VK_SUBTRACT);
    CHECK(EiemParseKeyChord("NumpadMultiply", &chord) && chord.vk == VK_MULTIPLY);
    CHECK(EiemParseKeyChord("NumpadDivide", &chord) && chord.vk == VK_DIVIDE);
    CHECK(EiemParseKeyChord("NumpadDecimal", &chord) && chord.vk == VK_DECIMAL);
    for (const auto &s : {"F25", "F10garbage", "Ctrl", "Ctrl+Ctrl+F6", "F6+F7", "F6+", "bogus"})
      CHECK(!EiemParseKeyChord(s, &chord));
  } else return 2;
  return 0;
}
'''


class ModProgramTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("cl"):
            raise unittest.SkipTest("Requires MSVC developer environment (cl)")
        cls.temp = tempfile.TemporaryDirectory(prefix="eiem-program-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        source = cls.folder / "program_test.cpp"
        source.write_text(SOURCE, encoding="utf-8")
        cls.exe = cls.folder / "program_test.exe"
        build = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/utf-8",
                                f"/I{ROOT / 'src'}", str(source), f"/Fe{cls.exe}"],
                               cwd=cls.folder, capture_output=True, text=True)
        if build.returncode:
            raise AssertionError(build.stdout + build.stderr)

    def run_case(self, name):
        result = subprocess.run([str(self.exe), name], cwd=self.folder, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_independent_actions_and_repeated_material_slot(self): self.run_case("semantics")
    def test_bad_file_cannot_publish_partial_or_broaden_match(self): self.run_case("invalid")
    def test_compiled_rule_scope_is_mod_local(self): self.run_case("compile")
    def test_reload_and_state_reapply_order(self): self.run_case("reload")
    def test_default_off_nested_conditions_and_roundtrip_cycle(self): self.run_case("conditions")
    def test_expression_precedence_short_circuit_and_validation(self): self.run_case("expressions")
    def test_inactive_branch_errors_never_partially_publish(self): self.run_case("condition_errors")
    def test_ordered_fields_mod_local_variables_and_static_partner_scope(self): self.run_case("ordered_and_scope")
    def test_hold_key_moves_toward_target_without_cycle_repeat(self): self.run_case("hold")
    def test_key_chords_are_explicit_and_strict(self): self.run_case("chords")

    def test_directory_loading_is_deterministic(self):
        for name, asset in (("z_last", "Z"), ("a_first", "A")):
            directory = self.folder / "plugin" / "mods" / name
            directory.mkdir(parents=True)
            (directory / "mod.ini").write_text(f"[RenderMain]\nasset={asset}\n", encoding="utf-8")
        self.run_case("publish")

    def test_invalid_mod_does_not_block_valid_sibling(self):
        self.run_case("independent_invalid")


if __name__ == "__main__":
    unittest.main()
