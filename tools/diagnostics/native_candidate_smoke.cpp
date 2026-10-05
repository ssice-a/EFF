#include "../../src/eiem_resource_entry.cpp"
#include <functional>
#include <iostream>
using namespace Entry;
using namespace EiemNativeInput;
namespace Fixture {
void *source=(void*)0x1000,*renderer=(void*)0x1100,*candidate=(void*)0x1200,*bundle=(void*)0x1300,
     *create=(void*)0x1400,*asset=(void*)0x1500,*meshClass=(void*)0x1600,*originalBundle=(void*)0x1700;
unsigned char mainThread=1, createDone=0, assetDone=0, alive=1;
int sourceId=21,candidateId=22,originalBundleId=23,candidateBundleId=24,vertices=2,shapes=2,submeshes=1,frames=1;
float weight=100;
unsigned loadCalls=0,assetCalls=0,resultCalls=0,unloadCalls=0;
bool sourceRetired=false,aliasId=false,aliasBundle=false,wrongShapes=false,loadException=false;
std::string marker;
std::map<uint32_t,void*> handles;uint32_t nextHandle=0;
struct Header {void *klass=nullptr,*monitor=nullptr,*bounds=nullptr;uint64_t count=0;};
template<class T,size_t N>struct Array {Header header{(void*)sizeof(T),nullptr,nullptr,N};T values[N]{};};
Array<EiemShapeSnapshot::Vec3,2> vectors;
Array<EiemShapeSnapshot::Vec4,2> tangents;
Array<EiemCaptureSkin::Matrix,1> poses;
Array<NativeCandidate::Weight,2> weights;
Array<int,3> indices;
std::vector<std::unique_ptr<Bytes>> allocations;
void* MakeString(const std::string& text) {
 auto bytes=std::make_unique<Bytes>(20+text.size()*2);int size=int(text.size());std::memcpy(bytes->data()+16,&size,4);
 for(size_t i=0;i<text.size();++i){uint16_t c=uint8_t(text[i]);std::memcpy(bytes->data()+20+i*2,&c,2);}
 auto pointer=bytes->data();allocations.push_back(std::move(bytes));return pointer;
}
void* NewArray(void*,size_t count) {
 auto bytes=std::make_unique<Bytes>(32+count*12);Header h{(void*)12,nullptr,nullptr,count};std::memcpy(bytes->data(),&h,32);
 auto pointer=bytes->data();allocations.push_back(std::move(bytes));return pointer;
}
void* Invoke(void* method,void* self,void** args,void** exception){
 *exception=nullptr;
 auto code=(uintptr_t)method;
 if(code==1)return &mainThread;
 if(code==2){static unsigned char result;result=(!sourceRetired || args[0]!=renderer) && args[0]!=nullptr;return &result;}
 if(code==3){Require(mainThread,"load invoked off Unity thread");++loadCalls;Require(*(uint32_t*)args[1]==0 && *(uint64_t*)args[2]==0,"load crc/offset altered");if(loadException){*exception=(void*)1;return nullptr;}return create;}
 if(code==4)return self==create?&createDone:&assetDone;
 if(code==5){Require(createDone,"Bundle getter before completion");return bundle;}
 if(code==6){Require(self==bundle && args[1],"asset owner/type lost");++assetCalls;return asset;}
 if(code==7){Require(assetDone,"asset getter before completion");++resultCalls;return candidate;}
 if(code==8){Require(self==bundle && !*(unsigned char*)args[0] && createDone && (assetCalls==0 || assetDone),"early/wrong/destroying unload");++unloadCalls;return nullptr;}
 if(code==9){if(self==originalBundle)return &originalBundleId;if(self==bundle)return aliasBundle?&originalBundleId:&candidateBundleId;return self==source || aliasId?&sourceId:&candidateId;}
 if(code==10)return &submeshes;
 if(code==11)return MakeString(marker);
 if(code==12)return &vertices;
 if(code==13)return &shapes;
 if(code==14)return &alive;
 if(code==15)return &vectors;
 if(code==16)return &tangents;
 if(code==17)return &poses;
 if(code==18)return &weights;
 if(code==19)return &indices;
 if(code==20){int i=*(int*)args[0];return MakeString(i?"AnusOpen":"VaginaOpen");}
 if(code==21)return &frames;
 if(code==22)return &weight;
 if(code==23){if(wrongShapes && self==candidate){float f=1;std::memcpy((char*)args[2]+32,&f,4);}return nullptr;}
 throw std::runtime_error("unexpected candidate fixture managed call");
}
void Setup(){
 il2cpp_runtime_invoke=Invoke;il2cpp_object_unbox=[](void* o){return o;};
 il2cpp_string_new=[](const char* s){return MakeString(s);};il2cpp_array_new=NewArray;
 il2cpp_gchandle_new=[](void* o,bool)->uint32_t{handles[++nextHandle]=o;return nextHandle;};
 RuntimeShapes::Ref::weakHandle=[](void* o,bool)->uint32_t{handles[++nextHandle]=o;return nextHandle;};
 il2cpp_gchandle_get_target=[](uint32_t h)->void*{auto it=handles.find(h);return it==handles.end()?nullptr:it->second;};
 il2cpp_gchandle_free=[](uint32_t h){Require(handles.erase(h)==1,"double candidate handle release");};
 il2cpp_object_get_class=[](void* o)->void*{if(o==source || o==candidate)return meshClass;return *(void**)o;};
 il2cpp_class_get_element_class=[](void* k){return k;};il2cpp_class_value_size=[](void* k,uint32_t* align)->int32_t{*align=4;return int32_t(uintptr_t(k));};
 RuntimeShapes::api.main=(void*)1;RuntimeShapes::api.alive=(void*)2;
 RuntimeShapes::api.name=(void*)11;RuntimeShapes::api.vertexCount=(void*)12;RuntimeShapes::api.shapeCount=(void*)13;
 RuntimeShapes::api.readable=(void*)14;RuntimeShapes::api.vertices=(void*)15;RuntimeShapes::api.normals=(void*)15;
 RuntimeShapes::api.tangents=(void*)16;RuntimeShapes::api.bindPoses=(void*)17;RuntimeShapes::api.boneWeights=(void*)18;
 RuntimeShapes::api.indices=(void*)19;RuntimeShapes::api.shapeName=(void*)20;RuntimeShapes::api.frameCount=(void*)21;
 RuntimeShapes::api.frameWeight=(void*)22;RuntimeShapes::api.frameVertices=(void*)23;
 RuntimeShapes::originalMeshGetter=[](void*)->void*{return source;};
 NativeCandidate::api.load=(void*)3;NativeCandidate::api.done=(void*)4;NativeCandidate::api.bundle=(void*)5;
 NativeCandidate::api.assetLoad=(void*)6;NativeCandidate::api.result=(void*)7;NativeCandidate::api.unload=(void*)8;
 NativeCandidate::api.instanceId=(void*)9;NativeCandidate::api.submeshes=(void*)10;NativeCandidate::api.meshClass=meshClass;
 NativeCandidate::api.meshType=RuntimeShapes::Ref::Make(meshClass,false);
 NativeCandidate::enabled=true;RuntimeShapes::enabled=true;RuntimeShapes::managedAlive=true;
}
void Reset(){
 NativeCandidate::attempt.reset();NativeCandidate::pending.reset();NativeCandidate::busy=false;RuntimeShapes::bound.clear();LoadingObservation::bundles.clear();
 mainThread=1;createDone=assetDone=0;sourceRetired=aliasId=aliasBundle=wrongShapes=loadException=false;
 loadCalls=assetCalls=resultCalls=unloadCalls=0;active=true;allocations.clear();
}
void Rejects(const std::function<void()>& fn,const char* reason){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}Require(rejected,reason);}
void Write(const std::filesystem::path& path,const Bytes& bytes){std::ofstream out(path,std::ios::binary);out.write((const char*)bytes.data(),bytes.size());Require(bool(out),"fixture write");}
void Bind(const RuntimeControls::Spec& spec){RuntimeShapes::Bound b{RuntimeShapes::Ref::Make(renderer,true),RuntimeShapes::Ref::Make(source,false),spec.marker,{}};RuntimeShapes::bound.emplace(renderer,std::move(b));}
void Original(const std::shared_ptr<const NativeCandidate::Plan>& plan){
 LoadingObservation::Identity identity;identity.input=plan->input;identity.serializedHash=plan->originalHash;identity.weak=RuntimeShapes::Ref::Make(originalBundle,true);
 LoadingObservation::bundles.emplace(originalBundle,std::move(identity));
}
void Ready(const std::shared_ptr<const NativeCandidate::Plan>& plan){Original(plan);NativeCandidate::busy=true;NativeCandidate::pending=plan;Bind(plan->spec);NativeCandidate::Frame();Require(loadCalls==1 && NativeCandidate::attempt,"candidate load not started");}
void Complete(){createDone=1;NativeCandidate::Frame();assetDone=1;NativeCandidate::Frame();}
}
int main(int argc,char** argv){
 using namespace Fixture;
 try{
  Require(argc==6,"usage: native_candidate_smoke <mod> <v3 table> <game> <candidate file> <candidate config>");
  auto package=EiemDirectory::Load(std::filesystem::u8path(argv[1]));
  auto inputs=std::make_shared<InputSubstitutions>(ReadInputSubstitutions(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]),package));
  auto graph=std::make_shared<const std::vector<Package>>(std::vector<Package>{package});
  std::atomic_store(&published,graph);std::shared_ptr<const InputSubstitutions> immutable=inputs;std::atomic_store(&substitutions,immutable);
  auto sandbox=std::filesystem::absolute("diagnostics/resource-directory-entry-20261004/v31-native-body-candidate/fixture-"+std::to_string(GetCurrentProcessId()));
  Require(!std::filesystem::exists(sandbox),"fixture destination exists");gameDirectory=sandbox;
  auto destination=sandbox/"plugin/resource-input-candidates/v31-native-body-candidate/input-040.vfs";
  auto config=sandbox/"plugin/resource-input-diagnostics/native-candidate-validation.tsv";
  std::filesystem::create_directories(destination.parent_path());std::filesystem::create_directories(config.parent_path());
  std::filesystem::copy_file(std::filesystem::u8path(argv[4]),destination);
  auto configBytes=EiemDirectory::Read(std::filesystem::u8path(argv[5]));Write(config,configBytes);
  auto actual=NativeCandidate::ReadPlan(config);
  Require(actual->spec.vertices==14693 && actual->spec.channels.size()==2 && actual->bones>0 && actual->submeshes==1,"real body plan/layout differs");
  HANDLE writer=CreateFileW(destination.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
  Require(writer==INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION,"candidate file was writable during ownership");
  auto corrupt=configBytes;auto headerEnd=std::find(corrupt.begin(),corrupt.end(),'\n');auto digestStart=std::find(headerEnd+1,corrupt.end(),'\t');digestStart=std::find(digestStart+1,corrupt.end(),'\t');*(digestStart+1)^=1;
  Write(config,corrupt);Rejects([&]{NativeCandidate::ReadPlan(config);},"corrupt candidate config accepted");Write(config,configBytes);
  Setup();auto plan=std::make_shared<NativeCandidate::Plan>(*actual);plan->spec.vertices=2;plan->bones=1;plan->indices=3;marker=plan->marker;
  // Success: separate Unity ticks for requests, base arrays and native shape
  // frames. No dispatch off-thread, no duplicate preparation, no early unload.
  Original(plan);mainThread=0;NativeCandidate::busy=true;NativeCandidate::pending=plan;Bind(plan->spec);NativeCandidate::Frame();Require(loadCalls==0,"off-main frame initiated resources");mainThread=1;
  NativeCandidate::Frame();Require(loadCalls==1,"main-thread load absent");NativeCandidate::Frame();Require(assetCalls==0 && unloadCalls==0,"unfinished create request advanced/unloaded");
  NativeCandidate::Prepare();Require(loadCalls==1 && !NativeCandidate::pending,"repeat trigger queued another witness");
  createDone=1;NativeCandidate::Frame();Require(assetCalls==1 && resultCalls==0,"async asset not awaited");assetDone=1;NativeCandidate::Frame();
  for(int i=0;i<4;++i)NativeCandidate::Frame();Require(NativeCandidate::attempt->phase==NativeCandidate::Phase::Coexist,"native arrays/shape parity failed");
  NativeCandidate::attempt->coexistStarted=GetTickCount64()-10001;NativeCandidate::Frame();
  Require(NativeCandidate::attempt->phase==NativeCandidate::Phase::Passed && unloadCalls==1 && NativeCandidate::attempt->candidate.Get()==candidate && RuntimeShapes::SourceMesh(renderer)==source,
      "coexistence/owned unload lost witness or original Renderer");
  NativeCandidate::Frame();Require(unloadCalls==1 && loadCalls==1,"completed witness repeated native calls");
  // Consumer retires while bundle is in flight: preserve all roots until both
  // native requests finish; unload only the owned bundle with destroy=false.
  Reset();Ready(plan);sourceRetired=true;NativeCandidate::Frame();Require(NativeCandidate::attempt->phase==NativeCandidate::Phase::Rejected,"retired source accepted");
  NativeCandidate::Frame();Require(unloadCalls==0 && NativeCandidate::attempt->createRequest.Get()==create,"inflight failure lost root/early unload");createDone=1;NativeCandidate::Frame();
  Require(NativeCandidate::attempt->nativeFailed && unloadCalls==1,"completed abandoned bundle not drained");
  Reset();Ready(plan);createDone=1;NativeCandidate::Frame();active=false;NativeCandidate::Frame();NativeCandidate::Frame();
  Require(unloadCalls==0 && NativeCandidate::attempt->assetRequest.Get()==asset,"disabled plugin released pending asset request");assetDone=1;NativeCandidate::Frame();Require(unloadCalls==1,"disabled plugin did not drain completed owned request");
  // Different managed wrappers must not be considered distinct when Unity
  // GetInstanceID identifies the same native Mesh.
  Reset();Ready(plan);aliasId=true;Complete();Require(NativeCandidate::attempt->phase==NativeCandidate::Phase::Rejected,"native Mesh alias accepted");NativeCandidate::Frame();Require(unloadCalls==1,"alias failure not drained safely");
  Reset();Ready(plan);aliasBundle=true;createDone=1;NativeCandidate::Frame();NativeCandidate::Frame();
  Require(NativeCandidate::attempt->nativeFailed && unloadCalls==0 && NativeCandidate::attempt->bundle.Get()==bundle,"aliased original native Bundle was unloaded or released");
  Reset();Ready(plan);wrongShapes=true;Complete();for(int i=0;i<4;++i)NativeCandidate::Frame();Require(NativeCandidate::attempt->phase==NativeCandidate::Phase::Rejected,"altered native shape deltas accepted");NativeCandidate::Frame();Require(unloadCalls==1,"bad-shape candidate not drained");
  Reset();Original(plan);loadException=true;NativeCandidate::busy=true;NativeCandidate::pending=plan;Bind(plan->spec);NativeCandidate::Frame();Require(NativeCandidate::attempt && NativeCandidate::attempt->phase==NativeCandidate::Phase::Rejected,"managed load exception swallowed as success");NativeCandidate::Frame();Require(NativeCandidate::attempt->nativeFailed && unloadCalls==0,"null failed request cleanup invalid");
  Reset();NativeCandidate::api.meshType={};plan.reset();actual.reset();Require(handles.empty(),"candidate GC handles leaked");
  std::filesystem::remove(destination);std::filesystem::remove(config);
  std::cout<<"PASS native body candidate: real accepted body + namespace bytes verified; write lock and corrupt spec rejection; exact-main dispatch; asynchronous completion; native vertex/index/weight/bindpose and shape parity; distinct native IDs; repeat-trigger dedup; retired consumer/disable/inflight/Mesh-alias/Bundle-alias/shape/managed-exception failures drain with destroy=false; GC roots released; production renderer untouched\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
