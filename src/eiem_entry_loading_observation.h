#pragma once
// Observe the game's existing native bindings. These bindings have pointer
// returns and no injected/struct ABI. Exact managed metadata is required; icall
// addresses are resolved at startup. No load/getter/unload is initiated here.
namespace LoadingObservation {
using BundleGetterFn=void*(*)(void*);
using HashAssetFn=void*(*)(void*,int64_t,void*);
using StringAssetFn=void*(*)(void*,void*,void*);
std::atomic<bool> enabled{false};
BundleGetterFn originalBundleGetter=nullptr;
HashAssetFn originalHashAsset=nullptr,originalHashAsyncAsset=nullptr;
StringAssetFn originalStringAsset=nullptr,originalStringAsyncAsset=nullptr;
BundleGetterFn originalAssetResult=nullptr;
struct Identity {
  std::string input;
  std::string serializedHash;
  unsigned inputGeneration=0;
  uint64_t serial=0;
  RuntimeShapes::Ref weak;
};
std::mutex mutex;
std::map<void*,Identity> pending,bundles;
struct AssetRequest {
  Identity origin;
  RuntimeShapes::Ref weak;
  void* bundle=nullptr;
  void* type=nullptr;
  std::string label;
  int64_t hash=0;
  bool nullObserved=false;
};
std::map<void*,AssetRequest> assetRequests;
std::atomic<uint64_t> stringAssetCalls{0},hashAsyncAssetCalls{0},stringAsyncAssetCalls{0},assetResultCalls{0},matchedAssetRequests{0},matchedAssetCompletions{0},expiredAssetRequests{0};
template<class F>void ObserveLoad(F f) {
  try{f();}catch(const std::exception& e){Log("[RELOAD-LOAD-ERROR] reason=%s originalPreserved=1",e.what());}catch(...){Log("[RELOAD-LOAD-ERROR] reason=unknown-C++-exception originalPreserved=1");}
}
bool NativeStringObserved(){return originalStringAsset!=nullptr;}

std::atomic<uint64_t> serials{0},bundleGetterCalls{0},hashAssetCalls{0},matchedCompletions{0},matchedAssets{0},expiredRequests{0},expiredBundles{0};
std::atomic<unsigned> samples{0},unmatchedSamples{0};
constexpr size_t identityLimit=256;
bool Active(){return enabled.load(std::memory_order_acquire);}
bool Sample(){return samples.fetch_add(1,std::memory_order_relaxed)<256;}
void PruneLocked() {
  auto prune=[](auto& objects,auto& raw,auto& expired) {
    for(auto it=objects.begin();it!=objects.end();) {
      if(it->second.weak.Get()==it->first){++it;continue;}
      {std::lock_guard<std::mutex> lock(submissionMutex);raw.erase(it->first);}
      ++expired;it=objects.erase(it);
    }
  };
  prune(pending,submittedRequests,expiredRequests);
  prune(bundles,submittedBundles,expiredBundles);
  for(auto it=assetRequests.begin();it!=assetRequests.end();) {
    if(it->second.weak.Get()==it->first){++it;continue;}
    ++expiredAssetRequests;it=assetRequests.erase(it);
  }
}
void Submitted(void* object,const std::string& input,unsigned inputGeneration,bool async,const std::string& serializedHash) {
  if(!Active() || !object)return;
  Require(!serializedHash.empty(),"loading observation serialized content identity missing");
  std::lock_guard<std::mutex> lock(mutex);
  PruneLocked();
  auto& objects=async?pending:bundles;
  if(pending.size()+bundles.size()+assetRequests.size()>=identityLimit && !objects.count(object)) {
    {std::lock_guard<std::mutex> rawLock(submissionMutex);(async?submittedRequests:submittedBundles).erase(object);}
    if(Sample())Log("[RELOAD-LOAD-LIMIT] input=%s maxIdentities=%zu originalPreserved=1",input.c_str(),identityLimit);
    return;
  }
  Identity identity{input,serializedHash,inputGeneration,++serials,RuntimeShapes::Ref::Make(object,true)};
  objects.insert_or_assign(object,std::move(identity));
  if(Sample())Log("[RELOAD-LOAD-SUBMITTED] input=%s object=%p async=%d inputGeneration=%u serializedHash=%s serial=%llu weakIdentity=1 strongRetention=0",input.c_str(),object,int(async),inputGeneration,serializedHash.c_str(),(unsigned long long)serials.load());
}
void Completed(void* request,void* result,const char* boundary) {
  if(!Active())return;
  Identity identity;
  bool sameBundle=false,conflictingBundle=false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto found=pending.find(request);
    if(found==pending.end() || found->second.weak.Get()!=request) {
      if(unmatchedSamples.fetch_add(1)<16)Log("[RELOAD-LOAD-UNMATCHED] boundary=%s request=%p bundle=%p inputCorrelation=0",boundary,request,result);
      return;
    }
    identity=found->second;
    if(result) {
      auto previous=bundles.find(result);
      sameBundle=previous!=bundles.end() && previous->second.weak.Get()==result;
      conflictingBundle=sameBundle && (previous->second.serializedHash!=identity.serializedHash);
      if(!conflictingBundle) {
        auto bundleIdentity=identity;
        bundleIdentity.weak=RuntimeShapes::Ref::Make(result,true);
        bundles.insert_or_assign(result,std::move(bundleIdentity));
      }
      pending.erase(found);
    }
  }
  if(result) {
    ++matchedCompletions;
    std::lock_guard<std::mutex> lock(submissionMutex);
    submittedRequests.erase(request);
    if(!conflictingBundle)submittedBundles[result]=identity.input;
    ++completedInputs;
  }
  if(Sample())Log("[RELOAD-LOAD-COMPLETE] boundary=%s input=%s inputGeneration=%u serializedHash=%s request=%p bundle=%p serial=%llu returnedNonNull=%d weakIdentity=1 sameLiveBundle=%d conflictingContent=%d cacheIsolationVerified=0",boundary,identity.input.c_str(),identity.inputGeneration,identity.serializedHash.c_str(),request,result,(unsigned long long)identity.serial,int(result!=nullptr),int(sameBundle),int(conflictingBundle));
}
void AssetFromIdentity(const Identity& identity,void* bundle,void* result,const char* boundary,const std::string& label,int64_t hash,void* type,uint64_t begin) {
  if(result){++loadedAssets;++matchedAssets;}
  if(!Sample())return;
  auto klass=result?il2cpp_object_get_class(result):nullptr;
  auto ns=klass?il2cpp_class_get_namespace(klass):"<null>";
  auto kind=klass?il2cpp_class_get_name(klass):"<null>";
  Log("[RELOAD-LOAD-ASSET] boundary=%s input=%s inputGeneration=%u serializedHash=%s bundle=%p object=%p typeArgument=%p objectType=%s.%s name=%s hash=%016llX elapsedMs=%.6f returnedNonNull=%d weakIdentity=1 serial=%llu cacheIsolationVerified=0",boundary,identity.input.c_str(),identity.inputGeneration,identity.serializedHash.c_str(),bundle,result,type,ns?ns:"?",kind?kind:"?",label.c_str(),(unsigned long long)hash,ReloadObservation::Milliseconds(begin,ReloadObservation::Clock()),int(result!=nullptr),(unsigned long long)identity.serial);
}
void Asset(void* bundle,void* result,const char* boundary,void* name,int64_t hash,void* type,uint64_t begin) {
  if(!Active())return;
  Identity identity;
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto found=bundles.find(bundle);
    if(found==bundles.end() || found->second.weak.Get()!=bundle)return;
    identity=found->second;
  }
  AssetFromIdentity(identity,bundle,result,boundary,name?String(name):"<hash>",hash,type,begin);
}
void AssetStarted(void* bundle,void* request,const char* boundary,void* name,int64_t hash,void* type) {
  if(!Active() || !request)return;
  std::lock_guard<std::mutex> lock(mutex);
  auto found=bundles.find(bundle);
  if(found==bundles.end() || found->second.weak.Get()!=bundle)return;
  if(pending.size()+bundles.size()+assetRequests.size()>=identityLimit && !assetRequests.count(request)) {
    if(Sample())Log("[RELOAD-LOAD-LIMIT] kind=assetRequest input=%s maxIdentities=%zu originalPreserved=1",found->second.input.c_str(),identityLimit);
    return;
  }
  AssetRequest identity{found->second,RuntimeShapes::Ref::Make(request,true),bundle,type,name?String(name):"<hash>",hash,false};
  assetRequests.insert_or_assign(request,std::move(identity));++matchedAssetRequests;
  const auto& source=found->second;
  auto requestClass=il2cpp_object_get_class(request);
  auto requestNamespace=requestClass?il2cpp_class_get_namespace(requestClass):"?";
  auto requestName=requestClass?il2cpp_class_get_name(requestClass):"?";
  void* resultMethod=nullptr;
  auto resultClass=requestClass;
  for(unsigned depth=0;resultClass && !resultMethod && depth<32;++depth) {
    resultMethod=Exact(resultClass,"GetResult",{},"UnityEngine.Object",false);
    resultClass=il2cpp_class_get_parent?il2cpp_class_get_parent(resultClass):nullptr;
  }
  auto resultAddress=resultMethod?((MInfo*)resultMethod)->mp:nullptr;
  if(Sample())Log("[RELOAD-LOAD-ASSET-REQUEST] boundary=%s input=%s inputGeneration=%u serializedHash=%s bundle=%p request=%p requestType=%s.%s resultMethod=%p typeArgument=%p hash=%016llX serial=%llu weakIdentity=1 strongRetention=0",boundary,source.input.c_str(),source.inputGeneration,source.serializedHash.c_str(),bundle,request,requestNamespace?requestNamespace:"?",requestName?requestName:"?",resultAddress,type,(unsigned long long)hash,(unsigned long long)source.serial);
}
void AssetCompleted(void* request,void* result,uint64_t begin) {
  if(!Active())return;
  AssetRequest identity;
  bool logNull=false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto found=assetRequests.find(request);
    if(found==assetRequests.end() || found->second.weak.Get()!=request)return;
    identity=found->second;
    if(result)assetRequests.erase(found);
    else {logNull=!found->second.nullObserved;found->second.nullObserved=true;}
  }
  if(!result) {
    if(logNull && Sample())Log("[RELOAD-LOAD-ASSET-COMPLETE] request=%p object=%p input=%s inputGeneration=%u serializedHash=%s returnedNonNull=0 weakIdentity=1 serial=%llu",request,result,identity.origin.input.c_str(),identity.origin.inputGeneration,identity.origin.serializedHash.c_str(),(unsigned long long)identity.origin.serial);
    return;
  }
  ++matchedAssetCompletions;
  if(Sample())Log("[RELOAD-LOAD-ASSET-COMPLETE] request=%p object=%p input=%s inputGeneration=%u serializedHash=%s returnedNonNull=1 weakIdentity=1 bundleStillManagedAlive=%d serial=%llu",request,result,identity.origin.input.c_str(),identity.origin.inputGeneration,identity.origin.serializedHash.c_str(),int(identity.origin.weak.Get()==identity.bundle),(unsigned long long)identity.origin.serial);
  AssetFromIdentity(identity.origin,identity.bundle,result,"native.AssetBundleRequest.GetResult",identity.label,identity.hash,identity.type,begin);
}
void* BundleGetter(void* request) {
  ++bundleGetterCalls;
  auto result=originalBundleGetter(request);
  ObserveLoad([&]{Completed(request,result,"native.get_assetBundle");});
  return result;
}
void* HashAsset(void* bundle,int64_t hash,void* type) {
  ++hashAssetCalls;
  auto begin=ReloadObservation::Clock();
  auto result=originalHashAsset(bundle,hash,type);
  ObserveLoad([&]{Asset(bundle,result,"native.LoadAssetByHash_Internal",nullptr,hash,type,begin);});
  return result;
}
void* StringAsset(void* bundle,void* name,void* type) {
  ++stringAssetCalls;auto begin=ReloadObservation::Clock();
  auto result=originalStringAsset(bundle,name,type);
  ObserveLoad([&]{Asset(bundle,result,"native.LoadAsset_Internal",name,0,type,begin);});
  return result;
}
void* HashAsyncAsset(void* bundle,int64_t hash,void* type) {
  ++hashAsyncAssetCalls;
  auto result=originalHashAsyncAsset(bundle,hash,type);
  ObserveLoad([&]{AssetStarted(bundle,result,"native.LoadAssetAsyncByHash_Internal",nullptr,hash,type);});
  return result;
}
void* StringAsyncAsset(void* bundle,void* name,void* type) {
  ++stringAsyncAssetCalls;
  auto result=originalStringAsyncAsset(bundle,name,type);
  ObserveLoad([&]{AssetStarted(bundle,result,"native.LoadAssetAsync_Internal",name,0,type);});
  return result;
}
void* AssetResult(void* request) {
  ++assetResultCalls;auto begin=ReloadObservation::Clock();
  auto result=originalAssetResult(request);
  ObserveLoad([&]{AssetCompleted(request,result,begin);});
  return result;
}
void Unloaded(void* bundle) {
  if(!Active())return;
  std::lock_guard<std::mutex> lock(mutex);
  auto found=bundles.find(bundle);
  if(found==bundles.end() || found->second.weak.Get()!=bundle)return;
  if(Sample())Log("[RELOAD-LOAD-RELEASE] bundle=%p input=%s inputGeneration=%u serial=%llu boundary=game.Unload originalReturned=1",bundle,found->second.input.c_str(),found->second.inputGeneration,(unsigned long long)found->second.serial);
  {std::lock_guard<std::mutex> rawLock(submissionMutex);submittedBundles.erase(bundle);}
  bundles.erase(found);
}
void Frame(int frame) {
  if(!Active() || frame<0 || frame%120)return;
  ObserveLoad([&]{std::lock_guard<std::mutex> lock(mutex);PruneLocked();});
}
void Counters() {
  if(!Active())return;
  std::lock_guard<std::mutex> lock(mutex);
  Log("[RELOAD-LOAD-COUNTERS] nativeBundleGetterCalls=%llu nativeHashAssetCalls=%llu matchedCompletions=%llu matchedAssets=%llu pending=%zu liveBundles=%zu expiredRequests=%llu expiredBundles=%llu resourceCallsInitiated=0 publicationGenerationDoesNotProveResourceChange=1",(unsigned long long)bundleGetterCalls.load(),(unsigned long long)hashAssetCalls.load(),(unsigned long long)matchedCompletions.load(),(unsigned long long)matchedAssets.load(),pending.size(),bundles.size(),(unsigned long long)expiredRequests.load(),(unsigned long long)expiredBundles.load());  Log("[RELOAD-LOAD-ASYNC-COUNTERS] nativeStringAssetCalls=%llu nativeHashAsyncAssetCalls=%llu nativeStringAsyncAssetCalls=%llu nativeAssetResultCalls=%llu matchedAssetRequests=%llu matchedAssetCompletions=%llu pendingAssetRequests=%zu expiredAssetRequests=%llu resourceCallsInitiated=0",(unsigned long long)stringAssetCalls.load(),(unsigned long long)hashAsyncAssetCalls.load(),(unsigned long long)stringAsyncAssetCalls.load(),(unsigned long long)assetResultCalls.load(),(unsigned long long)matchedAssetRequests.load(),(unsigned long long)matchedAssetCompletions.load(),assetRequests.size(),(unsigned long long)expiredAssetRequests.load());
}
bool Hook(const char* name,void* detour,void** original) {
  auto address=il2cpp_resolve_icall(name);
  if(!address || installed.count(address)){Log("[RELOAD-LOAD-HOOK] unavailable=%s address=%p",name,address);return false;}
  auto status=MH_CreateHook(address,detour,original);
  if(status==MH_OK) {
    status=MH_EnableHook(address);
    if(status!=MH_OK)MH_RemoveHook(address);
  }
  if(status!=MH_OK){*original=nullptr;Log("[RELOAD-LOAD-HOOK] failed=%s status=%d",name,int(status));return false;}
  installed.insert(address);
  Log("[RELOAD-LOAD-HOOK] installed=%s address=%p runtimeResolved=1 injectedAbi=0",name,address);
  return true;
}
void InstallOptional(void** assemblies,size_t count) {
  if(!ReloadObservation::enabled.load() || Active())return;
  Observe([&]{
    auto request=FindClass("UnityEngine","AssetBundleCreateRequest",assemblies,count);
    auto bundle=FindClass("UnityEngine","AssetBundle",assemblies,count);
    Require(Exact(request,"get_assetBundle",{},"UnityEngine.AssetBundle",false) && Exact(bundle,"LoadAssetByHash_Internal",{"System.Int64","System.Type"},"UnityEngine.Object",false),"native loading observation managed ABI unavailable");
    Require(il2cpp_resolve_icall && il2cpp_object_get_class && il2cpp_gchandle_get_target && il2cpp_gchandle_free,"native loading observation APIs unavailable");
    if(!RuntimeShapes::Ref::weakHandle)RuntimeShapes::Ref::weakHandle=(decltype(RuntimeShapes::Ref::weakHandle))GetProcAddress(hGA,"il2cpp_gchandle_new_weakref");
    Require(RuntimeShapes::Ref::weakHandle,"native loading observation weak identity unavailable");
    bool completion=Hook("UnityEngine.AssetBundleCreateRequest::get_assetBundle()",(void*)BundleGetter,(void**)&originalBundleGetter);
    bool hash=Hook("UnityEngine.AssetBundle::LoadAssetByHash_Internal(System.Int64,System.Type)",(void*)HashAsset,(void**)&originalHashAsset);
    bool string=false,asyncHash=false,asyncString=false,assetResult=false;
    if(Exact(bundle,"LoadAsset_Internal",{"System.String","System.Type"},"UnityEngine.Object",false))
      string=Hook("UnityEngine.AssetBundle::LoadAsset_Internal(System.String,System.Type)",(void*)StringAsset,(void**)&originalStringAsset);
    auto assetRequest=FindClass("UnityEngine","AssetBundleRequest",assemblies,count);
    if(Exact(assetRequest,"GetResult",{},"UnityEngine.Object",false))
      assetResult=Hook("UnityEngine.AssetBundleRequest::GetResult()",(void*)AssetResult,(void**)&originalAssetResult);
    if(assetResult && Exact(bundle,"LoadAssetAsyncByHash_Internal",{"System.Int64","System.Type"},"UnityEngine.AssetBundleRequest",false))
      asyncHash=Hook("UnityEngine.AssetBundle::LoadAssetAsyncByHash_Internal(System.Int64,System.Type)",(void*)HashAsyncAsset,(void**)&originalHashAsyncAsset);
    if(assetResult && Exact(bundle,"LoadAssetAsync_Internal",{"System.String","System.Type"},"UnityEngine.AssetBundleRequest",false))
      asyncString=Hook("UnityEngine.AssetBundle::LoadAssetAsync_Internal(System.String,System.Type)",(void*)StringAsyncAsset,(void**)&originalStringAsyncAsset);

    enabled.store(completion || hash || string || asyncHash || asyncString || assetResult,std::memory_order_release);
    Log("[RELOAD-LOAD-READY] enabled=%d nativeCompletion=%d nativeHash=%d nativeString=%d nativeAsyncHash=%d nativeAsyncString=%d nativeAssetResult=%d weakIdentity=1 identityLimit=%zu resourceCallsInitiated=0 cacheIsolationVerified=0",int(Active()),int(completion),int(hash),int(string),int(asyncHash),int(asyncString),int(assetResult),identityLimit);
  });
}
} // namespace LoadingObservation
