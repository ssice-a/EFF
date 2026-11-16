#pragma once
#include "eiem_author_transform_hierarchy.h"
// Only synthetic author objects are animated. The game skeleton and Animator
// remain owned by the game. Playback time comes from RuntimeControls::Player.
namespace RuntimeAuthorTransforms {
using RuntimeShapes::Ref;using RuntimeShapes::Call;
struct Api {void *go=nullptr,*ctor=nullptr,*add=nullptr,*sample=nullptr,*length=nullptr,*destroy=nullptr;Ref animator,clip;} api;
bool bound=false;
void Bind(){if(bound)return;size_t n=0;auto assemblies=il2cpp_domain_get_assemblies(il2cpp_domain_get(),&n);
 auto klass=[&](const char* name){auto c=FindClass("UnityEngine",name,assemblies,n);Require(c,std::string("author mount class absent: ")+name);return c;};
 auto method=[](void* c,const char* name,std::initializer_list<const char*> args,const char* result,bool stat=false){auto m=Exact(c,name,args,result,stat);Require(m,std::string("author mount method absent: ")+name);return m;};
 auto type=[](void* c){auto t=Ref::Make(il2cpp_type_get_object(il2cpp_class_get_type(c)),false);Require(t.Get(),"author mount managed type absent");return t;};
 Api next;next.go=klass("GameObject");next.ctor=method(next.go,".ctor",{"System.String"},"System.Void");next.add=method(next.go,"AddComponent",{"System.Type"},"UnityEngine.Component");
 next.animator=type(klass("Animator"));auto clip=klass("AnimationClip");next.clip=type(clip);next.sample=method(clip,"SampleAnimation",{"UnityEngine.GameObject","System.Single"},"System.Void");next.length=method(clip,"get_length",{},"System.Single");
 next.destroy=method(klass("Object"),"DestroyImmediate",{"UnityEngine.Object","System.Boolean"},"System.Void",true);api=std::move(next);bound=true;
}
void Destroy(const Ref& object){if(!RuntimeShapes::Alive(object.Get()))return;unsigned char assets=0;void* args[]={object.Get(),&assets};Call(api.destroy,nullptr,args);}
std::pair<Ref,Ref> Create(const std::string& name,void* parent){
 auto go=Ref::Make(il2cpp_object_new(api.go),false);Require(go.Get(),"author mount allocation failed");
 try{auto label=Ref::Make(il2cpp_string_new(name.c_str()),false);void* args[]={label.Get()};Call(api.ctor,go.Get(),args);
  auto transform=Ref::Make(Call(FullResourceReload::api.goTransform,go.Get()),false);Require(RuntimeShapes::Alive(transform.Get()),"author mount Transform absent");
  unsigned char keep=0;void* attach[]={parent,&keep};Call(EffectDebug::api.setParent,transform.Get(),attach);return {go,transform};
 }catch(...){try{Destroy(go);}catch(...){}throw;}
}
void Transform(void* target,const AuthorTransformNode& n){void* args[]={const_cast<float*>(n.position.data())};Call(EffectDebug::api.position,target,args);args[0]=const_cast<float*>(n.rotation.data());Call(EffectDebug::api.rotation,target,args);args[0]=const_cast<float*>(n.scale.data());Call(EffectDebug::api.scale,target,args);}
// This basis is outside every switchable LOD branch. Preserve each local TRS
// separately: multiplying rotated/nonuniform scales into one TRS loses shear.
struct Basis {Ref owner,root,tip;~Basis(){if(RuntimeShapes::managedAlive&&root.Get())try{Destroy(root);}catch(const std::exception& e){static auto* quarantine=new std::vector<Ref>();quarantine->push_back(root);Log("[AUTHOR-LOD-BASIS-RELEASE-GAP] reason=%s",e.what());}}};
using BasisKey=std::tuple<void*,std::string,void*,std::string>;
std::map<BasisKey,std::weak_ptr<Basis>> bases;
std::shared_ptr<Basis> AcquireBasis(void* owner,void* parent,const Package& package,const EffectAttachment& effect){
 Require(RuntimeShapes::Main()&&effect.lodAnchor&&!effect.lodAnchor->nodes.empty(),"effect LOD basis declaration absent");Bind();
 BasisKey key{owner,package.id,parent,EffectLodIdentity(effect)};
 if(auto previous=bases[key].lock())if(previous->owner.Get()==owner&&RuntimeShapes::Alive(previous->root.Get()))return previous;
 auto basis=std::make_shared<Basis>();basis->owner=Ref::Make(owner,true);
 for(size_t i=0;i<effect.lodAnchor->nodes.size();++i){auto node=Create("EFF_LodBasis_"+std::to_string(i),i?basis->tip.Get():parent);if(!i)basis->root=node.first;basis->tip=node.second;Transform(node.second.Get(),effect.lodAnchor->nodes[i]);}
 bases[key]=basis;return basis;
}
struct Clip {Ref bundle,asset;std::string assetPath;float duration=0;~Clip(){if(RuntimeShapes::managedAlive&&bundle.Get())try{NativeBundle::Unload(bundle.Get(),true);}catch(const std::exception& e){Log("[AUTHOR-CLIP-RELEASE-GAP] reason=%s",e.what());}}};
std::map<std::string,std::weak_ptr<Clip>> clips;
std::shared_ptr<Clip> Load(const ShapeAnimation::NativeOutput& output,double duration){Require(bool(output.open),"native Animation has no generation input");auto view=output.open();Require(view&&view->Size()<=64*1024*1024,"native Animation input exceeds budget");
 auto key=EiemInputSpanView::Digest(*view);auto previous=clips.find(key);if(previous!=clips.end())if(auto p=previous->second.lock()){Require(p->assetPath==output.asset&&std::abs(p->duration-duration)<=.001,"shared native Animation asset/timeline differs");return p;}
 auto p=std::make_shared<Clip>();p->assetPath=output.asset;p->bundle=Ref::Make(NativeBundle::LoadView(view,0,false),false);Require(RuntimeShapes::Alive(p->bundle.Get()),"native Animation bundle unavailable");
 auto label=Ref::Make(il2cpp_string_new(output.asset.c_str()),false);void* args[]={label.Get(),api.clip.Get()};p->asset=Ref::Make(Call(FullResourceReload::api.assetSync,p->bundle.Get(),args),false);
 Require(RuntimeShapes::Alive(p->asset.Get()),"native Animation asset absent");p->duration=RuntimeShapes::Scalar<float>(api.length,p->asset.Get());Require(std::isfinite(p->duration)&&std::abs(p->duration-duration)<=.001,"native Animation duration differs from shared timeline");clips[key]=p;return p;
}
struct Tree {
 Ref owner,base,root;std::string package,id,signature;std::shared_ptr<const AuthorTransformHierarchy> declaration;
 std::map<std::string,Ref> nodes;std::map<std::string,std::shared_ptr<Clip>> loaded;std::set<std::string> failed;
 uint64_t checkedRevision=~uint64_t(0);bool compatible=true;bool posed=false;std::string lastClip;double lastTime=-1;
 ~Tree(){if(RuntimeShapes::managedAlive&&root.Get())try{Destroy(root);}catch(const std::exception& e){
   // Quarantine consumers and their native Clip pins together if destruction failed.
   static auto* quarantine=new std::vector<std::pair<Ref,std::map<std::string,std::shared_ptr<Clip>>>>();quarantine->emplace_back(root,std::move(loaded));Log("[AUTHOR-MOUNT-RELEASE-GAP] reason=%s resourcesRetained=1",e.what());}}
 void Restore(){for(const auto& n:declaration->nodes){auto t=nodes.at(n.path).Get();if(!RuntimeShapes::Alive(t))continue;void* p[]={const_cast<float*>(n.position.data())};Call(EffectDebug::api.position,t,p);p[0]=const_cast<float*>(n.rotation.data());Call(EffectDebug::api.rotation,t,p);p[0]=const_cast<float*>(n.scale.data());Call(EffectDebug::api.scale,t,p);}posed=false;lastClip.clear();lastTime=-1;}
};
using Key=std::tuple<void*,std::string,void*,std::string>;
std::map<Key,std::weak_ptr<Tree>> trees;
std::string Signature(const Package& p,const AuthorTransformHierarchy& graph){auto s=EiemAuthorTransforms::Identity(graph);for(const auto& row:p.shapeAnimations)for(const auto& o:row.second.nativeOutputs)if(o.hierarchy==graph.id)s+="/"+row.first+"/"+std::to_string(row.second.duration)+"/"+o.file+"/"+o.asset+"/"+o.revision;return s;}
std::shared_ptr<Tree> Acquire(void* owner,void* base,const Package& package,const std::shared_ptr<const AuthorTransformHierarchy>& graph){
 Require(RuntimeShapes::Main()&&graph,"author mount requires main thread and declaration");Bind();auto signature=Signature(package,*graph);Key key{owner,package.id,base,signature};if(auto previous=trees[key].lock())if(previous->owner.Get()==owner&&RuntimeShapes::Alive(previous->root.Get()))return previous;
 auto tree=std::make_shared<Tree>();tree->owner=Ref::Make(owner,true);tree->base=Ref::Make(base,false);tree->package=package.id;tree->id=graph->id;tree->signature=signature;tree->declaration=graph;
 auto root=Create("EFF_Mount_"+graph->id,base);tree->root=root.first;
 for(const auto& node:graph->nodes){auto at=node.path.rfind('/');auto parent=at==node.path.npos?root.second.Get():tree->nodes.at(node.path.substr(0,at)).Get();auto next=Create(node.path.substr(at==node.path.npos?0:at+1),parent);tree->nodes.emplace(node.path,next.second);}
 tree->Restore();bool animation=false;for(const auto& row:package.shapeAnimations)for(const auto& output:row.second.nativeOutputs)animation|=output.hierarchy==graph->id;
 if(animation){void* args[]={api.animator.Get()};Require(Call(api.add,tree->root.Get(),args),"author mount Animator absent");}
 trees[key]=tree;Log("[AUTHOR-MOUNT-READY] package=%s hierarchy=%s owner=%p base=%p nodes=%zu animator=%d",package.id.c_str(),graph->id.c_str(),owner,base,graph->nodes.size(),int(animation));return tree;
}
void Frame(){if(!RuntimeShapes::Main())return;for(auto i=trees.begin();i!=trees.end();){auto tree=i->second.lock();if(!tree){i=trees.erase(i);continue;}++i;
  if(!RuntimeShapes::Alive(tree->owner.Get())||!RuntimeShapes::Alive(tree->root.Get()))continue;
  const Package* package=nullptr;if(RuntimeControls::controlGraph)for(const auto& p:*RuntimeControls::controlGraph)if(p.id==tree->package)package=&p;
  auto player=RuntimeControls::animations.find(tree->package);if(!package||player==RuntimeControls::animations.end())continue;
  if(tree->checkedRevision!=RuntimeControls::revision){tree->compatible=Signature(*package,*tree->declaration)==tree->signature;tree->checkedRevision=RuntimeControls::revision;}if(!tree->compatible)continue;
  const ShapeAnimation* selected=nullptr;const ShapeAnimation::NativeOutput* output=nullptr;double time=0;
  for(const auto& row:package->shapeAnimations){auto state=player->second.clips.find(row.first);if(state==player->second.clips.end()||!state->second.playing)continue;for(const auto& o:row.second.nativeOutputs)if(o.hierarchy==tree->id){selected=&row.second;output=&o;time=state->second.time;}}
  try{if(!selected){if(tree->posed)tree->Restore();continue;}if(tree->failed.count(selected->id))continue;
   auto& clip=tree->loaded[selected->id];if(!clip)clip=Load(*output,selected->duration);if(tree->lastClip==selected->id&&tree->lastTime==time)continue;
   float value=float(time);void* args[]={tree->root.Get(),&value};Call(api.sample,clip->asset.Get(),args);tree->lastClip=selected->id;tree->lastTime=time;tree->posed=true;
  }catch(const std::exception& e){if(selected)tree->failed.insert(selected->id);Log("[AUTHOR-MOUNT-ANIMATION-GAP] package=%s hierarchy=%s reason=%s retry=next-generation",tree->package.c_str(),tree->id.c_str(),e.what());}
 }
 for(auto i=clips.begin();i!=clips.end();)if(i->second.expired())i=clips.erase(i);else ++i;
 for(auto i=bases.begin();i!=bases.end();)if(i->second.expired())i=bases.erase(i);else ++i;
}
}
