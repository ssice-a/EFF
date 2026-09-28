"""Compile the real Skeleton reader/Unity adapter; no game or user assets required."""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
from test_mod_controls import function

ROOT = Path(__file__).resolve().parents[1]

def skeleton_bytes(version=2, named=False):
    def string(value):
        data=value.encode(); assert len(data)<128
        return bytes([len(data)])+data
    nodes=[('',-1),('Rig',0),('Rig/Pelvis',1),('Rig/Unused',1),('Rig/Pelvis/Extra',2)]
    if named:
        nodes=[(p, parent-1) for p,parent in nodes[1:]]
    data=b'EFFSKEL\0'+struct.pack('<i',version)+string('unity-y-up-left-handed')+struct.pack('<i',len(nodes))
    for i,(path,parent) in enumerate(nodes):
        new=i==len(nodes)-1
        data+=string(path)+struct.pack('<i3f4f3f',parent,*( (.2,.3,.4) if new else (0,0,0)),
                                     *( (0,0,.6,.8) if new else (0,0,0,1)),1,1,1)
    data+=struct.pack('<ii',0,-1)
    if version==2:
        data+=struct.pack('<i',len(nodes))+bytes([1]*(len(nodes)-1)+[0])
    return data

SOURCE=r'''
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "eiem_skin_binding.h"
#include "eiem_file_io.h"
// READER
struct Array { char pad[24]{}; size_t count=0; void *items[32]{}; };
struct Node { std::string name; Node *parent=nullptr; std::vector<Node *> children; bool alive=true; float p[3]{},q[4]{0,0,0,1},s[3]{1,1,1}; };
struct Renderer { Array *bones; };
struct Box { char pad[16]{}; int value=0; } box;
static constexpr size_t IL2CPP_ARRAY_DATA=32;
static int methods[20], creates=0, destroys=0, sourceWrites=0;
static bool unityThread=true, failPosition=false, failDestroy=false;
static void *g_smr_get_bones=&methods[0],*g_transform_get_parent=&methods[1],*g_object_get_name=&methods[2];
static void *g_transform_get_childCount=&methods[3],*g_transform_GetChild=&methods[4],*g_transformClass=&methods[5];
static void *g_gameObjectClass=&methods[6],*g_gameObject_ctor=&methods[7],*g_gameObject_get_transform=&methods[8];
static void *g_transform_set_parent=&methods[9],*g_object_destroy=&methods[10];
static void *g_transform_set_localPosition=&methods[11],*g_transform_set_localRotation=&methods[12],*g_transform_set_localScale=&methods[13];
static void *g_transform_get_localPosition=&methods[14],*g_transform_get_localRotation=&methods[15],*g_transform_get_localScale=&methods[16];
struct TrsBox { char pad[16]{}; float value[4]{}; } trsBox;
static std::vector<std::unique_ptr<Node>> created;
static std::vector<std::unique_ptr<Array>> arrays;
static std::vector<std::unique_ptr<std::string>> strings;
static auto il2cpp_object_new=+[](void *)->void * { created.push_back(std::make_unique<Node>()); created.back()->alive=false; ++creates; return created.back().get(); };
static auto il2cpp_string_new=+[](const char *s)->void * { strings.push_back(std::make_unique<std::string>(s)); return strings.back().get(); };
static auto il2cpp_array_new=+[](void *,size_t n)->void * { auto a=std::make_unique<Array>(); a->count=n; arrays.push_back(std::move(a)); return arrays.back().get(); };
static bool EiemOnUnityThread() { return unityThread; }
static int EiemNativeObjectStatus(void *p) { return p && ((Node *)p)->alive ? 1:0; }
struct EiemUnityRef {
 void *p=nullptr;
 static EiemUnityRef Capture(void *p,bool=true) { return {p}; }
 explicit operator bool() const { return p!=nullptr; }
 void *Target() const { return p; }
 int Status() const { return EiemNativeObjectStatus(p); }
};
static size_t EiemManagedArrayLength(void *p) { return p ? ((Array *)p)->count : 0; }
static void ReadStrUtf8(void *p,char *out,size_t n) { strncpy_s(out,n,((std::string *)p)->c_str(),_TRUNCATE); }
static void Log(const char *,...) {}
static bool InvokeChecked(void *m,void *p,void **args,void **out) {
 *out=nullptr;
 if(m==g_smr_get_bones) { *out=((Renderer *)p)->bones; return true; }
 auto n=(Node *)p;
 if(m==g_transform_get_parent) *out=n->parent;
 else if(m==g_object_get_name) *out=&n->name;
 else if(m==g_transform_get_childCount) { box.value=(int)n->children.size(); *out=&box; }
 else if(m==g_transform_GetChild) *out=n->children.at(*(int *)args[0]);
 else if(m==g_gameObject_get_transform) *out=n;
 else if(m==g_gameObject_ctor) { n->name=*(std::string *)args[0]; n->alive=true; }
 else if(m==g_transform_set_parent) {
   if(n->parent) { auto &v=n->parent->children; v.erase(std::remove(v.begin(),v.end(),n),v.end()); }
   assert(!*(bool *)args[1]); n->parent=(Node *)args[0]; if(n->parent)n->parent->children.push_back(n);
 } else if(m==g_object_destroy) {
   if(failDestroy)return false;
   ((Node *)args[0])->alive=false; ++destroys;
 } else if(m==g_transform_get_localPosition || m==g_transform_get_localRotation || m==g_transform_get_localScale) {
   const float *source=m==g_transform_get_localPosition?n->p:m==g_transform_get_localRotation?n->q:n->s;
   memcpy(trsBox.value,source,m==g_transform_get_localRotation?16:12); *out=&trsBox;
 } else if(m==g_transform_set_localPosition || m==g_transform_set_localRotation || m==g_transform_set_localScale) {
   if(n->name.rfind("EFF_Bone_",0)!=0)++sourceWrites;
   if(m==g_transform_set_localPosition && failPosition)return false;
   memcpy(m==g_transform_set_localPosition?n->p:m==g_transform_set_localRotation?n->q:n->s,args[0],m==g_transform_set_localRotation?16:12);
 } else return false;
 return true;
}
struct EiemModRule { char modPath[256]="A/mod.ini",skeleton[192]="SkeletonTest"; bool hasPhysics=false; };
struct EiemModResource { char modPath[256]{},section[192]{}; };
static std::string disk;
static uint64_t stamp=1;
static bool EiemFindModResource(const char *mod,const char *section,const char *,EiemModResource *r) {
 strcpy_s(r->modPath,mod); strcpy_s(r->section,section); return true;
}
static bool EiemResolveResourceDiskPath(const EiemModResource &,char *p,size_t n) { strcpy_s(p,n,disk.c_str()); return true; }
static uint64_t EiemMeshResourceFileStamp(const char *) { return stamp; }
static constexpr bool kEiemValidationIdentityProbe=false;
#include "eiem_skeleton_runtime.h"
int main(int argc,char **argv) {
 assert(argc==5); disk=argv[1]; std::string error;
 EiemNativeReader file(disk.c_str()); EiemSkeletonDocument doc;
 assert(EiemReadSkeleton(file,doc,error) && doc.nodes.size()==5 && !doc.nodes.back().source);
 auto invalid=doc; invalid.nodes[4].parent=4; assert(!EiemValidateSkeleton(invalid,error));
 invalid=doc; invalid.nodes[4].path=invalid.nodes[3].path; assert(!EiemValidateSkeleton(invalid,error));
 invalid=doc; invalid.nodes[4].rotation[3]=2; assert(!EiemValidateSkeleton(invalid,error));
 invalid=doc; invalid.nodes[4].position[0]=INFINITY; assert(!EiemValidateSkeleton(invalid,error));
 invalid=doc; invalid.nodes[2].source=false; assert(EiemValidateSkeleton(invalid,error)); // valid new subtree
 invalid.nodes[4].source=true; assert(!EiemValidateSkeleton(invalid,error));
 EiemNativeReader truncated(argv[4]); EiemSkeletonDocument untouched=doc;
 assert(!EiemReadSkeleton(truncated,untouched,error) && untouched.nodes.size()==5); // atomic parse

 Node scene{"Scene"},actor{"Actor",&scene},rig{"Rig",&actor},pelvis{"Pelvis",&rig},unused{"Unused",&rig};
 Node partnerA{"EIEM Partner Render",&rig},partnerB{"EIEM Partner Render",&rig};
 scene.children={&actor}; actor.children={&rig};
 // These generated Partner objects intentionally share a name. They are
 // outside the Skeleton resource and must not make source resolution fail.
 rig.children={&pelvis,&unused,&partnerA,&partnerB};
 Array palette; palette.count=1; palette.items[0]=&pelvis; Renderer renderer{&palette},otherPart{&palette};
 EiemModRule rule; char message[256]{}; std::shared_ptr<EiemSkeletonInstance> a,b,c;
 assert(EiemAcquireSkeleton(rule,&renderer,a,message,sizeof(message)) && a->ready && creates==1);
 auto extra=(Node *)a->nodes[4].Target();
 assert(extra->parent==&pelvis && extra->p[0]==.2f && extra->q[2]==.6f && extra->q[3]==.8f);
 assert(sourceWrites==0 && renderer.bones==&palette);
 assert(EiemAcquireSkeleton(rule,&otherPart,b,message,sizeof(message)) && b==a && creates==1);
 EiemSkinIdentity skin; skin.paths={"Rig/Unused","Rig/Pelvis/Extra","Rig/Pelvis"};
 skin.boneIndexPaths={"1","0/0","0"};
 void *bound=nullptr;
 assert(EiemSkeletonMeshBones(skin,*a,&bound,message,sizeof(message)));
 auto mapped=(Array *)bound; assert(mapped->count==3 && mapped->items[0]==&unused && mapped->items[1]==extra && mapped->items[2]==&pelvis);
 skin.paths[0]="Rig/Missing"; skin.boneIndexPaths[0]="0/99";
 assert(!EiemSkeletonMeshBones(skin,*a,&bound,message,sizeof(message)) && !bound);

 // Another Mod can use identical author paths without sharing added nodes.
 strcpy_s(rule.modPath,"B/mod.ini");
 assert(EiemAcquireSkeleton(rule,&renderer,c,message,sizeof(message)) && c!=a);
 assert(c->nodes[4].Target()!=extra && ((Node *)c->nodes[4].Target())->name!=extra->name);
 c.reset(); EiemCollectSkeletonInstances(); assert(destroys==1 && extra->alive);
 strcpy_s(rule.modPath,"A/mod.ini");
 // Same resource, another actor: only resource data may be shared, never Transforms.
 Node actor2{"Actor2",&scene},rig2{"Rig",&actor2},pelvis2{"Pelvis",&rig2},unused2{"Unused",&rig2};
 actor2.children={&rig2}; rig2.children={&pelvis2,&unused2};
 Array palette2; palette2.count=1; palette2.items[0]=&pelvis2; Renderer renderer2{&palette2};
 assert(EiemAcquireSkeleton(rule,&renderer2,c,message,sizeof(message)) && c!=a);
 assert(((Node *)c->nodes[4].Target())->parent==&pelvis2);
 c.reset(); EiemCollectSkeletonInstances(); assert(destroys==2);

 // A changed file starts a private replacement generation immediately. The
 // previous generation stays alive for its existing Renderer/native users,
 // but must reject new consumers instead of making the first F10 replay fail.
 ++stamp;
 assert(EiemAcquireSkeleton(rule,&renderer,c,message,sizeof(message)) && c &&
        c!=a && !a->ready && c->ready && c->nodes[4].Target()!=extra);
 --stamp;
 a.reset(); EiemCollectSkeletonInstances(); assert(extra->alive && destroys==2); // second consumer owns it
 b.reset(); unityThread=false; EiemCollectSkeletonInstances(); assert(extra->alive);
 unityThread=true; failDestroy=true; EiemCollectSkeletonInstances();
 assert(!s_eiemSkeletonInstances[0]->ready && extra->alive && !extra->parent);
 assert(EiemAcquireSkeleton(rule,&renderer,c,message,sizeof(message)) && c->nodes[4].Target()!=extra);
 c.reset(); // new generation never reuses a retired node even if Destroy failed
 failDestroy=false; EiemCollectSkeletonInstances(); assert(!extra->alive && s_eiemSkeletonInstances.empty());

 // A partial failed build remains owned until its native children are retired.
 failPosition=true; assert(!EiemAcquireSkeleton(rule,&renderer,a,message,sizeof(message)) && !a);
 assert(!s_eiemSkeletonInstances[0]->ready);
 EiemCollectSkeletonInstances(); assert(s_eiemSkeletonInstances.empty()); failPosition=false;
 assert(EiemAcquireSkeleton(rule,&renderer,a,message,sizeof(message)));
 Node consumer{"RetiringRenderer"}; EiemRetainSkeletonConsumer(*a,EiemUnityRef::Capture(&consumer));
 auto pending=(Node *)a->nodes[4].Target();
 a.reset(); EiemCollectSkeletonInstances(); assert(pending->alive); // native Destroy has not completed yet
 assert(EiemAcquireSkeleton(rule,&renderer,b,message,sizeof(message)) && b->nodes[4].Target()!=pending);
 consumer.alive=false; EiemCollectSkeletonInstances(); assert(!pending->alive);
 b.reset(); EiemCollectSkeletonInstances();
 // Teardown retention performs no Unity calls and is safe off the Unity thread.
 assert(EiemAcquireSkeleton(rule,&renderer,a,message,sizeof(message)));
 Node pooled{"PooledRenderer"}; auto pooledBone=(Node *)a->nodes[4].Target();
 unityThread=false;
 std::thread release([lease=a,&pooled] { EiemRetainSkeletonConsumer(*lease,EiemUnityRef::Capture(&pooled)); });
 release.join(); unityThread=true; a.reset(); EiemCollectSkeletonInstances(); assert(pooledBone->alive);
 pooled.alive=false; EiemCollectSkeletonInstances(); assert(!pooledBone->alive);
 // Missing original nodes must not be silently fabricated.
 unused.name="Missing"; int before=creates;
 assert(!EiemAcquireSkeleton(rule,&renderer,a,message,sizeof(message)) && creates==before); unused.name="Unused";
 disk=argv[2]; // Obsolete V1 format is rejected before building any nodes.
 assert(!EiemAcquireSkeleton(rule,&renderer,a,message,sizeof(message)) && creates==before);
 disk=argv[3]; // Named-root Skeleton from author/test packages.
 assert(EiemAcquireSkeleton(rule,&renderer,a,message,sizeof(message)) && a->anchor.Target()==&rig);
 a.reset(); EiemCollectSkeletonInstances();
 assert(s_eiemSkeletonInstances.empty() && sourceWrites==0);
 assert(actor.alive && rig.alive && pelvis.alive && unused.alive && creates==destroys);
}
'''

class SkeletonRuntimeTests(unittest.TestCase):
    def test_parser_and_instance_lifecycle(self):
        if not shutil.which('cl'): self.skipTest('Requires MSVC')
        backend=(ROOT/'src/eiem_resource_backend.h').read_text(encoding='utf-8')
        reader=function(backend,'class EiemNativeReader {')+';'
        with tempfile.TemporaryDirectory(prefix='eiem-skeleton-runtime-') as directory:
            folder=Path(directory); cpp=folder/'test.cpp'; exe=folder/'test.exe'
            cpp.write_text(SOURCE.replace('// READER',reader),encoding='utf-8')
            inputs=[folder/name for name in ('new.skeleton','v1.skeleton','named.skeleton','truncated.skeleton')]
            for path,data in zip(inputs,(skeleton_bytes(),skeleton_bytes(1),skeleton_bytes(named=True),skeleton_bytes()[:-1])):
                path.write_bytes(data)
            build=subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/utf-8',f'/I{ROOT/"src"}',str(cpp),f'/Fe{exe}'],cwd=folder,capture_output=True,text=True,encoding='utf-8',errors='replace')
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe),*(str(p) for p in inputs)],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

if __name__=='__main__': unittest.main()
