#define EIEM_MESH_TARGET_PROBE 1
#define main FullResourceFixtureMain
#include "full_resource_reload_smoke.cpp"
#undef main
#include <future>

namespace {
unsigned hierarchyQueries=0,meshReads=0;
thread_local bool workerDelivery=false;
bool proxyInfoUnavailable=false;
unsigned proxyInfoCalls=0;
void* Invoke(void* method,void* self,void** args,void** exception) {
  if(method==(void*)1) return workerDelivery?&Test::no:&Test::yes;
  if(method==(void*)800) {Require(!proxyInfoUnavailable,"delivery reread a reused AssetProxy instead of its captured pack");++proxyInfoCalls;return Test::configBox;}
  if(method==(void*)801) return Test::StringValue("assets/not-in-any-offline-prefab-list.prefab");
  if(method==(void*)802) return Test::bundle;
  if(method==(void*)803) {++meshReads;return Test::oldMesh;}
  if(method==(void*)804) {Require(*(uint64_t*)args[0]==987,"FBX target lookup lost native subasset hash");++meshReads;return Test::oldMesh;}
  if(method==(void*)5) ++hierarchyQueries;
  return Test::Invoke(method,self,args,exception);
}
void DirectoryRegression(int argc,char** argv) {
  namespace D=EiemMeshTargetDirectory;
  std::unordered_set<uint64_t> targets{11,33};
  Require(D::Candidate(11,{},targets),"same-Bundle target was filtered out");
  Require(D::Candidate(100,{5,33,77},targets),"transitive dependency target was filtered out");
  auto begin=std::chrono::steady_clock::now();size_t expanded=0;
  for(uint64_t i=1000;i<201000;++i) if(D::Candidate(i,{1,2,3,4},targets))++expanded;
  Require(!expanded,"unrelated Bundle events caused Prefab expansion");
  std::cout<<"PASS 200000 unrelated dependency events, hierarchyExpansions=0 ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<'\n';
  if(argc>1) {
    auto directoryBegin=std::chrono::steady_clock::now();
    size_t mods=0,count=0;for(const auto& root:EiemComposedPackage::Roots(std::filesystem::u8path(argv[1]))) {
      auto entries=D::Read(root);++mods;count+=entries.size();
      std::cout<<"INI "<<root.filename().u8string()<<" targets="<<entries.size()<<'\n';
    }
    Require(mods&&count,"real installed Mod rules were not read");
    std::cout<<"PASS installed declarations mods="<<mods<<" targets="<<count<<" readMs="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-directoryBegin).count()<<" resourcePayloadBytesRead=0 compiledInputsRead=0\n";
  }
}
void DeliveryRegression() {
  namespace P=MeshTargetProbe;Test::Setup();Test::Reset();il2cpp_runtime_invoke=Invoke;
  P::targets.clear();P::names.clear();P::manifests.clear();P::manifestEvents.clear();P::clearEvents.clear();P::observations.clear();P::observationOrder.clear();P::counters={};
  P::rawDeliveries.clear();P::capturedRaw=P::drainedRaw=P::retriedRaw=0;P::rawCursor=nullptr;P::lateMeshes.clear();
  EiemMeshTargetDirectory::Target target;target.mod="existing";target.rule="ReplaceMesh";target.source={"assets/source.asset","marker"};target.parts=19;
  P::targets.push_back(target);P::names["marker"]={0};P::api.proxyInfo=(void*)800;P::api.assetPath=(void*)801;P::api.bundleGetter=(void*)802;P::api.loadPath=(void*)803;
  P::api.packClass=Test::configClass;P::api.bundleOffset=0;
  auto unbox=il2cpp_object_unbox;static uint64_t bundle=55;
  il2cpp_object_unbox=[](void* object)->void*{return object==Test::configBox?&bundle:object;};
  P::Manifest state;state.object=RuntimeShapes::Ref::Make(Test::dependencyManifest,true);state.bindings.resize(1);state.bindings[0].found=true;state.bindings[0].bundle=33;
  state.targetBundles.insert(33);state.candidates[55]=false;P::manifests.emplace(Test::dependencyManifest,std::move(state));
  P::meshType=RuntimeShapes::Ref::Make(Test::instance,false);P::enabled=true;hierarchyQueries=meshReads=0;
  {auto previous=StartupDependencies::api.poolField;StartupDependencies::api.poolField=nullptr;
    std::unique_lock<std::recursive_mutex> mainMatcher(P::mutex);
    auto worker=std::async(std::launch::async,[]{P::Dependencies(Test::dependencyManifest,55,nullptr,true);});
    auto ready=worker.wait_for(std::chrono::seconds(2))==std::future_status::ready;
    mainMatcher.unlock();worker.get();Require(ready,"loader worker blocked on main-thread matcher lock");StartupDependencies::api.poolField=previous;
  }
  P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(!hierarchyQueries&&!meshReads&&P::counters.rejected==1,"negative filter touched a Prefab hierarchy or Mesh payload");
  for(size_t i=0;i<1000;++i)P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(P::counters.sources==1&&P::counters.cached==1000,"repeated raw asset delivery repeated discovery");
  P::observations.clear();P::observationOrder.clear();P::manifests.begin()->second.candidates[55]=true;
  auto& binding=P::manifests.begin()->second.bindings[0];binding.mesh=RuntimeShapes::Ref::Make(Test::newMesh,true);
  P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(hierarchyQueries==1&&P::counters.hits==0,"a matching name with a different native Mesh caused a false match");
  P::observations.clear();P::observationOrder.clear();binding.mesh=RuntimeShapes::Ref::Make(Test::oldMesh,true);
  P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(hierarchyQueries==2&&P::counters.hits==1,"new scene Prefab did not match exact native source Mesh");
  auto cloneCount=Test::cloneCalls;
  FullResourceReload::Clone(Test::source);FullResourceReload::CloneParent(Test::source,Test::factoryParent,true);
  FullResourceReload::Instantiate(Test::source,Test::factoryPosition,Test::factoryRotation);
  FullResourceReload::InstantiateParent(Test::source,Test::factoryParent,Test::factoryPosition,Test::factoryRotation);
  FullResourceReload::InstantiateImmediate(Test::source,Test::factoryParent,true);
  FullResourceReload::InstantiateAsync(Test::source,1,Test::factoryParent,Test::factoryPosition,Test::factoryRotation);
  Require(P::counters.clones==6&&Test::cloneCalls==cloneCount+6,"sync/async clone hooks did not all observe the matched source or changed native flow");
  for(size_t i=0;i<1000;++i)P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(hierarchyQueries==2&&!meshReads,"cached Prefab repeated hierarchy or Mesh discovery");
  auto& identity=P::observations.at(Test::source);Test::handles.at(*identity.object.handle)=nullptr;
  P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(hierarchyQueries==3&&P::counters.hits==2,"native object reuse suppressed a new Prefab lifetime");
  P::BundleCleared(Test::dependencyManifest,33);
  P::DrainEvents();
  Require(!binding.mesh.Get(),"Bundle retirement retained target Mesh identity");
  namespace D=StartupDependencies;
  D::Snapshot snapshot{RuntimeShapes::Ref::Make(Test::instance,true),RuntimeShapes::Ref::Make(Test::dependencyManifest,true),33,{}};
  D::snapshots[Test::instance]=snapshot;D::snapshotKeys[{Test::dependencyManifest,33}]=Test::instance;
  auto& manifest=P::manifests.begin()->second;
  Require(P::ExactMesh(manifest,0).Get()==Test::oldMesh&&meshReads==1,"primary asset did not resolve only from its already-loaded native Bundle");
  P::BundleCleared(Test::dependencyManifest,33);P::DrainEvents();P::targets[0].source.path="assets/source.fbx";binding.asset=987;P::api.loadHash=(void*)804;
  Require(P::ExactMesh(manifest,0).Get()==Test::oldMesh&&meshReads==2,"FBX subasset did not use its exact native asset hash");
  D::snapshots.clear();D::snapshotKeys.clear();P::BundleCleared(Test::dependencyManifest,33);P::DrainEvents();
  Require(!P::ExactMesh(manifest,0).Get()&&meshReads==2,"unloaded target caused an independent game resource request");
  P::observations.clear();P::observationOrder.clear();P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(P::counters.incomplete==1&&!P::observations.count(Test::source),"not-yet-resolved native Mesh became a permanent source miss");
  binding.mesh=RuntimeShapes::Ref::Make(Test::oldMesh,true);P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(P::observations.at(Test::source).hits==1,"later Mesh identity readiness did not retry the original delivery");
  // Missing dependency evidence must remain retryable, never a cached miss.
  auto dependencyApi=D::api;D::api.poolAllocate=(void*)805;
  P::observations.clear();P::observationOrder.clear();manifest.candidates.clear();
  auto before=hierarchyQueries;P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(P::counters.unknown==1&&!P::observations.count(Test::source)&&hierarchyQueries==before,"not-ready dependency evidence caused expansion or a permanent negative result");
  manifest.candidates[55]=true;binding.mesh=RuntimeShapes::Ref::Make(Test::oldMesh,true);
  P::ObserveProxy(Test::source,9,Test::assetProxyObject);
  Require(P::observations.at(Test::source).hits==1&&hierarchyQueries==before+1,"later native readiness did not retry the original Prefab delivery");
  D::api=dependencyApi;
  // A worker must retain the boxed pack (including its managed members),
  // return without the matcher lock, and match before the native clone.
  P::observations.clear();P::observationOrder.clear();P::observedTargetMeshes.clear();
  P::originalAssetGet=[](void*,void*)->void*{return Test::source;};
  before=hierarchyQueries;auto sourcesBefore=P::counters.sources;
  {
    std::unique_lock<std::recursive_mutex> mainMatcher(P::mutex);
    auto worker=std::async(std::launch::async,[]{workerDelivery=true;auto source=P::AssetGet(Test::assetProxyObject,nullptr);workerDelivery=false;Require(source==Test::source,"worker delivery changed the original resource");});
    auto ready=worker.wait_for(std::chrono::seconds(2))==std::future_status::ready;
    mainMatcher.unlock();worker.get();Require(ready,"raw loader delivery blocked on main-thread matcher lock");
  }
  Require(P::rawDeliveries.size()==1&&P::capturedRaw==1&&hierarchyQueries==before&&P::counters.sources==sourcesBefore,"worker delivery queried a Unity hierarchy or failed to capture");
  Require(P::rawDeliveries.at(Test::source).info.Get()==Test::configBox,"worker pack was not rooted independently of the proxy");
  auto infoCalls=proxyInfoCalls;proxyInfoUnavailable=true;
  FullResourceReload::Clone(Test::source);proxyInfoUnavailable=false;
  Require(P::rawDeliveries.empty()&&P::drainedRaw==1&&P::observations.at(Test::source).hits==1&&hierarchyQueries==before+1&&proxyInfoCalls==infoCalls,"captured source did not match before native instantiation without rereading the proxy");
  P::observations.clear();P::observationOrder.clear();binding.mesh={};
  P::CaptureWorkerSource(Test::source,Test::assetProxyObject);P::DrainRawSource(Test::source);
  Require(P::rawDeliveries.size()==1&&P::retriedRaw==1&&!P::observations.count(Test::source),"not-ready worker evidence was discarded before the next real delivery");
  binding.mesh=RuntimeShapes::Ref::Make(Test::oldMesh,true);P::BeforeClone(Test::source,"worker-late-readiness");
  Require(P::rawDeliveries.empty()&&P::observations.at(Test::source).hits==1,"worker snapshot did not retry after native Mesh readiness");
  auto fields=il2cpp_class_get_fields;auto fieldName=il2cpp_field_get_name;
  auto fieldValue=il2cpp_field_get_value;auto objectClass=il2cpp_object_get_class;
  il2cpp_class_get_fields=[](void* klass,void** iterator)->void* {
    if(klass!=(void*)0xABF1)return Test::Fields(klass,iterator);
    auto index=uintptr_t(*iterator);if(index>=2)return nullptr;*iterator=(void*)(index+1);return (void*)(900+index);
  };
  il2cpp_field_get_name=[](void* field)->const char*{return field==(void*)900?"_items":field==(void*)901?"_size":Test::FieldName(field);};
  il2cpp_field_get_value=[](void* object,void* field,void* out){if(field==(void*)900)*(void**)out=Test::Array(Test::oldRenderer);else if(field==(void*)901)*(int*)out=1;else Test::FieldValue(object,field,out);};
  il2cpp_object_get_class=[](void* object)->void*{return object==(void*)0xABF0?(void*)0xABF1:Test::GetClass(object);};
  before=hierarchyQueries;P::ConsumerList((void*)0xABF0,"before-material-init");
  Require(P::counters.consumerHits==1&&!P::counters.lateConsumerHits,"consumer audit lost an earlier exact source match");
  P::observedTargetMeshes.clear();P::observations.clear();P::observationOrder.clear();
  P::CaptureWorkerSource(Test::source,Test::assetProxyObject);
  P::ConsumerList((void*)0xABF0,"helper-init-completed");
  Require(P::counters.consumerHits==2&&P::counters.lateConsumerHits==1&&hierarchyQueries==before&&P::rawDeliveries.size()==1,"consumer audit concealed a missing early source observation by draining the mailbox after instantiation");
  FullResourceReload::Clone(Test::source);
  Require(P::rawDeliveries.empty()&&P::observations.at(Test::source).hits==1,"pending source was not handled at the next real clone boundary");
  il2cpp_class_get_fields=fields;il2cpp_field_get_name=fieldName;il2cpp_field_get_value=fieldValue;il2cpp_object_get_class=objectClass;
  il2cpp_object_unbox=unbox;P::enabled=false;P::manifests.clear();P::manifestEvents.clear();P::clearEvents.clear();P::observations.clear();P::observedTargetMeshes.clear();P::observationOrder.clear();P::rawDeliveries.clear();P::meshType={};Test::Reset();
  std::cout<<"PASS real hook seam: negative filter, same-name wrong-Mesh rejection, unseen scene, six sync/async factories, repeated delivery, retired native identity, exact asset/FBX lookup, late readiness, nonblocking worker capture, consumer audit; no replacement or compilation\n";
}
}
int main(int argc,char** argv) {
  try {DirectoryRegression(argc,argv);DeliveryRegression();}
  catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
