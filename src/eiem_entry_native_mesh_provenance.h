#pragma once
// Included after DependencyGroup. This is a read-only observer of a resource
// the game has already returned, not an alternate resource loading path.
std::shared_ptr<const EiemDemandMeshProvenance::Prefab> NativePrefabProof(
    const std::shared_ptr<Manifest>& manifest,const std::string& path){
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::DemandPrefabProof);
 {
  std::lock_guard<std::mutex> lock(manifest->provenanceMutex);
  auto prior=manifest->provenancePrefabs.find(path);if(prior!=manifest->provenancePrefabs.end()){performance.hit=true;return prior->second;}
  if(manifest->provenanceMisses.count(path))return {};
 }
 auto miss=[&]()->std::shared_ptr<const EiemDemandMeshProvenance::Prefab>{
  std::lock_guard<std::mutex> lock(manifest->provenanceMutex);manifest->provenanceMisses.insert(path);return {};
 };
 Bytes info;bool exact=false;if(!LookupTarget(manifest->object.Get(),{path,""},info,exact))return miss();
 uint64_t root=0,rootAsset=0;std::memcpy(&root,info.data()+api.bundleOffset,sizeof(root));
 std::memcpy(&rootAsset,info.data()+api.assetOffset,sizeof(rootAsset));if(!root)return {};
 auto shared=SharedFor(manifest);std::unique_lock<std::mutex> serial(shared->preparation);
 {
  std::lock_guard<std::mutex> lock(manifest->provenanceMutex);
  auto prior=manifest->provenancePrefabs.find(path);if(prior!=manifest->provenancePrefabs.end()){performance.hit=true;return prior->second;}
  if(manifest->provenanceMisses.count(path))return {};
 }
 std::optional<StartupDependencies::Scratch> prototype;
 DependencyGroup group;group.manifest=manifest;group.shared=shared;group.root=root;
 auto queryDependencies=[&]{
  prototype.emplace(StartupDependencies::NewListForManifest(manifest->object.Get()));group.prototype=prototype->Get();
  group.keys=group.Native(root);group.keys.push_back(root);
 };
 if(!manifest->unresolvedObservedMeshes){
  queryDependencies();
  if(std::none_of(group.keys.begin(),group.keys.end(),[&](uint64_t key){return manifest->observedMeshBundles.count(key)!=0;}))return miss();
 }
 auto source=group.Open(root);
 // An unknown logical route warrants a dependency walk only when the exact
 // requested prefab has a possible renderer. Local names only exclude Meshes.
 auto renderers=EiemDemandMeshProvenance::ReadRendererMeshes(*source->reader,path,manifest->catalog->observedMeshTargets,rootAsset);
 if(renderers.empty()){source->file->VerifyStable();return miss();}
 if(manifest->unresolvedObservedMeshes)queryDependencies();
 EiemDemandTargetLookup::Inspection inspection;
 EiemLoadPerformance::Scope walkPerformance(EiemLoadPerformance::DemandPrefabProofWalk);
 // Use the same structural reachability as replacement discovery. A native
 // dependency edge to a Texture/Shader is not evidence of a Mesh container.
 auto complete=group.WalkMeshSources(group.MeshDiscoverySeeds(manifest->observedMeshBundles),[&](uint64_t key){
  if(shared->observationChecked.count(key))return true;
  auto source=group.Open(key);EiemDemandMeshProvenance::Meshes additions;
  for(const auto& identity:manifest->catalog->observedMeshTargets){
   auto binding=manifest->observedMeshBindings.find(identity);
   if(binding!=manifest->observedMeshBindings.end()){
    if(binding->second.first!=key)continue;
    for(const auto& member:source->reader->directory.members){if(!(member.flags&4))continue;auto layout=source->reader->Layout(member);
     auto hashes=source->reader->HashAssets(member,layout);auto selected=hashes.find(binding->second.second);
     if(selected!=hashes.end()&&selected->second.classId==43){
      auto fields=source->reader->Fields(member,layout,selected->second.pathId,{"m_Name"});auto name=fields.find("m_Name");
      if(name!=fields.end()&&Text(name->second.raw.data(),name->second.raw.size())==identity.asset)
       additions[CanonicalSerializedAssetReference({selected->second.cab,selected->second.pathId})].insert(identity);
     }
    }
   }
   auto resolve=[&](const NativeReference& reference,const Identity& target,Asset& asset){
    auto location=group.DependencySource(reference.first,key);
    return inspection.ReferencedMesh(*location.first->reader,reference,target,asset);
   };
   for(const auto& asset:inspection.Find(*source->reader,identity,resolve))
    additions[CanonicalSerializedAssetReference({asset.cab,asset.pathId})].insert(identity);
  }
  group.RememberMeshSources(key,inspection);source->file->VerifyStable();
  for(const auto& row:additions)shared->observedMeshes[row.first].insert(row.second.begin(),row.second.end());
  shared->observationChecked.insert(key);source->reader->DiscardDecodedCache();return true;
 });
 walkPerformance.Pause();
 if(!complete)throw EiemDemandDependencySource::IncompleteSources("native Mesh source proof has unresolved structural references");
 auto proof=std::make_shared<EiemDemandMeshProvenance::Prefab>();proof->path=path;
 for(auto& renderer:renderers){
  auto mesh=shared->observedMeshes.find(renderer.reference);
  if(mesh!=shared->observedMeshes.end())proof->uses.push_back({std::move(renderer.renderer),mesh->second});
 }
 for(const auto& row:group.sources)row.second->file->VerifyStable();
 if(proof->uses.empty())return miss();
 performance.hit=true;walkPerformance.hit=true;
 {
  std::lock_guard<std::mutex> lock(manifest->provenanceMutex);
  manifest->provenancePrefabs.emplace(path,proof);
 }
 Log("[EFFECT-NATIVE-MESH-PROOF] scope=%llu path=%s renderers=%zu requestedDependencyInputs=%zu nativeResourceLoads=0 authorPayloadBytes=0",
  (unsigned long long)manifest->serial,path.c_str(),proof->uses.size(),group.sources.size());
 return proof;
}
void BindNativeMeshProof(const std::shared_ptr<Manifest>& manifest,void* source,
    const std::shared_ptr<const EiemDemandMeshProvenance::Prefab>& proof){
 if(!proof||proof->uses.empty())return;
 {
  std::lock_guard<std::mutex> lock(manifest->provenanceMutex);
  auto old=manifest->provenPrefabSources.find({source,proof->path});
  if(old!=manifest->provenPrefabSources.end()&&old->second.Get()==source)return;
 }
 FullResourceReload::AssemblyTree tree(source);std::map<void*,ProvenNativeMesh> witnesses;
 for(const auto& use:proof->uses){auto renderer=tree.Renderer(use.renderer);
  auto mesh=RuntimeShapes::SourceMesh(renderer);Require(RuntimeShapes::Alive(mesh),"native attachment Mesh retired before proof binding");
  auto& witness=witnesses[mesh];if(!witness.mesh.Get())witness.mesh=Ref::Make(mesh,true);
  witness.identities.insert(use.identities.begin(),use.identities.end());
 }
 {
  std::lock_guard<std::mutex> lock(manifest->provenanceMutex);
  for(auto it=manifest->provenMeshes.begin();it!=manifest->provenMeshes.end();)if(!it->second.mesh.Get())it=manifest->provenMeshes.erase(it);else ++it;
  for(auto it=manifest->provenPrefabSources.begin();it!=manifest->provenPrefabSources.end();)if(!it->second.Get())it=manifest->provenPrefabSources.erase(it);else ++it;
  for(auto& row:witnesses){auto& current=manifest->provenMeshes[row.first];if(current.mesh.Get()!=row.first)current=std::move(row.second);
   else current.identities.insert(row.second.identities.begin(),row.second.identities.end());}
  manifest->provenPrefabSources[{source,proof->path}]=Ref::Make(source,true);
 }
 // Generic story/Timeline clones need the same completion notification as
 // world/UI models, including attach-only Mods with no authored Mesh marker.
 FullResourceReload::ObservePrefabSource(source,proof->path);
}
std::set<Identity> ProvenNativeMeshIdentities(void* mesh){
 std::set<Identity> result;if(!mesh||!enabled||!RuntimeShapes::Main()||!RuntimeShapes::Alive(mesh))return result;
 auto installed=std::atomic_load(&installedGeneration);std::vector<std::shared_ptr<Manifest>> current;
 {std::lock_guard<std::mutex> lock(mutex);for(const auto& row:manifests)if(row.second->catalog==installed)current.push_back(row.second);}
 for(const auto& state:current){std::lock_guard<std::mutex> lock(state->provenanceMutex);auto found=state->provenMeshes.find(mesh);
  if(found!=state->provenMeshes.end()&&found->second.mesh.Get()==mesh)result.insert(found->second.identities.begin(),found->second.identities.end());
 }
 return result;
}
void ObserveNativeMeshSource(void* source,const std::string& requestedPath) noexcept {
 if(!source||!enabled||!RuntimeShapes::Main())return;
 auto installed=std::atomic_load(&installedGeneration);if(!installed||installed->observedMeshTargets.empty())return;
 auto path=FullResourceReload::NormalizeAssetPath(requestedPath);
 if(path.size()<7||path.compare(path.size()-7,7,".prefab"))return;
 try{
  if(!RuntimeShapes::Alive(source)||!FullResourceReload::Inherits(source,"UnityEngine","GameObject"))return;
  std::vector<Ref> objects;
  {std::lock_guard<std::mutex> lock(mutex);for(const auto& row:manifests)if(row.second->object.Get())objects.push_back(row.second->object);}
  for(const auto& object:objects){auto state=Current(object.Get());if(state->catalog!=installed)continue;
   std::shared_ptr<const EiemDemandMeshProvenance::Prefab> proof;
   try{proof=NativePrefabProof(state,path);}
   catch(const EiemDemandDependencySource::IncompleteSources& error){
    // A selected structural source may become available on a later native
    // request. Do not turn a partial walk into a generation-long miss.
    Log("[EFFECT-NATIVE-MESH-PROOF-GAP] scope=%llu path=%s reason=%s nameFallback=0 originalResourcePreserved=1 retryNextRequest=1",
      (unsigned long long)state->serial,path.c_str(),error.what());continue;
   }
   catch(const std::exception& error){
    // A missing/unsupported native input must not repeat disk inspection on
    // every owner reconciliation. F10 installs a fresh manifest and retries.
    {std::lock_guard<std::mutex> lock(state->provenanceMutex);state->provenanceMisses.insert(path);}
    Log("[EFFECT-NATIVE-MESH-PROOF-GAP] scope=%llu path=%s reason=%s nameFallback=0 originalResourcePreserved=1 retryNextGeneration=1",(unsigned long long)state->serial,path.c_str(),error.what());continue;
   }
   BindNativeMeshProof(state,source,proof);
  }
 }catch(const std::exception& error){Log("[EFFECT-NATIVE-MESH-PROOF-GAP] path=%s reason=%s nameFallback=0 originalResourcePreserved=1",path.c_str(),error.what());}
 catch(...){Log("[EFFECT-NATIVE-MESH-PROOF-GAP] path=%s reason=unknown nameFallback=0",path.c_str());}
}
