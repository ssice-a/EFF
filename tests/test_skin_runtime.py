"""Compile the production source-Renderer skin resolver with fake palettes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from runtime_source import read_runtime_source
from test_mod_controls import function

ROOT = Path(__file__).resolve().parents[1]

SOURCE = r'''
#include <windows.h>
#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "eiem_skin_binding.h"

struct Array { char pad[24]{}; size_t count=0; void *items[64]{}; };
struct Node { std::string name; bool alive=true; };
struct Renderer { Array *bones=nullptr; };
static constexpr size_t IL2CPP_ARRAY_DATA=32;
static int methods[12], writes=0;
static bool failSetter=false;
static bool s_eiemApplyingModMeshAssignment=false;
static void *g_smr_get_bones=&methods[0], *g_smr_set_bones=&methods[1];
static void *g_smr_get_sharedMesh=&methods[2], *g_transformClass=&methods[3];
static auto il2cpp_array_new=+[](void *,size_t n)->void * {
  auto *array = new Array(); array->count=n; return array;
};
static std::map<uint32_t,void *> handles;
static uint32_t serial=0;
static auto il2cpp_gchandle_new=+[](void *p,bool)->uint32_t { handles[++serial]=p; return serial; };
static auto il2cpp_gchandle_free=+[](uint32_t h) { assert(handles.erase(h)==1); };
static bool EiemOnUnityThread() { return true; }
static int EiemNativeObjectStatus(void *p) { return p && ((Node *)p)->alive ? 1:0; }
static size_t EiemManagedArrayLength(void *p) { return p ? ((Array *)p)->count : 0; }
static bool EiemManagedObjectArraySame(void *x,void *y) {
 if(!x || !y) return x==y;
 auto a=(Array *)x,b=(Array *)y;
 return a->count==b->count && std::equal(a->items,a->items+a->count,b->items);
}
static void *Invoke(void *m,void *p,void **args=nullptr) {
 if(m==g_smr_get_bones) return ((Renderer *)p)->bones;
 if(m==g_smr_get_sharedMesh) return nullptr;
 if(m==g_smr_set_bones) { ++writes; if(!failSetter) ((Renderer *)p)->bones=(Array *)args[0]; return nullptr; }
 return nullptr;
}
static void *EiemBackendInvokeNoThrow(void *m,void *p) { return Invoke(m,p); }
static bool EiemReadLiveMeshIdentity(void *,char *,size_t,char *,size_t) { return false; }
static bool EiemModSameLogicalPath(const char *a,const char *b) { return a && b && !_stricmp(a,b); }
static bool EiemCaptureSourceSkinPalette(const std::vector<void *> &, EiemSkinPaletteCache *) { return false; }
static bool InvokeChecked(void *m,void *p,void **args,void **out) { *out=Invoke(m,p,args); return !failSetter; }
struct State { uint32_t replacementBonesHandle=0; };
static std::vector<State> s_eiemOverrides(1);
static SRWLOCK s_eiemOverrideLock=SRWLOCK_INIT;
static size_t EiemFindOverrideLocked(void *) { return 0; }
static void Log(const char *,...) {}
static constexpr bool kEiemEnableSkinBindingDiagnostics = false;
#include "eiem_skin_resolver.h"

int main() {
 Node chest{"Chest"}, foot{"Foot"};
 Array source; source.count=1; source.items[0]=&chest;
 Renderer renderer{&source};
 EiemSkinPaletteCache cache; cache.model=&renderer;
 EiemSkinPaletteCache::SourcePalette palette;
 palette.renderer=&renderer; palette.mesh=(void *)1;
 palette.meshPath="assets/body_lod0.asset"; palette.meshAsset="Body_lod0";
 palette.bones={&chest,&foot};
 cache.sourcePalettes.push_back(palette);
 auto *previous=s_eiemActiveSkinPaletteCache; s_eiemActiveSkinPaletteCache=&cache;
 EiemSkinIdentity identity;
 identity.sourceCandidates={
   {{"assets/body_lod0.asset","Body_lod0",0}},
   {{"assets/body_lod0.asset","Body_lod0",1}},
 };
 void *out=nullptr; char error[256]{};
 assert(EiemResolveMeshBonesFromNativeInstance(identity,&renderer,&out,error,sizeof(error)));
 assert(((Array *)out)->count==2 && ((Array *)out)->items[0]==&chest &&
        ((Array *)out)->items[1]==&foot);
 EiemSkinIdentity invalid=identity;
 invalid.sourceCandidates[1]={{"assets/missing.asset","Missing",0}};
 out=nullptr;
 assert(!EiemResolveMeshBonesFromNativeInstance(invalid,&renderer,&out,error,sizeof(error)) && !out);
 EiemSkinPaletteCache::SourcePalette other=palette;
 Node otherChest{"OtherChest"}; other.bones[0]=&otherChest;
 cache.sourcePalettes.push_back(other);
 out=nullptr;
 assert(!EiemResolveMeshBonesFromNativeInstance(identity,&renderer,&out,error,sizeof(error)) && !out);
 cache.sourcePalettes.pop_back();
 Array expandedStorage; expandedStorage.count=2; expandedStorage.items[0]=&chest; expandedStorage.items[1]=&foot;
 assert(EiemPreserveSourceSkinning(&renderer,&expandedStorage,error,sizeof(error)) && writes==1);
 assert(EiemPreserveSourceSkinning(&renderer,&expandedStorage,error,sizeof(error)) && writes==1 && handles.size()==1);
 foot.alive=false; assert(!EiemPreserveSourceSkinning(&renderer,&expandedStorage,error,sizeof(error)) && writes==1);
 foot.alive=true; failSetter=true; renderer.bones=&source;
 assert(!EiemPreserveSourceSkinning(&renderer,&expandedStorage,error,sizeof(error)) && renderer.bones==&source && handles.size()==1);
 il2cpp_gchandle_free(s_eiemOverrides[0].replacementBonesHandle); assert(handles.empty());
 s_eiemActiveSkinPaletteCache=previous;
}
'''

class SkinRuntimeTests(unittest.TestCase):
    def test_actual_resolver_and_assignment(self):
        if not shutil.which('cl'):
            self.skipTest('Requires MSVC')
        trace = read_runtime_source(ROOT)
        funcs = '\n'.join(function(trace, s) for s in [
            'static bool EiemSourcePaletteMatches(',
            'static bool EiemResolveMeshBonesFromSourcePalettes(',
            'static bool EiemResolveMeshBonesFromNativeInstance(',
            'static bool EiemPreserveSourceSkinning(',
        ])
        with tempfile.TemporaryDirectory(prefix='eiem-skin-runtime-') as directory:
            folder = Path(directory); cpp = folder / 'skin.cpp'
            cpp.write_text(SOURCE.replace('#include "eiem_skin_resolver.h"', funcs),
                           encoding='utf-8')
            exe = folder / 'skin.exe'
            build = subprocess.run(
                ['cl','/nologo','/EHsc','/std:c++17','/utf-8',f'/I{ROOT / "src"}',
                 str(cpp),f'/Fe{exe}'], cwd=folder, capture_output=True,
                text=True, encoding='utf-8', errors='replace')
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True,
                                 encoding='utf-8', errors='replace')
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

if __name__ == '__main__': unittest.main()
