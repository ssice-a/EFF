#pragma once
#include "eiem_demand_dependency_scope.h"
#include "eiem_demand_material_build.h"
#include "eiem_demand_texture_build.h"
#include "eiem_native_generation_identity.h"
// Included inside DemandLoadingProbe after the current-manifest resolver.
// Only real load-tree entry points permit preparation; proxy/catalog creation
// and unrelated dependency queries cannot open author or game payloads.
thread_local unsigned sharedRequestDepth=0;
bool PrepareDependencyGroup(const std::shared_ptr<Manifest>& manifest,uint64_t hash,void* prototype);
struct SharedRequestScope {
  bool active;
  explicit SharedRequestScope(EiemDemandDependencyScope::Boundary boundary):active(EiemDemandDependencyScope::OpensRequestScope(boundary)){if(active)++sharedRequestDepth;}
  ~SharedRequestScope(){if(active)--sharedRequestDepth;}
};
struct LogicalDependencyBinding {NativeReference reference;uint64_t bundle=0;unsigned classId=0;};
struct SharedScope {
  struct RootUsage {
    std::set<std::string> mods,materials;
    std::set<Identity> logicalDependencies;
    std::map<std::string,std::shared_ptr<EiemAnimationSamples::Owner>> animationOwners;
  };
  uint64_t serial=0;
  std::shared_ptr<Manifest> manifest;
  EiemDemandSharedGraph::Graph graph;
  EiemDemandSharedGraph::DependencyExpansionCache dependencyExpansions;
  std::mutex preparation;
  std::mutex lifetimeMutex;
  std::mutex usageMutex;
  std::mutex vfsRouteMutex;
  EiemDemandGenerationRoutes::GenerationNamespace bundleNamespace;
  std::map<uint64_t,uint64_t> preparedGroups;
  std::map<uint64_t,RootUsage> rootUsage;
  std::map<uint64_t,size_t> effectRoots;
  std::set<uint64_t> activeAuthorProxies,pendingRetires;
  std::map<std::string,uint64_t> cabKeys;
  std::set<uint64_t> inspectedBundleDirectories;
  EiemNativeGenerationIdentity::Names generationNames;
  std::set<uint64_t> targetDiscoveryChecked;
  std::set<uint64_t> targetDiscoveryBudgetFailures;
  std::set<uint64_t> targetDiscoveryMisses;
  // Intrinsic serialized links only: no Reader, decoded payload or Mod
  // decision survives the request. Shared by replacement and attach proof.
  std::map<uint64_t,std::set<std::string>> meshSourceLinks;
  std::map<uint64_t,std::exception_ptr> preparationFailures;
  std::map<size_t,std::vector<Binding>> discoveredTargets;
  std::map<Identity,NativeMeshRoutes::Addresses> discoveredRestorations;
  std::set<std::pair<uint64_t,Identity>> meshCandidateMisses;
  std::map<uint64_t,uint64_t> vfsRoutes;
  std::map<Identity,LogicalDependencyBinding> logicalDependencies;
  EiemDemandMeshProvenance::Meshes observedMeshes;
  std::set<uint64_t> observationChecked;
  std::map<std::string,EiemNativeStandaloneInput::Input> materialVariants;
  std::atomic<unsigned> activePreparations{0},waitingPreparations{0};
  std::atomic<uint64_t> activeRoot{0},activeKey{0};
  std::atomic<const char*> activePhase{nullptr};
  std::atomic<ULONGLONG> preparationStarted{0},watchdogReported{0};
  explicit SharedScope(std::shared_ptr<Manifest> value):serial(value->serial),manifest(std::move(value)),bundleNamespace(serial){}
};
struct SharedLoadRouteContext {
  void* manifest=nullptr;
  std::shared_ptr<SharedScope> shared;
  EiemDemandSharedGraph::Input input;
  SharedLoadRouteContext* previous=nullptr;
};
struct SharedTreeMetadataContext {
  void* manifest=nullptr;
  void* proxy=nullptr;
  uint64_t alias=0,source=0;
  std::shared_ptr<SharedScope> shared;
  EiemDemandSharedGraph::Input input;
  SharedTreeMetadataContext* previous=nullptr;
};
// Inspection outcomes belong to the shared manifest, not a particular root
// request: several character/UI roots may reach the same native input.
template<class Inspect>bool InspectTargetInput(SharedScope& shared,uint64_t key,Inspect&& inspect){
 if(shared.targetDiscoveryChecked.count(key)||shared.targetDiscoveryBudgetFailures.count(key)||shared.preparationFailures.count(key))return false;
 try{inspect();shared.targetDiscoveryChecked.insert(key);return true;}
 catch(const R::InspectionBudgetExceeded& error){
  shared.targetDiscoveryBudgetFailures.insert(key);
  Log("[DEMAND-TARGET-DISCOVERY-GAP] bundle=%016llX reason=%s originalInputPreserved=1 failureCachedForManifest=1",key,error.what());
 }
 catch(const std::exception& error){Log("[DEMAND-TARGET-DISCOVERY-GAP] bundle=%016llX reason=%s originalInputPreserved=1",key,error.what());}
 return false;
}
// Only deterministic inspection failures are retained. Transient VFS/dependency
// failures remain retryable, and a new manifest/F10 owns a fresh outcome map.
bool RememberPreparationFailure(SharedScope& shared,uint64_t key,std::exception_ptr failure){
 try{std::rethrow_exception(failure);}
 catch(const R::InspectionBudgetExceeded&){}
 catch(const EiemNativeViewWriter::UnsupportedStorageFlags&){}
 catch(...){return false;}
 shared.preparationFailures.emplace(key,failure);return true;
}
thread_local SharedLoadRouteContext* activeSharedLoadRoute=nullptr;
thread_local SharedTreeMetadataContext* activeSharedTreeMetadata=nullptr;
struct SharedLoadRouteScope {
  SharedLoadRouteContext value;
  SharedLoadRouteScope(void* manifest,std::shared_ptr<SharedScope> shared,const EiemDemandSharedGraph::Input& input)
    :value{manifest,std::move(shared),input,activeSharedLoadRoute}{activeSharedLoadRoute=&value;}
  ~SharedLoadRouteScope(){activeSharedLoadRoute=value.previous;}
};
std::map<uint64_t,std::shared_ptr<SharedScope>> sharedScopes;
// An idle Unity frame must finish deferred clears even when no subsequent
// resource request reaches this scope. False keeps that frame path constant.
std::atomic<bool> sharedRetirementsPending{false};
struct SharedVfsRoute {std::weak_ptr<SharedScope> owner;uint64_t key=0;};
std::mutex sharedVfsRouteIndexMutex;
std::map<uint64_t,SharedVfsRoute> sharedVfsRouteIndex;
void PruneSharedLocked(){
  for(auto it=sharedScopes.begin();it!=sharedScopes.end();){
    // Own the scope until both lock guards have released its mutexes. Erasing
    // the map's last shared_ptr while merely referring to it destroys locked
    // mutexes and leaves their guards pointing at freed memory.
    const auto shared=it->second;auto current=manifests.find(shared->manifest->object.Get());
    const bool selected=current!=manifests.end()&&current->second==shared->manifest;
    if(selected||shared->activePreparations.load()||shared->waitingPreparations.load()){++it;continue;}
    std::unique_lock<std::mutex> preparation(shared->preparation,std::try_to_lock);
    if(!preparation.owns_lock()){++it;continue;}
    std::lock_guard<std::mutex> lifetime(shared->lifetimeMutex);
    bool nativeLease=false;for(const auto& row:requests)nativeLease|=row.second.generation==shared->manifest&&row.second.weak.Get();
    for(const auto& row:bundles)nativeLease|=row.second.generation==shared->manifest&&row.second.weak.Get();
    if(nativeLease||!shared->activeAuthorProxies.empty()||!shared->effectRoots.empty()){++it;continue;}
    {std::lock_guard<std::mutex> routes(sharedVfsRouteIndexMutex);
      for(auto route=sharedVfsRouteIndex.begin();route!=sharedVfsRouteIndex.end();){
        auto owner=route->second.owner.lock();
        if(!owner||owner==shared)route=sharedVfsRouteIndex.erase(route);else ++route;
      }}
    registry.RetireScope(shared->serial);it=sharedScopes.erase(it);
  }
}
uint64_t SharedVfsHash(const std::string& name);
std::shared_ptr<SharedScope> SharedFor(const std::shared_ptr<Manifest>& state){
  std::lock_guard<std::mutex> lock(mutex);
  auto& value=sharedScopes[state->serial];if(!value)value=std::make_shared<SharedScope>(state);return value;
}
std::shared_ptr<SharedScope> ExistingShared(void* manifest){
  std::lock_guard<std::mutex> lock(mutex);auto state=manifests.find(manifest);if(state==manifests.end()||state->second->object.Get()!=manifest)return {};
  auto found=sharedScopes.find(state->second->serial);return found==sharedScopes.end()?nullptr:found->second;
}
// Synthetic keys contain the manifest serial. Route old in-flight requests to
// their own graph after F10, rather than looking only in the newest graph.
std::shared_ptr<SharedScope> ExistingShared(void* manifest,uint64_t key){
  std::vector<std::shared_ptr<SharedScope>> candidates;
  {std::lock_guard<std::mutex> lock(mutex);auto state=manifests.find(manifest);
    if(state!=manifests.end()&&state->second->object.Get()==manifest){auto found=sharedScopes.find(state->second->serial);if(found!=sharedScopes.end())candidates.push_back(found->second);}
    for(const auto& row:sharedScopes)if(row.second->manifest->object.Get()==manifest&&
        (candidates.empty()||candidates.front()!=row.second))candidates.push_back(row.second);}
  EiemDemandSharedGraph::Input input;for(const auto& candidate:candidates)if(candidate->graph.Find(key,input))return candidate;
  return candidates.empty()?nullptr:candidates.front();
}
std::shared_ptr<Manifest> ManifestForScope(uint64_t serial){
  std::lock_guard<std::mutex> lock(mutex);auto found=scopes.find(serial);return found==scopes.end()?nullptr:found->second.lock();
}
bool SharedInputByVfsHash(uint64_t hash,std::shared_ptr<SharedScope>& owner,EiemDemandSharedGraph::Input& input){
  uint64_t key=0;{std::lock_guard<std::mutex> lock(sharedVfsRouteIndexMutex);auto found=sharedVfsRouteIndex.find(hash);
    if(found==sharedVfsRouteIndex.end())return false;owner=found->second.owner.lock();key=found->second.key;
    if(!owner){sharedVfsRouteIndex.erase(found);return false;}}
  return owner->graph.Find(key,input);
}
bool SharedInputByName(const std::string& name,std::shared_ptr<SharedScope>& owner,EiemDemandSharedGraph::Input& input){
  if(name.rfind("eff/",0)!=0)return false;const auto hash=SharedVfsHash(name);
  return SharedInputByVfsHash(hash,owner,input)&&EiemDemandGenerationRoutes::NameEquals(input.name,name);
}
bool SharedSyntheticPath(void* path)noexcept{
  static constexpr wchar_t fullPrefix[]=L"data/bundles/windows/eff/";
  static constexpr wchar_t barePrefix[]=L"eff/";
  __try{
    if(!path)return false;auto length=*reinterpret_cast<const int32_t*>(static_cast<const char*>(path)+16);
    constexpr size_t fullCount=sizeof(fullPrefix)/sizeof(fullPrefix[0])-1;
    constexpr size_t bareCount=sizeof(barePrefix)/sizeof(barePrefix[0])-1;
    if(length<int32_t(bareCount))return false;
    auto chars=reinterpret_cast<const wchar_t*>(static_cast<const char*>(path)+20);
    bool full=length>=int32_t(fullCount),bare=true;
    for(size_t i=0;i<bareCount;++i){auto value=chars[i];if(value>=L'A'&&value<=L'Z')value+=L'a'-L'A';if(value!=barePrefix[i])bare=false;}
    if(bare)return true;if(!full)return false;
    for(size_t i=0;i<fullCount;++i){auto value=chars[i];if(value>=L'A'&&value<=L'Z')value+=L'a'-L'A';if(value!=fullPrefix[i])return false;}
    return true;
  }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
struct SharedApi {
  void *vfsInstance=nullptr,*vfsQuery=nullptr,*vfsPhysical=nullptr,*vfsEncrypted=nullptr;
  size_t infoSize=0,offset=0,length=0,offsetWidth=0,lengthWidth=0;
  void *unsafeFromString=nullptr,*categoryInfo=nullptr,*proxyHashField=nullptr;
  void *proxyAssetBundleField=nullptr,*proxyLoadRequestField=nullptr;
  void *proxyLoadingOperationField=nullptr,*proxyLoadingGroupField=nullptr;
  void *operationBundleProxyField=nullptr;
  size_t operationBundleHashNameOffset=0,operationBundleProxyOffset=0;
  void *proxyBundleFlag=nullptr,*proxyIsLoading=nullptr,*proxyIsSuccessToLoad=nullptr;
  void *proxyIsFailedToLoad=nullptr,*proxyHasErrorOrUnloaded=nullptr;
  void *proxyFinishWithBundle=nullptr;
  void *managerTryGetBundleProxy=nullptr,*proxyRetainBundleRef=nullptr,*proxyReleaseBundleRef=nullptr,*reverseInfo=nullptr;
  void *proxyLookupInfo=nullptr,*treeSyncInfo=nullptr;
} sharedApi;
bool IsBundleKeyFieldType(const std::string& type){
  return type=="System.Int64"||type=="System.UInt64";
}
std::atomic<unsigned> sharedTreeMetadataRoutes{0};
struct SharedTreeMetadataScope {
  SharedTreeMetadataContext value;
  uint64_t previousKey=0;
  bool active=false;
  SharedTreeMetadataScope(void* manifest,void* proxy,const std::shared_ptr<SharedScope>& shared,
      const EiemDemandSharedGraph::Input& input){
    if(!manifest||!proxy||!shared||!input.versionedRoot||!input.key||!input.categorySource||input.key==input.categorySource||
        !sharedApi.proxyHashField||!il2cpp_field_get_value||!il2cpp_field_set_value)return;
    il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&previousKey);
    if(previousKey!=input.key)return;
    // The native tree builder resolves Bundle metadata from BundleProxy.hashName.
    // Present this request's current game key only while that synchronous tree
    // is assembled; keep the EFF generation alias for proxy state and async IO.
    value={manifest,proxy,input.key,input.categorySource,shared,input,activeSharedTreeMetadata};
    auto source=input.categorySource;
    il2cpp_field_set_value(proxy,sharedApi.proxyHashField,&source);
    activeSharedTreeMetadata=&value;active=true;
    auto sequence=++sharedTreeMetadataRoutes;
    if(sequence<=128)Log("[DEMAND-SHARED-METADATA-ROUTE] alias=%016llX source=%016llX name=%s manifest=%p proxy=%p",
      input.key,input.categorySource,input.name.c_str(),manifest,proxy);
  }
  ~SharedTreeMetadataScope(){
    if(!active)return;
    il2cpp_field_set_value(value.proxy,sharedApi.proxyHashField,&previousKey);
    activeSharedTreeMetadata=value.previous;
  }
};
struct SharedOperationBundleKeyScope {
  void* operation=nullptr;
  size_t offset=0;
  uint64_t previous=0;
  bool active=false;
  SharedOperationBundleKeyScope(void* value,size_t fieldOffset,uint64_t source,uint64_t alias)
      :operation(value),offset(fieldOffset),previous(source),active(value&&source!=alias){
    if(active)memcpy(static_cast<uint8_t*>(operation)+offset,&alias,sizeof(alias));
  }
  ~SharedOperationBundleKeyScope(){
    if(active)memcpy(static_cast<uint8_t*>(operation)+offset,&previous,sizeof(previous));
  }
};
using SharedNameFn=void*(*)(void*,uint64_t,void*);
using SharedIntFn=int32_t(*)(void*,uint64_t,void*);
using SharedCategoryFn=uint8_t(*)(void*,uint64_t,void*);
using SharedLoadFn=void*(*)(void*,void*);
using SharedVfsQueryFn=uint8_t(*)(void*,void*,void*,void*);
SharedNameFn originalSharedName=nullptr,originalSharedUnsafe=nullptr;
SharedIntFn originalSharedFlags=nullptr;
SharedCategoryFn originalSharedCategory=nullptr;
StartupDependencies::Query originalSharedReverse=nullptr;
SharedLoadFn originalSharedLoad=nullptr,originalSharedLoadAsync=nullptr;
SharedVfsQueryFn originalSharedVfsQuery=nullptr;
using SharedProxyLookupFn=void*(*)(void*,uint64_t,bool,void*);
using SharedDependencyReadyFn=bool(*)(void*,void*);
using SharedPriorityAssetBundleFn=void(*)(void*,void*,int64_t,void*);
SharedProxyLookupFn originalSharedProxyLookup=nullptr;
SharedDependencyReadyFn originalSharedDependencyReady=nullptr;
SharedPriorityAssetBundleFn originalSharedPriorityAssetBundle=nullptr;
using SharedUnloadDependenciesFn=void(*)(void*,void*,void*);
SharedUnloadDependenciesFn originalSharedUnloadDependencies=nullptr;
// IL2CPP instance-method trampolines carry a trailing MethodInfo*; forward it unchanged.
using ProxyLoadFn=uint8_t(*)(void*,void*);
using ProxyImmediateFn=void(*)(void*,void*);
ProxyLoadFn originalSharedProxyLoadSync=nullptr,originalSharedProxyLoadAsync=nullptr;
ProxyImmediateFn originalSharedProxyLoadImmediate=nullptr,originalSharedProxyUpdateLoading=nullptr;
std::atomic<unsigned> sharedProxyObservations{0},sharedDependencyWaitObservations{0};
std::atomic<uint64_t> sharedDependencyQueries{0},sharedQueueCalls{0},sharedQueueAppends{0},sharedQueueRelevant{0};
std::atomic<uint64_t> sharedQueueCacheHits{0},sharedQueueCacheMisses{0},sharedQueueExpandCalls{0};
std::atomic<uint64_t> sharedNativeCacheHits{0},sharedNativeCacheMisses{0},sharedReadyCalls{0};
std::atomic<uint64_t> sharedReadyTrueCalls{0},sharedReadyFalseCalls{0};
std::atomic<uint64_t> sharedAssetTreeCalls{0},sharedLoadAttempts{0},sharedLoadMatches{0};
std::atomic<uint64_t> sharedLoadSubmits{0},sharedLoadCompletions{0},sharedLoadFailures{0},sharedPendingLoads{0};
std::atomic<uint64_t> sharedProxyFallbackLoads{0};
std::atomic<uint64_t> sharedVfsRouteMatches{0};
std::atomic<uint64_t> sharedVfsInfoAliases{0};
std::atomic<uint64_t> sharedVfsInfoObservations{0};
std::atomic<unsigned> sharedVfsInfoNameMisses{0};
std::atomic<uint64_t> sharedLoadTreeMatches{0},sharedFlagQueries{0},sharedCategoryQueries{0};
std::atomic<uint64_t> sharedQueueFastRejects{0},sharedProxyLoadObservations{0};
std::atomic<uint64_t> sharedProxyPumpObservations{0},sharedProxyRetireObservations{0};
std::atomic<unsigned> sharedAssetTreeObservations{0};
std::atomic<unsigned> sharedPrefabPreflightObservations{0};
std::atomic<unsigned> sharedPriorityObservations{0};
std::mutex sharedQueryLogMutex;
std::set<std::pair<uint64_t,bool>> sharedDependencyLogged,sharedQueueLogged;
using TreeSyncFn=void(*)(void*,void*,void*,int32_t,void*);
using TreeFn=void(*)(void*,void*,void*,int32_t,int32_t,bool,int32_t,void*);
using AssetTreeFn=void*(*)(void*,void*,int32_t,bool,int32_t,void*);
TreeSyncFn originalTreeSync=nullptr;TreeFn originalTree=nullptr;AssetTreeFn originalAssetTree=nullptr;
using OperationUpdateFn=void(*)(void*,void*,void*);
OperationUpdateFn originalSharedOperationUpdate=nullptr;
std::atomic<uint64_t> sharedOperationUpdateCalls{0};
std::atomic<unsigned> sharedTreeRawObservations{0},sharedTreeScopeMisses{0},sharedPathMissEvents{0};
std::atomic<bool> sharedTreeRawArmed{false};
std::atomic<unsigned> sharedIdentityEvents{0};
std::atomic<bool> sharedInputsEnabled{false};
void MarkSharedProxyActive(const std::shared_ptr<SharedScope>&,uint64_t);
bool SharedInput(void* manifest,uint64_t hash,EiemDemandSharedGraph::Input& input){
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandLookup);
  if(!sharedInputsEnabled)return false;
  auto* route=activeSharedTreeMetadata;
  if(route&&route->manifest==manifest&&route->source==hash&&route->shared&&
      route->shared->manifest->object.Get()==manifest&&route->shared->graph.Find(route->alias,input)&&
      input.categorySource==route->source)return performance.hit=true;
  auto shared=ExistingShared(manifest,hash);return performance.hit=shared&&shared->graph.Find(hash,input);
}
void* SharedName(void* manifest,uint64_t hash,void* info){EiemDemandSharedGraph::Input input;
  if(!SharedInput(manifest,hash,input))return originalSharedName(manifest,hash,info);
  if(++sharedIdentityEvents<=64)Log("[DEMAND-SHARED-IDENTITY] key=%016llX name=%s boundary=string",hash,input.name.c_str());
  return il2cpp_string_new(input.name.c_str());
}
void* SharedUnsafe(void* manifest,uint64_t hash,void* info){EiemDemandSharedGraph::Input input;
  if(!SharedInput(manifest,hash,input))return originalSharedUnsafe(manifest,hash,info);
  if(++sharedIdentityEvents<=64)Log("[DEMAND-SHARED-IDENTITY] key=%016llX name=%s boundary=unsafe-string",hash,input.name.c_str());
  auto label=Ref::Make(il2cpp_string_new(input.name.c_str()),false);void* args[]={label.Get()};return Call(sharedApi.unsafeFromString,nullptr,args);
}
int32_t SharedFlags(void* manifest,uint64_t hash,void* info){EiemDemandSharedGraph::Input input;
  if(!SharedInput(manifest,hash,input))return originalSharedFlags(manifest,hash,info);
  ++sharedFlagQueries;auto sequence=sharedFlagQueries.load();if(sequence<=16){auto sourceFlags=originalSharedFlags(manifest,input.categorySource,info);
    Log("[DEMAND-SHARED-METADATA] key=%016llX name=%s flags=0 source=%016llX sourceFlags=%d",hash,input.name.c_str(),input.categorySource,sourceFlags);}
  return 0;
}
uint8_t SharedCategory(void* manifest,uint64_t hash,void* info){EiemDemandSharedGraph::Input input;
  if(!SharedInput(manifest,hash,input))return originalSharedCategory(manifest,hash,info);
  ++sharedCategoryQueries;return originalSharedCategory(manifest,input.categorySource,info);
}
std::vector<uint64_t> NativeDependencies(void* manifest,uint64_t hash,void* prototype,bool& complete){
  auto list=StartupDependencies::NewList(prototype);
  complete=StartupDependencies::originalAll(manifest,hash,list.Get(),StartupDependencies::api.allInfo);
  auto result=StartupDependencies::ReadList(list.Get());result.erase(std::remove(result.begin(),result.end(),hash),result.end());return result;
}
std::vector<uint64_t> SharedAdded(const std::shared_ptr<SharedScope>& shared,void* manifest,uint64_t hash,const std::vector<uint64_t>& before,bool direct,void* prototype){
  auto edges=[&](uint64_t key){return shared->graph.Edges(key);};
  auto native=[&](uint64_t key){std::vector<uint64_t> cached;if(shared->graph.Native(key,cached)){++sharedNativeCacheHits;return cached;}
    EiemDemandSharedGraph::Input input;if(shared->graph.Find(key,input))return std::vector<uint64_t>{};
    ++sharedNativeCacheMisses;bool complete=false;auto resolved=NativeDependencies(manifest,key,prototype,complete);
    if(complete)shared->graph.CacheNative(key,resolved);return resolved;};
  // A synthetic Bundle is absent from the game's manifest. Its native
  // dependency list must be supplied here, including generation aliases.
  std::vector<uint64_t> inherited,seed=before,added;EiemDemandSharedGraph::Input input;
  if(shared->graph.Find(hash,input)&&input.versionedRoot){
    if(direct){
      auto list=prototype?StartupDependencies::NewList(prototype):StartupDependencies::NewListForManifest(manifest);
      StartupDependencies::originalDirect(manifest,input.categorySource,list.Get(),StartupDependencies::api.directInfo);
      inherited=StartupDependencies::ReadList(list.Get());
      for(auto& key:inherited){uint64_t alias=0;if(shared->bundleNamespace.Alias(key,alias))key=alias;}
    }else shared->graph.Native(hash,inherited);
  }
  std::set<uint64_t> seen(before.begin(),before.end());seen.insert(hash);
  for(auto key:inherited)if(seen.insert(key).second){seed.push_back(key);added.push_back(key);}
  auto expanded=EiemBundleDependencies::Added(hash,seed,direct,edges,native);
  added.insert(added.end(),expanded.begin(),expanded.end());return added;
}
uint64_t SharedVfsPathHash(const std::string& value){
  auto path=FullResourceReload::Path(value);uint64_t hash=0;
  il2cpp_field_get_value(path.Get(),FullResourceReload::api.pathHashField,&hash);
  Require(hash,"shared native VFS path hash is zero: "+value);return hash;
}
uint64_t SharedVfsHash(const std::string& name){
  return SharedVfsPathHash("Data/Bundles/Windows/"+name);
}
#include "eiem_entry_demand_bundle_handoff.h"
void* SubmitSharedInput(const EiemDemandSharedGraph::Input& input,uint32_t crc,bool asynchronous,const char* boundary){
  auto started=std::chrono::steady_clock::now();if(asynchronous)++sharedPendingLoads;
#if defined(EIEM_SCRIPT_BINDING_PROBE)
  EiemScriptBindingProbe::Input(0,input.key,input.cab.c_str(),false,input.versionedRoot);
#endif
  try{const auto retired=RetirePreviousSharedInput(input);RememberNativeCabAliases(input);
    // A cached root can outlive its previous observation token.
    const auto record=input.observationScope?registry.Find(input.observationScope,input.categorySource):Q::Record{};
    const auto token=record.name==input.cab?record.token:0;
    auto completed=[key=input.key,cab=input.cab,token,started,asynchronous,boundary=std::string(boundary)](void* bundle){
      Submitted(token,bundle,false);
      if(asynchronous)--sharedPendingLoads;++sharedLoadCompletions;if(!bundle)++sharedLoadFailures;
#if defined(EIEM_SCRIPT_BINDING_PROBE)
      EiemScriptBindingProbe::Input(2,key,cab.c_str(),bundle!=nullptr);
#endif
      Log("[DEMAND-SHARED-COMPLETE] route=%s key=%016llX cab=%s async=%d bundle=%p elapsedMs=%.3f",
        boundary.c_str(),key,cab.c_str(),int(asynchronous),bundle,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());};
    auto result=NativeBundle::LoadView(input.view,crc,asynchronous,asynchronous?std::move(completed):std::function<void(void*)>{},input.generationIsolated?input.name:input.cab);
    if(!result){if(asynchronous)--sharedPendingLoads;++sharedLoadFailures;}
    else{++sharedLoadSubmits;if(asynchronous)Submitted(token,result,true);else completed(result);}
#if defined(EIEM_SCRIPT_BINDING_PROBE)
    EiemScriptBindingProbe::Input(1,input.key,input.cab.c_str(),result!=nullptr);
#endif
    Log("[DEMAND-SHARED-SUBMIT] route=%s key=%016llX cab=%s async=%d crc=%u result=%p inputBytes=%zu priorGenerationBundlesRetired=%zu gameProxyOwnsRequest=1",
      boundary,input.key,input.cab.c_str(),int(asynchronous),crc,result,input.view?input.view->Size():0,retired);return result;
  }catch(const std::exception& e){if(asynchronous)--sharedPendingLoads;++sharedLoadFailures;++gaps;
    Log("[DEMAND-SHARED-GAP] phase=submit route=%s key=%016llX reason=%s",boundary,input.key,e.what());return nullptr;}
}
bool SharedVfsLoad(uint64_t vfsHash,uint32_t crc,bool asynchronous,void*& result)noexcept{
  result=nullptr;if(!sharedInputsEnabled)return false;
  try{std::shared_ptr<SharedScope> shared;EiemDemandSharedGraph::Input input;
    if(activeSharedLoadRoute&&activeSharedLoadRoute->shared&&activeSharedLoadRoute->input.key){
      uint64_t route=0;{std::lock_guard<std::mutex> lock(activeSharedLoadRoute->shared->vfsRouteMutex);
        auto found=activeSharedLoadRoute->shared->vfsRoutes.find(vfsHash);
        if(found!=activeSharedLoadRoute->shared->vfsRoutes.end())route=found->second;}
      if(route==activeSharedLoadRoute->input.key){shared=activeSharedLoadRoute->shared;input=activeSharedLoadRoute->input;}
    }
    if(!shared&&!SharedInputByVfsHash(vfsHash,shared,input)){RetireSyntheticForNativeVfs(vfsHash);return false;}
    if(!input.view){RetireSyntheticForNativeVfs(vfsHash);return false;}
    ++sharedVfsRouteMatches;auto sequence=sharedVfsRouteMatches.load();if(sequence<=64)
      Log("[DEMAND-SHARED-VFS-ROUTE] hash=%016llX key=%016llX name=%s async=%d crc=%u inputBytes=%zu gameBundleProxy=1",
        vfsHash,input.key,input.name.c_str(),int(asynchronous),crc,input.view->Size());
    // A matched failure stays a failed game load. Falling through here would
    // silently load the original bytes into an already selected Mod graph.
    ++sharedLoadAttempts;++sharedLoadMatches;result=SubmitSharedInput(input,crc,asynchronous,"vfs-load");return true;
  }catch(const std::exception& e){++gaps;Log("[DEMAND-SHARED-GAP] phase=vfs-route hash=%016llX reason=%s originalFallbackBlocked=1",vfsHash,e.what());return true;}
   catch(...){++gaps;Log("[DEMAND-SHARED-GAP] phase=vfs-route hash=%016llX reason=unknown originalFallbackBlocked=1",vfsHash);return true;}
}
uint8_t SharedVfsGetFileInfo(void* self,void* path,void* info,void* methodInfo){
  auto found=originalSharedVfsQuery(self,path,info,methodInfo);if(!sharedInputsEnabled||!path||!info)return found;
  if(!SharedSyntheticPath(path))return found;
  try{auto name=Q::Name(String(path));const std::string prefix="data/bundles/windows/";auto at=name.find(prefix);
    if(at!=std::string::npos)name.erase(0,at+prefix.size());
    if(name.rfind("eff/",0)!=0)return found;
    std::shared_ptr<SharedScope> owner;EiemDemandSharedGraph::Input input;
    if(!SharedInputByName(name,owner,input)){auto sequence=++sharedVfsInfoNameMisses;if(sequence<=32)
        Log("[DEMAND-SHARED-VFS-INFO-MISS] path=%s originalFound=%d",name.c_str(),int(found));return found;}
    const auto vfsHash=SharedVfsHash(input.name);
    {std::lock_guard<std::mutex> lock(owner->vfsRouteMutex);auto route=owner->vfsRoutes.find(vfsHash);
      if(route==owner->vfsRoutes.end()||route->second!=input.key)return found;}
    if(found){auto sequence=++sharedVfsInfoObservations;if(sequence<=32)
        Log("[DEMAND-SHARED-VFS-INFO-PRESENT] name=%s key=%016llX originalFound=1",name.c_str(),input.key);
      return found;}
    auto manifest=ManifestForScope(owner->serial);if(!manifest){
      Log("[DEMAND-SHARED-GAP] phase=vfs-info-alias name=%s key=%016llX reason=manifest-scope-expired",name.c_str(),input.key);return found;}
    auto sourceKey=input.categorySource;void* args[]={&sourceKey};
    auto sourceName=String(RuntimeShapes::Call(StartupDependencies::api.bundleName,manifest->object.Get(),args));
    auto sourcePath=Ref::Make(il2cpp_string_new(("Data/Bundles/Windows/"+sourceName).c_str()),false);
    Bytes sourceInfo(sharedApi.infoSize);auto sourceFound=originalSharedVfsQuery(self,sourcePath.Get(),sourceInfo.data(),methodInfo);
    if(!sourceFound){Log("[DEMAND-SHARED-GAP] phase=vfs-info-alias name=%s source=%s reason=native-root-file-info-missing",name.c_str(),sourceName.c_str());return found;}
    uint64_t sourceOffset=0,sourceLength=0;
    memcpy(&sourceOffset,sourceInfo.data()+sharedApi.offset,sharedApi.offsetWidth);
    memcpy(&sourceLength,sourceInfo.data()+sharedApi.length,sharedApi.lengthWidth);
    memcpy(info,sourceInfo.data(),sharedApi.infoSize);auto sequence=++sharedVfsInfoAliases;
    if(sequence<=64)Log("[DEMAND-SHARED-VFS-INFO-ALIAS] name=%s root=%s key=%016llX offset=%llu length=%llu queryOnly=1 resourceBytesRead=0",
      name.c_str(),sourceName.c_str(),input.key,(unsigned long long)sourceOffset,(unsigned long long)sourceLength);
    return 1;
  }catch(const std::exception& e){++gaps;Log("[DEMAND-SHARED-GAP] phase=vfs-info-alias reason=%s",e.what());return found;}
}
void SharedUnloadDependencies(void* manager,void* proxy,void* info){
  if(!sharedInputsEnabled||!proxy){originalSharedUnloadDependencies(manager,proxy,info);return;}
  auto manifest=Call(StartupDependencies::api.managerManifest,manager);uint64_t hash=0;
  il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&hash);
  EiemDemandSharedGraph::Input input;
  if(!SharedInput(manifest,hash,input)){originalSharedUnloadDependencies(manager,proxy,info);return;}
  // The native manager inlines its direct-dependency manifest lookup here.
  // Synthetic keys never enter that table, so the query hook cannot release
  // their references. Preserve the same unloaded guard and native release API.
  if(!RuntimeShapes::Scalar<uint8_t>(StartupDependencies::api.proxyIsUnloaded,proxy))return;
  const auto graphKey=input.key;auto shared=ExistingShared(manifest,graphKey);
  auto dependencies=SharedAdded(shared,manifest,graphKey,{},true,nullptr);
  std::vector<Ref> proxies;proxies.reserve(dependencies.size());
  for(auto key:dependencies){void* args[]={&key};auto dependency=Call(sharedApi.managerTryGetBundleProxy,manager,args);
    if(dependency)proxies.push_back(Ref::Make(dependency,false));}
  // Resolve first; do not retry a partially executed ReleaseBundleRef batch.
  for(const auto& dependency:proxies){void* args[]={proxy};Call(sharedApi.proxyReleaseBundleRef,dependency.Get(),args);}
  Log("[DEMAND-SHARED-DEPENDENCIES-RELEASED] key=%016llX direct=%zu released=%zu gameProxyOwnsReferences=1 extraLoads=0",
    graphKey,dependencies.size(),proxies.size());
}
bool SharedDependencies(void* manifest,uint64_t hash,void* list,bool direct,bool& result) noexcept {
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandDependencies);
  // Already-published synthetic inputs remain queryable by UpdateLoading and
  // IsAllDependenciesLoaded after the load-tree stack has unwound. Querying
  // their graph does not prepare inputs or submit loads. Unmatched/native keys
  // still use the load-tree guard in AppendSharedDependencies below.
  EiemDemandSharedGraph::Input input;if(!SharedInput(manifest,hash,input))return false;performance.hit=true;
  try{const auto graphKey=input.key;auto shared=ExistingShared(manifest,graphKey);auto before=StartupDependencies::ReadList(list);auto added=SharedAdded(shared,manifest,graphKey,before,direct,list);
    for(auto key:added)StartupDependencies::Append(list,key);result=true;++sharedDependencyQueries;
    bool report=false;{std::lock_guard<std::mutex> lock(sharedQueryLogMutex);report=sharedDependencyLogged.emplace(graphKey,direct).second;}
    if(report)Log("[DEMAND-SHARED-DEPENDENCY-QUERY] key=%016llX name=%s direct=%d declared=%zu nativeBefore=%zu added=%zu",
      graphKey,input.name.c_str(),int(direct),shared->graph.Edges(graphKey).size(),before.size(),added.size());}
  catch(const std::exception& e){result=false;++gaps;Log("[DEMAND-SHARED-GAP] phase=synthetic-dependencies reason=%s",e.what());}
  return true;
}
bool AppendSharedDependencies(void* manifest,uint64_t hash,void* list,bool direct,bool result) noexcept {
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandDependencies);
  if(!EiemDemandDependencyScope::MayAugment(sharedInputsEnabled,sharedRequestDepth))return result;
  ++sharedQueueCalls;
  try{auto shared=ExistingShared(manifest,hash);if(!shared)return result;
    if(direct&&!shared->graph.HasRootExpansion(hash)){++sharedQueueFastRejects;return result;}
    auto before=StartupDependencies::ReadList(list);std::vector<uint64_t> added;
    auto revision=shared->graph.Revision();
    if(shared->dependencyExpansions.Read(revision,hash,direct,before,added))++sharedQueueCacheHits;
    else {
      ++sharedQueueCacheMisses;
      if(!shared->graph.HasExpansion(hash,before,direct))return result;
      ++sharedQueueRelevant;++sharedQueueExpandCalls;performance.hit=true;
      added=SharedAdded(shared,manifest,hash,before,direct,list);
      auto afterRevision=shared->graph.Revision();
      if(afterRevision==revision)shared->dependencyExpansions.Store(revision,hash,direct,before,added);
    }
    for(auto key:added)StartupDependencies::Append(list,key);
    if(!added.empty()){++sharedQueueAppends;bool report=false;{std::lock_guard<std::mutex> lock(sharedQueryLogMutex);report=sharedQueueLogged.emplace(hash,direct).second;}
      if(report)Log("[DEMAND-SHARED-QUEUE] bundle=%016llX direct=%d nativeBefore=%zu added=%zu listCount=%zu requestDepth=%u",
        hash,int(direct),before.size(),added.size(),StartupDependencies::ReadList(list).size(),sharedRequestDepth);}
    return result||!added.empty();
  }catch(const std::exception& e){++gaps;Log("[DEMAND-SHARED-GAP] phase=append reason=%s",e.what());return result;}
}
bool SharedReverse(void* manifest,uint64_t hash,void* list,void* info){
  EiemDemandSharedGraph::Input input;bool own=SharedInput(manifest,hash,input);const auto graphKey=own?input.key:hash;
  auto result=own?true:originalSharedReverse(manifest,hash,list,info);
  try{auto shared=ExistingShared(manifest,graphKey);if(!shared)return result;
    auto before=StartupDependencies::ReadList(list);std::set<uint64_t> present(before.begin(),before.end());
    auto added=shared->graph.Reverse(graphKey);for(auto key:added)if(present.insert(key).second)StartupDependencies::Append(list,key);
    return result||!added.empty();
  }catch(const std::exception& e){++gaps;Log("[DEMAND-SHARED-GAP] phase=reverse reason=%s",e.what());return result;}
}
void* LoadSharedInput(void* path,void* info,bool asynchronous){
  ++sharedLoadAttempts;
  auto name=Q::Name(String(path));const std::string prefix="data/bundles/windows/";
  auto at=name.find(prefix);if(at!=std::string::npos)name.erase(0,at+prefix.size());
  EiemDemandSharedGraph::Input input;std::shared_ptr<SharedScope> owner;bool matched=false;
  if(sharedInputsEnabled&&activeSharedLoadRoute&&activeSharedLoadRoute->input.name==name){input=activeSharedLoadRoute->input;matched=true;}
  if(sharedInputsEnabled&&!matched)matched=SharedInputByName(name,owner,input);
  if(!matched){if(name.find("eff/")!=std::string::npos){++sharedPathMissEvents;if(sharedPathMissEvents<=64)
      Log("[DEMAND-SHARED-PATH-MISS] path=%s async=%d graphNameLookup=0",name.c_str(),int(asynchronous));}
    if(sharedInputsEnabled&&nativeCabAliasesPresent.load(std::memory_order_relaxed)&&name.rfind("eff/",0)!=0)try{RetireSyntheticForNativeVfs(SharedVfsHash(name));}
      catch(const std::exception& error){++gaps;Log("[DEMAND-SHARED-GAP] phase=native-return name=%s reason=%s originalFallbackBlocked=1",name.c_str(),error.what());return nullptr;}
    return (asynchronous?originalSharedLoadAsync:originalSharedLoad)(path,info);}
  ++sharedLoadMatches;
  if(!input.view)return (asynchronous?originalSharedLoadAsync:originalSharedLoad)(path,info);
  // The original BundleProxy receives an actual Unity request. Its own status,
  // cancellation, retry, reference counts and unload path remain authoritative.
  return SubmitSharedInput(input,0,asynchronous,"proxy-file");
}
void* SharedLoad(void* path,void* info){return LoadSharedInput(path,info,false);}
void* SharedLoadAsync(void* path,void* info){return LoadSharedInput(path,info,true);}
bool SharedTreeInput(void* manager,void* proxy,uint64_t& key,EiemDemandSharedGraph::Input& input,
    std::shared_ptr<SharedScope>* owner=nullptr,void** manifestOut=nullptr)noexcept{
  try{if(!proxy)return false;il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&key);
    auto manifest=Call(StartupDependencies::api.managerManifest,manager);
    auto* route=activeSharedTreeMetadata;
    if(route&&route->manifest==manifest&&route->source==key&&route->shared&&
        route->shared->graph.Find(route->alias,input)&&input.categorySource==route->source){
      key=route->alias;if(owner)*owner=route->shared;if(manifestOut)*manifestOut=manifest;return true;}
    if(SharedInput(manifest,key,input)){if(owner)*owner=ExistingShared(manifest,key);if(manifestOut)*manifestOut=manifest;return true;}
    std::vector<std::shared_ptr<SharedScope>> active;{std::lock_guard<std::mutex> lock(mutex);for(const auto& row:sharedScopes)active.push_back(row.second);}
    for(const auto& scope:active)if(scope->graph.Find(key,input)){auto sequence=++sharedTreeScopeMisses;
      if(sequence<=64)Log("[DEMAND-SHARED-TREE-SCOPE-MISS] key=%016llX name=%s managerManifest=%p graphScopeMismatch=1",
        key,input.name.c_str(),manifest);return false;}
    return false;
  }catch(...){return false;}
}
void ObserveTreeEntry(const char* method,void* manager,void* main)noexcept{
  if(!sharedTreeRawArmed)return;unsigned sequence=0;
  try{const char* ns="?";const char* type="?";uint64_t key=0;bool proxyType=false,matched=false;void* manifest=nullptr;
    if(main){auto klass=il2cpp_object_get_class(main);if(klass){ns=il2cpp_class_get_namespace(klass);type=il2cpp_class_get_name(klass);
      proxyType=type&&!strcmp(type,"BundleProxy");if(proxyType)il2cpp_field_get_value(main,sharedApi.proxyHashField,&key);}}
    manifest=Call(StartupDependencies::api.managerManifest,manager);EiemDemandSharedGraph::Input input;
    matched=proxyType&&SharedInput(manifest,key,input);bool knownElsewhere=matched;
    if(proxyType&&!knownElsewhere){std::vector<std::shared_ptr<SharedScope>> active;
      {std::lock_guard<std::mutex> lock(mutex);for(const auto& row:sharedScopes)active.push_back(row.second);}
      for(const auto& scope:active)if(scope->graph.Find(key,input)){knownElsewhere=true;break;}}
    if(!knownElsewhere)return;sequence=++sharedTreeRawObservations;if(sequence>96)return;
    Log("[DEMAND-SHARED-TREE-RAW] method=%s call=%u main=%p type=%s.%s key=%016llX manifest=%p shared=%d graphKnown=%d name=%s",
      method,sequence,main,ns?ns:"?",type?type:"?",key,manifest,int(matched),int(knownElsewhere),input.name.c_str());
  }catch(const std::exception& e){Log("[DEMAND-SHARED-TREE-RAW-GAP] method=%s call=%u reason=%s",method,sequence,e.what());}
   catch(...){Log("[DEMAND-SHARED-TREE-RAW-GAP] method=%s call=%u reason=unknown",method,sequence);}
}
void* SharedGetOrCreate(void* manager,uint64_t hash,bool cancelRecycle,void* info){
  if(!sharedInputsEnabled)return originalSharedProxyLookup(manager,hash,cancelRecycle,info);
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandProxySelect);
  void* manifest=nullptr;
  try{manifest=Call(StartupDependencies::api.managerManifest,manager);
    // Some game paths select a cached BundleProxy before querying dependencies.
    // A real request may also discover dependency-only targets whose logical
    // identity is absent from the public manifest. Negative groups are cached.
    EiemDemandSharedGraph::Input published;
    if(sharedRequestDepth&&manifest&&!SharedInput(manifest,hash,published)){auto state=Current(manifest);
      auto verdict=CachedCandidate(state,hash);
      if(state->bundles.count(hash)||verdict==Q::Candidate::Yes||verdict==Q::Candidate::Target||!state->unresolved.empty()||!state->unresolvedRestorations.empty()){
        auto prototype=StartupDependencies::NewListForManifest(manifest);
        PrepareDependencyGroup(state,hash,prototype.Get());
      }
    }
  }catch(const std::exception& e){auto sequence=++sharedProxyObservations;if(sequence<=32)
    Log("[DEMAND-SHARED-PROXY-GAP] phase=prepare key=%016llX reason=%s nativeLookupStillRequired=1",hash,e.what());}
   catch(...){auto sequence=++sharedProxyObservations;if(sequence<=32)
    Log("[DEMAND-SHARED-PROXY-GAP] phase=prepare key=%016llX reason=unknown nativeLookupStillRequired=1",hash);}
  // Preparation is optional input work; it must never replace the game's
  // GetOrCreate result with an uninitialised nullptr. Resolve a previously
  // committed generation alias even if another preparation attempt failed.
  uint64_t selected=hash;bool aliased=false;
  try{auto current=ExistingShared(manifest);aliased=current&&current->bundleNamespace.Alias(hash,selected);}
  catch(const std::exception& e){auto sequence=++sharedProxyObservations;if(sequence<=32)
    Log("[DEMAND-SHARED-PROXY-GAP] phase=select key=%016llX reason=%s",hash,e.what());}
  // Call exactly once, outside EFF's exception handlers. Native failures
  // retain the game's own semantics rather than masquerading as EFF success.
  performance.Pause();auto result=originalSharedProxyLookup(manager,selected,cancelRecycle,info);performance.Resume();
  performance.hit=aliased;
  try{EiemDemandSharedGraph::Input input;
    if(aliased){auto current=ExistingShared(manifest);
      auto sequence=++sharedProxyObservations;if(sequence<=128)
        Log("[DEMAND-SHARED-ROOT-ALIAS] generation=%llu source=%016llX alias=%016llX proxy=%p independentBundleProxy=1",
          (unsigned long long)(current?current->serial:0),hash,selected,result);
      return result;
    }
    if(SharedInput(manifest,hash,input)){performance.hit=true;
      auto sequence=++sharedProxyObservations;if(sequence<=128)
        Log("[DEMAND-SHARED-PROXY] key=%016llX name=%s proxy=%p createdByGame=1 ordinal=%u",
          hash,input.name.c_str(),result,sequence);
      return result;
    }
    bool knownElsewhere=false;std::vector<std::shared_ptr<SharedScope>> active;
    {std::lock_guard<std::mutex> lock(mutex);for(const auto& row:sharedScopes)active.push_back(row.second);}
    for(const auto& scope:active)if(scope->graph.Find(hash,input)){knownElsewhere=true;break;}
    if(knownElsewhere){auto sequence=++sharedProxyObservations;if(sequence<=128)
      Log("[DEMAND-SHARED-PROXY-GAP] key=%016llX name=%s proxy=%p reason=manifest-scope-mismatch",hash,input.name.c_str(),result);}
  }catch(const std::exception& e){auto sequence=++sharedProxyObservations;if(sequence<=32)
    Log("[DEMAND-SHARED-PROXY-GAP] key=%016llX proxy=%p reason=%s",hash,result,e.what());}
  return result;
}
bool SharedDependenciesReady(void* proxy,void* info){
  auto result=originalSharedDependencyReady(proxy,info);if(!sharedInputsEnabled)return result;
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandReady);
  try{uint64_t hash=0;void* manager=nullptr;il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&hash);
    il2cpp_field_get_value(proxy,StartupDependencies::api.proxyManagerField,&manager);
    auto manifest=Call(StartupDependencies::api.managerManifest,manager);
    EiemDemandSharedGraph::Input input;if(!SharedInput(manifest,hash,input))return result;hash=input.key;
    auto shared=ExistingShared(manifest,hash);bool changed=false;
    if(!shared||!shared->graph.FindAndObserveReadiness(hash,result,input,changed))return result;performance.hit=true;
    ++sharedReadyCalls;if(result)++sharedReadyTrueCalls;else ++sharedReadyFalseCalls;
    auto sequence=changed?++sharedDependencyWaitObservations:sharedDependencyWaitObservations.load();
