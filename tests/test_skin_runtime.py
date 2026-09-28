from runtime_source import read_runtime_source
"""Compile the production canonical per-instance skin resolver with fake Unity nodes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_mod_controls import function

ROOT = Path(__file__).resolve().parents[1]

SOURCE = r'''
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "eiem_skin_binding.h"

struct Array { char pad[24]{}; size_t count=0; void *items[64]{}; };
struct Node { std::string name; Node *parent=nullptr; std::vector<Node *> children; bool alive=true; };
struct Renderer { Array *bones=nullptr; Node *rootBone=nullptr; Node *skinningRoot=nullptr; };
struct Box { char pad[16]{}; int value=0; } box;
static constexpr size_t IL2CPP_ARRAY_DATA=32;
static int methods[12], writes=0;
static bool failSetter=false;
static bool s_eiemApplyingModMeshAssignment=false;
static void *g_smr_get_bones=&methods[0], *g_transform_get_parent=&methods[1];
static void *g_object_get_name=&methods[2], *g_transform_get_childCount=&methods[3];
static void *g_transform_GetChild=&methods[4], *g_transformClass=&methods[5];
static void *g_smr_set_bones=&methods[6], *g_smr_get_rootBone=&methods[7];
static void *g_smr_get_skinningRoot=&methods[8];
static void *g_gameObject_get_transform=nullptr, *g_component_get_transform=nullptr;
static std::vector<std::unique_ptr<Array>> arrays;
static void *NewArray(void *,size_t n) { auto a=std::make_unique<Array>(); a->count=n; arrays.push_back(std::move(a)); return arrays.back().get(); }
static auto il2cpp_array_new=&NewArray;
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
 if(m==g_smr_get_rootBone) return ((Renderer *)p)->rootBone;
 if(m==g_smr_get_skinningRoot) return ((Renderer *)p)->skinningRoot;
 auto n=(Node *)p;
 if(m==g_transform_get_parent) return n->parent;
 if(m==g_object_get_name) return &n->name;
 if(m==g_transform_get_childCount) { box.value=(int)n->children.size(); return &box; }
 if(m==g_transform_GetChild) return n->children.at(*(int *)args[0]);
 if(m==g_smr_set_bones) { ++writes; if(!failSetter) ((Renderer *)p)->bones=(Array *)args[0]; return nullptr; }
 return nullptr;
}
static void *EiemBackendInvokeNoThrow(void *m,void *p) { return Invoke(m,p); }
static void ReadStrUtf8(void *p,char *out,size_t n) { strncpy_s(out,n,((std::string *)p)->c_str(),_TRUNCATE); }
static bool InvokeChecked(void *m,void *p,void **args,void **out) { *out=Invoke(m,p,args); return !failSetter; }
static void EiemCollectBonePathHashes(void *,std::vector<uint32_t> *) {}
struct State { uint32_t replacementBonesHandle=0; };
static std::vector<State> s_eiemOverrides(1);
static SRWLOCK s_eiemOverrideLock=SRWLOCK_INIT;
static size_t EiemFindOverrideLocked(void *) { return 0; }
static void Log(const char *,...) {}
static constexpr bool kEiemEnableSkinBindingDiagnostics = false;
// The production resolver is extracted without the full trace translation unit.
#include "eiem_skin_resolver.h"

int main() {
 Node scene{"Scene"},actor{"ActorA",&scene},root{"Rig",&actor},chest{"RenamedChest",&root},pelvis{"Pelvis",&root},foot{"RenamedFoot",&pelvis};
 scene.children={&actor}; actor.children={&root}; root.children={&chest,&pelvis}; pelvis.children={&foot};
 Array source; source.count=1; source.items[0]=&chest;
 Renderer renderer{&source,&root,&actor};
 EiemSkinIdentity identity;
 identity.paths={"PrefabBoneName/CompletelyRenamedChest","PrefabBoneName/CompletelyRenamedFoot"};
 identity.hashes={1,2}; identity.boneIndexPaths={"0","1/0"};
 void *out=nullptr; char error[256]{};
 assert(EiemResolveMeshBonesFromNativeInstance(identity,&renderer,&out,error,sizeof(error)));
 assert(((Array *)out)->count==2 && ((Array *)out)->items[0]==&chest && ((Array *)out)->items[1]==&foot);
 // rootBone is preferred over the wider skinningRoot.  The same "0" path
 // therefore means Rig/first-child, never Actor/first-child.
 EiemSkinPaletteCache cache; cache.model=&actor;
 auto *previous=s_eiemActiveSkinPaletteCache; s_eiemActiveSkinPaletteCache=&cache;
 out=nullptr; assert(EiemResolveMeshBonesFromNativeInstance(identity,&renderer,&out,error,sizeof(error)));
 assert(((Array *)out)->items[0]==&chest && cache.boneTables.size()==1);
 s_eiemActiveSkinPaletteCache=previous;
 // A second actor gets an independent table and never borrows the first actor.
 Node actor2{"ActorB",&scene},root2{"RigRenamed",&actor2},chest2{"ChestB",&root2},pelvis2{"PelvisB",&root2},foot2{"FootB",&pelvis2};
 actor2.children={&root2}; root2.children={&chest2,&pelvis2}; pelvis2.children={&foot2};
 Array source2; source2.count=1; source2.items[0]=&chest2; Renderer renderer2{&source2,&root2,&actor2};
 out=nullptr; assert(EiemResolveMeshBonesFromNativeInstance(identity,&renderer2,&out,error,sizeof(error)));
 assert(((Array *)out)->items[0]==&chest2 && ((Array *)out)->items[0]!=&chest);
 EiemSkinIdentity invalid=identity; invalid.boneIndexPaths={"0","99"}; out=nullptr;
 assert(!EiemResolveMeshBonesFromNativeInstance(invalid,&renderer,&out,error,sizeof(error)) && !out);
 EiemSkinIdentity duplicate=identity; duplicate.boneIndexPaths={"0","0"}; out=nullptr;
 assert(!EiemResolveMeshBonesFromNativeInstance(duplicate,&renderer,&out,error,sizeof(error)) && !out);
 // A renderer without rootBone can use the instance root only when the
 // exported paths are relative to that root.
 Renderer fallback{&source2,nullptr,&actor2}; EiemSkinIdentity rootRelative=identity;
 rootRelative.boneIndexPaths={"0/0","0/1/0"}; out=nullptr;
 assert(EiemResolveMeshBonesFromNativeInstance(rootRelative,&fallback,&out,error,sizeof(error)));
 assert(((Array *)out)->items[0]==&chest2 && ((Array *)out)->items[1]==&foot2);
 Array expandedStorage; expandedStorage.count=2; expandedStorage.items[0]=&chest; expandedStorage.items[1]=&foot;
 assert(EiemPreserveSourceSkinning(&renderer,&expandedStorage,error,sizeof(error)) && writes==1);
 assert(EiemPreserveSourceSkinning(&renderer,&expandedStorage,error,sizeof(error)) && writes==1 && handles.size()==1);
 foot.alive=false; assert(!EiemPreserveSourceSkinning(&renderer,&expandedStorage,error,sizeof(error)) && writes==1);
 foot.alive=true; failSetter=true; renderer.bones=&source;
 assert(!EiemPreserveSourceSkinning(&renderer,&expandedStorage,error,sizeof(error)) && renderer.bones==&source && handles.size()==1);
 il2cpp_gchandle_free(s_eiemOverrides[0].replacementBonesHandle); assert(handles.empty());
}
'''

class SkinRuntimeTests(unittest.TestCase):
    def test_actual_resolver_and_assignment(self):
        if not shutil.which('cl'):
            self.skipTest('Requires MSVC')
        trace=read_runtime_source(ROOT)
        funcs='\n'.join(function(trace,s) for s in [
            'static bool EiemParseSkinIndexPath(',
            'static void *EiemSkinGetChildByIndex(',
            'static bool EiemBuildSkinBoneTable(',
            'static bool EiemResolveMeshBonesFromIndexTable(',
            'static bool EiemResolveMeshBonesFromNativeInstance(',
            'static bool EiemPreserveSourceSkinning(',
        ])
        with tempfile.TemporaryDirectory(prefix='eiem-skin-runtime-') as directory:
            folder=Path(directory); cpp=folder/'skin.cpp'
            cpp.write_text(SOURCE.replace('#include "eiem_skin_resolver.h"',funcs),encoding='utf-8')
            exe=folder/'skin.exe'
            build=subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/utf-8',f'/I{ROOT/"src"}',str(cpp),f'/Fe{exe}'],cwd=folder,capture_output=True,text=True,encoding='utf-8',errors='replace')
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,encoding='utf-8',errors='replace')
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

if __name__=='__main__': unittest.main()
