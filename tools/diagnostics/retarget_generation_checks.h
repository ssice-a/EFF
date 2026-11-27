#pragma once
#include "../../src/eiem_static_resource_sources.h"
#include "native_mesh_fixture.h"
#include <future>
#include <thread>
namespace RetargetNativeFixture {
using ::Fixture;using ::Schema;using ::Ref;using ::Bundle;
}

void NativeSourceRouteIsolationRegression(){
 namespace D=DemandLoadingProbe;
 auto enabled=D::enabled.exchange(true),sharedEnabled=D::sharedInputsEnabled.exchange(true);
 auto helper=D::helperToken;
 auto record=D::registry.Observe(0xFEE100,0xFEE101,"main/retired-weapon.ab",D::Q::Candidate::Target);
 D::registry.BindVfs(record.token,0xFEE101);
 D::registry.BindFile(record.token,"d:/game/source.chunk",123);
 auto restore=[&]{D::registry.RetireScope(record.scope);D::enabled=enabled;D::sharedInputsEnabled=sharedEnabled;D::helperToken=helper;};
 try{
  auto previous=D::BeginVfs(0xFEE101);
  Require(!D::helperToken,"new native VFS request selected an old unisolated prepared Mod input");
  D::EndHelper(previous);
  D::helperToken=record.token;
  void* result=nullptr;
  Require(!D::SubmitCurrentHelper(0,false,result),"native helper bypassed the generation graph");
  Require(!D::BeforeFile(Test::StringValue("d:/game/source.chunk"),123,false),
   "native physical file reused a Mod route from another generation");
  D::BeginHelper(Test::StringValue("Data/Bundles/Windows"),Test::StringValue(record.name));
  Require(!D::helperToken,"native helper resurrected an unscoped prepared Mod route");
 }catch(...){restore();throw;}restore();
 std::cout<<"PASS native source requests cannot select unisolated Mod inputs; generation aliases remain authoritative\n";
}

void RetiredGenerationRouteIndexRegression(){
 namespace D=DemandLoadingProbe;
 auto state=std::make_shared<D::Manifest>();state->serial=0xFEE180;
 auto shared=std::make_shared<D::SharedScope>(state);
 D::sharedScopes.emplace(state->serial,shared);D::scopes.emplace(state->serial,state);
 const uint64_t hash=0xFEE181,key=0xFEE182;
 D::sharedVfsRouteIndex.emplace(hash,D::SharedVfsRoute{shared,key});shared->vfsRoutes.emplace(hash,key);
 auto record=D::registry.Observe(state->serial,key,"main/retired-generation.ab",D::Q::Candidate::Yes);
 D::preparedInputs.emplace(record.token,std::make_shared<D::PreparedInput>());
 auto cleanup=[&]{D::sharedScopes.erase(state->serial);D::scopes.erase(state->serial);D::sharedVfsRouteIndex.erase(hash);D::preparedInputs.erase(record.token);D::registry.RetireScope(state->serial);};
 try{
  shared->activeAuthorProxies.insert(key);
  D::PruneLocked();Require(D::sharedScopes.count(state->serial)&&D::sharedVfsRouteIndex.count(hash),"old live generation lost its VFS routes");
  shared->activeAuthorProxies.clear();std::weak_ptr<D::SharedScope> retired=shared;shared.reset();D::PruneLocked();
  Require(retired.expired()&&!D::sharedScopes.count(state->serial)&&!D::sharedVfsRouteIndex.count(hash)&&!D::preparedInputs.count(record.token)&&!D::registry.Get(record.token).token,
   "retired generation leaked VFS routes or prepared native inputs");
 }catch(...){cleanup();throw;}cleanup();
 std::cout<<"PASS retired generation drops VFS index and prepared input routes after its last native proxy\n";
}