#if defined(EIEM_SCRIPT_BINDING_PROBE)
    if(changed)EiemScriptBindingProbe::Input(3,hash,input.cab.c_str(),result);
#endif
    if(changed&&sequence<=128)Log("[DEMAND-SHARED-DEPENDENCIES] key=%016llX name=%s ready=%d transition=%u",
      hash,input.name.c_str(),int(result),sequence);
  }catch(const std::exception& e){auto sequence=++sharedDependencyWaitObservations;if(sequence<=16)
    Log("[DEMAND-SHARED-DEPENDENCIES-GAP] reason=%s",e.what());}
  return result;
}
bool SharedProxyInput(void* proxy,uint64_t& hash,EiemDemandSharedGraph::Input& input,std::shared_ptr<SharedScope>* owner=nullptr,void** manifestOut=nullptr)noexcept{
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandProxyIdentity);
  try{if(!proxy)return false;void* manager=nullptr;il2cpp_field_get_value(proxy,StartupDependencies::api.proxyManagerField,&manager);
    if(!manager)return false;il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&hash);
    auto manifest=Call(StartupDependencies::api.managerManifest,manager);auto* route=activeSharedTreeMetadata;
    if(route&&route->manifest==manifest&&route->proxy==proxy&&route->source==hash&&route->shared&&
        route->shared->graph.Find(route->alias,input)&&input.categorySource==route->source){
      hash=route->alias;if(owner)*owner=route->shared;if(manifestOut)*manifestOut=manifest;performance.hit=true;return true;}
    auto shared=ExistingShared(manifest,hash);if(!shared||!shared->graph.Find(hash,input))return false;
    if(manifestOut)*manifestOut=manifest;
    if(owner)*owner=std::move(shared);performance.hit=true;return true;
  }catch(const std::exception& e){Log("[DEMAND-PROXY-LOAD-GAP] phase=identity reason=%s",e.what());return false;}
   catch(...){Log("[DEMAND-PROXY-LOAD-GAP] phase=identity reason=unknown");return false;}
}
void LogSharedProxyState(const char* method,const char* phase,uint64_t hash,const EiemDemandSharedGraph::Input& input,void* proxy)noexcept{
  try{void* bundle=nullptr,*request=nullptr;il2cpp_field_get_value(proxy,sharedApi.proxyAssetBundleField,&bundle);
    il2cpp_field_get_value(proxy,sharedApi.proxyLoadRequestField,&request);
    int32_t operation=0,group=0;il2cpp_field_get_value(proxy,sharedApi.proxyLoadingOperationField,&operation);
    il2cpp_field_get_value(proxy,sharedApi.proxyLoadingGroupField,&group);
    auto flag=RuntimeShapes::Scalar<int>(sharedApi.proxyBundleFlag,proxy);
    auto loading=RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsLoading,proxy);
    auto succeeded=RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsSuccessToLoad,proxy);
    auto failed=RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsFailedToLoad,proxy);
    auto invalid=RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyHasErrorOrUnloaded,proxy);
    Log("[DEMAND-PROXY-LOAD] method=%s phase=%s key=%016llX name=%s flag=%d loading=%d succeeded=%d failed=%d errorOrUnloaded=%d operation=%d group=%d request=%p bundle=%p",
      method,phase,hash,input.name.c_str(),flag,int(loading),int(succeeded),int(failed),int(invalid),operation,group,request,bundle);
  }catch(const std::exception& e){Log("[DEMAND-PROXY-LOAD-GAP] method=%s phase=%s reason=%s",method,phase,e.what());}
   catch(...){Log("[DEMAND-PROXY-LOAD-GAP] method=%s phase=%s reason=unknown",method,phase);}
}
void SharedOperationUpdate(void* manager,void* operation,void* info){
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandOperation);
  uint64_t hash=0,operationHash=0;void* proxy=nullptr,*manifest=nullptr;EiemDemandSharedGraph::Input input;
  std::shared_ptr<SharedScope> owner;bool authored=false;
  try{if(operation){memcpy(&proxy,static_cast<const uint8_t*>(operation)+sharedApi.operationBundleProxyOffset,sizeof(proxy));
      if(proxy){il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&hash);manifest=Call(StartupDependencies::api.managerManifest,manager);
        memcpy(&operationHash,static_cast<const uint8_t*>(operation)+sharedApi.operationBundleHashNameOffset,sizeof(operationHash));
        authored=SharedInput(manifest,hash,input)&&
          (operationHash==input.categorySource||operationHash==input.key);
        if(authored)owner=ExistingShared(manifest,input.key);}}}
  catch(const std::exception& e){Log("[DEMAND-SHARED-OPERATION-GAP] phase=identify reason=%s",e.what());}
  if(authored){auto sequence=++sharedOperationUpdateCalls;if(sequence<=96){
      Log("[DEMAND-SHARED-OPERATION] phase=before sequence=%llu operation=%p proxy=%p bundleHashName=%016llX proxyKey=%016llX name=%s",
        (unsigned long long)sequence,operation,proxy,operationHash,hash,input.name.c_str());
      LogSharedProxyState("operation-update","before",hash,input,proxy);}}
  performance.hit=authored;performance.Pause();
  {
    // The scheduler looks up its BundleProxy with bundleHashName. Keep that
    // queue key in the same namespace as the generation-scoped proxy while
    // the native update runs, then restore the game's original key.
    SharedOperationBundleKeyScope bundleKey(authored?operation:nullptr,
      sharedApi.operationBundleHashNameOffset,operationHash,hash);
    originalSharedOperationUpdate(manager,operation,info);
  }
  performance.Resume();
  if(authored&&sharedOperationUpdateCalls.load()<=96){
    LogSharedProxyState("operation-update","after",hash,input,proxy);
    Log("[DEMAND-SHARED-OPERATION] phase=after operation=%p proxy=%p bundleHashName=%016llX proxyKey=%016llX name=%s",
      operation,proxy,operationHash,hash,input.name.c_str());}
}
void SharedForcePriorityAssetBundle(void* manager,void* assetProxy,int64_t bundleHashName,void* info){
  uint64_t selected=uint64_t(bundleHashName);bool aliased=false;
  if(sharedInputsEnabled){
    try{auto manifest=Call(StartupDependencies::api.managerManifest,manager);auto shared=ExistingShared(manifest);
      aliased=shared&&shared->bundleNamespace.Alias(uint64_t(bundleHashName),selected);
    }catch(const std::exception& error){auto sequence=++sharedPriorityObservations;if(sequence<=16)
        Log("[DEMAND-SHARED-PRIORITY-GAP] phase=resolve key=%016llX reason=%s",uint64_t(bundleHashName),error.what());
    }catch(...){auto sequence=++sharedPriorityObservations;if(sequence<=16)
        Log("[DEMAND-SHARED-PRIORITY-GAP] phase=resolve key=%016llX reason=unknown",uint64_t(bundleHashName));}
  }
  if(aliased){auto sequence=++sharedPriorityObservations;if(sequence<=128)
      Log("[DEMAND-SHARED-PRIORITY-ALIAS] source=%016llX alias=%016llX assetProxy=%p",
        uint64_t(bundleHashName),selected,assetProxy);}
  int64_t selectedKey=0;memcpy(&selectedKey,&selected,sizeof(selectedKey));
  originalSharedPriorityAssetBundle(manager,assetProxy,selectedKey,info);
}
bool CompleteSharedProxyImmediate(void* proxy,const EiemDemandSharedGraph::Input& input){
  if(!input.view||!sharedApi.proxyFinishWithBundle)return false;
  // LoadSync and the manager's fast path may call _LoadImmediate without ever
  // reaching the file/VFS detours. Submit only this already-matched input.
  // Existing requests still belong to the game's ForceFinishAsyncRequest path;
  // successful, failed and unloaded proxies retain the game's terminal rules.
  if(RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsSuccessToLoad,proxy))return true;
  if(RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyHasErrorOrUnloaded,proxy))return false;
  void* request=nullptr,*bundle=nullptr;
  il2cpp_field_get_value(proxy,sharedApi.proxyLoadRequestField,&request);
  il2cpp_field_get_value(proxy,sharedApi.proxyAssetBundleField,&bundle);
  if(request||bundle)return false;
  bundle=SubmitSharedInput(input,0,false,"proxy-sync-lifecycle");
  // This is the game's own completion method, including its null-Bundle error
  // path, request cleanup and operation reset. Never synthesize status values.
  void* args[]={bundle};RuntimeShapes::Call(sharedApi.proxyFinishWithBundle,proxy,args);
  auto sequence=++sharedProxyFallbackLoads;
  if(sequence<=64)Log("[DEMAND-PROXY-SYNC-COMPLETE] key=%016llX name=%s bundle=%p gameFinishWithBundle=1",
    input.key,input.name.c_str(),bundle);
  return true;
}
uint8_t SharedProxyLoadSync(void* proxy,void* info){
  uint64_t hash=0;EiemDemandSharedGraph::Input input;std::shared_ptr<SharedScope> owner;void* manifest=nullptr;
  if(!SharedProxyInput(proxy,hash,input,&owner,&manifest))return originalSharedProxyLoadSync(proxy,info);
  SharedLoadRouteScope route(manifest,owner,input);
  auto sequence=++sharedProxyLoadObservations;const bool report=sequence<=96;
  if(report)LogSharedProxyState("LoadSync","before",hash,input,proxy);
  CompleteSharedProxyImmediate(proxy,input);
  auto result=originalSharedProxyLoadSync(proxy,info);
  MarkSharedProxyActive(owner,hash);
  if(report)LogSharedProxyState("LoadSync","after",hash,input,proxy);
  return result;
}
uint8_t SharedProxyLoadAsync(void* proxy,void* info){
  uint64_t hash=0;EiemDemandSharedGraph::Input input;std::shared_ptr<SharedScope> owner;void* manifest=nullptr;
  if(!SharedProxyInput(proxy,hash,input,&owner,&manifest))return originalSharedProxyLoadAsync(proxy,info);
  SharedLoadRouteScope route(manifest,owner,input);
  auto sequence=++sharedProxyLoadObservations;const bool report=sequence<=96;
  if(report)LogSharedProxyState("LoadAsync","before",hash,input,proxy);
  auto result=originalSharedProxyLoadAsync(proxy,info);
  if(!result){
    void* request=nullptr,*bundle=nullptr;il2cpp_field_get_value(proxy,sharedApi.proxyLoadRequestField,&request);
    il2cpp_field_get_value(proxy,sharedApi.proxyAssetBundleField,&bundle);
    auto loading=RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsLoading,proxy);
    auto succeeded=RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsSuccessToLoad,proxy);
    auto failed=RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsFailedToLoad,proxy);
    auto invalid=RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyHasErrorOrUnloaded,proxy);
    if(input.view&&loading&&!succeeded&&!failed&&!invalid&&!request&&!bundle){
      request=SubmitSharedInput(input,0,true,"proxy-lifecycle-fallback");
      if(request){
        il2cpp_field_set_value_object(proxy,sharedApi.proxyLoadRequestField,request);
        void* retained=nullptr;il2cpp_field_get_value(proxy,sharedApi.proxyLoadRequestField,&retained);
        if(retained==request){
          result=1;auto fallback=++sharedProxyFallbackLoads;
          if(fallback<=64)Log("[DEMAND-PROXY-LOAD-FALLBACK] key=%016llX name=%s request=%p gameUpdateLoading=1",
            hash,input.name.c_str(),request);
        }else Log("[DEMAND-PROXY-LOAD-GAP] phase=request-bind key=%016llX request=%p retained=%p",
          hash,request,retained);
      }else if(sharedApi.proxyFinishWithBundle){
        // A null native submission cannot be pumped: there is no request for
        // UpdateLoading to finish. Use the game's terminal failure path rather
        // than leaving every dependent Prefab waiting on an empty operation.
        void* args[]={nullptr};RuntimeShapes::Call(sharedApi.proxyFinishWithBundle,proxy,args);
        Log("[DEMAND-PROXY-LOAD-FAILED] key=%016llX name=%s gameFinishWithBundle=1 emptyRequestWait=0",
          hash,input.name.c_str());
      }
    }
  }
  MarkSharedProxyActive(owner,hash);
  if(report){LogSharedProxyState("LoadAsync","after",hash,input,proxy);
    Log("[DEMAND-PROXY-LOAD-RETURN] method=LoadAsync key=%016llX result=%d",hash,int(result));}
  return result;
}
void SharedProxyUpdateLoading(void* proxy,void* info){
  uint64_t hash=0;EiemDemandSharedGraph::Input input;std::shared_ptr<SharedScope> owner;void* manifest=nullptr;
  if(!SharedProxyInput(proxy,hash,input,&owner,&manifest)){originalSharedProxyUpdateLoading(proxy,info);return;}
  SharedLoadRouteScope route(manifest,owner,input);
  auto sequence=++sharedProxyPumpObservations;const bool report=sequence<=96;
  if(report)LogSharedProxyState("UpdateLoading","before",hash,input,proxy);
  originalSharedProxyUpdateLoading(proxy,info);
  if(report)LogSharedProxyState("UpdateLoading","after",hash,input,proxy);
}
void SharedProxyLoadImmediate(void* proxy,void* info){
  uint64_t hash=0;EiemDemandSharedGraph::Input input;std::shared_ptr<SharedScope> owner;void* manifest=nullptr;
  if(!SharedProxyInput(proxy,hash,input,&owner,&manifest)){originalSharedProxyLoadImmediate(proxy,info);return;}
  SharedLoadRouteScope route(manifest,owner,input);
  auto sequence=++sharedProxyLoadObservations;const bool report=sequence<=96;
  if(report)LogSharedProxyState("_LoadImmediate","before",hash,input,proxy);
  if(!CompleteSharedProxyImmediate(proxy,input))originalSharedProxyLoadImmediate(proxy,info);
  MarkSharedProxyActive(owner,hash);
  if(report)LogSharedProxyState("_LoadImmediate","after",hash,input,proxy);
}
void TreeSync(void* self,void* pack,void* main,int32_t depth,void* info){SharedRequestScope scope(EiemDemandDependencyScope::Boundary::BundleTree);ObserveTreeEntry("sync",self,main);uint64_t key=0;void* manifest=nullptr;std::shared_ptr<SharedScope> owner;EiemDemandSharedGraph::Input input;
  bool shared=SharedTreeInput(self,main,key,input,&owner,&manifest);auto ordinal=shared?++sharedLoadTreeMatches:0;if(shared&&ordinal<=128)
    Log("[DEMAND-SHARED-TREE-BEGIN] key=%016llX name=%s async=0 depth=%d",key,input.name.c_str(),depth);
  SharedTreeMetadataScope metadata(manifest,main,owner,input);
  originalTreeSync(self,pack,main,depth,info);
  if(shared&&ordinal<=128)Log("[DEMAND-SHARED-TREE-END] key=%016llX name=%s async=0",key,input.name.c_str());}
