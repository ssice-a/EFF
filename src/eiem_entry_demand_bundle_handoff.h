#pragma once

// Included inside DemandLoadingProbe. Game proxy references own the handoff;
// this module never creates a replacement loader or destroys live objects.
uint64_t PrefabBundle(void* object,const std::string& path,uint64_t hash=0){
  Bytes info(api.packSize,0);bool exact=false;
  if(hash){void* args[]={&hash,info.data()};if(!RuntimeShapes::Scalar<uint8_t>(api.lookup,object,args))return 0;}
  else if(!LookupTarget(object,{path,""},info,exact))return 0;
  uint64_t key=0;memcpy(&key,info.data()+api.bundleOffset,8);return key;
}
size_t RetireBundleKey(uint64_t hash){
    std::vector<std::shared_ptr<SharedScope>> scopes;{std::lock_guard<std::mutex> lock(mutex);
      for(const auto& row:sharedScopes)scopes.push_back(row.second);}
    size_t retired=0;
    for(const auto& lease:StartupDependencies::ProxiesForBundle(hash)){
      auto proxy=lease.proxy.Get();if(!proxy||RuntimeShapes::Scalar<uint8_t>(StartupDependencies::api.proxyIsUnloaded,proxy))continue;
      void* manager=nullptr;il2cpp_field_get_value(proxy,StartupDependencies::api.proxyManagerField,&manager);
      std::vector<Ref> ancestors;std::vector<uint64_t> queue{hash};std::set<uint64_t> visited{hash};
      // Traverse only cached, already loaded reverse dependants. Never create
      // a proxy, manufacture a reference or force an active holder to unload.
      if(RuntimeShapes::Scalar<int>(StartupDependencies::api.proxyGetRefCount,proxy)>0&&
          manager&&originalSharedReverse&&sharedApi.managerTryGetBundleProxy)
        for(size_t at=0;at<queue.size();++at){
          auto scratch=StartupDependencies::NewListForManifest(lease.manifest.Get());
          std::vector<uint64_t> reverseKeys;bool synthetic=false;
          for(const auto& scope:scopes)if(scope->manifest->object.Get()==lease.manifest.Get()){
            EiemDemandSharedGraph::Input own;synthetic|=scope->graph.Find(queue[at],own);
            auto added=scope->graph.Reverse(queue[at]);reverseKeys.insert(reverseKeys.end(),added.begin(),added.end());}
          if(!synthetic){originalSharedReverse(lease.manifest.Get(),queue[at],scratch.Get(),sharedApi.reverseInfo);
            auto native=StartupDependencies::ReadList(scratch.Get());reverseKeys.insert(reverseKeys.end(),native.begin(),native.end());}
          for(auto reverse:reverseKeys){
            if(!visited.insert(reverse).second)continue;
            Require(visited.size()<=256,"native Bundle retirement reverse-dependency budget exhausted");
            void* args[]={&reverse};auto cached=Call(sharedApi.managerTryGetBundleProxy,manager,args);
            if(!cached||RuntimeShapes::Scalar<uint8_t>(StartupDependencies::api.proxyIsUnloaded,cached))continue;
            ancestors.push_back(Ref::Make(cached,false));queue.push_back(reverse);
          }
        }
      bool progress=true;while(progress){progress=false;
        for(const auto& ancestor:ancestors)if(StartupDependencies::RetireIdleProxy(ancestor.Get())){++retired;progress=true;}}
      if(StartupDependencies::RetireIdleProxy(proxy))++retired;
      if(!RuntimeShapes::Scalar<uint8_t>(StartupDependencies::api.proxyIsUnloaded,proxy))
        throw StartupDependencies::BundleRetainedError("game BundleProxy still retained: key="+std::to_string(hash)+
          " refs="+std::to_string(RuntimeShapes::Scalar<int>(StartupDependencies::api.proxyGetRefCount,proxy)));
    }return retired;
}
size_t RetirePreviousSharedInput(const EiemDemandSharedGraph::Input& input){
  Require(!input.cab.empty(),"shared native input source CAB identity absent");
  // Native member names, external PPtrs, resource paths and AssetBundle names
  // are all isolated before publication. No live original/older proxy needs
  // to be evicted to make this generation loadable.
  if(input.generationIsolated)return 0;
  {std::lock_guard<std::mutex> lock(NativeBundle::memoryMutex);for(const auto& request:NativeBundle::memoryRequests)
    if(request.second.fileIdentity==input.cab)throw StartupDependencies::BundleRetainedError(
      "same-CAB native load still pending: "+input.cab);}
  size_t retired=0;if(input.versionedRoot&&input.categorySource)retired+=RetireBundleKey(input.categorySource);
  std::vector<std::shared_ptr<SharedScope>> owners;{std::lock_guard<std::mutex> lock(mutex);
    for(const auto& row:sharedScopes)owners.push_back(row.second);}
  std::set<uint64_t> keys;for(const auto& owner:owners)for(const auto& previous:owner->graph.InputsForCab(input.cab))keys.insert(previous.key);
  for(auto key:keys)if(key!=input.key)retired+=RetireBundleKey(key);
  return retired+NativeBundle::UnloadIdentity(input.cab);
}
std::mutex nativeCabAliasesMutex;
std::map<uint64_t,std::string> nativeCabAliases;
std::atomic<bool> nativeCabAliasesPresent{false};
void RememberNativeCabAliases(const EiemDemandSharedGraph::Input& input){
  if(!input.versionedRoot||input.generationIsolated)return;
  const auto full=SharedVfsHash(input.cab),bare=SharedVfsPathHash(input.cab);
  std::lock_guard<std::mutex> lock(nativeCabAliasesMutex);
  nativeCabAliases.insert_or_assign(full,input.cab);nativeCabAliases.insert_or_assign(bare,input.cab);nativeCabAliasesPresent=true;
}
void RetireSyntheticForNativeVfs(uint64_t hash){
  if(!nativeCabAliasesPresent.load(std::memory_order_relaxed))return;
  std::string cab;{std::lock_guard<std::mutex> lock(nativeCabAliasesMutex);auto found=nativeCabAliases.find(hash);
    if(found==nativeCabAliases.end())return;cab=found->second;}
  EiemDemandSharedGraph::Input retired;retired.cab=std::move(cab);RetirePreviousSharedInput(retired);
}
// Cache gate for F10. This uses only the current logical manifest and cached
// proxies. Payload preparation remains at the game's actual load-tree entry.
void RetireNativePrefabContainers(const std::string& path,uint64_t pathHash=0){
  if((path.empty()&&!pathHash)||!sharedInputsEnabled)return;
  std::vector<Ref> objects;{std::lock_guard<std::mutex> lock(mutex);for(const auto& row:manifests)
    if(row.second->object.Get())objects.push_back(row.second->object);}
  for(const auto& object:objects){
    const auto root=PrefabBundle(object.Get(),path,pathHash);if(!root)continue;
    auto state=Current(object.Get());auto scratch=StartupDependencies::NewListForManifest(object.Get());
    StartupDependencies::originalAll(object.Get(),root,scratch.Get(),StartupDependencies::api.allInfo);
    auto keys=StartupDependencies::ReadList(scratch.Get());keys.push_back(root);
    std::set<uint64_t> affected;
    for(auto key:keys){
      bool relevant=state->bundles.count(key)!=0;
      if(!relevant){auto deps=StartupDependencies::NewListForManifest(object.Get());
        StartupDependencies::originalAll(object.Get(),key,deps.Get(),StartupDependencies::api.allInfo);
        for(auto dependency:StartupDependencies::ReadList(deps.Get()))relevant|=state->bundles.count(dependency)!=0;}
      if(relevant){affected.insert(key);RetireBundleKey(key);}
    }
    std::vector<std::shared_ptr<SharedScope>> oldScopes;{std::lock_guard<std::mutex> lock(mutex);
      for(const auto& scope:sharedScopes)if(scope.second->manifest->object.Get()==object.Get()&&scope.first!=state->serial)oldScopes.push_back(scope.second);}
    std::vector<EiemDemandSharedGraph::Input> previous;
    for(const auto& scope:oldScopes){auto inputs=scope->graph.InputsForSources(affected);previous.insert(previous.end(),inputs.begin(),inputs.end());}
    for(const auto& input:previous)RetireBundleKey(input.key);
    for(auto input:previous){input.key=0;input.versionedRoot=false;RetirePreviousSharedInput(input);}
  }
}
