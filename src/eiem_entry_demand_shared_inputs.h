#pragma once
#include "eiem_demand_dependency_scope.h"
#include "eiem_demand_material_build.h"
// Included inside DemandLoadingProbe after the current-manifest resolver.
// Only real load-tree entry points permit preparation; proxy/catalog creation
// and unrelated dependency queries cannot open author or game payloads.
thread_local unsigned sharedRequestDepth=0;
struct SharedRequestScope {
  bool active;
  explicit SharedRequestScope(EiemDemandDependencyScope::Boundary boundary):active(EiemDemandDependencyScope::OpensRequestScope(boundary)){if(active)++sharedRequestDepth;}
  ~SharedRequestScope(){if(active)--sharedRequestDepth;}
};
struct LogicalDependencyBinding {NativeReference reference;uint64_t bundle=0;unsigned classId=0;};
struct SharedScope {
  struct RootUsage {std::set<std::string> mods,materials;std::set<Identity> logicalDependencies;};
  uint64_t serial=0;
  std::shared_ptr<Manifest> manifest;
  EiemDemandSharedGraph::Graph graph;
  EiemDemandSharedGraph::DependencyExpansionCache dependencyExpansions;
  std::mutex preparation;
  std::mutex lifetimeMutex;
  std::mutex vfsRouteMutex;
  std::map<uint64_t,uint64_t> preparedGroups;
  std::map<uint64_t,RootUsage> rootUsage;
  std::set<uint64_t> activeAuthorProxies,pendingRetires;
  std::map<std::string,uint64_t> cabKeys;
  std::map<uint64_t,uint64_t> vfsRoutes;
  std::map<Identity,LogicalDependencyBinding> logicalDependencies;
  std::map<std::string,EiemNativeStandaloneInput::Input> materialVariants;
  std::atomic<unsigned> activePreparations{0},waitingPreparations{0};
  std::atomic<uint64_t> activeRoot{0},activeKey{0};
  std::atomic<const char*> activePhase{nullptr};
  std::atomic<ULONGLONG> preparationStarted{0},watchdogReported{0};
  explicit SharedScope(std::shared_ptr<Manifest> value):serial(value->serial),manifest(std::move(value)){}
};
struct SharedLoadRouteContext {
  void* manifest=nullptr;
  std::shared_ptr<SharedScope> shared;
  EiemDemandSharedGraph::Input input;
  SharedLoadRouteContext* previous=nullptr;
};
thread_local SharedLoadRouteContext* activeSharedLoadRoute=nullptr;
struct SharedLoadRouteScope {
  SharedLoadRouteContext value;
  SharedLoadRouteScope(void* manifest,std::shared_ptr<SharedScope> shared,const EiemDemandSharedGraph::Input& input)
    :value{manifest,std::move(shared),input,activeSharedLoadRoute}{activeSharedLoadRoute=&value;}
  ~SharedLoadRouteScope(){activeSharedLoadRoute=value.previous;}
};
std::map<uint64_t,std::shared_ptr<SharedScope>> sharedScopes;
void PruneSharedLocked(){
  for(auto it=sharedScopes.begin();it!=sharedScopes.end();){
    const auto& shared=it->second;auto current=manifests.find(shared->manifest->object.Get());
    const bool selected=current!=manifests.end()&&current->second==shared->manifest;
    if(selected||shared->activePreparations.load()||shared->waitingPreparations.load()){++it;continue;}
    std::unique_lock<std::mutex> preparation(shared->preparation,std::try_to_lock);
    if(!preparation.owns_lock()){++it;continue;}
    std::lock_guard<std::mutex> lifetime(shared->lifetimeMutex);
    bool nativeLease=false;for(const auto& row:requests)nativeLease|=row.second.generation==shared->manifest&&row.second.weak.Get();
    for(const auto& row:bundles)nativeLease|=row.second.generation==shared->manifest&&row.second.weak.Get();
    if(nativeLease||!shared->activeAuthorProxies.empty()){++it;continue;}
    registry.RetireScope(shared->serial);it=sharedScopes.erase(it);
  }
}
struct SharedVfsRoute {std::weak_ptr<SharedScope> owner;uint64_t key=0;};
std::mutex sharedVfsRouteIndexMutex;
std::map<uint64_t,SharedVfsRoute> sharedVfsRouteIndex;
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
  return SharedInputByVfsHash(hash,owner,input)&&input.name==name;
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
  size_t operationBundleProxyOffset=0;
  void *proxyBundleFlag=nullptr,*proxyIsLoading=nullptr,*proxyIsSuccessToLoad=nullptr;
  void *proxyIsFailedToLoad=nullptr,*proxyHasErrorOrUnloaded=nullptr;
} sharedApi;
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
SharedProxyLookupFn originalSharedProxyLookup=nullptr;
SharedDependencyReadyFn originalSharedDependencyReady=nullptr;
// IL2CPP instance-method trampolines carry a trailing MethodInfo*; forward it unchanged.
using ProxyLoadFn=uint8_t(*)(void*,void*);
using ProxyImmediateFn=void(*)(void*,void*);
ProxyLoadFn originalSharedProxyLoadSync=nullptr,originalSharedProxyLoadAsync=nullptr;
ProxyImmediateFn originalSharedProxyLoadImmediate=nullptr,originalSharedProxyUpdateLoading=nullptr;
std::atomic<unsigned> sharedProxyObservations{0},sharedDependencyWaitObservations{0};
std::mutex sharedDependencyStateMutex;
std::map<uint64_t,bool> sharedDependencyStates;
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
  if(!sharedInputsEnabled)return false;auto shared=ExistingShared(manifest,hash);return shared&&shared->graph.Find(hash,input);
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
std::vector<uint64_t> NativeDependencies(void* manifest,uint64_t hash,void* prototype){
  auto list=StartupDependencies::NewList(prototype);
  StartupDependencies::originalAll(manifest,hash,list.Get(),StartupDependencies::api.allInfo);
  auto result=StartupDependencies::ReadList(list.Get());result.erase(std::remove(result.begin(),result.end(),hash),result.end());return result;
}
std::vector<uint64_t> SharedAdded(const std::shared_ptr<SharedScope>& shared,void* manifest,uint64_t hash,const std::vector<uint64_t>& before,bool direct,void* prototype){
  auto edges=[&](uint64_t key){return shared->graph.Edges(key);};
  auto native=[&](uint64_t key){EiemDemandSharedGraph::Input input;if(shared->graph.Find(key,input))return std::vector<uint64_t>{};
    std::vector<uint64_t> cached;if(shared->graph.Native(key,cached)){++sharedNativeCacheHits;return cached;}
    ++sharedNativeCacheMisses;auto resolved=NativeDependencies(manifest,key,prototype);shared->graph.CacheNative(key,resolved);return resolved;};
  return EiemBundleDependencies::Added(hash,before,direct,edges,native);
}
uint64_t SharedVfsPathHash(const std::string& value){
  auto path=FullResourceReload::Path(value);uint64_t hash=0;
  il2cpp_field_get_value(path.Get(),FullResourceReload::api.pathHashField,&hash);
  Require(hash,"shared native VFS path hash is zero: "+value);return hash;
}
uint64_t SharedVfsHash(const std::string& name){
  return SharedVfsPathHash("Data/Bundles/Windows/"+name);
}
void* SubmitSharedInput(const EiemDemandSharedGraph::Input& input,uint32_t crc,bool asynchronous,const char* boundary){
  auto started=std::chrono::steady_clock::now();if(asynchronous)++sharedPendingLoads;
  try{auto completed=[key=input.key,cab=input.cab,started,asynchronous,boundary=std::string(boundary)](void* bundle){
      if(asynchronous)--sharedPendingLoads;++sharedLoadCompletions;if(!bundle)++sharedLoadFailures;
      Log("[DEMAND-SHARED-COMPLETE] route=%s key=%016llX cab=%s async=%d bundle=%p elapsedMs=%.3f",
        boundary.c_str(),key,cab.c_str(),int(asynchronous),bundle,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());};
    auto result=NativeBundle::LoadView(input.view,crc,asynchronous,asynchronous?std::move(completed):std::function<void(void*)>{});
    if(!result){if(asynchronous)--sharedPendingLoads;++sharedLoadFailures;}
    else{++sharedLoadSubmits;if(!asynchronous)completed(result);}
    Log("[DEMAND-SHARED-SUBMIT] route=%s key=%016llX cab=%s async=%d crc=%u result=%p inputBytes=%zu gameProxyOwnsRequest=1",
      boundary,input.key,input.cab.c_str(),int(asynchronous),crc,result,input.view->Size());return result;
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
    if(!shared&&!SharedInputByVfsHash(vfsHash,shared,input))return false;
    ++sharedVfsRouteMatches;auto sequence=sharedVfsRouteMatches.load();if(sequence<=64)
      Log("[DEMAND-SHARED-VFS-ROUTE] hash=%016llX key=%016llX name=%s async=%d crc=%u inputBytes=%zu gameBundleProxy=1",
        vfsHash,input.key,input.name.c_str(),int(asynchronous),crc,input.view->Size());
    ++sharedLoadAttempts;++sharedLoadMatches;result=SubmitSharedInput(input,crc,asynchronous,"vfs-load");return result!=nullptr;
  }catch(const std::exception& e){++gaps;Log("[DEMAND-SHARED-GAP] phase=vfs-route hash=%016llX reason=%s",vfsHash,e.what());return false;}
   catch(...){++gaps;Log("[DEMAND-SHARED-GAP] phase=vfs-route hash=%016llX reason=unknown",vfsHash);return false;}
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
bool SharedDependencies(void* manifest,uint64_t hash,void* list,bool direct,bool& result) noexcept {
  if(!EiemDemandDependencyScope::MayAugment(sharedInputsEnabled,sharedRequestDepth))return false;
  EiemDemandSharedGraph::Input input;if(!SharedInput(manifest,hash,input))return false;
  try{auto shared=ExistingShared(manifest,hash);auto before=StartupDependencies::ReadList(list);auto added=SharedAdded(shared,manifest,hash,before,direct,list);
    for(auto key:added)StartupDependencies::Append(list,key);result=true;++sharedDependencyQueries;
    bool report=false;{std::lock_guard<std::mutex> lock(sharedQueryLogMutex);report=sharedDependencyLogged.emplace(hash,direct).second;}
    if(report)Log("[DEMAND-SHARED-DEPENDENCY-QUERY] key=%016llX name=%s direct=%d declared=%zu nativeBefore=%zu added=%zu",
      hash,input.name.c_str(),int(direct),shared->graph.Edges(hash).size(),before.size(),added.size());}
  catch(const std::exception& e){result=false;++gaps;Log("[DEMAND-SHARED-GAP] phase=synthetic-dependencies reason=%s",e.what());}
  return true;
}
bool AppendSharedDependencies(void* manifest,uint64_t hash,void* list,bool direct,bool result) noexcept {
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
      ++sharedQueueRelevant;++sharedQueueExpandCalls;
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
  EiemDemandSharedGraph::Input input;bool own=SharedInput(manifest,hash,input);
  auto result=own?true:originalSharedReverse(manifest,hash,list,info);
  try{auto shared=ExistingShared(manifest,hash);if(!shared)return result;
    auto before=StartupDependencies::ReadList(list);std::set<uint64_t> present(before.begin(),before.end());
    auto added=shared->graph.Reverse(hash);for(auto key:added)if(present.insert(key).second)StartupDependencies::Append(list,key);
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
    return (asynchronous?originalSharedLoadAsync:originalSharedLoad)(path,info);}
  ++sharedLoadMatches;
  // The original BundleProxy receives an actual Unity request. Its own status,
  // cancellation, retry, reference counts and unload path remain authoritative.
  return SubmitSharedInput(input,0,asynchronous,"proxy-file");
}
void* SharedLoad(void* path,void* info){return LoadSharedInput(path,info,false);}
void* SharedLoadAsync(void* path,void* info){return LoadSharedInput(path,info,true);}
bool SharedTreeInput(void* manager,void* proxy,uint64_t& key,EiemDemandSharedGraph::Input& input)noexcept{
  try{if(!proxy)return false;il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&key);
    auto manifest=Call(StartupDependencies::api.managerManifest,manager);if(SharedInput(manifest,key,input))return true;
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
  auto result=originalSharedProxyLookup(manager,hash,cancelRecycle,info);
  if(!sharedInputsEnabled)return result;
  try{auto manifest=Call(StartupDependencies::api.managerManifest,manager);EiemDemandSharedGraph::Input input;
    if(SharedInput(manifest,hash,input)){
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
  try{uint64_t hash=0;void* manager=nullptr;il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&hash);
    il2cpp_field_get_value(proxy,StartupDependencies::api.proxyManagerField,&manager);
    auto manifest=Call(StartupDependencies::api.managerManifest,manager);
    EiemDemandSharedGraph::Input input;if(!SharedInput(manifest,hash,input))return result;
    ++sharedReadyCalls;if(result)++sharedReadyTrueCalls;else ++sharedReadyFalseCalls;
    bool changed=false;{std::lock_guard<std::mutex> lock(sharedDependencyStateMutex);auto prior=sharedDependencyStates.find(hash);
      changed=prior==sharedDependencyStates.end()||prior->second!=result;sharedDependencyStates[hash]=result;}
    auto sequence=changed?++sharedDependencyWaitObservations:sharedDependencyWaitObservations.load();
    if(changed&&sequence<=128)Log("[DEMAND-SHARED-DEPENDENCIES] key=%016llX name=%s ready=%d transition=%u",
      hash,input.name.c_str(),int(result),sequence);
  }catch(const std::exception& e){auto sequence=++sharedDependencyWaitObservations;if(sequence<=16)
    Log("[DEMAND-SHARED-DEPENDENCIES-GAP] reason=%s",e.what());}
  return result;
}
bool SharedProxyInput(void* proxy,uint64_t& hash,EiemDemandSharedGraph::Input& input,std::shared_ptr<SharedScope>* owner=nullptr,void** manifestOut=nullptr)noexcept{
  try{if(!proxy)return false;void* manager=nullptr;il2cpp_field_get_value(proxy,StartupDependencies::api.proxyManagerField,&manager);
    if(!manager)return false;il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&hash);
    auto manifest=Call(StartupDependencies::api.managerManifest,manager);auto shared=ExistingShared(manifest,hash);if(!shared||!shared->graph.Find(hash,input))return false;
    if(manifestOut)*manifestOut=manifest;
    if(owner)*owner=std::move(shared);return true;
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
  uint64_t hash=0;void* proxy=nullptr;EiemDemandSharedGraph::Input input;bool authored=false;
  try{if(operation){memcpy(&proxy,static_cast<const uint8_t*>(operation)+sharedApi.operationBundleProxyOffset,sizeof(proxy));
      if(proxy){il2cpp_field_get_value(proxy,sharedApi.proxyHashField,&hash);auto manifest=Call(StartupDependencies::api.managerManifest,manager);
        authored=SharedInput(manifest,hash,input);}}}
  catch(const std::exception& e){Log("[DEMAND-SHARED-OPERATION-GAP] phase=identify reason=%s",e.what());}
  if(authored){auto sequence=++sharedOperationUpdateCalls;if(sequence<=96){
      Log("[DEMAND-SHARED-OPERATION] phase=before sequence=%llu operation=%p proxy=%p key=%016llX name=%s",
        (unsigned long long)sequence,operation,proxy,hash,input.name.c_str());
      LogSharedProxyState("operation-update","before",hash,input,proxy);}}
  originalSharedOperationUpdate(manager,operation,info);
  if(authored&&sharedOperationUpdateCalls.load()<=96){
    LogSharedProxyState("operation-update","after",hash,input,proxy);
    Log("[DEMAND-SHARED-OPERATION] phase=after operation=%p proxy=%p key=%016llX name=%s",
      operation,proxy,hash,input.name.c_str());}
}
uint8_t SharedProxyLoadSync(void* proxy,void* info){
  uint64_t hash=0;EiemDemandSharedGraph::Input input;std::shared_ptr<SharedScope> owner;void* manifest=nullptr;
  if(!SharedProxyInput(proxy,hash,input,&owner,&manifest))return originalSharedProxyLoadSync(proxy,info);
  SharedLoadRouteScope route(manifest,owner,input);
  auto sequence=++sharedProxyLoadObservations;const bool report=sequence<=96;
  if(report)LogSharedProxyState("LoadSync","before",hash,input,proxy);
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
    if(loading&&!succeeded&&!failed&&!invalid&&!request&&!bundle){
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
  originalSharedProxyLoadImmediate(proxy,info);
  MarkSharedProxyActive(owner,hash);
  if(report)LogSharedProxyState("_LoadImmediate","after",hash,input,proxy);
}
void TreeSync(void* self,void* pack,void* main,int32_t depth,void* info){SharedRequestScope scope(EiemDemandDependencyScope::Boundary::BundleTree);ObserveTreeEntry("sync",self,main);uint64_t key=0;EiemDemandSharedGraph::Input input;
  bool shared=SharedTreeInput(self,main,key,input);auto ordinal=shared?++sharedLoadTreeMatches:0;if(shared&&ordinal<=128)
    Log("[DEMAND-SHARED-TREE-BEGIN] key=%016llX name=%s async=0 depth=%d",key,input.name.c_str(),depth);
  originalTreeSync(self,pack,main,depth,info);
  if(shared&&ordinal<=128)Log("[DEMAND-SHARED-TREE-END] key=%016llX name=%s async=0",key,input.name.c_str());}
void Tree(void* self,void* pack,void* main,int32_t group,int32_t depth,bool asynchronous,int32_t priority,void* info){SharedRequestScope scope(EiemDemandDependencyScope::Boundary::BundleTree);ObserveTreeEntry("async",self,main);uint64_t key=0;EiemDemandSharedGraph::Input input;
  bool shared=SharedTreeInput(self,main,key,input);auto ordinal=shared?++sharedLoadTreeMatches:0;if(shared&&ordinal<=128)
    Log("[DEMAND-SHARED-TREE-BEGIN] key=%016llX name=%s async=%d group=%d depth=%d",key,input.name.c_str(),int(asynchronous),group,depth);
  originalTree(self,pack,main,group,depth,asynchronous,priority,info);
  if(shared&&ordinal<=128)Log("[DEMAND-SHARED-TREE-END] key=%016llX name=%s async=%d",key,input.name.c_str(),int(asynchronous));}
void* AssetTree(void* self,void* asset,int32_t group,bool asynchronous,int32_t priority,void* info){SharedRequestScope scope(EiemDemandDependencyScope::Boundary::AssetTree);
  DemandLoadingProbe::PrefabAssetRequest(self,asset);
  ++sharedAssetTreeCalls;
  auto ordinal=++sharedAssetTreeObservations;if(ordinal<=16)Log("[DEMAND-SHARED-REQUEST] ordinal=%u group=%d async=%d nativeAssetProxy=%p",ordinal,group,int(asynchronous),asset);
  return originalAssetTree(self,asset,group,asynchronous,priority,info);}
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
void RetireSharedLocked(const std::shared_ptr<SharedScope>& shared,uint64_t hash) {
  const auto before=shared->graph.Size();EiemDemandSharedGraph::Input input;const bool authored=shared->graph.Find(hash,input);
  const bool prepared=shared->preparedGroups.erase(hash)!=0;shared->rootUsage.erase(hash);bool active=false;
  {std::lock_guard<std::mutex> lock(shared->lifetimeMutex);active=shared->activeAuthorProxies.erase(hash)!=0;shared->pendingRetires.erase(hash);}
  if(!prepared&&!active&&!authored)return;
  std::vector<uint64_t> roots;roots.reserve(shared->preparedGroups.size());
  for(const auto& row:shared->preparedGroups)roots.push_back(row.first);
  {std::lock_guard<std::mutex> lock(shared->lifetimeMutex);roots.insert(roots.end(),shared->activeAuthorProxies.begin(),shared->activeAuthorProxies.end());}
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
  for(const auto& row:shared->rootUsage){liveMods.insert(row.second.mods.begin(),row.second.mods.end());
    liveMaterials.insert(row.second.materials.begin(),row.second.materials.end());
    liveLogicalDependencies.insert(row.second.logicalDependencies.begin(),row.second.logicalDependencies.end());}
  size_t materialCacheEvictions=0,logicalCacheEvictions=0;
  for(auto it=shared->materialVariants.begin();it!=shared->materialVariants.end();)if(!liveMaterials.count(it->first)){
    it=shared->materialVariants.erase(it);++materialCacheEvictions;}else ++it;
  for(auto it=shared->logicalDependencies.begin();it!=shared->logicalDependencies.end();)if(!liveLogicalDependencies.count(it->first)){
    it=shared->logicalDependencies.erase(it);++logicalCacheEvictions;}else ++it;
  if(roots.empty())shared->cabKeys.clear();
  std::vector<std::shared_ptr<EiemDemandAuthorResources::Catalog>> catalogs;
  size_t removedMods=0;
  {
    std::lock_guard<std::mutex> lock(mutex);auto found=authors.find(shared->serial);
    if(found!=authors.end()){
      std::set<std::string> removed;
      for(auto it=found->second.begin();it!=found->second.end();)if(!liveMods.count(it->first)){
        if(it->second.author&&it->second.author->catalog)catalogs.push_back(it->second.author->catalog);
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
  for(const auto& catalog:catalogs){auto counts=catalog->ClearAssetCaches();cached.meshes+=counts.meshes;cached.textures+=counts.textures;
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
void MarkSharedProxyActive(const std::shared_ptr<SharedScope>& shared,uint64_t hash) {
  if(!shared)return;std::lock_guard<std::mutex> lock(shared->lifetimeMutex);shared->pendingRetires.erase(hash);shared->activeAuthorProxies.insert(hash);
}
void ObserveSharedRetire(void* manifest,uint64_t hash) noexcept {
  if(!sharedInputsEnabled)return;
  try{auto shared=ExistingShared(manifest,hash);if(!shared)return;std::unique_lock<std::mutex> serial(shared->preparation,std::try_to_lock);
    if(!serial.owns_lock()){{std::lock_guard<std::mutex> lock(shared->lifetimeMutex);shared->pendingRetires.insert(hash);}
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
  std::map<std::string,std::pair<std::shared_ptr<GroupSource>,size_t>> cabs;
  std::vector<EiemDemandSharedGraph::Input> inputs;
  std::map<uint64_t,std::shared_ptr<const EiemInputSpanView::View>> prepared;
  std::set<uint64_t> pendingTokens;
  std::set<std::string> usedMods,usedMaterials;
  std::set<Identity> usedLogicalDependencies;
  size_t headerReads=0,materials=0;
  void Stage(const char* value,uint64_t key=0){phase=value;if(shared){shared->activePhase.store(value,std::memory_order_relaxed);if(key)shared->activeKey.store(key,std::memory_order_relaxed);}}
  const std::vector<uint64_t>& Native(uint64_t key){auto found=native.find(key);if(found!=native.end())return found->second;
    return native.emplace(key,NativeDependencies(manifest->object.Get(),key,prototype)).first->second;}
  std::shared_ptr<GroupSource> Open(uint64_t key){auto prior=sources.find(key);if(prior!=sources.end())return prior->second;
    openingKey=key;openingName.clear();auto source=std::make_shared<GroupSource>();source->key=key;
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
    auto file=source->file;source->reader=std::make_unique<R::Reader>(size_t(source->length),[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);},limits);
    Stage("index-native-cabs");for(size_t i=0;i<source->reader->directory.members.size();++i){const auto& member=source->reader->directory.members[i];if(!(member.flags&4))continue;
      auto inserted=cabs.emplace(member.name,std::make_pair(source,i));Require(inserted.second||inserted.first->second.first->key==key,"native CAB belongs to multiple requested bundles");}
    Stage("register-native-cabs");for(const auto& member:source->reader->directory.members)if(member.flags&4){auto prior=shared->cabKeys.emplace(member.name,key);
      Require(prior.second||prior.first->second==key,"native CAB changed requested bundle within one manifest generation");}
    Stage("cache-vfs-input");sources.emplace(key,source);++headerReads;return source;
  }
  std::pair<std::shared_ptr<GroupSource>,size_t> DependencySource(const std::string& cab){Stage("resolve-native-dependency-source");
    auto found=EiemDemandDependencySource::Resolve(cab,keys,cabs,shared->cabKeys,[&](uint64_t key){
      Stage("open-dependency-source-candidate",key);Open(key);Stage("lookup-dependency-source-cab",key);});
    if(found!=cabs.end()){Stage("native-dependency-source-ready");return found->second;}
    Stage("native-dependency-source-not-found");throw std::runtime_error("referenced CAB absent from requested native dependency group: "+cab);
  }
  LogicalDependencyBinding ResolveLogicalDependency(const EiemRuntimeAuthorDirectory::NativeDependency& declaration){
    Stage("resolve-declared-game-dependency");
    Require(!declaration.logical.path.empty()&&!declaration.logical.asset.empty()&&declaration.classId,
      "native Material game dependency has no logical identity");
    usedLogicalDependencies.insert(declaration.logical);
    auto prior=shared->logicalDependencies.find(declaration.logical);
    if(prior!=shared->logicalDependencies.end()){
      Require(prior->second.classId==declaration.classId,"logical game dependency class differs");
      Open(prior->second.bundle);Native(prior->second.bundle);return prior->second;
    }
    Bytes info(api.packSize);bool found=false;const auto& identity=declaration.logical;
    if(identity.path.size()>=4&&identity.path.substr(identity.path.size()-4)==".fbx"){
      auto path=Ref::Make(il2cpp_string_new(identity.path.c_str()),false),name=Ref::Make(il2cpp_string_new(identity.asset.c_str()),false);
      void* args[]={path.Get(),info.data(),name.Get()};found=RuntimeShapes::Scalar<unsigned char>(api.subLookup,manifest->object.Get(),args)!=0;
    }else{
      auto path=FullResourceReload::Path(identity.path);uint64_t hash=0;
      il2cpp_field_get_value(path.Get(),FullResourceReload::api.pathHashField,&hash);void* args[]={&hash,info.data()};
      found=RuntimeShapes::Scalar<unsigned char>(api.lookup,manifest->object.Get(),args)!=0;
    }
    Require(found,"declared game dependency logical path is absent: "+identity.path);
    uint64_t bundle=0,asset=0;memcpy(&bundle,info.data()+api.bundleOffset,8);memcpy(&asset,info.data()+api.assetOffset,8);
    auto source=Open(bundle);Native(bundle);NativeReference resolved;size_t matches=0;
    for(const auto& member:source->reader->directory.members){if(!(member.flags&4))continue;
      auto layout=source->reader->Layout(member);auto hashes=source->reader->HashAssets(member,layout);auto selected=hashes.find(asset);
      if(selected!=hashes.end()&&selected->second.classId==int(declaration.classId)){
        auto exact=source->reader->Assets(member,layout);
        auto exactIdentity=std::find_if(exact.begin(),exact.end(),[&](const auto& candidate){return candidate.pathId==selected->second.pathId&&
          candidate.classId==int(declaration.classId)&&candidate.path==identity.path&&candidate.asset==identity.asset;});
        if(exactIdentity!=exact.end()){resolved={exactIdentity->cab,exactIdentity->pathId};++matches;continue;}
      }
      for(const auto& candidate:source->reader->Assets(member,layout))if(candidate.classId==int(declaration.classId)&&
          candidate.path==identity.path&&candidate.asset==identity.asset){resolved={candidate.cab,candidate.pathId};++matches;}
    }
    Require(matches==1,"declared game dependency has no unique current native identity: "+identity.path);
    shared->logicalDependencies.emplace(identity,LogicalDependencyBinding{resolved,bundle,declaration.classId});
    Log("[DEMAND-SHARED-LOGICAL-DEPENDENCY] path=%s bundle=%016llX class=%u nativeAddressChanged=%d consumerPfbSearch=0",
      identity.path.c_str(),bundle,declaration.classId,int(resolved!=NativeReference{declaration.cab,declaration.pathId}));
    return shared->logicalDependencies.at(identity);
  }
  EiemNativeStandaloneInput::Input MaterialInput(const EiemDemandAuthorResources::Material& material,const M::Use& use){
    // Resolve the declared source Material in the current game's manifest,
    // then edit that serialized object before the game's normal creation.
    EiemRuntimeAuthorDirectory::NativeDependency logical;logical.logical=material.declaration.source;logical.classId=21;
    auto binding=ResolveLogicalDependency(logical);usedLogicalDependencies.insert(logical.logical);
    auto cacheKey=material.ownerRoot.u8string()+"\n"+logical.logical.path+"\n"+logical.logical.asset+
      "\n"+binding.reference.first+"/"+std::to_string(binding.reference.second)+"\n"+material.declaration.hash;
    for(const auto& texture:material.textures)cacheKey+="\n"+texture.first+"="+texture.second.first+"/"+std::to_string(texture.second.second);
    usedMaterials.insert(cacheKey);auto prior=shared->materialVariants.find(cacheKey);
    if(prior!=shared->materialVariants.end())return prior->second;
    auto source=cabs.find(binding.reference.first);Require(source!=cabs.end(),"current logical Material CAB was not opened");
    auto layout=source->second.first->reader->Layout(source->second.first->reader->directory.members.at(source->second.second));
    auto file=source->second.first->file;auto extent=size_t(source->second.first->length);
    auto lease=[file,extent](size_t at,unsigned char* output,size_t size){Require(at<=extent&&size<=extent-at,"current Material source range exceeds Bundle");file->ReadAt(at,output,size);};
    auto input=EiemDemandMaterialBuild::Build(*source->second.first->reader,source->second.second,layout,
      binding.reference.second,logical.logical,material.authored,material.textures,std::move(lease));
    shared->materialVariants.emplace(std::move(cacheKey),input);return input;
  }
  uint64_t Key(const std::string& name){Bytes identity(name.begin(),name.end());auto digest=Sha(identity.data(),identity.size());uint64_t key=0;memcpy(&key,digest.data(),8);
    Require(key&&std::find(keys.begin(),keys.end(),key)==keys.end(),"shared native bundle key collides with requested game bundle");return key;}
  EiemDemandSharedGraph::Input Input(const std::string& cab,const std::shared_ptr<const EiemInputSpanView::View>& view){
    EiemDemandSharedGraph::Input input;input.name="eff/"+std::to_string(manifest->serial)+"/"+Q::Name(cab)+".ab";input.key=Key(input.name);input.cab=cab;input.view=view;input.categorySource=root;return input;
  }
  M::Shared PrepareMaterials(const N::Author& author,const std::vector<M::Use>& uses){
    Stage("open-authored-material-edits");auto result=M::PrepareShared(author,uses,[&](const auto& material,const auto& use){return MaterialInput(material,use);});
    Stage("build-authored-material-inputs");
    std::map<std::string,EiemDemandSharedGraph::Input> authored;
    for(const auto& row:result.textures.inputs)authored.emplace(row.first,Input(row.first,row.second->input));
    for(const auto& row:result.inputs)authored.emplace(row.first,Input(row.first,row.second.view));
    for(const auto& row:result.inputs){Stage("build-native-material-dependency-edges");auto& output=authored.at(row.first);std::set<uint64_t> dependencies;
      for(const auto& ref:row.second.dependencies){auto own=authored.find(ref.first);
        dependencies.insert(own==authored.end()?DependencySource(ref.first).first->key:own->second.key);}
      output.dependencies.assign(dependencies.begin(),dependencies.end());}
    Stage("publish-native-material-dependency-edges");auto& required=edges[consumer];for(const auto& row:result.inputs)required.push_back(authored.at(row.first).key);
    for(const auto& row:authored)inputs.push_back(row.second);
    materials+=result.variants;return result;
  }
  Q::Record Prepare(uint64_t key,Q::Candidate verdict){auto source=Open(key);Stage("route-native-input",key);auto record=CandidateRoute(manifest,key,source->name,verdict);Require(record.token,"native dependency route budget exhausted");
    // Carrier-first ordering can visit the same input again as a consumer.
    // Its view remains unpublished until the whole dependency graph commits.
    if(pendingTokens.count(record.token))return record;
    {std::lock_guard<std::mutex> lock(mutex);auto found=preparedInputs.find(record.token);if(found!=preparedInputs.end()&&found->second->ready){Require(!found->second->blocked,"requested native dependency has a failed preparation");return record;}
      preparedInputs.emplace(record.token,std::make_shared<PreparedInput>());}
    pendingTokens.insert(record.token);
    registry.BindFile(record.token,source->physical,source->offset);consumer=key;
    MaterialPrepare material=[&](const N::Author& author,const std::vector<M::Use>& uses){return PrepareMaterials(author,uses);};
    PreparedPublisher publish=[&](uint64_t token,std::shared_ptr<const EiemInputSpanView::View> view,bool blocked){Require(!blocked,"group preparation unexpectedly blocked");prepared.emplace(token,std::move(view));};
    Stage("rewrite-native-input",key);
    auto phaseChanged=[](void* context,const char* value){static_cast<DependencyGroup*>(context)->Stage(value);};
    PrepareReplacement(record.token,record.scope,*source->reader,source->file,manifest,&material,&publish,scopeTransaction,&phase,phaseChanged,this,&usedMods);source->reader->DiscardDecodedCache();return record;
  }
};
void PrepareDependencyGroup(const std::shared_ptr<Manifest>& manifest,uint64_t hash,void* prototype){
  if(!sharedInputsEnabled)return;auto shared=SharedFor(manifest);++shared->waitingPreparations;std::unique_lock<std::mutex> serial(shared->preparation);--shared->waitingPreparations;
  shared->activeRoot=hash;shared->activeKey=hash;shared->activePhase="read-native-dependencies";shared->preparationStarted=GetTickCount64();shared->watchdogReported=0;shared->activePreparations=1;
  struct Activity {std::shared_ptr<SharedScope> scope;~Activity(){scope->activePreparations=0;scope->activePhase=nullptr;scope->activeRoot=0;scope->activeKey=0;scope->preparationStarted=0;}} activity{shared};
  auto prior=shared->preparedGroups.find(hash);if(prior!=shared->preparedGroups.end()&&registry.Get(prior->second).token)return;
  auto begin=std::chrono::steady_clock::now();DependencyGroup group;group.manifest=manifest;group.shared=shared;group.prototype=prototype;group.root=hash;
  std::vector<uint64_t> native;
  try{group.Stage("read-native-dependencies",hash);native=group.Native(hash);}
  catch(const std::exception& e){Log("[DEMAND-SHARED-GAP] phase=group bundle=%016llX stage=%s reason=%s",hash,group.phase,e.what());throw;}
  group.keys=native;group.keys.push_back(hash);bool relevant=manifest->bundles.count(hash)!=0;
  for(auto key:native)relevant|=manifest->bundles.count(key)!=0;if(!relevant)return;
  auto scopeMutex=ScopePreparationLock(manifest->serial);std::unique_lock<std::mutex> scopeTransaction(*scopeMutex);group.scopeTransaction=&scopeTransaction;
  struct DrainRetiresOnExit {std::shared_ptr<SharedScope> scope;~DrainRetiresOnExit(){DrainPendingSharedRetiresLocked(scope);}} drainRetires{shared};
  struct ScopeSnapshot {bool hadAuthors=false,hadParts=false;ScopeAuthors authors;std::vector<BuiltMeshPart> parts;} snapshot;
  {std::lock_guard<std::mutex> lock(mutex);auto priorAuthors=authors.find(manifest->serial);if(priorAuthors!=authors.end()){snapshot.hadAuthors=true;snapshot.authors=priorAuthors->second;}
    auto priorParts=scopeParts.find(manifest->serial);if(priorParts!=scopeParts.end()){snapshot.hadParts=true;snapshot.parts=priorParts->second;}}
  std::vector<std::pair<uint64_t,std::pair<std::string,uint64_t>>> inspections;
  bool hadPreparedGroup=false;uint64_t previousPreparedGroup=0;bool insertedPreparedGroup=false;
  bool hadRootUsage=false;SharedScope::RootUsage previousRootUsage;
  try{
    group.Stage("prepare-carrier-inputs");
    for(auto key:group.keys)if(manifest->bundles.count(key))group.Prepare(key,Q::Candidate::Target);
    Q::Record root;
    group.Stage("prepare-target-consumers");
    for(auto key:group.keys){auto deps=group.Native(key);auto verdict=manifest->bundles.count(key)?Q::Candidate::Target:
        EiemMeshTargetDirectory::Candidate(key,deps,manifest->bundles)?Q::Candidate::Yes:Q::Candidate::No;
      if(verdict==Q::Candidate::No)continue;auto record=group.Prepare(key,verdict);if(key==hash)root=record;}
    // Validate all inherited native edges before publishing any consumer view.
    group.Stage("validate-shared-dependencies");
    for(const auto& input:group.inputs)for(auto key:input.dependencies){EiemDemandSharedGraph::Input existing;
      if(std::find(group.keys.begin(),group.keys.end(),key)!=group.keys.end())group.Native(key);}
    for(const auto& row:group.prepared){auto record=registry.Get(row.first);auto source=group.sources.at(record.bundle);
      inspections.push_back({row.first,{Q::Name(source->physical),source->offset}});}
    if(root.token){auto prior=shared->preparedGroups.find(hash);if(prior==shared->preparedGroups.end()){
        insertedPreparedGroup=shared->preparedGroups.emplace(hash,root.token).second;
      }else{hadPreparedGroup=true;previousPreparedGroup=prior->second;prior->second=root.token;}
      auto oldUsage=shared->rootUsage.find(hash);if(oldUsage!=shared->rootUsage.end()){hadRootUsage=true;previousRootUsage=oldUsage->second;}
      shared->rootUsage.insert_or_assign(hash,SharedScope::RootUsage{group.usedMods,group.usedMaterials,group.usedLogicalDependencies});}
    {std::lock_guard<std::mutex> lock(mutex);for(const auto& inspection:inspections)inspected.insert(inspection);}
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
    shared->graph.Commit(group.inputs,group.edges,group.native);
    shared->vfsRoutes.merge(stagedVfsRoutes);routeLock.unlock();
    sharedVfsRouteIndex.merge(stagedRouteIndex);routeIndexLock.unlock();
    if(!group.inputs.empty())sharedTreeRawArmed=true;
    group.Stage("publish-prepared-inputs");
    for(auto& row:group.prepared)PublishPrepared(row.first,std::move(row.second));
    Log("[DEMAND-SHARED-GROUP] scope=%llu bundle=%016llX nativeDependencies=%zu openedHeaders=%zu preparedInputs=%zu sharedInputs=%zu completeMaterials=%zu prepareMs=%.3f wholeGameScan=0 nativeObjectsCreatedByEff=0",
      manifest->serial,hash,native.size(),group.headerReads,group.prepared.size(),shared->graph.Size(),group.materials,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
  }catch(...){
    try{throw;}catch(const std::exception& e){Log("[DEMAND-SHARED-GAP] phase=group bundle=%016llX stage=%s openKey=%016llX openName=%s reason=%s",hash,group.phase,group.openingKey,group.openingName.c_str(),e.what());}
      catch(...){Log("[DEMAND-SHARED-GAP] phase=group bundle=%016llX stage=%s reason=non-standard-exception",hash,group.phase);}
    {std::lock_guard<std::mutex> lock(mutex);if(snapshot.hadAuthors)authors[manifest->serial]=std::move(snapshot.authors);else authors.erase(manifest->serial);
      if(snapshot.hadParts)scopeParts[manifest->serial]=std::move(snapshot.parts);else scopeParts.erase(manifest->serial);
      for(const auto& inspection:inspections)inspected.erase(inspection);}
    if(insertedPreparedGroup)shared->preparedGroups.erase(hash);else if(hadPreparedGroup)shared->preparedGroups[hash]=previousPreparedGroup;
    if(hadRootUsage)shared->rootUsage.insert_or_assign(hash,std::move(previousRootUsage));else shared->rootUsage.erase(hash);
    for(auto token:group.pendingTokens)PublishPrepared(token,{},true);throw;
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
  Require(sharedApi.proxyHashField&&sharedApi.proxyAssetBundleField&&sharedApi.proxyLoadRequestField&&sharedApi.proxyBundleFlag&&
    sharedApi.proxyIsLoading&&sharedApi.proxyIsSuccessToLoad&&sharedApi.proxyIsFailedToLoad&&sharedApi.proxyHasErrorOrUnloaded,
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
  auto operationUpdate=unique(manager,"_UpdateOperationLoadBundle",1);
  auto operationParam=il2cpp_method_get_param(operationUpdate,0);
  auto operationType=StartupDependencies::TypeName(operationParam);
  Require(il2cpp_type_is_byref(operationParam),"shared load operation parameter must be by-reference");
  auto operationClass=il2cpp_class_from_type(operationParam);
  Require(operationClass,"shared load operation value type absent");
  auto operationSize=StartupDependencies::ValueSize(operationClass);
  Require(operationSize>=sizeof(void*),"shared load operation value type is smaller than BundleProxy reference");
  sharedApi.operationBundleProxyField=StartupDependencies::Field(operationClass,"bundleProxy");
  auto operationProxyType=il2cpp_field_get_type(sharedApi.operationBundleProxyField);
  Require(!il2cpp_type_is_byref(operationProxyType)&&il2cpp_type_get_type(operationProxyType)==0x12&&
    StartupDependencies::TypeName(operationProxyType).find("BundleProxy")!=std::string::npos,
    "shared load operation BundleProxy field must be an object reference");
  sharedApi.operationBundleProxyOffset=StartupDependencies::ValueOffset(operationClass,"bundleProxy",sizeof(void*));
  Require(sharedApi.operationBundleProxyOffset<=operationSize-sizeof(void*),"shared load operation BundleProxy offset exceeds value type");
  Log("[DEMAND-SHARED-OPERATION-ABI] manager=%s parameterByRef=1 valueBytes=%zu bundleProxyOffset=%zu fieldRead=value-memory",
    operationType.c_str(),operationSize,sharedApi.operationBundleProxyOffset);
  hook(manager,"GetOrCreateBundleProxy",{scalar.c_str(),"System.Boolean"},proxyType.c_str(),false,(void*)SharedGetOrCreate,(void**)&originalSharedProxyLookup);
  hook(proxy,"IsAllDependenciesLoaded",{},"System.Boolean",false,(void*)SharedDependenciesReady,(void**)&originalSharedDependencyReady);
  auto pack=StartupDependencies::TypeName(il2cpp_method_get_param(tree,0));auto proxyName=StartupDependencies::TypeName(il2cpp_method_get_param(tree,1));
  Require(StartupDependencies::TypeName(il2cpp_method_get_param(treeSync,0))==pack&&StartupDependencies::TypeName(il2cpp_method_get_param(treeSync,1))==proxyName,
    "shared native load-tree argument ABI differs");
  auto packSize=StartupDependencies::ValueSize(il2cpp_class_from_type(il2cpp_method_get_param(tree,0)));Require(packSize>8&&packSize<=4096,"shared native AssetPackInfo indirect ABI differs");
  auto asset=StartupDependencies::TypeName(il2cpp_method_get_param(assetTree,0));auto priority=StartupDependencies::TypeName(il2cpp_method_get_param(assetTree,3));
  Require(StartupDependencies::ValueSize(il2cpp_class_from_type(il2cpp_method_get_param(assetTree,3)))==4,"shared native asset priority ABI differs");
  hook(manager,"_LoadBundleProxySync",{pack.c_str(),proxyName.c_str(),"System.Int32"},"System.Void",false,(void*)TreeSync,(void**)&originalTreeSync);
  hook(manager,"_LoadBundleProxy",{pack.c_str(),proxyName.c_str(),"System.Int32","System.Int32","System.Boolean","System.Int32"},"System.Void",false,(void*)Tree,(void**)&originalTree);
  hook(manager,"_UpdateOperationLoadBundle",{operationType.c_str()},"System.Void",false,(void*)SharedOperationUpdate,(void**)&originalSharedOperationUpdate);
  hook(manager,"_LoadAssetProxy",{asset.c_str(),"System.Int32","System.Boolean",priority.c_str()},asset.c_str(),false,(void*)AssetTree,(void**)&originalAssetTree);
  hook(proxy,"LoadSync",{},"System.Boolean",false,(void*)SharedProxyLoadSync,(void**)&originalSharedProxyLoadSync);
  hook(proxy,"LoadAsync",{},"System.Boolean",false,(void*)SharedProxyLoadAsync,(void**)&originalSharedProxyLoadAsync);
  hook(proxy,"UpdateLoading",{},"System.Void",false,(void*)SharedProxyUpdateLoading,(void**)&originalSharedProxyUpdateLoading);
  hook(proxy,"_LoadImmediate",{},"System.Void",false,(void*)SharedProxyLoadImmediate,(void**)&originalSharedProxyLoadImmediate);
  hook(manifest,"GetBundleFlags",{scalar.c_str()},"System.Int32",false,(void*)SharedFlags,(void**)&originalSharedFlags);
  hook(manifest,"GetBundleCategory",{scalar.c_str()},categoryName.c_str(),false,(void*)SharedCategory,(void**)&originalSharedCategory);
  hook(manifest,"GetBundleNameString",{scalar.c_str()},"System.String",false,(void*)SharedName,(void**)&originalSharedName);
  hook(manifest,"GetBundleNameUnSafeString",{scalar.c_str()},"Beyond.UnSafeString",false,(void*)SharedUnsafe,(void**)&originalSharedUnsafe);
  auto listName=StartupDependencies::TypeName(il2cpp_method_get_param(StartupDependencies::api.allInfo,1));
  hook(manifest,"TryGetBundleReverseDeps",{scalar.c_str(),listName.c_str()},"System.Boolean",false,(void*)SharedReverse,(void**)&originalSharedReverse);
  hook(vfs,"GetFileInfo",{"System.String",infoTypeName.c_str()},"System.Boolean",false,(void*)SharedVfsGetFileInfo,(void**)&originalSharedVfsQuery);
  hook(proxy,"_LoadAssetBundle",{"System.String"},"UnityEngine.AssetBundle",true,(void*)SharedLoad,(void**)&originalSharedLoad);
  hook(proxy,"_LoadAssetBundleAsync",{"System.String"},"UnityEngine.AssetBundleCreateRequest",true,(void*)SharedLoadAsync,(void**)&originalSharedLoadAsync);
  sharedInputsEnabled=true;Log("[DEMAND-SHARED-READY] gameDependencyQueue=1 gameProxyLifecycle=1 vfsNativeLoadRouting=1 vfsFileInfoAlias=1 asyncRequestPreserved=1 privateFieldWrites=0 wholeGameScan=0 proxyLoadStateProbe=1 requestScope=load-tree");
}