void Tree(void* self,void* pack,void* main,int32_t group,int32_t depth,bool asynchronous,int32_t priority,void* info);
std::atomic<unsigned> sharedAsyncDependenciesQueued{0};
void QueueSharedAsyncDependencies(void* manager,void* pack,void* parent,int32_t group,int32_t depth,
    bool asynchronous,int32_t priority,void* manifest,const SharedScope& owner,
    const EiemDemandSharedGraph::Input& input){
  if(depth>=99)return;
  // Read the published edges, including dependencies attached to a reused
  // input by a later request group.
  const auto dependencies=owner.graph.Edges(input.key);
  if(dependencies.empty())return;
  int32_t operation=0;
  il2cpp_field_get_value(parent,sharedApi.proxyLoadingOperationField,&operation);
  if(operation||RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsSuccessToLoad,parent)||
      RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsLoading,parent)||
      RuntimeShapes::Scalar<uint8_t>(sharedApi.proxyIsFailedToLoad,parent))return;
  // The asynchronous tree reads the native manifest directly. Only the extra
  // graph edges need to be queued here. Each child still enters the same
  // native tree with the caller's group/priority and the native parent retain.
  std::set<uint64_t> original;
  if(input.versionedRoot){
    auto list=StartupDependencies::NewListForManifest(manifest);
    StartupDependencies::originalDirect(manifest,input.categorySource,list.Get(),StartupDependencies::api.directInfo);
    auto nativeDependencies=StartupDependencies::ReadList(list.Get());original.insert(nativeDependencies.begin(),nativeDependencies.end());
  }
  size_t queued=0;
  for(auto key:dependencies){
    if(!original.insert(key).second)continue;
    auto dependency=Ref::Make(SharedGetOrCreate(manager,key,true,sharedApi.proxyLookupInfo),false);
    Require(dependency.Get()&&dependency.Get()!=parent,"matched PFB dependency has no distinct game BundleProxy");
    Tree(manager,pack,dependency.Get(),group,depth+1,asynchronous,priority,nullptr);
    void* args[]={parent};Call(sharedApi.proxyRetainBundleRef,dependency.Get(),args);
    ++queued;
  }
  if(queued&&++sharedAsyncDependenciesQueued<=128)
    Log("[DEMAND-SHARED-ASYNC-EDGES] key=%016llX queued=%zu gameGroup=%d depth=%d",input.key,queued,group,depth);
}
void Tree(void* self,void* pack,void* main,int32_t group,int32_t depth,bool asynchronous,int32_t priority,void* info){SharedRequestScope scope(EiemDemandDependencyScope::Boundary::BundleTree);ObserveTreeEntry("async",self,main);uint64_t key=0;void* manifest=nullptr;std::shared_ptr<SharedScope> owner;EiemDemandSharedGraph::Input input;
  bool shared=SharedTreeInput(self,main,key,input,&owner,&manifest);auto ordinal=shared?++sharedLoadTreeMatches:0;if(shared&&ordinal<=128)
    Log("[DEMAND-SHARED-TREE-BEGIN] key=%016llX name=%s async=%d group=%d depth=%d",key,input.name.c_str(),int(asynchronous),group,depth);
  if(shared&&owner)try{QueueSharedAsyncDependencies(self,pack,main,group,depth,asynchronous,priority,manifest,*owner,input);}
  catch(const std::exception& error){++gaps;Log("[DEMAND-SHARED-GAP] phase=async-tree-edges key=%016llX reason=%s",key,error.what());}
  SharedTreeMetadataScope metadata(manifest,main,owner,input);
  originalTree(self,pack,main,group,depth,asynchronous,priority,info);
  if(shared&&ordinal<=128)Log("[DEMAND-SHARED-TREE-END] key=%016llX name=%s async=%d",key,input.name.c_str(),int(asynchronous));}
