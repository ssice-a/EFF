#pragma once
#include <array>
#include <exception>
// Optional observation only. Exact managed signatures; every intercepted
// operation calls its original exactly once with unchanged arguments/MethodInfo.
// No resource reload, release, model change, or Renderer write is initiated here.
namespace ReloadObservation {
std::atomic<bool> enabled{false};
std::atomic<unsigned> events{0},assets{0},requests{0};
std::atomic<int> observedFrame{-1};
uint64_t frequency=0;
thread_local void* helperContext=nullptr;
thread_local void* modelContext=nullptr;
using ZeroFn=void(*)(void*,void*);
using HandleFn=void(*)(void*,int,void*);
using EnumeratorFn=void*(*)(void*,void*);
using ModelFn=void(*)(void*,void*,void*);
using BoolModelFn=bool(*)(void*,void*,void*);
using AnimationChangeFn=bool(*)(void*,void*,bool,void*);
using MaterialFn=void(*)(void*,void*,void*,void*,bool,void*);
using UnloadFn=void(*)(void*,bool,void*);
ZeroFn originalInit=nullptr,originalRefind=nullptr,originalHelperRelease=nullptr,originalMaterialRelease=nullptr;
ModelFn originalModel=nullptr;
BoolModelFn originalSetModel=nullptr;
std::atomic<bool> sharedMaterial{false};
bool ShareMaterialInit(){return enabled.load(std::memory_order_relaxed) && sharedMaterial.load(std::memory_order_acquire);}
ZeroFn originalLoadMain=nullptr,originalRemoveMain=nullptr;
ZeroFn originalProxyLoadImmediate=nullptr,originalProxyRelease=nullptr;
ZeroFn originalProxyUnload=nullptr,originalProxyClear=nullptr,originalProxyDispose=nullptr;
HandleFn originalProxyReleaseHandle=nullptr;
ZeroFn originalProxyManagerRelease=nullptr,originalProxyManagerUnloadAll=nullptr;
ZeroFn originalProxyManagerOnDispose=nullptr,originalModelLoaded=nullptr;
EnumeratorFn originalBundleManagerUnloadUnused=nullptr;
AnimationChangeFn originalAnimationChange=nullptr;
MaterialFn originalMaterial=nullptr;
UnloadFn originalUnload=nullptr;
uint64_t Clock() {
  if(!enabled.load(std::memory_order_relaxed)) return 0;
  LARGE_INTEGER t{};return QueryPerformanceCounter(&t)?uint64_t(t.QuadPart):0;
}
double Milliseconds(uint64_t first,uint64_t last) {
  return first && frequency && last>=first?double(last-first)*1000.0/double(frequency):0;
}
bool Budget(std::atomic<unsigned>& counter,unsigned limit) {
  if(!enabled.load(std::memory_order_relaxed)) return false;
  return counter.fetch_add(1,std::memory_order_relaxed)<limit;
}
void* Field(void* object,const char* name,const char* expected) {
  Require(object && il2cpp_object_get_class && il2cpp_class_get_fields && il2cpp_field_get_value,"reload probe field API unavailable");
  auto klass=il2cpp_object_get_class(object);
  for(unsigned depth=0;klass && depth<32;++depth,klass=il2cpp_class_get_parent(klass)) {
    void* it=nullptr;
    while(auto f=il2cpp_class_get_fields(klass,&it)) {
      auto n=il2cpp_field_get_name(f);if(!n || strcmp(n,name)) continue;
      Require(!(il2cpp_field_get_flags(f)&0x10),"reload probe refuses static field");
      auto type=il2cpp_field_get_type(f);auto text=type?il2cpp_type_get_name(type):nullptr;
      bool correct=text && (expected?!strcmp(text,expected):(!strncmp(text,"System.Collections.Generic.List<",32) && text[strlen(text)-1]=='>'));if(text)il2cpp_free((void*)text);
      Require(correct,"reload probe field type mismatch");return f;
    }
  }
  throw std::runtime_error("reload probe field not found");
}
template<class T> T ReadField(void* object,const char* name,const char* expected) {
  T value{};il2cpp_field_get_value(object,Field(object,name,expected),&value);return value;
}
int ListCount(void* list) {
  if(!list) return 0;
  int count=ReadField<int>(list,"_size","System.Int32");
  Require(count>=0 && count<=4096,"reload probe list count outside budget");return count;
}
void HelperState(void* owner,const char* phase) {
  if(!owner) return;
  auto material=ReadField<void*>(owner,"m_rendererAndMaterialController","Beyond.Rendering.EntityRenderHelperMaterialController");
  auto capture=ReadField<void*>(owner,"m_captureMeshController","Beyond.Rendering.EntityRenderCaptureMeshController");
  Log("[RELOAD-PROBE-HELPER] phase=%s helper=%p materialController=%p captureController=%p frame=%d generation=%u",phase,owner,material,capture,observedFrame.load(),generation.load());
}
bool IsSkinned(void* object) {
  if(!object)return false;
  for(auto k=il2cpp_object_get_class(object);k;k=il2cpp_class_get_parent(k)) {
    auto n=il2cpp_class_get_name(k),ns=il2cpp_class_get_namespace(k);
    if(n && ns && !strcmp(n,"SkinnedMeshRenderer") && !strcmp(ns,"UnityEngine"))return true;
  }
  return false;
}
void MaterialState(void* controller,void* list) {
  int count=ListCount(list);
  auto infos=ReadField<void*>(controller,"m_rendererInfos",nullptr);
  int registered=ListCount(infos);
  Log("[RELOAD-PROBE-ASSEMBLY] modelComponent=%p helper=%p materialController=%p inputRenderers=%d registeredInfos=%d frame=%d generation=%u",modelContext,helperContext,controller,count,registered,observedFrame.load(),generation.load());
  if(!list || !RuntimeShapes::enabled || !RuntimeShapes::Main())return;
  auto items=ReadField<void*>(list,"_items","UnityEngine.Renderer[]");
  size_t length=0;Require(ArrayLength(items,&length) && length>=size_t(count) && length<=4096,"reload probe Renderer array mismatch");
  auto renderers=RuntimeShapes::Array<void*>(items,length);
  unsigned detailed=0;
  for(int i=0;i<count;++i) {
    auto renderer=renderers[i];if(!renderer || !IsSkinned(renderer) || !RuntimeShapes::Alive(renderer))continue;
    auto mesh=RuntimeShapes::SourceMesh(renderer);if(!mesh || !RuntimeShapes::Alive(mesh))continue;
    auto name=String(RuntimeShapes::Call(RuntimeShapes::api.name,mesh));
    // Print authored parts first; ordinary meshes are represented by the list
    // counts above. This is evidence filtering, not a production matching rule.
    if(!RuntimeControls::specs.count(name) || detailed++>=64)continue;
    auto bones=RuntimeShapes::Call(RuntimeShapes::api.bones,renderer);size_t slots=0;
    Require(ArrayLength(bones,&slots) && slots<=4096,"reload probe bone array mismatch");
    auto bindposes=RuntimeShapes::Call(RuntimeShapes::api.bindPoses,mesh);size_t bindCount=0;
    Require(ArrayLength(bindposes,&bindCount) && bindCount<=4096,"reload probe bindpose array mismatch");
    auto root=RuntimeShapes::Call(RuntimeShapes::api.rootBone,renderer);
    auto skin=RuntimeShapes::api.skinningRoot?RuntimeShapes::Call(RuntimeShapes::api.skinningRoot,renderer):nullptr;
    Log("[RELOAD-PROBE-PART] helper=%p controller=%p renderer=%p mesh=%p marker=%s vertices=%d shapes=%d boneSlots=%zu bindposes=%zu rootBone=%p skinningRoot=%p generation=%u",helperContext,controller,renderer,mesh,name.c_str(),RuntimeShapes::Scalar<int>(RuntimeShapes::api.vertexCount,mesh),RuntimeShapes::Scalar<int>(RuntimeShapes::api.shapeCount,mesh),slots,bindCount,root,skin,generation.load());
  }
}
std::mutex eventBudgetMutex;
std::map<std::string,unsigned> eventBudgets;
bool SampleEvent(const char* name){std::lock_guard<std::mutex> lock(eventBudgetMutex);return eventBudgets[name]++<128;}
struct Event {
  const char* name;void *self,*info;uint64_t start=0,originalEnd=0;bool sampled=false;unsigned id=0;int exceptions=0;
  Event(const char* n,void* s,void* i):name(n),self(s),info(i),exceptions(std::uncaught_exceptions()) {
    if(enabled){id=events.fetch_add(1,std::memory_order_relaxed)+1;sampled=SampleEvent(name);}
    if(sampled)Observe([&]{Log("[RELOAD-PROBE-EVENT] phase=enter id=%u name=%s self=%p methodInfo=%p helper=%p modelComponent=%p frame=%d generation=%u thread=%lu",id,name,self,info,helperContext,modelContext,observedFrame.load(),generation.load(),GetCurrentThreadId());});
    start=Clock();
  }
  void OriginalReturned(){originalEnd=Clock();}
  ~Event() noexcept {
    if(sampled)try {Log("[RELOAD-PROBE-EVENT] phase=return id=%u name=%s self=%p methodInfo=%p helper=%p originalElapsedMs=%.6f observerSnapshotMs=%.6f originalThrew=%d frame=%d generation=%u",id,name,self,info,helperContext,Milliseconds(start,originalEnd?originalEnd:Clock()),originalEnd?Milliseconds(originalEnd,Clock()):0,std::uncaught_exceptions()>exceptions,observedFrame.load(),generation.load());}catch(...){}
  }
};
struct ModelScope {
  void* before;ModelScope(void* component):before(modelContext){modelContext=component;}
  ~ModelScope(){modelContext=before;}
};
struct HelperScope {
  void* before;HelperScope(void* owner):before(helperContext){helperContext=owner;}
  ~HelperScope(){helperContext=before;}
};
void Init(void* self,void* info) {
  HelperScope scope(self);Event event("EntityRenderHelper._InitRenderAndMaterial",self,info);
  originalInit(self,info);event.OriginalReturned();
  if(event.sampled)Observe([&]{HelperState(self,"init-return");});
}
void Refind(void* self,void* info) {
  HelperScope scope(self);Event event("EntityRenderHelper.ReFindRenderersInChildren",self,info);
  originalRefind(self,info);event.OriginalReturned();
  if(event.sampled)Observe([&]{HelperState(self,"refind-return");});
}
void HelperRelease(void* self,void* info) {
  HelperScope scope(self);Event event("EntityRenderHelper.OnRelease",self,info);originalHelperRelease(self,info);event.OriginalReturned();
}
void Material(void* self,void* renderers,void* types,void* config,bool bounds,void* info) {
  Event event("EntityRenderHelperMaterialController.Init",self,info);
  originalMaterial(self,renderers,types,config,bounds,info);event.OriginalReturned();
  if(event.sampled)Observe([&]{MaterialState(self,renderers);});
}
void MaterialRelease(void* self,void* info) {
  Event event("EntityRenderHelperMaterialController.OnRelease",self,info);originalMaterialRelease(self,info);event.OriginalReturned();
}
void Model(void* self,void* id,void* info) {
  ModelScope scope(self);Event event("ModelComponent.ChangeMainModelSync",self,info);
  if(event.sampled)Observe([&]{Log("[RELOAD-PROBE-MODEL] component=%p newModelId=%s",self,String(id).c_str());});
  originalModel(self,id,info);event.OriginalReturned();
}
bool SetModel(void* self,void* id,void* info) {
  ModelScope scope(self);Event event("BaseModelComponent.SetModelId",self,info);
  auto result=originalSetModel(self,id,info);event.OriginalReturned();
  return result;
}
void LoadMain(void* self,void* info) {
  ModelScope scope(self);Event event("BaseModelComponent.LoadMainModelSync",self,info);
  originalLoadMain(self,info);event.OriginalReturned();
}
void RemoveMain(void* self,void* info) {
  ModelScope scope(self);Event event("BaseModelComponent.RemoveMainPart",self,info);
  originalRemoveMain(self,info);event.OriginalReturned();
}
void ProxyLoadImmediate(void* self,void* info) {
  Event event("PrefabInstantiateProxy.LoadImmediate",self,info);
  originalProxyLoadImmediate(self,info);event.OriginalReturned();
}
void ProxyRelease(void* self,void* info) {
  Event event("PrefabInstantiateProxy.Release",self,info);
  originalProxyRelease(self,info);event.OriginalReturned();
}
void ProxyReleaseHandle(void* self,int handle,void* info) {
  Event event("PrefabInstantiateProxy.ReleaseHandle",self,info);
  if(event.sampled)Observe([&]{Log("[RELOAD-PROBE-HANDLE] proxy=%p handle=%d",self,handle);});
  originalProxyReleaseHandle(self,handle,info);event.OriginalReturned();
}
void ProxyUnload(void* self,void* info) {
  Event event("PrefabInstantiateProxy.Unload",self,info);
  originalProxyUnload(self,info);event.OriginalReturned();
}
void ProxyClear(void* self,void* info) {
  Event event("PrefabInstantiateProxy.Clear",self,info);
  originalProxyClear(self,info);event.OriginalReturned();
}
void ProxyDispose(void* self,void* info) {
  Event event("PrefabInstantiateProxy.Dispose",self,info);
  originalProxyDispose(self,info);event.OriginalReturned();
}
void ProxyManagerRelease(void* self,void* info) {
  Event event("PrefabInstantiateProxyManager.Release",self,info);
  originalProxyManagerRelease(self,info);event.OriginalReturned();
}
void ProxyManagerUnloadAll(void* self,void* info) {
  Event event("PrefabInstantiateProxyManager.UnloadAll",self,info);
  originalProxyManagerUnloadAll(self,info);event.OriginalReturned();
}
void ProxyManagerOnDispose(void* proxy,void* info) {
  Event event("PrefabInstantiateProxyManager.OnProxyDispose",proxy,info);
  originalProxyManagerOnDispose(proxy,info);event.OriginalReturned();
}
void* BundleManagerUnloadUnused(void* self,void* info) {
  Event event("BundleResourceManager.UnloadUnusedAssets",self,info);
  auto coroutine=originalBundleManagerUnloadUnused(self,info);event.OriginalReturned();
  if(event.sampled)Observe([&]{Log("[RELOAD-PROBE-COROUTINE] owner=%p iterator=%p creationOnly=1",self,coroutine);});
  return coroutine;
}
void ModelLoaded(void* self,void* info) {
  ModelScope scope(self);Event event("BaseModelComponent.OnModelLoaded",self,info);
  originalModelLoaded(self,info);event.OriginalReturned();
}
bool AnimationChange(void* self,void* data,bool resume,void* info) {
  Event event("ComplexAnimatorComponent.ChangeAnimationConfig",self,info);
  if(event.sampled)Observe([&]{Log("[RELOAD-PROBE-ANIMATION] component=%p data=%p resume=%d",self,data,int(resume));});
  auto result=originalAnimationChange(self,data,resume,info);event.OriginalReturned();
  if(event.sampled)Observe([&]{Log("[RELOAD-PROBE-ANIMATION] component=%p result=%d",self,int(result));});
  return result;
}
void Unload(void* self,bool destroy,void* info) {
  Event event("AssetBundle.Unload",self,info);
  if(event.sampled)Observe([&]{Log("[RELOAD-PROBE-UNLOAD] bundle=%p unloadAllLoadedObjects=%d gameInitiated=1",self,int(destroy));});
  originalUnload(self,destroy,info);event.OriginalReturned();
  Observe([&]{LoadingObservation::Unloaded(self);});
}
void Asset(void* bundle,void* name,void* result,uint64_t begin) {
  {std::lock_guard<std::mutex> lock(submissionMutex);if(!submittedBundles.count(bundle))return;}
  if(!Budget(assets,128))return;
  Log("[RELOAD-PROBE-ASSET] bundle=%p name=%s object=%p elapsedMs=%.6f generation=%u pointerReuseIsNotCacheProof=1",bundle,String(name).c_str(),result,Milliseconds(begin,Clock()),generation.load());
}
struct FrameWindow {
  uint64_t previous=0;std::array<double,120> gaps{};size_t count=0;unsigned blocks=0,request=0;
} window;
void Frame(int frame) {
  if(!enabled.load(std::memory_order_relaxed))return;
  observedFrame.store(frame);auto now=Clock();auto request=requests.load();
  if(window.request!=request){window={};window.request=request;}
  auto before=window.previous;window.previous=now;
  if(!before || window.blocks>=30 || request>8)return;
  window.gaps[window.count++]=Milliseconds(before,now);
  if(window.count<window.gaps.size())return;
  auto sorted=window.gaps;std::sort(sorted.begin(),sorted.end());double sum=0;for(auto value:sorted)sum+=value;
  unsigned over33=0,over50=0;for(auto value:sorted){over33+=value>33.333;over50+=value>50.0;}
  ++window.blocks;window.count=0;
  Log("[RELOAD-PROBE-FRAMES] block=%u frame=%d generation=%u request=%u samples=%zu callbackGapMeanMs=%.6f callbackGapP95Ms=%.6f callbackGapMaxMs=%.6f over33ms=%u over50ms=%u gpuTiming=0 liveRenderers=%zu captureGroups=%zu",window.blocks,frame,generation.load(),requests.load(),sorted.size(),sum/sorted.size(),sorted[113],sorted.back(),over33,over50,RuntimeShapes::bound.size(),RuntimeShapes::groups.size());
}
void Prepared(bool accepted,uint64_t start,const char* error) {
  if(!enabled)return;
  Log("[RELOAD-PROBE-PREPARE] request=%u accepted=%d elapsedMs=%.6f error=%s resourceReloadImplemented=0",++requests,int(accepted),Milliseconds(start,Clock()),error?error:"<none>");
}
void Committed(uint64_t start,bool accepted) {
  if(!enabled)return;
  Log("[RELOAD-PROBE-COMMIT] accepted=%d elapsedMs=%.6f frame=%d generation=%u resourcesChanged=0 liveRenderers=%zu captureGroups=%zu",int(accepted),Milliseconds(start,Clock()),observedFrame.load(),generation.load(),RuntimeShapes::bound.size(),RuntimeShapes::groups.size());
}
void InventoryType(void* klass,const char* label) {
  if(!klass){Log("[RELOAD-PROBE-META] class=%s missing=1",label);return;}
  void* it=nullptr;
  while(auto m=il2cpp_class_get_methods(klass,&it)) {
    auto n=il2cpp_method_get_name(m);if(!n)continue;
    if(!strstr(n,"Load") && !strstr(n,"Release") && !strstr(n,"Unload") && !strstr(n,"Init") && !strstr(n,"ReFind") && !strstr(n,"ChangeMain") && !strstr(n,"SetModel") && !strstr(n,"Refresh") && !strstr(n,"Reload") && !strstr(n,"Dispose") && !strstr(n,"Clear"))continue;
    std::string args;for(unsigned i=0;i<il2cpp_method_get_param_count(m);++i){auto t=il2cpp_method_get_param(m,i);auto text=t?il2cpp_type_get_name(t):nullptr;if(i)args+=",";args+=text?text:"?";if(text)il2cpp_free((void*)text);}
    auto t=il2cpp_method_get_return_type(m);auto ret=t?il2cpp_type_get_name(t):nullptr;uint32_t impl=0;auto flags=il2cpp_method_get_flags(m,&impl);auto mp=((MInfo*)m)->mp;
    Log("[RELOAD-PROBE-META] %s.%s(%s)->%s static=%d address=%p rva=%llX",label,n,args.c_str(),ret?ret:"?",!!(flags&0x10),mp,(unsigned long long)(mp?(uintptr_t)mp-(uintptr_t)hGA:0));if(ret)il2cpp_free((void*)ret);
  }
  it=nullptr;while(auto f=il2cpp_class_get_fields(klass,&it)){
    auto n=il2cpp_field_get_name(f);auto t=il2cpp_field_get_type(f);auto text=t?il2cpp_type_get_name(t):nullptr;
    Log("[RELOAD-PROBE-FIELD] class=%s name=%s type=%s offset=%zu static=%d",label,n?n:"?",text?text:"?",il2cpp_field_get_offset(f),!!(il2cpp_field_get_flags(f)&0x10));if(text)il2cpp_free((void*)text);
  }
}
void InstallOptional(void** assemblies,size_t count) {
  if(enabled)return;
  try {
    if(!std::filesystem::exists(gameDirectory/"plugin/resource-input-diagnostics/full-reload-observation.enabled"))return;
    Require(il2cpp_class_get_fields && il2cpp_class_get_parent && il2cpp_field_get_name && il2cpp_field_get_type && il2cpp_field_get_flags && il2cpp_field_get_offset && il2cpp_field_get_value && il2cpp_type_get_name && il2cpp_free,"reload probe metadata/field API unavailable");
    LARGE_INTEGER f{};Require(QueryPerformanceFrequency(&f) && f.QuadPart>0,"reload probe clock unavailable");frequency=uint64_t(f.QuadPart);enabled=true;
    auto helperClass=FindClass("Beyond.Gameplay.View","EntityRenderHelper",assemblies,count);
    auto material=FindClass("Beyond.Rendering","EntityRenderHelperMaterialController",assemblies,count);
    auto model=FindClass("Beyond.Gameplay.View","ModelComponent",assemblies,count);
    auto baseModel=FindClass("Beyond.Gameplay.View","BaseModelComponent",assemblies,count);
    auto proxyManager=FindClass("Beyond.Resource.Runtime","PrefabInstantiateProxyManager",assemblies,count);
    auto proxy=FindClass("Beyond.Resource.Runtime","PrefabInstantiateProxy",assemblies,count);
    auto animation=FindClass("Beyond.Gameplay.View","ComplexAnimatorComponent",assemblies,count);
    auto bundle=FindClass("UnityEngine","AssetBundle",assemblies,count);
    auto manager=FindClass("Beyond.Resource.Runtime","BundleResourceManager",assemblies,count);
    auto resource=FindClass("Beyond.Resource","ResourceManager",assemblies,count);
    auto capture=FindClass("Beyond.Rendering","EntityRenderCaptureMeshController",assemblies,count);
    for(auto pair:{std::make_pair(helperClass,"EntityRenderHelper"),{material,"EntityRenderHelperMaterialController"},{model,"ModelComponent"},{manager,"BundleResourceManager"},{resource,"ResourceManager"},{capture,"EntityRenderCaptureMeshController"},{baseModel,"BaseModelComponent"},{proxyManager,"PrefabInstantiateProxyManager"},{proxy,"PrefabInstantiateProxy"},{animation,"ComplexAnimatorComponent"}})InventoryType(pair.first,pair.second);
    Install(helperClass,"_InitRenderAndMaterial",{},"System.Void",false,(void*)Init,(void**)&originalInit);
    Install(helperClass,"ReFindRenderersInChildren",{},"System.Void",false,(void*)Refind,(void**)&originalRefind);
    Install(helperClass,"OnRelease",{},"System.Void",false,(void*)HelperRelease,(void**)&originalHelperRelease);
    auto materialMethod=Exact(material,"Init",{"System.Collections.Generic.List<UnityEngine.Renderer>","System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>","Beyond.Rendering.EntityCustomizeRendererPropertyConfig","System.Boolean"},"System.Void",false);
    if(materialMethod && RuntimeShapes::originalInit && installed.count(((MInfo*)materialMethod)->mp)) {
      originalMaterial=RuntimeShapes::originalInit;sharedMaterial.store(true,std::memory_order_release);
      Log("[RELOAD-PROBE-SHARED] entry=EntityRenderHelperMaterialController.Init owner=RuntimeShapes exactSignatureVerified=1 secondDetour=0");
    } else Install(material,"Init",{"System.Collections.Generic.List<UnityEngine.Renderer>","System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>","Beyond.Rendering.EntityCustomizeRendererPropertyConfig","System.Boolean"},"System.Void",false,(void*)Material,(void**)&originalMaterial);
    Install(material,"OnRelease",{},"System.Void",false,(void*)MaterialRelease,(void**)&originalMaterialRelease);
    Install(model,"ChangeMainModelSync",{"System.String"},"System.Void",false,(void*)Model,(void**)&originalModel);
    Install(baseModel,"SetModelId",{"System.String"},"System.Boolean",false,(void*)SetModel,(void**)&originalSetModel);
    Install(baseModel,"LoadMainModelSync",{},"System.Void",false,(void*)LoadMain,(void**)&originalLoadMain);
    Install(baseModel,"RemoveMainPart",{},"System.Void",false,(void*)RemoveMain,(void**)&originalRemoveMain);
    bool proxyLoad=Install(proxy,"LoadImmediate",{},"System.Void",false,(void*)ProxyLoadImmediate,(void**)&originalProxyLoadImmediate);
    bool proxyRelease=Install(proxy,"Release",{},"System.Void",false,(void*)ProxyRelease,(void**)&originalProxyRelease);
    bool proxyReleaseHandle=Install(proxy,"ReleaseHandle",{"System.Int32"},"System.Void",false,(void*)ProxyReleaseHandle,(void**)&originalProxyReleaseHandle);
    bool proxyUnload=Install(proxy,"Unload",{},"System.Void",false,(void*)ProxyUnload,(void**)&originalProxyUnload);
    bool proxyClear=Install(proxy,"Clear",{},"System.Void",false,(void*)ProxyClear,(void**)&originalProxyClear);
    bool proxyDispose=Install(proxy,"Dispose",{},"System.Void",false,(void*)ProxyDispose,(void**)&originalProxyDispose);
    bool proxyManagerRelease=Install(proxyManager,"Release",{},"System.Void",false,(void*)ProxyManagerRelease,(void**)&originalProxyManagerRelease);
    bool proxyManagerUnloadAll=Install(proxyManager,"UnloadAll",{},"System.Void",false,(void*)ProxyManagerUnloadAll,(void**)&originalProxyManagerUnloadAll);
    bool proxyManagerOnDispose=Install(proxyManager,"OnProxyDispose",{"Beyond.Resource.Runtime.PrefabInstantiateProxy"},"System.Void",true,(void*)ProxyManagerOnDispose,(void**)&originalProxyManagerOnDispose);
    bool bundleManagerUnloadUnused=Install(manager,"UnloadUnusedAssets",{},"System.Collections.IEnumerator",false,(void*)BundleManagerUnloadUnused,(void**)&originalBundleManagerUnloadUnused);
    bool modelLoaded=Install(baseModel,"OnModelLoaded",{},"System.Void",false,(void*)ModelLoaded,(void**)&originalModelLoaded);
    bool animationChange=Install(animation,"ChangeAnimationConfig",{"Beyond.Gameplay.View.ComplexAnimatorComponentData","System.Boolean"},"System.Boolean",false,(void*)AnimationChange,(void**)&originalAnimationChange);
    Install(bundle,"Unload",{"System.Boolean"},"System.Void",false,(void*)Unload,(void**)&originalUnload);
    Log("[RELOAD-PROBE-READY] enabled=1 helperInit=%d refind=%d helperRelease=%d materialInit=%d materialRelease=%d modelChange=%d bundleUnload=%d proxyLoadImmediate=%d proxyRelease=%d proxyReleaseHandle=%d proxyUnload=%d proxyClear=%d proxyDispose=%d proxyManagerRelease=%d proxyManagerUnloadAll=%d proxyManagerOnDispose=%d bundleManagerUnloadUnused=%d modelLoaded=%d animationChange=%d materialInitShared=%d setModelId=%d loadMain=%d removeMain=%d gameInitiatedOnly=1 nativeFactoryCalls=0 resourceWrites=0 gfxRequested=0 eventLimitPerBoundary=128 assetLimit=128 frameBlocksPerRequest=30 requestLimit=8",!!originalInit,!!originalRefind,!!originalHelperRelease,!!originalMaterial,!!originalMaterialRelease,!!originalModel,!!originalUnload,int(proxyLoad),int(proxyRelease),int(proxyReleaseHandle),int(proxyUnload),int(proxyClear),int(proxyDispose),int(proxyManagerRelease),int(proxyManagerUnloadAll),int(proxyManagerOnDispose),int(bundleManagerUnloadUnused),int(modelLoaded),int(animationChange),int(sharedMaterial.load()),!!originalSetModel,!!originalLoadMain,!!originalRemoveMain);
  }catch(const std::exception& e){Log("[RELOAD-PROBE-UNAVAILABLE] reason=%s productionPreserved=1",e.what());}
}
} // namespace ReloadObservation
