#pragma once
#include "eiem_entry_author_materials.h"
#include "eiem_entry_author_asset_load.h"
// A developer-only acceptance seam. It does not publish routes, modify live
// instances or replace the production byte backend. Discovery starts only at
// real raw Prefab returns. Frames advance already-pending native requests and
// their queued proofs; they never discover resources or search the scene.
namespace PrefabAdaptationProbe {
using RuntimeShapes::Ref;using RuntimeShapes::Call;using RuntimeShapes::Scalar;
struct Api {
 void *assetInfo=nullptr,*subAssetInfo=nullptr,*assetInfoClass=nullptr,*bundleInfoClass=nullptr,*proxyInfo=nullptr,*assetPath=nullptr,*loadMeshHash=nullptr;
 void *bundleGetter=nullptr,*loadMesh=nullptr,*goClass=nullptr,*goCtor=nullptr,*setActive=nullptr;
 void *activeHierarchy=nullptr,*cloneParent=nullptr,*destroyImmediate=nullptr,*setSkinMesh=nullptr,*textureWidth=nullptr,*textureHeight=nullptr,*textureMips=nullptr;
 size_t assetSize=0,bundleSize=0,bundleHashOffset=0,assetHashOffset=0;
} api;
struct Config {std::shared_ptr<const Bytes> bytes;std::string path,name,marker,digest;int vertices=0,bones=0,shapes=0;} config;
bool enabled=false,constructing=false,loading=false;
std::filesystem::path sampleRoot;
std::optional<EiemRuntimeAuthorDirectory::Directory> sampleDirectory;
unsigned initializedDuringClone=0;Ref meshType,authorMesh,resolvedMesh;
Ref textureType;void* textureByIdSetter=nullptr;
AuthorAssetLoad::Cache assetLoads;AuthorAssetLoad::Ticket meshLoad,textureLoad;
std::optional<EiemRuntimeAuthorDirectory::TextureAsset> sampleTexture;
std::shared_ptr<const Bytes> textureBytes;Ref authorTexture;
std::map<int,Ref> observed;std::map<int,size_t> matchedRoots;std::map<int,bool> verifiedRoots;
using ProbeClock=std::chrono::steady_clock;
inline double Elapsed(ProbeClock::time_point begin){return std::chrono::duration<double,std::milli>(ProbeClock::now()-begin).count();}
struct Timing {
 double lookup=0,discovery=0,authorLoad=0,clone=0,binding=0,appearance=0,sourceCheck=0,cleanup=0,total=0;
 double wait=0;bool deferred=false;
};
std::map<int,Timing> timings;
struct PendingPrefab {Ref source,mesh;int id=0;std::string path,boundary;size_t hits=0;ProbeClock::time_point begin;Timing timing;};
std::map<int,PendingPrefab> pendingPrefabs;
using LifecycleFn=void(*)(void*,void*);
LifecycleFn originalHelperAwake=nullptr,originalClothAwake=nullptr,originalClothEnable=nullptr;
void Initializing(const char* entry){
 if(constructing){++initializedDuringClone;Log("[ADAPT-PROBE-INITIALIZATION] entry=%s duringInactiveTemplateConstruction=1",entry);}
}
void HelperAwake(void* self,void* info){Initializing("EntityRenderHelper.OnAwake");originalHelperAwake(self,info);}
void ClothAwake(void* self,void* info){Initializing("BeyondBoneCloth.Awake");originalClothAwake(self,info);}
void ClothEnable(void* self,void* info){Initializing("BeyondBoneCloth.OnEnable");originalClothEnable(self,info);}
void* ShapeMethod(void* klass,const char* name,size_t count){
 void* selected=nullptr,*iterator=nullptr;while(auto method=il2cpp_class_get_methods(klass,&iterator)){
  if(strcmp(il2cpp_method_get_name(method),name)||il2cpp_method_get_param_count(method)!=count)continue;
  uint32_t impl=0;Require(!(il2cpp_method_get_flags(method,&impl)&0x10),"adaptation probe method unexpectedly static");
  Require(!selected,"adaptation probe method shape ambiguous");selected=method;
 }
 Require(selected,"adaptation probe method shape absent");return selected;
}
void BindLookup(void* manifest){
 api.assetInfo=ShapeMethod(manifest,"TryGetAssetInfoFromPath",2);
 auto keyType=StartupDependencies::TypeName(il2cpp_method_get_param(api.assetInfo,0));
 Require((keyType=="System.UInt64"||keyType=="System.Int64")&&
         il2cpp_type_is_byref(il2cpp_method_get_param(api.assetInfo,1))&&
         StartupDependencies::TypeName(il2cpp_method_get_return_type(api.assetInfo))=="System.Boolean",
         "adaptation probe asset lookup signature differs");
 api.assetInfoClass=il2cpp_class_from_type(il2cpp_method_get_param(api.assetInfo,1));
 api.assetSize=StartupDependencies::ValueSize(api.assetInfoClass);
 // RuntimeManifestBinary returns AssetInfoPack, not its nested AssetInfo.
 // Its bundleData already contains the current Bundle record. Bind the nested
 // value through real field metadata instead of querying/reconstructing it.
 auto field=StartupDependencies::Field(api.assetInfoClass,"bundleData");
 api.bundleInfoClass=il2cpp_class_from_type(il2cpp_field_get_type(field));
 api.bundleSize=StartupDependencies::ValueSize(api.bundleInfoClass);
 auto key=StartupDependencies::Field(api.bundleInfoClass,"hashName");
 auto scalar=StartupDependencies::TypeName(il2cpp_field_get_type(key));
 Require(scalar=="System.Int64"||scalar=="System.UInt64","adaptation probe nested Bundle key is not 64-bit");
 api.bundleHashOffset=StartupDependencies::ValueOffset(api.assetInfoClass,"bundleData",api.bundleSize)+
                      StartupDependencies::ValueOffset(api.bundleInfoClass,"hashName",sizeof(uint64_t));
 Require(api.bundleHashOffset<=api.assetSize&&sizeof(uint64_t)<=api.assetSize-api.bundleHashOffset,"adaptation probe nested key escapes result value");
 Log("[ADAPT-PROBE-LOOKUP] resultType=%s resultBytes=%zu bundleType=%s nestedHashOffset=%zu metadataBound=1",
     StartupDependencies::TypeName(il2cpp_method_get_param(api.assetInfo,1)).c_str(),api.assetSize,
     StartupDependencies::TypeName(il2cpp_field_get_type(field)).c_str(),api.bundleHashOffset);
}
Ref ExactLoadedMesh(){
 if(RuntimeShapes::Alive(resolvedMesh.Get()))return Ref::Make(resolvedMesh.Get(),false);
 auto path=FullResourceReload::Path(config.path);uint64_t hash=0;
 il2cpp_field_get_value(path.Get(),FullResourceReload::api.pathHashField,&hash);
 std::vector<StartupDependencies::Snapshot> snapshots;
 {std::lock_guard<std::mutex> lock(StartupDependencies::mutex);for(const auto& entry:StartupDependencies::snapshots)snapshots.push_back(entry.second);}
 std::set<void*> checked;
 for(const auto& snapshot:snapshots){auto manifest=snapshot.manifest.Get();if(!manifest||!checked.insert(manifest).second)continue;
  Bytes info(api.assetSize);bool subasset=config.path.size()>=4&&config.path.substr(config.path.size()-4)==".fbx";
  if(subasset){
   auto text=Ref::Make(il2cpp_string_new(config.path.c_str()),false),name=Ref::Make(il2cpp_string_new(config.name.c_str()),false);
   void* args[]={text.Get(),info.data(),name.Get()};if(!Scalar<unsigned char>(api.subAssetInfo,manifest,args))continue;
  }else{void* args[]={&hash,info.data()};if(!Scalar<unsigned char>(api.assetInfo,manifest,args))continue;}
  uint64_t key=0;memcpy(&key,info.data()+api.bundleHashOffset,sizeof(key));
  for(const auto& candidate:snapshots){if(candidate.manifest.Get()!=manifest||candidate.hash!=key)continue;
   auto proxy=candidate.proxy.Get();if(!proxy)continue;auto loaded=Ref::Make(Call(api.bundleGetter,proxy),false);if(!loaded.Get())continue;
   // Query only the exact .asset on an already loaded Bundle. This does not
   // request a Bundle or wait on ResourceManager's asynchronous load queue.
   Ref mesh;
   if(subasset){uint64_t assetHash=0;memcpy(&assetHash,info.data()+api.assetHashOffset,sizeof(assetHash));void* meshArgs[]={&assetHash,meshType.Get()};mesh=Ref::Make(Call(api.loadMeshHash,loaded.Get(),meshArgs),false);}
   else{auto name=Ref::Make(il2cpp_string_new(config.path.c_str()),false);void* meshArgs[]={name.Get(),meshType.Get()};mesh=Ref::Make(Call(api.loadMesh,loaded.Get(),meshArgs),false);}
   if(RuntimeShapes::Alive(mesh.Get())){Log("[ADAPT-PROBE-SOURCE] path=%s asset=%s bundleHash=%016llX meshId=%d exactManifestLookup=1 alreadyLoadedBundle=1 sceneDiscovery=0",
    config.path.c_str(),config.name.c_str(),(unsigned long long)key,FullResourceReload::Id(mesh.Get()));resolvedMesh=Ref::Make(mesh.Get(),true);return mesh;}
  }
 }
 return {};
}
void RequestAuthorAssets(){
 if(!meshLoad&&!RuntimeShapes::Alive(authorMesh.Get())){
  meshLoad=assetLoads.Request({{config.digest,config.path,"Mesh"},config.bytes,meshType,[](void* mesh){
   Require(Scalar<int>(RuntimeShapes::api.vertexCount,mesh)==config.vertices&&Scalar<int>(RuntimeShapes::api.shapeCount,mesh)==config.shapes,
           "independent author Mesh vertex/shape metadata differs");
   auto poses=Ref::Make(Call(RuntimeShapes::api.bindPoses,mesh),false);size_t count=0;
   Require(ArrayLength(poses.Get(),&count)&&count==size_t(config.bones),"independent author Mesh bindpose metadata differs");
   Require(String(Call(RuntimeShapes::api.name,mesh))==config.marker,"independent author Mesh provenance differs");
  }});
  Log("[ADAPT-PROBE-ASSET-SUBMITTED] kind=Mesh submitMs=%.3f asynchronous=1 actualSourceObserved=1 synchronousFallback=0",meshLoad->submitMs);
 }
 if(sampleTexture&&!textureLoad){
  textureLoad=assetLoads.Request({{sampleTexture->hash,sampleTexture->path,"Texture2D"},textureBytes,textureType,[](void* texture){
   Require(Scalar<int>(api.textureWidth,texture)==int(sampleTexture->width)&&Scalar<int>(api.textureHeight,texture)==int(sampleTexture->height)&&Scalar<int>(api.textureMips,texture)==int(sampleTexture->mips),"independent inline Texture metadata differs");
  }});
  textureBytes.reset();
  Log("[ADAPT-PROBE-ASSET-SUBMITTED] kind=Texture2D submitMs=%.3f asynchronous=1 actualSourceObserved=1 synchronousFallback=0",textureLoad->submitMs);
 }
}
bool AuthorAssetsReady(){
 auto accept=[&](const AuthorAssetLoad::Ticket& load,Ref& object,const char* kind){
  if(!load)return;
  Require(load->phase!=AuthorAssetLoad::Phase::Failed&&load->phase!=AuthorAssetLoad::Phase::Cancelled,"independent author async load failed: "+load->error);
  if(load->phase==AuthorAssetLoad::Phase::Ready&&!object.Get()){
   object=load->asset;
   Log("[ADAPT-PROBE-ASSET-READY] kind=%s submitMs=%.3f unityWorkMs=%.3f maxStepMs=%.3f wallMs=%.3f sharedAcrossPrefabs=1 productionHotPathMeasured=0",kind,load->submitMs,load->workMs,load->maxStepMs,load->wallMs);
  }
 };
 accept(meshLoad,authorMesh,"Mesh");accept(textureLoad,authorTexture,"Texture2D");
 return RuntimeShapes::Alive(authorMesh.Get())&&(!sampleTexture||RuntimeShapes::Alive(authorTexture.Get()));
}
void VerifyTextureAndMaterial(void* candidate){
 if(!sampleDirectory)return;
 Require(sampleTexture&&RuntimeShapes::Alive(authorTexture.Get()),"shared independent Texture not ready");auto texture=authorTexture;
 for(const auto& r:FullResourceReload::Components(candidate)){
  auto materials=Ref::Make(Call(FullResourceReload::api.materials,r.Get()),false);auto values=FullResourceReload::ReferenceArray(materials.Get());
  if(values.empty()||!RuntimeShapes::Alive(values.front()))continue;
  auto donor=values.front();EiemRuntimeAuthorDirectory::MaterialEntry material;material.id="private-validation";
  auto slots=Ref::Make(Call(FullResourceReload::api.textureIds,donor),false);size_t count=0;
  Require(ArrayLength(slots.Get(),&count),"Material Texture property IDs absent");if(!count)continue;int32_t id=0;
  Require(CopyRaw((char*)slots.Get()+32,&id,sizeof(id)),"Material Texture property ID read failed");
  void* beforeArgs[]={&id};auto before=Ref::Make(Call(FullResourceReload::api.textureById,donor,beforeArgs),false);
  auto copy=AuthorMaterials::Create(donor,material,{});
  struct ReleaseMaterial {Ref object;~ReleaseMaterial(){if(object.Get())try{void* args[]={object.Get()};Call(api.destroyImmediate,nullptr,args);}catch(...){}}} releaseMaterial{copy};
  void* assign[]={&id,texture.Get()};Call(textureByIdSetter,copy.Get(),assign);
  Require(Call(FullResourceReload::api.shader,copy.Get())==Call(FullResourceReload::api.shader,donor),"private Material copy changed the source Shader");
  Require(Call(FullResourceReload::api.textureById,copy.Get(),beforeArgs)==texture.Get()&&Call(FullResourceReload::api.textureById,donor,beforeArgs)==before.Get(),"private Material texture override leaked into donor");
  Log("[ADAPT-PROBE-TEXTURE-MATERIAL] width=%u height=%u mips=%u independentInlineTexture=1 privateMaterialCopy=1 originalMaterialPreserved=1 sharedNativeTexture=1",sampleTexture->width,sampleTexture->height,sampleTexture->mips);return;
 }
 throw std::runtime_error("no Material texture slot available for independent acceptance sample");
}
void VerifyBonePaths(void* candidate,void* renderer,const std::vector<void*>& palette,void* skinRoot){
 if(!sampleDirectory)return;
 auto author=std::find_if(sampleDirectory->meshes.begin(),sampleDirectory->meshes.end(),[](const auto& m){return m.marker==config.marker;});
 Require(author!=sampleDirectory->meshes.end(),"native author directory lacks the probe Mesh layout");
 std::vector<EiemPrefabBoneBinding::Node> nodes;std::vector<Ref> transforms;std::map<void*,size_t> indexes;
 auto walk=[&](auto&& self,void* node,size_t parent)->void{
  auto index=nodes.size();Require(indexes.emplace(node,index).second,"candidate bone hierarchy contains a cycle");
  nodes.push_back({parent,String(Call(RuntimeShapes::api.name,node))});transforms.push_back(Ref::Make(node,false));
  int count=Scalar<int>(FullResourceReload::api.childCount,node);Require(count>=0,"candidate Transform child count is negative");
  for(int i=0;i<count;++i){void* args[]={&i};auto child=Call(FullResourceReload::api.getChild,node,args);Require(RuntimeShapes::Alive(child),"candidate child retired during skeleton binding");self(self,child,index);}
 };
 walk(walk,Call(FullResourceReload::api.goTransform,candidate),SIZE_MAX);
 auto owner=Call(RuntimeShapes::api.transform,renderer);Require(indexes.count(owner)&&indexes.count(skinRoot),"candidate Renderer/root bone is outside owning Prefab");
 EiemPrefabBoneBinding::Tree tree(nodes);auto slots=tree.Resolve(indexes.at(owner),indexes.at(skinRoot),author->bones);
 Require(slots.size()==palette.size(),"candidate author bone path coverage differs");
 for(size_t i=0;i<slots.size();++i)Require(transforms.at(slots[i]).Get()==palette[i],"runtime author path maps to another native bone slot");
 auto reversed=author->bones;std::reverse(reversed.begin(),reversed.end());auto alternate=tree.Resolve(indexes.at(owner),indexes.at(skinRoot),reversed);
 for(size_t i=0;i<alternate.size();++i)Require(transforms.at(alternate[i]).Get()==palette[palette.size()-1-i],"runtime bone mapping depends on donor palette order");
 Log("[ADAPT-PROBE-BONE-PATHS] bones=%zu reorderedSlotsVerified=1 prefabScopeOnly=1 sourcePaletteNotUsedForLookup=1",slots.size());
}
void VerifyMatchedPrefab(PendingPrefab proof){
 auto source=proof.source.Get();auto exact=proof.mesh;auto id=proof.id;auto hits=proof.hits;
 const auto& path=proof.path;const auto& boundary=proof.boundary;auto begin=proof.begin;auto timing=proof.timing;
 const char* phase="asset-ready";
 try{
  struct Guard {Guard(){loading=true;}~Guard(){loading=false;constructing=false;}} guard;
  Require(RuntimeShapes::Alive(source)&&FullResourceReload::Id(source)==id&&RuntimeShapes::Alive(exact.Get()),"pending proof source identity retired");
  Require(AuthorAssetsReady(),"pending proof independent assets not ready");
  auto stage=ProbeClock::now();phase="inactive-clone";
  auto holder=Ref::Make(il2cpp_object_new(api.goClass),false);auto name=Ref::Make(il2cpp_string_new("EFF inactive adaptation validation"),false);void* ctor[]={name.Get()};Call(api.goCtor,holder.Get(),ctor);
  struct DestroyHolder {Ref object;void Destroy(){if(object.Get()){void* args[]={object.Get()};Call(api.destroyImmediate,nullptr,args);object={};}}~DestroyHolder(){try{Destroy();}catch(...){}}} cleanup{holder};
  unsigned char inactive=0;void* activeArgs[]={&inactive};Call(api.setActive,holder.Get(),activeArgs);
  auto parent=Ref::Make(Call(FullResourceReload::api.goTransform,holder.Get()),false);unsigned char world=0;void* cloneArgs[]={source,parent.Get(),&world};
  initializedDuringClone=0;constructing=true;auto candidate=Ref::Make(Call(api.cloneParent,nullptr,cloneArgs),false);
  Require(candidate.Get()&&!Scalar<unsigned char>(api.activeHierarchy,candidate.Get())&&!initializedDuringClone,
          "inactive candidate initialized or became active before adaptation");
  timing.clone=Elapsed(stage);stage=ProbeClock::now();phase="mesh-bone-binding";size_t rebound=0;
  for(const auto& entry:FullResourceReload::Components(candidate.Get())){
   auto renderer=entry.Get();if(RuntimeShapes::SourceMesh(renderer)!=exact.Get())continue;
   auto klass=il2cpp_object_get_class(renderer);Require(klass&&FullResourceReload::Inherits(renderer,"UnityEngine","SkinnedMeshRenderer"),
           "first adaptation proof requires a SkinnedMeshRenderer");
   auto bonesBefore=Ref::Make(Call(RuntimeShapes::api.bones,renderer),false);
   auto palette=FullResourceReload::ReferenceArray(bonesBefore.Get());
   Require(palette.size()==size_t(config.bones),"first adaptation proof donor bone palette differs from author");
   auto root=Ref::Make(Call(RuntimeShapes::api.rootBone,renderer),false);
   Require(RuntimeShapes::Alive(root.Get()),"inactive candidate has no skin root");
   VerifyBonePaths(candidate.Get(),renderer,palette,root.Get());
   void* meshArgs[]={authorMesh.Get()};Call(api.setSkinMesh,renderer,meshArgs);
   auto bonesAfter=Ref::Make(Call(RuntimeShapes::api.bones,renderer),false);
   Require(RuntimeShapes::SourceMesh(renderer)==authorMesh.Get()&&FullResourceReload::ReferenceArray(bonesAfter.Get())==palette&&Call(RuntimeShapes::api.rootBone,renderer)==root.Get(),
           "inactive candidate Mesh assignment altered bone references");
   ++rebound;
  }
  Require(rebound==hits,"inactive candidate did not bind every matched Renderer");
  timing.binding=Elapsed(stage);stage=ProbeClock::now();phase="texture-material";
  VerifyTextureAndMaterial(candidate.Get());
  timing.appearance=Elapsed(stage);stage=ProbeClock::now();phase="source-check";
  // The proof changes only this disposable inactive copy. It never routes
  // the live model; arbitrary bone remapping/0-N parts remain separate work.
  for(const auto& entry:FullResourceReload::Components(source))Require(RuntimeShapes::SourceMesh(entry.Get())!=authorMesh.Get(),
           "inactive candidate assignment leaked into the game source Prefab");
  Require(FullResourceReload::Id(source)==id,"adaptation probe source resource changed");
  Require(!initializedDuringClone,"inactive candidate initialized consumers during adaptation");
  timing.sourceCheck=Elapsed(stage);stage=ProbeClock::now();phase="cleanup";
  cleanup.Destroy();candidate={};parent={};holder={};constructing=false;
  timing.cleanup=Elapsed(stage);timing.total=Elapsed(begin);timings[id]=timing;verifiedRoots[id]=true;
  Log("[ADAPT-PROBE-TIMING] prefab=%s lookupMs=%.3f discoveryMs=%.3f authorSubmitMs=%.3f inactiveCloneMs=%.3f bindingMs=%.3f textureMaterialMs=%.3f sourceCheckMs=%.3f cleanupMs=%.3f asyncWaitMs=%.3f totalWallMs=%.3f verificationDeferred=%d sharedNativeAssets=1 productionHotPathMeasured=0",
   path.c_str(),timing.lookup,timing.discovery,timing.authorLoad,timing.clone,timing.binding,timing.appearance,timing.sourceCheck,timing.cleanup,timing.wait,timing.total,int(timing.deferred));
  Log("[ADAPT-PROBE-PASS] prefab=%s elapsedMs=%.3f independentMeshLoaded=1 inactiveCandidate=1 candidateMeshBindings=%zu donorBoneReferencesPreserved=1 helperInitializationCalls=%u liveModelChanged=0 routesPublished=0 verificationDeferred=%d actualInstanceDeliveryDelayed=0 fullAssemblyVerified=0",
   path.c_str(),timing.total,rebound,initializedDuringClone,int(timing.deferred));
 }catch(const std::exception& error){if(matchedRoots.count(id))verifiedRoots[id]=false;Log("[ADAPT-PROBE-FAILED] prefab=%s boundary=%s phase=%s reason=%s liveModelPreserved=1",path.c_str(),boundary.c_str(),phase,error.what());}
}
void ObservePrefab(void* source,const std::string& path,const char* boundary){
 if(!enabled||loading||!RuntimeShapes::Main()||!FullResourceReload::Inherits(source,"UnityEngine","GameObject"))return;
 int id=0;
 try{
  id=FullResourceReload::Id(source);auto prior=observed.find(id);
  if(prior!=observed.end()&&prior->second.Get()==source)return;
  matchedRoots.erase(id);timings.erase(id);pendingPrefabs.erase(id);if(prior!=observed.end())verifiedRoots.erase(id);
  auto begin=ProbeClock::now();Timing timing;Ref exact;
  {struct Guard{Guard(){loading=true;}~Guard(){loading=false;}}guard;exact=ExactLoadedMesh();}
  if(!exact.Get())return;timing.lookup=Elapsed(begin);
  auto stage=ProbeClock::now();RequestAuthorAssets();timing.authorLoad=Elapsed(stage);
  stage=ProbeClock::now();observed[id]=Ref::Make(source,true);
  size_t hits=0;for(const auto& r:FullResourceReload::Components(source))if(RuntimeShapes::SourceMesh(r.Get())==exact.Get())++hits;
  timing.discovery=Elapsed(stage);if(!hits)return;
  matchedRoots[id]=hits;
  Log("[ADAPT-PROBE-MATCH] prefab=%s boundary=%s renderers=%zu exactNativeMeshReference=1 sourceId=%d",path.c_str(),boundary,hits,id);
  if(verifiedRoots.count(id))return;
  PendingPrefab proof{observed.at(id),Ref::Make(exact.Get(),true),id,path,boundary,hits,begin,timing};
  if(AuthorAssetsReady())VerifyMatchedPrefab(std::move(proof));
  else{proof.timing.deferred=true;pendingPrefabs[id]=std::move(proof);Log("[ADAPT-PROBE-AWAIT] prefab=%s asynchronousAuthorAssets=1 gameFlowPreserved=1 actualInstanceDeliveryDelayed=0",path.c_str());}
 }catch(const std::exception& error){if(matchedRoots.count(id))verifiedRoots[id]=false;Log("[ADAPT-PROBE-FAILED] prefab=%s boundary=%s phase=observe reason=%s liveModelPreserved=1",path.c_str(),boundary,error.what());}
}
void AdvancePending(){
 if(!enabled||loading||(assetLoads.PendingCount()==0&&pendingPrefabs.empty())||!RuntimeShapes::Main())return;
 auto begin=ProbeClock::now();assetLoads.Advance();
 bool ready=false;
 try{ready=AuthorAssetsReady();}
 catch(const std::exception& error){
  for(const auto& item:pendingPrefabs){verifiedRoots[item.first]=false;Log("[ADAPT-PROBE-FAILED] prefab=%s phase=async-assets reason=%s liveModelPreserved=1",item.second.path.c_str(),error.what());}
  pendingPrefabs.clear();return;
 }
 if(!ready)return;
 while(!pendingPrefabs.empty()&&Elapsed(begin)<4){
  auto item=pendingPrefabs.begin();auto proof=std::move(item->second);pendingPrefabs.erase(item);
  if(!RuntimeShapes::Alive(proof.source.Get())||!RuntimeShapes::Alive(proof.mesh.Get())){
   Log("[ADAPT-PROBE-RETIRED] prefab=%s pendingValidationCancelled=1",proof.path.c_str());continue;
  }
  proof.timing.wait=std::max(0.0,Elapsed(proof.begin)-proof.timing.lookup-proof.timing.discovery-proof.timing.authorLoad);
  VerifyMatchedPrefab(std::move(proof));
 }
}
void ObserveHandle(void* handle,void* source){
 if(!enabled||loading||!FullResourceReload::Inherits(source,"UnityEngine","GameObject"))return;
 try{
  auto boxed=Ref::Make(il2cpp_value_box(FullResourceReload::api.handleClass,handle),false);uint32_t alignment=0;
  auto size=il2cpp_class_value_size(FullResourceReload::api.handleUntrackedClass,&alignment);Require(size>0,"adaptation probe resource handle layout absent");
  Bytes untracked(size);il2cpp_field_get_value(boxed.Get(),FullResourceReload::api.handleUntrackedField,untracked.data());
  if(!FullResourceReload::Inherits(Call(FullResourceReload::api.untrackedAssetGet,untracked.data()),"","AssetProxy"))return;
  ObservePrefab(source,String(Call(FullResourceReload::api.handlePath,handle)),"resource-handle");
 }catch(const std::exception& error){Log("[ADAPT-PROBE-FAILED] phase=handle reason=%s liveModelPreserved=1",error.what());}
}
void ObserveProxy(void* source,uint64_t hash,void* asset=nullptr){
 if(!enabled||loading)return;
 std::string path="hash/"+std::to_string(hash);
 try{if(asset&&api.proxyInfo&&api.assetPath){auto boxed=Ref::Make(Call(api.proxyInfo,asset),false);
   Require(boxed.Get()&&il2cpp_object_get_class(boxed.Get())==api.assetInfoClass,"Prefab proxy returned another AssetInfoPack structure");
   path=String(Call(api.assetPath,il2cpp_object_unbox(boxed.Get())));}}
 catch(const std::exception& error){Log("[ADAPT-PROBE-FAILED] phase=proxy-path reason=%s",error.what());}
 ObservePrefab(source,path,"prefab-instantiate-proxy");
}
void InstallOptional(void** assemblies,size_t count){
 try{
  auto configurationFile=gameDirectory/"plugin/resource-adaptation-probe/probe.tsv";if(!std::filesystem::is_regular_file(configurationFile))return;
  auto raw=EiemDirectory::Read(configurationFile);std::istringstream lines(std::string(raw.begin(),raw.end()));std::string line;
  Require(bool(std::getline(lines,line))&&line=="EFF_PREFAB_ADAPTATION_PROBE/mesh+source+layout/tsv","adaptation probe configuration header differs");
  Require(bool(std::getline(lines,line)),"adaptation probe configuration absent");auto fields=EiemNativeInput::TabFields(line);Require(fields.size()==8,"adaptation probe configuration fields differ");
  auto relative=std::filesystem::u8path(fields[0]);Require(!relative.is_absolute()&&relative.parent_path().empty(),"adaptation probe asset must be beside configuration");
  config.bytes=std::make_shared<const Bytes>(EiemDirectory::Read(configurationFile.parent_path()/relative));auto hash=Sha(config.bytes->data(),config.bytes->size());Require(Hex(hash.data(),hash.size())==fields[7],"adaptation probe asset digest differs");config.digest=fields[7];
  config.path=fields[1];config.name=fields[2];config.marker=fields[3];config.vertices=std::stoi(fields[4]);config.bones=std::stoi(fields[5]);config.shapes=std::stoi(fields[6]);
  // Subasset FBX selection is a separate required proof, never a first-Mesh
  // guess. This acceptance run intentionally uses a primary .asset Mesh.
  Require(config.path.size()>6&&config.path.substr(config.path.size()-6)==".asset"&&config.vertices>0&&config.bones>=0&&config.shapes>=0,
          "adaptation probe expects a primary Mesh asset and valid native counts");
  Require(FullResourceReload::enabled&&StartupDependencies::enabled,"adaptation probe required game bindings unavailable");
  auto manifest=FindClass("Beyond.ManifestBinary","RuntimeManifestBinary",assemblies,count);BindLookup(manifest);
  auto packType=StartupDependencies::TypeName(il2cpp_class_get_type(api.assetInfoClass));
  auto packRef=packType+"&";api.subAssetInfo=FullResourceReload::Method(manifest,"TryGetAssetInfoFromPath",{"System.String",packRef.c_str(),"System.String"},"System.Boolean",false);
  auto assetField=StartupDependencies::Field(api.assetInfoClass,"assetData");auto assetClass=il2cpp_class_from_type(il2cpp_field_get_type(assetField));
  api.assetHashOffset=StartupDependencies::ValueOffset(api.assetInfoClass,"assetData",StartupDependencies::ValueSize(assetClass))+
                      StartupDependencies::ValueOffset(assetClass,"pathHashHead",sizeof(uint64_t));
  Require(api.assetHashOffset<=api.assetSize&&sizeof(uint64_t)<=api.assetSize-api.assetHashOffset,"asset path hash escapes returned pack");
  auto assetProxy=FindClass("","AssetProxy",assemblies,count);
  api.proxyInfo=FullResourceReload::Method(assetProxy,"GetAssetInfo",{},packType.c_str(),false);
  api.assetPath=FullResourceReload::Method(api.assetInfoClass,"get_assetPath",{},"System.String",false);
  auto proxy=FindClass("","BundleProxy",assemblies,count),bundle=FindClass("UnityEngine","AssetBundle",assemblies,count);
  api.bundleGetter=FullResourceReload::Method(proxy,"get_assetBundle",{},"UnityEngine.AssetBundle",false);
  api.loadMesh=FullResourceReload::Method(bundle,"LoadAsset",{"System.String","System.Type"},"UnityEngine.Object",false);
  api.loadMeshHash=FullResourceReload::Method(bundle,"LoadAsset",{"System.Int64","System.Type"},"UnityEngine.Object",false);
  auto mesh=FindClass("UnityEngine","Mesh",assemblies,count);meshType=FullResourceReload::Type(mesh);
  api.goClass=FindClass("UnityEngine","GameObject",assemblies,count);auto object=FindClass("UnityEngine","Object",assemblies,count);
  api.goCtor=FullResourceReload::Method(api.goClass,".ctor",{"System.String"},"System.Void",false);
  api.setActive=FullResourceReload::Method(api.goClass,"SetActive",{"System.Boolean"},"System.Void",false);
  api.activeHierarchy=FullResourceReload::Method(api.goClass,"get_activeInHierarchy",{},"System.Boolean",false);
  api.cloneParent=FullResourceReload::Method(object,"Instantiate",{"UnityEngine.Object","UnityEngine.Transform","System.Boolean"},"UnityEngine.Object",true);
  api.destroyImmediate=FullResourceReload::Method(object,"DestroyImmediate",{"UnityEngine.Object"},"System.Void",true);
  api.setSkinMesh=FullResourceReload::Method(FindClass("UnityEngine","SkinnedMeshRenderer",assemblies,count),"set_sharedMesh",{"UnityEngine.Mesh"},"System.Void",false);
  sampleRoot=configurationFile.parent_path()/"sample";
  if(std::filesystem::is_regular_file(sampleRoot/"assets.bin")){
   sampleDirectory=EiemRuntimeAuthorDirectory::Decode(EiemDirectory::Read(sampleRoot/"assets.bin"));
   auto selected=std::find_if(sampleDirectory->textures.begin(),sampleDirectory->textures.end(),[](const auto& t){return std::filesystem::is_regular_file(sampleRoot/std::filesystem::u8path(t.file));});
   Require(selected!=sampleDirectory->textures.end(),"independent Texture acceptance sample absent");sampleTexture=*selected;
   textureBytes=std::make_shared<const Bytes>(EiemDirectory::Read(sampleRoot/std::filesystem::u8path(selected->file)));
   auto textureHash=Sha(textureBytes->data(),textureBytes->size());Require(Hex(textureHash.data(),textureHash.size())==selected->hash,"independent Texture acceptance sample changed");
   auto texture=FindClass("UnityEngine","Texture",assemblies,count);textureType=FullResourceReload::Type(FindClass("UnityEngine","Texture2D",assemblies,count));
   api.textureWidth=FullResourceReload::Method(texture,"get_width",{},"System.Int32",false);api.textureHeight=FullResourceReload::Method(texture,"get_height",{},"System.Int32",false);
   api.textureMips=FullResourceReload::Method(texture,"get_mipmapCount",{},"System.Int32",false);
   textureByIdSetter=FullResourceReload::Method(FindClass("UnityEngine","Material",assemblies,count),"SetTexture",{"System.Int32","UnityEngine.Texture"},"System.Void",false);AuthorMaterials::Bind(assemblies,count);
  }
  Require(RuntimeShapes::api.bindPoses,"adaptation probe public bindpose getter unavailable");
  auto helper=FindClass("Beyond.Gameplay.View","EntityRenderHelper",assemblies,count);
  auto cloth=FindClass("BeyondDynamicBone","BeyondBoneCloth",assemblies,count);
  Require(Entry::Install(helper,"OnAwake",{},"System.Void",false,(void*)HelperAwake,(void**)&originalHelperAwake),"adaptation probe helper Awake hook absent");
  Require(Entry::Install(cloth,"Awake",{},"System.Void",false,(void*)ClothAwake,(void**)&originalClothAwake),"adaptation probe cloth Awake hook absent");
  Require(Entry::Install(cloth,"OnEnable",{},"System.Void",false,(void*)ClothEnable,(void**)&originalClothEnable),"adaptation probe cloth OnEnable hook absent");
  enabled=true;Log("[ADAPT-PROBE-READY] path=%s asset=%s exactManifestLookup=1 observedPrefabsOnly=1 frameDiscovery=0 pendingAsyncWorkOnly=1 assetBytes=%zu perPrefabValidation=1 splitTimings=1 asyncSharedAssets=1",config.path.c_str(),config.name.c_str(),config.bytes->size());
 }catch(const std::exception& error){Log("[ADAPT-PROBE-FAILED] phase=bind reason=%s liveModelPreserved=1",error.what());}
}
} // namespace PrefabAdaptationProbe
