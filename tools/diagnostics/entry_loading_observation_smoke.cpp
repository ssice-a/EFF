#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
using namespace Entry;
namespace Fixture {
struct Invocation{void* self;int64_t hash;void* type;void* name=nullptr;};
std::vector<Invocation> calls;
void* bundleResult=(void*)0x2020;
void* assetResult=(void*)0x3030;
void* asyncResult=(void*)0xA010;
struct ManagedString{void* klass=nullptr;void* monitor=nullptr;int length=3;wchar_t text[4]={L'L',L'Z',L'Y',0};};
ManagedString assetName;
bool fail=false,bad=false,badClass=false;
__declspec(noinline) void* NativeBundle(void* self){calls.push_back({self,0,nullptr});if(fail)throw std::runtime_error("original failure");return bundleResult;}
__declspec(noinline) void* NativeHash(void* self,int64_t hash,void* type){calls.push_back({self,hash,type});if(fail)throw std::runtime_error("original failure");return assetResult;}
__declspec(noinline) void* ManagedBundle(void* self,void*){return NativeBundle(self);}
__declspec(noinline) void* NativeString(void* self,void* name,void* type){calls.push_back({self,0,type,name});if(fail)throw std::runtime_error("original failure");return assetResult;}
__declspec(noinline) void* NativeHashAsync(void* self,int64_t hash,void* type){calls.push_back({self,hash,type});if(fail)throw std::runtime_error("original failure");return asyncResult;}
__declspec(noinline) void* NativeStringAsync(void* self,void* name,void* type){calls.push_back({self,0,type,name});if(fail)throw std::runtime_error("original failure");return asyncResult;}
__declspec(noinline) void* NativeResult(void* self){calls.push_back({self,0,nullptr});if(fail)throw std::runtime_error("original failure");return assetResult;}
__declspec(noinline) void* ManagedStringAsset(void* self,void* name,void* type,void*){return NativeString(self,name,type);}

struct Method{void* mp;const char* name;const char* result;std::vector<const char*> args;};
Method getter{(void*)NativeBundle,"get_assetBundle","UnityEngine.AssetBundle",{}},hashLoad{(void*)NativeHash,"LoadAssetByHash_Internal","UnityEngine.Object",{"System.Int64","System.Type"}};
Method stringLoad{(void*)NativeString,"LoadAsset_Internal","UnityEngine.Object",{"System.String","System.Type"}},asyncHash{(void*)NativeHashAsync,"LoadAssetAsyncByHash_Internal","UnityEngine.AssetBundleRequest",{"System.Int64","System.Type"}},asyncString{(void*)NativeStringAsync,"LoadAssetAsync_Internal","UnityEngine.AssetBundleRequest",{"System.String","System.Type"}},getResult{(void*)NativeResult,"GetResult","UnityEngine.Object",{}};
struct Klass{const char* ns;const char* name;std::vector<Method*> methods;};
Klass request{"UnityEngine","AssetBundleCreateRequest",{&getter}},bundle{"UnityEngine","AssetBundle",{&hashLoad,&stringLoad,&asyncHash,&asyncString}},assetRequest{"UnityEngine","AssetBundleRequest",{&getResult}},mesh{"UnityEngine","Mesh",{}};
std::array<Klass*,4> classes{&request,&bundle,&assetRequest,&mesh};
std::map<uint32_t,void*> handles;uint32_t nextHandle=0;unsigned strongCalls=0;
uint32_t Weak(void* object,bool trackResurrection){Require(!trackResurrection,"unexpected resurrection tracking");auto id=++nextHandle;handles[id]=object;return id;}
void InstallMetadata(){
 il2cpp_assembly_get_image=[](void* a){return a;};il2cpp_image_get_class_count=[](void*)->size_t{return classes.size();};il2cpp_image_get_class=[](void*,size_t index)->void*{return classes.at(index);};
 il2cpp_class_get_name=[](void* k){return ((Klass*)k)->name;};il2cpp_class_get_namespace=[](void* k){return ((Klass*)k)->ns;};
 il2cpp_class_get_methods=[](void* k,void** it)->void*{auto& methods=((Klass*)k)->methods;size_t i=(size_t)*it;if(i>=methods.size())return nullptr;*it=(void*)(i+1);return methods[i];};
 il2cpp_method_get_name=[](void* m){return ((Method*)m)->name;};il2cpp_method_get_param_count=[](void* m)->uint32_t{return unsigned(((Method*)m)->args.size());};
 il2cpp_method_get_param=[](void* m,uint32_t i)->void*{return (void*)((Method*)m)->args.at(i);};il2cpp_method_get_flags=[](void*,uint32_t* impl)->uint32_t{*impl=0;return 0;};
 il2cpp_method_get_return_type=[](void* m)->void*{return (void*)(bad?"System.Int32":((Method*)m)->result);};il2cpp_type_get_name=[](void* t)->const char*{return _strdup((char*)t);};il2cpp_free=[](void* p){free(p);};
 il2cpp_object_get_class=[](void*)->void*{if(badClass)throw std::runtime_error("diagnostic class read failure");return &mesh;};
 il2cpp_gchandle_new=[](void*,bool)->uint32_t{++strongCalls;return 0;};il2cpp_gchandle_get_target=[](uint32_t h)->void*{auto it=handles.find(h);return it==handles.end()?nullptr:it->second;};il2cpp_gchandle_free=[](uint32_t h){Require(handles.erase(h)==1,"double weak handle release");};
 RuntimeShapes::Ref::weakHandle=Weak;
 il2cpp_resolve_icall=[](const char* name)->void*{
  if(!strcmp(name,"UnityEngine.AssetBundleCreateRequest::get_assetBundle()"))return (void*)NativeBundle;
  if(!strcmp(name,"UnityEngine.AssetBundle::LoadAssetByHash_Internal(System.Int64,System.Type)"))return (void*)NativeHash;
  if(!strcmp(name,"UnityEngine.AssetBundle::LoadAsset_Internal(System.String,System.Type)"))return (void*)NativeString;
  if(!strcmp(name,"UnityEngine.AssetBundle::LoadAssetAsyncByHash_Internal(System.Int64,System.Type)"))return (void*)NativeHashAsync;
  if(!strcmp(name,"UnityEngine.AssetBundle::LoadAssetAsync_Internal(System.String,System.Type)"))return (void*)NativeStringAsync;
  if(!strcmp(name,"UnityEngine.AssetBundleRequest::GetResult()"))return (void*)NativeResult;

  return nullptr;
 };
}
}
int main(){
 using namespace Fixture;
 try{
  InstallMetadata();Require(MH_Initialize()==MH_OK,"MinHook init failed");void* assemblies[]={(void*)1};
  LoadingObservation::InstallOptional(assemblies,1);Require(installed.empty() && calls.empty(),"disabled observer installed/called native loader");
  ReloadObservation::enabled=true;bad=true;LoadingObservation::InstallOptional(assemblies,1);Require(installed.empty() && calls.empty(),"incompatible ABI accepted");bad=false;
  LoadingObservation::InstallOptional(assemblies,1);Require(LoadingObservation::Active() && installed.size()==6 && calls.empty(),"native bindings unavailable or install initiated resources");
  LoadingObservation::InstallOptional(assemblies,1);Require(installed.size()==6,"duplicate native install");
  LoadingObservation::BundleGetterFn volatile get=NativeBundle;LoadingObservation::HashAssetFn volatile hash=NativeHash;
  void* req=(void*)0x1010;void* type=(void*)0x4040;
  submittedRequests[req]="input-1.vfs";LoadingObservation::Submitted(req,"input-1.vfs",7,true,"fixture-digest");
  auto result=get(req);Require(result==bundleResult && calls.size()==1 && calls.back().self==req,"completion forwarding changed");
  Require(LoadingObservation::pending.empty() && LoadingObservation::bundles.at(bundleResult).inputGeneration==7 && completedInputs==1,"weak completion attribution failed");
  requestBundle=ManagedBundle;RequestBundle(req,(void*)0x5050);Require(calls.size()==2 && completedInputs==1,"nested managed/native observation double counted completion");
  Require(hash(bundleResult,INT64_MIN+19,type)==assetResult && calls.size()==3 && calls.back().hash==INT64_MIN+19 && calls.back().type==type,"native hash/type/return forwarding changed");Require(loadedAssets==1,"matched asset not observed");
  badClass=true;Require(hash(bundleResult,42,type)==assetResult && calls.size()==4,"diagnostic failure changed native result");badClass=false;
  fail=true;bool threw=false;try{hash(bundleResult,12,type);}catch(const std::runtime_error&){threw=true;}fail=false;Require(threw && calls.size()==5,"native original exception swallowed or original repeated");
  LoadingObservation::HashAssetFn volatile hashAsync=NativeHashAsync;LoadingObservation::StringAssetFn volatile stringAsync=NativeStringAsync;LoadingObservation::BundleGetterFn volatile assetGet=NativeResult;
  auto nativeBundle=(void*)0x2020;
  auto matchedBefore=loadedAssets.load();auto callBefore=calls.size();loadAsset=ManagedStringAsset;Require(LoadAsset(nativeBundle,&assetName,type,(void*)0x5050)==assetResult && calls.size()==callBefore+1 && calls.back().name==&assetName && calls.back().type==type && loadedAssets==matchedBefore+1,"managed/native String shared result double counted or arguments changed");
  callBefore=calls.size();Require(hashAsync(nativeBundle,INT64_MIN+23,type)==asyncResult && calls.size()==callBefore+1 && calls.back().hash==INT64_MIN+23 && calls.back().type==type,"async hash ABI/return changed");
  Require(LoadingObservation::assetRequests.at(asyncResult).origin.serializedHash=="fixture-digest" && LoadingObservation::assetRequests.at(asyncResult).hash==INT64_MIN+23,"async request origin/hash not retained");
  assetResult=nullptr;Require(assetGet(asyncResult)==nullptr && LoadingObservation::assetRequests.count(asyncResult),"null asset treated as completed");
  assetResult=(void*)0x3030;matchedBefore=loadedAssets.load();Require(assetGet(asyncResult)==assetResult && loadedAssets==matchedBefore+1 && !LoadingObservation::assetRequests.count(asyncResult),"async object completion/identity failed");assetGet(asyncResult);Require(loadedAssets==matchedBefore+1,"repeated completed getter double counted");
  asyncResult=(void*)0xA020;callBefore=calls.size();Require(stringAsync(nativeBundle,&assetName,type)==asyncResult && calls.size()==callBefore+1 && calls.back().name==&assetName && calls.back().type==type,"async String ABI/return changed");Require(LoadingObservation::assetRequests.at(asyncResult).label=="LZY","async String label lifetime not copied");
  auto expiredAsset=LoadingObservation::assetRequests.at(asyncResult).weak.handle;handles[*expiredAsset]=nullptr;matchedBefore=loadedAssets.load();assetGet(asyncResult);Require(loadedAssets==matchedBefore,"reused async request pointer correlated with expired identity");expiredAsset.reset();LoadingObservation::Frame(120);Require(!LoadingObservation::assetRequests.count(asyncResult),"expired asset request retained");
  asyncResult=(void*)0xA030;hashAsync(nativeBundle,42,type);callBefore=calls.size();fail=true;threw=false;try{assetGet(asyncResult);}catch(const std::runtime_error&){threw=true;}fail=false;Require(threw && calls.size()==callBefore+1 && LoadingObservation::assetRequests.count(asyncResult),"GetResult original exception changed or identity consumed");
  LoadingObservation::Unloaded(nativeBundle);matchedBefore=loadedAssets.load();Require(assetGet(asyncResult)==assetResult && loadedAssets==matchedBefore+1,"valid outstanding asset request lost origin after natural bundle unload");
  LoadingObservation::Submitted(nativeBundle,"input-1.vfs",7,false,"fixture-digest");
  submittedRequests[(void*)0x6060]="null.vfs";
LoadingObservation::Submitted((void*)0x6060,"null.vfs",8,true,"fixture-digest");bundleResult=nullptr;Require(get((void*)0x6060)==nullptr && LoadingObservation::pending.count((void*)0x6060) && completedInputs==1,"null result was treated as completed or return changed");
  auto expired=LoadingObservation::pending.at((void*)0x6060).weak.handle;handles[*expired]=nullptr;auto completedBefore=completedInputs.load();bundleResult=(void*)0x7070;get((void*)0x6060);Require(completedInputs==completedBefore && !LoadingObservation::bundles.count(bundleResult),"reused pointer associated with expired request");expired.reset();
  LoadingObservation::Frame(120);Require(!LoadingObservation::pending.count((void*)0x6060) && !submittedRequests.count((void*)0x6060),"expired request not retired");
  submittedRequests[(void*)0x6060]="new.vfs";LoadingObservation::Submitted((void*)0x6060,"new.vfs",9,true,"fixture-digest");get((void*)0x6060);Require(LoadingObservation::bundles.at(bundleResult).input=="new.vfs" && LoadingObservation::bundles.at(bundleResult).inputGeneration==9,"new request weak identity not accepted");
  submittedRequests[(void*)0x9090]="same-content.vfs";LoadingObservation::Submitted((void*)0x9090,"same-content.vfs",10,true,"fixture-digest");get((void*)0x9090);Require(LoadingObservation::bundles.at(bundleResult).inputGeneration==10,"configuration publication falsely treated as different native content");
  submittedRequests[(void*)0x9091]="changed-content.vfs";LoadingObservation::Submitted((void*)0x9091,"changed-content.vfs",11,true,"changed-digest");get((void*)0x9091);Require(LoadingObservation::bundles.at(bundleResult).serializedHash=="fixture-digest" && LoadingObservation::bundles.at(bundleResult).inputGeneration==10,"same live bundle accepted with conflicting serialized content");
  LoadingObservation::Unloaded(bundleResult);Require(!LoadingObservation::bundles.count(bundleResult) && !submittedBundles.count(bundleResult),"natural unload observation retained identity");
  auto oldBundle=(void*)0x2020;auto weakBundle=LoadingObservation::bundles.at(oldBundle).weak.handle;handles[*weakBundle]=nullptr;auto loadedBefore=loadedAssets.load();hash(oldBundle,0,type);Require(loadedAssets==loadedBefore,"expired bundle matched new object at same pointer");weakBundle.reset();LoadingObservation::Frame(240);Require(!LoadingObservation::bundles.count(oldBundle),"expired bundle not retired");
  LoadingObservation::Submitted((void*)0xB010,"limit-source.vfs",11,false,"fixture-digest");
  for(size_t i=0;i<LoadingObservation::identityLimit+10;++i)LoadingObservation::Submitted((void*)(uintptr_t(0x10000)+32*i),"bounded.vfs",11,true,"fixture-digest");
  Require(LoadingObservation::pending.size()+LoadingObservation::bundles.size()+LoadingObservation::assetRequests.size()==LoadingObservation::identityLimit,"observation identities unbounded");
  asyncResult=(void*)0xB020;auto capCalls=calls.size();Require(hashAsync((void*)0xB010,23,type)==asyncResult && calls.size()==capCalls+1 && !LoadingObservation::assetRequests.count(asyncResult),"asset identity budget changed native result or exceeded capacity");
  asyncResult=(void*)0xA040;hashAsync((void*)0xA050,1,type);Require(!LoadingObservation::assetRequests.count(asyncResult),"untracked bundle associated with asset request");
  LoadingObservation::samples=256;auto before=calls.size();hash((void*)0x8080,999,type);Require(calls.size()==before+1 && calls.back().hash==999,"budget stopped native original");
  LoadingObservation::enabled=false;before=calls.size();hash((void*)0x8080,INT64_MAX,type);Require(calls.size()==before+1 && calls.back().hash==INT64_MAX,"disabled observer changed native forwarding");
  LoadingObservation::pending.clear();LoadingObservation::bundles.clear();LoadingObservation::assetRequests.clear();Require(handles.empty() && strongCalls==0,"weak identity leak or strong retention");
  for(auto address:installed)Require(MH_DisableHook(address)==MH_OK && MH_RemoveHook(address)==MH_OK,"native hook teardown failed");Require(MH_Uninitialize()==MH_OK,"MinHook teardown failed");
  std::cout<<"PASS: exact ABI, real native MinHook, once-only forwarding, hash/type/return/exception preservation, shared managed/native bundle and String forwarding, both async routes and GetResult origin, null/repeated completion, weak GC request/bundle identity/reuse/unload/expiry and outstanding result after bundle release, bounded diagnostics, disabled/budget forwarding; no resource calls initiated.\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
