"""Real UTF-8 Mod discovery, disk readers and reload in isolated directories."""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from test_mod_controls import function
from test_physics_resources import skeleton_bytes
from test_physics_document import codec, fixture

ROOT = Path(__file__).resolve().parents[1]

PREFIX = r'''
#include <windows.h>
#include <limits>
#include <cstdio>
#include <sstream>
static void Log(const char *, ...) {}
static int g_guiToggleVK=VK_INSERT, g_modReloadVK=VK_F10;
#include "eiem_config.h"
#include "eiem_mods.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(false)
'''

MAIN = r'''
int wmain(int argc, wchar_t **argv) {
  CHECK(argc==2);
  const std::wstring mode=argv[1];
  const std::string mod=u8"plugin\\mods\\中文角色😀\\mod.ini";
  const std::string meshSection=u8"Mesh衣服", materialSection=u8"Material材质";
  std::string error;
  if (mode==L"sections") {
    EiemModProgram p;
    std::istringstream input(u8"[Mesh衣服]\npath=meshes/衣服.mesh\n"
      "[Render衣服]\nasset=Cloth\nmesh=Mesh衣服\n");
    CHECK(EiemModParseStream(input,"ascii/mod.ini",p,&error));
    EiemPublishModState(p);
    EiemModResource resource;
    CHECK(EiemFindModResource("ascii/mod.ini",meshSection.c_str(),"Mesh",&resource));
    CHECK(p.rules.size()==1 && std::string(p.rules[0].mesh)==meshSection);
  } else if (mode==L"global") {
    CHECK(LoadEiemConfig());
    CHECK(std::filesystem::is_regular_file(L"plugin/eff.ini"));
    CHECK(EiemGetGlobalConfig().reload.vk==VK_F10);
    { std::ofstream update(L"plugin/eff.ini",std::ios::binary);
      update<<"[Hotkeys]\nreload=F8\ngui=INSERT\n"; }
    CHECK(LoadEiemConfig() && EiemGetGlobalConfig().reload.vk==VK_F8);
  } else if (mode==L"disk") {
    EiemModProgram p;
    CHECK(EiemModParseFile(mod.c_str(),p,&error));
    CHECK(p.resources.size()==5 && p.resources.back().physicsAsset);
    EiemPublishModState(p);
    EiemModResource mesh,material,texture,skeleton;
    CHECK(EiemFindModResource(mod.c_str(),meshSection.c_str(),"Mesh",&mesh));
    CHECK(EiemFindModResource(mod.c_str(),materialSection.c_str(),"Material",&material));
    CHECK(EiemFindModResource(mod.c_str(),u8"Texture贴图","Texture",&texture));
    CHECK(EiemFindModResource(mod.c_str(),u8"Skeleton骨架","Skeleton",&skeleton));
    char path[4096]={},detail[256]={};
    CHECK(EiemResolveResourceDiskPath(mesh,path,sizeof(path)));
    CHECK(std::filesystem::u8path(path)==std::filesystem::absolute(
          std::filesystem::u8path(mod).parent_path()/std::filesystem::u8path(mesh.path)).lexically_normal());
    EiemNativeReader reader(path); uint32_t value=0;
    CHECK(reader.Good() && reader.Value(&value) && value==0x12345678);
    const uint64_t before=EiemMeshResourceFileStamp(path);
    CHECK(before!=0);
    { std::ofstream update(std::filesystem::u8path(path),std::ios::binary|std::ios::app); update<<"changed"; }
    CHECK(EiemMeshResourceFileStamp(path)!=before);
    CHECK(EiemResolveResourceDiskPath(material,path,sizeof(path)));
    std::vector<std::pair<std::string,std::string>> values;
    CHECK(EiemReadMaterialFile(path,&values,detail,sizeof(detail)));
    CHECK((values==std::vector<std::pair<std::string,std::string>>{
          {"format","EFFMAT"},{"texture._BaseMap",u8"Texture贴图"}}));
    CHECK(EiemResolveResourceDiskPath(texture,path,sizeof(path)));
    EiemNativeReader png(path); char magic[8]={};
    CHECK(png.Bytes(magic,8) && std::string(magic,8)==std::string("\x89PNG\r\n\x1a\n",8));
    CHECK(EiemResolveResourceDiskPath(skeleton,path,sizeof(path)));
    EiemNativeReader bones(path); EiemSkeletonDocument document;
    CHECK(EiemReadSkeleton(bones,document,error) && document.nodes.size()==4);
    strcpy_s(mesh.path,"../escape.mesh");
    CHECK(!EiemResolveResourceDiskPath(mesh,path,sizeof(path)));
  } else if (mode==L"unreadable") {
    EiemModProgram p;
    CHECK(!EiemModParseFile(u8"plugin/mods/missing/mod.ini",p,&error) && error.empty());
    CHECK(!EiemModParseFile(std::filesystem::u8path(mod).parent_path().u8string().c_str(),p,&error) && !error.empty());
    CHECK(p.rules.empty() && p.resources.empty());
  } else if (mode==L"reload") {
    CHECK(EiemReloadMods() && s_eiemModProgram.states.size()==1);
    CHECK(s_eiemModProgram.states[0].path==mod);
    CHECK(EiemGetSelectedModPath()==mod);
    const LONG first=s_eiemModGeneration;
    EiemModProgram next; std::vector<std::string> affected;
    CHECK(EiemPrepareInputUpdate({{{VK_F6,0},first,mod}},&next,&affected));
    CHECK(next.states[0].variables.at("$toggle")==1);
    EiemPublishModState(next); s_eiemPersistentStates.Flush(true);
    CHECK(std::filesystem::exists(std::filesystem::u8path(mod).parent_path()/L"state.ini"));
    CHECK(EiemReloadMods() && s_eiemModGeneration==first+1);
    CHECK(EiemGetSelectedModPath()==mod && s_eiemModProgram.states[0].variables.at("$toggle")==1);
    // Replace the source INI on disk: F10 must consume the changed program.
    { std::ofstream update(std::filesystem::u8path(mod),std::ios::binary);
      update<<u8"[Constants]\npersist $toggle=0\n[Key衣服]\nkey=F6\ntype=cycle\n$toggle=0,1\n"
              "[Render衣服]\nasset=ClothUpdated\nhandling=skip\n"; }
    CHECK(EiemReloadMods() && s_eiemModProgram.rules.size()==1);
    CHECK(std::string(s_eiemModProgram.rules[0].asset)=="ClothUpdated");
    CHECK(s_eiemModProgram.states[0].variables.at("$toggle")==1);
  } else return 2;
  return 0;
}
'''


class UnicodePathTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("cl"):
            raise unittest.SkipTest("Requires MSVC developer environment")
        cls.build = tempfile.TemporaryDirectory(prefix="eiem-unicode-build-")
        cls.addClassCleanup(cls.build.cleanup)
        folder = Path(cls.build.name)
        backend = (ROOT / "src/eiem_resource_backend.h").read_text(encoding="utf-8")
        reader = function(backend, "class EiemNativeReader {") + ";"
        start = backend.index("static bool EiemResolveResourceDiskPath(",
                              backend.index("static bool EiemBuildMeshResource("))
        disk_functions = function(backend[start:], "static bool EiemResolveResourceDiskPath(")
        disk_functions += function(backend, "static bool EiemReadMaterialFile(")
        stamp_start = backend.index("static uint64_t EiemMeshResourceFileStamp(const char *path) {")
        disk_functions += function(backend[stamp_start:], "static uint64_t EiemMeshResourceFileStamp(")
        cpp = folder / "unicode.cpp"
        cpp.write_text(PREFIX + reader + disk_functions + MAIN, encoding="utf-8")
        cls.exe = folder / "unicode.exe"
        result = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/utf-8",
                                 f"/I{ROOT / 'src'}", str(cpp), f"/Fe{cls.exe}"],
                                cwd=folder, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def run_case(self, mode):
        with tempfile.TemporaryDirectory(prefix="eiem-unicode-") as temp:
            root = Path(temp) / "中文游戏目录😀"
            mod = root / "plugin/mods/中文角色😀"
            for subdir in ("meshes", "materials", "textures", "skeletons", "physics"):
                (mod / subdir).mkdir(parents=True)
            (mod / "meshes/衣服.mesh").write_bytes(struct.pack("<I", 0x12345678))
            (mod / "materials/材质.mat").write_text("format=EFFMAT\ntexture._BaseMap=Texture贴图\n", encoding="utf-8")
            (mod / "textures/贴图.png").write_bytes(b"\x89PNG\r\n\x1a\n")
            (mod / "skeletons/骨架.skeleton").write_bytes(skeleton_bytes())
            physics = fixture()
            # Physics dependencies are relative to their author file directory.
            physics["skeleton"] = "骨架.skeleton"
            (mod / "physics/骨架.skeleton").write_bytes(skeleton_bytes())
            (mod / "physics/物理.physics").write_bytes(codec.encode(physics))
            text = ("[Constants]\npersist $toggle=0\n[Key衣服]\nkey=F6\ntype=cycle\n$toggle=0,1\n"
                    "[Mesh衣服]\npath=meshes/衣服.mesh\n[Material材质]\npath=materials/材质.mat\n"
                    "[Texture贴图]\npath=textures/贴图.png\n[Skeleton骨架]\npath=skeletons/骨架.skeleton\n"
                    "[Physics物理]\npath=physics/物理.physics\n[Render衣服]\nasset=Cloth\n"
                    "mesh=Mesh衣服\nmaterial.0=Material材质\nskeleton=Skeleton骨架\nphysics=Physics物理\n")
            (mod / "mod.ini").write_text(text, encoding="utf-8-sig")
            result = subprocess.run([str(self.exe), mode], cwd=root, capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_and_unreadable_mod_ini_are_distinct(self):
        self.run_case("unreadable")

    def test_chinese_sections_and_references(self):
        self.run_case("sections")

    def test_global_ini_in_chinese_game_directory(self):
        self.run_case("global")

    def test_chinese_resource_files_and_cache_stamps(self):
        self.run_case("disk")

    def test_chinese_mod_discovery_keys_persistence_and_reload(self):
        self.run_case("reload")
