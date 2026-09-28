"""Execute the disk dependency reader and Mod loader; no Unity/native physics."""
import copy
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

from test_physics_document import codec, fixture

ROOT = Path(__file__).resolve().parents[1]


def skeleton_bytes(version=2, tip="Tip", extra_path=None, position=0.0):
    def string(value):
        data = value.encode("utf-8")
        length, prefix = len(data), bytearray()
        while length >= 128:
            prefix.append((length & 127) | 128)
            length >>= 7
        return bytes(prefix) + bytes([length]) + data

    nodes = [("", -1), ("Rig", 0), ("Rig/Root", 1), (f"Rig/Root/{tip}", 2)]
    if extra_path is not None:
        nodes.append((extra_path, 1))
    data = b"EFFSKEL\0" + struct.pack("<i", version)
    data += string(codec.COORDINATE) + struct.pack("<I", len(nodes))
    for path, parent in nodes:
        data += string(path) + struct.pack("<i3f4f3f", parent, position, 0, 0, 0, 0, 0, 1, 1, 1, 1)
    data += struct.pack("<Ii", 0, -1)
    if version == 2:
        flags = [1, 1, 1, 0] + ([1] if extra_path is not None else [])
        data += struct.pack("<I", len(nodes)) + bytes(flags)
    return data


