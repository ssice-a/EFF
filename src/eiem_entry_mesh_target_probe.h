#pragma once

// Matching stage of the new-route acceptance build. This module does not mutate
// source resources or load author payloads. A separately gated native delivery
// acceptance consumes its exact hits; matching-only builds remain unchanged.
namespace MeshTargetProbe {
using RuntimeShapes::Ref;
using RuntimeShapes::Call;
using RuntimeShapes::Scalar;
using Clock = std::chrono::steady_clock;
struct Api {
  void *lookup=nullptr, *subLookup=nullptr, *packClass=nullptr, *proxyInfo=nullptr;
  void *assetPath=nullptr, *bundleGetter=nullptr, *loadPath=nullptr, *loadHash=nullptr;
  size_t packSize=0, bundleOffset=0, assetOffset=0;
} api;
std::vector<EiemMeshTargetDirectory::Target> targets;
std::unordered_map<std::string,std::vector<size_t>> names;
Ref meshType;
std::atomic<bool> enabled{false};
using AssetGetFn=void*(*)(void*,void*);
AssetGetFn originalAssetGet=nullptr;
void ReadInstalledTargets() {
  auto begin=Clock::now();size_t mods=0,resourceReferences=0;
  for(const auto& root:DiscoverModRoots()) {
    try {
      auto declarations=EiemMeshTargetDirectory::Read(root);
      for(const auto& target:declarations)resourceReferences+=target.resources.size();
      targets.insert(targets.end(),std::make_move_iterator(declarations.begin()),std::make_move_iterator(declarations.end()));++mods;
    } catch(const std::exception& e) {Log("[MESH-TARGET-GAP] phase=ini root=%s reason=%s",root.u8string().c_str(),e.what());}
  }
  Log("[MESH-TARGET-DIRECTORY] mods=%zu targets=%zu resourceReferences=%zu readMs=%.3f resourcePayloadBytesRead=0 compiledInputsRead=0 consumerPrefabEnumeration=0 replacementApplied=0",
    mods,targets.size(),resourceReferences,std::chrono::duration<double,std::milli>(Clock::now()-begin).count());
}
struct Binding { uint64_t bundle=0, asset=0; bool found=false; Ref mesh; };
struct Manifest {
  Ref object, pool;
  std::vector<Binding> bindings;
  std::unordered_set<uint64_t> targetBundles;
  std::unordered_map<uint64_t,bool> candidates;
  std::unordered_map<uint64_t,std::unordered_set<uint64_t>> candidateTargets;
};
std::recursive_mutex mutex;
std::map<void*,Manifest> manifests;
// Loader callbacks can run on workers. They only publish small observations;
// they never wait for the main-thread matcher or enter a Unity loading API.
struct ManifestEvent {Ref object,pool;bool dirty=true;};
std::mutex eventMutex;
std::map<void*,ManifestEvent> manifestEvents;
std::map<std::pair<void*,uint64_t>,Ref> clearEvents;
struct Observation { Ref object; int id=0; size_t hits=0; unsigned boundaries=0; uint64_t serial=0; };
std::unordered_map<void*,Observation> observations;
std::deque<std::pair<void*,uint64_t>> observationOrder;
uint64_t observationSerial=0;
std::unordered_map<void*,Ref> observedTargetMeshes;
struct RawDelivery {Ref source,info;uint64_t pathHash=0;};
std::mutex rawMutex;
std::map<void*,RawDelivery> rawDeliveries;
uint64_t capturedRaw=0,drainedRaw=0,retriedRaw=0;
void* rawCursor=nullptr;
std::set<int> lateMeshes;
struct Counters {
  uint64_t dependencies=0, dependencyItems=0, resolved=0, missing=0;
  uint64_t sources=0, cached=0, rejected=0, unknown=0, incomplete=0, candidates=0, walks=0, renderers=0;
  uint64_t exactLookups=0, hits=0, matchedPrefabs=0, clones=0, unknownClones=0, consumerHits=0, lateConsumerHits=0;
  double manifestMs=0, dependencyMs=0, sourceMs=0, maxSourceMs=0, meshMs=0, maxWalkMs=0, consumerMs=0;
} counters;
inline double Ms(Clock::time_point begin) { return std::chrono::duration<double,std::milli>(Clock::now()-begin).count(); }
bool Lookup(void* manifest,const EiemDirectory::Identity& source,Bytes& info) {
  if(source.path.size()>=4&&source.path.substr(source.path.size()-4)==".fbx") {
    auto path=Ref::Make(il2cpp_string_new(source.path.c_str()),false);
    auto name=Ref::Make(il2cpp_string_new(source.asset.c_str()),false);
    void* args[]={path.Get(),info.data(),name.Get()};
    return Scalar<unsigned char>(api.subLookup,manifest,args)!=0;
  }
  auto path=FullResourceReload::Path(source.path); uint64_t hash=0;
  il2cpp_field_get_value(path.Get(),FullResourceReload::api.pathHashField,&hash);
  void* args[]={&hash,info.data()}; return Scalar<unsigned char>(api.lookup,manifest,args)!=0;
}
Manifest& Remember(void* object) {
  auto found=manifests.find(object);
  if(found!=manifests.end()&&found->second.object.Get()==object) return found->second;
  auto begin=Clock::now();
  Manifest next; next.object=Ref::Make(object,true); next.bindings.resize(targets.size());
  for(size_t i=0;i<targets.size();++i) {
    Bytes info(api.packSize); auto& binding=next.bindings[i];
    if(!Lookup(object,targets[i].source,info)) {
      ++counters.missing;
      Log("[MESH-TARGET-MISSING] mod=%s path=%s asset=%s manifestLookupOnly=1",targets[i].mod.c_str(),targets[i].source.path.c_str(),targets[i].source.asset.c_str());
      continue;
    }
    memcpy(&binding.bundle,info.data()+api.bundleOffset,8);
    memcpy(&binding.asset,info.data()+api.assetOffset,8); binding.found=true;
    next.targetBundles.insert(binding.bundle); ++counters.resolved;
  }
  auto elapsed=Ms(begin);counters.manifestMs+=elapsed;
  Log("[MESH-TARGET-MANIFEST] declared=%zu bundleKeys=%zu resolveMs=%.3f lookupByLogicalIdentity=1 reverseSearch=0 prefabEnumeration=0",targets.size(),next.targetBundles.size(),elapsed);
  return manifests.insert_or_assign(object,std::move(next)).first->second;
}
void Dependencies(void* object,uint64_t bundle,void* list,bool complete) noexcept {
  if(!enabled) return;
  (void)bundle;(void)complete;
  try {
    std::lock_guard<std::mutex> lock(eventMutex);auto found=manifestEvents.find(object);
    if(found!=manifestEvents.end()&&found->second.object.Get()==object&&
       (!StartupDependencies::api.poolField||found->second.pool.Get()))return;
    void* pool=nullptr;if(StartupDependencies::api.poolField)il2cpp_field_get_value(list,StartupDependencies::api.poolField,&pool);
    manifestEvents[object]={Ref::Make(object,true),Ref::Make(pool,true),true};
  } catch(const std::exception& e) { Log("[MESH-TARGET-GAP] phase=dependencies reason=%s",e.what()); }
}
void BundleCleared(void* manifest,uint64_t bundle) noexcept {
  if(!enabled) return;
  try {std::lock_guard<std::mutex> lock(eventMutex);clearEvents[{manifest,bundle}]=Ref::Make(manifest,true);}
  catch(const std::exception& e){Log("[MESH-TARGET-GAP] phase=retire reason=%s",e.what());}
}
void DrainEvents() {
  std::vector<ManifestEvent> pending;
  std::map<std::pair<void*,uint64_t>,Ref> cleared;
  {std::lock_guard<std::mutex> lock(eventMutex);
    for(auto it=manifestEvents.begin();it!=manifestEvents.end();) {
      if(!it->second.object.Get()){it=manifestEvents.erase(it);continue;}
      if(it->second.dirty){pending.push_back(it->second);it->second.dirty=false;}++it;
    }
    cleared.swap(clearEvents);
  }
  std::lock_guard<std::recursive_mutex> lock(mutex);
  for(const auto& event:pending)if(auto object=event.object.Get()) {auto& state=Remember(object);if(event.pool.Get())state.pool=event.pool;}
  for(const auto& event:cleared) {
    auto found=manifests.find(event.first.first);if(found==manifests.end()||found->second.object.Get()!=event.second.Get())continue;
    found->second.candidates.erase(event.first.second);
    found->second.candidateTargets.erase(event.first.second);
    for(auto& binding:found->second.bindings)if(binding.bundle==event.first.second)binding.mesh={};
  }
}
std::optional<bool> Candidate(Manifest& state,uint64_t bundle) {
  if(auto found=state.candidates.find(bundle);found!=state.candidates.end()) return found->second;
  // Query exactly this requested Bundle through the game's native index. The
  // pool itself is retained weakly, never a disposed pooled-list instance.
  if(StartupDependencies::api.poolAllocate&&!state.pool.Get()) return {};
  auto begin=Clock::now();
  auto list=StartupDependencies::api.poolAllocate
    ? StartupDependencies::Scratch(Ref::Make(Call(StartupDependencies::api.poolAllocate,state.pool.Get()),false),true)
    : StartupDependencies::NewList();
  Require(list.Get(),"Mesh target native dependency list unavailable");
  StartupDependencies::originalAll(state.object.Get(),bundle,list.Get(),StartupDependencies::api.allInfo);
  auto values=StartupDependencies::ReadList(list.Get());
  bool result=EiemMeshTargetDirectory::Candidate(bundle,values,state.targetBundles);
  auto& selected=state.candidateTargets[bundle];
  if(state.targetBundles.count(bundle))selected.insert(bundle);
  for(auto key:values)if(state.targetBundles.count(key))selected.insert(key);
  state.candidates.emplace(bundle,result); ++counters.dependencies; counters.dependencyItems+=values.size();
  counters.dependencyMs+=Ms(begin);
  return result;
}
Ref ExactMesh(Manifest& state,size_t index) {
  auto& binding=state.bindings.at(index); if(!binding.found) return {};
  if(RuntimeShapes::Alive(binding.mesh.Get())) return Ref::Make(binding.mesh.Get(),false);
  Ref proxy;
  { std::lock_guard<std::mutex> lock(StartupDependencies::mutex);
    auto key=StartupDependencies::snapshotKeys.find({state.object.Get(),binding.bundle});
    if(key!=StartupDependencies::snapshotKeys.end()) {
      auto value=StartupDependencies::snapshots.find(key->second);
      if(value!=StartupDependencies::snapshots.end()&&value->second.proxy.Get()==key->second) proxy=value->second.proxy;
    }
  }
  if(!proxy.Get()) return {};
  auto bundle=Ref::Make(Call(api.bundleGetter,proxy.Get()),false); if(!bundle.Get()) return {};
  auto begin=Clock::now(); Ref result;
  const auto& source=targets.at(index).source;
  if(source.path.size()>=4&&source.path.substr(source.path.size()-4)==".fbx") {
    auto hash=binding.asset;void* args[]={&hash,meshType.Get()};result=Ref::Make(Call(api.loadHash,bundle.Get(),args),false);
  } else {
    auto path=Ref::Make(il2cpp_string_new(source.path.c_str()),false);
    void* args[]={path.Get(),meshType.Get()};result=Ref::Make(Call(api.loadPath,bundle.Get(),args),false);
  }
  ++counters.exactLookups; counters.meshMs+=Ms(begin);
  if(RuntimeShapes::Alive(result.Get())) {
    Require(String(Call(RuntimeShapes::api.name,result.Get()))==source.asset,"exact target returned a different named Mesh");
    binding.mesh=Ref::Make(result.Get(),true);
  }
  return result;
}
// Exact current logical identity on an ALREADY loaded Bundle only. Used by
// native resource acceptance for its declared Material donor, never by a
// global name search or a blocking ResourceManager request.
void* LoadedAsset(const EiemDirectory::Identity& source,void* type) {
  Require(RuntimeShapes::Main(),"exact loaded asset query requires Unity thread");
  for(auto& row:manifests){auto& state=row.second;if(!state.object.Get())continue;Bytes info(api.packSize);if(!Lookup(state.object.Get(),source,info))continue;uint64_t key=0,hash=0;memcpy(&key,info.data()+api.bundleOffset,8);memcpy(&hash,info.data()+api.assetOffset,8);Ref proxy;
    {std::lock_guard<std::mutex> lock(StartupDependencies::mutex);auto entry=StartupDependencies::snapshotKeys.find({state.object.Get(),key});if(entry!=StartupDependencies::snapshotKeys.end()){auto snapshot=StartupDependencies::snapshots.find(entry->second);if(snapshot!=StartupDependencies::snapshots.end()&&snapshot->second.proxy.Get()==entry->second)proxy=snapshot->second.proxy;}}
    if(!proxy.Get())continue;auto bundle=Ref::Make(Call(api.bundleGetter,proxy.Get()),false);if(!bundle.Get())continue;Ref asset;
    if(source.path.size()>=4&&source.path.substr(source.path.size()-4)==".fbx"){void* args[]={&hash,type};asset=Ref::Make(Call(api.loadHash,bundle.Get(),args),false);}
    else{auto path=Ref::Make(il2cpp_string_new(source.path.c_str()),false);void* args[]={path.Get(),type};asset=Ref::Make(Call(api.loadPath,bundle.Get(),args),false);}
    if(RuntimeShapes::Alive(asset.Get())){Require(String(Call(RuntimeShapes::api.name,asset.Get()))==source.asset,"exact loaded asset returned another named object");return asset.Get();}
  }return nullptr;
}
bool Cached(void* source) {
  auto found=observations.find(source); if(found==observations.end()) return false;
  if(found->second.object.Get()==source&&RuntimeShapes::Alive(source)&&FullResourceReload::Id(source)==found->second.id) {
    ++counters.cached;return true;
  }
  observations.erase(found);return false;
}
void Save(void* source,size_t hits) {
  if(!observations.count(source)) {
    while(observationOrder.size()>=8192) {
      auto entry=observationOrder.front();observationOrder.pop_front();
      auto found=observations.find(entry.first);if(found!=observations.end()&&found->second.serial==entry.second)observations.erase(found);
    }
  }
  auto serial=++observationSerial;observationOrder.push_back({source,serial});
  observations[source]={Ref::Make(source,true),FullResourceReload::Id(source),hits,0,serial};
}
void ObserveAsset(void* source,uint64_t pathHash,void* asset,const char* boundary,const Ref& capturedInfo={}) {
  DrainEvents();
  std::lock_guard<std::recursive_mutex> lock(mutex);
  if(Cached(source)) return;
  if(!FullResourceReload::Inherits(source,"UnityEngine","GameObject")||!RuntimeShapes::Alive(source)) return;
  ++counters.sources; auto begin=Clock::now();
  struct Timer { Clock::time_point begin; ~Timer(){auto ms=Ms(begin);counters.sourceMs+=ms;counters.maxSourceMs=std::max(counters.maxSourceMs,ms);} } timer{begin};
  auto boxed=capturedInfo.Get()?capturedInfo:Ref::Make(Call(api.proxyInfo,asset),false);
  Require(boxed.Get()&&il2cpp_object_get_class(boxed.Get())==api.packClass,"source AssetProxy returned a different AssetInfoPack");
  auto info=il2cpp_object_unbox(boxed.Get()); uint64_t bundle=0;memcpy(&bundle,(char*)info+api.bundleOffset,8);
  std::vector<Manifest*> eligible; bool unknown=false;
  for(auto it=manifests.begin();it!=manifests.end();) {
    if(!it->second.object.Get()) {it=manifests.erase(it);continue;}
    auto candidate=Candidate(it->second,bundle);
    if(!candidate) unknown=true; else if(*candidate) eligible.push_back(&it->second);
    ++it;
  }
  if(eligible.empty()) {
    if(unknown||manifests.empty()) {
      ++counters.unknown;
      if(counters.unknown<=8) Log("[MESH-TARGET-GAP] phase=source-filter pathHash=%016llX reason=native-dependency-evidence-not-ready retryOnNextDelivery=1 hierarchyWalk=0",(unsigned long long)pathHash);
    } else {++counters.rejected;Save(source,0);} return;
  }
  ++counters.candidates; auto path=String(Call(api.assetPath,info));
  auto walk=Clock::now(); auto renderers=FullResourceReload::Components(source); ++counters.walks; counters.renderers+=renderers.size();
  counters.maxWalkMs=std::max(counters.maxWalkMs,Ms(walk)); size_t hits=0;bool incomplete=unknown;
  for(const auto& renderer:renderers) {
    auto mesh=Ref::Make(RuntimeShapes::SourceMesh(renderer.Get()),false); if(!RuntimeShapes::Alive(mesh.Get())) continue;
    auto name=String(Call(RuntimeShapes::api.name,mesh.Get())); auto wanted=names.find(name); if(wanted==names.end()) continue;
    for(auto index:wanted->second) {
      bool exact=false;
      for(auto* state:eligible) {
        if(!state->bindings.at(index).found)continue;
        auto selected=state->candidateTargets.find(bundle);
        if(selected!=state->candidateTargets.end()&&!selected->second.count(state->bindings.at(index).bundle))continue;
        auto target=ExactMesh(*state,index);
        if(!RuntimeShapes::Alive(target.Get())){incomplete=true;continue;}
        if(target.Get()==mesh.Get()){exact=true;break;}
      }
      if(!exact) continue;
      observedTargetMeshes[mesh.Get()]=Ref::Make(mesh.Get(),true);
      ++hits; ++counters.hits; const auto& target=targets.at(index);
#if defined(EIEM_MESH_NATIVE_ACCEPTANCE)
      MeshNativeAcceptance::Matched(source,renderer.Get(),mesh.Get(),target,path);
#endif
      Log("[MESH-TARGET-HIT] mod=%s rule=%s lod=%u prefab=%s rendererId=%d meshId=%d target=%s asset=%s parts=%zu skip=%d boundary=%s exactNativeIdentity=1 replacementApplied=0",
        target.mod.c_str(),target.rule.c_str(),target.lod,path.c_str(),FullResourceReload::Id(renderer.Get()),FullResourceReload::Id(mesh.Get()),target.source.path.c_str(),target.source.asset.c_str(),target.parts,int(target.skip),boundary);
    }
  }
  if(!incomplete)Save(source,hits);
  else {++counters.incomplete;if(counters.incomplete<=8)Log("[MESH-TARGET-GAP] phase=mesh-identity path=%s exactBindingNotReady=1 negativeResultCached=0",path.c_str());}
  if(hits)++counters.matchedPrefabs;
  Log("[MESH-TARGET-PREFAB] path=%s bundle=%016llX renderers=%zu hits=%zu discoveryMs=%.3f sourceBoundary=%s nativeLoadedHierarchyOnly=1 serializedParse=0 replacementApplied=0",
    path.c_str(),(unsigned long long)bundle,renderers.size(),hits,Ms(begin),boundary);
}
// AssetProxy.Get can complete on the game's loading workers. Snapshot only
// its immutable AssetInfoPack there. Do not walk/modify a Unity hierarchy or
// take the main matcher lock. Holding the boxed pack roots managed members;
// copying unrooted struct bytes would leave its path references unprotected.
void CaptureWorkerSource(void* source,void* asset) {
  auto boxed=Ref::Make(Call(api.proxyInfo,asset),false);
  Require(boxed.Get()&&il2cpp_object_get_class(boxed.Get())==api.packClass,"worker source returned another AssetInfoPack");
  auto hash=Scalar<uint64_t>(FullResourceReload::api.proxyHash,asset);
  RawDelivery delivery{Ref::Make(source,true),boxed,hash};
  std::lock_guard<std::mutex> lock(rawMutex);
  Require(rawDeliveries.size()<8192||rawDeliveries.count(source),"worker raw source mailbox full");
  rawDeliveries[source]=std::move(delivery);++capturedRaw;
}
void DrainRawSource(void* source) {
  Require(RuntimeShapes::Main(),"worker source matching requires Unity thread");
  RawDelivery delivery;
  {std::lock_guard<std::mutex> lock(rawMutex);auto found=rawDeliveries.find(source);if(found==rawDeliveries.end())return;delivery=std::move(found->second);rawDeliveries.erase(found);++drainedRaw;}
  if(delivery.source.Get()!=source||!RuntimeShapes::Alive(source))return;
  auto retry=[&]{std::lock_guard<std::mutex> lock(rawMutex);if(rawDeliveries.size()<8192&&rawDeliveries.try_emplace(source,delivery).second)++retriedRaw;};
  try {
    ObserveAsset(source,delivery.pathHash,nullptr,"worker-delivery-before-consumer",delivery.info);
    std::lock_guard<std::recursive_mutex> lock(mutex);
    // Missing manifest/Mesh evidence is retryable. Keep its independently
    // rooted pack; a later attempt must not read a reused AssetProxy.
    if(!observations.count(source))retry();
  }catch(...){retry();throw;}
}
void DrainRawSources() {
  Require(RuntimeShapes::Main(),"worker source mailbox drain requires Unity thread");
  auto begin=Clock::now();
  std::vector<void*> pending;
  {std::lock_guard<std::mutex> lock(rawMutex);auto it=rawDeliveries.upper_bound(rawCursor);
    for(size_t i=0,count=std::min<size_t>(16,rawDeliveries.size());i<count;++i){if(it==rawDeliveries.end())it=rawDeliveries.begin();pending.push_back(it->first);++it;}}
  for(auto source:pending){rawCursor=source;DrainRawSource(source);if(Ms(begin)>1)break;
  }
}
void ObserveProxy(void* source,uint64_t hash,void* asset) noexcept {
  if(!enabled||!RuntimeShapes::Main()) return;
  try {ObserveAsset(source,hash,asset,"prefab-proxy-before-preload");}
  catch(const std::exception& e){Log("[MESH-TARGET-GAP] phase=proxy reason=%s",e.what());}
}
void* AssetGet(void* self,void* info) {
  auto source=originalAssetGet(self,info);
  if(!enabled||!source) return source;
  try {
    if(!FullResourceReload::Inherits(source,"UnityEngine","GameObject"))return source;
    if(!RuntimeShapes::Main()){CaptureWorkerSource(source,self);return source;}
    {std::lock_guard<std::recursive_mutex> lock(mutex);if(Cached(source))return source;}
    auto hash=Scalar<uint64_t>(FullResourceReload::api.proxyHash,self);
    ObserveAsset(source,hash,self,"asset-proxy-get");
  } catch(const std::exception& e){Log("[MESH-TARGET-GAP] phase=asset-get reason=%s",e.what());}
  return source;
}
void ObserveHandle(void* handle,void* source) noexcept {
  if(!enabled||!RuntimeShapes::Main()) return;
  try {
    {std::lock_guard<std::recursive_mutex> lock(mutex);if(Cached(source))return;}
    if(!FullResourceReload::Inherits(source,"UnityEngine","GameObject")) return;
    auto boxed=Ref::Make(il2cpp_value_box(FullResourceReload::api.handleClass,handle),false);uint32_t alignment=0;
    auto size=il2cpp_class_value_size(FullResourceReload::api.handleUntrackedClass,&alignment);Require(size>0,"source handle layout absent");
    Bytes untracked(size);il2cpp_field_get_value(boxed.Get(),FullResourceReload::api.handleUntrackedField,untracked.data());
    auto asset=Call(FullResourceReload::api.untrackedAssetGet,untracked.data());
    if(!FullResourceReload::Inherits(asset,"","AssetProxy")) return;
    auto hash=Scalar<uint64_t>(FullResourceReload::api.proxyHash,asset);ObserveAsset(source,hash,asset,"raw-resource-handle");
  } catch(const std::exception& e){Log("[MESH-TARGET-GAP] phase=handle reason=%s",e.what());}
}
void BeforeClone(void* source,const char* entry) noexcept {
  if(!enabled||!RuntimeShapes::Main()) return;
  try {
    DrainRawSource(source);
    std::lock_guard<std::recursive_mutex> lock(mutex);auto found=observations.find(source);
    if(found==observations.end()||found->second.object.Get()!=source||!RuntimeShapes::Alive(source)||FullResourceReload::Id(source)!=found->second.id) {++counters.unknownClones;return;}
    if(!found->second.hits)return;
    ++counters.clones;
    if(!found->second.boundaries++) Log("[MESH-TARGET-DELIVERY] sourceId=%d hits=%zu factory=%s matchedBeforeInstantiate=1 replacementApplied=0",found->second.id,found->second.hits,entry);
  } catch(const std::exception& e){Log("[MESH-TARGET-GAP] phase=clone reason=%s",e.what());}
}
void ConsumerList(void* list,const char* boundary) noexcept {
  if(!enabled||!list||!RuntimeShapes::Main())return;
#if defined(EIEM_MESH_NATIVE_ACCEPTANCE)
  MeshNativeAcceptance::Consumer(list,boundary);
#endif
  try {
    DrainEvents();std::lock_guard<std::recursive_mutex> lock(mutex);auto begin=Clock::now();
    auto klass=il2cpp_object_get_class(list);
    auto itemsField=StartupDependencies::Field(klass,"_items"),sizeField=StartupDependencies::Field(klass,"_size");
    void* items=nullptr;int size=0;size_t capacity=0;
    il2cpp_field_get_value(list,itemsField,&items);il2cpp_field_get_value(list,sizeField,&size);
    Require(size>=0&&ArrayLength(items,&capacity)&&size_t(size)<=capacity,"consumer Renderer list range differs");
    size_t hits=0,late=0;
    for(int i=0;i<size;++i) {
      void* renderer=nullptr;Require(CopyRaw((char*)items+32+size_t(i)*sizeof(void*),&renderer,sizeof(renderer)),"consumer Renderer list unreadable");
      if(!RuntimeShapes::Alive(renderer)||!RuntimeShapes::RendererKind(renderer))continue;
      auto mesh=Ref::Make(RuntimeShapes::SourceMesh(renderer),false);if(!RuntimeShapes::Alive(mesh.Get()))continue;
      auto wanted=names.find(String(Call(RuntimeShapes::api.name,mesh.Get())));if(wanted==names.end())continue;
      bool exact=false;
      for(auto index:wanted->second)for(auto& state:manifests){auto target=ExactMesh(state.second,index);if(target.Get()&&target.Get()==mesh.Get()){exact=true;break;}}
      if(!exact)continue;
      ++hits;auto observed=observedTargetMeshes.find(mesh.Get());if(observed==observedTargetMeshes.end()||observed->second.Get()!=mesh.Get()){
        ++late;auto id=FullResourceReload::Id(mesh.Get());if(lateMeshes.size()<64&&lateMeshes.insert(id).second)Log("[MESH-TARGET-LATE] boundary=%s meshId=%d mesh=%s rendererId=%d earlierSourceEvidenceAbsent=1",boundary,id,String(Call(RuntimeShapes::api.name,mesh.Get())).c_str(),FullResourceReload::Id(renderer));
      }
    }
    auto elapsed=Ms(begin);counters.consumerMs+=elapsed;counters.consumerHits+=hits;counters.lateConsumerHits+=late;
    if(hits)Log("[MESH-TARGET-CONSUMER] boundary=%s renderers=%d exactHits=%zu hitsWithoutEarlierSourceMatch=%zu inspectMs=%.3f usesGameRendererList=1 hierarchyWalk=0 replacementApplied=0",boundary,size,hits,late,elapsed);
  }catch(const std::exception& e){Log("[MESH-TARGET-GAP] phase=consumer-audit reason=%s",e.what());}
}
void Report(int frame) noexcept {
  if(!enabled) return;
#if defined(EIEM_MESH_NATIVE_ACCEPTANCE)
  MeshNativeAcceptance::Frame(frame);
#endif
  try{DrainEvents();DrainRawSources();}catch(const std::exception& e){Log("[MESH-TARGET-GAP] phase=manifest-events reason=%s",e.what());}
  if(frame%300)return;
  {std::lock_guard<std::mutex> lock(rawMutex);Log("[MESH-TARGET-WORKER-DELIVERY] frame=%d captured=%llu drained=%llu retries=%llu pending=%zu nativeHierarchyQueriesOnWorker=0 mainMatcherLockOnWorker=0",frame,(unsigned long long)capturedRaw,(unsigned long long)drainedRaw,(unsigned long long)retriedRaw,rawDeliveries.size());}
  std::lock_guard<std::recursive_mutex> lock(mutex); const auto& c=counters;
  for(auto it=observedTargetMeshes.begin();it!=observedTargetMeshes.end();)if(!RuntimeShapes::Alive(it->second.Get()))it=observedTargetMeshes.erase(it);else ++it;
  Log("[MESH-TARGET-STATS] frame=%d dependencyQueries=%llu dependencyItems=%llu resolved=%llu missing=%llu sources=%llu cached=%llu rejected=%llu unknown=%llu incomplete=%llu candidates=%llu matchedPrefabs=%llu hierarchyWalks=%llu renderers=%llu exactMeshLookups=%llu hits=%llu matchedClones=%llu unobservedCloneCalls=%llu consumerHits=%llu lateConsumerHits=%llu manifestMs=%.3f dependencyMs=%.3f sourceMs=%.3f maxSourceMs=%.3f meshLookupMs=%.3f maxHierarchyMs=%.3f consumerMs=%.3f weakSourceEntries=%zu compiledInputsRead=0 replacementApplied=0",
    frame,c.dependencies,c.dependencyItems,c.resolved,c.missing,c.sources,c.cached,c.rejected,c.unknown,c.incomplete,c.candidates,c.matchedPrefabs,c.walks,c.renderers,c.exactLookups,c.hits,c.clones,c.unknownClones,c.consumerHits,c.lateConsumerHits,c.manifestMs,c.dependencyMs,c.sourceMs,c.maxSourceMs,c.meshMs,c.maxWalkMs,c.consumerMs,observations.size());
}
void Install(void** assemblies,size_t count) {
  try {
    Require(FullResourceReload::enabled&&StartupDependencies::enabled,"Mesh target acceptance requires game loader bindings");
    auto klass=FindClass("Beyond.ManifestBinary","RuntimeManifestBinary",assemblies,count);
    void* iterator=nullptr;
    while(auto method=il2cpp_class_get_methods(klass,&iterator)) {
      if(strcmp(il2cpp_method_get_name(method),"TryGetAssetInfoFromPath")||il2cpp_method_get_param_count(method)!=2)continue;
      auto first=StartupDependencies::TypeName(il2cpp_method_get_param(method,0));if(first!="System.UInt64"&&first!="System.Int64")continue;
      Require(!api.lookup&&il2cpp_type_is_byref(il2cpp_method_get_param(method,1))&&StartupDependencies::TypeName(il2cpp_method_get_return_type(method))=="System.Boolean","Mesh target native lookup signature differs");
      api.lookup=method;api.packClass=il2cpp_class_from_type(il2cpp_method_get_param(method,1));
    }
    Require(api.lookup&&api.packClass,"Mesh target native directory lookup absent");api.packSize=StartupDependencies::ValueSize(api.packClass);
    auto field=StartupDependencies::Field(api.packClass,"bundleData");auto bundleClass=il2cpp_class_from_type(il2cpp_field_get_type(field));
    api.bundleOffset=StartupDependencies::ValueOffset(api.packClass,"bundleData",StartupDependencies::ValueSize(bundleClass))+StartupDependencies::ValueOffset(bundleClass,"hashName",8);
    field=StartupDependencies::Field(api.packClass,"assetData");auto assetClass=il2cpp_class_from_type(il2cpp_field_get_type(field));
    api.assetOffset=StartupDependencies::ValueOffset(api.packClass,"assetData",StartupDependencies::ValueSize(assetClass))+StartupDependencies::ValueOffset(assetClass,"pathHashHead",8);
    Require(api.bundleOffset+8<=api.packSize&&api.assetOffset+8<=api.packSize,"Mesh target pack fields escape result");
    auto pack=StartupDependencies::TypeName(il2cpp_class_get_type(api.packClass)),packRef=pack+"&";
    api.subLookup=FullResourceReload::Method(klass,"TryGetAssetInfoFromPath",{"System.String",packRef.c_str(),"System.String"},"System.Boolean",false);
    api.assetPath=FullResourceReload::Method(api.packClass,"get_assetPath",{},"System.String",false);
    api.proxyInfo=FullResourceReload::Method(FindClass("","AssetProxy",assemblies,count),"GetAssetInfo",{},pack.c_str(),false);
    api.bundleGetter=FullResourceReload::Method(FindClass("","BundleProxy",assemblies,count),"get_assetBundle",{},"UnityEngine.AssetBundle",false);
    auto bundle=FindClass("UnityEngine","AssetBundle",assemblies,count);
    api.loadPath=FullResourceReload::Method(bundle,"LoadAsset",{"System.String","System.Type"},"UnityEngine.Object",false);
    api.loadHash=FullResourceReload::Method(bundle,"LoadAsset",{"System.Int64","System.Type"},"UnityEngine.Object",false);
    meshType=FullResourceReload::Type(FindClass("UnityEngine","Mesh",assemblies,count));
    for(size_t i=0;i<targets.size();++i)names[targets[i].source.asset].push_back(i);
    Require(Entry::Install(FindClass("","AssetProxy",assemblies,count),"Get",{},"UnityEngine.Object",false,(void*)AssetGet,(void**)&originalAssetGet),"Mesh target raw AssetProxy delivery hook unavailable");
    enabled=true;
    std::vector<Ref> existing;
    {std::lock_guard<std::mutex> lock(StartupDependencies::mutex);std::set<void*> seen;
      for(const auto& entry:StartupDependencies::snapshots)if(auto manifest=entry.second.manifest.Get();manifest&&seen.insert(manifest).second)existing.push_back(entry.second.manifest);}
    {std::lock_guard<std::recursive_mutex> lock(mutex);for(const auto& manifest:existing)Remember(manifest.Get());}
    Log("[MESH-TARGET-READY] targets=%zu currentNativeManifest=1 requestDependencyFilter=1 exactMeshObjectMatch=1 allConsumerPrefabsEnumerated=0 precompiledPfbInputs=0 replacementApplied=0",targets.size());
  } catch(const std::exception& e){enabled=false;Log("[MESH-TARGET-GAP] phase=bind reason=%s replacementApplied=0",e.what());}
}
} // namespace MeshTargetProbe
