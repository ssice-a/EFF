#pragma once
#include "native_mesh_fixture.h"
namespace EffectNativeMeshProofTest {
using namespace Entry;
namespace D=DemandLoadingProbe;namespace S=StartupDependencies;namespace A=RuntimeAttachments;
std::map<void*,std::vector<void*>> consumers;
std::map<void*,void*> rendererMeshes,owners;
std::string physical;
uint64_t extent=0;unsigned vfsQueries=0,logicalQueries=0,dependencyQueries=0;
constexpr uint64_t bundleKey=0xB180;
MInfo encrypted{},backing{};
void* Physical(void*,void*){return Test::StringValue(physical);}
unsigned char Encrypted(void*,void*){return 0;}
void* Invoke(void* method,void* self,void** args,void** error){
 *error=nullptr;auto m=uintptr_t(method);
 if(m==1700){++logicalQueries;auto hash=*(uint64_t*)args[0];bool found=false;
  for(const auto& path:{"assets/a.prefab","assets/b.prefab","assets/shared.prefab","assets/static.mesh","assets/empty.prefab","assets/other.prefab"})found|=hash==std::hash<std::string>{}(path);
  if(!found)return &Test::no;memcpy(args[1],&bundleKey,8);uint64_t asset=hash==std::hash<std::string>{}("assets/static.mesh")?42:0;memcpy((char*)args[1]+8,&asset,8);return &Test::yes;
 }
 if(m==1701)return &Test::no;
 if(m==1702)return Test::StringValue("proof.ab");
 if(m==1703)return Test::dependencyManifest;
 if(m==1704){++vfsQueries;Require(Entry::String(args[0])=="Data/Bundles/Windows/proof.ab","native proof escaped requested VFS dependency");
  uint64_t zero=0;memcpy(args[1],&zero,8);memcpy((char*)args[1]+8,&extent,8);return &Test::yes;}
 if(m==1705)return self;
 if(m==1706)return rendererMeshes.at(self);
 if(m==79||m==5||(m==909&&args[0]!=(void*)910))return Test::Array(consumers[self]);
 if(m==927){auto found=owners.find(self);return found==owners.end()?self:found->second;}
 if(m==4&&(rendererMeshes.count(self)||self==Test::oldMesh||self==Test::newMesh))return Test::StringValue("Static");
 return EffectTest::Invoke(method,self,args,error);
}
void Checks(){
 EffectTest::Setup();auto oldInvoke=il2cpp_runtime_invoke;auto oldClass=il2cpp_object_get_class;auto oldNew=il2cpp_object_new;
 auto oldApi=D::api;auto oldShared=D::sharedApi;auto oldStartup=S::api;auto oldAll=S::originalAll;
 auto oldScopes=D::sharedScopes;bool oldRuntimeEnabled=RuntimeShapes::enabled.load();auto oldProofs=D::nativeProofs.load();
 auto directory=std::filesystem::temp_directory_path()/("EFF-native-proof-"+std::to_string(GetCurrentProcessId()));
 Require(!std::filesystem::exists(directory),"native proof fixture directory already exists");std::filesystem::create_directories(directory);
 auto cleanup=[&]{
  D::sharedScopes=oldScopes;D::api=oldApi;D::sharedApi=oldShared;S::api=oldStartup;S::originalAll=oldAll;D::nativeProofs=oldProofs;
  RuntimeShapes::enabled=oldRuntimeEnabled;il2cpp_runtime_invoke=oldInvoke;il2cpp_object_get_class=oldClass;il2cpp_object_new=oldNew;
  A::bindings.clear();A::standalone=std::make_shared<const std::vector<Package>>();RuntimeControls::controlGraph.reset();RuntimeControls::variables.clear();
  FullResourceReload::controlledPrefabSources.clear();FullResourceReload::controlledPrefabPaths.clear();FullResourceReload::liveControlledClones.clear();consumers.clear();rendererMeshes.clear();owners.clear();
  std::error_code ignored;std::filesystem::remove_all(directory,ignored);
 };
 try{
  Fixture fixture;fixture.Add(101,5);SetString(fixture.T(101),Child(fixture.T(101),fixture.V(101),"m_Name"),"Static");fixture.Pointer(22,"m_Mesh",101);
  auto& container=ArrayValue(fixture.T(300),Child(fixture.T(300),fixture.V(300),"m_Container"));auto row=ArrayElement(fixture.T(300),container);
  SetString(fixture.T(300),Child(fixture.T(300),row,"first"),"assets/shared.prefab");
  SetReference(fixture.T(300),Child(fixture.T(300),Child(fixture.T(300),row,"second"),"asset"),{fixture.cab,10},fixture.refs,fixture.cab);container.children.push_back(row);container.count=container.children.size();
  fixture.Add(102,5);SetString(fixture.T(102),Child(fixture.T(102),fixture.V(102),"m_Name"),"Other");
  fixture.Add(40,0);fixture.Add(41,1);fixture.Add(42,2);fixture.Add(43,3);
  fixture.Pointer(41,"m_GameObject",40);fixture.Pointer(42,"m_GameObject",40);fixture.Pointer(42,"m_Mesh",102);
  fixture.Pointer(43,"m_GameObject",40);fixture.List(40,"m_Component",{41,42,43},"component");
  for(const auto& entry:std::map<std::string,int64_t>{{"assets/empty.prefab",30},{"assets/other.prefab",40}}){
   auto added=ArrayElement(fixture.T(300),container);
   SetString(fixture.T(300),Child(fixture.T(300),added,"first"),entry.first);
   SetReference(fixture.T(300),Child(fixture.T(300),Child(fixture.T(300),added,"second"),"asset"),{fixture.cab,entry.second},fixture.refs,fixture.cab);
   container.children.push_back(std::move(added));container.count=container.children.size();
  }
  auto& hashed=ArrayValue(fixture.T(300),Child(fixture.T(300),fixture.V(300),"m_HashContainer"));auto hashRow=ArrayElement(fixture.T(300),hashed);
  SetInteger(fixture.T(300),Child(fixture.T(300),hashRow,"first"),42);SetReference(fixture.T(300),Child(fixture.T(300),Child(fixture.T(300),hashRow,"second"),"asset"),{fixture.cab,100},fixture.refs,fixture.cab);hashed.children.push_back(hashRow);hashed.count=1;
  auto bytes=EndfieldInput::Encode(EiemAuthorMeshBundle::EnvelopeHeader(),{{fixture.cab,4,fixture.BytesOf()}});
  auto file=directory/"proof.vfs";EiemResourceReloadPlan::Save(file,bytes);physical=file.u8string();extent=bytes.size();vfsQueries=logicalQueries=dependencyQueries=0;
  D::api.packSize=16;D::api.bundleOffset=0;D::api.assetOffset=8;D::api.lookup=(void*)1700;D::api.subLookup=(void*)1701;
  S::api={};S::api.bundleName=(void*)1702;S::api.listCtor=(void*)140;S::api.listAdd=(void*)141;S::api.listClass=(void*)700;S::api.listItems=(void*)18;S::api.listSize=(void*)19;
  S::originalAll=[](void* self,uint64_t key,void* list,void* info){++dependencyQueries;return NativeDependencyQuery(self,key,list,info);};
  Test::nativeDependencyLists[bundleKey]={};Test::propertyElementWidth=8;
  auto& api=D::sharedApi;api.infoSize=16;api.offset=0;api.offsetWidth=8;api.length=8;api.lengthWidth=8;api.vfsInstance=(void*)1703;api.vfsQuery=(void*)1704;
  encrypted.mp=(void*)Encrypted;backing.mp=(void*)Physical;api.vfsEncrypted=&encrypted;api.vfsPhysical=&backing;
  il2cpp_runtime_invoke=Invoke;
  il2cpp_object_get_class=[](void* object)->void*{return rendererMeshes.count(object)?Test::staticClass:Test::GetClass(object);};
  il2cpp_object_new=[](void*)->void*{auto bytes=std::make_unique<Bytes>(8);auto object=bytes->data();Test::storage.push_back(std::move(bytes));Test::dependencyLists[object]={};return object;};
  RuntimeShapes::api.getComponent=(void*)1705;RuntimeShapes::api.filterType=(void*)0x1707;RuntimeShapes::api.filterMesh=(void*)1706;RuntimeShapes::enabled=true;
  RuntimeShapes::api.modelRenderers=(void*)5;FullResourceReload::api.directComponents=(void*)79;FullResourceReload::api.componentGo=(void*)927;
  consumers={{Test::source,{Test::oldRenderer}},{Test::unrelatedRoot,{Test::unrelatedRenderer}},{Test::oldRoot,{Test::auxiliaryRenderer}}};
  rendererMeshes={{Test::oldRenderer,Test::oldMesh},{Test::unrelatedRenderer,Test::newMesh},{Test::auxiliaryRenderer,Test::oldMesh}};
  owners={{Test::oldRenderer,Test::source},{Test::unrelatedRenderer,Test::unrelatedRoot},{Test::auxiliaryRenderer,Test::oldRoot}};
  Test::transformParents={{Test::source,nullptr},{Test::unrelatedRoot,nullptr},{Test::oldRoot,nullptr}};
  auto scope=std::make_unique<EffectNativeMeshFixture::Scope>();scope->generation->observedMeshTargets.insert({"assets/static.mesh","Static"});scope->generation->observedMeshTargets.insert({"assets/static.mesh","WrongStatic"});
  scope->manifest->unresolvedObservedMeshes=true;
  const auto loads=Test::memoryCalls,leases=Test::leaseRequests,scans=Test::sceneQueries,opened=Test::opened,clones=Test::cloneCalls;
  D::ObserveNativeMeshSource(Test::unrelatedRoot,"assets/empty.prefab");
  Require(!dependencyQueries,"attach-only lookup expanded dependencies for a prefab without Renderer Mesh references");
  D::ObserveNativeMeshSource(Test::unrelatedRoot,"assets/other.prefab");
  Require(!dependencyQueries,"attach-only lookup expanded dependencies despite every local Renderer Mesh excluding the declared target names");
  auto negativeReads=vfsQueries;
  D::ObserveNativeMeshSource(Test::unrelatedRoot,"assets/empty.prefab");
  D::ObserveNativeMeshSource(Test::unrelatedRoot,"assets/other.prefab");
  Require(vfsQueries==negativeReads,"conclusive prefab misses reopened the source within the same generation");
  D::ObserveNativeMeshSource(Test::source,"ASSETS/A.PREFAB");
  Require(D::ProvenNativeMeshIdentities(Test::oldMesh).count({"assets/static.mesh","Static"}),"exact native source callback did not prove an untouched Mesh");
  Require(scope->generation->authors.empty()&&D::nativeProofs==128,"native proof accidentally depended on authored markers or diagnostic budget");
  Require(FullResourceReload::ControlledCloneSource(Test::source),"attach-only source never entered normal generic-clone completion");
  D::ObserveNativeMeshSource(Test::unrelatedRoot,"assets/b.prefab");
  Require(D::ProvenNativeMeshIdentities(Test::newMesh).empty(),"same-name native Mesh from an unrelated logical path acquired proof");
  D::ObserveNativeMeshSource(Test::oldRoot,"assets/shared.prefab");
  Require(D::ProvenNativeMeshIdentities(Test::oldMesh).size()==1,"shared Mesh across a second PFB lost or duplicated its logical identity");
  auto queries=vfsQueries;auto lookup=logicalQueries;auto handles=Test::handles.size();
  for(unsigned repeat=0;repeat<100;++repeat)D::ObserveNativeMeshSource(Test::source,"assets/a.prefab");
  Require(vfsQueries==queries&&logicalQueries==lookup&&Test::handles.size()==handles,"cached source observation reopened disk, queried manifest or allocated handles");
  Package package;package.id="NativeAttachOnly";EffectAttachment effect;effect.id="native";effect.variable="$fx";effect.autoplay=false;effect.targetPath="assets/static.mesh";effect.targetAsset="Static";effect.native=EffectTest::Manifest('6');package.effects[effect.id]=effect;
  A::standalone=std::make_shared<const std::vector<Package>>(std::vector<Package>{package});RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>();RuntimeControls::specs.clear();
  A::ModelReady(Test::source);A::ModelReady(Test::unrelatedRoot);
  RuntimeShapes::packageGeneration=Entry::generation.load();
  FullResourceReload::CompleteControlledClone(Test::oldRoot,FullResourceReload::ControlledCloneSource(Test::source),FullResourceReload::ControlledSourcePath(Test::source));
  Require(A::bindings.size()==2,"real ModelReady failed attach-only multi-PFB source identity isolation");
  Require(FullResourceReload::liveControlledClones.count(Test::oldRoot)&&FullResourceReload::ControlledSourcePath(Test::oldRoot)=="assets/a.prefab","generic clone lost its weak live-owner registration or logical source path");
  A::OwnerReleasing(Test::source);FullResourceReload::liveParts.clear();
  A::revision=RuntimeControls::revision;A::activeStamps=A::CaptureStamps();A::stampsInitialized=true;A::needed=true;
  auto installed=std::make_shared<D::InstalledGeneration>(*scope->generation);installed->serial++;
  std::atomic_store(&D::installedGeneration,std::shared_ptr<const D::InstalledGeneration>(installed));
  Require(D::ProvenNativeMeshIdentities(Test::oldMesh).empty(),"F10 accepted a retired generation's native Mesh witness");
  package.effects.at(effect.id).position[0]=3;A::standalone=std::make_shared<const std::vector<Package>>(std::vector<Package>{package});++RuntimeControls::revision;
  A::Frame();auto refreshed=D::Current(Test::dependencyManifest);
  Require(A::bindings.size()==1&&A::bindings.begin()->second.attachment.position[0]==3,"definitionsChanged Frame did not rebind an attach-only generic owner without a model-part/UI/helper callback");
  Require(refreshed!=scope->manifest&&D::ProvenNativeMeshIdentities(Test::oldMesh).count({"assets/static.mesh","Static"}),"F10 failed to re-observe an existing native owner");
  Require(!D::ProvenNativeMeshIdentities(Test::oldMesh).count({"assets/static.mesh","WrongStatic"}),"path-only manifest hash bypassed the declared Mesh asset identity");
  Test::handles[*refreshed->provenMeshes.at(Test::oldMesh).mesh.handle]=nullptr;
  Require(D::ProvenNativeMeshIdentities(Test::oldMesh).empty(),"recycled Mesh pointer inherited an expired weak witness");
  D::ObserveNativeMeshSource(Test::unrelatedRoot,"assets/b.prefab");
  Require(D::ProvenNativeMeshIdentities(Test::oldMesh).empty(),"unrelated source refreshed an expired Mesh pointer by name");
  D::ObserveNativeMeshSource(Test::source,"assets/missing.prefab");auto rejected=logicalQueries;
  for(unsigned repeat=0;repeat<100;++repeat)D::ObserveNativeMeshSource(Test::source,"assets/missing.prefab");
  Require(logicalQueries==rejected,"missing logical path was retried at every reconciliation");
  auto cloneId=FullResourceReload::Id(Test::oldRoot);EffectTest::dead.insert(Test::oldRoot);auto liveOwners=EffectDebug::Owners();Require(!liveOwners.count(cloneId)&&!FullResourceReload::liveControlledClones.count(Test::oldRoot),"dead generic clone retained its weak owner lease");liveOwners.clear();for(unsigned tick=0;tick<64;++tick)A::Frame();Require(A::bindings.empty(),"dead generic clone retained an attachment binding");EffectTest::dead.erase(Test::oldRoot);
  FullResourceReload::CompleteControlledClone(Test::oldRoot,true,"assets/a.prefab");A::OwnerReleasing(Test::oldRoot);
  Require(!FullResourceReload::liveControlledClones.count(Test::oldRoot)&&!FullResourceReload::controlledPrefabSources.count(Test::oldRoot),"explicit owner release retained generic clone provenance");
  Require(Test::memoryCalls==loads&&Test::leaseRequests==leases&&Test::sceneQueries==scans&&Test::opened==opened&&Test::cloneCalls==clones,"native provenance loaded/instantiated resources or scanned the scene");
  refreshed.reset();installed.reset();scope.reset();cleanup();EffectTest::Teardown();
  std::cout<<"PASS native source provenance: real manifest/VFS/metadata/source/ModelReady/Frame seam, attach-only, same-name isolation, shared PFBs, weak lifetime, generic clone F10/release/death and bounded cache; no resource loads, extra clones or scene scans\n";
 }catch(...){cleanup();throw;}
}
}