void DeferredSharedRetirementRegression(){
 namespace D=DemandLoadingProbe;
 auto state=std::make_shared<D::Manifest>();state->serial=0xFEE190;
 state->object=RuntimeShapes::Ref::Make(Test::dependencyManifest,false);
 auto shared=std::make_shared<D::SharedScope>(state);
 const auto oldManifests=D::manifests;
 const auto oldCatalog=D::CurrentInstalledGeneration();
 const bool wasEnabled=D::enabled.exchange(true),wasShared=D::sharedInputsEnabled.exchange(true);
 D::manifests[state->object.Get()]=state;D::sharedScopes.emplace(state->serial,shared);D::scopes.emplace(state->serial,state);
 const uint64_t key=0xFEE191,hash=0xFEE192;
 auto record=D::registry.Observe(state->serial,key,"main/deferred-retirement.ab",D::Q::Candidate::Yes);
 D::preparedInputs.emplace(record.token,std::make_shared<D::PreparedInput>());
 auto bytes=std::make_shared<const Bytes>(Bytes{1});
 auto view=std::make_shared<const EiemInputSpanView::View>(
   std::vector<EiemInputSpanView::Span>{{EiemInputSpanView::literalSource,0,1,bytes}},
   std::vector<size_t>{},EiemInputSpanView::ReadRange{});
 EiemDemandSharedGraph::Input input;input.key=key;input.categorySource=10;input.name="eff/deferred-retirement";input.cab="CAB-deferred-retirement";input.view=view;
 shared->graph.Commit({input},{},{});shared->activeAuthorProxies.insert(key);
 EiemDemandSharedGraph::Input observed;bool readinessChanged=false;
 Require(shared->graph.FindAndObserveReadiness(key,true,observed,readinessChanged)&&readinessChanged&&
   shared->graph.FindAndObserveReadiness(key,true,observed,readinessChanged)&&!readinessChanged,
   "readiness observation did not track transitions for the active input");
 shared->vfsRoutes.emplace(hash,key);D::sharedVfsRouteIndex.emplace(hash,D::SharedVfsRoute{shared,key});
 auto cleanup=[&]{D::manifests=oldManifests;D::sharedScopes.erase(state->serial);D::scopes.erase(state->serial);
   D::sharedVfsRouteIndex.erase(hash);D::preparedInputs.erase(record.token);D::registry.RetireScope(state->serial);
   std::atomic_store(&D::installedGeneration,oldCatalog);
   D::enabled=wasEnabled;D::sharedInputsEnabled=wasShared;};
 // Another operation owns preparation. Do not block the Unity frame or use
 // recursive try_lock on the same thread's std::mutex in this fixture.
 auto queueWhileBusy=[&](auto verify){
   std::promise<void> locked,release;auto released=release.get_future();
   std::thread preparing([&]{std::lock_guard<std::mutex> lock(shared->preparation);locked.set_value();released.wait();});
   locked.get_future().wait();
   try{D::ObserveSharedRetire(state->object.Get(),key);verify();}
   catch(...){release.set_value();preparing.join();throw;}
   release.set_value();preparing.join();
 };
 try{
   queueWhileBusy([&]{
     Require(shared->pendingRetires.count(key),"busy preparation did not queue native retirement");
     CommitPendingReload();
     Require(shared->graph.Size()==1&&shared->activeAuthorProxies.count(key),"Unity frame retired an input while preparation was locked");
   });
   // No new model, resource request or F10 is needed for the final event.
   D::manifests.erase(state->object.Get());
   CommitPendingReload();D::Report(300);
   Require(shared->pendingRetires.empty()&&shared->activeAuthorProxies.empty()&&shared->graph.Size()==0,
     "final queued retirement remained stranded without another resource request");
   Require(!D::sharedScopes.count(state->serial)&&!D::sharedVfsRouteIndex.count(hash)&&!D::preparedInputs.count(record.token),
     "queued retirement did not reclaim its old scope, VFS routes and prepared input");
   // A cached proxy can become active again before the queued clear is
   // processed. Its new owner cancels that obsolete retirement.
   D::manifests[state->object.Get()]=state;D::sharedScopes.emplace(state->serial,shared);
   shared->graph.Commit({input},{},{});D::MarkSharedProxyActive(shared,key);
   Require(shared->graph.FindAndObserveReadiness(key,true,observed,readinessChanged)&&readinessChanged,
     "retired input left a readiness observation behind");
   queueWhileBusy([&]{D::MarkSharedProxyActive(shared,key);});
   CommitPendingReload();
   Require(shared->graph.Size()==1&&shared->activeAuthorProxies.count(key)&&shared->pendingRetires.empty(),
     "deferred clear retired a reactivated native proxy");
   D::ObserveSharedRetire(state->object.Get(),key);CommitPendingReload();
   Require(shared->graph.Size()==0,"reactivated proxy did not release on its real final retirement");
   // A negative query used to exit before the retirement scope guard existed.
   // Invoke that production entry without a frame pump hiding the omission.
   const auto miss=key+10;shared->targetDiscoveryMisses.try_emplace(miss);
   shared->graph.Commit({input},{},{});D::MarkSharedProxyActive(shared,key);queueWhileBusy([]{});
   Require(!D::PrepareDependencyGroup(state,miss,nullptr)&&shared->pendingRetires.empty()&&shared->graph.Size()==0,
     "negative dependency lookup bypassed deferred retirement");
   // Failed effect preparation must use the same boundary, before any native
   // load starts. An absent author is a real validation exit inside its lock.
   auto catalog=std::make_shared<const D::InstalledGeneration>();state->catalog=catalog;
   std::atomic_store(&D::installedGeneration,catalog);
   shared->graph.Commit({input},{},{});D::MarkSharedProxyActive(shared,key);queueWhileBusy([]{});
   EiemEffectNative::Manifest effect;effect.unifiedResources=true;effect.authorRoot="missing-author-fixture";
   bool rejected=false;try{D::PrepareEffectResources(effect,{});}catch(const std::exception& error){
     rejected=std::string(error.what()).find("does not belong to the installed generation")!=std::string::npos;}
   Require(rejected&&shared->pendingRetires.empty()&&shared->graph.Size()==0,
     "failed effect preparation bypassed deferred retirement");
   // Lease retirement shares preparation as well; even an invalid lease must
   // not strand another proxy's final clear on its exception path.
   shared->graph.Commit({input},{},{});D::MarkSharedProxyActive(shared,key);queueWhileBusy([]{});
   D::EffectResourceLease invalidLease;invalidLease.shared=shared;invalidLease.roots={key+20};
   rejected=false;try{invalidLease.Release();}catch(const std::exception& error){
     rejected=std::string(error.what()).find("root lease differs")!=std::string::npos;}
   Require(rejected&&shared->pendingRetires.empty()&&shared->graph.Size()==0,
     "failed effect lease release bypassed deferred retirement");
 }catch(...){cleanup();throw;}cleanup();
 std::cout<<"PASS deferred shared retirement: nonblocking Unity frame, final old scope/routes, reactivation, negative query and failed effect preparation/release\n";
}

