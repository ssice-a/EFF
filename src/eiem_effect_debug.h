#pragma once

// Included inside Entry, after native Bundle and model owner bindings.
// Native lifecycle is shared by Mod attachments. Only the debug build starts
// the local author pipe; ordinary production does not expose that interface.
namespace RuntimeAttachments {void Reload(const std::string&,const EffectAttachment&);}
namespace EffectDebug {
using RuntimeShapes::Ref;
using Manifest=EiemEffectNative::Manifest;
using FullResourceReload::Call;
enum class Op {Owners,Nodes,Prepare,Play,Stop,Clear,ReloadAttachment};
struct Command {uint64_t id=0,clock=0;Op op=Op::Owners;std::shared_ptr<const Manifest> manifest;int owner=0;std::string path,package;std::shared_ptr<const EffectAttachment> attachment;};
struct Mailbox {
 std::mutex mutex;std::deque<Command> commands;std::map<uint64_t,std::string> results;
 std::atomic<bool> waiting{false};uint64_t sequence=0;std::string status="phase=disabled\n";
 uint64_t Submit(Command c){std::lock_guard<std::mutex> lock(mutex);Require(commands.size()<128,"effect command queue busy");c.id=++sequence;auto id=c.id;commands.push_back(std::move(c));waiting=true;return id;}
 std::deque<Command> Drain(){std::lock_guard<std::mutex> lock(mutex);std::deque<Command> out;out.swap(commands);waiting=false;return out;}
 void Finish(uint64_t id,std::string result){if(!id)return;std::lock_guard<std::mutex> lock(mutex);results[id]=std::move(result);while(results.size()>128)results.erase(results.begin());}
 std::string Result(uint64_t id){std::lock_guard<std::mutex> lock(mutex);auto it=results.find(id);return it==results.end()?"PENDING\n":it->second;}
 void Publish(std::string text){std::lock_guard<std::mutex> lock(mutex);status=std::move(text);}
 std::string Status(){std::lock_guard<std::mutex> lock(mutex);return status;}
} mailbox;
std::string Esc(std::string s){std::string out;for(char c:s){if(c=='\t')out+="\\t";else if(c=='\n'||c=='\r')out+=' ';else out+=c;}return out;}
struct Api {
 void *add=nullptr,*remove=nullptr,*active=nullptr,*activeSelf=nullptr,*activeHierarchy=nullptr,*setParent=nullptr;
 void *play=nullptr,*stop=nullptr,*clear=nullptr,*playOnAwake=nullptr,*allBundles=nullptr;
 void *position=nullptr,*rotation=nullptr,*scale=nullptr,*layer=nullptr,*setLayer=nullptr;
 void *getComponent=nullptr,*renderInUI=nullptr,*setRenderInUI=nullptr;
 Ref particleType,particleRendererType,bundleType;
} api;
std::atomic<bool> enabled{false};
struct Bundle {Ref request,object;bool done=false,owned=false,unloadCalled=false;int id=0;};
enum class Phase {Lease,Bundles,Asset,Ready,Rejected};
struct Generation {
 std::shared_ptr<const Manifest> manifest;Phase phase=Phase::Lease;uint64_t ticket=0,clock=0;
 FullResourceReload::Lease lease;std::vector<Bundle> bundles;Ref assetRequest,prefab;
 std::set<int> priorBundleIds;bool assetDone=false,quarantined=false,finished=false;
 size_t externalPins=0;bool retryRequested=false;std::string failure;
};
struct Instance {
 std::shared_ptr<Generation> generation;Ref object,owner,parent,manager;int ownerId=0;
 std::vector<Ref> particles,layerObjects;int layer=-1;
 uint64_t observationClock=0;unsigned observationStep=0;
 bool registrationAttempted=false,removed=false,destroyAttempted=false,quarantined=false;
};
#include "eiem_effect_observation.h"
std::shared_ptr<Generation> candidate,ready;
std::shared_ptr<Instance> playing;
std::vector<std::shared_ptr<Generation>> generations;
std::vector<std::shared_ptr<Instance>> retiredInstances;
bool firstFramePending=false;uint64_t firstFrameClock=0;
bool nativeWork=false;

void Status(){
 std::ostringstream out;out<<"OK\tphase="<<(candidate?"preparing":playing?"playing":ready?"ready":"idle")
  <<"\tprepared="<<(ready?ready->manifest->key:"")<<"\tplaying="<<(playing?playing->generation->manifest->key:"")
  <<"\towner="<<(playing?playing->ownerId:0)<<"\tretiring="<<retiredInstances.size()<<"\n";mailbox.Publish(out.str());
}
std::map<int,Ref> Owners(){
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::EffectOwners);
 std::map<int,Ref> roots;
 for(const auto& p:FullResourceReload::liveParts){auto root=p.second.root.Get(),part=p.second.part.Get();
  if(part&&RuntimeShapes::Alive(root)&&FullResourceReload::ReadObject(part,"m_model","UnityEngine.GameObject")==root)roots.emplace(FullResourceReload::Id(root),p.second.root);
 }
 for(const auto& p:FullResourceReload::liveUIModels){auto root=p.second.root.Get(),loader=p.second.loader.Get();
  if(loader&&RuntimeShapes::Alive(root)&&FullResourceReload::UIOwns(loader,root))roots.emplace(FullResourceReload::Id(root),p.second.root);
 }
 std::map<int,Ref> helperRoots;
 for(auto it=FullResourceReload::liveHelperModels.begin();it!=FullResourceReload::liveHelperModels.end();){auto helper=it->second.helper.Get(),root=it->second.root.Get();
  if(!RuntimeShapes::Alive(helper)||!RuntimeShapes::Alive(root)||(FullResourceReload::api.helperInited&&!RuntimeShapes::Scalar<unsigned char>(FullResourceReload::api.helperInited,helper))){it=FullResourceReload::liveHelperModels.erase(it);++FullResourceReload::ownerRevision;continue;}
  helperRoots.emplace(FullResourceReload::Id(root),it->second.root);++it;
 }
 for(const auto& candidate:helperRoots){bool covered=false;
  for(const auto& known:roots)if(FullResourceReload::RootContains(known.second.Get(),candidate.second.Get())){covered=true;break;}
  if(!covered)for(const auto& known:helperRoots)if(known.first!=candidate.first&&FullResourceReload::RootContains(known.second.Get(),candidate.second.Get())){covered=true;break;}
  if(!covered)roots.emplace(candidate);
 }return roots;
}
Ref Owner(int id){auto owners=Owners();auto it=owners.find(id);Require(it!=owners.end(),"effect target model instance is no longer owned by the game");return it->second;}
std::vector<Ref> Particles(void* root){unsigned char inactive=1;void* args[]={api.particleType.Get(),&inactive};
 auto array=Ref::Make(Call(FullResourceReload::api.components,root,args),false);std::vector<Ref> out;
 for(auto p:FullResourceReload::ReferenceArray(array.Get())){Require(RuntimeShapes::Alive(p),"effect ParticleSystem retired");out.push_back(Ref::Make(p,false));}return out;
}
void Active(void* root,bool value){unsigned char v=value;void* args[]={&v};Call(api.active,root,args);}
void Retire(const std::shared_ptr<Instance>& instance){
 if(!instance)return;EiemLoadPerformance::Scope performance(EiemLoadPerformance::EffectRetire);performance.hit=true;EiemLoadPerformance::Add(EiemLoadPerformance::Operation::EffectRetire);retiredInstances.push_back(instance);nativeWork=true;
 // Keep generation resources until deferred Destroy really retires the native
 // object. All cleanup steps are attempted, even after a managed failure.
 auto attempt=[&](auto action){try{action();}catch(const std::exception& e){instance->quarantined=true;Log("[EFFECT-NATIVE-CLEANUP-GAP] reason=%s generationRetained=1",e.what());}};
 attempt([&]{for(const auto& p:instance->particles)if(RuntimeShapes::Alive(p.Get())){unsigned char children=0;void* args[]={&children};Call(api.stop,p.Get(),args);Call(api.clear,p.Get(),args);}});
 attempt([&]{if(RuntimeShapes::Alive(instance->object.Get()))Active(instance->object.Get(),false);});
 attempt([&]{if(instance->registrationAttempted&&!instance->removed){instance->removed=true;void* args[]={instance->object.Get()};Call(api.remove,instance->manager.Get(),args);}});
 attempt([&]{if(RuntimeShapes::Alive(instance->object.Get())){auto transform=Ref::Make(Call(FullResourceReload::api.goTransform,instance->object.Get()),false);unsigned char world=0;void* args[]={nullptr,&world};Call(api.setParent,transform.Get(),args);}});
 attempt([&]{if(!instance->quarantined&&!instance->destroyAttempted&&RuntimeShapes::Alive(instance->object.Get())){instance->destroyAttempted=true;void* args[]={instance->object.Get()};Call(RuntimeShapes::api.destroy,nullptr,args);}});
}
void Stop(){auto old=std::move(playing);Retire(old);firstFramePending=false;}
void OwnerReleasing(void* root) noexcept {
 if(!enabled||!root||!playing)return;
 try{if(!RuntimeShapes::Main())return;if(playing->owner.Get()==root){Stop();Status();Log("[EFFECT-NATIVE-OWNER-RELEASE] detachedBeforeGamePool=1");}}
 catch(const std::exception& e){Log("[EFFECT-NATIVE-CLEANUP-GAP] ownerRelease=%s",e.what());}
}
void Prepare(const Command& command){
 // Exact immutable native inputs share a resident generation. Repeating a
 // command never tries to load a second copy of the same CAB namespace.
 std::shared_ptr<Generation> reuse;
 for(auto& g:generations)if(!g->finished&&g->manifest->key==command.manifest->key){
  Require(!g->quarantined&&g->phase==Phase::Ready,"effect namespace still loading/retiring; await its result or clear first");
  bool equal=g->manifest->prefab==command.manifest->prefab&&g->manifest->rootName==command.manifest->rootName&&g->manifest->root==command.manifest->root&&g->manifest->particles==command.manifest->particles&&g->manifest->bundles.size()==command.manifest->bundles.size();
  for(size_t i=0;equal&&i<g->manifest->bundles.size();++i)equal=g->manifest->bundles[i].hash==command.manifest->bundles[i].hash&&g->manifest->bundles[i].dependencies==command.manifest->bundles[i].dependencies;
  Require(equal,"effect namespace key reused for different native inputs");reuse=g;break;
 }
 if(candidate){mailbox.Finish(candidate->ticket,"ERR\tsuperseded by a newer PREPARE\n");candidate->phase=Phase::Rejected;candidate.reset();}
 if(reuse){ready=reuse;mailbox.Finish(command.id,"OK\tready\treused=1\n");Status();return;}
 auto g=std::make_shared<Generation>();g->manifest=command.manifest;g->ticket=command.id;g->clock=command.clock;g->bundles.resize(g->manifest->bundles.size());
 generations.push_back(g);candidate=g;Status();
}
bool Step(Generation& g){
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::EffectLoad);performance.hit=true;
 const auto& native=FullResourceReload::api;
 if(g.phase==Phase::Lease){
  if(!g.lease.handle.Get()){
   g.lease.pathHash=FullResourceReload::Path(g.manifest->prefab);void* args[]={FullResourceReload::Self(g.lease.pathHash,native.pathClass),FullResourceReload::prefabType.Get(),FullResourceReload::api.rootCategory.Data(),FullResourceReload::api.priority.Data()};
   g.lease.handle=Ref::Make(Call(native.load,nullptr,args),false);Require(g.lease.handle.Get(),"effect source lease returned null");return true;
  }
  auto self=FullResourceReload::Self(g.lease.handle,native.handleClass);if(!RuntimeShapes::Scalar<unsigned char>(native.done,self))return false;g.lease.done=true;
  Require(!RuntimeShapes::Scalar<unsigned char>(native.error,self),"effect original source lease failed");
  struct SourceRead {bool old=FullResourceReload::readingSource;SourceRead(){FullResourceReload::readingSource=true;}~SourceRead(){FullResourceReload::readingSource=old;}} sourceRead;
  g.lease.source=Ref::Make(Call(native.get,self),false);Require(RuntimeShapes::Alive(g.lease.source.Get()),"effect original prefab unavailable");
  // Public type-limited Bundle inventory once per preparation, no scene query.
  void* args[]={api.bundleType.Get()};auto array=Ref::Make(Call(api.allBundles,nullptr,args),false);
  for(auto bundle:FullResourceReload::ReferenceArray(array.Get()))if(RuntimeShapes::Alive(bundle))g.priorBundleIds.insert(FullResourceReload::Id(bundle));
  g.phase=Phase::Bundles;return true;
 }
 if(g.phase==Phase::Bundles){
  for(size_t i=0;i<g.bundles.size();++i){auto& b=g.bundles[i];if(b.done)continue;
   if(!b.request.Get()){const auto& source=g.manifest->bundles[i];Require(EiemEffectNative::Archive(source.Data(),source.Size()),"effect requires the game native archive envelope; rebuild with the updated AnimeStudio");b.request=Ref::Make(NativeBundle::LoadBytes(source.Data(),source.Size(),source.Owner(),0,true),false);Require(b.request.Get(),"effect async Bundle submission returned null");return true;}
   if(!RuntimeShapes::Scalar<unsigned char>(NativeBundle::api.done,b.request.Get()))continue;
   b.object=Ref::Make(Call(NativeBundle::api.bundle,b.request.Get()),false);b.done=true;
   Require(RuntimeShapes::Alive(b.object.Get()),"effect native Bundle load returned null: bundle="+std::to_string(i)+" file="+g.manifest->bundles[i].file);b.id=FullResourceReload::Id(b.object.Get());
   Require(!g.priorBundleIds.count(b.id),"effect native Bundle aliases a pre-existing container");
   for(const auto& other:g.bundles)if(&other!=&b&&other.done&&other.object.Get())Require(other.id!=b.id,"effect two inputs alias a native Bundle");
   b.owned=true;return true;
  }
  if(std::all_of(g.bundles.begin(),g.bundles.end(),[](const auto& b){return b.done;})){g.phase=Phase::Asset;return true;}return false;
 }
 if(g.phase==Phase::Asset){
  if(!g.assetRequest.Get()){auto label=Ref::Make(il2cpp_string_new(g.manifest->prefab.c_str()),false);void* args[]={label.Get(),FullResourceReload::prefabType.Get()};g.assetRequest=Ref::Make(Call(NativeBundle::api.assetLoad,g.bundles.at(g.manifest->root).object.Get(),args),false);Require(g.assetRequest.Get(),"effect async asset request returned null");return true;}
  if(!RuntimeShapes::Scalar<unsigned char>(NativeBundle::api.done,g.assetRequest.Get()))return false;
  g.prefab=Ref::Make(Call(NativeBundle::api.result,g.assetRequest.Get()),false);g.assetDone=true;
  Require(RuntimeShapes::Alive(g.prefab.Get())&&g.prefab.Get()!=g.lease.source.Get(),"effect prefab missing or aliases original");
  Require(String(Call(RuntimeShapes::api.name,g.prefab.Get()))==g.manifest->rootName,"effect prefab root name differs");
  Require(!RuntimeShapes::Scalar<unsigned char>(api.activeSelf,g.prefab.Get()),"effect debug prefab must have offline inactive root");
  Require(Particles(g.prefab.Get()).size()==g.manifest->particles,"effect prefab particle hierarchy differs");g.phase=Phase::Ready;return true;
 }return false;
}
void FollowLayer(Instance& instance,void* source){
 Require(RuntimeShapes::Alive(source),"effect render-layer owner retired");int layer=RuntimeShapes::Scalar<int>(api.layer,source);
 Require(layer>=0&&layer<32,"effect owner render-layer outside Unity domain");if(instance.layer==layer)return;
 for(const auto& object:instance.layerObjects){Require(RuntimeShapes::Alive(object.Get()),"effect render-layer child retired");void* args[]={&layer};Call(api.setLayer,object.Get(),args);}
 instance.layer=layer;
}
void EnableUIRenderers(Instance& instance,const char* phase){
 Require(api.getComponent&&api.renderInUI&&api.setRenderInUI&&api.particleRendererType.Get(),"UI ParticleSystemRenderer API unavailable");
 size_t found=0,accepted=0,rejected=0;
 for(const auto& particle:instance.particles){
  Require(RuntimeShapes::Alive(particle.Get()),"UI ParticleSystem retired before renderer setup");
  void* lookup[]={api.particleRendererType.Get()};auto renderer=Ref::Make(Call(api.getComponent,particle.Get(),lookup),false);
  Require(RuntimeShapes::Alive(renderer.Get()),"UI ParticleSystemRenderer missing");
  ++found;
  if(RuntimeShapes::Scalar<unsigned char>(api.renderInUI,renderer.Get())){++accepted;continue;}
  unsigned char enabled=1;void* set[]={&enabled};Call(api.setRenderInUI,renderer.Get(),set);
  if(RuntimeShapes::Scalar<unsigned char>(api.renderInUI,renderer.Get()))++accepted;
  else{++rejected;Log("[EFFECT-UI-RENDER-SET-GAP] phase=%s owner=%p renderer=%p setterCalled=1 renderInUI=0 playbackContinues=1",phase,instance.owner.Get(),renderer.Get());}
 }
 Require(found,"UI effect has no ParticleSystemRenderer");
 Log("[EFFECT-UI-RENDER] phase=%s owner=%p renderers=%zu accepted=%zu rejected=%zu requiredForPlayback=0",phase,instance.owner.Get(),found,accepted,rejected);
}
std::shared_ptr<Instance> CreateInstance(const std::shared_ptr<Generation>& generation,void* owner,void* parent,const EffectAttachment* attachment=nullptr,void* layerSource=nullptr,bool uiOwner=false){
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::EffectCreate);performance.hit=true;EiemLoadPerformance::Add(EiemLoadPerformance::Operation::EffectCreate);
 Require(generation&&generation->phase==Phase::Ready,"effect has no prepared native prefab");auto instance=std::make_shared<Instance>();instance->generation=generation;instance->owner=Ref::Make(owner,false);instance->ownerId=FullResourceReload::Id(owner);instance->parent=Ref::Make(parent,false);
 try{
  // Root is inactive in the offline bytes: Awake/OnEnable/playOnAwake cannot
  // start an effect at the wrong position before its native registration.
  instance->object=Ref::Make(FullResourceReload::originalCloneParent(generation->prefab.Get(),instance->parent.Get(),false),false);
  Require(RuntimeShapes::Alive(instance->object.Get()),"effect clone failed");Require(!RuntimeShapes::Scalar<unsigned char>(api.activeSelf,instance->object.Get()),"effect clone unexpectedly active");
  if(attachment){auto transform=Ref::Make(Call(FullResourceReload::api.goTransform,instance->object.Get()),false);void* position[]={const_cast<float*>(attachment->position.data())};Call(api.position,transform.Get(),position);void* rotation[]={const_cast<float*>(attachment->rotation.data())};Call(api.rotation,transform.Get(),rotation);void* scale[]={const_cast<float*>(attachment->scale.data())};Call(api.scale,transform.Get(),scale);}
  // Parenting does not inherit GameObject.layer. UI cameras select different
  // layers from world cameras, including changes after loader completion.
  // Cache only this clone's descendants once; never scan the scene per frame.
  FullResourceReload::AssemblyTree tree(instance->object.Get());for(const auto& node:tree.nodes)instance->layerObjects.push_back(Ref::Make(Call(FullResourceReload::api.componentGo,node.second.Get()),false));
  FollowLayer(*instance,layerSource?layerSource:owner);
  instance->particles=Particles(instance->object.Get());Require(instance->particles.size()==generation->manifest->particles,"effect cloned particle hierarchy differs");
  if(uiOwner)EnableUIRenderers(*instance,"before-activation");
  instance->manager=Ref::Make(FullResourceReload::ReadManager(FullResourceReload::api.effectManager),false);Require(instance->manager.Get(),"effect manager unavailable");
  instance->registrationAttempted=true;void* args[]={instance->object.Get()};Call(api.add,instance->manager.Get(),args);Active(instance->object.Get(),true);
  // Some game-side renderers reject this property while their cloned owner is inactive.
  if(uiOwner)EnableUIRenderers(*instance,"after-activation");
  // Respect disabled children and sub-emitters whose playOnAwake is false.
  for(const auto& p:instance->particles){auto go=Ref::Make(Call(FullResourceReload::api.componentGo,p.Get()),false);
   if(RuntimeShapes::Scalar<unsigned char>(api.activeHierarchy,go.Get())&&RuntimeShapes::Scalar<unsigned char>(api.playOnAwake,p.Get())){unsigned char children=0;void* play[]={&children};Call(api.play,p.Get(),play);}}
    instance->observationClock=NativeBundle::Clock();Observation::Capture(*instance,"after-play");
    return instance;
 }catch(...){Retire(instance);instance.reset();throw;}
}
void Play(const Command& c){
 auto owner=Owner(c.owner);FullResourceReload::AssemblyTree tree(owner.Get());auto instance=CreateInstance(ready,owner.Get(),tree.At(c.path),nullptr,nullptr,FullResourceReload::liveUIModels.count(owner.Get())!=0);
 auto old=std::move(playing);playing=instance;Retire(old);firstFrameClock=c.clock;firstFramePending=true;
  Log("[EFFECT-NATIVE-PLAY] key=%s ownerId=%d particles=%zu commandToPlayMs=%.6f coldPrepareExcluded=1 gpuVisibilityVerified=0",ready->manifest->key.c_str(),c.owner,instance->particles.size(),NativeBundle::Ms(c.clock));
  mailbox.Finish(c.id,"OK\tplaying\n");Status();
}
bool Drain(Generation& g){
 auto& native=FullResourceReload::api;
 if(g.lease.handle.Get()&&!g.lease.done){if(!RuntimeShapes::Scalar<unsigned char>(native.done,FullResourceReload::Self(g.lease.handle,native.handleClass)))return false;g.lease.done=true;}
 for(auto& b:g.bundles)if(b.request.Get()&&!b.done){if(!RuntimeShapes::Scalar<unsigned char>(NativeBundle::api.done,b.request.Get()))return false;b.object=Ref::Make(Call(NativeBundle::api.bundle,b.request.Get()),false);b.done=true;
  if(RuntimeShapes::Alive(b.object.Get())){b.id=FullResourceReload::Id(b.object.Get());b.owned=!g.priorBundleIds.count(b.id);for(const auto& other:g.bundles)if(&other!=&b&&other.done&&other.object.Get()&&other.id==b.id)b.owned=false;}}
 if(g.assetRequest.Get()&&!g.assetDone){if(!RuntimeShapes::Scalar<unsigned char>(NativeBundle::api.done,g.assetRequest.Get()))return false;g.assetDone=true;}
 for(auto& b:g.bundles)if(b.object.Get()&&!b.owned){g.quarantined=true;Log("[EFFECT-NATIVE-QUARANTINE] ownershipUnproven=1 sourceLeaseRetained=1");return false;}
 for(auto& b:g.bundles)if(b.owned&&!b.unloadCalled){b.unloadCalled=true;if(RuntimeShapes::Alive(b.object.Get())){unsigned char destroy=0;void* args[]={&destroy};Call(NativeBundle::api.unload,b.object.Get(),args);}}
 FullResourceReload::Dispose(g.lease);g.finished=true;return true;
}
void Collect(){
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::EffectCollect);performance.hit=!retiredInstances.empty();
 for(auto it=retiredInstances.begin();it!=retiredInstances.end();){auto& i=**it;if(!i.quarantined&&!RuntimeShapes::Alive(i.object.Get()))it=retiredInstances.erase(it);else ++it;}
 for(auto it=generations.begin();it!=generations.end();){auto& g=**it;bool used=g.externalPins||candidate.get()==&g||ready.get()==&g||(playing&&playing->generation.get()==&g);
  for(const auto& i:retiredInstances)used=used||i->generation.get()==&g;
  if(used||g.quarantined){++it;continue;}try{if(Drain(g)){it=generations.erase(it);continue;}}catch(const std::exception& e){g.quarantined=true;Log("[EFFECT-NATIVE-QUARANTINE] retire=%s repeatedUnloadRefused=1",e.what());}++it;
 }
}
void Execute(const Command& c){
 switch(c.op){
 case Op::ReloadAttachment:RuntimeAttachments::Reload(c.package,*c.attachment);mailbox.Finish(c.id,"OK\tattachment queued; old instance retained until native ready\n");return;
 case Op::Prepare:Prepare(c);return;
 case Op::Play:Play(c);return;
 case Op::Stop:Stop();mailbox.Finish(c.id,"OK\tstopped\n");Status();return;
 case Op::Clear:Stop();ready.reset();if(candidate){mailbox.Finish(candidate->ticket,"ERR\tcleared\n");candidate->phase=Phase::Rejected;candidate.reset();}mailbox.Finish(c.id,"OK\tcleared\n");Status();return;
 case Op::Owners:{std::ostringstream out;out<<"OK\towners\n";for(auto& owner:Owners())out<<owner.first<<'\t'<<Esc(String(Call(RuntimeShapes::api.name,owner.second.Get())))<<'\n';mailbox.Finish(c.id,out.str());return;}
 case Op::Nodes:{auto root=Owner(c.owner);FullResourceReload::AssemblyTree tree(root.Get());std::ostringstream out;out<<"OK\tnodes\t"<<c.owner<<'\n';for(const auto& node:tree.nodes)out<<(node.first.empty()?"-":node.first)<<'\t'<<Esc(String(Call(RuntimeShapes::api.name,node.second.Get())))<<'\n';mailbox.Finish(c.id,out.str());return;}
 }
}
void Frame(){
 if(!enabled||(!mailbox.waiting.load(std::memory_order_relaxed)&&!nativeWork))return;
 if(!RuntimeShapes::Main())return;auto clock=NativeBundle::Clock();
 if(firstFramePending){firstFramePending=false;Log("[EFFECT-NATIVE-FIRST-FRAME] commandToNextUnityFrameMs=%.6f gpuVisibilityVerified=0",NativeBundle::Ms(firstFrameClock));}
 auto commands=mailbox.Drain();for(const auto& c:commands)try{Execute(c);}catch(const std::exception& e){mailbox.Finish(c.id,"ERR\t"+Esc(e.what())+"\n");Log("[EFFECT-NATIVE-REJECT] command=%llu reason=%s previousEffectRetained=%d",c.id,e.what(),int(bool(playing)));}
 if(candidate){try{auto g=candidate;while(NativeBundle::Ms(clock)<1.&&Step(*g))if(g->phase==Phase::Ready){ready=g;candidate.reset();mailbox.Finish(g->ticket,"OK\tready\treused=0\n");Log("[EFFECT-NATIVE-READY-NATIVE] key=%s prepareMs=%.6f bundles=%zu oldEffectRetained=%d",g->manifest->key.c_str(),NativeBundle::Ms(g->clock),g->bundles.size(),int(bool(playing)));Status();break;}}
  catch(const std::exception& e){mailbox.Finish(candidate->ticket,"ERR\t"+Esc(e.what())+"\n");Log("[EFFECT-NATIVE-REJECT] prepare=%s oldEffectRetained=%d",e.what(),int(bool(playing)));candidate->phase=Phase::Rejected;candidate->failure=e.what();candidate.reset();Status();}}
   if(playing)Observation::Tick(*playing);
   Collect();nativeWork=bool(candidate)||firstFramePending;
 for(const auto& i:retiredInstances)nativeWork=nativeWork||!i->quarantined;
 for(const auto& g:generations){bool used=g->externalPins||g==ready||(playing&&playing->generation==g);for(const auto& i:retiredInstances)used=used||i->generation==g;nativeWork=nativeWork||(!used&&!g->quarantined);}
 auto ms=NativeBundle::Ms(clock);if(ms>2.)Log("[EFFECT-NATIVE-FRAME] mainMs=%.6f softBudgetMs=1 nativeCallNotPreemptible=1",ms);
}

