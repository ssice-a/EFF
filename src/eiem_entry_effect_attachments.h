#pragma once
namespace RuntimeAttachments {
using RuntimeShapes::Ref;using FullResourceReload::Call;namespace Native=EffectDebug;
struct Binding {std::string package,id,variable;Ref owner,parent;std::vector<Ref> renderers;EffectAttachment attachment;std::shared_ptr<Native::Generation> generation;std::shared_ptr<Native::Instance> instance;bool failed=false,changed=false;};
// LOD renderers sharing a bone use one instance; separate skeleton copies
// under the same model root need their own instance and visibility state.
using BindingKey=std::tuple<void*,std::string,std::string,void*>;
std::map<BindingKey,Binding> bindings;
uint64_t revision=0;
bool needed=false;unsigned cleanupTick=0;
std::map<std::pair<std::string,std::string>,EffectAttachment> overrides;
// Presence of a package is authoritative, including an intentionally empty map.
std::map<std::string,std::map<std::string,EffectAttachment>> diskEffects;
const std::map<std::string,EffectAttachment>& Effects(const Package& p){auto found=diskEffects.find(p.id);return found==diskEffects.end()?p.effects:found->second;}
std::shared_ptr<const std::vector<Package>> standalone=std::make_shared<const std::vector<Package>>();
std::shared_ptr<const std::vector<Package>> pendingStandalone;
std::map<std::string,std::map<std::string,EffectAttachment>> pendingEffects;
struct AttachmentStamp {std::string part,bone,variable,nativeKey,targetPrefab,rendererPath,targetPath,targetAsset;bool autoplay=true;std::array<float,3> position{0,0,0},scale{1,1,1};std::array<float,4> rotation{0,0,0,1};std::vector<std::string> targetBones;
 bool operator==(const AttachmentStamp& other)const{return part==other.part&&bone==other.bone&&variable==other.variable&&nativeKey==other.nativeKey&&targetPrefab==other.targetPrefab&&rendererPath==other.rendererPath&&targetPath==other.targetPath&&targetAsset==other.targetAsset&&autoplay==other.autoplay&&position==other.position&&scale==other.scale&&rotation==other.rotation&&targetBones==other.targetBones;}
 bool operator!=(const AttachmentStamp& other)const{return !(*this==other);}
};
using AttachmentStamps=std::map<std::tuple<unsigned,std::string,std::string>,AttachmentStamp>;
AttachmentStamps activeStamps;bool stampsInitialized=false,pendingForceReload=false;
std::mutex reloadMutex;std::atomic<bool> reloadPending{false};uint64_t reconcileClock=0;
std::map<void*,Ref> pendingOwners;
std::map<void*,Ref> retryOwners;
void QueueOwnerReady(void* root){if(root&&!pendingOwners.count(root))pendingOwners.emplace(root,Ref::Make(root,true));}
std::string OwnerPath(void* root){for(const auto& row:FullResourceReload::liveParts)if(row.second.root.Get()==root)return row.second.path;auto ui=FullResourceReload::liveUIModels.find(root);if(ui!=FullResourceReload::liveUIModels.end())return ui->second.path;for(const auto& row:FullResourceReload::liveHelperModels)if(row.second.root.Get()==root)return "render-helper";return {};}
void AddStamps(AttachmentStamps& result,unsigned source,const std::string& package,const std::map<std::string,EffectAttachment>& effects,bool applyOverrides){for(const auto& row:effects){const EffectAttachment* effect=&row.second;if(applyOverrides){auto replacement=overrides.find({package,row.first});if(replacement!=overrides.end())effect=&replacement->second;}AttachmentStamp stamp;stamp.part=effect->part;stamp.bone=effect->bone;stamp.variable=effect->variable;stamp.nativeKey=effect->native?effect->native->key:std::string();stamp.targetPrefab=effect->targetPrefab;stamp.rendererPath=effect->rendererPath;stamp.targetPath=effect->targetPath;stamp.targetAsset=effect->targetAsset;stamp.targetBones=effect->targetBones;stamp.autoplay=effect->autoplay;stamp.position=effect->position;stamp.rotation=effect->rotation;stamp.scale=effect->scale;result.emplace(std::make_tuple(source,package,row.first),std::move(stamp));}}
AttachmentStamps CaptureStamps(){AttachmentStamps result;auto graph=RuntimeControls::controlGraph;if(graph)for(const auto& p:*graph){AddStamps(result,0,p.id,p.effects,false);AddStamps(result,1,p.id,Effects(p),true);}if(standalone)for(const auto& p:*standalone)AddStamps(result,2,p.id,Effects(p),true);return result;}
void PrepareDiskReload(bool force=true){try{
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::EffectDisk);
 StandaloneEffects::PublishReadLock publication(gameDirectory/"plugin/mods");
 auto own=std::make_shared<const std::vector<Package>>(StandaloneEffects::ReadAll(gameDirectory));std::map<std::string,std::map<std::string,EffectAttachment>> attached;auto config=settings.Get();
 for(const auto& p:*own)for(const auto& key:p.bindings)Require(key.key.vk!=config.reload.vk&&key.key.vk!=config.gui.vk&&key.key.vk!=config.effectsReload.vk,"effect Mod key overlaps global shortcut");
 auto graph=std::atomic_load(&published);if(graph)for(const auto& p:*graph)attached[p.id]=StandaloneEffects::ReadAttached(p);
 std::lock_guard<std::mutex> lock(reloadMutex);pendingStandalone=std::move(own);pendingEffects=std::move(attached);pendingForceReload=pendingForceReload||force;reloadPending=true;
 performance.hit=true;Log("[EFFECT-RELOAD] prepared=1 modelRebuilds=0");
 }catch(const std::exception& e){{std::lock_guard<std::mutex> lock(reloadMutex);pendingStandalone.reset();pendingEffects.clear();pendingForceReload=false;reloadPending=false;}Log("[EFFECT-RELOAD-REJECT] reason=%s oldEffectsRetained=1",e.what());}}
