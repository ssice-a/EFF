#define EIEM_EFFECTS_DEBUG
#define main ExistingReloadSmokeMain
#include "full_resource_reload_smoke.cpp"
#undef main
#include <thread>
#include "../../src/eiem_author_mesh_bundle.h"

namespace EffectTest {
using namespace Entry;namespace D=Entry::EffectDebug;
DWORD unityThread=0;unsigned calls=0,checks=0,adds=0,removes=0,stops=0,clears=0,destroys=0;
bool badName=false,failRegister=false,deferDestroy=true,foreignBundle=false;
bool rejectNextUIRenderSet=false;
std::set<void*> dead;std::map<void*,bool> active;std::map<void*,void*> cloneParents;
std::map<void*,std::vector<void*>> particles;std::set<void*> destroyed;
std::map<void*,void*> particleRenderers;std::map<void*,bool> renderInUI;
std::set<void*> particleSystems;std::map<void*,void*> particleGameObjects;
std::map<void*,int> layers,requiredLayers;
std::map<void*,void*> effectChildren;
unsigned layerWrites=0;
float observedLow=.5f,observedHigh=1.5f,observedOne=1;
int observedMode=3,observedCount=2;std::array<float,3> observedScale{1,1,1};
unsigned observationCalls=0;
std::weak_ptr<D::Instance> failedInstance;
void Check(bool value,const char* label){++checks;Require(value,label);}
void* Invoke(void* method,void* self,void** args,void** error){
 Check(GetCurrentThreadId()==unityThread,"effect worker called Unity");++calls;*error=nullptr;auto m=uintptr_t(method);
 if(m==2){auto p=args[0];return !p||dead.count(p)?&Test::no:&Test::yes;}
 if(m==4&&self==Test::candidate)return Test::StringValue(badName?"wrong-root":"effect");
 if(m==900){++adds;auto object=args[0];Check(!active[self]&&!active[object],"effect active before registration");if(requiredLayers.count(object))Check(layers[object]==requiredLayers.at(object)&&layers[effectChildren.at(object)]==requiredLayers.at(object),"UI effect renderer layers were not set before registration/activation");if(failRegister)throw std::runtime_error("fixture registration rejected");return nullptr;}
 if(m==901){++removes;return nullptr;}
 if(m==902){active[self]=*(unsigned char*)args[0]!=0;return nullptr;}
 if(m==903)return active[self]?&Test::yes:&Test::no;
 if(m==904){cloneParents[self]=args[0];return nullptr;}
 if(m==905){Check(!*(unsigned char*)args[0],"debug started all subemitters recursively");return nullptr;}
 if(m==906){++stops;return nullptr;}if(m==907){++clears;return nullptr;}
 if(m==908){auto p=args[0];++destroys;destroyed.insert(p);if(!deferDestroy)dead.insert(p);return nullptr;}
 if(m==909){Check(args[0]==(void*)910&&*(unsigned char*)args[1],"particle typed array ABI");return Test::Array(particles[self]);}
 if(m==911){++Test::sceneQueries;return Test::Array(foreignBundle?std::vector<void*>{(void*)0x9001}:std::vector<void*>{});}
 if(m==912)return &Test::yes;
 if(m==913)return Test::Array(std::vector<void*>{});
 if(m==914)return self; // clone Transform identity
 if(m==923){Check(particleSystems.count(self)!=0,"GetComponent receiver must be the ParticleSystem Component");Check(args&&args[0]==(void*)926,"ParticleSystemRenderer type lookup ABI");return particleRenderers[self];}
 if(m==924)return renderInUI[self]?&Test::yes:&Test::no;
 if(m==925){Check(args&&args[0]&&*(unsigned char*)args[0],"UI render flag must be enabled");if(rejectNextUIRenderSet){rejectNextUIRenderSet=false;return nullptr;}renderInUI[self]=true;return nullptr;}
 if(m>=916&&m<=918){Check(!active[self],"effect transform applied after native activation");return nullptr;}
 if(m==927){auto found=particleGameObjects.find(self);return found==particleGameObjects.end()?self:found->second;}
 if(m==921)return &layers[self];
 if(m==922){Check(args&&args[0]&&*(int*)args[0]>=0&&*(int*)args[0]<32,"GameObject layer setter argument ABI");layers[self]=*(int*)args[0];++layerWrites;return nullptr;}
 if(m>=930&&m<=940){++observationCalls;
  if(m==930||m==931)return self;
  if(m==932)return &observedLow;if(m==933)return &observedHigh;
  if(m==934)return &observedMode;if(m==935)return &observedCount;
  if(m==936)return &observedOne;if(m==937)return &observedScale;
  if(m==938)return &Test::yes;
  throw std::runtime_error("observation getter rejected");
 }
 return Test::Invoke(method,self,args,error);
}
void Setup(){Test::Setup();Test::Reset();unityThread=GetCurrentThreadId();il2cpp_runtime_invoke=Invoke;
 Entry::logFile=stdout;
 D::api.add=(void*)900;D::api.remove=(void*)901;D::api.active=(void*)902;D::api.activeSelf=(void*)903;D::api.activeHierarchy=(void*)912;
 D::api.setParent=(void*)904;D::api.play=(void*)905;D::api.stop=(void*)906;D::api.clear=(void*)907;D::api.playOnAwake=(void*)912;
 D::api.allBundles=(void*)911;D::api.bundleType=RuntimeShapes::Ref::Make((void*)915,false);D::api.particleType=RuntimeShapes::Ref::Make((void*)910,false);
 D::api.position=(void*)916;D::api.rotation=(void*)917;D::api.scale=(void*)918;
 D::api.layer=(void*)921;D::api.setLayer=(void*)922;
 D::api.getComponent=(void*)923;D::api.renderInUI=(void*)924;D::api.setRenderInUI=(void*)925;D::api.particleRendererType=RuntimeShapes::Ref::Make((void*)926,false);
 RuntimeShapes::api.destroy=(void*)908;FullResourceReload::api.components=(void*)909;FullResourceReload::api.goTransform=(void*)914;
 NativeBundle::api.byteClass=Test::byteClass;NativeBundle::api.streamClass=(void*)0xBA11;NativeBundle::api.streamCtor=(void*)88;NativeBundle::api.nativeStreamCtor=nullptr;
 NativeBundle::api.streamAsyncIcall=[](void*,uint32_t,uint32_t)->void*{auto request=(void*)(uintptr_t(0x8000)+ ++Test::opened);Test::scheduleDone[request]=false;Test::scheduleBundles[request]=(void*)(uintptr_t(0x9000)+Test::opened);return request;};
 il2cpp_object_new=[](void*)->void*{auto b=std::make_unique<Bytes>(8);auto p=b->data();Test::storage.push_back(std::move(b));return p;};
 FullResourceReload::originalCloneParent=[](void* prefab,void* parent,bool world)->void*{Check(prefab==Test::candidate&&!world&&!active[prefab],"clone lost inactive prefab or local transform mode");auto p=(void*)(uintptr_t(0xD000)+ ++Test::cloneCalls);active[p]=false;cloneParents[p]=parent;auto particle=(void*)(uintptr_t(p)+0x1000);particles[p]={particle};particleSystems.insert(particle);particleGameObjects[particle]=(void*)(uintptr_t(p)+0x2000);particleRenderers[particle]=(void*)(uintptr_t(p)+0x3000);return p;};
 FullResourceReload::api.componentGo=(void*)927;
 auto& p=FullResourceReload::liveParts[Test::mainPart];p.part=RuntimeShapes::Ref::Make(Test::mainPart,true);p.root=RuntimeShapes::Ref::Make(Test::oldRoot,true);p.path=Test::modelPath;
 Test::transformParents[Test::oldRoot]=nullptr;Test::transformParents[Test::bone]=Test::oldRoot;
 D::enabled=true;D::Status();particles[Test::candidate]={(void*)0xE000};
}
void ObservationChecks(){
 auto saved=D::Observation::api;auto& a=D::Observation::api;
 a.ready=true;a.mainModule=a.emission=(void*)930;a.life=a.start=a.rate=(void*)931;
 a.low=(void*)932;a.high=(void*)933;a.mode=a.max=a.space=(void*)934;a.count=(void*)935;
 a.time=a.speed=a.timeScale=a.duration=a.multiplier=(void*)936;a.lossyScale=(void*)937;a.loop=a.playing=(void*)938;
 D::Instance instance;instance.owner=RuntimeShapes::Ref::Make(Test::oldRoot,false);instance.object=instance.owner;instance.particles={RuntimeShapes::Ref::Make((void*)0xE000,false)};
 auto transformWrites=layerWrites;auto registrations=adds;auto destroyedCount=destroys;auto clearCount=clears;
 auto curve=D::Observation::Curve(a.life,instance.particles[0].Get());Check(curve.find("min:0.5,max:1.5")!=std::string::npos,"native observation lost curve limits");
 a.multiplier=nullptr;
 auto strippedCurve=D::Observation::Curve(a.life,instance.particles[0].Get());
 Check(strippedCurve.find("multiplier:unavailable")!=std::string::npos,"stripped optional curve getter disabled native particle observation");
 D::Observation::Capture(instance,"test-stripped-getter");Check(instance.observationStep==0,"missing optional curve getter disabled basic particle snapshots");
 a.multiplier=(void*)936;
 D::Observation::Capture(instance,"test");Check(instance.observationStep==0,"read-only observation failed");
 instance.observationClock=NativeBundle::Clock();auto before=observationCalls;D::Observation::Tick(instance);Check(observationCalls==before,"observation polled native fields before scheduled time");
 instance.observationClock=0;D::Observation::Tick(instance);D::Observation::Tick(instance);before=observationCalls;D::Observation::Tick(instance);
 Check(instance.observationStep==2&&observationCalls==before,"observation did not stop after bounded snapshots");
 a.life=(void*)940;D::Observation::Capture(instance,"test-reject");Check(instance.observationStep==2,"observation rejection kept retrying");
 Check(layerWrites==transformWrites&&adds==registrations&&destroys==destroyedCount&&clears==clearCount,"observation changed playback lifecycle");
 D::Observation::api=saved;std::cout<<"PASS read-only native effect observations: curve ranges, delayed samples, bounded idle, failure isolation\n";
}
std::shared_ptr<const D::Manifest> Manifest(char key){auto m=std::make_shared<D::Manifest>();m->key=std::string(64,key);m->prefab="assets/effect.prefab";m->rootName="effect";m->particles=1;
 EiemEffectDebugPacket::Bundle b;b.file="runtime/bundles/test.ab";b.hash=std::string(64,key);b.bytes=std::make_shared<const Bytes>(EiemNativeInput::EndfieldInput::Encode(EiemAuthorMeshBundle::EnvelopeHeader(),{{"CAB-test",4,Bytes{1,2,3,4}}}));m->bundles.push_back(b);return m;}
uint64_t Submit(D::Op op,std::shared_ptr<const D::Manifest> m={},std::string path=""){D::Command c;c.op=op;c.manifest=m;c.owner=int(uintptr_t(Test::oldRoot));c.path=path;c.clock=NativeBundle::Clock();return D::mailbox.Submit(std::move(c));}
void Pump(unsigned n=10){for(unsigned i=0;i<n;++i)D::Frame();}
void Complete(){for(unsigned i=0;i<10;++i){for(auto& p:Test::scheduleDone)p.second=true;Test::done=1;Pump();}}
bool Good(uint64_t ticket){return D::mailbox.Result(ticket).rfind("OK",0)==0;}
void TestCases(){
 Setup();unsigned before=calls;std::string response;
 std::thread worker([&]{response=D::Dispatch("OWNERS");D::Dispatch("STATUS");D::Dispatch("SPEED\t1");});worker.join();Check(calls==before&&response.rfind("QUEUED",0)==0,"worker query performed native work");Pump();
 auto first=Submit(D::Op::Prepare,Manifest('A'));Pump();Check(D::candidate&&!D::ready,"preparation skipped async lease wait");Complete();Check(Good(first)&&D::ready,"native prepare failed");
 before=Test::opened;auto reused=Submit(D::Op::Prepare,Manifest('A'));Pump();Check(Good(reused)&&Test::opened==before,"identical ready generation reloaded its CAB namespace");
 auto play=Submit(D::Op::Play);Pump();Check(Good(play)&&D::playing&&adds==1,"prepared debug effect did not play");auto old=D::playing;
 auto badMount=Submit(D::Op::Play,{},"/missing[0]");Pump();Check(!Good(badMount)&&D::playing==old,"missing mount destroyed playing effect");
 auto second=Submit(D::Op::Prepare,Manifest('B'));Pump(1);Check(D::playing==old,"prepare retired the visible effect");Complete();Check(Good(second)&&D::playing==old,"completed prepare auto-committed");
 failRegister=true;auto bad=Submit(D::Op::Play);Pump();Check(!Good(bad)&&D::playing==old&&D::retiredInstances.back()->removed,"partial failed registration lost old playing effect or native deregistration");failedInstance=D::retiredInstances.back();failRegister=false;
 auto next=Submit(D::Op::Play);Pump();Check(Good(next)&&D::playing!=old&&old->removed&&old->destroyAttempted,"new playback did not retire old through native lifecycle");
 auto oldGeneration=old->generation;before=Test::unloadCalls;Pump();Check(!oldGeneration->finished&&Test::unloadCalls==before,"generation unloaded before deferred Destroy");
 for(auto p:destroyed)dead.insert(p);Pump();Check(oldGeneration->finished,"dead clone did not release retired generation");
 auto owner=D::playing->owner.Get();D::OwnerReleasing(owner);Check(!D::playing&&cloneParents[destroyed.empty()?nullptr:*destroyed.begin()]==nullptr,"owner release did not detach debug child before pool return");
 auto clear=Submit(D::Op::Clear);Pump();Check(Good(clear)&&!D::ready,"clear retained prepared selection");
 // Supersede an outstanding native request. Its memory, Bundle and source
 // lease must survive until completion even though it will never be exposed.
 Test::done=0;auto cancel=Submit(D::Op::Prepare,Manifest('C'));Pump(1);auto pending=D::candidate;
 auto replacement=Submit(D::Op::Prepare,Manifest('D'));Pump(1);Check(!pending->finished&&!Good(cancel),"canceled lease disposed while native request pending");
 Complete();Check(pending->finished&&Good(replacement),"superseded request failed to drain");
 // Reject a native root mismatch while the previously prepared root survives.
 badName=true;auto prior=D::ready;auto mismatch=Submit(D::Op::Prepare,Manifest('E'));Pump();Complete();Check(!Good(mismatch)&&D::ready==prior,"failed prepare replaced last verified root");badName=false;
 // A portable author archive must never be passed to the game's native reader.
 auto interchange=std::const_pointer_cast<D::Manifest>(Manifest('7'));interchange->bundles.front().bytes=std::make_shared<const Bytes>(Bytes{'U','n','i','t','y','F','S',0});
 auto submissions=Test::opened;auto archiveFailure=Submit(D::Op::Prepare,interchange);Pump();Complete();
 Check(!Good(archiveFailure)&&D::ready==prior&&Test::opened==submissions,"portable UnityFS archive reached native submission or displaced the last verified root");
 // Query result retains signed Unity ids and root path without whitespace parsing.
 Check(D::OwnerId("-2147483648")==INT_MIN,"negative Unity instance id rejected");Check(D::Dispatch("PLAY\t0\t-").rfind("ERR",0)==0,"invalid target id accepted");
 auto node=Submit(D::Op::Nodes);Pump();Check(Good(node)&&D::mailbox.Result(node).find("\n-\t")!=std::string::npos,"root mount not enumerated");
 auto stopped=Submit(D::Op::Stop);Pump();Check(Good(stopped),"STOP ticket failed");
 auto beforeReady=D::ready;foreignBundle=true;Test::opened=0;auto alias=Submit(D::Op::Prepare,Manifest('F'));Pump();Complete();
 Check(!Good(alias)&&D::ready==beforeReady&&D::generations.back()->quarantined,"pre-existing Bundle alias was accepted or unloaded");foreignBundle=false;
 // With a settled selection, the disabled debug loop does no Unity work.
 for(auto p:destroyed)dead.insert(p);Pump();before=calls;Pump(1000);Check(calls==before,"idle effect debugger repeatedly called Unity");
 std::thread host([]{AP_PluginDisable();});host.join();Pump();Check(!D::ready,"host disable did not queue Unity-thread resource retirement");
 Check(D::Dispatch("OWNERS").rfind("ERR",0)==0,"disabled debugger accepted a new action");Entry::active=true;
 std::cout<<"PASS effect native lifecycle: "<<checks<<" checks; failed prepare/play, supersession, deferred destruction, owner release, worker isolation\n"<<std::flush;
}
void Teardown(){
 D::playing.reset();D::ready.reset();D::candidate.reset();D::retiredInstances.clear();D::generations.clear();D::api={};
 NativeBundle::memoryRequests.clear();NativeBundle::memoryBundles.clear();NativeBundle::memoryUnloads.clear();FullResourceReload::liveParts.clear();FullResourceReload::observedPrefabs.clear();
 Check(failedInstance.expired(),"failed native registration retained an orphaned instance");
 Check(Test::handles.empty(),"effect debugger leaked managed handles after all generations retired");RuntimeShapes::managedAlive=false;
}
}
#include "material_animation_runtime_checks.h"
#include "effect_attachment_runtime_checks.h"
void EffectDiskChecks(const std::filesystem::path& character,const std::filesystem::path& independent){
 using namespace EiemDirectory;auto p=Load(character);auto directory=std::filesystem::temp_directory_path()/("EFF-effect-disk-test-"+std::to_string(GetCurrentProcessId()));Require(!std::filesystem::exists(directory),"disk test temporary exists");std::filesystem::create_directories(directory);
 struct Cleanup{std::filesystem::path root;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(root,ignored);}}cleanup{directory};
 auto game=directory/"game",root=game/"plugin/mods/Character";std::filesystem::create_directories(root);std::filesystem::copy_file(character/"mod.ini",root/"mod.ini");
 for(const auto& row:p.resources)if(row.second->kind==10){auto target=root/std::filesystem::u8path(row.first);std::filesystem::create_directories(target.parent_path());std::filesystem::copy_file(character/std::filesystem::u8path(row.first),target);}
 p.root=root;auto effects=Entry::StandaloneEffects::ReadAttached(p);Require(effects.size()==p.effects.size(),"F9 effect-only reader changed declarations");Require(!std::filesystem::exists(root/"meshes"),"effect-only regression accidentally supplied Mesh bytes");
 auto independentRoot=root/std::filesystem::u8path(u8"花瓣");std::filesystem::copy(independent,independentRoot,std::filesystem::copy_options::recursive);auto standalone=Entry::StandaloneEffects::ReadAll(game);Require(standalone.size()==1&&standalone.front().root==std::filesystem::canonical(independentRoot)&&EiemComposedPackage::Roots(game/"plugin/mods").size()==1,"nested independent Mod missing or leaked into character roots");
 Entry::gameDirectory=game;std::atomic_store(&Entry::published,std::make_shared<const std::vector<Package>>(std::vector<Package>{p}));Entry::RuntimeAttachments::PrepareDiskReload();Require(Entry::RuntimeAttachments::reloadPending&&Entry::RuntimeAttachments::pendingEffects.at(p.id).size()==p.effects.size(),"disk F9 did not prepare full snapshot");
 {std::ofstream invalid(root/"mod.ini",std::ios::binary|std::ios::trunc);invalid<<"[Mod]\nformat=2\nid=Wrong\n";}
 Entry::RuntimeAttachments::PrepareDiskReload();Require(!Entry::RuntimeAttachments::reloadPending&&!Entry::RuntimeAttachments::pendingStandalone,"rejected F9 left an earlier candidate armed");
 std::filesystem::create_directories(game/"plugin/mods/Effects/group");std::filesystem::rename(independentRoot,game/"plugin/mods/Effects/group/Petals");std::filesystem::remove_all(root);std::atomic_store(&Entry::published,std::make_shared<const std::vector<Package>>());Entry::RuntimeAttachments::PrepareDiskReload();Require(Entry::RuntimeAttachments::reloadPending&&Entry::RuntimeAttachments::pendingStandalone->size()==1&&Entry::RuntimeAttachments::pendingEffects.empty(),"nested standalone-only startup requires a character Mod");
 auto config=Entry::settings.Get();Require(EiemParseKeyChord("F8",&config.effectsReload),"test shortcut invalid");auto candidate=Entry::EntryReload::Validate(std::atomic_load(&Entry::published),config);Require(candidate&&candidate->controls.graph->empty(),"standalone-only F10 configuration validation failed");
 Entry::RuntimeAttachments::pendingStandalone.reset();Entry::RuntimeAttachments::pendingEffects.clear();Entry::RuntimeAttachments::reloadPending=false;
 std::cout<<"PASS F9 disk snapshots: no Mesh/texture reads, nested independent discovery, failed candidate cancellation and nested standalone-only startup\n";
}
int main(int argc,char** argv){
 try{
  if(argc>2&&std::string(argv[1])=="--native-archives"){
   for(int i=2;i<argc;++i){auto bytes=EiemNativeInput::Read(std::filesystem::u8path(argv[i]));Require(EiemEffectNative::Archive(bytes),"effect game envelope rejected");auto archive=EiemNativeInput::EndfieldInput::Decode(bytes);
    Require(EiemNativeInput::EndfieldInput::Encode(EiemAuthorMeshBundle::EnvelopeHeader(),archive.members)==bytes,"C# archive writer differs from production C++ native writer");
    for(const auto& member:archive.members){auto expected=EiemNativeInput::Read(std::filesystem::u8path(argv[i]).parent_path()/member.name);Require(member.bytes==expected,"native envelope changed effect member bytes");}
    std::cout<<"PASS real effect native envelope: "<<argv[i]<<"; C# / C++ bytes identical, all member payloads preserved\n";
   }return 0;
  }
  if(argc==2&&std::string(argv[1])=="--serve"){
   EffectTest::Setup();auto pipe=CreateThread(nullptr,0,EffectDebug::PipeMain,nullptr,0,nullptr);Require(pipe,"fixture pipe worker failed");CloseHandle(pipe);
   std::cout<<"SERVER "<<GetCurrentProcessId()<<'\n'<<std::flush;
   for(unsigned frame=0;frame<60000;++frame){EffectTest::Complete();Sleep(1);}RuntimeShapes::managedAlive=false;return 0;
  }
  if(argc==4&&std::string(argv[1])=="--disk-check"){EffectDiskChecks(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]));return 0;}
  EffectTest::TestCases();EffectTest::ObservationChecks();MaterialAnimationTest::Checks();EffectAttachmentTest::Checks();if(argc==2){auto m=EiemEffectDebugPacket::ReadManifest(std::filesystem::absolute(argv[1]));std::cout<<"PASS real offline runtime manifest: bundles="<<m->bundles.size()<<" particles="<<m->particles<<"\n"<<std::flush;}EffectTest::Teardown();return 0;
 }
 catch(const std::exception& e){std::cerr<<"FAIL effect debug: "<<e.what()<<'\n'<<std::flush;RuntimeShapes::managedAlive=false;return 1;}
}