void RemovedDependencyMeshGenerationRegression(bool externalMesh=false,bool fallback=false,bool routed=false,bool lateStory=false){
 using namespace Test;namespace D=DemandLoadingProbe;namespace S=StartupDependencies;
 auto saved=std::make_tuple(D::api,D::sharedApi,S::api,S::originalAll,il2cpp_runtime_invoke,il2cpp_object_new,
   propertyElementWidth,D::sharedInputsEnabled.load(),D::enabled.load(),gameDirectory);
 auto savedDirect=S::originalDirect;
 auto catalog=D::CurrentInstalledGeneration();auto manifests=D::manifests;auto scopes=D::scopes;auto sharedScopes=D::sharedScopes;
 const auto folder=std::filesystem::absolute("bin")/("retarget-generation-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 Require(std::filesystem::create_directory(folder),"retarget fixture exists");
 auto cleanup=[&]{
  {std::lock_guard<std::mutex> lock(D::sharedVfsRouteIndexMutex);
   for(auto it=D::sharedVfsRouteIndex.begin();it!=D::sharedVfsRouteIndex.end();){auto owner=it->second.owner.lock();
    if(owner&&!sharedScopes.count(owner->serial))it=D::sharedVfsRouteIndex.erase(it);else ++it;}}
  for(const auto& row:D::sharedScopes)if(!sharedScopes.count(row.first))D::registry.RetireScope(row.first);
  D::manifests=manifests;D::scopes=scopes;D::sharedScopes=sharedScopes;
  std::atomic_store(&D::installedGeneration,catalog);
  D::api=std::get<0>(saved);D::sharedApi=std::get<1>(saved);S::api=std::get<2>(saved);S::originalAll=std::get<3>(saved);
  S::originalDirect=savedDirect;
  il2cpp_runtime_invoke=std::get<4>(saved);il2cpp_object_new=std::get<5>(saved);propertyElementWidth=std::get<6>(saved);
  D::sharedInputsEnabled=std::get<7>(saved);D::enabled=std::get<8>(saved);gameDirectory=std::get<9>(saved);
  dependencyLists.clear();dependencyNames.clear();D::PruneLocked();
  std::error_code error;std::filesystem::remove_all(folder,error);
 };
 static std::string physical;static uint64_t sourceLength=0,remoteLength=0,wrapperLength=0,routeLength=0,configLength=0;static unsigned unrelatedReads=0,vfsQueries=0;
 static bool crossBundle=false,useFallback=false,remoteUnavailable=false,nativeIncomplete=false;
 static MInfo physicalMethod{},encryptedMethod{};
 try{
  RetargetNativeFixture::Fixture fixture;
  fixture.types[6]=RetargetNativeFixture::Schema(21,{{"string","m_Name",{}},RetargetNativeFixture::Ref("m_Shader"),RetargetNativeFixture::Ref("m_Texture")});
  fixture.objects.erase(200);fixture.Add(200,6);fixture.Pointer(200,"m_Shader",201);fixture.Pointer(200,"m_Texture",202);
  fixture.types.push_back(RetargetNativeFixture::Schema(48,{{"string","m_Name",{}}}));fixture.Add(201,9);
  fixture.types.push_back(RetargetNativeFixture::Schema(28,{{"string","m_Name",{}}}));fixture.Add(202,10);
  Bytes remoteBytes;crossBundle=externalMesh;useFallback=fallback;remoteUnavailable=false;nativeIncomplete=false;
  const std::string logical=routed?"assets/static.asset":"assets/static.mesh";
  auto renameContainer=[](RetargetNativeFixture::Fixture& value,const std::string& path){
    auto& rows=ArrayValue(value.T(300),Child(value.T(300),value.V(300),"m_Container"));
    SetString(value.T(300),Child(value.T(300),rows.children.back(),"first"),path);
  };
  renameContainer(fixture,logical);
  if(externalMesh){
   RetargetNativeFixture::Fixture remote;remote.cab="CAB-remote-mesh";
   renameContainer(remote,lateStory?"assets/story/private.mesh":logical);
   for(auto it=remote.objects.begin();it!=remote.objects.end();)if(it->first!=100&&it->first!=300)it=remote.objects.erase(it);else ++it;
   remote.SetList(300,"m_PreloadTable",{100});
   auto& rows=ArrayValue(remote.T(300),Child(remote.T(300),remote.V(300),"m_Container"));rows.children={rows.children.back()};rows.count=1;
   SetInteger(remote.T(300),Child(remote.T(300),Child(remote.T(300),rows.children[0],"second"),"preloadSize"),1);
   remoteBytes=EiemNativeInput::EndfieldInput::Encode(RetargetNativeFixture::Bundle(remote.BytesOf(),remote.cab),{{remote.cab,4,remote.BytesOf()}});
   fixture.objects.erase(100);
   for(auto id:{12,22,32})SetReference(fixture.T(id),Child(fixture.T(id),fixture.V(id),"m_Mesh"),{remote.cab,100},fixture.refs,fixture.cab);
   auto& containers=ArrayValue(fixture.T(300),Child(fixture.T(300),fixture.V(300),"m_Container"));
   if(!fallback&&!routed&&!lateStory){
    // Only the Renderer's actual reference leads to this Mesh carrier.
    containers.children.pop_back();containers.count=containers.children.size();
   }else{
    // Exercise an external reference while looking up the exact logical
    // container, before the structural walk can resolve its Renderer links.
    auto& asset=Child(fixture.T(300),Child(fixture.T(300),containers.children.back(),"second"),"asset");
    SetReference(fixture.T(300),asset,{remote.cab,100},fixture.refs,fixture.cab);
   }
   fixture.SetList(300,"m_PreloadTable",{10,11,12,13,20,21,22,23,200});
   for(auto& row:containers.children){
    SetInteger(fixture.T(300),Child(fixture.T(300),Child(fixture.T(300),row,"second"),"preloadIndex"),0);
    SetInteger(fixture.T(300),Child(fixture.T(300),Child(fixture.T(300),row,"second"),"preloadSize"),9);
   }
   SetInteger(fixture.T(300),Child(fixture.T(300),Child(fixture.T(300),fixture.V(300),"m_MainAsset"),"preloadSize"),9);
   // Additional vertex streams also refer to the current remote Mesh.
   SetReference(fixture.T(13),Child(fixture.T(13),fixture.V(13),"m_AdditionalVertexStreams"),{remote.cab,100},fixture.refs,fixture.cab);
  }
  Bytes wrapperBytes,routeBytes,configBytes;
  const bool checkWrapper=externalMesh&&!fallback&&!routed&&!lateStory;
  if(checkWrapper){
   RetargetNativeFixture::Fixture wrapper;wrapper.cab="CAB-nonconsumer";
   wrapper.objects.clear();wrapper.types.push_back(RetargetNativeFixture::Schema(1001,{RetargetNativeFixture::Ref("m_SourcePrefab")}));
   wrapper.Add(301,wrapper.types.size()-1);
   wrapperBytes=EiemNativeInput::EndfieldInput::Encode(RetargetNativeFixture::Bundle(wrapper.BytesOf(),wrapper.cab),{{wrapper.cab,4,wrapper.BytesOf()}});
   RetargetNativeFixture::Fixture config=wrapper;config.cab="CAB-native-config";
   config.objects.clear();config.types.push_back(RetargetNativeFixture::Schema(114,{RetargetNativeFixture::Ref("modelReference")}));
   config.Add(301,config.types.size()-1);config.Add(300,fixture.objects.at(300).first);
   auto& rows=ArrayValue(config.T(300),Child(config.T(300),config.V(300),"m_Container"));
   auto row=ArrayElement(config.T(300),rows);
   SetString(config.T(300),Child(config.T(300),row,"first"),"assets/unrelated.asset");
   SetReference(config.T(300),Child(config.T(300),Child(config.T(300),row,"second"),"asset"),{config.cab,301},config.refs,config.cab);
   rows.children={std::move(row)};rows.count=1;
   SetReference(config.T(301),Child(config.T(301),config.V(301),"modelReference"),{"CAB-remote-mesh",100},config.refs,config.cab);
   const auto configSerialized=config.BytesOf();
   configBytes=EiemNativeInput::EndfieldInput::Encode(RetargetNativeFixture::Bundle(configSerialized,config.cab),{{config.cab,4,configSerialized}});
   fixture.types.push_back(config.T(301));fixture.Add(302,fixture.types.size()-1);
   SetReference(fixture.T(302),Child(fixture.T(302),fixture.V(302),"modelReference"),{config.cab,301},fixture.refs,fixture.cab);
  }
  if(lateStory){
   // The Timeline holds a real Renderer pointer. A separate non-Renderer
   // Bundle publishes the authored logical Mesh through its exact preload range.
   RetargetNativeFixture::Fixture story=fixture;story.cab="CAB-story-model";
   renameContainer(story,"assets/story/private.mesh");
   const auto serialized=story.BytesOf();
   wrapperBytes=EiemNativeInput::EndfieldInput::Encode(
     RetargetNativeFixture::Bundle(serialized,story.cab),{{story.cab,4,serialized}});
   RetargetNativeFixture::Fixture route=fixture;route.cab="CAB-mesh-route";
   for(auto it=route.objects.begin();it!=route.objects.end();)if(it->first!=300)it=route.objects.erase(it);else ++it;
   route.types.push_back(RetargetNativeFixture::Schema(114,{{"string","m_Name",{}}}));route.Add(301,route.types.size()-1);
   auto& rows=ArrayValue(route.T(300),Child(route.T(300),route.V(300),"m_Container"));
   rows.children={rows.children.back()};rows.count=1;
   auto& info=Child(route.T(300),rows.children[0],"second");
   SetReference(route.T(300),Child(route.T(300),info,"asset"),{route.cab,301},route.refs,route.cab);
   route.SetList(300,"m_PreloadTable",{100});
   auto& preloads=ArrayValue(route.T(300),Child(route.T(300),route.V(300),"m_PreloadTable"));
   SetReference(route.T(300),preloads.children.front(),{"CAB-remote-mesh",100},route.refs,route.cab);
   SetInteger(route.T(300),Child(route.T(300),info,"preloadSize"),1);
   const auto routeSerialized=route.BytesOf();
   routeBytes=EiemNativeInput::EndfieldInput::Encode(
     RetargetNativeFixture::Bundle(routeSerialized,route.cab),{{route.cab,4,routeSerialized}});
  }
  auto bytes=RetargetNativeFixture::Bundle(fixture.BytesOf(),fixture.cab);
  bytes=EiemNativeInput::EndfieldInput::Encode(bytes,{{fixture.cab,4,fixture.BytesOf()}});
  physical=(folder/"source.ab").u8string();sourceLength=bytes.size();remoteLength=remoteBytes.size();wrapperLength=wrapperBytes.size();routeLength=routeBytes.size();configLength=configBytes.size();
  {std::ofstream file(std::filesystem::u8path(physical),std::ios::binary);for(const auto& segment:{&bytes,&remoteBytes,&wrapperBytes,&routeBytes,&configBytes})if(!segment->empty())file.write((const char*)segment->data(),segment->size());Require(bool(file),"write native fixture");}
  gameDirectory=folder;std::filesystem::create_directories(folder/"plugin/mods");
  D::sharedInputsEnabled=true;D::enabled=true;propertyElementWidth=8;
  D::api={};D::api.packSize=16;D::api.bundleOffset=0;D::api.assetOffset=8;D::api.subLookup=(void*)181;
  D::api.lookup=routed?(void*)0xFEE214:(void*)181;
  S::api={};S::api.listCtor=(void*)140;S::api.listClass=(void*)700;S::api.listItems=(void*)18;S::api.listSize=(void*)19;S::api.listAdd=(void*)141;S::api.bundleName=(void*)143;
  dependencyNames={{"main/restoration.ab",0xFEE201},{"textures/unrelated.ab",0xFEE202},{"meshes/remote.ab",0xFEE203},
    {"main/other-scene.ab",0xFEE204},{"main/incomplete-unrelated.ab",0xFEE205},{"main/wrapper.ab",0xFEE206},{"main/mesh-route.ab",0xFEE208},{"main/config.ab",0xFEE209}};unrelatedReads=0;
  S::originalAll=[](void*,uint64_t key,void* list,void*)->bool{
   auto& dependencies=dependencyLists[list];dependencies.clear();
   if(key==0xFEE201){dependencies.push_back(0xFEE202);if(crossBundle)dependencies.push_back(0xFEE203);if(configLength)dependencies.push_back(0xFEE209);}
   if(key==0xFEE206||key==0xFEE208||key==0xFEE209)dependencies.push_back(0xFEE203);
   return !nativeIncomplete;
  };
  S::originalDirect=[](void*,uint64_t key,void* list,void*)->bool{
   auto& dependencies=dependencyLists[list];dependencies.clear();
   if(key==0xFEE201&&crossBundle&&!useFallback){dependencies.push_back(0xFEE203);if(configLength)dependencies.push_back(0xFEE209);}
   return true;
  };
  il2cpp_object_new=[](void*)->void*{auto value=std::make_unique<Bytes>(8);auto p=value->data();storage.push_back(std::move(value));dependencyLists[p]={};return p;};
  physicalMethod.mp=(void*)+[](void*,void*)->void*{return Test::StringValue(physical);};
  encryptedMethod.mp=(void*)+[](void*,void*)->unsigned char{return 0;};
  D::sharedApi={};D::sharedApi.vfsInstance=(void*)0xFEE210;D::sharedApi.vfsQuery=(void*)0xFEE211;
  D::sharedApi.infoSize=16;D::sharedApi.offset=0;D::sharedApi.length=8;D::sharedApi.offsetWidth=D::sharedApi.lengthWidth=8;
  D::sharedApi.vfsPhysical=&physicalMethod;D::sharedApi.vfsEncrypted=&encryptedMethod;
  il2cpp_runtime_invoke=[](void* method,void* self,void** args,void** exception)->void*{
   if(method==(void*)0xFEE210)return (void*)0xFEE212;
   if(method==(void*)0xFEE214){uint64_t root=0xFEE201,asset=0;memcpy(args[1],&root,8);memcpy((char*)args[1]+8,&asset,8);return &yes;}
   if(method==(void*)0xFEE211){++vfsQueries;if(String(args[0]).find("unrelated.ab")!=std::string::npos){++unrelatedReads;throw std::runtime_error("unrelated Texture must not be inspected for Mesh targets");}
    const auto path=String(args[0]);const bool remote=path.find("remote.ab")!=std::string::npos,wrapper=path.find("wrapper.ab")!=std::string::npos,route=path.find("mesh-route.ab")!=std::string::npos,config=path.find("config.ab")!=std::string::npos;
    if(remote&&remoteUnavailable)return &no;
    uint64_t offset=config?sourceLength+remoteLength+wrapperLength+routeLength:route?sourceLength+remoteLength+wrapperLength:wrapper?sourceLength+remoteLength:remote?sourceLength:0;
    uint64_t length=config?configLength:route?routeLength:wrapper?wrapperLength:remote?remoteLength:sourceLength;
    memcpy(args[1],&offset,8);memcpy((char*)args[1]+8,&length,8);return &yes;}
   return Invoke(method,self,args,exception);
  };
  auto loads=Test::memoryCalls;std::vector<uint64_t> aliases;
  auto mod=folder/"plugin/mods/weapon";std::filesystem::create_directory(mod);
  {std::ofstream ini(mod/"mod.ini");ini<<"[Mod]\nformat=3\nid=RemovedWeapon\n[ReplaceOld]\ntarget.path="<<logical<<"\ntarget.asset=Static\nhandling=skip\n";}
  std::atomic_store(&D::installedGeneration,std::shared_ptr<const D::InstalledGeneration>{});
  D::PublishInstalledGeneration(D::BuildInstalledGeneration());
  auto oldState=D::Current(dependencyManifest);auto oldList=S::NewListForManifest(dependencyManifest);
  if(lateStory){
   Require(!D::PrepareDependencyGroup(oldState,0xFEE206,oldList.Get()),
     "story PFB matched an unconfirmed logical Mesh");
   auto shared=D::SharedFor(oldState);const auto& proof=shared->targetDiscoveryMisses.at(0xFEE206);
   Require(proof&&proof->count(CanonicalSerializedAssetReference({"CAB-remote-mesh",100})),
     "story PFB miss did not retain its cross-Bundle Mesh pointer");
   Require(!D::PrepareDependencyGroup(oldState,0xFEE208,oldList.Get()),"Mesh route-only Bundle was rewritten");
   Require(!shared->targetDiscoveryMisses.count(0xFEE206),
     "Bundle without a Renderer did not publish the logical Mesh that the story PFB references");
   Require(D::PrepareDependencyGroup(oldState,0xFEE206,oldList.Get()),
     "story PFB failed to match before the world consumer was requested");
  }
  if(checkWrapper){
   const auto beforeConfig=vfsQueries;
   Require(!D::PrepareDependencyGroup(oldState,0xFEE209,oldList.Get())&&vfsQueries==beforeConfig+1,
     "a standalone config with no Mesh target path walked its external model references");
   auto shared=D::SharedFor(oldState);
   Require(!shared->targetDiscoveryChecked.count(0xFEE209)&&!shared->meshSourceLinks.count(0xFEE209),
     "the standalone config outcome leaked into structural PFB discovery");
   const auto before=vfsQueries;
   Require(!D::PrepareDependencyGroup(oldState,0xFEE206,oldList.Get())&&vfsQueries==before+1,
     "root without a Mesh or an external reference walked native dependencies");
   // These two roots have already inspected their renderer pointers. Only
   // the root referencing the newly discovered object can change outcome.
   const auto cab=SerializedAssetCabKey("CAB-remote-mesh");
   shared->targetDiscoveryMisses[0xFEE204].emplace().emplace(cab,101);
   shared->targetDiscoveryMisses[0xFEE205].emplace().emplace(cab,100);
   shared->targetDiscoveryMisses.try_emplace(0xFEE207);
  }
  Require(D::PrepareDependencyGroup(oldState,0xFEE201,oldList.Get()),"initial dependency-only Mod did not prepare");
  if(checkWrapper){
   const auto before=vfsQueries;
   auto shared=D::SharedFor(oldState);
   Require(shared->targetDiscoveryChecked.count(0xFEE209)&&shared->meshSourceLinks.at(0xFEE209).count("CAB-remote-mesh"),
     "PFB discovery failed to traverse a config rejected as a standalone root");
   Require(!D::PrepareDependencyGroup(oldState,0xFEE206,oldList.Get())&&vfsQueries==before,
     "a newly discovered dependency Mesh invalidated the structural nonconsumer result");
   Require(!D::PrepareDependencyGroup(oldState,0xFEE204,oldList.Get())&&vfsQueries==before,
     "discovering a different Mesh in the same CAB reopened a previously excluded root");
   Require(!shared->targetDiscoveryMisses.count(0xFEE205),
     "a root referencing the newly discovered Mesh did not become eligible for retry");
   Require(!shared->targetDiscoveryMisses.count(0xFEE207),
     "route-only miss without serialized proof did not become eligible for retry");
  }
  Require(fallback?unrelatedReads==1:!unrelatedReads,"Mesh discovery read an unexpected number of unrelated native dependencies");
  auto oldShared=D::SharedFor(oldState);uint64_t oldAlias=0;EiemDemandSharedGraph::Input oldInput;
  Require(oldShared->bundleNamespace.Alias(0xFEE201,oldAlias)&&oldShared->graph.Find(oldAlias,oldInput),"initial Mod input missing");
  if(lateStory){
   Require(!oldShared->targetDiscoveryMisses.count(0xFEE206)&&D::PrepareDependencyGroup(oldState,0xFEE206,oldList.Get()),
     "story PFB could not retry its newly confirmed logical Mesh");
   uint64_t storyAlias=0;EiemDemandSharedGraph::Input story;
   Require(oldShared->bundleNamespace.Alias(0xFEE206,storyAlias)&&oldShared->graph.Find(storyAlias,story),
     "matched story PFB did not publish through the native dependency graph");
   EiemNativeRandomAccess::Reader reader(story.view->Size(),[view=story.view](size_t at,unsigned char* out,size_t n){
     Require(view->ReadAt(at,out,n)==n,"story PFB truncated");});
   const auto& member=reader.directory.members.at(0);auto layout=reader.Layout(member);
   const auto& tt=layout.types.at(layout.objects.at(11).type);
   auto ownerTransform=reader.Fields(member,layout,11,{"m_Children"});
   const auto& children=ConstArray(tt,ownerTransform.at("m_Children"));
   bool hasOriginalMarker=false;
   for(const auto& pointer:children.children){
     auto child=NullableReference(tt,pointer,layout,member.name);
     auto childTransform=reader.Fields(member,layout,child.second,{"m_GameObject"});
     auto go=NullableReference(tt,childTransform.at("m_GameObject"),layout,member.name);
     auto goName=reader.Fields(member,layout,go.second,{"m_Name"}).at("m_Name").raw;
     if(Text(goName.data(),goName.size())==EiemDirectory::OriginalRendererTag(23,0))hasOriginalMarker=true;
   }
   Require(hasOriginalMarker&&Integer(reader.Fields(member,layout,13,{"m_Enabled"}).at("m_Enabled"))==1,
     "story PFB did not attach the original-visibility marker after logical Mesh discovery");
   std::cout<<"PASS story PFB late logical Mesh discovery, cross-Bundle retry and native publication\n";
  }
  if(routed){
    const auto before=vfsQueries;
    Require(!D::PrepareDependencyGroup(oldState,0xFEE204,oldList.Get())&&vfsQueries==before&&oldShared->targetDiscoveryChecked.empty(),
      "verified logical route still opened or traversed an unrelated scene");
    nativeIncomplete=true;
    const auto beforeIncomplete=vfsQueries;
    Require(!D::PrepareDependencyGroup(oldState,0xFEE205,oldList.Get())&&vfsQueries>beforeIncomplete&&
      !oldShared->targetDiscoveryMisses.count(0xFEE205),"incomplete dependency table suppressed a retryable source search");
    nativeIncomplete=false;
    unrelatedReads=0;
  }
  {
   const auto beforeReuse=vfsQueries;
   D::DependencyGroup later;later.manifest=oldState;later.shared=oldShared;later.root=0xFEE204;later.prototype=oldList.Get();
   auto record=later.Prepare(0xFEE201,D::Q::Candidate::Yes);
   auto reused=later.VersionedRoot(0xFEE201,record);
   Require(reused.view==oldInput.view&&reused.dependencies==oldInput.dependencies,
     "later scene did not reuse the published input and dependencies");
   later.keys={0xFEE201};
   D::M::Shared material;
   EiemNativeStandaloneInput::Input variant;
   variant.cab="CAB-test-variant";variant.view=oldInput.view;variant.dependencies.emplace(fixture.cab,200);
   material.inputs.emplace(variant.cab,variant);
   later.consumer=later.root;
   later.AddResourceInputs(material);
   Require(later.inputs.size()==1&&later.inputs.front().dependencies==std::vector<uint64_t>{0xFEE201},
     "a reused native CAB did not retain the material dependency edge");
   Require(vfsQueries==beforeReuse&&later.headerReads==0,"later scene reopened an already prepared source Bundle");
   std::cout<<"PASS shared scene input and Material dependency reuse: source Bundle opens="<<(vfsQueries-beforeReuse)<<std::endl;
  }
  oldShared->activeAuthorProxies.insert(oldAlias);aliases.push_back(oldAlias);
  {EiemNativeRandomAccess::Reader reader(oldInput.view->Size(),[view=oldInput.view](size_t at,unsigned char* out,size_t n){Require(view->ReadAt(at,out,n)==n,"old input truncated");});
   const auto& member=reader.directory.members.at(0);auto layout=reader.Layout(member);
   const auto& tt=layout.types.at(layout.objects.at(11).type);
   auto ownerTransform=reader.Fields(member,layout,11,{"m_Children"});
   const auto& children=ConstArray(tt,ownerTransform.at("m_Children"));
   bool hasOriginalMarker=false;
   for(const auto& pointer:children.children){
     auto child=NullableReference(tt,pointer,layout,member.name);
     auto childTransform=reader.Fields(member,layout,child.second,{"m_GameObject"});
     auto go=NullableReference(tt,childTransform.at("m_GameObject"),layout,member.name);
     auto goName=reader.Fields(member,layout,go.second,{"m_Name"}).at("m_Name").raw;
     if(Text(goName.data(),goName.size())==EiemDirectory::OriginalRendererTag(23,0))hasOriginalMarker=true;
   }
   Require(hasOriginalMarker&&Integer(reader.Fields(member,layout,13,{"m_Enabled"}).at("m_Enabled"))==1,"initial Mod did not gate the source Renderer with an original marker");}
  {
   auto observed=std::make_shared<D::InstalledGeneration>();observed->observedMeshTargets.insert({logical,"Static"});
   auto state=std::make_shared<D::Manifest>();state->serial=++D::serial;state->object=oldState->object;state->catalog=observed;state->unresolvedObservedMeshes=true;
   // The attach-only observer must use the same selective source traversal,
   // including when the Mesh exists only in another native input.
   D::api.lookup=(void*)0xFEE214;
   if(fallback){
    const auto beforeFailure=unrelatedReads;
    remoteUnavailable=true;bool incomplete=false;
    try{D::NativePrefabProof(state,"assets/a.prefab");}
    catch(const EiemDemandDependencySource::IncompleteSources&){incomplete=true;}
    catch(const std::exception&){}
    remoteUnavailable=false;
    Require(incomplete&&unrelatedReads==beforeFailure+1,
      "temporary external CAB read failure did not leave a bounded, retryable attachment lookup");
   }
   const auto priorReads=unrelatedReads;
   auto proof=D::NativePrefabProof(state,"assets/a.prefab");D::api.lookup=routed?(void*)0xFEE214:(void*)181;
   Require(proof&&proof->uses.size()==1&&proof->uses.front().identities.count({logical,"Static"})&&unrelatedReads==priorReads+unsigned(fallback),
     "attach-only Mesh proof lost its exact source or inspected an unrelated dependency");
   auto shared=D::SharedFor(state);Require(shared->observationChecked.size()==size_t(externalMesh?2:1)+size_t(checkWrapper),"attach-only observer expanded the complete native dependency group");
   Require(D::NativePrefabProof(state,"assets/a.prefab")==proof,"attach-only observer did not retain its generation-local structural proof");
  }
  std::filesystem::remove(mod/"mod.ini");
  const auto beforeReloads=unrelatedReads;
  for(unsigned reload=0;reload<3;++reload){
   auto restored=D::BuildInstalledGeneration();D::PublishInstalledGeneration(restored);
   auto state=D::Current(dependencyManifest);auto prototype=S::NewListForManifest(dependencyManifest);
   Require(state->bindings.empty()&&state->catalog->targets.empty(),"baseline restoration retained author replacement bindings");
   Require(D::PrepareDependencyGroup(state,0xFEE201,prototype.Get()),
    "removed dependency-only Mesh without a public entry or Bundle hint did not prepare baseline inputs");
   auto shared=D::SharedFor(state);uint64_t alias=0;
   if(routed)Require(state->meshRoutes==oldState->meshRoutes&&shared->targetDiscoveryChecked.empty(),
     "F10 restoration repeated confirmed Mesh discovery or reused a Mod binding");
   Require(shared->bundleNamespace.Alias(0xFEE201,alias)&&std::find(aliases.begin(),aliases.end(),alias)==aliases.end(),
    "baseline restoration reused an older generation proxy");aliases.push_back(alias);
   shared->activeAuthorProxies.insert(alias);EiemDemandSharedGraph::Input input;
   Require(shared->graph.Find(alias,input)&&input.generationIsolated&&input.dependencies.empty()&&shared->rootUsage.at(0xFEE201).mods.empty(),
    "restored native root retained removed Mod material dependencies: deps="+std::to_string(input.dependencies.size())+" mods="+std::to_string(shared->rootUsage.at(0xFEE201).mods.size()));
   EiemNativeRandomAccess::Reader reader(input.view->Size(),[view=input.view](size_t at,unsigned char* out,size_t n){Require(view->ReadAt(at,out,n)==n,"restored view truncated");});
   const auto& member=reader.directory.members.at(0);auto layout=reader.Layout(member);
   auto renderer=reader.Fields(member,layout,13,{"m_Enabled","m_Materials"});
   const auto& tt=layout.types.at(layout.objects.at(11).type);
   auto ownerTransform=reader.Fields(member,layout,11,{"m_Children"});
   Require(Integer(renderer.at("m_Enabled"))==1&&ConstArray(tt,ownerTransform.at("m_Children")).count==0,"restored Renderer retained handling=skip marker");
   if(externalMesh){
    uint64_t remoteAlias=0;EiemDemandSharedGraph::Input carrier;std::vector<uint64_t> inherited;
    Require(shared->bundleNamespace.Alias(0xFEE203,remoteAlias)&&shared->graph.Find(remoteAlias,carrier)&&shared->graph.Native(alias,inherited)&&
      std::find(inherited.begin(),inherited.end(),remoteAlias)!=inherited.end(),"restored root lost its real cross-CAB Mesh dependency");
    EiemNativeRandomAccess::Reader remote(carrier.view->Size(),[view=carrier.view](size_t at,unsigned char* out,size_t n){Require(view->ReadAt(at,out,n)==n,"remote Mesh input truncated");});
    const auto& remoteMember=remote.directory.members.at(0);auto fields=reader.Fields(member,layout,12,{"m_Mesh"});const auto& filter=layout.types.at(layout.objects.at(12).type);
    const auto& pointer=fields.at("m_Mesh");auto file=Integer(Child(filter,pointer,"m_FileID"));
    Require(file&&file<=layout.externals.size()&&layout.externals[size_t(file)-1]==remoteMember.name&&Integer(Child(filter,pointer,"m_PathID"))==100,
      "restored consumer Mesh pointer did not follow its isolated carrier generation");
   }
   auto material=reader.Fields(member,layout,200,{"m_Shader","m_Texture"});const auto& type=layout.types.at(layout.objects.at(200).type);
   Require(Integer(Child(type,material.at("m_Shader"),"m_PathID"))==201&&Integer(Child(type,material.at("m_Texture"),"m_PathID"))==202&&layout.objects.count(201)&&layout.objects.count(202),
    "baseline restoration lost its original Shader/Texture references");
   for(auto previous:aliases)Require(D::SharedInput(dependencyManifest,previous,input),"restoration invalidated a live older UI/world generation");
  }
  Require(unrelatedReads==beforeReloads+(fallback?3u:0u),"baseline restoration read an unexpected number of unrelated dependencies");
  Require(Test::memoryCalls==loads&&!leaseRequests&&!sceneQueries,"baseline preparation loaded native objects or scanned the scene");
 }catch(...){cleanup();throw;}cleanup();
 std::cout<<"PASS removed dependency-only target: crossBundle="<<externalMesh<<" fallback="<<fallback<<" nativeRoute="<<routed<<" baseline Mesh/Renderer/Shader/Texture, repeated F10 and pinned old generations\n";
}