void* AssetTree(void* self,void* asset,int32_t group,bool asynchronous,int32_t priority,void* info){SharedRequestScope scope(EiemDemandDependencyScope::Boundary::AssetTree);
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandAssetTree);
  auto request=DemandLoadingProbe::PrefabAssetRequest(self,asset);
  performance.hit=bool(request.token);
  if(sharedInputsEnabled&&request.token){
    try{auto manifest=Call(StartupDependencies::api.managerManifest,self);auto state=Current(manifest);
      Require(state&&state->serial==request.scope,"Prefab request generation changed during preparation");
      auto prototype=StartupDependencies::NewListForManifest(manifest);
      if(PrepareDependencyGroup(state,request.bundle,prototype.Get())){
        uint64_t alias=0;auto prepared=ExistingShared(manifest);
        Require(prepared&&prepared->bundleNamespace.Alias(request.bundle,alias),"matched Prefab has no generation proxy alias before native cache lookup");
        Log("[DEMAND-PREFAB-GENERATION-READY] scope=%llu source=%016llX alias=%016llX beforeAssetTree=1",
          (unsigned long long)request.scope,request.bundle,alias);
      }
    }catch(const std::exception& e){++gaps;Log("[DEMAND-SHARED-GAP] phase=prefab-before-cache bundle=%016llX reason=%s",request.bundle,e.what());}
  }
  ++sharedAssetTreeCalls;
  auto ordinal=++sharedAssetTreeObservations;if(ordinal<=16)Log("[DEMAND-SHARED-REQUEST] ordinal=%u group=%d async=%d nativeAssetProxy=%p",ordinal,group,int(asynchronous),asset);
  performance.Pause();return originalAssetTree(self,asset,group,asynchronous,priority,info);}
