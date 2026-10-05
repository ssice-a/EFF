#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
using namespace Entry;
namespace Fixture {
struct Invocation {int which;void *self,*info,*a=nullptr,*b=nullptr,*c=nullptr;bool flag=false;};
std::vector<Invocation> calls;bool fail=false,setResult=true;
#define ZERO(name,id) __declspec(noinline) void name(void* self,void* info){calls.push_back({id,self,info});if(fail)throw std::runtime_error("original failure");}
ZERO(Init,1) ZERO(Refind,2) ZERO(HelperRelease,3) ZERO(MaterialRelease,4) ZERO(LoadMain,8) ZERO(RemoveMain,9)
ZERO(ProxyUnload,15) ZERO(ProxyClear,16) ZERO(ProxyDispose,17) ZERO(ManagerOnDispose,18)
__declspec(noinline) void ProxyReleaseHandle(void* self,int handle,void* info){calls.push_back({14,self,info,(void*)(uintptr_t)handle});if(fail)throw std::runtime_error("original failure");}
__declspec(noinline) void* UnloadUnused(void* self,void* info){calls.push_back({19,self,info});if(fail)throw std::runtime_error("original failure");return (void*)0xBEEF;}
__declspec(noinline) void Material(void* self,void* a,void* b,void* c,bool flag,void* info){calls.push_back({5,self,info,a,b,c,flag});if(fail)throw std::runtime_error("original failure");}
__declspec(noinline) void Model(void* self,void* a,void* info){calls.push_back({6,self,info,a});if(fail)throw std::runtime_error("original failure");}
__declspec(noinline) void Unload(void* self,bool flag,void* info){calls.push_back({7,self,info,nullptr,nullptr,nullptr,flag});if(fail)throw std::runtime_error("original failure");}
__declspec(noinline) bool SetModel(void* self,void* a,void* info){calls.push_back({10,self,info,a});if(fail)throw std::runtime_error("original failure");return setResult;}
struct Method {void* mp;const char* name;std::vector<const char*> args;};
Method init{(void*)Init,"_InitRenderAndMaterial",{}},refind{(void*)Refind,"ReFindRenderersInChildren",{}},helperRelease{(void*)HelperRelease,"OnRelease",{}},materialRelease{(void*)MaterialRelease,"OnRelease",{}},material{(void*)Material,"Init",{"System.Collections.Generic.List<UnityEngine.Renderer>","System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>","Beyond.Rendering.EntityCustomizeRendererPropertyConfig","System.Boolean"}},model{(void*)Model,"ChangeMainModelSync",{"System.String"}},unload{(void*)Unload,"Unload",{"System.Boolean"}};
Method loadMain{(void*)LoadMain,"LoadMainModelSync",{}},removeMain{(void*)RemoveMain,"RemoveMainPart",{}},setModel{(void*)SetModel,"SetModelId",{"System.String"}};
struct Klass {const char* ns;const char* name;std::vector<Method*> methods;};
Klass helper{"Beyond.Gameplay.View","EntityRenderHelper",{&init,&refind,&helperRelease}},controller{"Beyond.Rendering","EntityRenderHelperMaterialController",{&material,&materialRelease}},component{"Beyond.Gameplay.View","ModelComponent",{&model}},bundle{"UnityEngine","AssetBundle",{&unload}},manager{"Beyond.Resource.Runtime","BundleResourceManager",{}},resource{"Beyond.Resource","ResourceManager",{}},capture{"Beyond.Rendering","EntityRenderCaptureMeshController",{}};
Klass baseModel{"Beyond.Gameplay.View","BaseModelComponent",{&loadMain,&removeMain,&setModel}};
std::array<Klass*,8> classes{&helper,&controller,&component,&bundle,&manager,&resource,&capture,&baseModel};
bool bad=false;
void InstallMetadata() {
  il2cpp_assembly_get_image=[](void* a){return a;};il2cpp_image_get_class_count=[](void*)->size_t{return classes.size();};il2cpp_image_get_class=[](void*,size_t index)->void*{return classes.at(index);};
  il2cpp_class_get_name=[](void* k){return ((Klass*)k)->name;};il2cpp_class_get_namespace=[](void* k){return ((Klass*)k)->ns;};
  il2cpp_class_get_methods=[](void* k,void** it)->void*{auto& methods=((Klass*)k)->methods;size_t i=(size_t)*it;if(i>=methods.size())return nullptr;*it=(void*)(i+1);return methods[i];};
  il2cpp_method_get_name=[](void* m){return ((Method*)m)->name;};il2cpp_method_get_param_count=[](void* m)->uint32_t{return unsigned(((Method*)m)->args.size());};
  il2cpp_method_get_param=[](void* m,uint32_t i)->void*{return (void*)((Method*)m)->args.at(i);};il2cpp_method_get_flags=[](void*,uint32_t* impl)->uint32_t{*impl=0;return 0;};
  il2cpp_method_get_return_type=[](void* m)->void*{return (void*)(bad?"System.Int32":m==&setModel?"System.Boolean":"System.Void");};il2cpp_type_get_name=[](void* t)->const char*{return _strdup((char*)t);};il2cpp_free=[](void* p){free(p);};
  il2cpp_field_get_name=[](void*){return "<unused>";};il2cpp_field_get_type=[](void*)->void*{return (void*)"System.Int32";};il2cpp_field_get_flags=[](void*)->int{return 0;};il2cpp_field_get_offset=[](void*)->size_t{return 0;};
  il2cpp_class_get_fields=[](void*,void**)->void*{return nullptr;};il2cpp_class_get_parent=[](void*)->void*{return nullptr;};il2cpp_object_get_class=[](void*)->void*{return &helper;};il2cpp_field_get_value=[](void*,void*,void*){};
}
}
int main() {
  using namespace Fixture;
  try {
    InstallMetadata();Require(MH_Initialize()==MH_OK,"MinHook init failed");void* assemblies[]={(void*)1};
    auto root=std::filesystem::absolute("diagnostics/resource-directory-entry-20261004/v27-full-reload-observation/smoke-"+std::to_string(GetCurrentProcessId()));
    Require(!std::filesystem::exists(root),"fixture already exists");gameDirectory=root;
    ReloadObservation::InstallOptional(assemblies,1);Require(!ReloadObservation::enabled && installed.empty(),"unmarked probe installs hooks");
    auto marker=root/"plugin/resource-input-diagnostics/full-reload-observation.enabled";std::filesystem::create_directories(marker.parent_path());{std::ofstream file(marker);file<<"smoke\n";}
    bad=true;ReloadObservation::InstallOptional(assemblies,1);Require(installed.empty() && calls.empty(),"wrong return metadata accepted or probe initiated game operation");bad=false;ReloadObservation::enabled=false;
    ReloadObservation::InstallOptional(assemblies,1);Require(installed.size()==10 && calls.empty(),"exact probe hooks failed or install invoked original");
    ReloadObservation::InstallOptional(assemblies,1);Require(installed.size()==10,"duplicate probe install");
    void* self=(void*)0x1234;void* info=(void*)0x5678;void* a=(void*)0x9012;void* b=(void*)0x3456;void* c=(void*)0x7890;
    ReloadObservation::ZeroFn volatile z1=Init,z2=Refind,z3=HelperRelease,z4=MaterialRelease;
    ReloadObservation::MaterialFn volatile mat=Fixture::Material;ReloadObservation::ModelFn volatile mod=Model;ReloadObservation::UnloadFn volatile unl=Unload;
    z1(self,info);z2(self,info);z3(self,info);z4(self,info);mat(self,a,b,c,true,info);mod(self,nullptr,info);unl(self,true,info);
    Require(calls.size()==7,"probe did not forward once");for(size_t i=0;i<calls.size();++i)Require(calls[i].which==int(i+1) && calls[i].self==self && calls[i].info==info,"probe changed instance/order/MethodInfo");
    Require(calls[4].a==a && calls[4].b==b && calls[4].c==c && calls[4].flag && calls[6].flag,"probe changed list/config/Boolean arguments");
    Require(!ReloadObservation::helperContext,"helper context leaked after diagnostic field failure");
    ReloadObservation::ZeroFn volatile load=LoadMain,remove=RemoveMain;ReloadObservation::BoolModelFn volatile set=SetModel;load(self,info);remove(self,info);Require(set(self,a,info),"Boolean true return changed");Require(calls.size()==10 && calls[7].which==8 && calls[8].which==9 && calls[9].which==10 && calls[9].a==a && !ReloadObservation::modelContext,"base model forwarding/context failed");
    fail=true;bool thrown=false;try{z2(self,info);}catch(const std::runtime_error&){thrown=true;}fail=false;
    Require(thrown && calls.size()==11 && !ReloadObservation::helperContext,"probe swallowed original exception/repeated call/leaked context");
    {std::lock_guard<std::mutex> lock(ReloadObservation::eventBudgetMutex);ReloadObservation::eventBudgets["EntityRenderHelper._InitRenderAndMaterial"]=128;}z1(self,info);Require(calls.size()==12,"event budget prevents original");
    ReloadObservation::enabled=false;unl(self,false,info);Require(calls.size()==13 && !calls.back().flag,"disabled observer prevents original or changes flag");ReloadObservation::enabled=true;
    ReloadObservation::originalProxyReleaseHandle=ProxyReleaseHandle;
    ReloadObservation::originalProxyUnload=ProxyUnload;
    ReloadObservation::originalProxyClear=ProxyClear;
    ReloadObservation::originalProxyDispose=ProxyDispose;
    ReloadObservation::originalProxyManagerOnDispose=ManagerOnDispose;
    ReloadObservation::originalBundleManagerUnloadUnused=UnloadUnused;
    ReloadObservation::ProxyReleaseHandle(self,42,info);
    ReloadObservation::ProxyUnload(self,info);
    ReloadObservation::ProxyClear(self,info);
    ReloadObservation::ProxyDispose(self,info);
    ReloadObservation::ProxyManagerOnDispose(self,info);
    Require(ReloadObservation::BundleManagerUnloadUnused(self,info)==(void*)0xBEEF,"IEnumerator return changed");
    Require(calls.size()==19,"new proxy/coroutine boundaries did not forward once");
    for(size_t i=13;i<calls.size();++i)Require(calls[i].which==int(i+1)&&calls[i].self==self&&calls[i].info==info,"new proxy/coroutine boundary changed argument or order");
    Require(calls[13].a==(void*)42,"proxy handle argument changed");
    for(int frame=0;frame<122;++frame)ReloadObservation::Frame(frame);Require(ReloadObservation::window.blocks==1,"frame summary missing");
    ReloadObservation::Prepared(true,ReloadObservation::Clock(),nullptr);ReloadObservation::Frame(123);Require(ReloadObservation::window.blocks==0 && ReloadObservation::window.request==1,"F10 did not restart measurement window");
    ReloadObservation::Committed(ReloadObservation::Clock(),true);
    for(auto address:installed){Require(MH_DisableHook(address)==MH_OK && MH_RemoveHook(address)==MH_OK,"probe hook teardown failed");}Require(MH_Uninitialize()==MH_OK,"MinHook shutdown failed");
    for(auto k:installed){(void)k;}installed.clear();ReloadObservation::enabled=false;ReloadObservation::sharedMaterial=false;RuntimeShapes::originalInit=nullptr;
    Require(MH_Initialize()==MH_OK,"shared-path MinHook init failed");
    Require(Install(&controller,"Init",{"System.Collections.Generic.List<UnityEngine.Renderer>","System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>","Beyond.Rendering.EntityCustomizeRendererPropertyConfig","System.Boolean"},"System.Void",false,(void*)RuntimeShapes::Init,(void**)&RuntimeShapes::originalInit),"production material detour fixture failed");
    calls.clear();RuntimeShapes::enabled=false;ReloadObservation::InstallOptional(assemblies,1);
    Require(installed.size()==10 && ReloadObservation::ShareMaterialInit() && ReloadObservation::originalMaterial==RuntimeShapes::originalInit,"existing material detour was not shared");
    mat(self,a,b,c,true,info);Require(calls.size()==1 && calls[0].which==5 && calls[0].self==self && calls[0].info==info && calls[0].a==a && calls[0].b==b && calls[0].c==c && calls[0].flag,"shared material entry failed once-only/ABI forwarding");
    fail=true;thrown=false;try{mat(self,a,b,c,false,info);}catch(const std::runtime_error&){thrown=true;}fail=false;Require(thrown && calls.size()==2,"shared material entry swallowed/repeated exception");
    setResult=false;Require(!set(self,a,info),"Boolean false return changed");
    for(auto address:installed){Require(MH_DisableHook(address)==MH_OK && MH_RemoveHook(address)==MH_OK,"shared probe hook teardown failed");}Require(MH_Uninitialize()==MH_OK,"shared MinHook shutdown failed");
    std::filesystem::remove(marker);std::filesystem::remove(marker.parent_path());std::filesystem::remove(root/"plugin");std::filesystem::remove(root);
    std::cout<<"PASS optional reload observation: marker gate, exact signature rejection, 10 real MinHook detours, original-only install, unchanged args/MethodInfo, diagnostic field failure preserves return, original exceptions/TLS, budget/disabled forwarding, QPC frame windows and F10 reset; proxy lifecycle forwarding and IEnumerator return; existing production Init sharing without duplicate detour, true/false Boolean SetModelId returns\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