// Request/response is one UTF-8 message per connection. Unity commands return
// a ticket; RESULT polls immutable replies without touching IL2CPP on this thread.
#if defined(EIEM_EFFECTS_DEBUG)
int OwnerId(const std::string& text){size_t used=0;auto value=std::stoll(text,&used);Require(used==text.size()&&value>=INT_MIN&&value<=INT_MAX&&value!=0,"invalid signed Unity instance id");return int(value);}
std::string Dispatch(const std::string& message){
 try{Require(message.find('\0')==message.npos,"effect command contains NUL");std::vector<std::string> fields;std::istringstream in(message);std::string s;while(std::getline(in,s,'\t'))fields.push_back(s);Require(!fields.empty(),"empty effect command");
  if(fields[0]=="STATUS"&&fields.size()==1)return mailbox.Status();
  if(fields[0]=="RESULT"&&fields.size()==2){size_t used=0;auto id=std::stoull(fields[1],&used);Require(used==fields[1].size(),"invalid effect ticket");return mailbox.Result(id);}
  Command c;c.clock=NativeBundle::Clock();
  if(fields[0]=="OWNERS"&&fields.size()==1)c.op=Op::Owners;
  else if(fields[0]=="NODES"&&fields.size()==2){c.op=Op::Nodes;c.owner=OwnerId(fields[1]);}
  else if(fields[0]=="PREPARE"&&fields.size()==2){c.op=Op::Prepare;c.manifest=EiemEffectDebugPacket::ReadManifest(std::filesystem::u8path(fields[1]));}
  else if(fields[0]=="PLAY"&&fields.size()==3){c.op=Op::Play;c.owner=OwnerId(fields[1]);c.path=fields[2]=="-"?"":fields[2];}
  else if(fields[0]=="RELOAD_ATTACHMENT"&&fields.size()==2){auto path=std::filesystem::u8path(fields[1]);Require(path.is_absolute(),"attachment reload requires absolute resource path");auto resource=std::make_shared<const Resource>(Decode(EiemDirectory::Read(path),10));auto decoded=DecodeEffectAttachment(*resource,resource);c.op=Op::ReloadAttachment;c.package=decoded.first;c.attachment=std::make_shared<const EffectAttachment>(std::move(decoded.second));}
  else if(fields[0]=="STOP"&&fields.size()==1)c.op=Op::Stop;
  else if(fields[0]=="CLEAR"&&fields.size()==1)c.op=Op::Clear;
  else throw std::runtime_error("unknown effect command/arity");
  Require(active.load()||c.op==Op::Clear||c.op==Op::Stop,"effect debugger disabled by plugin host");
  return "QUEUED\t"+std::to_string(mailbox.Submit(std::move(c)))+"\n";
 }catch(const std::exception& e){return "ERR\t"+Esc(e.what())+"\n";}
}
bool CompleteIO(HANDLE pipe,OVERLAPPED& op,BOOL immediate,DWORD& bytes,DWORD timeout){
 if(immediate)return true;if(GetLastError()!=ERROR_IO_PENDING)return false;
 if(WaitForSingleObject(op.hEvent,timeout)!=WAIT_OBJECT_0){CancelIoEx(pipe,&op);GetOverlappedResult(pipe,&op,&bytes,TRUE);return false;}return GetOverlappedResult(pipe,&op,&bytes,FALSE)!=0;
}
DWORD WINAPI PipeMain(void*){
 auto name=L"\\\\.\\pipe\\EFF-Effects-"+std::to_wstring(GetCurrentProcessId());
 for(;;){auto pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,nullptr);
  if(pipe==INVALID_HANDLE_VALUE){Log("[EFFECT-NATIVE-PIPE-GAP] createError=%lu",GetLastError());return 1;}
  OVERLAPPED op{};op.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!op.hEvent){CloseHandle(pipe);Log("[EFFECT-NATIVE-PIPE-GAP] eventCreateFailed=1");return 1;}DWORD bytes=0;auto connected=ConnectNamedPipe(pipe,&op);auto error=GetLastError();
  bool ok=connected||error==ERROR_PIPE_CONNECTED;if(!ok&&error==ERROR_IO_PENDING){WaitForSingleObject(op.hEvent,INFINITE);ok=GetOverlappedResult(pipe,&op,&bytes,FALSE)!=0;}
  if(ok){ResetEvent(op.hEvent);char input[65536];auto read=ReadFile(pipe,input,sizeof(input),&bytes,&op);
   if(CompleteIO(pipe,op,read,bytes,5000)){auto reply=Dispatch(std::string(input,bytes));ResetEvent(op.hEvent);auto written=WriteFile(pipe,reply.data(),DWORD(reply.size()),&bytes,&op);
    if(CompleteIO(pipe,op,written,bytes,5000)){ResetEvent(op.hEvent);char ack=0;auto received=ReadFile(pipe,&ack,1,&bytes,&op);CompleteIO(pipe,op,received,bytes,5000);}}}
  CloseHandle(op.hEvent);DisconnectNamedPipe(pipe);CloseHandle(pipe);
 }return 0;
}
#endif
void Install(void** assemblies,size_t count){
 try{Require(FullResourceReload::enabled&&NativeBundle::enabled&&FullResourceReload::originalCloneParent,"effect debug requires native resource and owner lifecycle bindings");auto method=FullResourceReload::Method;
  auto manager=FindClass("Beyond.Gameplay","EffectManager",assemblies,count),go=FindClass("UnityEngine","GameObject",assemblies,count),transform=FindClass("UnityEngine","Transform",assemblies,count),component=FindClass("UnityEngine","Component",assemblies,count),ps=FindClass("UnityEngine","ParticleSystem",assemblies,count),particleRenderer=FindClass("UnityEngine","ParticleSystemRenderer",assemblies,count);
  api.add=method(manager,"AddUnmanagedEffect",{"UnityEngine.GameObject"},"System.Void",false);api.remove=method(manager,"RemoveUnmanagedEffect",{"UnityEngine.GameObject"},"System.Void",false);
  api.active=method(go,"SetActive",{"System.Boolean"},"System.Void",false);api.activeSelf=method(go,"get_activeSelf",{},"System.Boolean",false);api.activeHierarchy=method(go,"get_activeInHierarchy",{},"System.Boolean",false);
  api.layer=method(go,"get_layer",{},"System.Int32",false);api.setLayer=method(go,"set_layer",{"System.Int32"},"System.Void",false);
  api.setParent=method(transform,"SetParent",{"UnityEngine.Transform","System.Boolean"},"System.Void",false);
  api.position=method(transform,"set_localPosition",{"UnityEngine.Vector3"},"System.Void",false);api.rotation=method(transform,"set_localRotation",{"UnityEngine.Quaternion"},"System.Void",false);api.scale=method(transform,"set_localScale",{"UnityEngine.Vector3"},"System.Void",false);
  api.play=method(ps,"Play",{"System.Boolean"},"System.Void",false);api.stop=method(ps,"Stop",{"System.Boolean"},"System.Void",false);api.clear=method(ps,"Clear",{"System.Boolean"},"System.Void",false);api.playOnAwake=method(ps,"get_playOnAwake",{},"System.Boolean",false);
  api.getComponent=method(component,"GetComponent",{"System.Type"},"UnityEngine.Component",false);
  api.renderInUI=Exact(particleRenderer,"get_renderInUI",{},"System.Boolean",false);api.setRenderInUI=Exact(particleRenderer,"set_renderInUI",{"System.Boolean"},"System.Void",false);
  api.particleRendererType=FullResourceReload::Type(particleRenderer);
  api.particleType=FullResourceReload::Type(ps);api.bundleType=FullResourceReload::Type(FindClass("UnityEngine","AssetBundle",assemblies,count));
  api.allBundles=method(FindClass("UnityEngine","Resources",assemblies,count),"FindObjectsOfTypeAll",{"System.Type"},"UnityEngine.Object[]",true);
    Observation::Install(ps,transform,assemblies,count);
    enabled=true;Status();Log("[EFFECT-NATIVE-READY] ownerLifecycle=model-part+ui-loader renderLayerFollow=1 uiRendererBinding=%d",int(api.getComponent&&api.renderInUI&&api.setRenderInUI));
#if defined(EIEM_EFFECTS_DEBUG)
  auto worker=CreateThread(nullptr,0,PipeMain,nullptr,0,nullptr);Require(worker,"effect debug pipe thread creation failed");CloseHandle(worker);
  Log("[EFFECT-DEBUG-READY] pipe=\\\\.\\pipe\\EFF-Effects-%lu productionBuildOptIn=1 workerUnityCalls=0",GetCurrentProcessId());
#endif
 }catch(const std::exception& e){enabled=false;Log("[EFFECT-NATIVE-REJECT] install=%s productionStartupPreserved=1",e.what());}
}
} // namespace EffectDebug
