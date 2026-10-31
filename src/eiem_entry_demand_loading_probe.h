#pragma once
#include "eiem_target_mod_precedence.h"
// Creation-before observation, with an explicitly separate native-input
// acceptance build. The ordinary observation build never changes inputs.
namespace DemandLoadingProbe {
using namespace EiemNativeInput;
using RuntimeShapes::Ref;using RuntimeShapes::Call;
namespace Q=EiemDemandRequests;
namespace R=EiemNativeRandomAccess;
struct Api {void *lookup=nullptr,*subLookup=nullptr,*packClass=nullptr,*proxyInfo=nullptr,*assetPath=nullptr;size_t packSize=0,bundleOffset=0,assetOffset=0;} api;
struct Binding {size_t target=0;uint64_t bundle=0,asset=0;};
struct InstalledGeneration;
struct Manifest {Ref object;uint64_t serial=0;std::vector<Binding> bindings;std::unordered_set<uint64_t> bundles;std::mutex candidateMutex;std::map<uint64_t,Q::Candidate> candidates;
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  std::shared_ptr<const InstalledGeneration> catalog;
#endif
};
std::mutex mutex;
Q::Registry registry;
std::vector<EiemMeshTargetDirectory::Target> targets;
std::map<void*,std::shared_ptr<Manifest>> manifests;
std::map<uint64_t,std::weak_ptr<Manifest>> scopes;
using MeshIdentity=std::pair<std::string,int64_t>;
std::map<uint64_t,std::map<MeshIdentity,size_t>> meshes;
std::map<uint64_t,std::pair<uint64_t,std::set<MeshIdentity>>> tokenMeshes;
std::atomic<unsigned> inspectors{0};
struct Native {Ref weak;uint64_t token=0;std::chrono::steady_clock::time_point submittedAt{};std::shared_ptr<Manifest> generation;};
std::map<void*,Native> requests,bundles;
std::set<std::pair<uint64_t,std::pair<std::string,uint64_t>>> inspected;
std::set<uint64_t> vfsBound;
std::set<std::pair<uint64_t,bool>> dependencyLogged;
std::atomic<uint64_t> vfsEvents{0},matchedVfs{0};
std::atomic<bool> enabled{false};
std::atomic<uint64_t> readBytes{0},decodedBytes{0},inspectMicros{0},maxInspectMicros{0};
std::atomic<uint64_t> serial{0},fileEvents{0},unknownFiles{0},unrelatedFiles{0},candidateFiles{0},exactBindings{0},exactReferences{0},assetEvents{0},gaps{0};
std::atomic<uint64_t> candidateAssetEvents{0},candidateAssetCompletions{0};
std::atomic<uint64_t> timingEvents{0},timingDropped{0};
std::atomic<unsigned> gapLogs{0};
constexpr uint64_t timingEventLimit=1024;
void LogGap(const char* phase,const char* reason) noexcept {
  auto ordinal=++gapLogs;if(ordinal<=64)Log("[DEMAND-TIMING-GAP] ordinal=%u phase=%s reason=%s",ordinal,phase,reason);
}
double ElapsedMs(std::chrono::steady_clock::time_point begin) noexcept {
  return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
}
void LogTiming(uint64_t token,const char* phase,double elapsedMs,const char* detail) noexcept {
  auto ordinal=++timingEvents;if(ordinal>timingEventLimit){++timingDropped;return;}
  Log("[DEMAND-TIMING] event=%llu token=%llu phase=%s elapsedMs=%.3f %s",ordinal,token,phase,elapsedMs,detail);
}
thread_local uint64_t helperToken=0;
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
namespace A=EiemDemandMeshPreparation;
namespace N=EiemDemandNativeAssembly;
namespace M=EiemDemandMaterialAssembly;
using MaterialPrepare=std::function<M::Shared(const N::Author&,const std::vector<M::Use>&)>;
using PreparedPublisher=std::function<void(uint64_t,std::shared_ptr<const EiemInputSpanView::View>,bool)>;
struct PreparedInput {bool ready=false,blocked=false;std::shared_ptr<const EiemInputSpanView::View> view;};
struct NativeMeshObservation {Ref mesh;std::set<std::string> bundleInputs;};
std::map<uint64_t,std::shared_ptr<PreparedInput>> preparedInputs;
struct AuthorRuntime {
  std::shared_ptr<N::Author> author;
  AssetIndex targets;
};
using ScopeAuthors=std::map<std::string,AuthorRuntime>;
struct InstalledGeneration {
  uint64_t serial=0;
  std::map<std::string,std::shared_ptr<N::Author>> authors;
  std::vector<EiemMeshTargetDirectory::Target> targets;
  std::shared_ptr<const std::vector<Package>> packages;
};
// Immutable declarations are shared between manifest generations. Opening
// them never opens Mesh/Texture/effect payloads or old compiled consumers.
std::shared_ptr<const InstalledGeneration> installedGeneration;
std::shared_ptr<const InstalledGeneration> pendingInstalledGeneration;
uint64_t installedGenerationSerial=0;
std::map<uint64_t,ScopeAuthors> authors;
std::map<uint64_t,std::vector<BuiltMeshPart>> scopeParts;
std::map<uint64_t,std::shared_ptr<std::mutex>> scopePreparationLocks;
std::condition_variable preparedReady;
std::atomic<uint64_t> replacementSubmissions{0},authorPayloadBytes{0};
std::atomic<unsigned> nativeProofs{0};
// The key is the stable logical target. CAB/PathID and the weak native object
// below are observations for this game generation, never lookup identities.
std::map<std::tuple<uint64_t,std::string,std::string,std::string>,NativeMeshObservation> nativeMeshObservations;
std::map<std::tuple<uint64_t,std::string,std::string>,NativeMeshObservation> nativeResourceObservations;
thread_local bool submittingView=false;
std::shared_ptr<std::mutex> ScopePreparationLock(uint64_t scope){std::lock_guard<std::mutex> guard(mutex);auto& value=scopePreparationLocks[scope];if(!value)value=std::make_shared<std::mutex>();return value;}
void PublishPrepared(uint64_t token,std::shared_ptr<const EiemInputSpanView::View> view,bool blocked=false){
  {std::lock_guard<std::mutex> lock(mutex);auto found=preparedInputs.find(token);if(found==preparedInputs.end())return;
    found->second->view=std::move(view);found->second->blocked=blocked;found->second->ready=true;}
  preparedReady.notify_all();
}
void PrepareReplacement(uint64_t token,uint64_t scope,R::Reader& reader,const std::shared_ptr<R::File>& file,const std::shared_ptr<Manifest>& manifest,
    const MaterialPrepare* materialPrepare=nullptr,const PreparedPublisher* publisher=nullptr,std::unique_lock<std::mutex>* scopeTransaction=nullptr,const char** phaseOut=nullptr,
    void (*phaseObserver)(void*,const char*)=nullptr,void* phaseContext=nullptr,std::set<std::string>* usedMods=nullptr){
  auto phase=[&](const char* value){if(phaseOut)*phaseOut=value;if(phaseObserver)phaseObserver(phaseContext,value);};phase("initialize-rewrite");
  auto publish=[&](std::shared_ptr<const EiemInputSpanView::View> view){if(publisher)(*publisher)(token,std::move(view),false);else PublishPrepared(token,std::move(view));};
  phase("lock-mod-generation");
  auto scopeLock=ScopePreparationLock(scope);std::unique_lock<std::mutex> serial;
  if(scopeTransaction)Require(scopeTransaction->owns_lock(),"demand scope transaction is not locked");
  else serial=std::unique_lock<std::mutex>(*scopeLock);
  auto begin=std::chrono::steady_clock::now();auto record=registry.Get(token);std::vector<EiemNativeSpliceView::Rewrite> rewrites;size_t carriers=0,carrierParts=0,consumerMatches=0;
  ScopeAuthors staged;std::vector<BuiltMeshPart> stagedParts;{std::lock_guard<std::mutex> lock(mutex);auto old=authors.find(scope);if(old!=authors.end())staged=old->second;auto parts=scopeParts.find(scope);if(parts!=scopeParts.end())stagedParts=parts->second;}
  auto nativeName=[](const std::string& value){auto result=value;std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c){return char(std::tolower(c));});return result;};
  for(size_t mi=0;mi<reader.directory.members.size();++mi){const auto& member=reader.directory.members[mi];if(!(member.flags&4))continue;phase("read-native-member-layout");auto layout=reader.Layout(member);
    EiemNativeEditTransaction::Member transaction(reader,mi,layout);bool touched=false;
    if(record.candidate==Q::Candidate::Target){
      phase("match-target-meshes");
      std::map<std::string,AssetIndex> selectedByMod;std::map<std::string,std::set<size_t>> bindingIndices;auto hashes=reader.HashAssets(member,layout);std::vector<Asset> names;
      for(size_t bi=0;bi<manifest->bindings.size();++bi){const auto& binding=manifest->bindings[bi];if(binding.bundle!=record.bundle)continue;const auto& target=manifest->catalog->targets.at(binding.target);
        const EiemNativeInput::Asset* selected=nullptr;auto hashed=hashes.find(binding.asset);if(hashed!=hashes.end()&&hashed->second.classId==43&&nativeName(hashed->second.cab)==nativeName(member.name))selected=&hashed->second;
        if(!selected){if(names.empty())names=reader.Assets(member,layout);for(const auto& candidate:names)if(candidate.classId==43&&candidate.cab==member.name&&nativeName(candidate.path)==nativeName(target.source.path)&&candidate.asset==target.source.asset){selected=&candidate;break;}}
        if(!selected)continue;auto asset=*selected;asset.path=target.source.path;asset.asset=target.source.asset;
        if(target.skip)Log("[DEMAND-SKIP-MESH] token=%llu mod=%s target=%s#%s cab=%s pathId=%lld",token,target.mod.c_str(),target.source.path.c_str(),target.source.asset.c_str(),asset.cab.c_str(),asset.pathId);
        auto& targetsForMod=selectedByMod[target.mod];Require(targetsForMod.emplace(target.source,std::move(asset)).second,"two manifest bindings resolved one native Mesh identity");bindingIndices[target.mod].insert(binding.target);
      }
      for(auto& selected:selectedByMod){const auto& mod=selected.first;auto author=staged.find(mod);
        if(usedMods)usedMods->insert(mod);
        if(author==staged.end()){
          auto declaration=manifest->catalog->authors.find(mod);Require(declaration!=manifest->catalog->authors.end(),"native author declaration absent: "+mod);
          author=staged.emplace(mod,AuthorRuntime{}).first;author->second.author=declaration->second;
        }
        for(auto index:bindingIndices.at(mod)){const auto& target=manifest->catalog->targets.at(index);Require(author->second.author->package.targetIndex.count(target.source),"native author package lacks declared target "+target.source.path+" / "+target.source.asset);}
        for(const auto& target:selected.second){auto prior=author->second.targets.find(target.first);if(prior!=author->second.targets.end())Require(NativeReference{prior->second.cab,prior->second.pathId}==NativeReference{target.second.cab,target.second.pathId},"native target changed its current asset identity during one generation");
          author->second.targets[target.first]=target.second;
        }
      }
    }
    phase("compose-author-set");std::vector<std::shared_ptr<N::Author>> authorPackages;AssetIndex combinedTargets;
    for(const auto& row:staged)if(!row.second.targets.empty()){
      authorPackages.push_back(row.second.author);
      for(const auto& target:row.second.targets){auto prior=combinedTargets.find(target.first);
        if(prior!=combinedTargets.end())Require(NativeReference{prior->second.cab,prior->second.pathId}==NativeReference{target.second.cab,target.second.pathId},"overlapping Mods resolved one logical Mesh to different native identities");
        else combinedTargets.emplace(target.first,target.second);
      }
    }
    if(!authorPackages.empty()){
      phase("compose-authors");auto combined=N::Author::Compose(authorPackages);
      phase("stage-mesh-carrier");
      auto carrier=N::StageCarrier(transaction,reader,mi,*combined,combinedTargets);++carriers;carrierParts+=carrier.parts.size();touched|=!carrier.parts.empty();
      if(record.candidate==Q::Candidate::Target){std::set<Identity> refreshed;
        for(const auto& target:combinedTargets)if(target.second.cab==member.name&&layout.objects.count(target.second.pathId))refreshed.insert(target.first);
        stagedParts.erase(std::remove_if(stagedParts.begin(),stagedParts.end(),[&](const auto& part){return refreshed.count(part.target)!=0;}),stagedParts.end());
        for(const auto& part:carrier.parts){auto duplicate=std::find_if(stagedParts.begin(),stagedParts.end(),[&](const auto& old){return !(old.target<part.target)&&!(part.target<old.target)&&old.part==part.part&&old.cab==part.cab&&old.id==part.id;});if(duplicate==stagedParts.end())stagedParts.push_back(part);}
      }
      phase("collect-renderer-material-uses");auto uses=M::CollectUses(transaction.Layout(),member.name,*combined,combinedTargets,
        [&](int64_t id,const std::set<std::string>& fields){return transaction.Project(id,fields);});
      M::Shared materials;
      if(!uses.empty()){
        phase("prepare-shared-materials");
        Require(materialPrepare,"matched native Renderer reached input before its shared dependency group was prepared");
        materials=(*materialPrepare)(*combined,uses);Require(materials.references.size()==uses.size(),"native Renderer Material donor group is incomplete");
      }
      phase("stage-renderer-consumer");
      auto consumer=N::StageConsumer(transaction,*combined,combinedTargets,stagedParts,
        uses.empty()?nullptr:&materials.references,uses.empty()?nullptr:&materials.closures);
      for(const auto& target:combinedTargets){auto selected=combined->package.targetIndex.find(target.first);
        if(selected==combined->package.targetIndex.end()||!combined->package.rules.at(selected->second).skip)continue;
        NativeReference mesh{target.second.cab,target.second.pathId};std::vector<int64_t> renderers;
        for(const auto& row:consumer.result.consumers)if(CanonicalSerializedAssetReference(row.second.mesh)==CanonicalSerializedAssetReference(mesh))renderers.push_back(row.second.renderer);
        if(renderers.empty())continue;
        Log("[DEMAND-SKIP-RENDERERS] token=%llu target=%s#%s cab=%s pathId=%lld matches=%zu disabled=%zu",token,target.first.path.c_str(),target.first.asset.c_str(),mesh.first.c_str(),mesh.second,renderers.size(),renderers.size());
        for(auto renderer:renderers)Log("[DEMAND-SKIP-RENDERER] token=%llu target=%s#%s pfb=%s rendererPathId=%lld action=m_Enabled:0",token,target.first.path.c_str(),target.first.asset.c_str(),member.name.c_str(),renderer);
        Require(consumer.result.skipped>=renderers.size(),"handling=skip disabled Renderer count differs from target consumers");
      }
      if(consumer.result.matches&&usedMods)for(const auto& row:staged)if(!row.second.targets.empty())usedMods->insert(row.first);
      consumerMatches+=consumer.result.matches;touched|=consumer.result.matches!=0;
    }
    if(touched||transaction.ChangedObjects()||transaction.AddedObjects()){phase("commit-member-rewrite");rewrites.push_back(transaction.Commit().rewrite);}
  }
  auto commitScope=[&](){std::lock_guard<std::mutex> lock(mutex);authors[scope]=std::move(staged);scopeParts[scope]=std::move(stagedParts);};
  if(rewrites.empty()){file->VerifyStable();commitScope();publish({});return;}
  phase("encode-native-input-view");
  auto prepared=EiemNativeSpliceView::BuildRanges(reader,rewrites,32*1024*1024);file->VerifyStable();auto view=prepared.Bind([file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);});
  commitScope();
  publish(std::move(view));
    Log("[DEMAND-NATIVE-PREPARED] token=%llu carrierMods=%zu carrierParts=%zu consumers=%zu ownedLiteralBytes=%zu reusedEncodedBytes=%zu streamedAuthorBytes=%zu prepareMs=%.3f authorPayloadDecoded=0 vertexBoundsComputed=0 nativeObjectsCreatedByEff=0 generation=%llu",token,carriers,carrierParts,consumerMatches,prepared.literalBytes,prepared.reusedEncoded,prepared.streamedAuthorBytes,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count(),(unsigned long long)manifest->catalog->serial);
}
bool SubmitPrepared(uint64_t token,uint32_t crc,bool asynchronous,void*& result){
  std::shared_ptr<const EiemInputSpanView::View> view;bool blocked=false;auto waitBegin=std::chrono::steady_clock::now();
  {std::unique_lock<std::mutex> lock(mutex);auto found=preparedInputs.find(token);if(found==preparedInputs.end())return false;auto input=found->second;
    if(!input->ready&&!preparedReady.wait_for(lock,std::chrono::milliseconds(1000),[&]{return input->ready;})){LogTiming(token,"prepared-input-wait-timeout",ElapsedMs(waitBegin),"async=unknown");Log("[DEMAND-NATIVE-FALLBACK] token=%llu reason=preparation-timeout originalInputPreserved=1",token);return false;}
    blocked=input->blocked;view=input->view;}
  LogTiming(token,"prepared-input-wait",ElapsedMs(waitBegin),asynchronous?"async=1":"async=0");
  if(blocked){Log("[DEMAND-NATIVE-FALLBACK] token=%llu reason=preparation-failed originalInputPreserved=1",token);return false;}if(!view)return false;
  // Reuse the loader's retained seekable stream. Its cursor owns the view,
  // and the view callback owns the source handle beyond async completion.
  Require(!crc,"demand native acceptance caller CRC differs");struct Guard{bool previous;Guard():previous(submittingView){submittingView=true;}~Guard(){submittingView=previous;}} guard;
  LogTiming(token,"native-view-submit-enter",0.0,asynchronous?"async=1":"async=0");auto nativeBegin=std::chrono::steady_clock::now();
  result=NativeBundle::LoadView(view,0,asynchronous);LogTiming(token,"native-view-submit-return",ElapsedMs(nativeBegin),asynchronous?"async=1":"async=0");++replacementSubmissions;
  Log("[DEMAND-NATIVE-SUBMIT] token=%llu async=%d nativeResult=%p viewBytes=%zu nativeDeserializerOwnsCreation=1",token,int(asynchronous),result,view->Size());return true;
}
void VerifyAsset(uint64_t token,void* object){
  auto record=registry.Get(token);ScopeAuthors loaded;
  {std::lock_guard<std::mutex> lock(mutex);auto found=authors.find(record.scope);if(found!=authors.end())loaded=found->second;}
  if(loaded.empty()||!RuntimeShapes::Main()||nativeProofs>=128)return;
  auto klass=il2cpp_object_get_class(object);auto name=klass?il2cpp_class_get_name(klass):nullptr;if(!name)return;
  std::vector<void*> renderers;
  if(!strcmp(name,"GameObject")){bool inactive=true;void* args[]={RuntimeShapes::api.rendererType,&inactive};auto values=Ref::Make(Call(RuntimeShapes::api.modelRenderers,object,args),false);renderers=FullResourceReload::ReferenceArray(values.Get());}
  auto check=[&](void* mesh,void* renderer){if(!mesh)return;auto meshName=String(Call(RuntimeShapes::api.name,mesh));for(const auto& row:loaded)for(const auto& part:row.second.author->package.parts){auto marker=row.second.author->markers.at(part.first);if(meshName!=marker)continue;
    const auto& expected=row.second.author->package.meshes.at(part.first);
    auto vertices=RuntimeShapes::Scalar<int>(RuntimeShapes::api.vertexCount,mesh);auto shapes=RuntimeShapes::Scalar<int>(RuntimeShapes::api.shapeCount,mesh);
    auto poses=Ref::Make(Call(RuntimeShapes::api.bindPoses,mesh),false);size_t bindCount=0;Require(ArrayLength(poses.Get(),&bindCount),"demand native bindpose readback absent");
    size_t boneCount=0;if(renderer){auto bones=Ref::Make(Call(RuntimeShapes::api.bones,renderer),false);Require(ArrayLength(bones.Get(),&boneCount),"demand native Renderer bones absent");}
    Identity source;bool hasSource=false;for(const auto& target:row.second.author->package.targetIndex){const auto& rule=row.second.author->package.rules.at(target.second);
      if(std::find(rule.parts.begin(),rule.parts.end(),part.first)!=rule.parts.end()){source=target.first;hasSource=true;break;}}
    Require(hasSource,"native readback part has no source Mesh target");auto native=row.second.targets.find(source);Require(native!=row.second.targets.end(),"native readback source Mesh identity is absent");
    int sharedAcrossBundles=-1;size_t distinctBundles=0;{
      std::lock_guard<std::mutex> lock(mutex);auto key=std::make_tuple(record.scope,source.path,source.asset,part.first);auto& observation=nativeMeshObservations[key];
      auto previous=observation.mesh.Get();if(!previous){observation.mesh=Ref::Make(mesh,true);observation.bundleInputs.clear();}
      else if(std::any_of(observation.bundleInputs.begin(),observation.bundleInputs.end(),[&](const auto& input){return input!=record.name;}))sharedAcrossBundles=int(previous==mesh);
      observation.bundleInputs.insert(record.name);distinctBundles=observation.bundleInputs.size();}
    ++nativeProofs;
    Log("[DEMAND-NATIVE-READBACK] token=%llu bundle=%s logicalPath=%s logicalAsset=%s currentCab=%s currentPathId=%lld part=%s object=%p mesh=%p renderer=%p vertices=%d expectedVertices=%zu shapes=%d bindposes=%zu rendererBones=%zu expectedBones=%zu sharedAcrossBundles=%d distinctBundles=%zu",token,record.name.c_str(),source.path.c_str(),source.asset.c_str(),native->second.cab.c_str(),native->second.pathId,part.first.c_str(),object,mesh,renderer,vertices,size_t(expected.vertices),shapes,bindCount,boneCount,expected.bones.size(),sharedAcrossBundles,distinctBundles);
    Require(vertices==int(expected.vertices)&&shapes==int(expected.shapes.size())&&bindCount==expected.bones.size()&&(!renderer||boneCount==expected.bones.size()),"demand native loaded geometry/bone coverage differs");
    if(renderer){
      auto values=Ref::Make(Call(FullResourceReload::api.materials,renderer),false);auto materials=FullResourceReload::ReferenceArray(values.Get());
      if(materials.size()!=1){Log("[DEMAND-NATIVE-MATERIAL-GAP] reason=slot-count part=%s renderer=%p actualSlots=%zu expectedSlots=1",part.first.c_str(),renderer,materials.size());
        throw std::runtime_error("demand native Renderer Material slot count differs");}
      if(!RuntimeShapes::Alive(materials.front())){Log("[DEMAND-NATIVE-MATERIAL-GAP] reason=empty-slot part=%s renderer=%p actualSlots=1 slot0=null",part.first.c_str(),renderer);
        throw std::runtime_error("demand native Renderer Material slot is empty");}
      auto material=materials.front();
      auto declaration=row.second.author->GetMaterial(row.second.author->package.parts.at(part.first).material);
      auto materialName=String(Call(RuntimeShapes::api.name,material));
      if(materialName!=declaration->declaration.source.asset){Log("[DEMAND-NATIVE-MATERIAL-GAP] reason=source-identity part=%s renderer=%p actual=%s expected=%s material=%p",
          part.first.c_str(),renderer,materialName.c_str(),declaration->declaration.source.asset.c_str(),material);
        throw std::runtime_error("demand native Renderer received a different Material identity");}
      auto proof=[&](const std::string& kind,const std::string& identity,void* resource){int shared=-1;size_t consumers=0;
        {std::lock_guard<std::mutex> lock(mutex);auto& observation=nativeResourceObservations[{record.scope,kind,identity}];auto old=observation.mesh.Get();
          if(!old){observation.mesh=Ref::Make(resource,true);observation.bundleInputs.clear();}
          else if(std::any_of(observation.bundleInputs.begin(),observation.bundleInputs.end(),[&](const auto& input){return input!=record.name;}))shared=int(old==resource);
          observation.bundleInputs.insert(record.name);consumers=observation.bundleInputs.size();}
        Log("[DEMAND-SHARED-READBACK] token=%llu kind=%s identity=%s object=%p part=%s sharedAcrossBundles=%d distinctBundles=%zu",token,kind.c_str(),identity.c_str(),resource,part.first.c_str(),shared,consumers);};
      proof("Material",declaration->declaration.source.path+"#"+declaration->declaration.source.asset+"#"+declaration->declaration.hash,material);
      Log("[DEMAND-NATIVE-MATERIAL-READBACK] part=%s sourcePath=%s sourceAsset=%s authorHash=%s targetSlotCount=%zu variantBuiltFromCurrentSource=1",
        part.first.c_str(),declaration->declaration.source.path.c_str(),declaration->declaration.source.asset.c_str(),declaration->declaration.hash.c_str(),materials.size());
    }return;
  }
  };
  if(!strcmp(name,"Mesh"))check(object,nullptr);
  for(auto renderer:renderers)check(Call(RuntimeShapes::api.mesh,renderer),renderer);
}
#endif
void PruneSharedLocked();
void PruneLocked() {
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  PruneSharedLocked();
  for(auto it=scopes.begin();it!=scopes.end();)if(it->second.expired()){registry.RetireScope(it->first);it=scopes.erase(it);}else ++it;
  for(auto it=preparedInputs.begin();it!=preparedInputs.end();)if(!registry.Get(it->first).token){it->second->blocked=true;it->second->ready=true;it=preparedInputs.erase(it);preparedReady.notify_all();}else ++it;
  for(auto it=authors.begin();it!=authors.end();){auto scope=scopes.find(it->first);if(scope==scopes.end()||scope->second.expired()){
      auto retired=it->first;scopePreparationLocks.erase(retired);scopeParts.erase(retired);
      for(auto observed=nativeMeshObservations.begin();observed!=nativeMeshObservations.end();)if(std::get<0>(observed->first)==retired)observed=nativeMeshObservations.erase(observed);else ++observed;
      for(auto observed=nativeResourceObservations.begin();observed!=nativeResourceObservations.end();)if(std::get<0>(observed->first)==retired)observed=nativeResourceObservations.erase(observed);else ++observed;
      it=authors.erase(it);
    }else ++it;}
#endif
  for(auto it=tokenMeshes.begin();it!=tokenMeshes.end();) {
    if(registry.Get(it->first).token){++it;continue;}
    auto known=meshes.find(it->second.first);
    if(known!=meshes.end()){for(const auto& identity:it->second.second){auto ref=known->second.find(identity);if(ref!=known->second.end()&&!--ref->second)known->second.erase(ref);}if(known->second.empty())meshes.erase(known);}
    it=tokenMeshes.erase(it);
  }
  for(auto it=inspected.begin();it!=inspected.end();)if(!registry.Get(it->first).token)it=inspected.erase(it);else ++it;
  for(auto it=vfsBound.begin();it!=vfsBound.end();)if(!registry.Get(*it).token)it=vfsBound.erase(it);else ++it;
  for(auto it=dependencyLogged.begin();it!=dependencyLogged.end();)if(!registry.Get(it->first).token)it=dependencyLogged.erase(it);else ++it;
  auto prune=[](auto& map){for(auto it=map.begin();it!=map.end();)if(!registry.Get(it->second.token).token||it->second.weak.Get()!=it->first)it=map.erase(it);else ++it;};
  prune(requests);prune(bundles);
}
void UnsupportedInput(const char* kind) noexcept {
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  if(submittingView)return;
#endif
  if(!enabled)return;auto n=++gaps;if(n<=32)Log("[DEMAND-PROBE-GAP] phase=input boundary=%s fileCorrelationUnavailable=1 originalArgumentsPreserved=1",kind);
}
std::shared_ptr<Manifest> Current(void* object) {
  for(;;){
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  auto catalog=std::atomic_load(&installedGeneration);Require(bool(catalog),"format 3 runtime generation is absent");const auto& activeTargets=catalog->targets;
#else
  const auto& activeTargets=targets;
#endif
  {std::lock_guard<std::mutex> lock(mutex);auto old=manifests.find(object);if(old!=manifests.end()&&old->second->object.Get()==object
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
      &&old->second->catalog==catalog
#endif
      )return old->second;}
  auto state=std::make_shared<Manifest>();state->object=Ref::Make(object,true);state->serial=++serial;
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  state->catalog=catalog;
#endif
  auto begin=std::chrono::steady_clock::now();
  for(size_t i=0;i<activeTargets.size();++i){const auto& source=activeTargets[i].source;Bytes info(api.packSize);bool found=false;
    if(source.path.size()>=4&&source.path.substr(source.path.size()-4)==".fbx"){
      auto path=Ref::Make(il2cpp_string_new(source.path.c_str()),false),name=Ref::Make(il2cpp_string_new(source.asset.c_str()),false);void* args[]={path.Get(),info.data(),name.Get()};found=RuntimeShapes::Scalar<unsigned char>(api.subLookup,object,args)!=0;
    }else{auto path=FullResourceReload::Path(source.path);uint64_t hash=0;il2cpp_field_get_value(path.Get(),FullResourceReload::api.pathHashField,&hash);void* args[]={&hash,info.data()};found=RuntimeShapes::Scalar<unsigned char>(api.lookup,object,args)!=0;}
    if(!found){if(activeTargets[i].skip)Log("[DEMAND-SKIP-MANIFEST] scope=%llu mod=%s target=%s#%s resolved=0",state->serial,activeTargets[i].mod.c_str(),source.path.c_str(),source.asset.c_str());continue;}
    Binding binding;binding.target=i;std::memcpy(&binding.bundle,info.data()+api.bundleOffset,8);std::memcpy(&binding.asset,info.data()+api.assetOffset,8);state->bindings.push_back(binding);state->bundles.insert(binding.bundle);
    if(activeTargets[i].skip)Log("[DEMAND-SKIP-MANIFEST] scope=%llu mod=%s target=%s#%s resolved=1 bundle=%016llX asset=%016llX",state->serial,activeTargets[i].mod.c_str(),source.path.c_str(),source.asset.c_str(),binding.bundle,binding.asset);
  }
  {std::lock_guard<std::mutex> lock(mutex);auto old=manifests.find(object);if(old!=manifests.end()&&old->second->object.Get()==object
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
      &&old->second->catalog==catalog
#endif
      )return old->second;
    // A slower builder must not publish an older catalog over a completed F10.
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
    if(std::atomic_load(&installedGeneration)!=catalog)continue;
#endif
    for(auto it=manifests.begin();it!=manifests.end();)if(!it->second->object.Get()){registry.RetireScope(it->second->serial);scopes.erase(it->second->serial);it=manifests.erase(it);}else if(it->first==object){it=manifests.erase(it);}else ++it;
    PruneLocked();Require(manifests.size()<64,"demand manifest scope budget exhausted");
    manifests[object]=state;scopes[state->serial]=state;}
  Log("[DEMAND-PROBE-DIRECTORY] scope=%llu generation=%llu declared=%zu resolved=%zu targetBundles=%zu ms=%.3f reverseSearch=0 payloadBytes=0",state->serial,
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
      (unsigned long long)catalog->serial,
#else
      0ull,
#endif
      activeTargets.size(),state->bindings.size(),state->bundles.size(),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());return state;
  }
}
Q::Record CandidateRoute(const std::shared_ptr<Manifest>& state,uint64_t hash,const std::string& name,Q::Candidate verdict) {
  // Proxy construction also initializes thousands of catalog entries which
  // are never loaded. Only positives need persistent native file routes.
  if(verdict!=Q::Candidate::Target&&verdict!=Q::Candidate::Yes)return {};
  auto record=registry.ObserveCandidate(state->serial,hash,name,verdict);if(!record.token){++gaps;LogGap("candidate-route","registry returned no token");return {};}
  {std::lock_guard<std::mutex> lock(mutex);if(vfsBound.count(record.token))return record;}
  // Native VFS spelling previously verified against the game's GetFileInfo.
  // Build its hash through the game's own StringPathHash constructor.
  auto path=FullResourceReload::Path("Data/Bundles/Windows/"+name);uint64_t nativeHash=0;
  il2cpp_field_get_value(path.Get(),FullResourceReload::api.pathHashField,&nativeHash);
  Require(registry.BindVfs(record.token,nativeHash),"candidate VFS alias budget exhausted");
  {std::lock_guard<std::mutex> lock(mutex);vfsBound.insert(record.token);}
  Log("[DEMAND-PROBE-VFS-IDENTITY] token=%llu scope=%llu bundle=%016llX vfsHash=%016llX logical=Data/Bundles/Windows/%s nativeHashConstructor=1",record.token,record.scope,record.bundle,nativeHash,name.c_str());
  return record;
}
Q::Candidate CachedCandidate(const std::shared_ptr<Manifest>& state,uint64_t bundle){
  if(state->bundles.count(bundle))return Q::Candidate::Target;
  std::lock_guard<std::mutex> lock(state->candidateMutex);auto found=state->candidates.find(bundle);
  return found==state->candidates.end()?Q::Candidate::Unknown:found->second;
}
void CacheCandidate(const std::shared_ptr<Manifest>& state,uint64_t bundle,Q::Candidate candidate){
  if(candidate==Q::Candidate::Unknown)return;std::lock_guard<std::mutex> lock(state->candidateMutex);
  auto& old=state->candidates[bundle];if(old!=Q::Candidate::Target)old=candidate;
}
std::atomic<unsigned> prefabCandidateObservations{0};
std::atomic<unsigned> assetTreeRouteObservations{0};
void PrefabAssetRequest(void* manager,void* asset) noexcept {
  if(!enabled||!manager||!asset||!api.proxyInfo||!api.assetPath)return;
  try{
    auto boxed=Ref::Make(Call(api.proxyInfo,asset),false);
    Require(boxed.Get()&&il2cpp_object_get_class(boxed.Get())==api.packClass,"Prefab asset request returned a different AssetInfoPack");
    auto info=il2cpp_object_unbox(boxed.Get());auto path=Q::Name(String(Call(api.assetPath,info)));
    uint64_t bundle=0;std::memcpy(&bundle,(char*)info+api.bundleOffset,sizeof(bundle));
    auto observed=++assetTreeRouteObservations;
    if(observed<=64)Log("[DEMAND-ASSET-TREE-IDENTITY] ordinal=%u path=%s bundle=%016llX",observed,path.c_str(),(unsigned long long)bundle);
    if(path.size()<7||path.compare(path.size()-7,7,".prefab"))return;
    auto manifest=Call(StartupDependencies::api.managerManifest,manager);if(!manifest)return;
    auto state=Current(manifest);if(!bundle)return;
    auto verdict=CachedCandidate(state,bundle);const bool cached=verdict!=Q::Candidate::Unknown;
    size_t dependencyCount=0;bool complete=false,dependencyTarget=false;
    if(verdict==Q::Candidate::Unknown){
      auto list=StartupDependencies::NewListForManifest(manifest);complete=StartupDependencies::originalAll(manifest,bundle,list.Get(),StartupDependencies::api.allInfo);
      auto dependencies=StartupDependencies::ReadList(list.Get());dependencyCount=dependencies.size();dependencyTarget=EiemMeshTargetDirectory::Candidate(bundle,dependencies,state->bundles);
      verdict=state->bundles.count(bundle)?Q::Candidate::Target:
        dependencyTarget?Q::Candidate::Yes:
        complete?Q::Candidate::No:Q::Candidate::Unknown;
      CacheCandidate(state,bundle,verdict);
    }
    auto routeOrdinal=++prefabCandidateObservations;
    if(routeOrdinal<=256)Log("[DEMAND-PREFAB-ROUTE-DECISION] ordinal=%u path=%s bundle=%016llX cached=%d selfTarget=%d dependencyTarget=%d dependencyCount=%zu complete=%d verdict=%d",
      routeOrdinal,path.c_str(),(unsigned long long)bundle,int(cached),int(state->bundles.count(bundle)!=0),int(dependencyTarget),dependencyCount,int(complete),int(verdict));
    if(verdict!=Q::Candidate::Target&&verdict!=Q::Candidate::Yes)return;
    void* args[]={&bundle};auto name=String(Call(StartupDependencies::api.bundleName,manifest,args));
    auto record=CandidateRoute(state,bundle,name,verdict);
    if(routeOrdinal<=512)
      Log("[DEMAND-PREFAB-CANDIDATE] path=%s bundle=%016llX name=%s dependencyCount=%zu verdict=%s token=%llu beforeAssetTree=1",
        path.c_str(),bundle,name.c_str(),dependencyCount,verdict==Q::Candidate::Target?"target":"dependency",
        (unsigned long long)record.token);
  }catch(const std::exception& e){++gaps;LogGap("prefab-asset-route",e.what());}
   catch(...){++gaps;LogGap("prefab-asset-route","unknown");}
}
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
#include "eiem_entry_demand_shared_inputs.h"
#endif
uint64_t BeginVfs(uint64_t hash) noexcept {
  auto previous=helperToken;if(!enabled)return previous;++vfsEvents;
  try{auto record=registry.Vfs(hash);helperToken=record.token;if(record.token){auto n=++matchedVfs;if(n<=128)Log("[DEMAND-PROBE-VFS-BEGIN] token=%llu hash=%016llX beforePhysicalTranslation=1",record.token,hash);}else if(vfsEvents<=16)Log("[DEMAND-PROBE-VFS-UNRELATED] hash=%016llX candidateRoute=0 authorPayloadBytes=0",hash);}
  catch(const std::exception& e){helperToken=0;++gaps;LogGap("vfs-route",e.what());}catch(...){helperToken=0;++gaps;LogGap("vfs-route","unknown");}return previous;
}
void BundleRequest(void* manifest,uint64_t hash,const char* stage) noexcept {
  if(!enabled)return;try{auto state=Current(manifest);if(!state->bundles.count(hash))return;CacheCandidate(state,hash,Q::Candidate::Target);void* args[]={&hash};auto name=String(Call(StartupDependencies::api.bundleName,manifest,args));
    auto record=CandidateRoute(state,hash,name,state->bundles.count(hash)?Q::Candidate::Target:Q::Candidate::Unknown);
    if(record.token)Log("[DEMAND-PROBE-REQUEST] token=%llu scope=%llu bundle=%016llX stage=%s name=%s target=%d beforeNativeInput=1",record.token,record.scope,hash,stage,name.c_str(),int(record.candidate==Q::Candidate::Target));
  }catch(const std::exception& e){++gaps;Log("[DEMAND-PROBE-GAP] phase=request reason=%s",e.what());}
}
void Dependencies(void* manifest,uint64_t hash,void* list,bool complete,bool direct=false) noexcept {
  if(!enabled)return;try{auto state=Current(manifest);auto dependencies=StartupDependencies::ReadList(list);
    auto verdict=state->bundles.count(hash)?Q::Candidate::Target:EiemMeshTargetDirectory::Candidate(hash,dependencies,state->bundles)?Q::Candidate::Yes:complete?Q::Candidate::No:Q::Candidate::Unknown;
    if(verdict==Q::Candidate::Target||verdict==Q::Candidate::Yes||(!direct&&complete))CacheCandidate(state,hash,verdict);
    if(verdict!=Q::Candidate::Yes&&verdict!=Q::Candidate::Target)return;
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
    if(sharedRequestDepth)PrepareDependencyGroup(state,hash,list);
#endif
    void* args[]={&hash};auto name=String(Call(StartupDependencies::api.bundleName,manifest,args));
    auto record=CandidateRoute(state,hash,name,verdict);bool report=false;
    if(record.token){std::lock_guard<std::mutex> lock(mutex);report=dependencyLogged.insert({record.token,complete}).second;}
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
    if(report)Log("[DEMAND-PROBE-DEPENDENCIES] token=%llu bundle=%016llX complete=%d dependencies=%zu candidate=%d requestDepth=%u beforeNativeInput=1",record.token,hash,int(complete),dependencies.size(),int(verdict),sharedRequestDepth);
#else
    if(report)Log("[DEMAND-PROBE-DEPENDENCIES] token=%llu bundle=%016llX complete=%d dependencies=%zu candidate=%d beforeNativeInput=1",record.token,hash,int(complete),dependencies.size(),int(verdict));
#endif
  }catch(const std::exception& e){++gaps;Log("[DEMAND-PROBE-GAP] phase=dependencies reason=%s",e.what());}
}
void Retire(void* manifest,uint64_t hash) noexcept {
  try{
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
    ObserveSharedRetire(manifest,hash);
#endif
    std::lock_guard<std::mutex> lock(mutex);auto found=manifests.find(manifest);
    if(found!=manifests.end()){registry.Retire(found->second->serial,hash);PruneLocked();}
  }catch(const std::exception& e){++gaps;LogGap("manifest-retire",e.what());}catch(...){++gaps;LogGap("manifest-retire","unknown");}
}
uint64_t BeginHelper(void* dir,void* name) noexcept {
  auto previous=helperToken;if(!enabled)return previous;
  try{auto label=String(name),directory=String(dir);auto selected=registry.Resolve({label,(std::filesystem::u8path(directory)/std::filesystem::u8path(label)).generic_u8string()});helperToken=selected.token?selected.token:previous;
    if(!helperToken&&unknownFiles.load()<16)Log("[DEMAND-PROBE-HELPER-UNKNOWN] dir=%s name=%s identityNotGuessed=1",directory.c_str(),label.c_str());
  }catch(const std::exception& e){helperToken=0;++gaps;LogGap("helper-route",e.what());}catch(...){helperToken=0;++gaps;LogGap("helper-route","unknown");}return previous;
}
void EndHelper(uint64_t previous) noexcept {helperToken=previous;}
uint64_t BeforeFile(void* path,uint64_t offset,bool asynchronous) noexcept {
  if(!enabled)return 0;
  uint64_t selectedToken=0;std::pair<uint64_t,std::pair<std::string,uint64_t>> inspection;bool started=false;
  try{auto physical=String(path);auto record=helperToken?registry.BindFile(helperToken,physical,offset):registry.File(physical,offset);++fileEvents;selectedToken=record.token;
    if(!record.token){auto n=++unknownFiles;if(n<=16)Log("[DEMAND-PROBE-INPUT-UNKNOWN] path=%s offset=%llu async=%d payloadBytesRead=0",physical.c_str(),offset,int(asynchronous));return 0;}
    if(record.candidate==Q::Candidate::No){++unrelatedFiles;return record.token;}
    if(record.candidate==Q::Candidate::Unknown)return record.token;
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
    // Early dependency preparation owns this transaction, including failure.
    // A late file callback must never replace it with a mesh-only fallback.
    {std::lock_guard<std::mutex> lock(mutex);if(preparedInputs.count(record.token))return record.token;}
#endif
    struct Permit{bool held;Permit():held(++inspectors<=2){if(!held)--inspectors;}~Permit(){if(held)--inspectors;}} permit;
    if(!permit.held){++gaps;Log("[DEMAND-NATIVE-FALLBACK] token=%llu reason=inspection-budget originalInputPreserved=1",record.token);return record.token;}
    std::shared_ptr<Manifest> state;
    {std::lock_guard<std::mutex> lock(mutex);auto found=scopes.find(record.scope);if(found!=scopes.end())state=found->second.lock();
      inspection={record.token,{Q::Name(physical),offset}};
      if(!state||!inspected.insert(inspection).second)return record.token;started=true;
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
      preparedInputs.emplace(record.token,std::make_shared<PreparedInput>());
#endif
    }
    ++candidateFiles;auto begin=std::chrono::steady_clock::now();auto sourceFile=std::make_shared<R::File>(std::filesystem::u8path(physical),offset);R::Limits limits;limits.blockBytes=8*1024*1024;limits.metadataBytes=8*1024*1024;limits.fieldBytes=4*1024*1024;limits.cacheBytes=1024*1024;limits.readBytes=8*1024*1024;
    R::Reader reader(sourceFile->Size(),[sourceFile](size_t at,unsigned char* out,size_t n){sourceFile->ReadAt(at,out,n);},limits);
    size_t bindings=0,refs=0,hits=0;std::vector<std::pair<std::string,int64_t>> references;std::set<std::pair<std::string,int64_t>> bound;
    for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;auto layout=reader.Layout(member);
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
      if(record.candidate==Q::Candidate::Target){auto names=reader.Assets(member,layout);auto hashes=reader.HashAssets(member,layout);
        for(const auto& binding:state->bindings){if(binding.bundle!=record.bundle)continue;const auto& target=state->catalog->targets[binding.target];
          const EiemNativeInput::Asset* selected=nullptr;auto native=hashes.find(binding.asset);if(native!=hashes.end()&&native->second.classId==43)selected=&native->second;
          if(!selected)for(const auto& asset:names)if(asset.classId==43&&asset.path==Q::Name(target.source.path)&&asset.asset==target.source.asset){selected=&asset;break;}
          if(selected){bound.insert({Q::Name(selected->cab),selected->pathId});++bindings;Log("[DEMAND-PROBE-MESH-IDENTITY] token=%llu logical=%s asset=%s nativeKey=%016llX cab=%s pathId=%lld beforeDeserialize=1",record.token,target.source.path.c_str(),target.source.asset.c_str(),binding.asset,selected->cab.c_str(),selected->pathId);}
        }
      }
#endif
      for(auto id:layout.order){const auto& type=layout.types.at(layout.objects.at(id).type);if(type.classId!=137&&type.classId!=33)continue;
        auto fields=reader.Fields(member,layout,id,{"m_Mesh"});const auto& p=fields.at("m_Mesh");auto index=Integer(Child(type,p,"m_FileID"));auto meshId=int64_t(Integer(Child(type,p,"m_PathID")));Require(index<=layout.externals.size(),"candidate Mesh reference external invalid");
        if(meshId)references.push_back({Q::Name(index?layout.externals[size_t(index)-1]:member.name),meshId});++refs;
      }
    }
    sourceFile->VerifyStable();
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
    PrepareReplacement(record.token,record.scope,reader,sourceFile,state);
#endif
    {std::lock_guard<std::mutex> lock(mutex);if(!registry.Get(record.token).token){inspected.erase(inspection);return 0;}
      auto& known=meshes[record.scope];auto& owned=tokenMeshes[record.token];owned.first=record.scope;
      for(const auto& identity:bound)if(owned.second.insert(identity).second)++known[identity];
      for(const auto& ref:references)hits+=known.count(ref);}
    exactBindings+=bindings;exactReferences+=hits;
    readBytes+=reader.stats.encodedBytes;decodedBytes+=reader.stats.inflatedBytes;
    auto micros=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count());inspectMicros+=micros;
    auto maximum=maxInspectMicros.load();while(maximum<micros&&!maxInspectMicros.compare_exchange_weak(maximum,micros)){}
    Log("[DEMAND-PROBE-INPUT] token=%llu bundle=%016llX path=%s offset=%llu async=%d inputBytes=%zu readBytes=%zu decodedBytes=%zu decodedBlocks=%zu totalBlocks=%zu boundMeshes=%zu rendererRefs=%zu exactRefs=%zu inspectMs=%.3f beforeNativeBundleCall=1",record.token,record.bundle,physical.c_str(),offset,int(asynchronous),reader.header.bytes,reader.stats.encodedBytes,reader.stats.inflatedBytes,reader.stats.inflatedBlocks,reader.directory.blocks.size(),bindings,refs,hits,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());return record.token;
  }catch(const std::exception& e){++gaps;Log("[DEMAND-PROBE-GAP] phase=before-input reason=%s",e.what());
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
    if(started)PublishPrepared(selectedToken,{},true);
#else
    if(started){std::lock_guard<std::mutex> lock(mutex);inspected.erase(inspection);}
#endif
    return selectedToken;}
}
void Submitted(uint64_t token,void* result,bool asynchronous) noexcept {
  if(!enabled||!token||!result)return;
  try{auto record=registry.Get(token);if(record.candidate!=Q::Candidate::Yes&&record.candidate!=Q::Candidate::Target)return;
    auto weak=Ref::Make(result,true);std::lock_guard<std::mutex> lock(mutex);auto& map=asynchronous?requests:bundles;
    for(auto it=map.begin();it!=map.end();)if(!it->second.weak.Get())it=map.erase(it);else ++it;
    auto scope=scopes.find(record.scope);auto lease=scope==scopes.end()?nullptr:scope->second.lock();
    if(map.size()<8192)map[result]={std::move(weak),token,std::chrono::steady_clock::now(),std::move(lease)};else {++gaps;LogGap("bundle-submit-track","request map capacity exceeded");}
  }catch(const std::exception& e){++gaps;LogGap("bundle-submit-track",e.what());}catch(...){++gaps;LogGap("bundle-submit-track","unknown");}
}
bool SubmitCurrentHelper(uint32_t crc,bool asynchronous,void*& result) noexcept {
  result=nullptr;
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  const auto token=helperToken;
  if(!enabled||!token)return false;
  try{
    if(!SubmitPrepared(token,crc,asynchronous,result)||!result){result=nullptr;return false;}
    Submitted(token,result,asynchronous);
    Log("[DEMAND-NATIVE-HELPER-SUBMIT] token=%llu async=%d nativeResult=%p gameHelperOwnsRequest=1",token,int(asynchronous),result);
    return true;
  }catch(const std::exception& error){
    result=nullptr;++gaps;
    Log("[DEMAND-NATIVE-FALLBACK] token=%llu reason=helper-submit:%s originalInputPreserved=1",token,error.what());
  }catch(...){
    result=nullptr;++gaps;
    Log("[DEMAND-NATIVE-FALLBACK] token=%llu reason=helper-submit-unknown originalInputPreserved=1",token);
  }
#else
  (void)crc;(void)asynchronous;
#endif
  return false;
}
void Completed(void* request,void* bundle) noexcept {
  if(!enabled||!bundle)return;uint64_t token=0;auto started=std::chrono::steady_clock::time_point{};{std::lock_guard<std::mutex> lock(mutex);auto found=requests.find(request);if(found!=requests.end()&&found->second.weak.Get()==request){token=found->second.token;started=found->second.submittedAt;requests.erase(found);}}
  if(token){if(started.time_since_epoch().count())LogTiming(token,"bundle-async-complete",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),"async=1");Submitted(token,bundle,false);Log("[DEMAND-PROBE-BUNDLE-COMPLETE] token=%llu bundle=%p",token,bundle);}
}
uint64_t AssetBeginning(void* bundle,uint64_t hash,void* label,const char* kind) noexcept {
  if(!enabled)return 0;try{++assetEvents;uint64_t token=0;{std::lock_guard<std::mutex> lock(mutex);auto found=bundles.find(bundle);if(found!=bundles.end()&&found->second.weak.Get()==bundle)token=found->second.token;}
    auto record=registry.Get(token);if(!record.token){if(assetEvents<=16)Log("[DEMAND-PROBE-ASSET-UNKNOWN] boundary=%s bundle=%p hash=%016llX",kind,bundle,hash);return 0;}
    if((record.candidate==Q::Candidate::Yes||record.candidate==Q::Candidate::Target)&&++candidateAssetEvents<=256)Log("[DEMAND-PROBE-ASSET-BEGIN] token=%llu boundary=%s hash=%016llX name=%s beforeNativeAssetCall=1 bundleAlreadyLoaded=1",token,kind,hash,label?String(label).c_str():"<hash>");
    return record.token;
  }catch(const std::exception& e){++gaps;LogGap("asset-begin",e.what());}catch(...){++gaps;LogGap("asset-begin","unknown");}return 0;
}
uint64_t AssetToken(void* bundle) {
  std::lock_guard<std::mutex> lock(mutex);auto found=bundles.find(bundle);return found!=bundles.end()&&found->second.weak.Get()==bundle?found->second.token:0;
}
void AssetSubmitted(void* bundle,void* result,bool asynchronous) noexcept {
  if(!enabled||!result)return;try{auto token=AssetToken(bundle);if(!token)return;
    if(asynchronous){auto weak=Ref::Make(result,true);std::lock_guard<std::mutex> lock(mutex);auto record=registry.Get(token);auto scope=scopes.find(record.scope);
      auto lease=scope==scopes.end()?nullptr:scope->second.lock();
      if(requests.size()<8192)requests[result]={std::move(weak),token,std::chrono::steady_clock::now(),std::move(lease)};else {++gaps;LogGap("asset-submit-track","request map capacity exceeded");}}
    else {if(++candidateAssetCompletions<=256)Log("[DEMAND-PROBE-ASSET-COMPLETE] token=%llu async=0 object=%p",token,result);
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
      VerifyAsset(token,result);
#endif
    }
  }catch(const std::exception& e){++gaps;LogGap("asset-submit-track",e.what());}catch(...){++gaps;LogGap("asset-submit-track","unknown");}
}
void AssetCompleted(void* request,void* object) noexcept {
  if(!enabled||!object)return;try{uint64_t token=0;auto started=std::chrono::steady_clock::time_point{};{std::lock_guard<std::mutex> lock(mutex);auto found=requests.find(request);
    if(found!=requests.end()&&found->second.weak.Get()==request){token=found->second.token;started=found->second.submittedAt;if(++candidateAssetCompletions<=256)Log("[DEMAND-PROBE-ASSET-COMPLETE] token=%llu async=1 request=%p object=%p",token,request,object);requests.erase(found);}}
    if(token&&started.time_since_epoch().count())LogTiming(token,"asset-async-complete",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),"async=1");
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
    if(token)VerifyAsset(token,object);
#endif
  }catch(const std::exception& e){++gaps;LogGap("asset-complete",e.what());}catch(...){++gaps;LogGap("asset-complete","unknown");}
}
using HashFn=void*(*)(void*,int64_t,void*);using StringFn=void*(*)(void*,void*,void*);using Getter=void*(*)(void*);
HashFn hashAsset=nullptr,hashAsync=nullptr;StringFn stringAsset=nullptr,stringAsync=nullptr;Getter bundleGetter=nullptr,assetGetter=nullptr;
void* HashAsset(void* self,int64_t hash,void* type){auto token=AssetBeginning(self,uint64_t(hash),nullptr,"hash-sync");auto begin=std::chrono::steady_clock::now();auto result=hashAsset(self,hash,type);if(token)LogTiming(token,"asset-call-return",ElapsedMs(begin),"async=0 lookup=hash");AssetSubmitted(self,result,false);return result;}
void* HashAsync(void* self,int64_t hash,void* type){auto token=AssetBeginning(self,uint64_t(hash),nullptr,"hash-async");auto begin=std::chrono::steady_clock::now();auto result=hashAsync(self,hash,type);if(token)LogTiming(token,"asset-call-return",ElapsedMs(begin),"async=1 lookup=hash");AssetSubmitted(self,result,true);return result;}
void* StringAsset(void* self,void* name,void* type){auto token=AssetBeginning(self,0,name,"string-sync");auto begin=std::chrono::steady_clock::now();auto result=stringAsset(self,name,type);if(token)LogTiming(token,"asset-call-return",ElapsedMs(begin),"async=0 lookup=string");AssetSubmitted(self,result,false);return result;}
void* StringAsync(void* self,void* name,void* type){auto token=AssetBeginning(self,0,name,"string-async");auto begin=std::chrono::steady_clock::now();auto result=stringAsync(self,name,type);if(token)LogTiming(token,"asset-call-return",ElapsedMs(begin),"async=1 lookup=string");AssetSubmitted(self,result,true);return result;}
void* BundleGetter(void* self){auto bundle=bundleGetter(self);Completed(self,bundle);return bundle;}
void* AssetGetter(void* self){auto result=assetGetter(self);AssetCompleted(self,result);return result;}
using NativeFileFn=void*(*)(void*,uint32_t,uint64_t);
NativeFileFn nativeFile=nullptr,nativeFileAsync=nullptr;
void* FileResult(void* path,uint32_t crc,uint64_t offset,bool asynchronous){auto routeBegin=std::chrono::steady_clock::now();auto token=BeforeFile(path,offset,asynchronous);void* result=nullptr;bool handled=false;
  auto record=token?registry.Get(token):Q::Record{};bool timed=record.token&&(record.candidate==Q::Candidate::Yes||record.candidate==Q::Candidate::Target);
  if(timed)LogTiming(token,"file-route-ready",ElapsedMs(routeBegin),asynchronous?"async=1":"async=0");
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  try{handled=SubmitPrepared(token,crc,asynchronous,result);if(handled&&!result){handled=false;Log("[DEMAND-NATIVE-FALLBACK] token=%llu reason=replacement-loader-returned-null originalInputPreserved=1",token);}}
  catch(const std::exception& e){handled=false;result=nullptr;++gaps;Log("[DEMAND-NATIVE-FALLBACK] token=%llu reason=%s originalInputPreserved=1",token,e.what());}
#endif
  if(!handled){if(timed)LogTiming(token,"native-file-call-enter",0.0,asynchronous?"async=1 passthrough=1":"async=0 passthrough=1");auto nativeBegin=std::chrono::steady_clock::now();result=(asynchronous?nativeFileAsync:nativeFile)(path,crc,offset);if(timed)LogTiming(token,"native-file-call-return",ElapsedMs(nativeBegin),asynchronous?"async=1 passthrough=1":"async=0 passthrough=1");}
  if(timed)LogTiming(token,"file-route-return",ElapsedMs(routeBegin),handled?"replacement=1":"replacement=0");Submitted(token,result,asynchronous);return result;
}
void* NativeFile(void* path,uint32_t crc,uint64_t offset){return FileResult(path,crc,offset,false);}
void* NativeFileAsync(void* path,uint32_t crc,uint64_t offset){return FileResult(path,crc,offset,true);}
void Report(int frame) noexcept {
  if(!enabled||frame%300)return;
  try{std::lock_guard<std::mutex> lock(mutex);PruneLocked();}catch(const std::exception& e){++gaps;LogGap("periodic-prune",e.what());}catch(...){++gaps;LogGap("periodic-prune","unknown");}
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  ReportShared(frame);
  Log("[DEMAND-NATIVE-STATS] frame=%d submissions=%llu authorPayloadBytes=%llu gaps=%llu nativeObjectsCreatedByEff=0",frame,replacementSubmissions.load(),authorPayloadBytes.load(),gaps.load());
#endif
  Log("[DEMAND-TIMING-STATS] frame=%d emitted=%llu dropped=%llu gapLogs=%u gapCount=%llu eventLimit=%llu",frame,timingEvents.load()-timingDropped.load(),timingDropped.load(),gapLogs.load(),gaps.load(),timingEventLimit);
  Log("[DEMAND-PROBE-STATS] frame=%d fileEvents=%llu unassociated=%llu unrelated=%llu inspected=%llu meshBindings=%llu exactRendererRefs=%llu assetEvents=%llu gaps=%llu liveRoutes=%zu vfsEvents=%llu matchedVfs=%llu readBytes=%llu decodedBytes=%llu inspectMs=%.3f maxInspectMs=%.3f nativeObjectsCreatedByEff=0",frame,fileEvents.load(),unknownFiles.load(),unrelatedFiles.load(),candidateFiles.load(),exactBindings.load(),exactReferences.load(),assetEvents.load(),gaps.load(),registry.Size(),vfsEvents.load(),matchedVfs.load(),readBytes.load(),decodedBytes.load(),inspectMicros.load()/1000.0,maxInspectMicros.load()/1000.0);
}
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
std::shared_ptr<const InstalledGeneration> BuildInstalledPackages(){
  auto begin=std::chrono::steady_clock::now();std::map<std::string,std::shared_ptr<N::Author>> next;auto graph=std::make_shared<std::vector<Package>>();std::vector<EiemMeshTargetDirectory::Target> selected;
  for(const auto& root:DiscoverModRoots()){
    // During migration the original Mod remains the enabled/install identity;
    // only its small INI header is consulted, never its old resource payloads.
    auto sections=ParseIni(EiemDirectory::Read(root/"mod.ini"));std::string mod;
    for(const auto& section:sections)if(section.id=="Mod")mod=section.values.at("id");Require(Identifier(mod),"native Mod identity absent");
    auto catalog=std::make_shared<EiemDemandAuthorResources::Catalog>(root);
    Require(catalog->Directory().mod==mod,"native author directory belongs to a different Mod");auto author=std::make_shared<N::Author>(std::move(catalog));author->package.root=root;
    Require(next.emplace(mod,author).second,"duplicate native Mod id");graph->push_back(author->package);
    for(const auto& rule:author->package.rules)for(const auto& target:rule.second.targets){EiemMeshTargetDirectory::Target row;row.mod=mod;row.root=root;row.rule=rule.first;row.source=target.second;row.lod=target.first;row.partIds=rule.second.parts;row.parts=row.partIds.size();row.skip=rule.second.skip;selected.push_back(std::move(row));}
  }
  auto result=std::make_shared<InstalledGeneration>();result->serial=++installedGenerationSerial;result->authors=std::move(next);result->targets=std::move(selected);result->packages=graph;
  Log("[DEMAND-AUTHOR-DECLARATIONS] generation=%llu mods=%zu targets=%zu ms=%.3f meshTextureEffectPayloadBytes=0 compiledInputsRead=0 consumerPfbEnumeration=0",
    (unsigned long long)result->serial,graph->size(),result->targets.size(),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());return result;
}
std::shared_ptr<const std::vector<Package>> PrepareInstalledPackages(){pendingInstalledGeneration=BuildInstalledPackages();return pendingInstalledGeneration->packages;}
void PublishPreparedInstalledPackages(){Require(bool(pendingInstalledGeneration),"format 3 reload candidate is absent");std::atomic_store(&installedGeneration,pendingInstalledGeneration);pendingInstalledGeneration.reset();}
void DiscardPreparedInstalledPackages()noexcept{pendingInstalledGeneration.reset();}
std::shared_ptr<const std::vector<Package>> ReadInstalledPackages(){auto packages=PrepareInstalledPackages();PublishPreparedInstalledPackages();return packages;}
#endif
void ReadInstalledTargets(){auto begin=std::chrono::steady_clock::now();size_t mods=0;
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  auto current=std::atomic_load(&installedGeneration);if(!current){ReadInstalledPackages();current=std::atomic_load(&installedGeneration);}Require(bool(current),"format 3 runtime generation is absent");mods=current->packages->size();
  const auto& selectedTargets=current->targets;
#else
  targets.clear();for(const auto& root:DiscoverModRoots()){auto rows=EiemMeshTargetDirectory::Read(root);targets.insert(targets.end(),rows.begin(),rows.end());++mods;}
  const auto& selectedTargets=targets;
#endif
  std::vector<EiemTargetModPrecedence::Claim> claims;claims.reserve(selectedTargets.size());for(const auto& target:selectedTargets)claims.push_back({{target.source.path,target.source.asset},target.mod});
  auto selection=EiemTargetModPrecedence::Resolve(claims);
  Log("[DEMAND-PROBE-DECLARATIONS] mods=%zu targets=%zu overlappingTargets=%zu overlapPolicy=combine-parts-mod-id-last-fields ms=%.3f authorPayloadBytes=0 compiledInputsRead=0 consumerPfbEnumeration=0",mods,selectedTargets.size(),selection.overlappingTargets,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}
void Install(void** assemblies,size_t count){try{
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  Require(NativeBundle::enabled&&NativeBundle::viewCallbacksReady,"demand native acceptance stream lifecycle bindings absent");
#endif
  Require(StartupDependencies::enabled&&FullResourceReload::enabled,"creation-before probe requires directory/loader metadata");auto klass=FindClass("Beyond.ManifestBinary","RuntimeManifestBinary",assemblies,count);void* iterator=nullptr;
  while(auto method=il2cpp_class_get_methods(klass,&iterator)){if(strcmp(il2cpp_method_get_name(method),"TryGetAssetInfoFromPath")||il2cpp_method_get_param_count(method)!=2)continue;
    auto first=StartupDependencies::TypeName(il2cpp_method_get_param(method,0));if(first!="System.UInt64"&&first!="System.Int64")continue;
    Require(!api.lookup&&il2cpp_type_is_byref(il2cpp_method_get_param(method,1)),"demand directory lookup ABI differs");api.lookup=method;api.packClass=il2cpp_class_from_type(il2cpp_method_get_param(method,1));}
  Require(api.lookup&&api.packClass,"demand directory lookup absent");api.packSize=StartupDependencies::ValueSize(api.packClass);
  auto field=StartupDependencies::Field(api.packClass,"bundleData");auto bundleClass=il2cpp_class_from_type(il2cpp_field_get_type(field));
  api.bundleOffset=StartupDependencies::ValueOffset(api.packClass,"bundleData",StartupDependencies::ValueSize(bundleClass))+StartupDependencies::ValueOffset(bundleClass,"hashName",8);
  field=StartupDependencies::Field(api.packClass,"assetData");auto assetClass=il2cpp_class_from_type(il2cpp_field_get_type(field));
  api.assetOffset=StartupDependencies::ValueOffset(api.packClass,"assetData",StartupDependencies::ValueSize(assetClass))+StartupDependencies::ValueOffset(assetClass,"pathHashHead",8);
  Require(api.bundleOffset+8<=api.packSize&&api.assetOffset+8<=api.packSize,"demand directory pack range differs");
  auto pack=StartupDependencies::TypeName(il2cpp_class_get_type(api.packClass))+"&";
  api.subLookup=FullResourceReload::Method(klass,"TryGetAssetInfoFromPath",{"System.String",pack.c_str(),"System.String"},"System.Boolean",false);
  auto assetProxy=FindClass("","AssetProxy",assemblies,count);Require(assetProxy,"demand Prefab AssetProxy class absent");
  api.proxyInfo=FullResourceReload::Method(assetProxy,"GetAssetInfo",{},StartupDependencies::TypeName(il2cpp_class_get_type(api.packClass)).c_str(),false);
  api.assetPath=FullResourceReload::Method(api.packClass,"get_assetPath",{},"System.String",false);
  auto bundle=FindClass("UnityEngine","AssetBundle",assemblies,count);auto request=FindClass("UnityEngine","AssetBundleCreateRequest",assemblies,count);auto assetRequest=FindClass("UnityEngine","AssetBundleRequest",assemblies,count);
  auto hook=[&](void* type,const char* method,std::initializer_list<const char*> args,const char* result,const char* icall,void* detour,void** original,bool isStatic=false){Require(Exact(type,method,args,result,isStatic),std::string("demand native ABI absent: ")+method);Require(LoadingObservation::Hook(icall,detour,original),std::string("demand native hook absent: ")+method);};
  hook(bundle,"LoadAssetByHash_Internal",{"System.Int64","System.Type"},"UnityEngine.Object","UnityEngine.AssetBundle::LoadAssetByHash_Internal(System.Int64,System.Type)",(void*)HashAsset,(void**)&hashAsset);
  hook(bundle,"LoadAsset_Internal",{"System.String","System.Type"},"UnityEngine.Object","UnityEngine.AssetBundle::LoadAsset_Internal(System.String,System.Type)",(void*)StringAsset,(void**)&stringAsset);
  hook(bundle,"LoadAssetAsyncByHash_Internal",{"System.Int64","System.Type"},"UnityEngine.AssetBundleRequest","UnityEngine.AssetBundle::LoadAssetAsyncByHash_Internal(System.Int64,System.Type)",(void*)HashAsync,(void**)&hashAsync);
  hook(bundle,"LoadAssetAsync_Internal",{"System.String","System.Type"},"UnityEngine.AssetBundleRequest","UnityEngine.AssetBundle::LoadAssetAsync_Internal(System.String,System.Type)",(void*)StringAsync,(void**)&stringAsync);
  hook(request,"get_assetBundle",{},"UnityEngine.AssetBundle","UnityEngine.AssetBundleCreateRequest::get_assetBundle()",(void*)BundleGetter,(void**)&bundleGetter);
  hook(assetRequest,"GetResult",{},"UnityEngine.Object","UnityEngine.AssetBundleRequest::GetResult()",(void*)AssetGetter,(void**)&assetGetter);
  hook(bundle,"LoadFromFile_Internal",{"System.String","System.UInt32","System.UInt64"},"UnityEngine.AssetBundle","UnityEngine.AssetBundle::LoadFromFile_Internal(System.String,System.UInt32,System.UInt64)",(void*)NativeFile,(void**)&nativeFile,true);
  hook(bundle,"LoadFromFileAsync_Internal",{"System.String","System.UInt32","System.UInt64"},"UnityEngine.AssetBundleCreateRequest","UnityEngine.AssetBundle::LoadFromFileAsync_Internal(System.String,System.UInt32,System.UInt64)",(void*)NativeFileAsync,(void**)&nativeFileAsync,true);
  enabled=true;Log("[DEMAND-PROBE-READY] beforeNativeBundle=1 beforeNativeAsset=1 partialBlockReader=1 compiledBackendDisabled=1 nativeObjectsCreatedByEff=0");
  Log("[DEMAND-TIMING-BUDGET] eventLimit=%llu gapLimit=64 matchedVfsBeginLimit=128 candidateAssetLimit=256 phase=route-submit-async-completion-asset-call",timingEventLimit);
#if defined(EIEM_DEMAND_NATIVE_ACCEPTANCE)
  ReadInstalledTargets();InstallSharedInputs(assemblies,count);
  Log("[DEMAND-NATIVE-READY] targets=declarationDriven carrier=originalCabAndPathId consumerBones=creationBefore nativeDeserializerOwnsCreation=1 materialOverrides=shared-native-dependencies effects=lazy-native-inputs controls=author-declarations f10=immutable-generation");
#endif
 }catch(const std::exception& e){enabled=false;Log("[DEMAND-PROBE-GAP] phase=bind reason=%s",e.what());}}
} // namespace DemandLoadingProbe
