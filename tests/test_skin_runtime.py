from runtime_source import read_runtime_source
"""Run the actual Unity adapter with fake nodes and managed arrays, not a new algorithm."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_mod_controls import function

ROOT=Path(__file__).resolve().parents[1]
SOURCE=r'''
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <cassert>
#include "eiem_skin_binding.h"
struct Array { char pad[24]{}; size_t count=0; void *items[32]{}; };
struct Node { std::string name; Node *parent=nullptr; std::vector<Node *> children; bool alive=true; };
struct Renderer { Array *bones; Node *rootBone=nullptr; Node *skinningRoot=nullptr; };
struct Box { char pad[16]{}; int value=0; } box;
static constexpr size_t IL2CPP_ARRAY_DATA=32;
static int method[10],writes=0;
static bool failSetter=false, s_eiemApplyingModMeshAssignment=false;
static volatile LONG s_traceHierarchyIndexFailureCount=0;
static void *g_smr_get_bones=&method[0], *g_transform_get_parent=&method[1], *g_object_get_name=&method[2];
static void *g_transform_get_childCount=&method[3], *g_transform_GetChild=&method[4], *g_transformClass=&method[5], *g_smr_set_bones=&method[6];
static void *g_gameObject_get_transform=nullptr, *g_component_get_transform=nullptr;
static void *g_smr_get_rootBone=&method[7], *g_smr_get_skinningRoot=&method[8];
static void *s_eiemActivePrefabInstance=nullptr;
static std::vector<std::unique_ptr<Array>> arrays;
static std::unordered_map<void *, std::string> identityAssets;
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
// The extracted resolver functions use the production diagnostic gate. Keep
// this focused harness quiet while supplying the same compile-time contract.
static constexpr bool kEiemEnableSkinBindingDiagnostics = false;
static bool EiemModEquals(const char *a,const char *b) {
 return a && b && _stricmp(a,b)==0;
}
static bool EiemModSameLogicalPath(const char *a,const char *b) {
 return EiemModEquals(a,b);
}
static bool EiemBuildRelativeRendererPath(void *,void *,char *,size_t) { return false; }
static void *EiemReadSharedMesh(void *renderer,const char *) { return renderer; }
static bool EiemPrepareRenderInput(void *,void *,const char *,void **) { return true; }
static bool EiemReadLiveMeshIdentity(void *mesh,char *,size_t,char *asset,size_t assetSize) {
 auto found=identityAssets.find(mesh); if(found==identityAssets.end()) return false;
 strncpy_s(asset,assetSize,found->second.c_str(),_TRUNCATE); return true;
}
static thread_local const std::vector<EiemLiveSkinSource> *s_eiemLiveSkinSources=nullptr;
static bool EiemResolveMeshBonesFromAssembly(
    const EiemSkinIdentity &, void *, void **, char *, size_t) { return false; }
// FUNCTIONS
int main() {
 Node scene{"Scene"},actor{"ActorA",&scene},root{"Root",&actor},chest{"Chest",&root},pelvis{"Pelvis",&root},foot{"Foot",&pelvis};
 scene.children={&actor}; actor.children={&root}; root.children={&chest,&pelvis}; pelvis.children={&foot};
 Array source; source.count=1; source.items[0]=&chest; Renderer renderer{&source,nullptr,&root};
 EiemSkinIdentity v6;
 v6.paths={"AuthorRoot/CompletelyRenamedChest"};
 v6.hashes={1};
 v6.sourceCandidates={{{"assets/character/chest.asset","MeshChest",0}}};
 std::vector<EiemLiveSkinSource> liveA={{"assets/character/chest.asset","MeshChest",&renderer,&source}};
 s_eiemLiveSkinSources=&liveA;
 void *v6Out=nullptr; char v6Error[256]{};
 // A complete v6 source palette does not need any matching Transform name.
 assert(EiemResolveMeshBonesFromNativeInstance(v6,&renderer,&v6Out,v6Error,sizeof(v6Error)));
 assert(((Array *)v6Out)->count==1 && ((Array *)v6Out)->items[0]==&chest);
 char error[256]{}; void *out=nullptr;
 Array expandedStorage; expandedStorage.count=2;
 expandedStorage.items[0]=&chest; expandedStorage.items[1]=&foot;
 auto expanded=&expandedStorage;
 assert(EiemPreserveSourceSkinning(&renderer,expanded,error,sizeof(error)) && writes==1);
 assert(EiemPreserveSourceSkinning(&renderer,expanded,error,sizeof(error)) && writes==1 && handles.size()==1);
 Node otherActor{"ActorB",&scene},otherRoot{"Root",&otherActor},otherChest{"Chest",&otherRoot},otherPelvis{"Pelvis",&otherRoot},otherFoot{"Foot",&otherPelvis};
 otherActor.children={&otherRoot}; otherRoot.children={&otherChest,&otherPelvis}; otherPelvis.children={&otherFoot};
 Array otherSource; otherSource.count=1; otherSource.items[0]=&otherChest; Renderer other{&otherSource,nullptr,&otherRoot};
 std::vector<EiemLiveSkinSource> liveB={{"assets/character/chest.asset","MeshChest",&other,&otherSource}};
 s_eiemLiveSkinSources=&liveB;
 v6Out=nullptr;
 assert(EiemResolveMeshBonesFromNativeInstance(v6,&other,&v6Out,v6Error,sizeof(v6Error)));
 assert(((Array *)v6Out)->items[0]==&otherChest);
 assert(((Array *)v6Out)->items[0]!=&chest); // never borrow another model instance
 Array footSource; footSource.count=1; footSource.items[0]=&otherFoot;
 Renderer footRenderer{&footSource,nullptr,&otherRoot};
 liveB.push_back({"assets/character/foot.asset","MeshFoot",&footRenderer,&footSource});
 Array unrelatedSource; unrelatedSource.count=1; unrelatedSource.items[0]=&otherFoot;
 Renderer unrelatedRenderer{&unrelatedSource,nullptr,&otherRoot};
 // Different Mesh sub-assets commonly share one FBX container.  Their local
 // slot numbers are unrelated, so a path-only match must never make this
 // renderer a donor for MeshChest slot 0.
 liveB.push_back({"assets/character/chest.asset","MeshUnrelated",&unrelatedRenderer,&unrelatedSource});
 EiemSkinIdentity donorCandidates;
 donorCandidates.paths={"NPC/RenamedChest","NPC/RenamedFoot"};
 donorCandidates.hashes={1,2};
 donorCandidates.sourceCandidates={
   {{"assets/character/chest.asset","MeshChest",0}},
   {{"assets/character/foot.asset","MeshFoot",0}}};
 void *donorOut=nullptr;
 // v6 uses only original Mesh/slot donors.  The authored names are
 // deliberately unrelated to the native hierarchy.
 assert(EiemResolveMeshBonesFromNativeInstance(
     donorCandidates,&other,&donorOut,v6Error,sizeof(v6Error)));
 assert(((Array *)donorOut)->count==2 &&
        ((Array *)donorOut)->items[0]==&otherChest &&
        ((Array *)donorOut)->items[1]==&otherFoot);
 EiemSkinIdentity missingDonor=donorCandidates;
 missingDonor.sourceCandidates[1]={{"assets/missing.asset","MeshMissing",0}};
 donorOut=nullptr;
 // A missing donor is a hard failure; v6 must never fall back to a guessed
 // name or hierarchy index.
 assert(!EiemResolveMeshBonesFromNativeInstance(
     missingDonor,&other,&donorOut,v6Error,sizeof(v6Error)) && !donorOut);
 EiemSkinIdentity merged;
 merged.paths={"AuthorRoot/Chest","AuthorRoot/Foot"};
 merged.hashes={1,2};
 merged.sourceCandidates={{{"assets/character/chest.asset","MeshChest",0}},{{"assets/character/foot.asset","MeshFoot",0}}};
 void *mergedOut=nullptr;
 assert(EiemResolveMeshBonesFromNativeInstance(merged,&other,&mergedOut,v6Error,sizeof(v6Error)));
 assert(((Array *)mergedOut)->items[0]==&otherChest &&
        ((Array *)mergedOut)->items[1]==&otherFoot); // same numeric slot, different source Mesh
 // A lower LOD may expose only the first local slot while the replacement
 // Mesh intentionally uses the complete LOD0 palette. The resolver must
 // choose the complete LOD0 donor once and return both Transform entries for
 // the lower-LOD target instead of resolving its local array independently.
 Array lod1Storage; lod1Storage.count=1; lod1Storage.items[0]=&otherChest;
 Renderer lod1Renderer{&lod1Storage,nullptr,&otherRoot};
 Array lod0Storage; lod0Storage.count=2;
 lod0Storage.items[0]=&otherChest; lod0Storage.items[1]=&otherFoot;
 Renderer lod0Renderer{&lod0Storage,nullptr,&otherRoot};
 EiemSkinIdentity lod0Replacement;
 lod0Replacement.sourceCandidates={
   {{"assets/character/chest.asset","MeshChest",0}},
   {{"assets/character/chest.asset","MeshChest",1}}};
 std::vector<EiemLiveSkinSource> lodLive={
   {"assets/character/chest.asset","MeshChest",&lod1Renderer,&lod1Storage,1},
   {"assets/character/chest.asset","MeshChest",&lod0Renderer,&lod0Storage,0}};
 EiemSkinPaletteCache lodCache; lodCache.model=&otherActor;
 auto *previousPaletteCache=s_eiemActiveSkinPaletteCache;
 s_eiemActiveSkinPaletteCache=&lodCache;
 s_eiemLiveSkinSources=&lodLive;
 void *lodOut=nullptr;
 assert(EiemResolveMeshBonesFromNativeInstance(
     lod0Replacement,&lod1Renderer,&lodOut,v6Error,sizeof(v6Error)));
 assert(((Array *)lodOut)->count==2 &&
        ((Array *)lodOut)->items[0]==&otherChest &&
        ((Array *)lodOut)->items[1]==&otherFoot);
 assert(lodCache.entries.size()==1 && !lodCache.entries[0].failed &&
        lodCache.entries[0].providerRenderer==&lod0Renderer &&
        lodCache.entries[0].providerLod==0);
 s_eiemActiveSkinPaletteCache=previousPaletteCache;
 // When a replacement cloth Mesh is applied to LOD1, the same skinningRoot
 // may also expose body and cloth02 LOD0 donors with larger local palettes.
 // The donor must stay within the target Mesh family, otherwise a breast slot
 // can resolve to another branch's Transform even though every pointer is
 // alive and the resulting palette is structurally valid.
 Node bodyBone{"BodyBreast"}, cloth02Bone{"Cloth02Breast"}, cloth03Bone{"Cloth03Breast"};
 Array targetLod1Bones; targetLod1Bones.count=1; targetLod1Bones.items[0]=&cloth03Bone;
 Array bodyLod0Bones; bodyLod0Bones.count=2; bodyLod0Bones.items[0]=&bodyBone;
 Array cloth02Lod0Bones; cloth02Lod0Bones.count=3; cloth02Lod0Bones.items[0]=&cloth02Bone;
 Array cloth03Lod0Bones; cloth03Lod0Bones.count=4; cloth03Lod0Bones.items[0]=&cloth03Bone;
 Renderer clothTarget{&targetLod1Bones,nullptr,&otherRoot};
 Renderer bodyDonor{&bodyLod0Bones,nullptr,&otherRoot};
 Renderer cloth02Donor{&cloth02Lod0Bones,nullptr,&otherRoot};
 Renderer cloth03Donor{&cloth03Lod0Bones,nullptr,&otherRoot};
 identityAssets[&clothTarget]="S_actor_lizhiyan_cloth_03_lod1";
 EiemSkinIdentity familyReplacement;
 familyReplacement.sourceCandidates={{{"assets/body.asset","S_actor_lizhiyan_body_01_lod0",0},
                                      {"assets/cloth02.asset","S_actor_lizhiyan_cloth_02_lod0",0},
                                      {"assets/cloth03.asset","S_actor_lizhiyan_cloth_03_lod0",0}}};
 std::vector<EiemLiveSkinSource> familyLive={
   {"assets/body.asset","S_actor_lizhiyan_body_01_lod0",&bodyDonor,&bodyLod0Bones,0},
   {"assets/cloth02.asset","S_actor_lizhiyan_cloth_02_lod0",&cloth02Donor,&cloth02Lod0Bones,0},
   {"assets/cloth03.asset","S_actor_lizhiyan_cloth_03_lod0",&cloth03Donor,&cloth03Lod0Bones,0}};
 EiemSkinPaletteCache familyCache; familyCache.model=&otherActor;
 previousPaletteCache=s_eiemActiveSkinPaletteCache;
 s_eiemActiveSkinPaletteCache=&familyCache;
 s_eiemLiveSkinSources=&familyLive;
 void *familyOut=nullptr;
 assert(EiemResolveMeshBonesFromNativeInstance(
     familyReplacement,&clothTarget,&familyOut,v6Error,sizeof(v6Error)));
 assert(((Array *)familyOut)->count==1 &&
        ((Array *)familyOut)->items[0]==&cloth03Bone);
 s_eiemActiveSkinPaletteCache=previousPaletteCache;
 identityAssets.clear();
 // Two native renderers may expose the same source Mesh/slot while belonging
 // to different PFB instances. The donor with the wrong rootBone/skinningRoot
 // must be ignored even when it appears first in the live-source snapshot.
 Node contextRootA{"ContextRootA"}, contextRootB{"ContextRootB"},
     contextSkinA{"ContextSkinA"}, contextSkinB{"ContextSkinB"},
     contextBoneWrong{"ContextWrongBone"}, contextBoneRight{"ContextRightBone"};
 Array contextTargetSource; contextTargetSource.count=1;
 contextTargetSource.items[0]=&contextBoneRight;
 Renderer contextTarget{&contextTargetSource,&contextRootA,&contextSkinA};
 Array contextWrongSource; contextWrongSource.count=1;
 contextWrongSource.items[0]=&contextBoneWrong;
 Renderer contextWrong{&contextWrongSource,&contextRootB,&contextSkinB};
 Array contextRightSource; contextRightSource.count=1;
 contextRightSource.items[0]=&contextBoneRight;
 Renderer contextRight{&contextRightSource,&contextRootA,&contextSkinA};
 std::vector<EiemLiveSkinSource> contextLive={
   {"assets/character/chest.asset","MeshChest",&contextWrong,&contextWrongSource},
   {"assets/character/chest.asset","MeshChest",&contextRight,&contextRightSource}};
 s_eiemLiveSkinSources=&contextLive;
 void *contextOut=nullptr;
 assert(EiemResolveMeshBonesFromNativeInstance(
     v6,&contextTarget,&contextOut,v6Error,sizeof(v6Error)));
 assert(((Array *)contextOut)->count==1 &&
        ((Array *)contextOut)->items[0]==&contextBoneRight);
 Node extra{"Extra",&otherPelvis}; otherPelvis.children={&otherFoot,&extra};
 EiemSkinIdentity hybrid;
 // The source Renderer exposes only slot 0 at this assembly boundary. Slot 1
 // is authored against the same full palette but is not present in its local
 // bones[]. A missing late source slot must not discard the exact mapping for
 // slot 0 and remap that slot by its (deliberately misleading) name path.
 hybrid.paths={"Root/Pelvis/Foot","Root/Pelvis/Extra"};
 hybrid.hashes={1,2};

 hybrid.sourceCandidates={{{"assets/character/chest.asset","MeshChest",0}},{{"assets/character/chest.asset","MeshChest",1}}};
 Array hybridNative; hybridNative.count=2;
 hybridNative.items[0]=&otherChest; hybridNative.items[1]=&otherFoot;
 Renderer hybridRenderer{&hybridNative,nullptr,&otherRoot};
 s_eiemLiveSkinSources=&liveB;
 void *hybridOut=nullptr;
 // A complete source table is authoritative. If a source Mesh/slot is not
 // present in this model instance, do not guess by a hierarchy index/name.
 assert(!EiemResolveMeshBonesFromNativeInstance(
     hybrid,&hybridRenderer,&hybridOut,v6Error,sizeof(v6Error)) && !hybridOut);
 foot.alive=false;
 assert(!EiemPreserveSourceSkinning(&renderer,expanded,error,sizeof(error)) && writes==1 && handles.size()==1);
 foot.alive=true; failSetter=true; renderer.bones=&source;
 assert(!EiemPreserveSourceSkinning(&renderer,expanded,error,sizeof(error)) && renderer.bones==&source && handles.size()==1);
 il2cpp_gchandle_free(s_eiemOverrides[0].replacementBonesHandle); assert(handles.empty());
}
'''

class SkinRuntimeTests(unittest.TestCase):
    def test_actual_resolver_and_assignment(self):
        if not shutil.which('cl'): self.skipTest('Requires MSVC')
        trace=read_runtime_source(ROOT)
        funcs='\n'.join(function(trace,s) for s in [
            'static bool EiemResolveMeshBonesFromNativeInstance(',
            'static bool EiemPreserveSourceSkinning('])
        with tempfile.TemporaryDirectory(prefix='eiem-skin-runtime-') as directory:
            folder=Path(directory); cpp=folder/'skin.cpp'; cpp.write_text(SOURCE.replace('// FUNCTIONS',funcs),encoding='utf-8'); exe=folder/'skin.exe'
            build=subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/utf-8',f'/I{ROOT/"src"}',str(cpp),f'/Fe{exe}'],cwd=folder,capture_output=True,text=True,encoding='utf-8',errors='replace')
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,encoding="utf-8",errors="replace")
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

if __name__=='__main__': unittest.main()