void ReportShared(int frame){
  if(frame%300)return;size_t graphInputs=0;std::vector<std::shared_ptr<SharedScope>> active;
  {std::lock_guard<std::mutex> lock(mutex);for(const auto& row:sharedScopes)active.push_back(row.second);}
  for(const auto& scope:active)graphInputs+=scope->graph.Size();
  Log("[DEMAND-SHARED-STATS] frame=%d graphInputs=%zu proxyMatches=%u dependencyQueries=%llu queueCalls=%llu queueFastRejects=%llu queueRelevant=%llu queueExpansions=%llu queueAppends=%llu queueCacheHits=%llu queueCacheMisses=%llu nativeCacheHits=%llu nativeCacheMisses=%llu readinessCalls=%llu readinessTrue=%llu readinessFalse=%llu readinessTransitions=%u treeMatches=%llu assetTreeCalls=%llu proxyLoadObservations=%llu proxyPumpObservations=%llu operationUpdates=%llu proxyRetireObservations=%llu flagQueries=%llu categoryQueries=%llu vfsRouteMatches=%llu loadAttempts=%llu loadMatches=%llu loadSubmits=%llu loadCompletions=%llu loadFailures=%llu pendingLoads=%llu",
    frame,graphInputs,sharedProxyObservations.load(),sharedDependencyQueries.load(),sharedQueueCalls.load(),sharedQueueFastRejects.load(),sharedQueueRelevant.load(),sharedQueueExpandCalls.load(),sharedQueueAppends.load(),sharedQueueCacheHits.load(),sharedQueueCacheMisses.load(),
    sharedNativeCacheHits.load(),sharedNativeCacheMisses.load(),sharedReadyCalls.load(),sharedReadyTrueCalls.load(),sharedReadyFalseCalls.load(),sharedDependencyWaitObservations.load(),
     sharedLoadTreeMatches.load(),sharedAssetTreeCalls.load(),sharedProxyLoadObservations.load(),sharedProxyPumpObservations.load(),sharedOperationUpdateCalls.load(),sharedProxyRetireObservations.load(),sharedFlagQueries.load(),sharedCategoryQueries.load(),sharedVfsRouteMatches.load(),sharedLoadAttempts.load(),sharedLoadMatches.load(),sharedLoadSubmits.load(),
    sharedLoadCompletions.load(),sharedLoadFailures.load(),sharedPendingLoads.load());
}
void RetireSharedLocked(const std::shared_ptr<SharedScope>& shared,uint64_t hash,bool proxyRetired=true) {
  uint64_t rootHash=hash;shared->bundleNamespace.Source(hash,rootHash);
  const auto before=shared->graph.Size();EiemDemandSharedGraph::Input input;const bool authored=shared->graph.Find(hash,input);
  const bool prepared=shared->preparedGroups.erase(rootHash)!=0;
  bool nativeProxyActive=false;{std::lock_guard<std::mutex> lock(shared->lifetimeMutex);nativeProxyActive=shared->activeAuthorProxies.count(hash)!=0;}
  {std::lock_guard<std::mutex> lock(shared->usageMutex);if(!shared->effectRoots.count(rootHash)&&
    (proxyRetired||!nativeProxyActive))shared->rootUsage.erase(rootHash);}bool active=false;
  {std::lock_guard<std::mutex> lock(shared->lifetimeMutex);if(proxyRetired){active=shared->activeAuthorProxies.erase(hash)!=0;shared->pendingRetires.erase(hash);}else active=shared->activeAuthorProxies.count(hash)!=0;}
  if(!prepared&&!active&&!authored)return;
  std::vector<uint64_t> roots;roots.reserve(shared->preparedGroups.size());
  for(const auto& row:shared->preparedGroups)roots.push_back(row.first);
  for(const auto& row:shared->effectRoots)roots.push_back(row.first);
  {std::lock_guard<std::mutex> lock(shared->lifetimeMutex);roots.insert(roots.end(),shared->activeAuthorProxies.begin(),shared->activeAuthorProxies.end());}
  // Prepared groups are keyed by native source, while the game caches their
  // generation aliases. Reusing a loaded proxy need not emit another load
  // callback. Keep both identities reachable until their real owner retires.
  const auto nativeRoots=roots.size();
  for(size_t i=0;i<nativeRoots;++i){uint64_t alias=0;if(shared->bundleNamespace.Alias(roots[i],alias))roots.push_back(alias);}
  auto evicted=shared->graph.RetireUnreachable(roots);std::set<std::string> liveMods,liveMaterials;
  std::vector<std::pair<uint64_t,uint64_t>> retiredRoutes;
  {std::lock_guard<std::mutex> lock(shared->vfsRouteMutex);for(auto it=shared->vfsRoutes.begin();it!=shared->vfsRoutes.end();){
      EiemDemandSharedGraph::Input input;if(!shared->graph.Find(it->second,input)){
        retiredRoutes.emplace_back(it->first,it->second);it=shared->vfsRoutes.erase(it);
      }else ++it;}}
  if(!retiredRoutes.empty()){std::lock_guard<std::mutex> lock(sharedVfsRouteIndexMutex);
    for(const auto& route:retiredRoutes){auto found=sharedVfsRouteIndex.find(route.first);
      if(found!=sharedVfsRouteIndex.end()&&found->second.owner.lock()==shared&&found->second.key==route.second)
        sharedVfsRouteIndex.erase(found);}}
  std::set<Identity> liveLogicalDependencies;
  {std::lock_guard<std::mutex> lock(shared->usageMutex);for(const auto& row:shared->rootUsage){liveMods.insert(row.second.mods.begin(),row.second.mods.end());
    liveMaterials.insert(row.second.materials.begin(),row.second.materials.end());
    liveLogicalDependencies.insert(row.second.logicalDependencies.begin(),row.second.logicalDependencies.end());}}
  size_t materialCacheEvictions=0,logicalCacheEvictions=0;
  for(auto it=shared->materialVariants.begin();it!=shared->materialVariants.end();)if(!liveMaterials.count(it->first)){
    it=shared->materialVariants.erase(it);++materialCacheEvictions;}else ++it;
  for(auto it=shared->logicalDependencies.begin();it!=shared->logicalDependencies.end();)if(!liveLogicalDependencies.count(it->first)){
    it=shared->logicalDependencies.erase(it);++logicalCacheEvictions;}else ++it;
  if(roots.empty()){shared->cabKeys.clear();shared->inspectedBundleDirectories.clear();}
  std::vector<std::shared_ptr<EiemDemandAuthorResources::Catalog>> catalogs;
  // The generation's Catalogs are shared by all native scopes. Trim only
  // unleased assets even when another target of the same Mod remains alive.
  if(shared->manifest->catalog)for(const auto& row:shared->manifest->catalog->authors)
    if(row.second->catalog)catalogs.push_back(row.second->catalog);
  size_t removedMods=0;
  {
    std::lock_guard<std::mutex> lock(mutex);auto found=authors.find(shared->serial);
    if(found!=authors.end()){
      std::set<std::string> removed;
      for(auto it=found->second.begin();it!=found->second.end();)if(!liveMods.count(it->first)){
        if(it->second.author&&it->second.author->catalog&&std::find(catalogs.begin(),catalogs.end(),it->second.author->catalog)==catalogs.end())catalogs.push_back(it->second.author->catalog);
        removed.insert(it->first);it=found->second.erase(it);++removedMods;
      }else ++it;
      if(found->second.empty())authors.erase(found);
      if(!removed.empty()){
        auto parts=scopeParts.find(shared->serial);if(parts!=scopeParts.end())parts->second.erase(
          std::remove_if(parts->second.begin(),parts->second.end(),[&](const auto& part){
            for(const auto& mod:removed){const auto prefix=EiemComposedPackage::Prefix(mod);if(part.part.rfind(prefix,0)==0)return true;}
            return false;
          }),parts->second.end());
        if(parts!=scopeParts.end()&&parts->second.empty())scopeParts.erase(parts);
      }
    }
    if(liveMods.empty())scopeParts.erase(shared->serial);
  }
  EiemDemandAuthorResources::Catalog::CacheCounts cached;
  for(const auto& catalog:catalogs){auto counts=catalog->TrimUnusedAssetCaches();cached.meshes+=counts.meshes;cached.textures+=counts.textures;
    cached.materials+=counts.materials;cached.physics+=counts.physics;}
  const auto sequence=++sharedProxyRetireObservations;
  if(sequence<=256)Log("[DEMAND-LIFETIME-RECLAIM] scope=%llu key=%016llX authoredInput=%d preparedRoot=%d activeProxy=%d liveRoots=%zu graphInputs=%zu->%zu evictedViews=%zu evictedViewBytes=%zu evictedDependencyNodes=%zu releasedMods=%zu catalogMeshes=%zu catalogTextures=%zu catalogMaterials=%zu catalogPhysics=%zu materialVariants=%zu logicalDependencies=%zu",
    shared->serial,hash,int(authored),int(prepared),int(active),roots.size(),before,shared->graph.Size(),evicted.inputs,evicted.viewBytes,evicted.dependencyNodes,
    removedMods,cached.meshes,cached.textures,cached.materials,cached.physics,materialCacheEvictions,logicalCacheEvictions);
}
void DrainPendingSharedRetiresLocked(const std::shared_ptr<SharedScope>& shared) noexcept {
  try{std::vector<uint64_t> pending;{std::lock_guard<std::mutex> lock(shared->lifetimeMutex);pending.assign(shared->pendingRetires.begin(),shared->pendingRetires.end());shared->pendingRetires.clear();}
    for(auto hash:pending)RetireSharedLocked(shared,hash);
  }catch(const std::exception& e){Log("[DEMAND-LIFETIME-GAP] phase=deferred-retire reason=%s",e.what());}
   catch(...){Log("[DEMAND-LIFETIME-GAP] phase=deferred-retire reason=unknown");}
}
// Every preparation exit, including negative hits and exceptions, has the
// same retirement boundary. An explicit unlock must drain before releasing
// the lock; the frame pump covers a clear arriving during the final unlock.
struct SharedPreparation {
  std::shared_ptr<SharedScope> shared;
  std::unique_lock<std::mutex> lock;
  explicit SharedPreparation(std::shared_ptr<SharedScope> value)
    :shared(std::move(value)),lock(shared->preparation){}
  ~SharedPreparation(){if(lock.owns_lock())DrainPendingSharedRetiresLocked(shared);}
  void unlock(){DrainPendingSharedRetiresLocked(shared);lock.unlock();}
};
void PumpSharedRetirements() noexcept {
  if(!sharedRetirementsPending.exchange(false,std::memory_order_acq_rel))return;
  try{
    if(!RuntimeShapes::Main()){sharedRetirementsPending.store(true,std::memory_order_release);return;}
    std::vector<std::shared_ptr<SharedScope>> pending;
    {std::lock_guard<std::mutex> lock(mutex);for(const auto& row:sharedScopes)pending.push_back(row.second);}
    for(const auto& shared:pending){
      {std::lock_guard<std::mutex> lifetime(shared->lifetimeMutex);if(shared->pendingRetires.empty())continue;}
      // Never wait for a worker or run RetireSharedLocked under the registry
      // mutex: retirement itself uses that mutex to release author caches.
      std::unique_lock<std::mutex> preparation(shared->preparation,std::try_to_lock);
      if(preparation.owns_lock())DrainPendingSharedRetiresLocked(shared);
      else sharedRetirementsPending.store(true,std::memory_order_release);
    }
  }catch(const std::exception& e){sharedRetirementsPending.store(true,std::memory_order_release);Log("[DEMAND-LIFETIME-GAP] phase=frame-retire reason=%s",e.what());}
   catch(...){sharedRetirementsPending.store(true,std::memory_order_release);Log("[DEMAND-LIFETIME-GAP] phase=frame-retire reason=unknown");}
}
void MarkSharedProxyActive(const std::shared_ptr<SharedScope>& shared,uint64_t hash) {
  if(!shared)return;std::lock_guard<std::mutex> lock(shared->lifetimeMutex);shared->pendingRetires.erase(hash);shared->activeAuthorProxies.insert(hash);
}
void ObserveSharedRetire(void* manifest,uint64_t hash) noexcept {
  if(!sharedInputsEnabled)return;
  try{auto shared=ExistingShared(manifest,hash);if(!shared)return;std::unique_lock<std::mutex> serial(shared->preparation,std::try_to_lock);
    if(!serial.owns_lock()){{std::lock_guard<std::mutex> lock(shared->lifetimeMutex);shared->pendingRetires.insert(hash);}
      sharedRetirementsPending.store(true,std::memory_order_release);
      ++sharedProxyRetireObservations;return;}
    RetireSharedLocked(shared,hash);
  }catch(const std::exception& e){Log("[DEMAND-LIFETIME-GAP] phase=retire-observation reason=%s",e.what());}
   catch(...){Log("[DEMAND-LIFETIME-GAP] phase=retire-observation reason=unknown");}
}
void Watchdog() noexcept {
  try {
    std::vector<std::pair<uint64_t,std::shared_ptr<SharedScope>>> active;
    {std::lock_guard<std::mutex> lock(mutex);for(const auto& row:sharedScopes)active.push_back(row);}
    const auto now=GetTickCount64();
    for(const auto& row:active){const auto& scope=row.second;
      if(!scope->activePreparations.load(std::memory_order_relaxed))continue;
      const auto started=scope->preparationStarted.load(std::memory_order_relaxed);
      if(!started||now-started<1000)continue;
      auto reported=scope->watchdogReported.load(std::memory_order_relaxed);
      if(now-reported<2000||!scope->watchdogReported.compare_exchange_strong(reported,now,std::memory_order_relaxed))continue;
      const auto phase=scope->activePhase.load(std::memory_order_relaxed);
      Log("[DEMAND-SHARED-WATCHDOG] scope=%llu root=%016llX stageKey=%016llX phase=%s elapsedMs=%llu waitingPreparations=%u mainThreadMayBeInsidePreparation=1",
        row.first,scope->activeRoot.load(std::memory_order_relaxed),scope->activeKey.load(std::memory_order_relaxed),phase?phase:"unknown",
        now-started,scope->waitingPreparations.load(std::memory_order_relaxed));
    }
  }catch(const std::exception& e){Log("[DEMAND-SHARED-WATCHDOG-GAP] reason=%s",e.what());}
   catch(...){Log("[DEMAND-SHARED-WATCHDOG-GAP] reason=unknown");}
}
struct GroupSource {
  uint64_t key=0,offset=0,length=0;std::string name,physical;
  std::shared_ptr<R::File> file;std::unique_ptr<R::Reader> reader;
};
struct DependencyGroup {
  std::shared_ptr<Manifest> manifest;std::shared_ptr<SharedScope> shared;void* prototype=nullptr;uint64_t root=0,consumer=0;
  const char* phase="group-initialization";
  uint64_t openingKey=0;std::string openingName;
  std::unique_lock<std::mutex>* scopeTransaction=nullptr;
  std::vector<uint64_t> keys;
  std::map<uint64_t,std::shared_ptr<GroupSource>> sources;
  EiemBundleDependencies::Keys native,edges;
  std::set<uint64_t> incompleteNative;
  EiemBundleDependencies::Keys directNative;
  std::map<std::string,std::pair<std::shared_ptr<GroupSource>,size_t>> cabs;
  Bytes schedulingPack;
  // Private native logic can bind different shared resources while retaining
  // byte-identical source archives. Its own members need a resource identity
  // before the common installed-generation isolation is applied.
  EiemNativeGenerationIdentity::Names privateMembers;
  std::vector<EiemDemandSharedGraph::Input> inputs;
  EiemDemandSharedGraph::PreparedRootViews prepared;
  std::set<uint64_t> pendingTokens;
  std::set<std::string> usedMods,usedMaterials;
  std::map<std::string,uint64_t> materialSourceBundles;
  std::set<Identity> usedLogicalDependencies;
  size_t headerReads=0,materials=0;
  void Stage(const char* value,uint64_t key=0){phase=value;if(shared){shared->activePhase.store(value,std::memory_order_relaxed);if(key)shared->activeKey.store(key,std::memory_order_relaxed);}}
  const std::vector<uint64_t>& Native(uint64_t key){
    auto found=native.find(key);if(found!=native.end())return found->second;
    std::vector<uint64_t> dependencies;
    if(!shared->graph.Native(key,dependencies)){
      bool complete=false;dependencies=NativeDependencies(manifest->object.Get(),key,prototype,complete);
      if(!complete)incompleteNative.insert(key);
    }
    return native.emplace(key,std::move(dependencies)).first->second;
  }
  const std::vector<uint64_t>& DirectNative(uint64_t key){
    static const std::vector<uint64_t> empty;
    if(!key||!StartupDependencies::originalDirect)return empty;
    auto found=directNative.find(key);if(found!=directNative.end())return found->second;
    auto list=StartupDependencies::NewList(prototype);
    if(!StartupDependencies::originalDirect(manifest->object.Get(),key,list.Get(),StartupDependencies::api.directInfo))return empty;
    return directNative.emplace(key,StartupDependencies::ReadList(list.Get())).first->second;
  }
  std::shared_ptr<GroupSource> Open(uint64_t key){auto prior=sources.find(key);if(prior!=sources.end())return prior->second;
    EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandOpen);
    openingKey=key;openingName.clear();auto failure=shared->preparationFailures.find(key);if(failure!=shared->preparationFailures.end())std::rethrow_exception(failure->second);auto source=std::make_shared<GroupSource>();source->key=key;
    Stage("open-bundle-name",key);void* args[]={&key};source->name=String(Call(StartupDependencies::api.bundleName,manifest->object.Get(),args));openingName=source->name;
    Stage("open-vfs-singleton");auto vfs=Call(sharedApi.vfsInstance);Require(vfs,"shared native VFS singleton absent");
    auto label=Ref::Make(il2cpp_string_new(("Data/Bundles/Windows/"+source->name).c_str()),false);Bytes info(sharedApi.infoSize);void* query[]={label.Get(),info.data()};
    Stage("query-vfs-entry");Require(RuntimeShapes::Scalar<unsigned char>(sharedApi.vfsQuery,vfs,query),"requested native dependency has no VFS entry: "+source->name);
    Stage("read-vfs-range");
    memcpy(&source->offset,info.data()+sharedApi.offset,sharedApi.offsetWidth);memcpy(&source->length,info.data()+sharedApi.length,sharedApi.lengthWidth);
    Require(source->length&&source->length<=SIZE_MAX&&source->offset<=INT64_MAX,"requested native dependency VFS range differs");
    using StringGetter=void*(*)(void*,void*);using BoolGetter=unsigned char(*)(void*,void*);
    Stage("check-vfs-encryption");Require(!((BoolGetter)((MInfo*)sharedApi.vfsEncrypted)->mp)(info.data(),sharedApi.vfsEncrypted),"encrypted VFS dependency requires the game's decoded input boundary");
    Stage("resolve-vfs-physical-path");
    source->physical=String(((StringGetter)((MInfo*)sharedApi.vfsPhysical)->mp)(info.data(),sharedApi.vfsPhysical));
    Stage("open-vfs-file");source->file=std::make_shared<R::File>(std::filesystem::u8path(source->physical),source->offset);Require(source->length<=source->file->Size(),"native dependency exceeds VFS backing file");
    Stage("parse-native-directory");R::Limits limits;limits.blockBytes=8*1024*1024;limits.metadataBytes=8*1024*1024;limits.fieldBytes=4*1024*1024;limits.cacheBytes=1024*1024;limits.readBytes=32*1024*1024;
    auto file=source->file;
    try{
      source->reader=std::make_unique<R::Reader>(size_t(source->length),[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);},limits);
    }catch(const EiemNativeViewWriter::UnsupportedStorageFlags& error){
      file->VerifyStable();
      RememberPreparationFailure(*shared,key,std::current_exception());
      Log("[DEMAND-NATIVE-FORMAT-GAP] bundle=%016llX name=%s reason=%s originalInputPreserved=1 failureCachedForManifest=1",
        key,source->name.c_str(),error.what());
      throw;
    }
    if(!shared->inspectedBundleDirectories.count(key))source->file->VerifyStable();
    // Validate the entire directory before publishing a candidate's CABs.
    Stage("index-native-cabs");
    for(const auto& member:source->reader->directory.members)if(member.flags&4){
      auto local=cabs.find(member.name);
      Require(local==cabs.end()||local->second.first->key==key,"native CAB belongs to multiple requested bundles");
      auto prior=shared->cabKeys.find(member.name);
      Require(prior==shared->cabKeys.end()||prior->second==key,"native CAB changed requested bundle within one manifest generation");
    }
    Stage("register-native-cabs");
    for(size_t i=0;i<source->reader->directory.members.size();++i){const auto& member=source->reader->directory.members[i];if(!(member.flags&4))continue;
      cabs.emplace(member.name,std::make_pair(source,i));shared->cabKeys.emplace(member.name,key);}
    shared->inspectedBundleDirectories.insert(key);
    Stage("cache-vfs-input");sources.emplace(key,source);++headerReads;return source;
  }
  std::pair<std::shared_ptr<GroupSource>,size_t> DependencySource(const std::string& cab,uint64_t sourceBundle=0){Stage("resolve-native-dependency-source");
    auto local=cabs.find(cab);if(local!=cabs.end()){Native(local->second.first->key);return local->second;}
    auto candidates=EiemDemandDependencySource::CandidateKeys(keys,native);
    if(incompleteNative.count(root)||incompleteNative.count(sourceBundle)){
      const auto& direct=DirectNative(sourceBundle);
      for(auto key:direct)if(std::find(candidates.begin(),candidates.end(),key)==candidates.end())candidates.push_back(key);
    }
    auto found=EiemDemandDependencySource::Resolve(cab,candidates,cabs,shared->cabKeys,shared->inspectedBundleDirectories,
      [&]()->const auto&{return DirectNative(sourceBundle);},[&](uint64_t key){
      Stage("open-dependency-source-candidate",key);Open(key);Stage("lookup-dependency-source-cab",key);});
    if(found!=cabs.end()){Native(found->second.first->key);Stage("native-dependency-source-ready");return found->second;}
    Stage("native-dependency-source-not-found");throw std::runtime_error("referenced CAB absent from requested native dependency group: "+cab);
  }
  uint64_t DependencyKey(const std::string& cab,uint64_t sourceBundle=0){
    auto local=cabs.find(cab);
    if(local!=cabs.end()){auto key=local->second.first->key;Native(key);return key;}
    auto known=shared->cabKeys.find(cab);
    if(known!=shared->cabKeys.end()){
      const auto key=known->second;
      auto contains=[key](const auto& values){return std::find(values.begin(),values.end(),key)!=values.end();};
      if(contains(keys)||std::any_of(native.begin(),native.end(),[&](const auto& row){return contains(row.second);})){
        Native(key);return key;
      }
    }
    return DependencySource(cab,sourceBundle).first->key;
  }
  template<class Keys>std::vector<uint64_t> MeshDiscoverySeeds(const Keys& routed)const{
    std::vector<uint64_t> result{root};for(auto key:keys)if(key!=root&&routed.count(key))result.push_back(key);return result;
  }
  void RememberMeshSources(uint64_t key,EiemDemandTargetLookup::Inspection& inspection){
    if(shared->meshSourceLinks.count(key))return;
    auto source=Open(key);auto links=inspection.MeshSources(*source->reader);source->file->VerifyStable();
    shared->meshSourceLinks.emplace(key,std::move(links));
  }
  NativeMeshRoutes::Addresses FindTarget(EiemDemandTargetLookup::Inspection& inspection,uint64_t key,const Identity& identity){
    auto source=Open(key);std::set<uint64_t> carriers{key};
    auto resolve=[&](const NativeReference& reference,const Identity& requested,Asset& asset){
      auto location=DependencySource(reference.first,key);
      carriers.insert(location.first->key);
      return inspection.ReferencedMesh(*location.first->reader,reference,requested,asset);
    };
    const auto assets=inspection.Find(*source->reader,identity,resolve);
    if(!assets.empty())for(auto carrier:carriers)sources.at(carrier)->file->VerifyStable();
    NativeMeshRoutes::Addresses result;result.reserve(assets.size());
    for(const auto& asset:assets)result.push_back({cabs.at(asset.cab).first->key,asset.cab,asset.pathId});
    return result;
  }
  NativeMeshRoutes::Addresses ConfirmTarget(EiemDemandTargetLookup::Inspection& inspection,uint64_t candidate,const Identity& identity){
    auto addresses=manifest->meshRoutes->Read(identity,candidate);
    if(!addresses.empty()||!candidate||std::find(keys.begin(),keys.end(),candidate)==keys.end())return addresses;
    if(shared->meshCandidateMisses.count({candidate,identity})||shared->targetDiscoveryBudgetFailures.count(candidate)||
       shared->preparationFailures.count(candidate))return {};
    try{
      addresses=FindTarget(inspection,candidate,identity);
      if(!addresses.empty())manifest->meshRoutes->Remember(identity,candidate,addresses);
      else shared->meshCandidateMisses.emplace(candidate,identity);
      return addresses;
    }catch(const R::InspectionBudgetExceeded& error){
      shared->targetDiscoveryBudgetFailures.insert(candidate);
      Log("[DEMAND-TARGET-DISCOVERY-GAP] bundle=%016llX reason=%s originalInputPreserved=1 failureCachedForManifest=1",candidate,error.what());
    }catch(const std::exception& error){
      Log("[DEMAND-TARGET-CANDIDATE-GAP] bundle=%016llX reason=%s retryable=1",candidate,error.what());
    }
    return {};
  }
  template<class Visit>bool WalkMeshSources(std::vector<uint64_t> pending,Visit&& visit){
    bool complete=true;std::set<uint64_t> visited;
    for(size_t i=0;i<pending.size();++i){const auto key=pending[i];if(!visited.insert(key).second)continue;
      if(!visit(key))complete=false;
      auto links=shared->meshSourceLinks.find(key);
      if(links==shared->meshSourceLinks.end()){complete=false;continue;}
      for(const auto& cab:links->second)try{
        auto dependency=DependencyKey(cab,key);
        if(std::find(keys.begin(),keys.end(),dependency)==keys.end())keys.push_back(dependency);
        pending.push_back(dependency);
      }catch(const std::exception& error){complete=false;
        Log("[DEMAND-MESH-SOURCE-GAP] scope=%llu root=%016llX bundle=%016llX cab=%s reason=%s retryable=1",
          manifest->serial,root,key,cab.c_str(),error.what());}
    }return complete;
  }
  LogicalDependencyBinding ResolveLogicalDependency(const EiemRuntimeAuthorDirectory::NativeDependency& declaration,const std::string& sourcePrefab={}){
    Stage("resolve-declared-game-dependency");
    Require(!declaration.logical.path.empty()&&declaration.classId,
      "native Material game dependency has no logical identity");
    usedLogicalDependencies.insert(declaration.logical);
    auto prior=shared->logicalDependencies.find(declaration.logical);
    if(prior!=shared->logicalDependencies.end()&&!schedulingPack.empty()){
      Require(prior->second.classId==declaration.classId,"logical game dependency class differs");
      Open(prior->second.bundle);Native(prior->second.bundle);return prior->second;
    }
    Bytes info(api.packSize);const auto& identity=declaration.logical;
    std::string lookupPath=identity.path;
    auto byPath=[&](){std::fill(info.begin(),info.end(),static_cast<unsigned char>(0));auto path=FullResourceReload::Path(lookupPath);uint64_t hash=0;
      il2cpp_field_get_value(path.Get(),FullResourceReload::api.pathHashField,&hash);void* args[]={&hash,info.data()};
      return RuntimeShapes::Scalar<unsigned char>(api.lookup,manifest->object.Get(),args)!=0;};
    auto byPathAndAsset=[&](){std::fill(info.begin(),info.end(),static_cast<unsigned char>(0));
      auto path=Ref::Make(il2cpp_string_new(identity.path.c_str()),false),name=Ref::Make(il2cpp_string_new(identity.asset.c_str()),false);
      void* args[]={path.Get(),info.data(),name.Get()};return RuntimeShapes::Scalar<unsigned char>(api.subLookup,manifest->object.Get(),args)!=0;};
    bool found=EiemLogicalAssetLookup::Find(identity.path,byPath,byPathAndAsset);
    if(!found&&!sourcePrefab.empty()){lookupPath=sourcePrefab;found=byPath();}
    Require(found,"declared game dependency logical path is absent: "+identity.path);
    if(schedulingPack.empty())schedulingPack=info;
    if(prior!=shared->logicalDependencies.end()){
      Require(prior->second.classId==declaration.classId,"logical game dependency class differs");
      Open(prior->second.bundle);Native(prior->second.bundle);return prior->second;
    }
    uint64_t bundle=0,asset=0;memcpy(&bundle,info.data()+api.bundleOffset,8);memcpy(&asset,info.data()+api.assetOffset,8);
    auto source=Open(bundle);keys.push_back(bundle);Native(bundle);NativeReference resolved;size_t matches=0;
    for(const auto& member:source->reader->directory.members){if(!(member.flags&4))continue;
      auto layout=source->reader->Layout(member);std::vector<NativeReference> candidates;
      for(const auto& row:source->reader->Containers(member,layout)){
        const auto& candidate=row.asset;
        if(candidate.path!=identity.path||candidate.classId!=int(declaration.classId))continue;
        if(!identity.asset.empty()){
          const auto& schema=layout.types.at(layout.objects.at(candidate.pathId).type);
          if(std::none_of(schema.nodes.at(0).children.begin(),schema.nodes.at(0).children.end(),
             [&](size_t node){return schema.nodes.at(node).name=="m_Name";}))continue;
          const auto fields=source->reader->Fields(member,layout,candidate.pathId,{"m_Name"});
          const auto& name=fields.at("m_Name");
          if(Text(name.raw.data(),name.raw.size())!=identity.asset)continue;
        }
        candidates.emplace_back(candidate.cab,candidate.pathId);
      }
      if(candidates.empty())continue;
      if(candidates.size()>1){auto hashes=source->reader->HashAssets(member,layout);auto selected=hashes.find(asset);
        if(selected!=hashes.end()&&selected->second.classId==int(declaration.classId)){
          auto exact=std::find_if(candidates.begin(),candidates.end(),[&](const auto& candidate){return candidate.second==selected->second.pathId;});
          if(exact!=candidates.end()){resolved=*exact;++matches;continue;}
        }
      }
      resolved=candidates.back();matches+=candidates.size();
    }
    bool embedded=false;
    if(!matches){
      // The public manifest can address a prefab while the requested Material
      // is embedded in its preload interval rather than a standalone asset.
      // Never search all named objects in a Bundle or unrelated prefabs.
      std::set<NativeReference> candidates,selected;
      for(const auto& member:source->reader->directory.members)if(member.flags&4){auto layout=source->reader->Layout(member);
        auto refs=source->reader->ContainerPreloads(member,layout,lookupPath);candidates.insert(refs.begin(),refs.end());}
      std::map<std::string,std::pair<std::pair<std::shared_ptr<GroupSource>,size_t>,SerializedLayout>> layouts;
      uint64_t selectedBundle=0;
      for(const auto& reference:candidates){
        auto layout=layouts.find(reference.first);
        if(layout==layouts.end()){auto location=DependencySource(reference.first,source->key);auto& reader=*location.first->reader;
          auto parsed=reader.Layout(reader.directory.members.at(location.second));
          layout=layouts.emplace(reference.first,std::make_pair(location,std::move(parsed))).first;}
        const auto& nativeLayout=layout->second.second;auto object=nativeLayout.objects.find(reference.second);
        Require(object!=nativeLayout.objects.end(),"native container preload object absent from its declared CAB");
        const auto& type=nativeLayout.types.at(object->second.type);if(type.classId!=int(declaration.classId))continue;
        const auto& location=layout->second.first;auto& reader=*location.first->reader;
        auto fields=reader.Fields(reader.directory.members.at(location.second),nativeLayout,reference.second,{"m_Name"});
        auto name=fields.find("m_Name");if(name==fields.end()||(!identity.asset.empty()&&Text(name->second.raw.data(),name->second.raw.size())!=identity.asset))continue;
        auto canonical=CanonicalSerializedAssetReference(reference);if(selected.insert(canonical).second){resolved=reference;selectedBundle=location.first->key;}
      }
      matches=selected.size();if(matches==1){bundle=selectedBundle;embedded=true;}
    }
    Require(matches==1,"declared game dependency has no unique current native identity: "+identity.path+"#"+identity.asset+
      " class="+std::to_string(declaration.classId)+" matches="+std::to_string(matches));
    shared->logicalDependencies.emplace(identity,LogicalDependencyBinding{resolved,bundle,declaration.classId});
    Log("[DEMAND-SHARED-LOGICAL-DEPENDENCY] path=%s asset=%s bundle=%016llX class=%u nativeAddressChanged=%d containerPreload=%d consumerPfbSearch=0",
      identity.path.c_str(),identity.asset.c_str(),bundle,declaration.classId,int(resolved!=NativeReference{declaration.cab,declaration.pathId}),int(embedded));
    return shared->logicalDependencies.at(identity);
  }
  LogicalDependencyBinding TextureSource(const EiemDemandAuthorResources::Texture& texture){
    EiemRuntimeAuthorDirectory::NativeDependency logical;logical.logical=texture.source;logical.classId=28;
    auto binding=ResolveLogicalDependency(logical);
    if(texture.GameOwned()&&texture.id)Require(binding.reference==NativeReference{texture.cab,texture.id},
      "cached game Texture identity differs from the current manifest");
    return binding;
  }
  std::shared_ptr<const EiemDemandAuthorResources::Texture> TextureInput(const EiemDemandAuthorResources::Texture& texture){
    auto binding=TextureSource(texture);auto source=cabs.find(binding.reference.first);
    Require(source!=cabs.end(),"current logical Texture CAB was not opened");
    if(texture.GameOwned()){
      auto result=std::make_shared<EiemDemandAuthorResources::Texture>(texture);
      result->cab=binding.reference.first;result->id=binding.reference.second;return result;
    }
    auto& reader=*source->second.first->reader;auto layout=reader.Layout(reader.directory.members.at(source->second.second));
    auto file=source->second.first->file;auto extent=size_t(source->second.first->length);
    auto lease=[file,extent](size_t at,unsigned char* output,size_t size){Require(at<=extent&&size<=extent-at,"current Texture source range exceeds Bundle");file->ReadAt(at,output,size);};
    return EiemDemandTextureBuild::Build(texture,reader,source->second.second,layout,binding.reference.second,std::move(lease));
  }
  EiemNativeStandaloneInput::Input MaterialInput(const EiemDemandAuthorResources::Material& material){
    // Resolve the declared source Material in the current game's manifest,
    // then edit that serialized object before the game's normal creation.
    EiemRuntimeAuthorDirectory::NativeDependency logical;logical.logical=material.declaration.source;logical.classId=21;
    auto binding=ResolveLogicalDependency(logical,material.sourcePrefab);usedLogicalDependencies.insert(logical.logical);
    // Catalog descriptors are shared across consumers. A cache hit can skip
    // TextureInput, but this dependency group still owns each declared logical
    // source and must retain it through the game's proxy lifecycle.
    for(const auto& texture:material.textureAssets)if(!texture.second->source.path.empty())TextureSource(*texture.second);
    auto cacheKey=material.ownerRoot.u8string()+"\n"+logical.logical.path+"\n"+logical.logical.asset+
      "\n"+binding.reference.first+"/"+std::to_string(binding.reference.second)+"\n"+material.declaration.hash;
    for(const auto& texture:material.textures)cacheKey+="\n"+texture.first+"="+texture.second.first+"/"+std::to_string(texture.second.second);
    usedMaterials.insert(cacheKey);auto prior=shared->materialVariants.find(cacheKey);
    if(prior!=shared->materialVariants.end()){
      materialSourceBundles.emplace(prior->second.cab,binding.bundle);return prior->second;
    }
    auto source=cabs.find(binding.reference.first);Require(source!=cabs.end(),"current logical Material CAB was not opened");
    auto layout=source->second.first->reader->Layout(source->second.first->reader->directory.members.at(source->second.second));
    auto file=source->second.first->file;auto extent=size_t(source->second.first->length);
    auto lease=[file,extent](size_t at,unsigned char* output,size_t size){Require(at<=extent&&size<=extent-at,"current Material source range exceeds Bundle");file->ReadAt(at,output,size);};
    auto input=EiemDemandMaterialBuild::Build(*source->second.first->reader,source->second.second,layout,
      binding.reference.second,logical.logical,material.authored,material.textures,std::move(lease));
    input.origin=binding.reference;
    materialSourceBundles.emplace(input.cab,binding.bundle);
    shared->materialVariants.emplace(std::move(cacheKey),input);return input;
  }
  EiemNativeStandaloneInput::Input MaterialInput(const EiemDemandAuthorResources::Material& material,const M::Use&){return MaterialInput(material);}
  uint64_t Key(const std::string& name){Bytes identity(name.begin(),name.end());auto digest=Sha(identity.data(),identity.size());uint64_t key=0;memcpy(&key,digest.data(),8);
    Require(key&&std::find(keys.begin(),keys.end(),key)==keys.end(),"shared native bundle key collides with requested game bundle");return key;}
  EiemDemandSharedGraph::Input Input(const std::string& cab,const std::shared_ptr<const EiemInputSpanView::View>& view){
    EiemDemandSharedGraph::Input input;input.name=shared->bundleNamespace.ResourceName(Q::Name(cab));input.key=Key(input.name);input.cab=cab;input.view=view;input.categorySource=root;return input;
  }
  EiemDemandSharedGraph::Input VersionedRoot(uint64_t source,const Q::Record& record){
    EiemDemandSharedGraph::Input input;input.name=shared->bundleNamespace.RootName(source);
    input.key=Key(input.name);keys.push_back(input.key);input.cab=Q::Name(record.name);
    input.view=EiemDemandSharedGraph::RequirePreparedRootView(prepared,record.token);
    input.categorySource=source;input.versionedRoot=true;input.observationScope=record.scope;
    auto edgesForRoot=edges.find(source);if(edgesForRoot!=edges.end())input.dependencies=edgesForRoot->second;
    // Cache hits skip PrepareMaterials, so this group's edge map does not
    // repeat the authored dependencies already encoded in the prepared view.
    // Reuse the whole published declaration, not just its payload pointer.
    EiemDemandSharedGraph::Input prior;
    if(shared->graph.Find(input.key,prior)){
      Require(prior.versionedRoot&&prior.name==input.name&&prior.cab==input.cab&&prior.observationScope==input.observationScope&&
        (prior.sourceView?prior.sourceView:prior.view)==input.view,
        "cached native root declaration differs from its published view");
      Require(edgesForRoot==edges.end()||input.dependencies==prior.dependencies,
        "cached native root authored dependencies changed within one manifest generation");
      return prior;
    }
    return input;
  }
  void AddResourceInputs(const M::Shared& result){
    Stage("build-authored-material-inputs");
    std::map<std::string,EiemDemandSharedGraph::Input> authored;
    for(const auto& row:result.textures.inputs)authored.emplace(row.first,Input(row.first,row.second->input));
    for(const auto& row:result.inputs)authored.emplace(row.first,Input(row.first,row.second.view));
    for(const auto& row:result.inputs){Stage("build-native-material-dependency-edges");auto& output=authored.at(row.first);std::set<uint64_t> dependencies;
      auto origin=materialSourceBundles.find(row.first);
      for(const auto& ref:row.second.dependencies){auto own=authored.find(ref.first);
        dependencies.insert(own==authored.end()?DependencyKey(ref.first,origin==materialSourceBundles.end()?0:origin->second):own->second.key);}
      output.dependencies.assign(dependencies.begin(),dependencies.end());}
    Stage("publish-native-material-dependency-edges");auto& required=edges[consumer];for(const auto& row:result.inputs)required.push_back(authored.at(row.first).key);
    for(const auto& row:authored)inputs.push_back(row.second);
    materials+=result.variants;
  }
  M::Shared PrepareMaterials(const N::Author& author,const std::vector<M::Use>& uses){
    EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandMaterials);
    Stage("open-authored-material-edits");auto result=M::PrepareShared(author,uses,
      [&](const auto& material,const auto& use){return MaterialInput(material,use);},
      [&](const auto& texture){return TextureInput(texture);});
    Stage("prepare-materials-for-matched-prefab");AddResourceInputs(result);return result;
  }
  bool ReusePrepared(const Q::Record& record,Q::Candidate verdict){
    if(!record.token)return false;
    // The carrier can be revisited before this group's views are published.
    if(pendingTokens.count(record.token))return true;
    std::lock_guard<std::mutex> lock(mutex);auto found=preparedInputs.find(record.token);
    if(found!=preparedInputs.end()&&found->second->ready){
        if(found->second->blocked&&found->second->failure)std::rethrow_exception(found->second->failure);
        Require(!found->second->blocked,"requested native dependency has a failed preparation");
        usedMods.insert(found->second->usedMods.begin(),found->second->usedMods.end());usedMaterials.insert(found->second->usedMaterials.begin(),found->second->usedMaterials.end());
        usedLogicalDependencies.insert(found->second->usedLogicalDependencies.begin(),found->second->usedLogicalDependencies.end());
        if(verdict==Q::Candidate::Target||verdict==Q::Candidate::Yes)
          EiemDemandSharedGraph::ReusePreparedRootView(prepared,record.token,found->second->ready,
            found->second->blocked,found->second->view);
        return true;
    }
    return false;
  }
  Q::Record Prepare(uint64_t key,Q::Candidate verdict){
    auto previous=registry.Find(manifest->serial,key);
    if(ReusePrepared(previous,verdict)){
      auto record=CandidateRoute(manifest,key,previous.name,verdict);
      Require(record.token==previous.token,"prepared native dependency route changed during reuse");
      return record;
    }
    auto source=Open(key);Stage("route-native-input",key);
    auto record=CandidateRoute(manifest,key,source->name,verdict);
    Require(record.token,"native dependency route budget exhausted");
    {std::lock_guard<std::mutex> lock(mutex);
      preparedInputs.emplace(record.token,std::make_shared<PreparedInput>());}
    pendingTokens.insert(record.token);
    registry.BindFile(record.token,source->physical,source->offset);consumer=key;
    MaterialPrepare material=[&](const N::Author& author,const std::vector<M::Use>& uses){return PrepareMaterials(author,uses);};
    PreparedPublisher publish=[&](uint64_t token,std::shared_ptr<const EiemInputSpanView::View> view,bool blocked){Require(!blocked,"group preparation unexpectedly blocked");prepared.emplace(token,std::move(view));};
    Stage("rewrite-native-input",key);
    auto phaseChanged=[](void* context,const char* value){static_cast<DependencyGroup*>(context)->Stage(value);};
    PrepareReplacement(record.token,record.scope,*source->reader,source->file,manifest,&material,&publish,scopeTransaction,&phase,phaseChanged,this,&usedMods);
    {std::lock_guard<std::mutex> lock(mutex);auto& input=*preparedInputs.at(record.token);
      input.usedMods=usedMods;input.usedMaterials=usedMaterials;input.usedLogicalDependencies=usedLogicalDependencies;}
    source->reader->DiscardDecodedCache();return record;
  }
};
void CommitDependencyInputs(DependencyGroup& group){
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandIsolate);
  auto shared=group.shared;auto manifest=group.manifest;
    group.Stage("isolate-native-generation-identities");
    std::vector<EiemNativeGenerationIdentity::View> sourceViews,versionedRoots;
    for(const auto& input:group.inputs){
      auto source=input.sourceView?input.sourceView:input.view;
      sourceViews.push_back(source);
      if(input.versionedRoot)versionedRoots.push_back(source);
    }
    auto generationNames=shared->generationNames;
    for(const auto& row:EiemNativeGenerationIdentity::Collect(shared->serial,sourceViews)){
      if(group.privateMembers.count(row.first))continue;
      auto prior=generationNames.emplace(row);Require(prior.second||prior.first->second==row.second,"native generation member mapping changed");}
    auto isolatedNames=generationNames;
    for(const auto& row:group.privateMembers)isolatedNames.insert_or_assign(row.first,row.second);
    const auto scriptPreloads=EiemNativeGenerationIdentity::CollectScriptPreloads(versionedRoots);
    for(auto& input:group.inputs){EiemDemandSharedGraph::Input prior;
      if(shared->graph.Find(input.key,prior)){
        auto sourceView=input.sourceView?input.sourceView:input.view;auto previous=prior.sourceView?prior.sourceView:prior.view;
        Require((sourceView==previous||EiemInputSpanView::SameBytes(*sourceView,*previous))&&
          input.dependencies==prior.dependencies,"cached native generation input changed");input=std::move(prior);continue;}
      input.sourceView=input.view;
      input.view=EiemNativeGenerationIdentity::Isolate(input.sourceView,isolatedNames,input.name,scriptPreloads);
      input.generationIsolated=true;
    }
    group.Stage("commit-shared-graph");
    std::map<uint64_t,uint64_t> stagedVfsRoutes;
    for(const auto& input:group.inputs)for(auto vfsHash:{SharedVfsHash(input.name),SharedVfsPathHash(input.name)}){
      Require(!manifest->bundles.count(vfsHash),"author VFS path hash collides with a native Bundle");
      auto staged=stagedVfsRoutes.emplace(vfsHash,input.key);
      Require(staged.second||staged.first->second==input.key,"author VFS path hash collision within dependency group");}
    std::map<uint64_t,SharedVfsRoute> stagedRouteIndex;
    for(const auto& row:stagedVfsRoutes)stagedRouteIndex.emplace(row.first,SharedVfsRoute{shared,row.second});
    std::unique_lock<std::mutex> routeLock(shared->vfsRouteMutex);
    for(const auto& row:stagedVfsRoutes){auto prior=shared->vfsRoutes.find(row.first);
      Require(prior==shared->vfsRoutes.end()||prior->second==row.second,"author VFS path hash collision within manifest");}
    std::unique_lock<std::mutex> routeIndexLock(sharedVfsRouteIndexMutex);
    for(const auto& row:stagedRouteIndex){auto prior=sharedVfsRouteIndex.find(row.first);
      if(prior!=sharedVfsRouteIndex.end())Require(prior->second.owner.lock()==shared&&prior->second.key==row.second.key,
        "author VFS path hash collision across manifests");}
    if(group.incompleteNative.empty())shared->graph.Commit(group.inputs,group.edges,group.native);
    else{
      // Keep a synthetic input's inherited edges, but never reuse a failed
      // native lookup as a complete dependency list for another request.
      auto verified=group.native;
      for(auto key:group.incompleteNative)verified.erase(key);
      shared->graph.Commit(group.inputs,group.edges,verified);
    }
    shared->generationNames.swap(generationNames);
    shared->vfsRoutes.merge(stagedVfsRoutes);routeLock.unlock();
    sharedVfsRouteIndex.merge(stagedRouteIndex);routeIndexLock.unlock();
    if(!group.inputs.empty())sharedTreeRawArmed=true;
}
std::shared_ptr<Manifest> CurrentEffectManifest(){
 Require(RuntimeShapes::Main(),"effect resource resolution requires the game main thread");
 std::shared_ptr<Manifest> state;
 {std::lock_guard<std::mutex> lock(mutex);auto installed=std::atomic_load(&installedGeneration);
  for(const auto& row:manifests)if(row.second->catalog==installed&&row.second->object.Get()&&(!state||state->serial<row.second->serial))state=row.second;}
 Require(bool(state),"effect resource has no current game manifest");return state;
}
unsigned EffectResourceClass(const std::string& type){
 static const std::map<std::string,unsigned> classes{{"Material",21},{"Mesh",43},{"Shader",48},{"Texture2D",28},{"Texture",28},
   {"Cubemap",89},{"Texture3D",117},{"Texture2DArray",187},{"CubemapArray",188},{"Sprite",213},{"AnimationClip",74}};
 auto kind=classes.find(type);Require(kind!=classes.end(),"unsupported logical effect resource class: "+type);return kind->second;
}
NativeReference ResolveEffectLogicalResource(const EiemEffectNative::GameResource& resource){
 auto state=CurrentEffectManifest();
 auto shared=SharedFor(state);SharedPreparation transaction(shared);
 auto prototype=StartupDependencies::NewListForManifest(state->object.Get());
 DependencyGroup group;group.manifest=state;group.shared=shared;group.prototype=prototype.Get();
 EiemRuntimeAuthorDirectory::NativeDependency declaration;declaration.logical={resource.path,resource.asset};declaration.classId=EffectResourceClass(resource.type);
 auto resolved=group.ResolveLogicalDependency(declaration,resource.sourcePrefab);for(const auto& source:group.sources)source.second->file->VerifyStable();
 return resolved.reference;
}
// SharedGraph owns immutable input declarations; the game's BundleProxy owns
// native loads and recycling. A live effect pins its own root proxies through
// the same RetainBundleRef/ReleaseBundleRef API used by native dependencies.
struct EffectResourceLease {
 std::shared_ptr<SharedScope> shared;std::vector<uint64_t> roots;std::vector<Ref> proxies;
 bool released=false;
 void Release(){
  if(released)return;Require(RuntimeShapes::Main(),"effect resource retirement requires game main thread");
  for(auto& proxy:proxies)if(proxy.Get()){void* args[]={proxy.Get()};Call(sharedApi.proxyReleaseBundleRef,proxy.Get(),args);proxy={};}
  if(shared){SharedPreparation transaction(shared);
   for(auto root:roots){auto found=shared->effectRoots.find(root);Require(found!=shared->effectRoots.end()&&found->second,"effect shared root lease differs");
    if(--found->second==0)shared->effectRoots.erase(found);
    RetireSharedLocked(shared,root,false);}}
  proxies.clear();roots.clear();shared.reset();released=true;
 }
};
using EffectLease=std::shared_ptr<EffectResourceLease>;
EffectLease PrepareEffectResources(const EiemEffectNative::Manifest& effect,
    const std::vector<std::shared_ptr<const EiemInputSpanView::View>>& sourceViews,const std::string& contextPath={}){
 Require((effect.unifiedResources||!effect.authorResources.empty())&&sourceViews.size()==effect.bundles.size(),"effect author resource preparation is incomplete");
 auto state=CurrentEffectManifest();auto shared=SharedFor(state);SharedPreparation transaction(shared);
 auto author=std::find_if(state->catalog->authors.begin(),state->catalog->authors.end(),[&](const auto& row){return row.second->catalog->Root().u8string()==effect.authorRoot;});
 Require(author!=state->catalog->authors.end(),"effect author Catalog does not belong to the installed generation");
 auto prototype=StartupDependencies::NewListForManifest(state->object.Get());DependencyGroup group;
 group.manifest=state;group.shared=shared;group.prototype=prototype.Get();group.usedMods.insert(author->first);
 auto privateIdentity=effect.authorRoot+"\n"+effect.key;auto privateHash=Sha(reinterpret_cast<const unsigned char*>(privateIdentity.data()),privateIdentity.size());uint64_t privateScope=0;
 memcpy(&privateScope,privateHash.data(),8);if(!privateScope)privateScope=1;
 for(const auto& row:EiemNativeGenerationIdentity::Collect(privateScope,sourceViews))
  group.privateMembers.emplace(row.first,EiemNativeGenerationIdentity::Scoped(shared->serial,row.second));
 // Scheduling belongs to the real consumer. The original template Prefab is
 // author provenance and need not exist in the current game at all.
 Bytes pack(api.packSize);
 if(!contextPath.empty()){
  auto path=FullResourceReload::Path(contextPath);uint64_t pathHash=0;
  il2cpp_field_get_value(path.Get(),FullResourceReload::api.pathHashField,&pathHash);void* lookupArgs[]={&pathHash,pack.data()};
  if(RuntimeShapes::Scalar<unsigned char>(api.lookup,state->object.Get(),lookupArgs))memcpy(&group.root,pack.data()+api.bundleOffset,8);
 }
 if(group.root){group.schedulingPack=pack;group.keys.push_back(group.root);group.Native(group.root);}
 auto lease=std::make_shared<EffectResourceLease>();
 std::vector<uint64_t> bundleKeys;for(size_t i=0;i<effect.bundles.size();++i){auto name=shared->bundleNamespace.ResourceName("effect/"+effect.key+"/"+std::to_string(i));bundleKeys.push_back(group.Key(name));}
 group.consumer=bundleKeys.at(effect.root);
 std::map<std::string,NativeReference> resources;
 M::Shared materials;
 for(const auto& declaration:effect.authorResources){auto previous=resources.find(declaration.resource);if(previous!=resources.end())continue;
  NativeReference reference;
  if(declaration.type=="Material"){
   auto material=author->second->catalog->GetMaterial(declaration.resource,[&](const auto& texture){return group.TextureInput(texture);});
   auto input=group.MaterialInput(*material);reference=input.asset;
   auto inserted=materials.inputs.emplace(input.cab,input);Require(inserted.second||M::SameInput(inserted.first->second,input),"effect author Material CAB collision");
   for(const auto& texture:material->textureAssets)if(texture.second->input)materials.textures.inputs.emplace(texture.second->cab,texture.second);
  }else if(declaration.type=="Mesh"){
   auto mesh=author->second->catalog->GetMeshResource(declaration.resource);
   // ParticleSystemRenderer consumes Unity Mesh geometry directly. A Mesh
   // carrying unused skinning fields is still a valid native Mesh resource.
   reference=mesh->native;
   auto input=group.Input(reference.first,mesh->input);group.edges[group.consumer].push_back(input.key);group.inputs.push_back(std::move(input));
  }else if(declaration.type=="Texture2D"||declaration.type=="Texture"){
   auto texture=author->second->catalog->GetTexture(declaration.resource,[&](const auto& value){return group.TextureInput(value);});reference={texture->cab,texture->id};
   if(!texture->source.path.empty())group.TextureSource(*texture);
   if(texture->input){auto input=group.Input(reference.first,texture->input);group.edges[group.consumer].push_back(input.key);group.inputs.push_back(std::move(input));}
   else group.edges[group.consumer].push_back(group.DependencyKey(reference.first));
  }else throw std::runtime_error("unsupported effect author Resource type: "+declaration.type);
  resources.emplace(declaration.resource,std::move(reference));
 }
 materials.variants=materials.inputs.size();group.AddResourceInputs(materials);
 std::map<const EiemEffectNative::GameResource*,NativeReference> gameResources;
 for(const auto& resource:effect.gameResources){EiemRuntimeAuthorDirectory::NativeDependency logical;
  logical.logical={resource.path,resource.asset};logical.classId=EffectResourceClass(resource.type);
  auto binding=group.ResolveLogicalDependency(logical,resource.sourcePrefab);gameResources.emplace(&resource,binding.reference);
  group.edges[group.consumer].push_back(binding.bundle);
 }
 if(!group.root&&!group.schedulingPack.empty()){pack=group.schedulingPack;memcpy(&group.root,pack.data()+api.bundleOffset,8);}
 Require(group.root,"effect has neither a current consumer nor a logical game resource scheduling context");
 for(auto& input:group.inputs)input.categorySource=group.root;
 auto dependencies=group.edges[group.consumer];
 for(size_t i=0;i<sourceViews.size();++i){
  auto view=EiemEffectRelocation::Bind(sourceViews[i],i,effect.gameResources,[&](const auto& resource){return gameResources.at(&resource);},
    effect.authorResources,[&](const auto& resource){return resources.at(resource.resource);});
  auto input=group.Input("effect/"+effect.key+"/"+std::to_string(i),view);input.key=bundleKeys[i];
  R::Reader nativeView(view->Size(),[view](size_t at,unsigned char* out,size_t n){Require(view->ReadAt(at,out,n)==n,"effect native source truncated");});
  auto member=std::find_if(nativeView.directory.members.begin(),nativeView.directory.members.end(),[](const auto& row){return row.flags&4;});
  Require(member!=nativeView.directory.members.end(),"effect native Bundle has no SerializedFile");input.cab=member->name;
  // Each effect Bundle can directly reference shared assets. Native dependency
  // ordering is declared once and remains under the game's loader.
  input.dependencies=dependencies;for(auto edge:effect.bundles[i].dependencies)input.dependencies.push_back(bundleKeys.at(edge));
  std::sort(input.dependencies.begin(),input.dependencies.end());input.dependencies.erase(std::unique(input.dependencies.begin(),input.dependencies.end()),input.dependencies.end());
  group.inputs.push_back(std::move(input));
 }
 group.edges.clear();for(const auto& source:group.sources)source.second->file->VerifyStable();
 // Find the already observed game manager for this exact manifest. No scene
 // traversal and no duplicate Material/Texture native loading is necessary.
 Ref manager;{std::lock_guard<std::mutex> lock(StartupDependencies::mutex);for(const auto& row:StartupDependencies::snapshots)
  if(row.second.manifest.Get()==state->object.Get()&&row.second.proxy.Get()){void* value=nullptr;il2cpp_field_get_value(row.second.proxy.Get(),StartupDependencies::api.proxyManagerField,&value);if(value){manager=Ref::Make(value,false);break;}}}
 Require(manager.Get()&&sharedApi.proxyLookupInfo&&sharedApi.treeSyncInfo&&sharedApi.proxyRetainBundleRef&&sharedApi.proxyReleaseBundleRef,
  "effect shared game Bundle manager is unavailable");
 // Allocate owner bookkeeping before graph publication. A failed preparation
 // must not strand a published graph behind a partially created root lease.
 std::map<uint64_t,size_t> stagedOwners;
 std::map<uint64_t,SharedScope::RootUsage> stagedUsage;
 {std::lock_guard<std::mutex> usageLock(shared->usageMutex);for(auto root:bundleKeys){
   auto owner=shared->effectRoots.find(root);stagedOwners.emplace(root,owner==shared->effectRoots.end()?1:owner->second+1);
   auto prior=shared->rootUsage.find(root);auto& usage=stagedUsage[root];if(prior!=shared->rootUsage.end())usage=prior->second;
   usage.mods.insert(group.usedMods.begin(),group.usedMods.end());usage.materials.insert(group.usedMaterials.begin(),group.usedMaterials.end());
   usage.logicalDependencies.insert(group.usedLogicalDependencies.begin(),group.usedLogicalDependencies.end());}}
 lease->roots=bundleKeys;lease->proxies.reserve(bundleKeys.size());
 CommitDependencyInputs(group);
 {std::lock_guard<std::mutex> usageLock(shared->usageMutex);
  for(const auto& row:stagedOwners){auto prior=shared->effectRoots.find(row.first);if(prior!=shared->effectRoots.end())prior->second=row.second;}
  shared->effectRoots.merge(stagedOwners);
  for(auto& row:stagedUsage){auto prior=shared->rootUsage.find(row.first);if(prior!=shared->rootUsage.end())std::swap(prior->second,row.second);}
  shared->rootUsage.merge(stagedUsage);}
 lease->shared=shared;
 transaction.unlock();
 try{
  for(auto key:bundleKeys){bool cancel=true;void* args[]={&key,&cancel};auto proxy=Ref::Make(Call(sharedApi.proxyLookupInfo,manager.Get(),args),false);
   Require(proxy.Get(),"effect shared BundleProxy is missing");void* retainArgs[]={proxy.Get()};Call(sharedApi.proxyRetainBundleRef,proxy.Get(),retainArgs);lease->proxies.push_back(proxy);
   int32_t depth=0;void* loadArgs[]={pack.data(),proxy.Get(),&depth};Call(sharedApi.treeSyncInfo,manager.Get(),loadArgs);
   Require(RuntimeShapes::Scalar<unsigned char>(sharedApi.proxyIsSuccessToLoad,proxy.Get()),"effect shared BundleProxy could not load its dependency tree");
  }
  Log("[EFFECT-SHARED-RESOURCES] mod=%s authorResources=%zu effectBundles=%zu materials=%zu gameProxyOwner=1 duplicateNativeLoads=0",author->first.c_str(),resources.size(),bundleKeys.size(),materials.variants);
  return lease;
 }catch(...){lease->Release();throw;}
}
void* EffectResourceBundle(const EffectLease& lease,size_t index){
 Require(lease&&!lease->released&&index<lease->proxies.size(),"effect shared resource lease is incomplete");
 void* result=nullptr;il2cpp_field_get_value(lease->proxies[index].Get(),sharedApi.proxyAssetBundleField,&result);Require(RuntimeShapes::Alive(result),"effect shared native Bundle has retired");return result;
}
#include "eiem_entry_native_mesh_provenance.h"
bool PrepareDependencyGroup(const std::shared_ptr<Manifest>& manifest,uint64_t hash,void* prototype){
  // Includes lock acquisition and already-prepared lookups, which prepareMs
  // deliberately excludes. Native proxy/tree bodies are measured separately.
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandPrepare);
  if(!sharedInputsEnabled)return false;
  // Published synthetic keys (including still-live older generations) already
  // have an authoritative graph. They are not native discovery candidates:
  // their query-only VFS alias points at a source file, not their own payload.
  EiemDemandSharedGraph::Input published;
  if(SharedInput(manifest->object.Get(),hash,published))return false;
  EiemLoadPerformance::DemandSample sample(manifest->serial,hash);
  EiemLoadPerformance::Scope sharedWait(EiemLoadPerformance::DemandSharedWait);
  auto shared=SharedFor(manifest);++shared->waitingPreparations;SharedPreparation serial(shared);--shared->waitingPreparations;
  sharedWait.Pause();
  shared->activeRoot=hash;shared->activeKey=hash;shared->activePhase="read-native-dependencies";shared->preparationStarted=GetTickCount64();shared->watchdogReported=0;shared->activePreparations=1;
  struct Activity {std::shared_ptr<SharedScope> scope;~Activity(){scope->activePreparations=0;scope->activePhase=nullptr;scope->activeRoot=0;scope->activeKey=0;scope->preparationStarted=0;}} activity{shared};
  auto prior=shared->preparedGroups.find(hash);if(prior!=shared->preparedGroups.end()&&registry.Get(prior->second).token)return true;
  uint64_t alias=0;
  if(shared->bundleNamespace.Alias(hash,alias)&&shared->graph.Find(alias,published))return true;
  if(shared->targetDiscoveryMisses.count(hash)||shared->preparationFailures.count(hash))return false;
  auto begin=std::chrono::steady_clock::now();DependencyGroup group;group.manifest=manifest;group.shared=shared;group.prototype=prototype;group.root=hash;
  sample.Outcome("failed");
  EiemLoadPerformance::Scope discovery(EiemLoadPerformance::DemandDiscovery);
  std::vector<uint64_t> native;
  try{group.Stage("read-native-dependencies",hash);native=group.Native(hash);}
  catch(const std::exception& e){Log("[DEMAND-SHARED-GAP] phase=group bundle=%016llX stage=%s reason=%s",hash,group.phase,e.what());throw;}
  group.keys=native;group.keys.push_back(hash);
  EiemDemandTargetLookup::Inspection targetInspection;
  bool discoveryComplete=true;
  if(!manifest->unresolved.empty()||!manifest->unresolvedRestorations.empty()){
    auto rememberTarget=[&](size_t index,const NativeMeshAddress& address){
      const auto& target=manifest->catalog->targets.at(index);
      auto& discovered=shared->discoveredTargets[index];
      auto duplicate=std::find_if(discovered.begin(),discovered.end(),[&](const auto& row){
        return row.bundle==address.bundle&&row.cab==address.cab&&row.object==address.object;});
      if(duplicate!=discovered.end())return;
      discovered.push_back({index,address.bundle,0,address.cab,address.object});
      shared->targetDiscoveryMisses.clear();
      Log("[DEMAND-TARGET-DISCOVERED] mod=%s path=%s asset=%s bundle=%016llX requestLocal=1",target.mod.c_str(),
        target.source.path.c_str(),target.source.asset.c_str(),address.bundle);
    };
    auto rememberRestoration=[&](const Identity& identity,const NativeMeshAddress& address){
      auto& addresses=shared->discoveredRestorations[identity];
      if(std::none_of(addresses.begin(),addresses.end(),[&](const auto& known){
          return known.bundle==address.bundle&&known.cab==address.cab&&known.object==address.object;})){
        addresses.push_back(address);
        shared->targetDiscoveryMisses.clear();
        Log("[DEMAND-BASELINE-DISCOVERED] scope=%llu path=%s asset=%s bundle=%016llX requestLocal=1 authorBindings=0",
          manifest->serial,identity.path.c_str(),identity.asset.c_str(),address.bundle);
      }
    };
    std::vector<size_t> remainingTargets;
    std::vector<const Identity*> remainingRestorations;
    const bool nativeComplete=!group.incompleteNative.count(hash);
    auto includeCarriers=[&](uint64_t candidate,const NativeMeshRoutes::Addresses& addresses){
      if(std::find(group.keys.begin(),group.keys.end(),candidate)==group.keys.end())return;
      for(const auto& address:addresses)
        if(std::find(group.keys.begin(),group.keys.end(),address.bundle)==group.keys.end())group.keys.push_back(address.bundle);
    };
    for(auto index:manifest->unresolved){
      const auto& identity=manifest->catalog->targets.at(index).source;
      auto route=std::lower_bound(manifest->bindings.begin(),manifest->bindings.end(),index,
        [](const Binding& binding,size_t target){return binding.target<target;});
      const auto candidate=route!=manifest->bindings.end()&&route->target==index?route->bundle:0;
      const auto addresses=group.ConfirmTarget(targetInspection,candidate,identity);
      for(const auto& address:addresses)rememberTarget(index,address);
      includeCarriers(candidate,addresses);
      if(addresses.empty()||!nativeComplete)remainingTargets.push_back(index);
    }
    for(const auto& identity:manifest->unresolvedRestorations){
      auto route=manifest->restorationCandidates.find(identity);
      const auto candidate=route==manifest->restorationCandidates.end()?0:route->second;
      const auto addresses=group.ConfirmTarget(targetInspection,candidate,identity);
      for(const auto& address:addresses)rememberRestoration(identity,address);
      includeCarriers(candidate,addresses);
      if(addresses.empty()||!nativeComplete)remainingRestorations.push_back(&identity);
    }
    if(!remainingTargets.empty()||!remainingRestorations.empty()){
      discoveryComplete=group.WalkMeshSources(group.MeshDiscoverySeeds(manifest->bundles),[&](uint64_t key){
        InspectTargetInput(*shared,key,[&]{
          for(auto index:remainingTargets)
            for(const auto& address:group.FindTarget(targetInspection,key,manifest->catalog->targets.at(index).source))
              rememberTarget(index,address);
          for(auto identity:remainingRestorations)
            for(const auto& address:group.FindTarget(targetInspection,key,*identity))rememberRestoration(*identity,address);
          group.RememberMeshSources(key,targetInspection);
          group.sources.at(key)->reader->DiscardDecodedCache();
        });return shared->targetDiscoveryChecked.count(key)!=0;
      });
    }
  }
  // A dependency closure is only a routing hint. Inspect the serialized Mesh
  // references in the current input before preparing any author payloads.
  std::set<uint64_t> requestBundles(group.keys.begin(),group.keys.end());
  std::map<NativeReference,std::set<uint64_t>> carriers;
  std::map<uint64_t,std::vector<Binding>> bindings;
  auto rememberBinding=[&](size_t index,const NativeMeshAddress& address){
    requestBundles.insert(address.bundle);
    auto& rows=bindings[address.bundle];
    if(std::none_of(rows.begin(),rows.end(),[&](const auto& row){
        return row.target==index&&row.cab==address.cab&&row.object==address.object;}))
      rows.push_back({index,address.bundle,0,address.cab,address.object});
    carriers[CanonicalSerializedAssetReference({address.cab,address.object})].insert(address.bundle);
  };
  for(const auto& binding:manifest->bindings){
    if(!requestBundles.count(binding.bundle))continue;
    const auto& identity=manifest->catalog->targets.at(binding.target).source;
    for(const auto& address:group.ConfirmTarget(targetInspection,binding.bundle,identity))
      rememberBinding(binding.target,address);
  }
  for(const auto& row:shared->discoveredTargets)for(const auto& binding:row.second)
    if(requestBundles.count(binding.bundle))
      rememberBinding(row.first,{binding.bundle,binding.cab,binding.object});
  auto rememberRestoration=[&](const NativeMeshAddress& address){
    auto reference=CanonicalSerializedAssetReference({address.cab,address.object});
    carriers[std::move(reference)].insert(address.bundle);
  };
  for(const auto& row:manifest->restorationCandidates){
    if(!requestBundles.count(row.second))continue;
    for(const auto& address:group.ConfirmTarget(targetInspection,row.second,row.first))rememberRestoration(address);
  }
  for(const auto& row:shared->discoveredRestorations)for(const auto& address:row.second)
    if(requestBundles.count(address.bundle))rememberRestoration(address);
  std::set<NativeReference> candidates;
  for(const auto& row:carriers)candidates.insert(row.first);
  std::set<uint64_t> matchedCarriers;
  size_t matchedRenderers=0;
  if(!candidates.empty()){
    group.Stage("match-requested-prefab-renderers",hash);
    auto source=group.Open(hash);auto& reader=*source->reader;
    for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;
      auto layout=reader.Layout(member);
      for(const auto& match:ProjectedMeshConsumers(layout,member.name,candidates,
          [&](int64_t id,const std::set<std::string>& fields){return reader.Project(member,layout,id,fields);},
          SerializedAssetCabKey)){
        const auto reference=CanonicalSerializedAssetReference(match.mesh);
        const auto& owners=carriers.at(reference);
        matchedCarriers.insert(owners.begin(),owners.end());++matchedRenderers;
      }
    }
  }
  discoveryComplete=discoveryComplete&&!group.incompleteNative.count(hash);
  if(!matchedRenderers){
    if(discoveryComplete)shared->targetDiscoveryMisses.insert(hash);
    sample.Outcome("unrelated");return false;
  }
  auto routing=std::make_shared<Manifest>();routing->object=manifest->object;routing->serial=manifest->serial;
  routing->catalog=manifest->catalog;routing->meshRoutes=manifest->meshRoutes;
  for(auto key:matchedCarriers){
    routing->bundles.insert(key);
    auto found=bindings.find(key);
    if(found!=bindings.end())routing->bindings.insert(routing->bindings.end(),found->second.begin(),found->second.end());
    if(std::find(group.keys.begin(),group.keys.end(),key)==group.keys.end())group.keys.push_back(key);
  }
  group.manifest=routing;
  if(++sharedPrefabPreflightObservations<=128)
    Log("[DEMAND-PREFAB-HIT-PREFLIGHT] bundle=%016llX candidateMeshes=%zu matchedCarriers=%zu matchedRenderers=%zu authorPayloadsBeforeMatch=0",
      hash,candidates.size(),matchedCarriers.size(),matchedRenderers);
  discovery.Pause();
  EiemLoadPerformance::Scope scopeWait(EiemLoadPerformance::DemandScopeWait);
  performance.hit=true;auto scopeMutex=ScopePreparationLock(manifest->serial);std::unique_lock<std::mutex> scopeTransaction(*scopeMutex);group.scopeTransaction=&scopeTransaction;
  scopeWait.Pause();
  struct ScopeSnapshot {bool hadAuthors=false,hadParts=false;ScopeAuthors authors;std::vector<BuiltMeshPart> parts;} snapshot;
  {std::lock_guard<std::mutex> lock(mutex);auto priorAuthors=authors.find(manifest->serial);if(priorAuthors!=authors.end()){snapshot.hadAuthors=true;snapshot.authors=priorAuthors->second;}
    auto priorParts=scopeParts.find(manifest->serial);if(priorParts!=scopeParts.end()){snapshot.hadParts=true;snapshot.parts=priorParts->second;}}
  std::vector<std::pair<uint64_t,std::pair<std::string,uint64_t>>> inspections;
  bool hadPreparedGroup=false;uint64_t previousPreparedGroup=0;bool insertedPreparedGroup=false;
  bool hadRootUsage=false;SharedScope::RootUsage previousRootUsage;
  try{
    group.Stage("prepare-carrier-inputs");
    for(auto key:group.keys)if(routing->bundles.count(key))group.Prepare(key,Q::Candidate::Target);
    Q::Record root;std::map<uint64_t,Q::Record> versionedRoots;
    group.Stage("prepare-target-consumers");
    std::set<uint64_t> requestKeys{hash};requestKeys.insert(matchedCarriers.begin(),matchedCarriers.end());
    for(auto key:group.keys){if(!requestKeys.count(key))continue;auto verdict=routing->bundles.count(key)?Q::Candidate::Target:Q::Candidate::Yes;
      auto record=group.Prepare(key,verdict);
      if(verdict==Q::Candidate::Target||verdict==Q::Candidate::Yes)versionedRoots.insert_or_assign(key,record);
      if(key==hash)root=record;}
    std::vector<std::pair<uint64_t,uint64_t>> rootAliases;
    std::map<uint64_t,uint64_t> stagedRootAliases;
    std::vector<std::pair<uint64_t,uint64_t>> stagedRootKeys;
    {EiemLoadPerformance::Scope rootsPerformance(EiemLoadPerformance::DemandRoots);
    group.Stage("stage-versioned-root-inputs");
    for(const auto& row:versionedRoots){auto input=group.VersionedRoot(row.first,row.second);
      rootAliases.emplace_back(row.first,input.key);stagedRootAliases.emplace(row.first,input.key);
      stagedRootKeys.emplace_back(row.first,input.key);group.inputs.push_back(std::move(input));}
    for(const auto& row:stagedRootKeys){auto dependencies=group.Native(row.first);
      for(auto& dependency:dependencies){auto alias=stagedRootAliases.find(dependency);if(alias!=stagedRootAliases.end())dependency=alias->second;}
      group.native[row.second]=std::move(dependencies);}
    }
    // Validate all inherited native edges before publishing any consumer view.
    group.Stage("validate-shared-dependencies");
    for(const auto& input:group.inputs)for(auto key:input.dependencies){EiemDemandSharedGraph::Input existing;
      if(std::find(group.keys.begin(),group.keys.end(),key)!=group.keys.end())group.Native(key);}
    for(const auto& row:group.prepared){if(!group.pendingTokens.count(row.first))continue;
      auto record=registry.Get(row.first);auto source=group.sources.at(record.bundle);
      inspections.push_back({row.first,{Q::Name(source->physical),source->offset}});}
    if(root.token){auto prior=shared->preparedGroups.find(hash);if(prior==shared->preparedGroups.end()){
        insertedPreparedGroup=shared->preparedGroups.emplace(hash,root.token).second;
      }else{hadPreparedGroup=true;previousPreparedGroup=prior->second;prior->second=root.token;}
      std::lock_guard<std::mutex> lock(shared->usageMutex);auto oldUsage=shared->rootUsage.find(hash);
      if(oldUsage!=shared->rootUsage.end()){hadRootUsage=true;previousRootUsage=oldUsage->second;}
      SharedScope::RootUsage usage{group.usedMods,group.usedMaterials,group.usedLogicalDependencies};
      for(const auto& mod:group.usedMods){auto author=manifest->catalog->authors.find(mod);
        if(author!=manifest->catalog->authors.end())usage.animationOwners.emplace(mod,author->second->catalog->AcquireAnimationOwner());}
      shared->rootUsage.insert_or_assign(hash,std::move(usage));}
    {std::lock_guard<std::mutex> lock(mutex);for(const auto& inspection:inspections)inspected.insert(inspection);}
    CommitDependencyInputs(group);
    EiemLoadPerformance::Scope publishPerformance(EiemLoadPerformance::DemandPublish);
    for(const auto& row:rootAliases)shared->bundleNamespace.BindRoot(row.first,row.second);
    group.Stage("publish-prepared-inputs");
    for(auto& row:group.prepared)if(group.pendingTokens.count(row.first))PublishPrepared(row.first,std::move(row.second));
    Log("[DEMAND-SHARED-GROUP] scope=%llu bundle=%016llX nativeDependencies=%zu openedHeaders=%zu preparedInputs=%zu sharedInputs=%zu completeMaterials=%zu prepareMs=%.3f wholeGameScan=0 nativeObjectsCreatedByEff=0",
      manifest->serial,hash,native.size(),group.headerReads,group.prepared.size(),shared->graph.Size(),group.materials,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
    sample.Outcome("prepared");
    return true;
  }catch(...){
    auto failure=std::current_exception();bool deterministic=RememberPreparationFailure(*shared,hash,failure);
    try{throw;}catch(const std::exception& e){Log("[DEMAND-SHARED-GAP] phase=group bundle=%016llX stage=%s openKey=%016llX openName=%s reason=%s",hash,group.phase,group.openingKey,group.openingName.c_str(),e.what());}
      catch(...){Log("[DEMAND-SHARED-GAP] phase=group bundle=%016llX stage=%s reason=non-standard-exception",hash,group.phase);}
    {std::lock_guard<std::mutex> lock(mutex);if(snapshot.hadAuthors)authors[manifest->serial]=std::move(snapshot.authors);else authors.erase(manifest->serial);
      if(snapshot.hadParts)scopeParts[manifest->serial]=std::move(snapshot.parts);else scopeParts.erase(manifest->serial);
      for(const auto& inspection:inspections)inspected.erase(inspection);}
    if(insertedPreparedGroup)shared->preparedGroups.erase(hash);else if(hadPreparedGroup)shared->preparedGroups[hash]=previousPreparedGroup;
    {std::lock_guard<std::mutex> lock(shared->usageMutex);if(hadRootUsage)shared->rootUsage.insert_or_assign(hash,std::move(previousRootUsage));else shared->rootUsage.erase(hash);}
    for(auto token:group.pendingTokens)PublishPrepared(token,{},true,failure);
    if(!deterministic){std::lock_guard<std::mutex> lock(mutex);for(auto token:group.pendingTokens)preparedInputs.erase(token);}
    throw;
  }
}
void InstallSharedInputs(void** assemblies,size_t count){
  auto vfs=FindClass("Beyond.VFS","VirtualFileSystem",assemblies,count);Require(vfs,"shared native VFS class absent");
  sharedApi.vfsInstance=Exact(vfs,"get_instance",{},"Beyond.VFS.VirtualFileSystem",true);
  void *iterator=nullptr,*infoClass=nullptr;
  while(auto method=il2cpp_class_get_methods(vfs,&iterator))if(!strcmp(il2cpp_method_get_name(method),"GetFileInfo")&&il2cpp_method_get_param_count(method)==2&&
      StartupDependencies::TypeName(il2cpp_method_get_param(method,0))=="System.String"){
    Require(!sharedApi.vfsQuery,"shared native VFS query ambiguous");sharedApi.vfsQuery=method;infoClass=il2cpp_class_from_type(il2cpp_method_get_param(method,1));}
  Require(sharedApi.vfsInstance&&infoClass,"shared native VFS lookup absent");sharedApi.infoSize=StartupDependencies::ValueSize(infoClass);
  auto infoType=il2cpp_method_get_param(sharedApi.vfsQuery,1);
  Require(il2cpp_type_is_byref(infoType)&&StartupDependencies::TypeName(il2cpp_method_get_return_type(sharedApi.vfsQuery))=="System.Boolean",
    "shared native VFS query must be Boolean GetFileInfo(String, ref FileInfo)");
  auto infoTypeName=StartupDependencies::TypeName(infoType);
  auto width=[&](const char* name){auto type=StartupDependencies::TypeName(il2cpp_field_get_type(StartupDependencies::Field(infoClass,name)));
    Require(type=="System.Int64"||type=="System.UInt64"||type=="System.Int32"||type=="System.UInt32","shared native VFS field ABI differs");return type=="System.Int64"||type=="System.UInt64"?size_t(8):size_t(4);};
  sharedApi.offsetWidth=width("offset");sharedApi.lengthWidth=width("len");sharedApi.offset=StartupDependencies::ValueOffset(infoClass,"offset",sharedApi.offsetWidth);sharedApi.length=StartupDependencies::ValueOffset(infoClass,"len",sharedApi.lengthWidth);
  sharedApi.vfsPhysical=Exact(infoClass,"GetChunkFileRedirectedLoaderPath",{},"System.String",false);sharedApi.vfsEncrypted=Exact(infoClass,"get_bUseEncrypt",{},"System.Boolean",false);
  Require(sharedApi.vfsPhysical&&sharedApi.vfsEncrypted,"shared native VFS path accessors absent");
  auto manifest=FindClass("Beyond.ManifestBinary","RuntimeManifestBinary",assemblies,count);auto scalar=StartupDependencies::TypeName(il2cpp_method_get_param(StartupDependencies::api.bundleName,0));
  auto proxy=FindClass("","BundleProxy",assemblies,count);auto manager=il2cpp_class_from_type(il2cpp_field_get_type(StartupDependencies::api.proxyManagerField));
  sharedApi.proxyHashField=StartupDependencies::Field(proxy,"<hashName>k__BackingField");
  sharedApi.proxyAssetBundleField=StartupDependencies::Field(proxy,"<assetBundle>k__BackingField");
  sharedApi.proxyLoadRequestField=StartupDependencies::Field(proxy,"m_loadRequest");
  Require(il2cpp_field_set_value_object,"shared BundleProxy object-reference setter API absent");
  Require(StartupDependencies::TypeName(il2cpp_field_get_type(sharedApi.proxyLoadRequestField))=="UnityEngine.AssetBundleCreateRequest",
    "shared BundleProxy load request field ABI differs");
  sharedApi.proxyLoadingOperationField=StartupDependencies::Field(proxy,"<loadingOperationID>k__BackingField");
  sharedApi.proxyLoadingGroupField=StartupDependencies::Field(proxy,"<loadingGroupID>k__BackingField");
  Require(StartupDependencies::TypeName(il2cpp_field_get_type(sharedApi.proxyLoadingOperationField))=="System.Int32"&&
    StartupDependencies::TypeName(il2cpp_field_get_type(sharedApi.proxyLoadingGroupField))=="System.Int32",
    "shared BundleProxy operation/group field ABI differs");
  sharedApi.proxyBundleFlag=Exact(proxy,"get_bundleFlag",{},"System.Int32",false);
  sharedApi.proxyIsLoading=Exact(proxy,"get_isLoading",{},"System.Boolean",false);
  sharedApi.proxyIsSuccessToLoad=Exact(proxy,"get_isSuccessToLoad",{},"System.Boolean",false);
  sharedApi.proxyIsFailedToLoad=Exact(proxy,"get_isFailedToLoad",{},"System.Boolean",false);
  sharedApi.proxyHasErrorOrUnloaded=Exact(proxy,"get_hasErrorOrUnloaded",{},"System.Boolean",false);
  sharedApi.proxyFinishWithBundle=Exact(proxy,"_FinishWithBundle",{"UnityEngine.AssetBundle"},"System.Void",false);
  Require(sharedApi.proxyHashField&&sharedApi.proxyAssetBundleField&&sharedApi.proxyLoadRequestField&&sharedApi.proxyBundleFlag&&
    sharedApi.proxyIsLoading&&sharedApi.proxyIsSuccessToLoad&&sharedApi.proxyIsFailedToLoad&&sharedApi.proxyHasErrorOrUnloaded&&sharedApi.proxyFinishWithBundle,
    "shared BundleProxy state accessors absent");
  auto unique=[&](void* klass,const char* name,uint32_t parameters){void *result=nullptr,*it=nullptr;while(auto method=il2cpp_class_get_methods(klass,&it))
      if(!strcmp(il2cpp_method_get_name(method),name)&&il2cpp_method_get_param_count(method)==parameters){Require(!result,std::string("ambiguous shared native API: ")+name);result=method;}
    Require(result&&((MInfo*)result)->mp,std::string("shared native API absent: ")+name);return result;};
  auto category=unique(manifest,"GetBundleCategory",1);sharedApi.categoryInfo=category;
  auto categoryType=il2cpp_method_get_return_type(category);auto categoryClass=il2cpp_class_from_type(categoryType);
  auto categoryName=StartupDependencies::TypeName(categoryType);std::string categoryUnderlying;void* categoryFields=nullptr;
  while(auto field=il2cpp_class_get_fields(categoryClass,&categoryFields))if(!strcmp(il2cpp_field_get_name(field),"value__")){
    Require(!(il2cpp_field_get_flags(field)&0x10),"shared bundle category enum value__ must be an instance field");
    categoryUnderlying=StartupDependencies::TypeName(il2cpp_field_get_type(field));break;
  }
  auto categoryWidth=StartupDependencies::ValueSize(categoryClass);
  Require(categoryUnderlying=="System.Byte"&&categoryWidth==sizeof(uint8_t),"shared bundle category ABI differs: "+categoryName+"/"+categoryUnderlying+"/"+std::to_string(categoryWidth));
  Log("[DEMAND-SHARED-CATEGORY-ABI] return=%s underlying=%s bytes=%zu",categoryName.c_str(),categoryUnderlying.c_str(),categoryWidth);
  auto unsafe=FindClass("Beyond","UnSafeString",assemblies,count);
  Require(unsafe&&il2cpp_type_get_type(il2cpp_class_get_type(unsafe))==0x12,"shared bundle unsafe name must be a managed reference");
  sharedApi.unsafeFromString=Exact(unsafe,"op_Implicit",{"System.String"},"Beyond.UnSafeString",true);Require(sharedApi.unsafeFromString,"shared bundle unsafe string conversion absent");
  auto hook=[&](void* klass,const char* name,std::initializer_list<const char*> args,const char* result,bool isStatic,void* detour,void** original){
    Require(Entry::Install(klass,name,args,result,isStatic,detour,original),std::string("shared native hook absent: ")+name);};
  auto treeSync=unique(manager,"_LoadBundleProxySync",3),tree=unique(manager,"_LoadBundleProxy",6),assetTree=unique(manager,"_LoadAssetProxy",4);
  auto proxyLookup=unique(manager,"GetOrCreateBundleProxy",2);auto proxyType=StartupDependencies::TypeName(il2cpp_method_get_return_type(proxyLookup));
  sharedApi.proxyLookupInfo=proxyLookup;sharedApi.treeSyncInfo=treeSync;
  auto operationUpdate=unique(manager,"_UpdateOperationLoadBundle",1);
  auto operationParam=il2cpp_method_get_param(operationUpdate,0);
  auto operationType=StartupDependencies::TypeName(operationParam);
  Require(il2cpp_type_is_byref(operationParam),"shared load operation parameter must be by-reference");
  auto operationClass=il2cpp_class_from_type(operationParam);
  Require(operationClass,"shared load operation value type absent");
  auto operationSize=StartupDependencies::ValueSize(operationClass);
  Require(operationSize>=sizeof(void*),"shared load operation value type is smaller than BundleProxy reference");
  auto operationBundleHashNameField=StartupDependencies::Field(operationClass,"bundleHashName");
  const auto operationBundleHashNameType=StartupDependencies::TypeName(il2cpp_field_get_type(operationBundleHashNameField));
  Require(IsBundleKeyFieldType(operationBundleHashNameType),
    "shared load operation bundleHashName must be a 64-bit integer");
  sharedApi.operationBundleHashNameOffset=StartupDependencies::ValueOffset(operationClass,"bundleHashName",sizeof(uint64_t));
  Require(sharedApi.operationBundleHashNameOffset<=operationSize-sizeof(uint64_t),
    "shared load operation bundleHashName offset exceeds value type");
  sharedApi.operationBundleProxyField=StartupDependencies::Field(operationClass,"bundleProxy");
  auto operationProxyType=il2cpp_field_get_type(sharedApi.operationBundleProxyField);
  Require(!il2cpp_type_is_byref(operationProxyType)&&il2cpp_type_get_type(operationProxyType)==0x12&&
    StartupDependencies::TypeName(operationProxyType).find("BundleProxy")!=std::string::npos,
    "shared load operation BundleProxy field must be an object reference");
  sharedApi.operationBundleProxyOffset=StartupDependencies::ValueOffset(operationClass,"bundleProxy",sizeof(void*));
  Require(sharedApi.operationBundleProxyOffset<=operationSize-sizeof(void*),"shared load operation BundleProxy offset exceeds value type");
  Log("[DEMAND-SHARED-OPERATION-ABI] manager=%s parameterByRef=1 valueBytes=%zu bundleHashNameType=%s bundleHashNameOffset=%zu bundleProxyOffset=%zu fieldRead=value-memory",
    operationType.c_str(),operationSize,operationBundleHashNameType.c_str(),sharedApi.operationBundleHashNameOffset,sharedApi.operationBundleProxyOffset);
  hook(manager,"GetOrCreateBundleProxy",{scalar.c_str(),"System.Boolean"},proxyType.c_str(),false,(void*)SharedGetOrCreate,(void**)&originalSharedProxyLookup);
  hook(proxy,"IsAllDependenciesLoaded",{},"System.Boolean",false,(void*)SharedDependenciesReady,(void**)&originalSharedDependencyReady);
  auto pack=StartupDependencies::TypeName(il2cpp_method_get_param(tree,0));auto proxyName=StartupDependencies::TypeName(il2cpp_method_get_param(tree,1));
  sharedApi.managerTryGetBundleProxy=Exact(manager,"TryGetBundleProxy",{scalar.c_str()},proxyName.c_str(),false);
  sharedApi.proxyReleaseBundleRef=Exact(proxy,"ReleaseBundleRef",{proxyName.c_str()},"System.Void",false);
  sharedApi.proxyRetainBundleRef=Exact(proxy,"RetainBundleRef",{proxyName.c_str()},"System.Void",false);
  Require(sharedApi.managerTryGetBundleProxy&&sharedApi.proxyReleaseBundleRef&&sharedApi.proxyRetainBundleRef,"shared native dependency reference API absent");
  Require(StartupDependencies::TypeName(il2cpp_method_get_param(treeSync,0))==pack&&StartupDependencies::TypeName(il2cpp_method_get_param(treeSync,1))==proxyName,
    "shared native load-tree argument ABI differs");
  auto packSize=StartupDependencies::ValueSize(il2cpp_class_from_type(il2cpp_method_get_param(tree,0)));Require(packSize>8&&packSize<=4096,"shared native AssetPackInfo indirect ABI differs");
  auto asset=StartupDependencies::TypeName(il2cpp_method_get_param(assetTree,0));auto priority=StartupDependencies::TypeName(il2cpp_method_get_param(assetTree,3));
  Require(StartupDependencies::ValueSize(il2cpp_class_from_type(il2cpp_method_get_param(assetTree,3)))==4,"shared native asset priority ABI differs");
  hook(manager,"_LoadBundleProxySync",{pack.c_str(),proxyName.c_str(),"System.Int32"},"System.Void",false,(void*)TreeSync,(void**)&originalTreeSync);
  hook(manager,"_LoadBundleProxy",{pack.c_str(),proxyName.c_str(),"System.Int32","System.Int32","System.Boolean","System.Int32"},"System.Void",false,(void*)Tree,(void**)&originalTree);
  hook(manager,"_UpdateOperationLoadBundle",{operationType.c_str()},"System.Void",false,(void*)SharedOperationUpdate,(void**)&originalSharedOperationUpdate);
  hook(manager,"UnloadBundleProxyDeps",{proxyName.c_str()},"System.Void",false,(void*)SharedUnloadDependencies,(void**)&originalSharedUnloadDependencies);
  hook(manager,"_LoadAssetProxy",{asset.c_str(),"System.Int32","System.Boolean",priority.c_str()},asset.c_str(),false,(void*)AssetTree,(void**)&originalAssetTree);
  const char* priorityKeyType=nullptr;
  if(Exact(manager,"_ForcePriorityAssetBundleOperation",{asset.c_str(),"System.Int64"},"System.Void",false))priorityKeyType="System.Int64";
  else if(Exact(manager,"_ForcePriorityAssetBundleOperation",{asset.c_str(),"System.UInt64"},"System.Void",false))priorityKeyType="System.UInt64";
  if(priorityKeyType){
    hook(manager,"_ForcePriorityAssetBundleOperation",{asset.c_str(),priorityKeyType},"System.Void",false,
      (void*)SharedForcePriorityAssetBundle,(void**)&originalSharedPriorityAssetBundle);
    Log("[DEMAND-SHARED-PRIORITY-ABI] manager=%s assetProxy=%s bundleHashName=%s enabled=1",
      StartupDependencies::TypeName(il2cpp_class_get_type(manager)).c_str(),asset.c_str(),priorityKeyType);
  }else Log("[DEMAND-SHARED-PRIORITY-ABI] manager=%s assetProxy=%s enabled=0",
      StartupDependencies::TypeName(il2cpp_class_get_type(manager)).c_str(),asset.c_str());
  hook(proxy,"LoadSync",{},"System.Boolean",false,(void*)SharedProxyLoadSync,(void**)&originalSharedProxyLoadSync);
  hook(proxy,"LoadAsync",{},"System.Boolean",false,(void*)SharedProxyLoadAsync,(void**)&originalSharedProxyLoadAsync);
  hook(proxy,"UpdateLoading",{},"System.Void",false,(void*)SharedProxyUpdateLoading,(void**)&originalSharedProxyUpdateLoading);
  hook(proxy,"_LoadImmediate",{},"System.Void",false,(void*)SharedProxyLoadImmediate,(void**)&originalSharedProxyLoadImmediate);
  hook(manifest,"GetBundleFlags",{scalar.c_str()},"System.Int32",false,(void*)SharedFlags,(void**)&originalSharedFlags);
  hook(manifest,"GetBundleCategory",{scalar.c_str()},categoryName.c_str(),false,(void*)SharedCategory,(void**)&originalSharedCategory);
  hook(manifest,"GetBundleNameString",{scalar.c_str()},"System.String",false,(void*)SharedName,(void**)&originalSharedName);
  hook(manifest,"GetBundleNameUnSafeString",{scalar.c_str()},"Beyond.UnSafeString",false,(void*)SharedUnsafe,(void**)&originalSharedUnsafe);
  auto listName=StartupDependencies::TypeName(il2cpp_method_get_param(StartupDependencies::api.allInfo,1));
  sharedApi.reverseInfo=Exact(manifest,"TryGetBundleReverseDeps",{scalar.c_str(),listName.c_str()},"System.Boolean",false);
  Require(sharedApi.reverseInfo,"shared native reverse dependency method info absent");
  hook(manifest,"TryGetBundleReverseDeps",{scalar.c_str(),listName.c_str()},"System.Boolean",false,(void*)SharedReverse,(void**)&originalSharedReverse);
  hook(vfs,"GetFileInfo",{"System.String",infoTypeName.c_str()},"System.Boolean",false,(void*)SharedVfsGetFileInfo,(void**)&originalSharedVfsQuery);
  hook(proxy,"_LoadAssetBundle",{"System.String"},"UnityEngine.AssetBundle",true,(void*)SharedLoad,(void**)&originalSharedLoad);
  hook(proxy,"_LoadAssetBundleAsync",{"System.String"},"UnityEngine.AssetBundleCreateRequest",true,(void*)SharedLoadAsync,(void**)&originalSharedLoadAsync);
  sharedInputsEnabled=true;Log("[DEMAND-SHARED-READY] gameDependencyQueue=1 gameProxyLifecycle=1 vfsNativeLoadRouting=1 vfsFileInfoAlias=1 asyncRequestPreserved=1 temporaryTreeProxyKeyRoute=1 operationBundleKeyAlias=1 proxyHashRestored=1 wholeGameScan=0 proxyLoadStateProbe=1 requestScope=load-tree");
}