SOURCE = r'''
#include <windows.h>
#include <cstdio>
#include <sstream>
static void Log(const char *, ...) {}
#include "eiem_mods.h"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s; %s\n",__LINE__,#x,error.c_str()); return 1; } } while(false)
int wmain(int argc, wchar_t **argv) {
  std::string error;
  CHECK(argc>=3);
  const std::wstring mode=argv[1];
  const std::filesystem::path path=argv[2];
  if (mode==L"asset" || mode==L"asset_bad" || mode==L"asset_v67") {
    auto sentinel=std::make_shared<EiemPhysicsAsset>(); sentinel->physics.id="sentinel";
    std::shared_ptr<const EiemPhysicsAsset> asset=sentinel;
    const bool ok=EiemLoadPhysicsAsset(path,asset,error);
    if (mode==L"asset_bad") {
      CHECK(!ok && asset==sentinel && !error.empty());
    } else {
      CHECK(ok && error.empty() && asset!=sentinel);
      CHECK(asset->physics.groups.size()==1);
      if (mode==L"asset_v67") {
        CHECK(asset->physics.colliders.empty() && asset->skeleton.nodes.size()==13);
        CHECK(asset->physics.groups[0].nodes.size()==3);
        CHECK(asset->physics.groups[0].nodes[2].bone==asset->skeleton.nodes.back().path);
      } else {
        CHECK(asset->physics.colliders.size()==1);
        CHECK(asset->skeleton.nodes.size()>=4 && asset->skeleton.nodes[3].path=="Rig/Root/Tip");
        CHECK(asset->physics.groups[0].nodes[1].bone==asset->skeleton.nodes[3].path);
      }
      CHECK(asset->skeletonPath.is_absolute());
      CHECK(argc==4 && asset->skeleton.nodes[3].source==(std::wstring(argv[3])==L"source"));
    }
  } else if (mode==L"snapshot") {
    CHECK(argc==4);
    std::shared_ptr<const EiemPhysicsAsset> original;
    CHECK(EiemLoadPhysicsAsset(path,original,error));
    const float gravity=original->physics.groups[0].parameters[0];
    std::filesystem::copy_file(argv[3],path,std::filesystem::copy_options::overwrite_existing);
    std::shared_ptr<const EiemPhysicsAsset> next;
    CHECK(EiemLoadPhysicsAsset(path,next,error));
    CHECK(next!=original && next->physics.groups[0].parameters[0]!=gravity);
    CHECK(original->physics.groups[0].parameters[0]==gravity);
    {std::ofstream damaged(original->skeletonPath,std::ios::binary); damaged<<"truncated";}
    auto previous=next;
    CHECK(!EiemLoadPhysicsAsset(path,next,error) && next==previous && !error.empty());
    CHECK(original->skeleton.nodes[3].path=="Rig/Root/Tip");
  } else if (mode==L"parse_bad") {
    EiemModProgram program;
    std::istringstream seed("[Constants]\n$keep=7\n[RenderKeep]\nasset=Keep\nhandling=skip\n");
    CHECK(EiemModParseStream(seed,"existing/mod.ini",program,&error));
    CHECK(!EiemModParseFile(path.u8string().c_str(),program,&error));
    CHECK(!error.empty() && program.states.size()==1 && program.definitions.size()==1);
    CHECK(program.rules.size()==1 && program.resources.empty() && program.prefabs.empty());
    CHECK(program.states[0].variables.at("$keep")==7 && std::string(program.rules[0].asset)=="Keep");
  } else if (mode==L"aliases") {
    EiemModProgram program;
    CHECK(EiemModParseFile(path.u8string().c_str(),program,&error));
    CHECK(program.resources.size()==3 && program.states.size()==1 && program.rules.size()==1);
    CHECK(program.resources[0].physicsAsset && program.resources[0].physicsAsset==program.resources[1].physicsAsset);
    CHECK(!program.resources[2].physicsAsset && program.rules[0].hasPhysics);
    CHECK(std::string(program.rules[0].physics)=="PhysicsOne");
  } else if (mode==L"intents") {
    EiemModProgram program;
    CHECK(EiemModParseFile(path.u8string().c_str(),program,&error));
    CHECK(program.resources.size()==2 && program.rules.size()==1);
    s_eiemModProgram=program;
    std::vector<EiemPhysicsIntent> intents;
    EiemModRule first=program.rules[0];
    void *rendererA=reinterpret_cast<void *>(1);
    void *rendererB=reinterpret_cast<void *>(2);
     CHECK(EiemCollectPhysicsIntent(first,&intents,rendererA));
     if (!kEiemEnableExperimentalPhysicsRuntime) {
       CHECK(intents.empty());
     } else {
       CHECK(intents.size()==1 && intents[0].rendererMatches==1);
       CHECK(intents[0].matchedRenderers.size()==1 && intents[0].matchedRenderers[0]==rendererA);
       CHECK(std::string(intents[0].resourceSection)=="PhysicsOne");
       CHECK(std::string(intents[0].firstRenderSection)=="RenderMain");
       CHECK(std::string(intents[0].firstRule.section)=="RenderMain");
     }
    EiemModRule alias=first;
    strcpy_s(alias.physics,"PhysicsTwo");
    strcpy_s(alias.section,"RenderLod");
     CHECK(EiemCollectPhysicsIntent(alias,&intents,rendererB));
     if (kEiemEnableExperimentalPhysicsRuntime) {
       CHECK(intents.size()==1 && intents[0].rendererMatches==2);
       CHECK(intents[0].matchedRenderers.size()==2 && intents[0].matchedRenderers[1]==rendererB);
     } else {
       CHECK(intents.empty());
     }
    EiemModRule cleared=first;
    cleared.hasPhysics=false; cleared.physics[0]='\0';
     CHECK(EiemCollectPhysicsIntent(cleared,&intents) &&
           intents.size()==(kEiemEnableExperimentalPhysicsRuntime ? 1u : 0u));
    EiemModRule missing=first;
    strcpy_s(missing.physics,"Missing");
     if (kEiemEnableExperimentalPhysicsRuntime)
       CHECK(!EiemCollectPhysicsIntent(missing,&intents) && intents.size()==1);
     else
       CHECK(EiemCollectPhysicsIntent(missing,&intents) && intents.empty());
    EiemModRule changed=first;
    strcpy_s(changed.physics,"PhysicsTwo");
    CHECK(!EiemSameRenderAssembly(first,changed));
    changed=first; changed.shapeWeights[0]+=1;
    CHECK(EiemSameRenderAssembly(first,changed));
  } else if (mode==L"append") {
    CHECK(argc==4);
    EiemModProgram program;
    CHECK(EiemModParseFile(path.u8string().c_str(),program,&error));
    CHECK(EiemModParseFile(std::filesystem::path(argv[3]).u8string().c_str(),program,&error));
    CHECK(program.states.size()==2 && program.definitions.size()==2 && program.resources.size()==2);
    CHECK(program.definitions[0].stateIndex==0 && program.definitions[1].stateIndex==1);
    const auto first=program.resources[0].physicsAsset, second=program.resources[1].physicsAsset;
    CHECK(first && second && first!=second && first->physics.groups[0].parameters[0]!=second->physics.groups[0].parameters[0]);
    CHECK(!program.rules[0].hasPhysics && program.rules[1].hasPhysics);
    program.states[0].variables["$enabled"]=1; EiemEvaluateModProgram(program);
    CHECK(program.rules[0].hasPhysics && program.rules[1].hasPhysics);
    CHECK(program.resources[0].physicsAsset==first && program.resources[1].physicsAsset==second);
  } else if (mode==L"validate") {
    EiemModProgram program;
    CHECK(EiemModParseFile(path.u8string().c_str(),program,&error));
    size_t physicsResources=0,physicsRules=0;
    for (const auto &resource:program.resources)
      if (EiemModEquals(resource.kind,"Physics") && resource.physicsAsset) ++physicsResources;
    for (const auto &rule:program.rules) if (rule.hasPhysics) ++physicsRules;
    CHECK(physicsResources>0 && physicsRules>0);
  } else if (mode==L"reload") {
    const size_t expected=std::stoul(path.wstring());
    const size_t expectedPhysics=argc>=4 ? std::stoul(argv[3]) : 0;
    EiemReloadMods();
    CHECK(s_eiemModGeneration==1 && s_eiemModProgram.states.size()==expected);
    CHECK(s_eiemModProgram.rules.size()==expected && s_eiemModProgram.definitions.size()==expected);
    CHECK(std::string(s_eiemModProgram.rules.front().asset)=="First");
    CHECK(std::string(s_eiemModProgram.rules.back().asset)=="Last");
    LONG generation=0;
    // The first switchable Mod is selected on load; ordinary Mods have no keys.
    CHECK(EiemGetModKeyChords(&generation).size()==(expected==3 ? 1u:0u));
    CHECK(EiemGetSelectedModPath()==(expected==3
          ? "plugin\\mods\\M\\mod.ini" : std::string()));
    if (expected==3)
      for (const auto &state : s_eiemModProgram.states)
        if (!state.keys.empty()) CHECK(EiemSelectControlledMod(state.path));
    const auto keys=EiemGetModKeyChords(&generation);
    const auto uis=EiemGetModUis(&generation);
    CHECK(keys.size()==(expected==3 ? 1u:0u) && uis.size()==(expected==3 ? 1u:0u));
    CHECK(s_eiemModProgram.resources.size()==(expected==3 ? 1u:0u));
    size_t physicsRules=0;
    for (const auto &rule:s_eiemModProgram.rules) if (rule.hasPhysics) ++physicsRules;
    CHECK(physicsRules==expectedPhysics);
    EiemReloadMods();
    CHECK(s_eiemModGeneration==2 && s_eiemModProgram.states.size()==expected);
    CHECK(s_eiemModProgram.definitions.back().stateIndex==expected-1);
  } else return 2;
  return 0;
}
'''


class PhysicsResourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("cl"):
            raise unittest.SkipTest("Requires MSVC developer environment")
        cls.build = tempfile.TemporaryDirectory(prefix="eiem-physics-resource-build-")
        cls.addClassCleanup(cls.build.cleanup)
        folder = Path(cls.build.name)
        source = folder / "resources.cpp"
        source.write_text(SOURCE, encoding="utf-8")
        cls.exe = folder / "resources.exe"
        result = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/utf-8",
                                 f"/I{ROOT / 'src'}", str(source), f"/Fe{cls.exe}"],
                                cwd=folder, capture_output=True, text=True, errors="replace")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="eiem-physics-resources-")
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)

    def run_case(self, mode, *args):
        result = subprocess.run([str(self.exe), mode, *(str(arg) for arg in args)],
                                cwd=self.folder, capture_output=True, text=True, errors="replace")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def package(self, folder=None, document=None, skeleton=None):
        folder = folder or self.folder
        document = copy.deepcopy(document if document is not None else fixture())
        rig = folder / document["skeleton"]
        rig.parent.mkdir(parents=True, exist_ok=True)
        rig.write_bytes(skeleton if skeleton is not None else skeleton_bytes())
        physics = folder / "body.physics"
        physics.write_bytes(codec.encode(document))
        return physics, rig

    def ini(self, folder, text):
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / "mod.ini"
        path.write_text(text, encoding="utf-8")
        return path

    def test_python_author_package_read_by_cpp_with_current_skeleton(self):
        for version in (2,):
            with self.subTest(version=version):
                physics, _ = self.package(skeleton=skeleton_bytes(version, extra_path="Rig/" + "x" * 160))
                self.run_case("asset", physics, "source" if version == 1 else "new")

    def test_utf8_asset_and_dependency_paths(self):
        doc = fixture(); doc["skeleton"] = "骨架/角色.skeleton"
        physics, _ = self.package(self.folder / "作者资源", doc)
        self.run_case("asset", physics, "new")

    def test_disk_edits_do_not_change_prepared_snapshot_or_failed_output(self):
        physics, _ = self.package()
        doc = fixture(); doc["groups"][0]["parameters"]["gravity"] += 1
        updated = self.folder / "updated.physics"; updated.write_bytes(codec.encode(doc))
        self.run_case("snapshot", physics, updated)

    def test_missing_group_or_collider_bones_leave_asset_unpublished(self):
        physics, _ = self.package(skeleton=skeleton_bytes(tip="Other"))
        self.run_case("asset_bad", physics)
        doc = fixture(); doc["colliders"][0]["bone"] = "Rig/Missing"
        physics, _ = self.package(document=doc)
        self.run_case("asset_bad", physics)

    def test_invalid_skeleton_wire_and_oversized_files_leave_asset_unpublished(self):
        valid = skeleton_bytes()
        invalid = [valid[:-1], valid+b"x", valid.replace(b"Rig/Root/Tip", b"Rig/Root/\xffip"),
                   valid[:-1]+b"\x02", b"\x00" * (16 * 1024 * 1024 + 1)]
        for index, payload in enumerate(invalid):
            with self.subTest(case=index):
                physics, _ = self.package(skeleton=payload)
                self.run_case("asset_bad", physics)
        physics, rig = self.package(); rig.unlink()
        self.run_case("asset_bad", physics)
        physics, _ = self.package(); physics.write_bytes(physics.read_bytes()+b"x")
        self.run_case("asset_bad", physics)

    def test_shared_file_aliases_prepare_once_and_match_explicit_skeleton(self):
        self.package()
        path = self.ini(self.folder, "[PhysicsOne]\npath=body.physics\n[PhysicsTwo]\npath=body.physics\n"
            "[SkeletonRig]\npath=skeletons/rig.skeleton\n[RenderMain]\nasset=Body\n"
            "skeleton=SkeletonRig\nphysics=PhysicsOne\n")
        self.run_case("aliases", path)

    def test_matched_renderer_physics_is_deduplicated_per_mod_snapshot(self):
        self.package()
        path = self.ini(self.folder, "[PhysicsOne]\npath=body.physics\n"
            "[PhysicsTwo]\npath=body.physics\n[RenderMain]\nasset=Body\n"
            "physics=PhysicsOne\n")
        self.run_case("intents", path)

    def test_failed_dependencies_do_not_append_any_mod_state(self):
        self.package()
        for reference in ("missing.physics", "../body.physics", "skeletons", "C:/body.physics", "/body.physics"):
            with self.subTest(reference=reference):
                path = self.ini(self.folder, f"[Constants]\n$new=1\n[PhysicsBody]\npath={reference}\n"
                    "[RenderMain]\nasset=Body\nphysics=PhysicsBody\n")
                self.run_case("parse_bad", path)

    def test_dependency_junction_cannot_escape_package_directory(self):
        outside = self.folder / "outside"
        self.package(outside)
        inside = self.folder / "inside"; inside.mkdir()
        junction = inside / "linked"
        result = subprocess.run(["cmd", "/d", "/c", "mklink", "/J", str(junction), str(outside)],
                                capture_output=True, text=True, errors="replace")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        try:
            path = self.ini(inside, "[PhysicsBody]\npath=linked/body.physics\n"
                            "[RenderMain]\nasset=Body\nphysics=PhysicsBody\n")
            self.run_case("parse_bad", path)
        finally:
            os.rmdir(junction)  # Remove this junction only, never traverse its target.
        self.assertTrue((outside / "body.physics").is_file())

    def test_inactive_references_and_skeleton_mismatches_reject_entire_document(self):
        self.package()
        second = fixture(); second["skeleton"] = "skeletons/other.skeleton"
        (self.folder / second["skeleton"]).write_bytes(skeleton_bytes(position=1))
        (self.folder / "other.physics").write_bytes(codec.encode(second))
        cases = [
            "[PhysicsBody]\npath=body.physics\n[PhysicsOther]\npath=other.physics\n"
            "[RenderMain]\nasset=Body\nif $enabled\nphysics=PhysicsBody\nelse\nphysics=PhysicsOther\nendif\n",
            "[PhysicsBody]\npath=body.physics\n[SkeletonOther]\npath=skeletons/other.skeleton\n"
            "[RenderMain]\nasset=Body\nphysics=PhysicsBody\nif !$enabled\nskeleton=SkeletonOther\nendif\n",
            "[PhysicsBody]\npath=body.physics\n[RenderMain]\nasset=Body\nif !$enabled\nphysics=PhysicsMissing\nendif\n",
            "[MeshWrong]\npath=body.mesh\n[RenderMain]\nasset=Body\nif !$enabled\nphysics=MeshWrong\nendif\n",
        ]
        for index, body in enumerate(cases):
            with self.subTest(case=index):
                path = self.ini(self.folder, "[Constants]\n$enabled=1\n" + body)
                self.run_case("parse_bad", path)

    def test_same_resource_names_in_different_mods_keep_snapshots_and_state_separate(self):
        paths = []
        for index in (0, 1):
            folder = self.folder / str(index)
            doc = fixture(); doc["groups"][0]["parameters"]["gravity"] += index
            self.package(folder, doc)
            paths.append(self.ini(folder, f"[Constants]\n$enabled={index}\n[PhysicsBody]\npath=body.physics\n"
                "[RenderMain]\nasset=Body\nif $enabled\nphysics=PhysicsBody\nendif\n"))
        self.run_case("append", *paths)

    def test_v67_typhoea_fixture_is_accepted_by_the_production_reader(self):
        script = ROOT / "tools/diagnostics/make_v67_typhoea_physics_fixture.py"
        result = subprocess.run([sys.executable, str(script), str(self.folder)],
                                capture_output=True, text=True, errors="replace")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_case("asset_v67", self.folder / "typhoea-left-index-v67.physics",
                      "source")

    def test_production_loader_publishes_valid_physics_mod_and_respects_conditions(self):
        for body, active in (("physics=PhysicsBody\n", 1),
                             ("if !$enabled\nphysics=PhysicsBody\nendif\n", 0),
                             ("physics=PhysicsBody\nphysics=\n", 0)):
            with self.subTest(body=body, active=active):
                self.make_reload_mods(body)
                self.run_case("reload", 3, active)

    def test_production_loader_keeps_ordinary_mods_and_inert_resource_declarations(self):
        self.make_reload_mods("physics=\n")
        self.run_case("reload", 3)

    def make_reload_mods(self, physics_body):
        mods = self.folder / "plugin/mods"
        self.ini(mods / "A", "[RenderFirst]\nasset=First\nhandling=skip\n")
        self.ini(mods / "Z", "[RenderLast]\nasset=Last\nhandling=skip\n")
        middle = mods / "M"
        self.package(middle)
        self.ini(middle, "[Constants]\n$enabled=1\n[KeyToggle]\nkey=F7\ntype=cycle\n$enabled=0,1\n"
            "[UISettings]\npath=ui.lua\n[PhysicsBody]\npath=body.physics\n"
            "[RenderMain]\nasset=Middle\nhandling=skip\n" + physics_body)


if __name__ == "__main__":
    unittest.main()
