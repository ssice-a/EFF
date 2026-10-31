#define EIEM_PREFAB_ADAPTATION_PROBE 1
#define main FullResourceFixtureMain
#include "full_resource_reload_smoke.cpp"
#undef main

unsigned prefabComponentQueries=0;
bool candidateMeshAssigned=false;
unsigned candidateDestroyed=0,candidateAssignments=0;
bool initializeDuringBinding=false;
std::vector<uintptr_t> materialCalls;
bool materialPropertyPresent=true;unsigned failedMaterialCleanup=0;
void* CandidateMeshGetter(void* renderer){return renderer==Test::newRenderer&&candidateMeshAssigned?Test::newMesh:Test::oldMesh;}
void* ProbeInvoke(void* method,void* self,void** args,void** exception){
 if(method==(void*)613){Require(!self&&args[0]==Test::candidate,"failed Material cleanup destroyed another object");++failedMaterialCleanup;return nullptr;}
 if(uintptr_t(method)>=600&&uintptr_t(method)<=612){
  auto m=uintptr_t(method);Require(self==Test::candidate,"author Material operation modified its donor");materialCalls.push_back(m);
  if(m==600){Require(args[0]==Test::material,"Material clone used another donor");return nullptr;}
  if(m==601)return materialPropertyPresent?&Test::yes:&Test::no;
  if(m==602)Require(Entry::String(args[0])=="_UseEmission"&&*(float*)args[1]==1,"Material float arguments differ");
  if(m==603)Require(*(int32_t*)args[1]==-2,"Material integer lost signed value");
  if(m==604)Require(((Color*)args[1])->r==1&&((Color*)args[1])->g==0,"Material color layout differs");
  if(m==609)Require(Entry::String(args[0])=="RenderType"&&Entry::String(args[1])=="Transparent","Material tag arguments differ");
  if(m==610)Require(*(int32_t*)args[0]==2999,"Material queue differs");
  if(m==611)Require(Entry::String(args[0])=="_MainTex"&&args[1]==Test::newTexture,"Material texture used author ID instead of object");
  return nullptr;
 }
 if(method==(void*)500){Require(!self&&(args[0]==Test::source||args[0]==Test::unrelatedRoot)&&args[1]==Test::prefabTransform&&!*(unsigned char*)args[2],"inactive clone lost source/parent/space arguments");candidateMeshAssigned=false;return Test::lazyCandidate;}
 if(method==(void*)501){Require(!self&&args[0]==Test::candidate,"destroyed live source rather than disposable holder");++candidateDestroyed;return nullptr;}
 if(method==(void*)502)return nullptr;
 if(method==(void*)503){Require(self==Test::candidate&&!*(unsigned char*)args[0],"holder was not inactive before cloning");return nullptr;}
 if(method==(void*)504)return &Test::no;
 if(method==(void*)505){Require(self==Test::newRenderer&&args[0]==Test::newMesh,"candidate setter modified another Renderer/Mesh");candidateMeshAssigned=true;++candidateAssignments;if(initializeDuringBinding)Entry::PrefabAdaptationProbe::Initializing("fixture binding initialization");return nullptr;}
 if(method==(void*)5)++prefabComponentQueries;
 return Test::Invoke(method,self,args,exception);
}
void MaterialApplicationRegression(){
 namespace M=Entry::AuthorMaterials;Test::Setup();Test::Reset();il2cpp_runtime_invoke=ProbeInvoke;
 auto create=il2cpp_object_new;il2cpp_object_new=[](void*)->void*{return Test::candidate;};
 M::api.klass=Test::gameObjectClass;M::api.clone=(void*)600;M::api.has=(void*)601;M::api.setFloat=(void*)602;M::api.setInteger=(void*)603;M::api.setColor=(void*)604;
 M::api.setScale=(void*)605;M::api.setOffset=(void*)606;M::api.enableKeyword=(void*)607;M::api.disableKeyword=(void*)608;M::api.setTag=(void*)609;M::api.setQueue=(void*)610;M::api.setTexture=(void*)611;M::api.destroy=(void*)613;
 EiemMaterialOperations::Edits entry;entry.floats["_UseEmission"]=1;entry.integers["_Mode"]=-2;entry.colors["_EmissionColor"]={1,0,0,1};
 entry.scales["_MainTex"]={2,1};entry.offsets["_MainTex"]={0.25,0};entry.keywords["_EMISSION"]=true;entry.keywords["_ALPHATEST_ON"]=false;entry.tags["RenderType"]="Transparent";entry.queue=2999;
 std::map<std::string,RuntimeShapes::Ref> textures{{"_MainTex",RuntimeShapes::Ref::Make(Test::newTexture,false)}};materialCalls.clear();
 {auto copy=M::Create(Test::material,entry,textures);Require(copy.Get()==Test::candidate,"private Material copy absent");}
 for(uintptr_t method=600;method<=611;++method)Require(std::find(materialCalls.begin(),materialCalls.end(),method)!=materialCalls.end(),"typed author Material operation was omitted");
 auto empty=EiemMaterialOperations::Edits{};materialCalls.clear();{auto copy=M::Create(Test::material,empty,{});}
 Require(materialCalls==std::vector<uintptr_t>{600},"unmodified Material default was overwritten instead of inherited");
 materialPropertyPresent=false;failedMaterialCleanup=0;bool rejected=false;
 try{M::Create(Test::material,entry,textures);}catch(const std::exception&){rejected=true;}
 Require(rejected&&failedMaterialCleanup==1,"invalid Material edit leaked its private native copy");materialPropertyPresent=true;
 il2cpp_object_new=create;Test::Reset();
}
void CandidateBindingRegression(){
 namespace P=Entry::PrefabAdaptationProbe;Test::Reset();P::observed.clear();P::matchedRoots.clear();P::verifiedRoots.clear();P::timings.clear();P::enabled=true;
 P::authorMesh=RuntimeShapes::Ref::Make(Test::newMesh,false);P::resolvedMesh=RuntimeShapes::Ref::Make(Test::oldMesh,true);
 P::config.bones=1;P::config.vertices=2;P::config.shapes=0;P::config.marker="marker";
 P::api.goClass=Test::gameObjectClass;P::api.goCtor=(void*)502;P::api.setActive=(void*)503;P::api.activeHierarchy=(void*)504;
 P::api.cloneParent=(void*)500;P::api.destroyImmediate=(void*)501;P::api.setSkinMesh=(void*)505;
 RuntimeShapes::originalMeshGetter=CandidateMeshGetter;
 auto newObject=il2cpp_object_new;il2cpp_object_new=[](void*)->void*{return Test::candidate;};
 candidateMeshAssigned=false;candidateDestroyed=candidateAssignments=0;
 P::ObservePrefab(Test::source,"assets/world.prefab","resource-handle");
 Require(P::verifiedRoots.at(int(uintptr_t(Test::source)))&&candidateMeshAssigned&&candidateAssignments==1&&candidateDestroyed==1,
         "inactive candidate did not bind author Mesh exactly once and retire");
 Require(CandidateMeshGetter(Test::oldRenderer)==Test::oldMesh&&P::initializedDuringClone==0,
         "candidate assignment altered the original Prefab or initialized consumers");
 P::ObservePrefab(Test::unrelatedRoot,"assets/npc.prefab","prefab-instantiate-proxy");
 Require(candidateAssignments==2&&candidateDestroyed==2,
         "a second matched Prefab skipped candidate binding after the first passed");
 Require(P::verifiedRoots.at(int(uintptr_t(Test::unrelatedRoot)))&&P::timings.size()==2,
         "second Prefab lacks its own acceptance result and split timing");
 auto queries=prefabComponentQueries;
 for(size_t i=0;i<1000;++i){P::ObservePrefab(Test::source,"assets/world.prefab","resource-handle");P::ObservePrefab(Test::unrelatedRoot,"assets/npc.prefab","prefab-instantiate-proxy");}
 Require(candidateAssignments==2&&candidateDestroyed==2&&prefabComponentQueries==queries,
         "duplicate Prefab events repeated candidate construction or discovery");
 auto expire=[&](void* root){auto& identity=P::observed.at(int(uintptr_t(root)));Test::handles.at(*identity.handle)=nullptr;};
 expire(Test::source);P::ObservePrefab(Test::source,"assets/world.prefab","resource-handle");
 Require(candidateAssignments==3&&candidateDestroyed==3&&P::verifiedRoots.at(int(uintptr_t(Test::source))),
         "a retired weak source identity suppressed a new native resource using its ID");
 expire(Test::source);initializeDuringBinding=true;
 P::ObservePrefab(Test::source,"assets/world.prefab","resource-handle");initializeDuringBinding=false;
 Require(!P::verifiedRoots.at(int(uintptr_t(Test::source)))&&candidateDestroyed==4&&P::timings.size()==1,
         "consumer initialization during binding was accepted or leaked a candidate");
 expire(Test::unrelatedRoot);P::ObservePrefab(Test::unrelatedRoot,"assets/npc.prefab","prefab-instantiate-proxy");
 Require(P::verifiedRoots.at(int(uintptr_t(Test::unrelatedRoot)))&&candidateAssignments==5&&candidateDestroyed==5,
         "one Prefab failure suppressed acceptance of another source");
 expire(Test::source);P::authorMesh={};P::meshLoad=std::make_shared<AuthorAssetLoad::Load>();
 P::ObservePrefab(Test::source,"assets/world.prefab","resource-handle");
 Require(P::pendingPrefabs.size()==1&&candidateAssignments==5,"pending asset blocked or built an incomplete candidate");
 queries=prefabComponentQueries;P::ObservePrefab(Test::source,"assets/world.prefab","resource-handle");P::AdvancePending();
 Require(prefabComponentQueries==queries&&candidateAssignments==5,"pending proof rescanned its Prefab or blocked on native completion");
 P::meshLoad->phase=AuthorAssetLoad::Phase::Ready;P::meshLoad->asset=RuntimeShapes::Ref::Make(Test::newMesh,false);P::AdvancePending();
 Require(P::pendingPrefabs.empty()&&candidateAssignments==6&&P::timings.at(int(uintptr_t(Test::source))).deferred,
         "ready async asset did not finish its queued proof with deferred timing");
 expire(Test::source);P::authorMesh={};P::meshLoad->phase=AuthorAssetLoad::Phase::Bundle;
 P::ObservePrefab(Test::source,"assets/world.prefab","resource-handle");expire(Test::source);
 P::meshLoad->phase=AuthorAssetLoad::Phase::Ready;P::AdvancePending();
 Require(P::pendingPrefabs.empty()&&candidateAssignments==6,"retired source was cloned after asynchronous completion");
 P::meshLoad={};
 il2cpp_object_new=newObject;RuntimeShapes::originalMeshGetter=Test::Mesh;
 P::enabled=false;P::authorMesh={};P::resolvedMesh={};P::observed.clear();P::matchedRoots.clear();P::verifiedRoots.clear();P::timings.clear();Test::Reset();
}
void NativeAuthorLoadRegression(){
 namespace A=AuthorAssetLoad;Test::Setup();Test::Reset();
 auto saved=NativeBundle::api;auto invoke=il2cpp_runtime_invoke;auto allocate=il2cpp_object_new;
 std::map<void*,bool> complete;unsigned submitted=0,assetRequests=0,resultReads=0,unloads=0,doneReads=0,verifications=0;
 std::map<void*,void*> bundles;std::vector<void*> bundleRequests,requests;
 NativeBundle::api.nativeStreamClass=Test::instanceClass;NativeBundle::api.nativeStreamCtor=(void*)700;
 // The stream icall is a native function pointer; the fixture request counter
 // is supplied through the managed async entry so its real arguments are used.
 NativeBundle::api.streamAsyncIcall=nullptr;NativeBundle::api.streamAsync=(void*)705;
 NativeBundle::api.done=(void*)701;NativeBundle::api.bundle=(void*)702;NativeBundle::api.assetLoad=(void*)703;NativeBundle::api.result=(void*)704;NativeBundle::api.unload=(void*)706;
 il2cpp_object_new=[](void*)->void*{return Test::instance;};
 // Install the captured fixture through a static function object because the
 // IL2CPP export is a C function pointer, not an owning callable.
 static std::function<void*(void*,void*,void**,void**)> nativeFixture;
 nativeFixture=[&](void* method,void* self,void** args,void** exception)->void*{
  if(method==(void*)700){Require(self==Test::instance&&args[0]&&*(int64_t*)args[1]==4,"author native stream lost immutable bytes");return nullptr;}
  if(method==(void*)705){Require(!self&&args[0]==Test::instance&&*(uint32_t*)args[1]==0,"author Bundle submission was not async stream input");auto request=(void*)(uintptr_t(0xD000)+ ++submitted);bundleRequests.push_back(request);complete[request]=false;bundles[request]=(void*)(uintptr_t(0xE000)+submitted);return request;}
  if(method==(void*)701){++doneReads;return complete.at(self)?&Test::yes:&Test::no;}
  if(method==(void*)702){Require(complete.at(self),"Bundle result read before native completion");return bundles.at(self);}
  if(method==(void*)703){Require(self&&args[1]==Test::instance&&Entry::String(args[0])=="assets/author.asset","author async asset lost exact path/type");auto request=(void*)(uintptr_t(0xF000)+ ++assetRequests);requests.push_back(request);complete[request]=false;return request;}
  if(method==(void*)704){Require(complete.at(self),"asset result read before completion");++resultReads;return Test::newMesh;}
  if(method==(void*)706){Require(self&&*(unsigned char*)args[0]==1,"unpublished author Bundle did not destroy its native assets");++unloads;NativeBundle::ReleasedMemory(self,NativeBundle::MemoryIdentity(self));return nullptr;}
  return invoke(method,self,args,exception);
 };
 il2cpp_runtime_invoke=[](void* m,void* s,void** a,void** e)->void*{return nativeFixture(m,s,a,e);};
 {
  A::Cache cache;auto bytes=std::make_shared<const Bytes>(Bytes{1,2,3,4});std::weak_ptr<const Bytes> backing=bytes;
  A::Input input{{std::string(64,'A'),"assets/author.asset","Mesh"},bytes,RuntimeShapes::Ref::Make(Test::instance,false),[&](void* mesh){Require(mesh==Test::newMesh,"author result is wrong object");++verifications;}};
  auto first=cache.Request(input);Require(cache.Request(input)==first&&submitted==1,"simultaneous consumers duplicated an author Bundle request");
  input.bytes.reset();bytes.reset();cache.Advance();Require(first->phase==A::Phase::Bundle&&assetRequests==0,"unfinished Bundle blocked or read its asset");
  complete[bundleRequests.back()]=true;cache.Advance();Require(first->phase==A::Phase::Asset&&assetRequests==1&&!backing.expired(),"Bundle completion lost backing or omitted asset request");
  cache.Advance();Require(resultReads==0,"unfinished asset was synchronously completed");
  complete[requests.back()]=true;cache.Advance();Require(first->phase==A::Phase::Ready&&verifications==1&&cache.PendingCount()==0&&!backing.expired(),"ready author asset lost its Bundle lifetime");
  auto reads=doneReads;for(size_t i=0;i<1000;++i){Require(cache.Request(input)==first,"ready asset was not reused");cache.Advance();}
  Require(doneReads==reads&&submitted==1&&verifications==1,"ready assets remained in per-frame polling work");
  cache.CancelUnpublished(first);Require(unloads==1&&backing.expired(),"unpublished author asset did not release native backing");
  input.key.digest=std::string(64,'B');input.bytes=std::make_shared<const Bytes>(Bytes{1,2,3,4});
  auto cancelled=cache.Request(input);cache.CancelUnpublished(cancelled);cache.Advance();Require(unloads==1&&cancelled->phase==A::Phase::Bundle,"cancelled pending Bundle unloaded before completion");
  complete[bundleRequests.back()]=true;cache.Advance();Require(cancelled->phase==A::Phase::Cancelled&&unloads==2&&assetRequests==1,"cancelled Bundle started unnecessary asset work or leaked");
  input.key.digest=std::string(64,'C');input.verify=[](void*){throw std::runtime_error("bad native metadata");};
  auto failed=cache.Request(input);complete[bundleRequests.back()]=true;cache.Advance();complete[requests.back()]=true;cache.Advance();
  Require(failed->phase==A::Phase::Failed&&failed->error=="bad native metadata"&&unloads==3,"native verification failure published or leaked an author asset");
  input.key.digest=std::string(64,'D');auto assetCancelled=cache.Request(input);complete[bundleRequests.back()]=true;cache.Advance();cache.CancelUnpublished(assetCancelled);cache.Advance();
  Require(unloads==3&&assetCancelled->phase==A::Phase::Asset,"pending asset cancelled its Bundle too early");
  complete[requests.back()]=true;cache.Advance();Require(assetCancelled->phase==A::Phase::Cancelled&&unloads==4,"cancelled asset request did not drain and retire");
  Require(NativeBundle::memoryRequests.empty()&&NativeBundle::memoryBundles.empty(),"author requests left native archive ownership behind");
 }
 nativeFixture={};NativeBundle::api=saved;il2cpp_runtime_invoke=invoke;il2cpp_object_new=allocate;Test::Reset();
}
namespace PackMetadata {
struct Class;
struct Type {const char* name;Class* klass=nullptr;bool byref=false;};
struct Field {const char* name;Type* type;size_t offset;};
struct Method {const char* name;std::vector<Type*> parameters;Type* result;};
struct Class {size_t bytes;std::vector<Field*> fields;std::vector<Method*> methods;};
void Regression(){
 Type boolean{"System.Boolean"},scalar{"System.UInt64"},bundleType{"Fixture.Bundle"},packType{"Fixture.AssetInfoPack&",nullptr,true};
 Field hash{"hashName",&scalar,16+24},nested{"bundleData",&bundleType,16+32};
 Class bundle{40,{&hash},{}},pack{96,{&nested},{}};bundleType.klass=&bundle;packType.klass=&pack;
 Method lookup{"TryGetAssetInfoFromPath",{&scalar,&packType},&boolean};Class manifest{16,{}, {&lookup}};
 il2cpp_class_get_methods=[](void* owner,void** cursor)->void*{auto& rows=((Class*)owner)->methods;auto n=size_t(*cursor);if(n>=rows.size())return nullptr;*cursor=(void*)(n+1);return rows[n];};
 il2cpp_method_get_name=[](void* method){return ((Method*)method)->name;};
 il2cpp_method_get_param_count=[](void* method)->uint32_t{return uint32_t(((Method*)method)->parameters.size());};
 il2cpp_method_get_param=[](void* method,uint32_t i)->void*{return ((Method*)method)->parameters.at(i);};
 il2cpp_method_get_return_type=[](void* method)->void*{return ((Method*)method)->result;};
 il2cpp_method_get_flags=[](void*,uint32_t* impl)->uint32_t{*impl=0;return 0;};
 il2cpp_type_get_name=[](void* type){return ((Type*)type)->name;};
 il2cpp_type_is_byref=[](void* type){return ((Type*)type)->byref;};
 il2cpp_class_from_type=[](void* type)->void*{return ((Type*)type)->klass;};
 il2cpp_class_get_fields=[](void* owner,void** cursor)->void*{auto& rows=((Class*)owner)->fields;auto n=size_t(*cursor);if(n>=rows.size())return nullptr;*cursor=(void*)(n+1);return rows[n];};
 il2cpp_field_get_name=[](void* field){return ((Field*)field)->name;};
 il2cpp_field_get_type=[](void* field)->void*{return ((Field*)field)->type;};
 il2cpp_field_get_offset=[](void* field){return ((Field*)field)->offset;};
 il2cpp_class_value_size=[](void* owner,uint32_t* alignment)->int32_t{*alignment=8;return int32_t(((Class*)owner)->bytes);};
 Entry::PrefabAdaptationProbe::BindLookup(&manifest);
 Require(Entry::PrefabAdaptationProbe::api.assetSize==96&&Entry::PrefabAdaptationProbe::api.bundleHashOffset==56,
         "asset lookup confused AssetInfoPack with its nested AssetInfo/Bundle");
 nested.offset=16+80;bool rejected=false;try{Entry::PrefabAdaptationProbe::BindLookup(&manifest);}catch(const std::exception&){rejected=true;}
 Require(rejected,"nested Bundle escaped the returned AssetInfoPack");
}
}
int main(){try{
 Test::Setup();Test::Reset();il2cpp_runtime_invoke=ProbeInvoke;
 namespace P=Entry::PrefabAdaptationProbe;
 P::enabled=true;P::observed.clear();P::matchedRoots.clear();P::verifiedRoots.clear();
 P::authorMesh=RuntimeShapes::Ref::Make(Test::newMesh,false);
 for(auto root:{Test::source,Test::candidate,Test::unrelatedRoot})P::verifiedRoots[int(uintptr_t(root))]=true;
 Test::identitySameMesh=true;P::resolvedMesh=RuntimeShapes::Ref::Make(Test::newMesh,true);
 // Deliberately do not install an offline root list. All three resource roles
 // enter the same actual probe boundary and match the exact sharedMesh object.
 P::ObservePrefab(Test::source,"assets/world.prefab","resource-handle");
 P::ObservePrefab(Test::candidate,"assets/ui.prefab","resource-handle");
 P::ObserveProxy(Test::unrelatedRoot,1234);
 Require(P::matchedRoots.size()==3&&prefabComponentQueries==3,
         "shared event boundary omitted main/UI/NPC or required an offline Prefab list");
 auto queries=prefabComponentQueries;
 for(size_t i=0;i<1000;++i){P::ObservePrefab(Test::source,"assets/world.prefab","resource-handle");P::ObserveProxy(Test::unrelatedRoot,1234);}
 Require(prefabComponentQueries==queries,"repeated observed Prefab rescanned its Renderer hierarchy");
 // Both Mesh names are 'marker' in this fixture. Their distinct native object
 // identities must still prevent a match; name equality is not provenance.
 Test::identitySameMesh=false;P::resolvedMesh=RuntimeShapes::Ref::Make(Test::oldMesh,true);
 P::ObservePrefab(Test::lazyCandidate,"assets/other.prefab","resource-handle");
 Require(P::matchedRoots.size()==3,"same-name unrelated Mesh matched the selected source object");
 P::enabled=false;P::authorMesh={};P::resolvedMesh={};P::matchedRoots.clear();P::observed.clear();P::verifiedRoots.clear();Test::Reset();
 CandidateBindingRegression();NativeAuthorLoadRegression();MaterialApplicationRegression();PackMetadata::Regression();
 std::cout<<"PASS Prefab probe: independent results/timings, duplicate event suppression, weak identity retirement, initialization rejection, queued proof completion, async author request sharing, idle zero polling, cancellation drain, native backing lifetime and failure isolation\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