void Stop(Binding& b){if(b.instance){--b.instance->generation->externalPins;Native::Retire(b.instance);b.instance.reset();}}
void Release(Binding& b){Stop(b);if(b.generation){--b.generation->externalPins;b.generation.reset();Native::nativeWork=true;}}
std::shared_ptr<Native::Generation> Acquire(const std::shared_ptr<const EiemEffectNative::Manifest>& manifest){
 for(const auto& g:Native::generations)if(!g->finished&&g->manifest->key==manifest->key){
  bool same=g->manifest->prefab==manifest->prefab&&g->manifest->rootName==manifest->rootName&&g->manifest->root==manifest->root&&g->manifest->particles==manifest->particles&&g->manifest->bundles.size()==manifest->bundles.size();
  for(size_t i=0;same&&i<manifest->bundles.size();++i)same=g->manifest->bundles[i].hash==manifest->bundles[i].hash&&g->manifest->bundles[i].dependencies==manifest->bundles[i].dependencies;
  Require(same,"effect native namespace reused with incompatible content");Require(!g->quarantined,"effect native namespace awaits safe cleanup: "+g->failure);++g->externalPins;return g;
 }
 auto g=std::make_shared<Native::Generation>();g->manifest=manifest;g->clock=NativeBundle::Clock();g->bundles.resize(manifest->bundles.size());g->externalPins=1;Native::generations.push_back(g);Native::nativeWork=true;return g;
}
void* Parent(void* root,void* renderer,const Package& p,const EffectAttachment& effect){
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::EffectMount);
 if(effect.bone.empty())return FullResourceReload::Call(FullResourceReload::api.goTransform,root);
 Require(RuntimeShapes::RendererKind(renderer)==137,"bone effect requires a skinned owner mesh");auto expected=effect.part.empty()?effect.targetBones:p.meshes.at(p.parts.at(effect.part).mesh).bones;
 if(effect.part.empty())if(auto spec=RuntimeShapes::Match(RuntimeShapes::SourceMesh(renderer)))if(RuntimeControls::controlGraph)for(const auto& author:*RuntimeControls::controlGraph)if(author.id==spec->package)expected=author.meshes.at(author.parts.at(spec->part).mesh).bones;
 auto array=Ref::Make(FullResourceReload::Call(RuntimeShapes::api.bones,renderer),false);size_t count=0;
 Require(ArrayLength(array.Get(),&count),"effect owner bone array unavailable: "+effect.part);
 Require(count==expected.size(),"effect owner bone palette differs: part="+effect.part+" expected="+std::to_string(expected.size())+" actual="+std::to_string(count));
 auto bones=FullResourceReload::ReferenceArray(array.Get(),count);Require(!bones.empty(),"effect owner mesh has no skeleton provenance");
 auto rootBone=Ref::Make(FullResourceReload::Call(RuntimeShapes::api.rootBone,renderer),false);FullResourceReload::BoneHierarchy hierarchy(root);void* anchor=nullptr;size_t anchors=0;
 for(auto candidate:hierarchy.Chain(rootBone.Get())){bool matches=true;for(size_t i=0;i<bones.size();++i){auto chain=hierarchy.Chain(bones[i]);if(std::find(chain.begin(),chain.end(),candidate)==chain.end()||hierarchy.Relative(chain,candidate)!=expected[i]){matches=false;break;}}if(matches){anchor=candidate;++anchors;}}
 Require(anchors==1,"effect skeleton anchor missing/ambiguous");FullResourceReload::AssemblyTree tree(root);void* parent=nullptr;size_t matches=0;
 for(const auto& node:tree.nodes){auto chain=hierarchy.Chain(node.second.Get());if(std::find(chain.begin(),chain.end(),anchor)!=chain.end()&&hierarchy.Relative(chain,anchor)==effect.bone){parent=node.second.Get();++matches;}}
 Require(matches==1,"effect bone path absent/ambiguous in owner skeleton: "+effect.bone);return parent;
}
void ModelReadyDetails(void* root,bool audit){
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::EffectBind);
 if(!root||!Native::enabled||!RuntimeShapes::Main())return;
 if(!RuntimeShapes::Alive(root)){retryOwners.erase(root);return;}
 std::vector<const Package*> packages;if(RuntimeControls::controlGraph)for(const auto& p:*RuntimeControls::controlGraph)packages.push_back(&p);for(const auto& p:*standalone)packages.push_back(&p);
 bool any=false;for(auto p:packages)any|=!Effects(*p).empty();if(!any){retryOwners.erase(root);return;}performance.hit=true;bool retry=false;
 auto renderers=FullResourceReload::Components(root);
 for(auto package:packages){const auto& p=*package;std::map<std::string,EffectAttachment> effects=Effects(p);for(const auto& replacement:overrides)if(replacement.first.first==p.id)effects[replacement.first.second]=replacement.second;
 for(const auto& row:effects){const auto& e=row.second;std::vector<Ref> matched;
  // A postmodel and a gameplay/display prefab can share the same source Mesh.
  // Verify renderer path + mesh identity/name + ordered skeleton palette below;
  // the author prefab is provenance, not a live instance address.
  // RuntimeControls prepares this Part's proven marker for EVERY target in
  // its Replace rule, including inactive LODs. Never fall back to a sibling
  // Part or an unverified same-name Mesh when the attachment has a Part.
  for(const auto& r:renderers){auto mesh=RuntimeShapes::SourceMesh(r.Get());auto spec=RuntimeShapes::Match(mesh);if(spec&&((spec->package==p.id&&spec->part==e.part)||(e.part.empty()&&!e.targetPrefab.empty()&&RuntimeControls::controlGraph&&[&](){for(const auto& author:*RuntimeControls::controlGraph)if(author.id==spec->package)for(const auto& rule:author.rules)for(const auto& target:rule.second.targets)if(target.second.path==e.targetPath&&target.second.asset==e.targetAsset&&std::find(rule.second.parts.begin(),rule.second.parts.end(),spec->part)!=rule.second.parts.end())return true;return false;}())))matched.push_back(r);
   else if(e.part.empty()&&RuntimeShapes::Alive(mesh)&&String(Call(RuntimeShapes::api.name,mesh))==e.targetAsset){auto go=Ref::Make(FullResourceReload::Call(FullResourceReload::api.componentGo,r.Get()),false);auto transform=FullResourceReload::Call(FullResourceReload::api.goTransform,go.Get());FullResourceReload::BoneHierarchy hierarchy(root);if(hierarchy.Relative(hierarchy.Chain(transform),hierarchy.root.Get())==e.rendererPath)matched.push_back(r);}}
  if(audit&&!matched.empty()){size_t packageRenderers=0;std::string sample;
   for(const auto& r:renderers){auto mesh=RuntimeShapes::SourceMesh(r.Get());auto spec=RuntimeShapes::Match(mesh);if(spec&&spec->package==p.id){++packageRenderers;if(sample.empty())sample=spec->part;}}
   Log("[EFFECT-ATTACHMENT-MATCH] package=%s effect=%s owner=%p scope=%s path=%s renderers=%zu packageRenderers=%zu matched=%zu requiredPart=%s samplePart=%s",p.id.c_str(),e.id.c_str(),root,FullResourceReload::liveUIModels.count(root)?"ui":"model-part",OwnerPath(root).c_str(),renderers.size(),packageRenderers,matched.size(),e.part.c_str(),sample.c_str());
  }
  std::map<void*,std::vector<Ref>> groups;
  try{for(const auto& r:matched)groups[Parent(root,r.Get(),p,e)].push_back(r);}
  catch(const std::exception& error){retry=true;Log("[EFFECT-ATTACHMENT-GAP] package=%s effect=%s reason=%s previousInstanceRetained=1",p.id.c_str(),e.id.c_str(),error.what());continue;}
  for(auto it=bindings.begin();it!=bindings.end();)if(std::get<0>(it->first)==root&&std::get<1>(it->first)==p.id&&std::get<2>(it->first)==e.id&&!groups.count(std::get<3>(it->first))){Release(it->second);it=bindings.erase(it);}else ++it;
  for(auto& group:groups){auto key=std::make_tuple(root,p.id,e.id,group.first);auto& matched=group.second;auto existing=bindings.find(key);
  if(existing!=bindings.end()){auto& old=existing->second;const auto& a=old.attachment;bool same=old.generation&&old.generation->manifest->key==e.native->key&&!old.failed&&a.part==e.part&&a.bone==e.bone&&a.variable==e.variable&&a.targetPrefab==e.targetPrefab&&a.rendererPath==e.rendererPath&&a.targetPath==e.targetPath&&a.targetAsset==e.targetAsset&&a.targetBones==e.targetBones&&a.position==e.position&&a.rotation==e.rotation&&a.scale==e.scale&&a.autoplay==e.autoplay;
   if(same){bool sameRenderers=old.renderers.size()==matched.size();for(size_t i=0;sameRenderers&&i<matched.size();++i)sameRenderers=old.renderers[i].Get()==matched[i].Get();
    // A late/retired LOD only changes which Renderer supplies visibility and
    // layer. Keep the bone's running simulation and native resource pins.
    if(!sameRenderers){old.renderers=std::move(matched);Log("[EFFECT-ATTACHMENT-LOD] package=%s effect=%s owner=%p renderers=%zu lodCoverage=all playbackRetained=1",p.id.c_str(),e.id.c_str(),root,old.renderers.size());}continue;}}
  try{Binding b;b.package=p.id;b.id=e.id;b.variable=e.variable;b.attachment=e;b.changed=true;b.owner=Ref::Make(root,true);b.parent=Ref::Make(group.first,false);b.renderers=std::move(matched);b.generation=Acquire(e.native);
   if(existing!=bindings.end()){b.instance=std::move(existing->second.instance);if(existing->second.generation)--existing->second.generation->externalPins;existing->second=std::move(b);}else bindings.emplace(key,std::move(b));
   Log("[EFFECT-ATTACHMENT-BIND] package=%s effect=%s part=%s bone=%s owner=%p scope=%s renderers=%zu lodCoverage=all path=%s",p.id.c_str(),e.id.c_str(),e.part.c_str(),e.bone.c_str(),root,FullResourceReload::liveUIModels.count(root)?"ui":"model-part",bindings.at(key).renderers.size(),OwnerPath(root).c_str());
  }catch(const std::exception& error){Log("[EFFECT-ATTACHMENT-GAP] package=%s effect=%s reason=%s previousInstanceRetained=1",p.id.c_str(),e.id.c_str(),error.what());}
  }
 }}
 if(retry)retryOwners[root]=Ref::Make(root,true);else retryOwners.erase(root);
}
void ModelReady(void* root){ModelReadyDetails(root,false);}
void Reload(const std::string& package,const EffectAttachment& effect){
 Require(RuntimeControls::controlGraph&&RuntimeShapes::Main(),"attachment reload requires live Unity graph");const Package* owner=nullptr;for(const auto& p:*RuntimeControls::controlGraph)if(p.id==package)owner=&p;
 Require(owner&&owner->effects.count(effect.id),"attachment reload requires an exported Mod effect declaration");const auto& previous=owner->effects.at(effect.id);
 Require(previous.part==effect.part&&previous.bone==effect.bone&&previous.variable==effect.variable,"changing effect owner/bone/control requires Mod export and F10");
 overrides[{package,effect.id}]=effect;activeStamps=CaptureStamps();stampsInitialized=true;for(const auto& row:Native::Owners())ModelReady(row.second.Get());
 Log("[EFFECT-ATTACHMENT-RELOAD] package=%s effect=%s key=%s modelRebuilds=0",package.c_str(),effect.id.c_str(),effect.native->key.c_str());
}
void OwnerReleasing(void* root) noexcept {
 if(!root)return;pendingOwners.erase(root);retryOwners.erase(root);if(!RuntimeShapes::Main())return;
 try{for(auto it=bindings.begin();it!=bindings.end();)if(it->second.owner.Get()==root){Release(it->second);it=bindings.erase(it);}else ++it;
  if(RuntimeShapes::Alive(root)&&!RuntimeMaterials::bound.empty())for(const auto& renderer:FullResourceReload::Components(root))RuntimeMaterials::Forget(renderer.Get());
 }catch(const std::exception& e){Log("[EFFECT-ATTACHMENT-GAP] ownerRelease=%s",e.what());}
}
void Frame(){
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::EffectFrame);
 bool diskReload=false,forceDiskReload=false;if(reloadPending.load()){std::lock_guard<std::mutex> lock(reloadMutex);if(reloadPending.exchange(false)){standalone=std::move(pendingStandalone);overrides.clear();diskEffects=std::move(pendingEffects);pendingEffects.clear();diskReload=true;forceDiskReload=pendingForceReload;pendingForceReload=false;needed=!standalone->empty();for(const auto& p:diskEffects)needed|=!p.second.empty();if(forceDiskReload)for(auto& b:bindings){b.second.failed=false;if(b.second.generation&&b.second.generation->phase==Native::Phase::Rejected)b.second.generation->retryRequested=true;}}}
 bool controlsChanged=revision!=RuntimeControls::revision;
 if(controlsChanged){revision=RuntimeControls::revision;if(!diskReload)overrides.clear();needed=false;if(RuntimeControls::controlGraph)for(const auto& p:*RuntimeControls::controlGraph)needed|=!Effects(p).empty();}
 needed=needed||!standalone->empty();bool sourceChanged=controlsChanged||diskReload;bool definitionsChanged=false;
 if(sourceChanged||!stampsInitialized){auto next=CaptureStamps();definitionsChanged=!stampsInitialized||next!=activeStamps;activeStamps=std::move(next);stampsInitialized=true;}
 bool reconcileAll=definitionsChanged||(diskReload&&forceDiskReload);
 if(!needed&&bindings.empty()&&!sourceChanged){pendingOwners.clear();return;}if(!Native::enabled||!RuntimeShapes::Main())return;
 if(reconcileAll){
  for(auto it=bindings.begin();it!=bindings.end();){bool exists=false;for(const auto& p:*standalone)exists|=p.id==it->second.package&&Effects(p).count(it->second.id);if(RuntimeControls::controlGraph)for(const auto& p:*RuntimeControls::controlGraph)exists|=p.id==it->second.package&&Effects(p).count(it->second.id);if(!exists){Release(it->second);it=bindings.erase(it);}else ++it;}
  pendingOwners.clear();retryOwners.clear();auto owners=Native::Owners();for(auto& owner:owners)ModelReadyDetails(owner.second.Get(),forceDiskReload);reconcileClock=NativeBundle::Clock();
  if(forceDiskReload){for(auto& row:bindings){auto& b=row.second;Log("[EFFECT-ATTACHMENT-STATE] package=%s effect=%s owner=%p parent=%p renderers=%zu instance=%p failed=%d desiredDefault=%d",b.package.c_str(),b.id.c_str(),b.owner.Get(),b.parent.Get(),b.renderers.size(),b.instance?b.instance->object.Get():nullptr,int(b.failed),int(b.attachment.autoplay));
    if(b.instance){Native::Observation::Capture(*b.instance,"after-F9");b.instance->observationClock=NativeBundle::Clock();b.instance->observationStep=0;}}}
  Log("[EFFECT-RELOAD] committed standalone=%zu owners=%zu bindings=%zu diskReload=%d definitionsChanged=%d forced=%d modelRebuilds=0",standalone->size(),owners.size(),bindings.size(),int(diskReload),int(definitionsChanged),int(forceDiskReload));}
 else if(!pendingOwners.empty()){
  auto owners=std::move(pendingOwners);pendingOwners.clear();
  for(auto& owner:owners)if(RuntimeShapes::Alive(owner.first))ModelReadyDetails(owner.first,false);
  reconcileClock=NativeBundle::Clock();
 }
 // A pooled model can rebuild its skeleton between owner callbacks. Retry
 // missing bindings against tracked game-owned models; never query the scene.
 if(NativeBundle::Ms(reconcileClock)>1000.){reconcileClock=NativeBundle::Clock();auto retry=std::move(retryOwners);retryOwners.clear();for(auto& owner:retry)if(RuntimeShapes::Alive(owner.first))ModelReadyDetails(owner.first,false);}
 if(bindings.empty())return;performance.hit=true;auto clock=NativeBundle::Clock();std::set<Native::Generation*> stepped;++cleanupTick;
 for(auto it=bindings.begin();it!=bindings.end();){auto& b=it->second;
  auto values=RuntimeControls::variables.find(b.package);bool desired=active&&(values==RuntimeControls::variables.end()||!values->second.count(b.variable)?b.attachment.autoplay:values->second.at(b.variable)>=.5);bool visible=false;Ref layerSource;
  if(!desired&&!b.instance&&(uintptr_t(std::get<0>(it->first))>>4)%64!=cleanupTick%64){++it;continue;}
  auto owner=b.owner.Get();bool ownerAlive=RuntimeShapes::Alive(owner);
  if(!ownerAlive||!RuntimeShapes::Alive(b.parent.Get())){if(ownerAlive)retryOwners[owner]=Ref::Make(owner,true);Release(b);it=bindings.erase(it);continue;}
  // The game's scene cleanup can retire an unmanaged effect independently of
  // its model. A strong managed handle does not keep the Unity object alive.
  if(b.instance&&(!RuntimeShapes::Alive(b.instance->object.Get())||!RuntimeShapes::Scalar<unsigned char>(Native::api.activeSelf,b.instance->object.Get()))){Stop(b);b.failed=false;b.changed=true;Log("[EFFECT-ATTACHMENT-RECOVER] package=%s effect=%s owner=%p reason=clone-retired-or-disabled",b.package.c_str(),b.id.c_str(),b.owner.Get());}
  for(const auto& r:b.renderers){EiemLoadPerformance::Add(EiemLoadPerformance::Operation::EffectVisibility);if(RuntimeShapes::Alive(r.Get())&&RuntimeShapes::Scalar<unsigned char>(RuntimeShapes::api.getEnabled,r.Get())){auto go=Ref::Make(Call(FullResourceReload::api.componentGo,r.Get()),false);if(RuntimeShapes::Scalar<unsigned char>(Native::api.activeHierarchy,go.Get())){visible=true;if(!layerSource.Get())layerSource=go;}}}desired&=visible;
  if(!desired){Stop(b);++it;continue;}
  auto& g=*b.generation;if(!b.failed&&g.phase!=Native::Phase::Ready&&(g.phase!=Native::Phase::Rejected||g.retryRequested)&&stepped.insert(&g).second){try{
    if(g.phase==Native::Phase::Rejected){
     // F9 may retry identical content only after all prior native requests and
     // owned containers drain. Preserve the shared generation identity/pins;
     // no clone can have been created from a rejected preparation.
     if(!g.quarantined&&Native::Drain(g)){auto manifest=g.manifest;auto pins=g.externalPins;g=Native::Generation{};g.manifest=std::move(manifest);g.externalPins=pins;g.clock=NativeBundle::Clock();g.bundles.resize(g.manifest->bundles.size());Log("[EFFECT-ATTACHMENT-RETRY] key=%s previousRequestsDrained=1",g.manifest->key.c_str());}
     else if(g.quarantined)g.retryRequested=false;
    }
    while(g.phase!=Native::Phase::Rejected&&NativeBundle::Ms(clock)<1.&&Native::Step(g))if(g.phase==Native::Phase::Ready){Log("[EFFECT-ATTACHMENT-READY] key=%s prepareMs=%.3f",g.manifest->key.c_str(),NativeBundle::Ms(g.clock));break;}}
   catch(const std::exception& e){g.phase=Native::Phase::Rejected;g.retryRequested=false;g.failure=e.what();b.failed=true;Log("[EFFECT-ATTACHMENT-GAP] prepare=%s previousInstanceRetained=1",e.what());}}
  if(!b.failed&&g.phase==Native::Phase::Ready&&(b.changed||!b.instance||b.instance->generation!=b.generation))try{auto next=Native::CreateInstance(b.generation,b.owner.Get(),b.parent.Get(),&b.attachment,layerSource.Get(),FullResourceReload::liveUIModels.count(b.owner.Get())!=0);++b.generation->externalPins;Stop(b);b.instance=std::move(next);b.changed=false;Log("[EFFECT-ATTACHMENT-PLAY] package=%s effect=%s owner=%p layer=%d particles=%zu readyToPlayMs=%.3f modelRebuilds=0",b.package.c_str(),b.id.c_str(),b.owner.Get(),b.instance->layer,b.instance->particles.size(),NativeBundle::Ms(clock));}
   catch(const std::exception& e){b.failed=true;Log("[EFFECT-ATTACHMENT-GAP] play=%s previousInstanceRetained=1",e.what());}
  if(b.instance&&!b.failed)try{Native::FollowLayer(*b.instance,layerSource.Get());Native::Observation::Tick(*b.instance);}catch(const std::exception& e){b.failed=true;Log("[EFFECT-ATTACHMENT-GAP] layer=%s previousInstanceRetained=1",e.what());}
  ++it;
 }
}
}
