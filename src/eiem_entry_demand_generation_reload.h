#pragma once

namespace DemandGenerationReload {
using FullResourceReload::Owner;
using RuntimeShapes::Ref;
constexpr ULONGLONG kPrefabProxyRetryIntervalMs=250;
constexpr ULONGLONG kPrefabProxyWaitTimeoutMs=3000;

struct Prepared {
  std::shared_ptr<const DemandLoadingProbe::InstalledGeneration> generation;
  EiemGlobalConfig config;
};
enum class OwnerPhase {AwaitExisting, Queued, Released, WaitingModel, WaitingUI, Done, Ended, Failed};
struct OwnerReload {
  Owner owner;
  OwnerPhase phase=OwnerPhase::Queued;
  ULONGLONG started=0;
  bool timeoutLogged=false;
  Ref rebuildParent;
};
struct PathReload {
  std::string path;
  uint64_t hash=0;
  std::vector<OwnerReload> owners;
  bool poolDiscarded=false,cacheInvalidated=false;
  bool prefabInvalidated=false,prefabInvalidationAttempted=false,rawCaptured=false;
  ULONGLONG prefabRetryAt=0,prefabBlockedSince=0;
  std::vector<Ref> rawAssets;
  bool failed=false;
  ULONGLONG started=0;
  bool waitLogged=false;
  std::string lastError;
};
std::mutex mutex;
std::shared_ptr<Prepared> pending;
std::vector<PathReload> reloads;
std::set<std::string> pendingUIPaths;
size_t cursor=0;
size_t uncoveredHelpers=0;
bool active=false;
uint64_t committedGeneration=0;

void EnsureOwnerPathHash(Owner& owner) {
  if(!owner.pathHash.Get()) {
    Require(!owner.path.empty(),"affected prefab path unavailable");
    owner.pathHash=FullResourceReload::Path(owner.path);
  }
  Require(owner.pathHash.Get(),"affected prefab path hash unavailable: "+owner.path);
}

size_t RetireOwnerBindings(const Owner& owner) {
  auto root=owner.root.Get();if(!RuntimeShapes::Alive(root))return 0;
  size_t retired=0;
  for(const auto& renderer:FullResourceReload::Components(root)) {
    RuntimeShapes::RetireRenderer(renderer.Get());
    RuntimeMaterials::Forget(renderer.Get());
    ++retired;
  }
  if(retired)Log("[DEMAND-F10-OWNER-INVALIDATE] path=%s renderers=%zu bindingsRetired=1",
    owner.path.c_str(),retired);
  return retired;
}

bool Affected(void* root,const DemandLoadingProbe::InstalledGeneration& next) {
  if(!next.delta.changed)return false;
  if(!RuntimeShapes::Alive(root))return true;
  std::set<std::string> changedParts;
  for(const auto& target:next.targets)if(next.delta.packages.count(target.mod)&&!next.delta.controlOnlyPackages.count(target.mod))
    changedParts.insert(target.partIds.begin(),target.partIds.end());
  for(const auto& renderer:FullResourceReload::Components(root)) {
    auto mesh=RuntimeShapes::SourceMesh(renderer.Get());
    auto spec=RuntimeShapes::Match(renderer.Get(),mesh);
    if(spec&&next.delta.packages.count(spec->package)&&!next.delta.controlOnlyPackages.count(spec->package))return true;
    if(changedParts.empty()||!RuntimeShapes::api.rendererGameObject)continue;
    auto gameObject=Ref::Make(RuntimeShapes::Call(RuntimeShapes::api.rendererGameObject,renderer.Get()),false);
    if(!RuntimeShapes::Alive(gameObject.Get()))continue;
    auto marker=EiemDirectory::RendererPartMarker(String(RuntimeShapes::Call(
      RuntimeShapes::api.name,gameObject.Get())));
    if(changedParts.count(marker))return true;
  }
  return false;
}

uint64_t PathHash(const std::string& path) {
  auto value=FullResourceReload::Path(path);uint64_t hash=0;
  Require(FullResourceReload::api.pathHashField,"game resource path hash field unavailable");
  il2cpp_field_get_value(value.Get(),FullResourceReload::api.pathHashField,&hash);
  Require(hash,"affected prefab path hash is zero: "+path);return hash;
}

uint64_t OwnerPathHash(const Owner& owner) {
  Require(owner.pathHash.Get()&&FullResourceReload::api.pathHashField,"affected prefab path hash unavailable: "+owner.path);
  uint64_t hash=0;il2cpp_field_get_value(owner.pathHash.Get(),FullResourceReload::api.pathHashField,&hash);
  Require(hash,"affected prefab path hash is zero: "+owner.path);return hash;
}

PathReload& AddPath(const std::string& path,uint64_t hash) {
  auto found=std::find_if(reloads.begin(),reloads.end(),[&](const auto& row){
    return row.hash==hash||FullResourceReload::SameAssetPath(row.path,path);
  });
  if(found==reloads.end()){
    PathReload row;row.path=path;row.hash=hash;
    reloads.push_back(std::move(row));return reloads.back();
  }
  if(!found->hash)found->hash=hash;
  if(found->path.rfind("prefab-proxy:",0)==0&&path.rfind("prefab-proxy:",0)!=0)found->path=path;
  return *found;
}

std::shared_ptr<const DemandLoadingProbe::InstalledGeneration> ExcludeMods(
    const std::shared_ptr<const DemandLoadingProbe::InstalledGeneration>& source,
    const std::set<std::string>& excluded) {
  if(excluded.empty())return source;
  auto next=std::make_shared<DemandLoadingProbe::InstalledGeneration>(*source);
  for(const auto& id:excluded){
    next->authors.erase(id);next->snapshot.erase(id);
    next->targets.erase(std::remove_if(next->targets.begin(),next->targets.end(),[&](const auto& target){return target.mod==id;}),next->targets.end());
  }
  auto packages=std::make_shared<std::vector<Package>>();
  for(const auto& package:*source->packages)if(!excluded.count(package.id))packages->push_back(package);
  next->packages=packages;
  auto standalone=std::make_shared<std::vector<Package>>();
  for(const auto& package:*source->standalone)if(!excluded.count(package.id))standalone->push_back(package);
  next->standalone=standalone;
  auto previous=DemandLoadingProbe::CurrentInstalledGeneration();
  next->delta=EiemDemandGenerationDiff::Compare(previous?previous->snapshot:EiemDemandGenerationDiff::Snapshot{},next->snapshot);
  return next;
}

Owner MakePartOwner(void* part,const FullResourceReload::LivePart& live) {
  Owner owner;owner.component=Ref::Make(part,false);owner.root=live.root;
  owner.path=live.path;owner.viewPart=true;owner.oldRootId=live.root.Get()?FullResourceReload::Id(live.root.Get()):0;
  if(live.model.Get()) {
    owner.viewPart=false;owner.component=live.model;owner.entity=Ref::Make(
      FullResourceReload::Call(FullResourceReload::api.modelEntity,owner.component.Get()),false);
    owner.oldRootId=owner.root.Get()?FullResourceReload::Id(owner.root.Get()):0;
    if(owner.entity.Get())FullResourceReload::CaptureConsumers(owner);
  } else if(!owner.path.empty()) {
    owner.config=FullResourceReload::PartConfig(part);
  }
  if(owner.path.empty()) {
    auto config=FullResourceReload::PartConfig(part);
    owner.config=config;
    owner.path=String(FullResourceReload::ReadObject(
      config.Get(),"modelPath","System.String"));
  }
  auto config=owner.config.Get()?owner.config:FullResourceReload::PartConfig(part);
  unsigned char pooled=0;il2cpp_field_get_value(config.Get(),FullResourceReload::Field(
    il2cpp_object_get_class(config.Get()),"usePool","System.Boolean"),&pooled);
  owner.directAsset=!pooled;
  if(owner.directAsset){
    owner.rawAsset=FullResourceReload::PartRawAsset(part);
    Require(owner.rawAsset.Get()||!RuntimeShapes::Alive(owner.root.Get()),"direct model part has no raw AssetProxy: "+owner.path);
  }
  EnsureOwnerPathHash(owner);
  return owner;
}

Owner MakeUIOwner(void* root,const FullResourceReload::LiveUIModel& live) {
  Owner owner;owner.uiModel=true;owner.viewPart=true;owner.component=live.loader;
  owner.root=live.root;owner.uiLoadedRoot=live.root;owner.uiParent=live.parent;
  owner.uiCallback=live.callback;owner.path=live.path;owner.oldRootId=FullResourceReload::Id(root);
  EnsureOwnerPathHash(owner);
  owner.uiPage=FullResourceReload::CaptureUIPage(live.loader.Get(),root);
  Require(FullResourceReload::BindUIPage(owner.uiPage),"UI page does not own its recorded model");
  return owner;
}

void Prepare() {
  try {
    auto candidate=std::make_shared<Prepared>();
    candidate->generation=DemandLoadingProbe::PrepareInstalledGeneration();
    Require(bool(candidate->generation),"Format 3 F10 candidate generation is absent");
    Require(DemandLoadingProbe::InstalledGenerationFilesUnchanged(*candidate->generation),
      "Format 3 Mod files changed while F10 declarations were prepared");
    candidate->config=EiemEntrySettings::Read(gameDirectory);
    std::set<std::string> excluded;auto expected=candidate->config.reload.vk,gui=candidate->config.gui.vk;
    for(const auto& package:*candidate->generation->packages){
      try{
        for(const auto& key:package.bindings)Require(key.key.vk!=expected&&key.key.vk!=gui,
          "Mod shortcut overlaps a global shortcut: "+key.id);
        for(const auto& animation:package.shapeAnimations)Require(animation.second.key.vk!=expected&&animation.second.key.vk!=gui,
          "Mod animation shortcut overlaps a global shortcut: "+animation.first);
      }
      catch(const std::exception& error){excluded.insert(package.id);Log("[DEMAND-F10-MOD-SKIP] mod=%s reason=%s otherModsContinue=1",package.id.c_str(),error.what());}
    }
    candidate->generation=ExcludeMods(candidate->generation,excluded);
    Require(DemandLoadingProbe::InstalledGenerationFilesUnchanged(*candidate->generation),
      "Format 3 Mod files changed while F10 declarations were validated");
    {std::lock_guard<std::mutex> lock(mutex);pending=candidate;}
    const auto& delta=candidate->generation->delta;
    Log("[DEMAND-F10-PREPARED] generation=%llu changed=%d mods=%zu added=%zu removed=%zu modified=%zu affectedTargets=%zu resourcePayloadBytesRead=0 compiledBinRead=0",
      (unsigned long long)candidate->generation->serial,int(delta.changed),candidate->generation->packages->size(),
      delta.addedPackages.size(),delta.removedPackages.size(),delta.modifiedPackages.size(),delta.targets.size());
  } catch(const std::exception& error) {
    {std::lock_guard<std::mutex> lock(mutex);pending.reset();}
    Log("[DEMAND-F10-REJECT] phase=prepare reason=%s previousGenerationRetained=1 resourcePayloadBytesRead=0",error.what());
  }
}

void AddAffectedOwners(const DemandLoadingProbe::InstalledGeneration& candidate) {
  auto previous=std::move(reloads);
  reloads.clear();pendingUIPaths.clear();cursor=0;uncoveredHelpers=0;
  std::vector<OwnerReload> foundOwners;std::set<std::string> affectedPaths;
  // A failed published path remains affected even when the disk delta is
  // empty. Capture holders created since that failure before merging retries.
  for(const auto& old:previous)if(old.failed)
    affectedPaths.insert(FullResourceReload::NormalizeAssetPath(old.path));
  for(auto it=FullResourceReload::rawPrefabsByPath.begin();it!=FullResourceReload::rawPrefabsByPath.end();){
    bool affected=false;
    for(auto entry=it->second.begin();entry!=it->second.end();){
      if(!entry->proxy.Get()){entry=it->second.erase(entry);continue;}
      auto source=entry->source.Get();if(RuntimeShapes::Alive(source)&&Affected(source,candidate))affected=true;++entry;
    }
    if(affected){AddPath(it->first,PathHash(it->first));affectedPaths.insert(it->first);}
    if(it->second.empty())it=FullResourceReload::rawPrefabsByPath.erase(it);else ++it;
  }
  for(auto it=FullResourceReload::prefabProxySourcesByPath.begin();it!=FullResourceReload::prefabProxySourcesByPath.end();) {
    bool affected=false;
    for(auto source=it->second.begin();source!=it->second.end();) {
      auto root=source->Get();if(!RuntimeShapes::Alive(root)){source=it->second.erase(source);continue;}
      affected=affected||Affected(root,candidate);++source;
    }
    if(affected){auto label="prefab-proxy:"+std::to_string(it->first);AddPath(label,it->first);
      Log("[DEMAND-F10-CACHED-PREFAB] hash=%016llX activeOwnerCaptured=0 invalidateBeforeNextLoad=1",
        (unsigned long long)it->first);}
    if(it->second.empty())it=FullResourceReload::prefabProxySourcesByPath.erase(it);else ++it;
  }
  for(auto it=FullResourceReload::observedPrefabs.begin();it!=FullResourceReload::observedPrefabs.end();) {
    auto root=it->second.Get();if(!RuntimeShapes::Alive(root)){
      it=FullResourceReload::observedPrefabs.erase(it);continue;
    }
    if(!Affected(root,candidate)){++it;continue;}
    auto path=it->first;AddPath(path,PathHash(path));affectedPaths.insert(FullResourceReload::NormalizeAssetPath(path));
    Log("[DEMAND-F10-CACHED-PREFAB] path=%s rootId=%d activeOwnerCaptured=0 invalidateBeforeNextLoad=1",
      path.c_str(),FullResourceReload::Id(root));
    ++it;
  }
  for(auto it=FullResourceReload::liveParts.begin();it!=FullResourceReload::liveParts.end();) {
    auto part=it->second.part.Get();if(!part){it=FullResourceReload::liveParts.erase(it);continue;}
    try{
      auto owner=MakePartOwner(part,it->second);++it;
      auto phase=RuntimeShapes::Alive(owner.root.Get())?OwnerPhase::Queued:OwnerPhase::AwaitExisting;
      if(Affected(owner.root.Get(),candidate))affectedPaths.insert(FullResourceReload::NormalizeAssetPath(owner.path));
      foundOwners.push_back({std::move(owner),phase});
    }catch(const std::exception& error){Log("[DEMAND-F10-OWNER-GAP] kind=world-part reason=%s otherOwnersContinue=1",error.what());++it;}
  }
  for(const auto& row:FullResourceReload::liveUIModels) {
    auto root=row.first;try{
      auto owner=MakeUIOwner(root,row.second);
      if(Affected(root,candidate))affectedPaths.insert(FullResourceReload::NormalizeAssetPath(owner.path));
      foundOwners.push_back({std::move(owner),OwnerPhase::Queued});
    }catch(const std::exception& error){Log("[DEMAND-F10-OWNER-GAP] kind=ui reason=%s otherOwnersContinue=1",error.what());}
  }
  for(const auto& row:FullResourceReload::pendingUIModels)
    if(candidate.delta.changed){pendingUIPaths.insert(row.second);affectedPaths.insert(FullResourceReload::NormalizeAssetPath(row.second));}
  for(auto& item:foundOwners){
    if(!affectedPaths.count(FullResourceReload::NormalizeAssetPath(item.owner.path)))continue;
    auto& batch=AddPath(item.owner.path,OwnerPathHash(item.owner));
    batch.owners.push_back(std::move(item));
  }
  // Publication precedes owner refresh. A cache-stage failure must survive a
  // subsequent unchanged disk snapshot: that F10 still has work to retry.
  for(auto& old:previous)if(old.failed){
    auto& batch=AddPath(old.path,old.hash);
    const bool empty=batch.owners.empty()&&!batch.rawCaptured;
    if(empty){batch=std::move(old);batch.failed=false;batch.started=0;batch.lastError.clear();batch.waitLogged=false;}
    else{
      for(auto& item:old.owners)if(std::none_of(batch.owners.begin(),batch.owners.end(),[&](const auto& next){
          return next.owner.component.Get()==item.owner.component.Get();}))batch.owners.push_back(std::move(item));
      batch.rawAssets.insert(batch.rawAssets.end(),old.rawAssets.begin(),old.rawAssets.end());
    }
    // Cached sources may have appeared while the transaction was idle.
    // Revisit the pool/raw observations; retain proof of an already-unloaded
    // instantiate proxy so an absent old proxy is not required a second time.
    batch.poolDiscarded=false;batch.cacheInvalidated=false;batch.rawCaptured=false;
    batch.prefabInvalidationAttempted=false;batch.prefabRetryAt=0;batch.prefabBlockedSince=0;
    for(auto& item:batch.owners)if(item.phase==OwnerPhase::Released){
      auto root=FullResourceReload::CurrentRoot(item.owner);
      if(RuntimeShapes::Alive(root)){
        item.owner.root=Ref::Make(root,false);item.owner.oldRootId=FullResourceReload::Id(root);
        if(item.owner.directAsset)item.owner.rawAsset=FullResourceReload::PartRawAsset(item.owner.component.Get());
        item.phase=OwnerPhase::Queued;batch.poolDiscarded=false;batch.prefabInvalidated=false;
        batch.cacheInvalidated=false;batch.rawCaptured=false;batch.prefabInvalidationAttempted=false;
        batch.prefabRetryAt=0;batch.prefabBlockedSince=0;
      }
    }
    Log("[DEMAND-F10-PATH-RETRY] path=%s previousCacheFailure=1 diskDeltaRequired=0",batch.path.c_str());
  }
  for(auto& batch:reloads)for(auto& child:batch.owners){
    if(!child.owner.viewPart||child.owner.uiModel||!FullResourceReload::Inherits(
       child.owner.component.Get(),"Beyond.Gameplay.View","WeaponModelViewPart"))continue;
    for(const auto& parentBatch:reloads)for(const auto& parent:parentBatch.owners){
      if(parent.owner.viewPart||parent.owner.uiModel||!parent.owner.weapons.Get())continue;
      if(FullResourceReload::RootContains(parent.owner.root.Get(),child.owner.root.Get()))
        child.rebuildParent=parent.owner.component;
    }
  }
  for(const auto& row:FullResourceReload::liveHelperModels){
    auto helperRoot=row.second.root.Get();if(!RuntimeShapes::Alive(helperRoot)||!Affected(helperRoot,candidate))continue;
    bool covered=false;for(const auto& batch:reloads)for(const auto& item:batch.owners){auto root=item.owner.root.Get();
      covered=covered||(RuntimeShapes::Alive(root)&&FullResourceReload::RootContains(root,helperRoot));}
    if(!covered){++uncoveredHelpers;Log("[DEMAND-F10-HELPER-GAP] rootId=%d reason=affected-helper-has-no-reloadable-model-owner otherOwnersContinue=1",FullResourceReload::Id(helperRoot));}
  }
}

void Commit(std::shared_ptr<Prepared> candidate) {
  Require(DemandLoadingProbe::InstalledGenerationFilesUnchanged(*candidate->generation),
    "Format 3 Mod files changed before F10 publication");
  RuntimeControls::State controls=RuntimeControls::Prepare(candidate->generation->packages,candidate->config);
  RuntimeControls::MergeLive(controls);
  AddAffectedOwners(*candidate->generation);
  auto graph=controls.graph;RuntimeControls::Swap(controls);
  std::atomic_store(&published,graph);settings.Publish(candidate->config);
  RuntimeAttachments::CommitResourceGeneration(candidate->generation);
  auto next=generation.load()+1;
  RuntimeShapes::packageGeneration=next;RuntimeShapes::checkedGeneration=next;generation.store(next);
  committedGeneration=candidate->generation->serial;active=true;
  size_t ownerCount=0;for(const auto& batch:reloads)ownerCount+=batch.owners.size();
  Log("[DEMAND-F10-COMMIT] generation=%llu mods=%zu targets=%zu changed=%d paths=%zu owners=%zu pendingUIPaths=%zu uncoveredHelpers=%zu payloadsPreloaded=0 ownerReload=game-lifecycle generationProxyNamespace=1 sameCabContainersRetiredOnSubmit=1 oldBundleLeaseBarrier=0",
    (unsigned long long)candidate->generation->serial,candidate->generation->packages->size(),
    candidate->generation->targets.size(),int(candidate->generation->delta.changed),reloads.size(),ownerCount,pendingUIPaths.size(),uncoveredHelpers);
}

void CompleteModel(OwnerReload& item,void* root) {
  auto& owner=item.owner;
  Require(RuntimeShapes::Alive(root)&&FullResourceReload::Id(root)!=owner.oldRootId,
    "Format 3 F10 model owner did not produce a new game-owned root");
  if(!owner.viewPart) {
    FullResourceReload::ReloadConsumers(owner,root);
    FullResourceReload::Follow(owner);
  }
  RuntimeShapes::RegisterModel(root);
  Log("[DEMAND-F10-OWNER-READY] path=%s oldRootId=%d newRootId=%d gameLifecycle=1 resourcePayloadsLoadedOnDemand=1",
    owner.path.c_str(),owner.oldRootId,FullResourceReload::Id(root));
  item.phase=OwnerPhase::Done;
}

void AppendOwner(Owner owner,OwnerPhase phase) {
  auto& batch=AddPath(owner.path,OwnerPathHash(owner));
  auto component=owner.component.Get();
  auto duplicate=std::find_if(batch.owners.begin(),batch.owners.end(),[&](const auto& row){
    return row.owner.component.Get()==component&&row.owner.uiModel==owner.uiModel;
  });
  if(duplicate==batch.owners.end())batch.owners.push_back({std::move(owner),phase});
  batch.started=0;batch.poolDiscarded=false;batch.cacheInvalidated=false;
  batch.prefabInvalidated=false;batch.prefabInvalidationAttempted=false;batch.rawCaptured=false;
  batch.prefabRetryAt=0;batch.prefabBlockedSince=0;
  batch.failed=false;
}

void StartLoads(PathReload& batch) {
  for(auto& item:batch.owners)if(item.phase==OwnerPhase::Released){
    auto& owner=item.owner;item.started=GetTickCount64();item.timeoutLogged=false;
    try{
      bool waitingForChild=false;
      for(const auto& childBatch:reloads)for(const auto& child:childBatch.owners)
        if(child.rebuildParent.Get()==owner.component.Get()){
          if(childBatch.failed){
            batch.failed=true;
            Log("[DEMAND-F10-PARENT-DEFERRED] path=%s weaponPath=%s retryRetainsReleasedOwner=1",
              owner.path.c_str(),childBatch.path.c_str());
          }
          waitingForChild=waitingForChild||!childBatch.cacheInvalidated;
        }
      if(waitingForChild)continue;
      if(owner.uiModel){
        owner.uiRequest=FullResourceReload::LoadUIModel(owner.component.Get(),owner.path,owner.uiParent.Get(),nullptr,{});
        item.phase=OwnerPhase::WaitingUI;
      }else{
        item.phase=OwnerPhase::WaitingModel;FullResourceReload::LoadOwner(owner);
        if(owner.viewPart)CompleteModel(item,FullResourceReload::CurrentRoot(owner));
      }
      Log("[DEMAND-F10-OWNER-REQUEST] path=%s oldRootId=%d gameLifecycle=1 asynchronous=%d",
        owner.path.c_str(),owner.oldRootId,int(owner.uiModel||!owner.viewPart));
    }catch(const std::exception& error){
      item.phase=OwnerPhase::Failed;
      Log("[DEMAND-F10-OWNER-FAIL] path=%s reason=%s generationPublished=1 siblingOwnersContinue=1",
        owner.path.c_str(),error.what());
    }
  }
}

void ReleasePath(PathReload& batch) {
  if(!batch.started)batch.started=GetTickCount64();
  if(!batch.rawCaptured){
    auto remember=[&](void* proxy){if(proxy&&std::none_of(batch.rawAssets.begin(),batch.rawAssets.end(),
        [&](const auto& ref){return ref.Get()==proxy;}))batch.rawAssets.push_back(Ref::Make(proxy,false));};
    for(const auto& item:batch.owners)remember(item.owner.rawAsset.Get());
    auto found=FullResourceReload::rawPrefabsByPath.find(FullResourceReload::NormalizeAssetPath(batch.path));
    if(found!=FullResourceReload::rawPrefabsByPath.end())for(const auto& raw:found->second)remember(raw.proxy.Get());
    batch.rawCaptured=true;
  }
  for(auto& item:batch.owners){
    auto& owner=item.owner;
    if(item.rebuildParent.Get()&&(item.phase==OwnerPhase::Queued||item.phase==OwnerPhase::AwaitExisting)){
      // UnloadWeapons/ReloadWeapon owns this part's lifecycle. Never resurrect
      // the old part after the parent has removed it from the component.
      if(!RuntimeShapes::Alive(FullResourceReload::CurrentRoot(owner))){
        item.phase=OwnerPhase::Ended;
        Log("[DEMAND-F10-OWNER-COVERED] path=%s parentRecreatesWeapon=1 oldPartReloaded=0",owner.path.c_str());
      }
      continue;
    }
    if(item.phase==OwnerPhase::AwaitExisting){
      auto root=FullResourceReload::CurrentRoot(owner);
      if(!RuntimeShapes::Alive(root)){
        item.phase=OwnerPhase::Ended;
        Log("[DEMAND-F10-OWNER-ABSENT] path=%s reason=old-owner-ended futureRequestUsesCurrentGeneration=1",owner.path.c_str());
        continue;
      }
      owner.root=Ref::Make(root,false);owner.oldRootId=FullResourceReload::Id(root);
      if(!owner.viewPart)FullResourceReload::CaptureConsumers(owner);
      item.phase=OwnerPhase::Queued;
    }
    if(item.phase!=OwnerPhase::Queued)continue;
    auto root=FullResourceReload::CurrentRoot(owner);if(!RuntimeShapes::Alive(root)){item.phase=OwnerPhase::AwaitExisting;continue;}
    if(FullResourceReload::Id(root)!=owner.oldRootId){
      owner.root=Ref::Make(root,false);owner.oldRootId=FullResourceReload::Id(root);
      if(!owner.viewPart)FullResourceReload::CaptureConsumers(owner);
    }
    RetireOwnerBindings(owner);
    if(owner.uiModel){
      auto oldRoot=owner.uiLoadedRoot.Get();
      if(RuntimeShapes::Alive(oldRoot)&&FullResourceReload::UIOwns(owner.component.Get(),oldRoot)){
        Require(FullResourceReload::BindUIPage(owner.uiPage,nullptr),"UI page refused to release its old model");
        void* args[]={oldRoot};FullResourceReload::Call(FullResourceReload::api.uiUnload,owner.component.Get(),args);
      }
      owner.uiLoadedRoot={};
    }else{
      if(!owner.viewPart)FullResourceReload::ReleaseConsumers(owner);
      FullResourceReload::RemoveOwner(owner);
    }
    item.phase=OwnerPhase::Released;
  }
  if(std::any_of(batch.owners.begin(),batch.owners.end(),[](const auto& item){
       return item.phase==OwnerPhase::Queued||item.phase==OwnerPhase::AwaitExisting;
     }))return;
  if(!batch.poolDiscarded){
    auto world=std::find_if(batch.owners.begin(),batch.owners.end(),[](const auto& item){return !item.owner.uiModel;});
    if(world!=batch.owners.end()||batch.owners.empty()){
      Require(batch.hash,"affected prefab path hash unavailable: "+batch.path);
      auto manager=Ref::Make(FullResourceReload::ModelManager(),false);Require(manager.Get(),"game ModelManager unavailable during Format 3 F10");
      FullResourceReload::DiscardPool(manager.Get(),batch.hash);
    }
    batch.poolDiscarded=true;
  }
  if(!batch.cacheInvalidated){
    Require(batch.hash,"affected prefab path hash unavailable: "+batch.path);
    if(!batch.prefabInvalidated||FullResourceReload::prefabProxiesByPath.count(batch.hash)){
      const auto now=GetTickCount64();
      if(batch.prefabRetryAt&&now<batch.prefabRetryAt)return;
      batch.prefabRetryAt=0;
      const bool pooled=!batch.prefabInvalidated&&std::any_of(batch.owners.begin(),batch.owners.end(),[](const auto& item){
        return !item.owner.directAsset&&item.phase!=OwnerPhase::Ended;
      });
      const bool logAttempt=!batch.prefabInvalidationAttempted;batch.prefabInvalidationAttempted=true;
      try{
        FullResourceReload::InvalidatePrefabProxyCache(batch.hash,batch.path,pooled,logAttempt);
        batch.prefabInvalidated=true;batch.prefabBlockedSince=0;
      }catch(const FullResourceReload::PrefabProxyRetainedError&){
        if(!batch.prefabBlockedSince)batch.prefabBlockedSince=now;
        batch.prefabRetryAt=now+kPrefabProxyRetryIntervalMs;throw;
      }catch(...){batch.prefabRetryAt=now+kPrefabProxyRetryIntervalMs;throw;}
    }
    for(const auto& raw:batch.rawAssets)if(auto proxy=raw.Get()){
      Require(FullResourceReload::api.rawAssetUnloaded&&FullResourceReload::api.rawAssetRefCount,
        "game raw AssetProxy lifecycle API unavailable");
      auto unloaded=il2cpp_object_get_virtual_method(proxy,FullResourceReload::api.rawAssetUnloaded);
      Require(unloaded,"game raw AssetProxy unload-state implementation unavailable");
      if(!RuntimeShapes::Scalar<unsigned char>(unloaded,proxy)){
        auto refCount=il2cpp_object_get_virtual_method(proxy,FullResourceReload::api.rawAssetRefCount);
        Require(refCount,"game raw AssetProxy refcount implementation unavailable");
        auto refs=RuntimeShapes::Scalar<int>(refCount,proxy);
        throw std::runtime_error("game raw prefab cache has not unloaded: "+batch.path+" handles="+std::to_string(refs));
      }
    }
    Log("[DEMAND-F10-ASSET-CACHE] path=%s rawProxies=%zu gameHandlesReleased=1 cacheUnloaded=1",
      batch.path.c_str(),batch.rawAssets.size());
    batch.cacheInvalidated=true;
  }
}

void Start(PathReload& batch) {
  if(batch.failed)return;
  EiemDemandGenerationRoutes::StartAfterCacheInvalidation(batch,[&](PathReload& ready){StartLoads(ready);});
}

void PollUIPaths() {
  for(auto it=pendingUIPaths.begin();it!=pendingUIPaths.end();) {
    auto found=std::find_if(FullResourceReload::liveUIModels.begin(),FullResourceReload::liveUIModels.end(),
      [&](const auto& row){return FullResourceReload::SameAssetPath(row.second.path,*it);});
    if(found==FullResourceReload::liveUIModels.end()){
      const bool pending=std::any_of(FullResourceReload::pendingUIModels.begin(),FullResourceReload::pendingUIModels.end(),
        [&](const auto& row){return FullResourceReload::SameAssetPath(row.second,*it);});
      if(pending){++it;continue;}
      Log("[DEMAND-F10-UI-REQUEST-GONE] path=%s reason=request-ended futureRequestUsesCurrentGeneration=1",it->c_str());
      it=pendingUIPaths.erase(it);continue;
    }
    try{AppendOwner(MakeUIOwner(found->first,found->second),OwnerPhase::Queued);}
      catch(const std::exception& error){Log("[DEMAND-F10-UI-OWNER-GAP] path=%s reason=%s ownerRecorded=1",it->c_str(),error.what());++it;continue;}
    it=pendingUIPaths.erase(it);
  }
}

void PollOwner(OwnerReload& item) {
  auto& owner=item.owner;
  try{
    if(item.phase==OwnerPhase::WaitingModel){
      auto root=FullResourceReload::CurrentRoot(owner);
      if(RuntimeShapes::Alive(root)&&FullResourceReload::Id(root)!=owner.oldRootId)CompleteModel(item,root);
    }else if(item.phase==OwnerPhase::WaitingUI){
      auto request=owner.uiRequest;if(!request)return;
      Require(!request->cancelled,"game cancelled Format 3 F10 UI load");
      if(!request->completed)return;
      Require(request->error.empty(),request->error);FullResourceReload::FinishUIModel(owner);
      item.phase=OwnerPhase::Done;
      Log("[DEMAND-F10-OWNER-READY] path=%s ui=1 gameLifecycle=1 resourcePayloadsLoadedOnDemand=1",owner.path.c_str());
    }
    if((item.phase==OwnerPhase::WaitingModel||item.phase==OwnerPhase::WaitingUI)&&item.started&&
       GetTickCount64()-item.started>30000&&!item.timeoutLogged){
      item.timeoutLogged=true;Log("[DEMAND-F10-OWNER-WAIT] path=%s elapsedMs=%llu gameRequestStillOwnsCompletion=1",
        owner.path.c_str(),(unsigned long long)(GetTickCount64()-item.started));
    }
  }catch(const std::exception& error){
    item.phase=OwnerPhase::Failed;
    Log("[DEMAND-F10-OWNER-FAIL] path=%s reason=%s generationPublished=1 siblingOwnersContinue=1",
      owner.path.c_str(),error.what());
  }
}

void Frame() {
  std::shared_ptr<Prepared> candidate;
  if(!active){std::lock_guard<std::mutex> lock(mutex);candidate.swap(pending);}
  if(candidate)try{Commit(std::move(candidate));}
    catch(const std::exception& error){Log("[DEMAND-F10-REJECT] phase=commit reason=%s previousGenerationRetained=1",error.what());return;}
  if(!active||!RuntimeShapes::Main())return;
  PollUIPaths();
  for(auto& batch:reloads)if(!batch.failed&&!batch.cacheInvalidated)try{ReleasePath(batch);}
    catch(const std::exception& error){
      if(batch.lastError!=error.what()){
        batch.lastError=error.what();batch.waitLogged=false;
        Log("[DEMAND-F10-PATH-WAIT] path=%s phase=owner-release reason=%s otherPathsContinue=1",batch.path.c_str(),error.what());
      }
    }
  size_t ownerCount=0,settled=0,failed=0,failedPaths=0,settledPaths=0;
  const auto now=GetTickCount64();
  for(auto& batch:reloads)if(!batch.failed&&!batch.cacheInvalidated&&batch.prefabBlockedSince&&
      now-batch.prefabBlockedSince>=kPrefabProxyWaitTimeoutMs){
    batch.failed=true;
    Log("[DEMAND-F10-PATH-FAIL] path=%s elapsedMs=%llu phase=prefab-proxy-release retryOnNextF10=1 otherPathsContinue=1",
      batch.path.c_str(),(unsigned long long)(now-batch.prefabBlockedSince));
  }
  for(auto& batch:reloads)if(!batch.failed&&batch.started&&GetTickCount64()-batch.started>30000&&
      !batch.cacheInvalidated){
    batch.failed=true;
    const bool ownersReleased=std::all_of(batch.owners.begin(),batch.owners.end(),[](const auto& item){
      return item.phase==OwnerPhase::Released||item.phase==OwnerPhase::WaitingModel||item.phase==OwnerPhase::WaitingUI||
        item.phase==OwnerPhase::Done||item.phase==OwnerPhase::Ended||item.phase==OwnerPhase::Failed;
    });
    Log("[DEMAND-F10-PATH-FAIL] path=%s elapsedMs=%llu phase=owner-release oldOwnersReleased=%d cacheInvalidated=%d otherPathsContinue=1",
      batch.path.c_str(),(unsigned long long)(GetTickCount64()-batch.started),
      int(ownersReleased),int(batch.cacheInvalidated));
  }
  for(const auto& batch:reloads){
    bool pathSettled=batch.failed||batch.cacheInvalidated;
    for(const auto& item:batch.owners){++ownerCount;const bool terminal=batch.failed||item.phase==OwnerPhase::Done||item.phase==OwnerPhase::Ended||item.phase==OwnerPhase::Failed;
      if(terminal)++settled;if(item.phase==OwnerPhase::Failed)++failed;pathSettled=pathSettled&&terminal;}
    if(batch.failed)++failedPaths;
    if(pathSettled)++settledPaths;
  }
  if(pendingUIPaths.empty()&&settledPaths==reloads.size()){
    active=false;
    size_t refreshed=0,ended=0;for(const auto& batch:reloads)for(const auto& item:batch.owners){
      if(item.phase==OwnerPhase::Done)++refreshed;if(item.phase==OwnerPhase::Ended)++ended;
    }
    Log("[DEMAND-F10-SETTLED] generation=%llu paths=%zu owners=%zu refreshed=%zu ended=%zu failed=%zu failedPaths=%zu uncoveredHelpers=%zu allModResourcesLoaded=0",
      (unsigned long long)committedGeneration,reloads.size(),ownerCount,refreshed,ended,failed+failedPaths,failedPaths,uncoveredHelpers);return;
  }
  if(reloads.empty())return;
  if(cursor>=reloads.size())cursor=0;
  auto& batch=reloads[cursor++];if(cursor>=reloads.size())cursor=0;
  try{Start(batch);for(auto& item:batch.owners)PollOwner(item);}
  catch(const std::exception& error){
    if(batch.lastError!=error.what()){
      batch.lastError=error.what();batch.waitLogged=false;
      Log("[DEMAND-F10-PATH-WAIT] path=%s reason=%s otherPathsContinue=1",batch.path.c_str(),error.what());
    }
    if(!batch.waitLogged&&batch.started&&GetTickCount64()-batch.started>30000){batch.waitLogged=true;
      Log("[DEMAND-F10-PATH-UNRESOLVED] path=%s elapsedMs=%llu oldOwnersReleased=%d cacheInvalidated=%d",
        batch.path.c_str(),(unsigned long long)(GetTickCount64()-batch.started),
        int(std::all_of(batch.owners.begin(),batch.owners.end(),[](const auto& item){return item.phase==OwnerPhase::Released||item.phase==OwnerPhase::WaitingModel||item.phase==OwnerPhase::WaitingUI||item.phase==OwnerPhase::Done||item.phase==OwnerPhase::Ended||item.phase==OwnerPhase::Failed;})),int(batch.cacheInvalidated));
    }
  }
}
} // namespace DemandGenerationReload
