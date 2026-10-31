#pragma once
#include "eiem_entry_native_author_resources.h"
#include "eiem_prefab_bone_binding.h"

// First real-delivery acceptance step of the Mesh-target backend. Deliberately
// scoped to chen's one-Part body: do not claim 0/N expansion, new bones,
// physics/effects, async completion gating or F10 have passed this test.
namespace MeshNativeAcceptance {
using RuntimeShapes::Ref;using RuntimeShapes::Call;using RuntimeShapes::Scalar;
using Clock=std::chrono::steady_clock;namespace Payload=EiemRuntimePayload;
struct Api {
 void *goClass=nullptr,*ctor=nullptr,*setActive=nullptr,*active=nullptr,*keep=nullptr;
 void *setMesh=nullptr,*setBones=nullptr,*setMaterials=nullptr,*setBounds=nullptr,*arraySet=nullptr;
} api;
bool enabled=false;thread_local bool constructing=false;unsigned initialized=0;
inline double Ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
inline uint64_t PrivateBytes(){using Fn=BOOL(WINAPI*)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD);static auto fn=(Fn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"K32GetProcessMemoryInfo");PROCESS_MEMORY_COUNTERS_EX m{};m.cb=sizeof(m);return fn&&fn(GetCurrentProcess(),(PPROCESS_MEMORY_COUNTERS)&m,sizeof(m))?uint64_t(m.PrivateUsage):0;}
struct Input {
 EiemMeshTargetDirectory::Target target;
 std::string part,materialId,textureId;
 std::filesystem::path mesh,material,texture;
 EiemDirectory::Identity donor;
 Settings textureSettings;
};
struct Prepared {
 Input input;
 Resource meshResource,materialResource,textureResource;
 Payload::Mesh mesh;Payload::Texture texture;
 size_t bytes=0;double readMs=0;
};
struct Shared {
 Ref mesh,material,texture;
 std::vector<std::string> bones;
 std::array<float,6> bounds{};
 unsigned vertices=0,shapes=0;
 uint64_t bytes=0;
};
std::optional<Input> input;
std::future<std::shared_ptr<Prepared>> preparation;
std::shared_ptr<Shared> shared;
Ref materialType;
std::string resourceFailure;
struct Match {Ref renderer,mesh;};
struct Source {
 Ref source,holder,candidate;int id=0,candidateId=0;
 std::string path,error;std::vector<Match> matches;
 unsigned deliveries=0;
};
std::map<void*,Source> sources;
using LifecycleFn=void(*)(void*,void*);
LifecycleFn helperAwake=nullptr,clothAwake=nullptr,clothEnable=nullptr;
inline void Initializing(const char* phase){if(constructing){++initialized;Log("[MESH-NATIVE-EARLY-INIT] phase=%s beforeAssemblyComplete=1",phase);}}
void HelperAwake(void* self,void* info){Initializing("EntityRenderHelper.OnAwake");helperAwake(self,info);}
void ClothAwake(void* self,void* info){Initializing("BeyondBoneCloth.Awake");clothAwake(self,info);}
void ClothEnable(void* self,void* info){Initializing("BeyondBoneCloth.OnEnable");clothEnable(self,info);}
// The native body acceptance uses exactly the existing declared source Material
// and BaseMap. It must not borrow a merely same-named Material from another Mod.
inline Input ReadInput(const EiemMeshTargetDirectory::Target& target){
 Require(target.resources.size()==2&&target.partIds.size()==1,"native body test requires one complete Part");Input i;i.target=target;i.part=target.partIds[0];i.mesh=target.resources[0];i.material=target.resources[1];
 auto sections=ParseIni(EiemDirectory::Read(target.root/"mod.ini"));std::map<std::string,Settings> values;for(const auto& section:sections)values.emplace(section.id,section.values);
 i.materialId=values.at(i.part).at("material");const auto& material=values.at(i.materialId);i.donor=EiemDirectory::Source(material,"source.");
 Require(material.size()==4&&material.count("texture._BaseMap"),"first body acceptance requires only its declared BaseMap override");i.textureId=material.at("texture._BaseMap");i.textureSettings=values.at(i.textureId);
 auto relative=std::filesystem::u8path(i.textureSettings.at("path"));Require(!relative.is_absolute()&&!relative.has_root_name(),"author texture path is absolute");for(const auto& segment:relative)Require(segment!="..","author texture escapes Mod");i.texture=target.root/relative;return i;
}
inline std::shared_ptr<Prepared> ReadPayload(Input i){
 auto start=Clock::now();auto p=std::make_shared<Prepared>();p->input=std::move(i);
 p->meshResource=Decode(EiemDirectory::Read(p->input.mesh),1);p->mesh=Payload::DecodeMesh(p->meshResource);
 p->materialResource=Decode(EiemDirectory::Read(p->input.material),2);Require(p->materialResource.blocks.empty(),"first body acceptance requires an unchanged donor Material");
 p->textureResource=Decode(EiemDirectory::Read(p->input.texture),4);p->texture=Payload::DecodeTexture(p->textureResource,p->input.textureSettings);
 p->bytes=ResourceSize(p->meshResource)+ResourceSize(p->materialResource)+ResourceSize(p->textureResource);p->readMs=Ms(start);return p;
}
void Matched(void* source,void* renderer,void* exact,const EiemMeshTargetDirectory::Target& target,const std::string& path){
 if(!enabled||constructing||target.mod!="chen"||target.rule!="ReplaceS_actor_chen_body_01_lod0")return;
 auto found=sources.find(source);if(found!=sources.end()&&(found->second.source.Get()!=source||found->second.id!=FullResourceReload::Id(source))){NativeAuthorResources::Destroy(found->second.holder);sources.erase(found);}
 auto& record=sources[source];if(!record.source.Get()){record.source=Ref::Make(source,true);record.id=FullResourceReload::Id(source);record.path=path;}
 for(const auto& match:record.matches)if(match.renderer.Get()==renderer)return;
 record.matches.push_back({Ref::Make(renderer,true),Ref::Make(exact,true)});
 if(!input&&!preparation.valid()&&!shared&&resourceFailure.empty()){
  input=ReadInput(target);auto captured=*input;preparation=std::async(std::launch::async,[captured]{return ReadPayload(captured);});
  Log("[MESH-NATIVE-READ-SUBMITTED] mod=%s part=%s actualMatchingPrefab=1 workerDiskOnly=1 payloadReadsAtStartup=0",target.mod.c_str(),input->part.c_str());
 }
}
inline Ref ReferenceArray(void* klass,const std::vector<void*>& values){
 auto array=Ref::Make(il2cpp_array_new(klass,values.size()),false);Require(array.Get(),"native reference array allocation failed");
 for(size_t i=0;i<values.size();++i){int index=int(i);void* args[]={values[i],&index};Call(api.arraySet,array.Get(),args);}return array;
}
inline std::shared_ptr<Shared> LoadResources(void* sourceRenderer){
 if(shared)return shared;Require(resourceFailure.empty(),"native author resource preparation previously failed: "+resourceFailure);Require(preparation.valid()&&input,"author body input not requested");auto start=Clock::now();auto before=PrivateBytes();
 try{
  // This acceptance measures a synchronous first-use wait on OUR disk-only
  // worker. It never waits on the game's resource queue. Production async
  // delivery gating is a separate acceptance step, not claimed here.
  auto p=preparation.get();auto waitMs=Ms(start);auto build=Clock::now();auto result=std::make_shared<Shared>();
  struct Cleanup {std::shared_ptr<Shared> data;bool keep=false;~Cleanup(){if(!keep){NativeAuthorResources::Destroy(data->material);NativeAuthorResources::Destroy(data->texture);NativeAuthorResources::Destroy(data->mesh);}}} cleanup{result};
  result->mesh=NativeAuthorResources::CreateMesh(p->mesh,"EFF native chen body acceptance");double meshMs=Ms(build);build=Clock::now();
  result->texture=NativeAuthorResources::CreateTexture(p->texture,"EFF native chen body BaseMap");double textureMs=Ms(build);build=Clock::now();
  (void)sourceRenderer;auto donor=Ref::Make(MeshTargetProbe::LoadedAsset(input->donor,materialType.Get()),false);Require(donor.Get(),"exact declared donor Material is not loaded");
  EiemMaterialOperations::Edits edit;result->material=AuthorMaterials::Create(donor.Get(),edit,{{"_BaseMap",result->texture}});
  result->bones=p->mesh.bones;result->bounds=p->mesh.bounds;result->vertices=p->mesh.vertices;result->shapes=unsigned(p->mesh.shapes.size());result->bytes=p->texture.pixels.size+p->mesh.vertexData.size+p->mesh.indexData.size;
  Log("[MESH-NATIVE-RESOURCES] mod=chen part=%s readDecodeMs=%.3f firstUseDiskWaitMs=%.3f meshCreateVerifyMs=%.3f textureCreateUploadMs=%.3f materialCreateMs=%.3f wholeStageMs=%.3f authorBytesRead=%zu retainedAuthorFileBytes=0 texturePixelBytes=%zu meshVertexBytes=%zu processPrivateBefore=%llu processPrivateNow=%llu wholeProcessSample=1 meshId=%d materialId=%d textureId=%d sharedAcrossPrefabSources=1 compilerInvoked=0 consumerPfbReads=0 asyncDeliveryGateImplemented=0",
   input->part.c_str(),p->readMs,waitMs,meshMs,textureMs,Ms(build),Ms(start),p->bytes,p->texture.pixels.size,p->mesh.vertexData.size,(unsigned long long)before,(unsigned long long)PrivateBytes(),FullResourceReload::Id(result->mesh.Get()),FullResourceReload::Id(result->material.Get()),FullResourceReload::Id(result->texture.Get()));
  cleanup.keep=true;shared=result;return result;
 }catch(const std::exception& e){resourceFailure=e.what();throw;}
}
struct Hierarchy {
 std::vector<EiemPrefabBoneBinding::Node> nodes;std::vector<Ref> transforms;std::map<void*,size_t> indices;
 void Walk(void* transform,size_t parent){auto id=nodes.size();Require(indices.emplace(transform,id).second,"native candidate Transform cycle");nodes.push_back({parent,String(Call(RuntimeShapes::api.name,transform))});transforms.push_back(Ref::Make(transform,false));int count=Scalar<int>(FullResourceReload::api.childCount,transform);Require(count>=0,"native candidate child count differs");for(int i=0;i<count;++i){void* args[]={&i};Walk(Call(FullResourceReload::api.getChild,transform,args),id);}}
};
inline std::array<float,6> RendererBounds(void* renderer,void* root,const std::array<float,6>& bounds){
 auto readMatrix=[](void* method,void* object){std::array<float,16> m{};auto box=Ref::Make(Call(method,object),false);Require(box.Get()&&CopyRaw(il2cpp_object_unbox(box.Get()),m.data(),64),"native bounds matrix read failed");return m;};
 auto local=readMatrix(RuntimeShapes::api.localToWorld,Call(RuntimeShapes::api.transform,renderer));auto inverse=readMatrix(RuntimeShapes::api.worldToLocal,root);std::array<float,16> matrix{};
 for(size_t c=0;c<4;++c)for(size_t r=0;r<4;++r)for(size_t k=0;k<4;++k)matrix[c*4+r]+=inverse[k*4+r]*local[c*4+k];std::array<float,6> out{};
 for(size_t r=0;r<3;++r){out[r]=matrix[12+r];for(size_t c=0;c<3;++c){out[r]+=matrix[c*4+r]*bounds[c];out[3+r]+=std::abs(matrix[c*4+r])*bounds[3+c];}}return out;
}
void Assemble(Source& record){
 auto source=record.source.Get();Require(RuntimeShapes::Alive(source)&&FullResourceReload::Id(source)==record.id,"native source retired before assembly");Require(!record.matches.empty(),"native candidate lacks exact target matches");
 auto assets=LoadResources(record.matches.front().renderer.Get());auto begin=Clock::now();auto privateBefore=PrivateBytes();Ref holder;
 struct Cleanup {Ref& holder;bool keep=false;~Cleanup(){if(!keep)NativeAuthorResources::Destroy(holder);}} cleanup{holder};
 struct Constructing {Constructing(){constructing=true;initialized=0;}~Constructing(){constructing=false;}} guard;
 holder=Ref::Make(il2cpp_object_new(api.goClass),false);auto label=Ref::Make(il2cpp_string_new("EFF native isolated templates"),false);void* ctor[]={label.Get()};Call(api.ctor,holder.Get(),ctor);unsigned char inactive=0;void* off[]={&inactive};Call(api.setActive,holder.Get(),off);void* retain[]={holder.Get()};Call(api.keep,nullptr,retain);
 auto parent=Ref::Make(Call(FullResourceReload::api.goTransform,holder.Get()),false);auto candidate=Ref::Make(FullResourceReload::originalCloneParent(source,parent.Get(),false),false);Require(RuntimeShapes::Alive(candidate.Get())&&!Scalar<unsigned char>(api.active,candidate.Get())&&!initialized,"native candidate initialized before assembly");
 NativeAuthorResources::Name(candidate.Get(),String(Call(RuntimeShapes::api.name,source)));auto cloneMs=Ms(begin);auto bind=Clock::now();
 Hierarchy hierarchy;hierarchy.Walk(Call(FullResourceReload::api.goTransform,candidate.Get()),SIZE_MAX);EiemPrefabBoneBinding::Tree tree(hierarchy.nodes);size_t replaced=0;
 for(const auto& r:FullResourceReload::Components(candidate.Get())){
  auto renderer=r.Get();auto original=RuntimeShapes::SourceMesh(renderer);bool exact=false;for(const auto& match:record.matches)exact|=match.mesh.Get()==original;if(!exact)continue;
  Require(FullResourceReload::Inherits(renderer,"UnityEngine","SkinnedMeshRenderer"),"native body candidate is not a skin Renderer");auto root=Ref::Make(Call(RuntimeShapes::api.rootBone,renderer),false);auto transform=Call(RuntimeShapes::api.transform,renderer);Require(root.Get()&&hierarchy.indices.count(root.Get())&&hierarchy.indices.count(transform),"native body skeleton is outside candidate");
  auto slots=tree.Resolve(hierarchy.indices.at(transform),hierarchy.indices.at(root.Get()),assets->bones);std::vector<void*> bones;for(auto slot:slots)bones.push_back(hierarchy.transforms.at(slot).Get());auto palette=ReferenceArray(RuntimeShapes::api.transformClass,bones);
  auto materials=ReferenceArray(AuthorMaterials::api.klass,{assets->material.Get()});auto bounds=RendererBounds(renderer,root.Get(),assets->bounds);void* boneArgs[]={palette.Get()},*meshArgs[]={assets->mesh.Get()},*materialArgs[]={materials.Get()},*boundsArgs[]={bounds.data()};
  Call(api.setBones,renderer,boneArgs);Call(api.setMesh,renderer,meshArgs);Call(api.setMaterials,renderer,materialArgs);Call(api.setBounds,renderer,boundsArgs);
  Require(RuntimeShapes::SourceMesh(renderer)==assets->mesh.Get()&&FullResourceReload::ReferenceArray(Call(RuntimeShapes::api.bones,renderer))==bones&&Call(RuntimeShapes::api.rootBone,renderer)==root.Get(),"native body candidate binding readback differs");++replaced;
 }
 Require(replaced==record.matches.size()&&!initialized,"native body candidate coverage/initialization differs");
 for(const auto& match:record.matches)Require(RuntimeShapes::SourceMesh(match.renderer.Get())==match.mesh.Get(),"native author binding leaked into source resource");
 record.holder=holder;record.candidate=candidate;record.candidateId=FullResourceReload::Id(candidate.Get());cleanup.keep=true;
 Log("[MESH-NATIVE-ASSEMBLED] prefab=%s sourceId=%d templateId=%d changedRenderers=%zu cloneMs=%.3f bindVerifyMs=%.3f totalAssemblyMs=%.3f processPrivateBefore=%llu processPrivateNow=%llu wholeProcessSample=1 earlyInitialization=%u sourceResourcePreserved=1 oneTemplatePerLoadedSource=1 meshSharedId=%d fullMultiPartAssembly=0 physicsAdapted=0 effectsAdapted=0",
  record.path.c_str(),record.id,record.candidateId,replaced,cloneMs,Ms(bind),Ms(begin),(unsigned long long)privateBefore,(unsigned long long)PrivateBytes(),initialized,FullResourceReload::Id(assets->mesh.Get()));
}
void* Route(void* source,const char* boundary) noexcept {
 if(!enabled||constructing||!RuntimeShapes::Main())return source;
 auto found=sources.find(source);if(found==sources.end())return source;auto& record=found->second;
 try{
  if(record.source.Get()!=source||!RuntimeShapes::Alive(source)||FullResourceReload::Id(source)!=record.id)return source;
  if(!record.error.empty())return source;if(!record.candidate.Get())Assemble(record);
  Require(RuntimeShapes::Alive(record.candidate.Get())&&FullResourceReload::Id(record.candidate.Get())==record.candidateId,"native isolated template retired");
  if(!record.deliveries++)Log("[MESH-NATIVE-DELIVERY] prefab=%s sourceId=%d templateId=%d factory=%s replacementAppliedBeforeInstantiate=1 templateActiveInHierarchy=%d",record.path.c_str(),record.id,record.candidateId,boundary,int(Scalar<unsigned char>(api.active,record.candidate.Get())));
  return record.candidate.Get();
 }catch(const std::exception& e){record.error=e.what();Log("[MESH-NATIVE-FAILED] prefab=%s phase=assembly-or-resource reason=%s originalGameSourceReturned=1",record.path.c_str(),e.what());return source;}
}
void Consumer(void* list,const char* boundary) noexcept {
 if(constructing){Initializing(boundary);return;}if(!enabled||!shared||!list)return;
 try{auto klass=il2cpp_object_get_class(list);void* array=nullptr;int count=0;il2cpp_field_get_value(list,StartupDependencies::Field(klass,"_items"),&array);il2cpp_field_get_value(list,StartupDependencies::Field(klass,"_size"),&count);size_t capacity=0;Require(count>=0&&ArrayLength(array,&capacity)&&size_t(count)<=capacity,"native consumer array range differs");size_t hits=0;
  for(int i=0;i<count;++i){void* renderer=nullptr;Require(CopyRaw((char*)array+32+size_t(i)*sizeof(void*),&renderer,sizeof(renderer)),"native consumer Renderer array unreadable");if(!RuntimeShapes::Alive(renderer)||!RuntimeShapes::RendererKind(renderer)||RuntimeShapes::SourceMesh(renderer)!=shared->mesh.Get())continue;++hits;auto materials=FullResourceReload::ReferenceArray(Call(FullResourceReload::api.materials,renderer));Require(materials.size()==1&&materials[0]==shared->material.Get(),"game consumer replaced authored Material during initialization");}
  if(hits)Log("[MESH-NATIVE-CONSUMER] boundary=%s authorRenderers=%zu nativeMeshId=%d authorMaterialPreserved=1 actualGameConsumer=1",boundary,hits,FullResourceReload::Id(shared->mesh.Get()));
 }catch(const std::exception& e){Log("[MESH-NATIVE-FAILED] phase=actual-consumer reason=%s",e.what());}
}
void Frame(int frame) noexcept {
 if(!enabled||constructing||frame%300)return;
 try{
 for(auto it=sources.begin();it!=sources.end();)if(!RuntimeShapes::Alive(it->second.source.Get())){NativeAuthorResources::Destroy(it->second.holder);it=sources.erase(it);}else ++it;
 size_t templates=0,delivered=0;for(const auto& r:sources){templates+=bool(r.second.candidate.Get());delivered+=r.second.deliveries;}
 Log("[MESH-NATIVE-STATS] frame=%d templates=%zu deliveries=%zu nativeMeshObjects=%d nativeTextureObjects=%d sourceScoped=1 authorPayloadsEagerlyLoadedAtStartup=0 resourceFailure=%s",frame,templates,delivered,int(bool(shared)),int(bool(shared)),resourceFailure.c_str());
 }catch(const std::exception& e){Log("[MESH-NATIVE-FAILED] phase=retirement reason=%s",e.what());}
}
void Install(void** assemblies,size_t count){try{
 Require(FullResourceReload::enabled&&StartupDependencies::enabled&&MeshTargetProbe::enabled,"native delivery requires game loading and exact matching hooks");
 NativeAuthorResources::Bind(assemblies,count);materialType=FullResourceReload::Type(AuthorMaterials::api.klass);auto object=FindClass("UnityEngine","Object",assemblies,count);api.goClass=FindClass("UnityEngine","GameObject",assemblies,count);auto skin=FindClass("UnityEngine","SkinnedMeshRenderer",assemblies,count);auto renderer=FindClass("UnityEngine","Renderer",assemblies,count);
 api.ctor=FullResourceReload::Method(api.goClass,".ctor",{"System.String"},"System.Void",false);api.setActive=FullResourceReload::Method(api.goClass,"SetActive",{"System.Boolean"},"System.Void",false);api.active=FullResourceReload::Method(api.goClass,"get_activeInHierarchy",{},"System.Boolean",false);api.keep=FullResourceReload::Method(object,"DontDestroyOnLoad",{"UnityEngine.Object"},"System.Void",true);
 api.setMesh=FullResourceReload::Method(skin,"set_sharedMesh",{"UnityEngine.Mesh"},"System.Void",false);api.setBones=FullResourceReload::Method(skin,"set_bones",{"UnityEngine.Transform[]"},"System.Void",false);api.setMaterials=FullResourceReload::Method(renderer,"set_sharedMaterials",{"UnityEngine.Material[]"},"System.Void",false);
 api.setBounds=NativeAuthorResources::LocalBoundsSetter(skin);
 Require(api.setBounds,"native Renderer localBounds setter absent in full inheritance chain");
 api.arraySet=FullResourceReload::Method(FindClass("System","Array",assemblies,count),"SetValue",{"System.Object","System.Int32"},"System.Void",false);
 auto helperClass=FindClass("Beyond.Gameplay.View","EntityRenderHelper",assemblies,count);auto cloth=FindClass("BeyondDynamicBone","BeyondBoneCloth",assemblies,count);
 Require(Entry::Install(helperClass,"OnAwake",{},"System.Void",false,(void*)HelperAwake,(void**)&helperAwake),"native helper initialization guard absent");Require(Entry::Install(cloth,"Awake",{},"System.Void",false,(void*)ClothAwake,(void**)&clothAwake),"native cloth Awake guard absent");Require(Entry::Install(cloth,"OnEnable",{},"System.Void",false,(void*)ClothEnable,(void**)&clothEnable),"native cloth Enable guard absent");
 enabled=true;Log("[MESH-NATIVE-READY] scope=chen-body-single-part realSourceRouting=1 nativeAuthorResourceCreation=1 consumerPfbPrecompile=0 fullFeatureBackend=0 F10Implemented=0");
 }catch(const std::exception& e){enabled=false;Log("[MESH-NATIVE-FAILED] phase=bind reason=%s originalGameSourceReturned=1",e.what());NativeAuthorResources::DescribeAvailableMethods();}}
} // namespace MeshNativeAcceptance
