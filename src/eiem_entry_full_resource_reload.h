#pragma once
#include "eiem_offline_resource_package.h"
// Resource reload transaction: offline manifest -> preloaded isolated graph ->
// native graph -> exact prefab content/owner preflight -> game-owned rebuild.
// No Renderer Mesh setters or private native/job memory writes are used.
namespace FullResourceReload {
using RuntimeShapes::Ref;
using Plan=EiemResourceReloadPlan::Plan;
using namespace EiemNativeInput;
struct Lease {Ref pathHash,handle,source;bool done=false,disposed=false,shared=false;};
struct Bundle {Ref path,request,object,bytes;std::shared_ptr<NativeBundle::FileHold> file;int id=0;bool done=false,owned=false,unloadCalled=false;};
struct Asset {Ref label,request,prefab;bool done=false;};
struct Route {Ref source,candidate;int sourceId=0,candidateId=0;};
using Routes=std::vector<Route>;
struct UIModelRequest {Ref loader,callback,closure,proxy,root,instantiateProxy,candidatePrefab;std::string path,error;uint64_t pathHash=0;void* completionInfo=nullptr;int id=0;bool submitted=false,completed=false,cancelled=false,delivered=false;};
struct Owner {Ref entity,component,root,pathHash;std::string path;int oldRootId=0;std::map<std::string,std::pair<unsigned,int>> originalMeshes;bool viewPart=false;Ref config;
 Ref animation,animationData,weapons,audio;
 bool uiModel=false;Ref uiParent,uiCallback,uiPage;mutable Ref uiLoadedRoot;std::shared_ptr<UIModelRequest> uiRequest;};
// Unity serializes asset paths with a different case than the offline
// manifest (for example `Assets/Beyond/...` versus `assets/beyond/...`).
// Paths are identity keys here, so compare their normalized form while still
// passing the manifest spelling to ResourceManager/StringPathHash.
inline std::string NormalizeAssetPath(std::string value) {
 std::replace(value.begin(),value.end(),'\\','/');
 while(value.size()>1&&value.front()=='/')value.erase(value.begin());
 std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
 return value;
}
inline bool SameAssetPath(const std::string& a,const std::string& b) {
 return NormalizeAssetPath(a)==NormalizeAssetPath(b);
}
inline const EiemResourceReloadPlan::Root* FindRoot(const Plan& plan,const std::string& path) {
 for(const auto& root:plan.roots)if(SameAssetPath(root.path,path))return &root;
 return nullptr;
}
struct Prepared {
 std::shared_ptr<const Plan> plan;RuntimeControls::State controls;EiemEntrySettings::Snapshot config;
 std::map<size_t,std::shared_ptr<NativeBundle::FileHold>> files;
 std::shared_ptr<EiemPrefabRules::Context> rules;std::map<size_t,std::shared_ptr<const Bytes>> ruleInputs;
 EiemOfflinePackage::Pointer pointer;
 std::shared_ptr<const std::vector<Package>> graph;
 std::map<std::filesystem::path,EiemNativeInput::InputFileStamp> authorFiles;
 std::map<uint64_t,size_t> rootHashes;
 unsigned base=0;bool testAuthor=false;
};
enum class Phase {Leases,Bundles,Assets,Verify,Ready,Owners,Rebuild,Passed,Reject,Drained,AwaitRequest,UIWait};
struct VerifyWalk;
struct Attempt {
 std::shared_ptr<Prepared> prepared;std::vector<Lease> leases;std::map<size_t,Bundle> bundles;std::vector<Asset> assets;
 std::vector<Owner> owners;Phase phase=Phase::AwaitRequest;size_t cursor=0,mutated=0;unsigned cloneCount=0;bool exposed=false;
 std::string error;ULONGLONG started=0;
 std::shared_ptr<VerifyWalk> verifier;
 uint64_t requestClock=0;std::map<int,double> costs;bool quarantined=false;
 ULONGLONG nativeStarted=0;
 std::set<size_t> selectedRoots;
 uint64_t selectionRevision=0;
 std::shared_ptr<const Routes> previousRoutes,nextRoutes;bool uiStaged=false;
};
struct NativeEnum {uint64_t bits=0;size_t width=0;void* Data(){return &bits;}};
struct ObjectField {void* field=nullptr;bool isStatic=false;};
struct Api {
 void *pathClass=nullptr,*handleClass=nullptr,*toPath=nullptr,*load=nullptr,*done=nullptr,*error=nullptr,*get=nullptr,*dispose=nullptr,*handlePath=nullptr,*assetSync=nullptr;
 NativeEnum rootCategory,priority;
 void *prefabType=nullptr,*rendererType=nullptr;
 void *components=nullptr,*materials=nullptr,*texture=nullptr,*textureIds=nullptr,*textureById=nullptr,*shader=nullptr,*width=nullptr,*height=nullptr,*mips=nullptr;
 void *getModel=nullptr,*getPath=nullptr,*modelCom=nullptr,*modelEntity=nullptr,*delegateTarget=nullptr,*released=nullptr;
 void *instance=nullptr,*discard=nullptr,*remove=nullptr,*loadModel=nullptr,*follow=nullptr;
 ObjectField modelManager,effectManager;
 void *animationCom=nullptr,*animationData=nullptr,*animationChange=nullptr,*animationAnimator=nullptr,*animationValid=nullptr;
 void *weaponCom=nullptr,*weaponUnload=nullptr,*weaponReload=nullptr,*weaponMounts=nullptr,*componentLookup=nullptr,*audioInvalidate=nullptr;
 void *instanceId=nullptr,*parent=nullptr,*isChild=nullptr,*goTransform=nullptr;
 void *childCount=nullptr,*getChild=nullptr,*componentGo=nullptr,*directComponents=nullptr;
 void *partRelease=nullptr,*partLoad=nullptr,*partSetup=nullptr;
 void *uiLoad=nullptr,*uiAsyncLoad=nullptr,*uiUnload=nullptr,*uiDictionaryContains=nullptr,*uiCallbackInvoke=nullptr,*uiUntrackedGet=nullptr,*uiActiveHierarchy=nullptr;
 void *uiLuaInstance=nullptr,*uiLuaDo=nullptr,*uiLuaCall=nullptr,*uiArraySet=nullptr,*uiObjectClass=nullptr;
 void *uiLuaKey=nullptr,*uiLuaEncode=nullptr,*uiLuaDecode=nullptr,*uiLuaCodeEquals=nullptr;
 void *uiLuaExceptionMessage=nullptr;
 void *proxyAsset=nullptr,*proxyHash=nullptr,*proxyGet=nullptr,*pathHashField=nullptr;
 void *handleUntrackedField=nullptr,*handleUntrackedClass=nullptr,*untrackedAssetGet=nullptr;
} api;
std::atomic<bool> configured{false},enabled{false},busy{false};std::mutex mutex;
std::atomic<bool> testAuthorCommitted{false};
std::shared_ptr<Prepared> pending;std::shared_ptr<Attempt> attempt,activeNative;
struct Ticket {EiemOfflinePackage::Pointer pointer;uint64_t clock=0;RuntimeControls::State controls;EiemEntrySettings::Snapshot config;};
std::shared_ptr<const Ticket> requested;
std::shared_ptr<const EiemOfflinePackage::Pointer> latest;
std::shared_ptr<const std::string> failedKey;
EiemOfflinePackage::Pointer lastPrepared; // Disk worker only.
std::shared_ptr<Prepared> diskPrepared;
// One game ResourceManager lease per source path protects external shared
// dependencies. Generations unload their own Bundle containers with false;
// Unity retains objects referenced by live Renderers/capture consumers.
std::map<std::string,Lease> sourceLeases;
std::vector<std::shared_ptr<Attempt>> retained;
std::map<std::string,Ref> observedPrefabs;
uint64_t ownerRevision=0;
using ResourceGetFn=void*(*)(void*,void*);
ResourceGetFn originalResourceGet=nullptr;
thread_local bool readingSource=false;
void* ResourceGet(void* self,void* info);
thread_local bool proxyContext=false;thread_local uint64_t proxyPathHash=0;
thread_local void* proxySource=nullptr;
thread_local Ref proxyUIOverride;
Ref CandidateUIProxy(void* proxy,uint64_t hash);
using ProxyPreloadFn=void(*)(void*,bool,void*);ProxyPreloadFn originalProxyPreload=nullptr;
void ProxyPreload(void* self,bool asynchronous,void* info);
void* RouteProxySource(void* source);
std::shared_ptr<const Routes> routes=std::make_shared<Routes>();
using CloneFn=void*(*)(void*);using CloneParentFn=void*(*)(void*,void*,bool);
CloneFn originalClone=nullptr;CloneParentFn originalCloneParent=nullptr;
using InstantiateFn=void*(*)(void*,void*,void*);using InstantiateParentFn=void*(*)(void*,void*,void*,void*);
using InstantiateAsyncFn=void*(*)(void*,int,void*,void*,void*);
InstantiateFn originalInstantiate=nullptr;InstantiateParentFn originalInstantiateParent=nullptr;
CloneParentFn originalInstantiateImmediate=nullptr;InstantiateAsyncFn originalInstantiateAsync=nullptr;
thread_local Attempt* rebuilding=nullptr;
void* Routed(void* source,const char* entry="clone") noexcept {
 try{
  if(proxyContext&&source==proxySource&&RuntimeShapes::Alive(proxyUIOverride.Get())){
   if(rebuilding)++rebuilding->cloneCount;
   Log("[FULL-RELOAD-UI-ROUTE] entry=%s sourceId=%d candidateId=%d exactRequestProxy=1",entry,RuntimeShapes::Scalar<int>(api.instanceId,source),RuntimeShapes::Scalar<int>(api.instanceId,proxyUIOverride.Get()));return proxyUIOverride.Get();
  }
  auto current=std::atomic_load(&routes);for(const auto& r:*current){
   if(rebuilding&&r.candidate.Get()==source&&RuntimeShapes::Alive(source)&&RuntimeShapes::Scalar<int>(api.instanceId,source)==r.candidateId){++rebuilding->cloneCount;return source;}
   if(r.source.Get()!=source)continue;
   if(!RuntimeShapes::Alive(source)||RuntimeShapes::Scalar<int>(api.instanceId,source)!=r.sourceId||
      !RuntimeShapes::Alive(r.candidate.Get())||RuntimeShapes::Scalar<int>(api.instanceId,r.candidate.Get())!=r.candidateId)return source;
   if(rebuilding){++rebuilding->cloneCount;Log("[FULL-RELOAD-ROUTE] entry=%s sourceId=%d candidateId=%d",entry,r.sourceId,r.candidateId);}return r.candidate.Get();
  }
  if(proxyContext)return RouteProxySource(source);
 }catch(const std::exception& e){if(proxyContext)Log("[FULL-RELOAD-PROXY-GAP] reason=%s gameSourcePreserved=1",e.what());}catch(...){}return source;
}
void* Clone(void* source){return originalClone(Routed(source));}
void* CloneParent(void* source,void* parent,bool world){return originalCloneParent(Routed(source),parent,world);}
void* Instantiate(void* source,void* position,void* rotation){return originalInstantiate(Routed(source,"position"),position,rotation);}
void* InstantiateParent(void* source,void* parent,void* position,void* rotation){return originalInstantiateParent(Routed(source,"parent-position"),parent,position,rotation);}
void* InstantiateImmediate(void* source,void* parent,bool world){return originalInstantiateImmediate(Routed(source,"immediate-parent"),parent,world);}
void* InstantiateAsync(void* source,int count,void* parent,void* position,void* rotation){return originalInstantiateAsync(Routed(source,"async-parent"),count,parent,position,rotation);}
void* Call(void* m,void* self=nullptr,void** args=nullptr){return RuntimeShapes::Call(m,self,args);}
void* Method(void* klass,const char* name,std::initializer_list<const char*> parameters,const char* result,bool isStatic){
 auto method=Exact(klass,name,parameters,result,isStatic);if(method)return method;
 std::string identity="<missing class>";if(klass){auto text=il2cpp_type_get_name(il2cpp_class_get_type(klass));if(text){identity=text;il2cpp_free((void*)text);}}
 std::string signature=identity+"::"+name+"(";for(auto parameter:parameters){if(signature.back()!='(')signature+=",";signature+=parameter;}
 throw std::runtime_error("reload exact managed signature absent: "+signature+")->"+result+" static="+std::to_string(int(isStatic)));
}
void BindMaterialTextureProperties(void* material){
 // IL2CPP can strip the allocating names overload while retaining the public
 // ID API. IDs enumerate every property without constructing a managed List
 // or guessing texture names from the current author declarations.
 api.textureIds=Method(material,"GetTexturePropertyNameIDs",{},"System.Int32[]",false);
 api.textureById=Method(material,"GetTexture",{"System.Int32"},"UnityEngine.Texture",false);
}
int Id(void* p){Require(RuntimeShapes::Alive(p),"reload native object retired");return RuntimeShapes::Scalar<int>(api.instanceId,p);}
void* Self(const Ref& value,void* expected){auto p=value.Get();Require(p&&il2cpp_object_get_class(p)==expected,"reload boxed value type differs");return il2cpp_object_unbox(p);}
Ref Path(const std::string& path){auto text=Ref::Make(il2cpp_string_new(path.c_str()),false);void* args[]={text.Get()};return Ref::Make(Call(api.toPath,nullptr,args),false);}
Ref Type(void* klass){Require(klass,"reload type absent");return Ref::Make(il2cpp_type_get_object(il2cpp_class_get_type(klass)),false);}
Ref prefabType,rendererType,characterAudioType;
using PartFinishedFn=void(*)(void*,void*);
PartFinishedFn originalPartFinished=nullptr;
PartFinishedFn originalBasePartFinished=nullptr,originalPartReleased=nullptr;
using PartLoadingFn=void(*)(void*,void*,void*,void*);
PartLoadingFn originalPartLoading=nullptr;
struct PartLoadScope {
 void* part;PartLoadScope* previous;
 static thread_local PartLoadScope* current;
 explicit PartLoadScope(void* value):part(value),previous(current){current=this;}
 ~PartLoadScope(){current=previous;}
 static bool Contains(void* value){for(auto scope=current;scope;scope=scope->previous)if(scope->part==value)return true;return false;}
};
inline thread_local PartLoadScope* PartLoadScope::current=nullptr;
using PartConfiguredFn=void(*)(void*,void*,void*);
PartConfiguredFn originalPartConfigured=nullptr;
using PartAddedFn=void(*)(void*,void*,void*);
PartAddedFn originalPartAdded=nullptr;
struct LivePart {Ref part,root;std::string path;bool persistent=false;Ref model;std::string ownershipError;bool ownerRecorded=false;};
std::map<void*,LivePart> liveParts;
std::vector<void*> ReferenceArray(void* array,size_t expected=SIZE_MAX) {
 size_t count=0;Require(ArrayLength(array,&count)&&(expected==SIZE_MAX||count==expected),"reload native reference array coverage/limit");
 std::vector<void*> values(count);Require(!count||CopyRaw((char*)array+32,values.data(),count*sizeof(void*)),"reload native reference array read");return values;
}
void* Field(void* klass,const char* name,const char* expected,bool isStatic=false){
 for(unsigned d=0;klass&&d<32;++d,klass=il2cpp_class_get_parent(klass)){void* it=nullptr;while(auto f=il2cpp_class_get_fields(klass,&it)){
  auto n=il2cpp_field_get_name(f);if(!n||strcmp(n,name))continue;auto t=il2cpp_type_get_name(il2cpp_field_get_type(f));
  auto flags=il2cpp_field_get_flags(f);std::string actual=t?t:"<missing type>";if(t)il2cpp_free((void*)t);
  Require((!expected||actual==expected)&&!!(flags&0x10)==isStatic,"reload field type/static contract differs: "+std::string(name)+
          " expected="+(expected?expected:"<structural check>")+" static="+std::to_string(int(isStatic))+" actual="+actual+" flags="+std::to_string(flags));return f;
 }}throw std::runtime_error("reload typed field absent: "+std::string(name));
}
void* ReadObject(void* obj,const char* name,const char* type){Require(obj,"reload owner object absent");void* value=nullptr;il2cpp_field_get_value(obj,Field(il2cpp_object_get_class(obj),name,type),&value);return value;}
// GameInstance manager storage is described by IL2CPP, not by the fact that
// get_instance returns an object. Keep the exact declared reference type;
// select the matching public getter from the field's actual static flag.
ObjectField BindObjectField(void* klass,const char* name,const char* expected){
 for(unsigned d=0;klass&&d<32;++d,klass=il2cpp_class_get_parent(klass)){void* it=nullptr;while(auto f=il2cpp_class_get_fields(klass,&it)){
  auto n=il2cpp_field_get_name(f);if(!n||strcmp(n,name))continue;auto flags=il2cpp_field_get_flags(f);bool isStatic=!!(flags&0x10);
  auto field=Field(klass,name,expected,isStatic);Require(!(flags&0x40),"reload manager field unexpectedly literal: "+std::string(name));
  Log("[FULL-RELOAD-FIELD] name=%s type=%s flags=%X static=%d getter=%s",name,expected,flags,int(isStatic),isStatic?"il2cpp_field_static_get_value":"il2cpp_field_get_value");
  return {field,isStatic};
 }}throw std::runtime_error("reload typed manager field absent: "+std::string(name));
}
void* ReadManager(const ObjectField& field){
 Require(field.field,"reload manager field not bound");void* value=nullptr;
 if(field.isStatic)il2cpp_field_static_get_value(field.field,&value);
 else{auto instance=Call(api.instance);Require(instance,"reload GameInstance unavailable");il2cpp_field_get_value(instance,field.field,&value);}
 return value;
}
Ref PartConfig(void* part){
 auto f=Field(il2cpp_object_get_class(part),"m_cfg","Beyond.Gameplay.View.BaseModelViewPartData");
 auto c=il2cpp_class_from_type(il2cpp_field_get_type(f));uint32_t alignment=0;auto size=il2cpp_class_value_size(c,&alignment);
 Require(size>0,"model part config size differs");Bytes bytes(size);il2cpp_field_get_value(part,f,bytes.data());
 return Ref::Make(il2cpp_value_box(c,bytes.data()),false);
}
bool InheritsClass(void* klass,const char* ns,const char* name){
 for(;klass;klass=il2cpp_class_get_parent(klass)){
  auto n=il2cpp_class_get_name(klass),space=il2cpp_class_get_namespace(klass);if(n&&space&&!strcmp(n,name)&&!strcmp(space,ns))return true;
 }return false;
}
bool Inherits(void* object,const char* ns,const char* name){return InheritsClass(object?il2cpp_object_get_class(object):nullptr,ns,name);}
std::string PartCallbackType(void* basePart){
 auto type=il2cpp_field_get_type(Field(basePart,"m_loadFinishCb",nullptr));auto klass=il2cpp_class_from_type(type);
 Require(InheritsClass(klass,"System","Delegate"),"reload model load callback is not a delegate");
 Method(klass,"Invoke",{"System.Boolean","Beyond.Gameplay.View.BaseModelViewPart"},"System.Void",false);
 auto text=il2cpp_type_get_name(type);Require(text,"reload model callback type absent");std::string name=text;il2cpp_free((void*)text);return name;
}
void RecordPartLoad(void* self,void* callback){
 // OnLoadFinish clears m_loadFinishCb before the component's completion
 // chain calls PostDealLoadedModel. Capture the public delegate Target at
 // LoadModel entry, while it still identifies the actual game owner.
 try{if(RuntimeShapes::Main()){
  LivePart live;live.part=Ref::Make(self,true);live.ownerRecorded=true;
  auto previous=liveParts.find(self);
  if(previous!=liveParts.end()&&previous->second.model.handle){auto model=previous->second.model.Get();
   // CreateMainPart adds the main part before SetupData; SyncInit then calls
   // StartSyncLoad without a delegate. Keep that authoritative attachment.
   // A retired owner stays fail-closed instead of becoming a display part.
   if(!model||ReadObject(model,"m_mainPart","Beyond.Gameplay.View.ComplexModelViewPart")==self)live.model=previous->second.model;
  }
  liveParts[self]=std::move(live);
  if(callback){Require(Inherits(callback,"System","Delegate"),"reload model callback object differs");
   auto target=Ref::Make(Call(api.delegateTarget,callback),false);
   if(Inherits(target.Get(),"Beyond.Gameplay.View","BaseModelComponent"))liveParts.at(self).model=Ref::Make(target.Get(),true);
  }
 }}catch(const std::exception& e){liveParts[self].ownershipError=e.what();Log("[FULL-RELOAD-PART-GAP] reason=%s",e.what());}
}
void PartAdded(void* self,void* part,void* info){
 originalPartAdded(self,part,info);
 try{if(!RuntimeShapes::Main()||ReadObject(self,"m_mainPart","Beyond.Gameplay.View.ComplexModelViewPart")!=part)return;
  Require(Inherits(self,"Beyond.Gameplay.View","BaseModelComponent"),"reload main-part owner type differs");
  auto& live=liveParts[part];live.part=Ref::Make(part,true);live.model=Ref::Make(self,true);live.ownerRecorded=true;live.ownershipError.clear();
 }catch(const std::exception& e){liveParts[part].ownershipError=e.what();Log("[FULL-RELOAD-PART-GAP] reason=%s",e.what());}
}
void PartLoading(void* self,void* data,void* callback,void* info){
 PartLoadScope loading(self);RecordPartLoad(self,callback);originalPartLoading(self,data,callback,info);
}
void PartConfigured(void* self,void* data,void* info){
 // Display callers also use SetupData -> StartSyncLoad directly, without
 // LoadModel's callback. Record this real game entry before completion.
 RecordPartLoad(self,nullptr);originalPartConfigured(self,data,info);
}
void RecordPartFinished(void* self){
 try{if(!RuntimeShapes::Main())return;auto root=ReadObject(self,"m_model","UnityEngine.GameObject");if(!RuntimeShapes::Alive(root)){liveParts.erase(self);return;}
  auto cfg=PartConfig(self);auto path=String(ReadObject(cfg.Get(),"modelPath","System.String"));
  unsigned char persistent=0;il2cpp_field_get_value(cfg.Get(),Field(il2cpp_object_get_class(cfg.Get()),"usePersistentPool","System.Boolean"),&persistent);
  auto& live=liveParts[self];live.part=Ref::Make(self,true);live.root=Ref::Make(root,true);live.path=path;live.persistent=persistent!=0;
  ++ownerRevision;
  RuntimeShapes::RegisterModel(root);
 }catch(const std::exception& e){Log("[FULL-RELOAD-PART-GAP] reason=%s",e.what());}
}
void PartFinished(void* self,void* info){originalPartFinished(self,info);RecordPartFinished(self);}
void BasePartFinished(void* self,void* info){
 originalBasePartFinished(self,info);
 // Complex parts are recorded after their own post-processing completes.
 // Other game view parts also use the base completion lifecycle.
 if(!Inherits(self,"Beyond.Gameplay.View","ComplexModelViewPart"))RecordPartFinished(self);
}
std::vector<Ref> Components(void* root);
void PartReleased(void* self,void* info){
 std::vector<Ref> renderers;
 try{if(RuntimeShapes::Main()&&!RuntimeShapes::bound.empty()){
  auto root=ReadObject(self,"m_model","UnityEngine.GameObject");if(RuntimeShapes::Alive(root))renderers=Components(root);
 }}catch(const std::exception& e){Log("[FULL-RELOAD-PART-GAP] releaseControlRegistration=%s",e.what());}
 originalPartReleased(self,info);
 if(!RuntimeShapes::Main())return;
 // ReleaseModel can return a still-alive root to the game's path pool.
 // It is no longer a live owner; native captures retain their own snapshots.
 for(const auto& renderer:renderers)RuntimeShapes::bound.erase(renderer.Get());
 RuntimeShapes::boundCount.store(unsigned(RuntimeShapes::bound.size()));
 auto live=liveParts.find(self);
 if(PartLoadScope::Contains(self)&&live!=liveParts.end()){
  // LoadModel releases the old root before starting its replacement. Keep
  // the incoming load's owner, but retire every reference to the old root.
  live->second.root={};live->second.path.clear();live->second.persistent=false;
 }else liveParts.erase(self);
 ++ownerRevision;
}
std::vector<Ref> Components(void* root);
#include "eiem_entry_ui_models.h"
void* CurrentRoot(const Owner& owner){return owner.uiModel?owner.uiLoadedRoot.Get():owner.viewPart?ReadObject(owner.component.Get(),"m_model","UnityEngine.GameObject"):Call(api.getModel,owner.component.Get());}
void RemoveOwner(const Owner& owner){if(owner.uiModel){auto root=CurrentRoot(owner);if(root&&UIOwns(owner.component.Get(),root)){void* args[]={root};Call(api.uiUnload,owner.component.Get(),args);}owner.uiLoadedRoot={};return;}
 auto component=owner.component.Get();auto method=owner.viewPart?il2cpp_object_get_virtual_method(component,api.partRelease):api.remove;
 Require(method,"reload actual model part release override unavailable");Call(method,component);}
void LoadOwner(const Owner& owner){if(owner.uiModel){FinishUIModel(owner);}else if(owner.viewPart){
 Require(owner.config.Get(),"display part config snapshot absent");void* args[]={il2cpp_object_unbox(owner.config.Get())};Call(api.partSetup,owner.component.Get(),args);
 Require(RuntimeShapes::Scalar<unsigned char>(api.partLoad,owner.component.Get())!=0,"display part synchronous load failed");
 }else Call(api.loadModel,owner.component.Get());}
NativeEnum Enum(void* klass,const char* name){
 Require(klass,"reload enum class absent");std::string underlying,enumType;void* literal=nullptr;
 auto enumName=il2cpp_type_get_name(il2cpp_class_get_type(klass));if(enumName){enumType=enumName;il2cpp_free((void*)enumName);}
 Require(!enumType.empty(),"reload enum identity absent");
 void* it=nullptr;while(auto f=il2cpp_class_get_fields(klass,&it)){auto n=il2cpp_field_get_name(f);if(!n)continue;
  if(!strcmp(n,"value__")){Require(!(il2cpp_field_get_flags(f)&0x10),"reload enum underlying field unexpectedly static");auto t=il2cpp_type_get_name(il2cpp_field_get_type(f));if(t){underlying=t;il2cpp_free((void*)t);}}
  if(!strcmp(n,name)){Require((il2cpp_field_get_flags(f)&0x50)==0x50,"reload enum member is not literal/static");
   auto t=il2cpp_type_get_name(il2cpp_field_get_type(f));bool correct=t&&enumType==t;if(t)il2cpp_free((void*)t);
   Require(correct&&!literal,"reload enum literal type/identity differs");literal=f;}
 }
 const std::map<std::string,size_t> widths={{"System.Byte",1},{"System.SByte",1},{"System.Int16",2},{"System.UInt16",2},{"System.Int32",4},{"System.UInt32",4},{"System.Int64",8},{"System.UInt64",8}};
 auto expected=widths.find(underlying);Require(expected!=widths.end(),"reload enum unsupported underlying type: "+enumType+"/"+underlying);
 uint32_t alignment=0;auto width=il2cpp_class_value_size(klass,&alignment);
 Require(width==int(expected->second),"reload enum metadata/value width differs: "+enumType+"/"+underlying+"/"+std::to_string(width));
 Require(literal,"reload enum literal absent: "+enumType+"/"+name);
 NativeEnum result;result.width=expected->second;il2cpp_field_static_get_value(literal,result.Data());
 Log("[FULL-RELOAD-ENUM] type=%s underlying=%s bytes=%zu literal=%s bits=%016llX",enumType.c_str(),underlying.c_str(),result.width,name,(unsigned long long)result.bits);return result;
}
std::vector<Ref> Components(void* root){unsigned char inactive=1;void* args[]={rendererType.Get(),&inactive};auto array=Ref::Make(Call(api.components,root,args),false);size_t count=0;
 Require(ArrayLength(array.Get(),&count),"reload Renderer discovery coverage/limit");std::vector<Ref> values;values.reserve(count);
 for(auto value:ReferenceArray(array.Get(),count))values.push_back(Ref::Make(value,false));return values;}
std::map<std::string,std::pair<unsigned,int>> MeshProfile(void* root) {
 std::map<std::string,std::pair<unsigned,int>> profile,authored;
 size_t ordinal=0;
 for(const auto& rendererRef:Components(root)){auto renderer=rendererRef.Get();auto meshRef=Ref::Make(RuntimeShapes::SourceMesh(renderer),false);auto mesh=meshRef.Get();
  // Public traversal order and native instance IDs also cover vanilla owners.
  // Names cannot establish identity when adding the first or removing a Mod.
  profile.emplace("renderer/"+std::to_string(ordinal++),std::make_pair(1u,RuntimeShapes::Alive(mesh)?Id(mesh):0));
  if(RuntimeShapes::Alive(mesh)){auto marker=String(Call(RuntimeShapes::api.name,mesh));if(RuntimeControls::specs.count(marker)){
   auto& value=authored[marker];++value.first;auto id=Id(mesh);Require(!value.second||value.second==id,"reload source Mesh marker aliases two native Meshes");value.second=id;
  }}
 }
 // Authored provenance scopes rollback to the model assets already controlled
 // by EFF, excluding mounted game attachments. A vanilla owner has no such
 // provenance and uses its full public Mesh profile after consumers rebind.
 return authored.empty()?profile:authored;
}
struct BoneHierarchy {
 Ref root;std::map<void*,Ref> nodes;std::map<void*,void*> parents;std::map<void*,std::string> names;
 explicit BoneHierarchy(void* gameObject):root(Ref::Make(Call(api.goTransform,gameObject),false)){
  Require(RuntimeShapes::Alive(root.Get()),"reload prefab root Transform absent");Hold(root.Get());
 }
 void Hold(void* node){if(nodes.count(node))return;
  Require(RuntimeShapes::Alive(node),"reload bone hierarchy Transform retired");nodes.emplace(node,Ref::Make(node,false));}
 void* Parent(void* node){Hold(node);auto found=parents.find(node);if(found!=parents.end())return found->second;
  auto parent=Call(api.parent,node);if(parent)Hold(parent);parents.emplace(node,parent);return parent;}
 const std::string& Name(void* node){Hold(node);auto found=names.find(node);if(found==names.end()){
   auto name=String(Call(RuntimeShapes::api.name,node));Require(!name.empty()&&name.find('/')==std::string::npos,"reload ambiguous bone path component");found=names.emplace(node,std::move(name)).first;
  }return found->second;}
 std::vector<void*> Chain(void* node){std::vector<void*> chain;std::set<void*> seen;
  for(auto current=node;current;current=Parent(current)){Hold(current);Require(seen.insert(current).second,"reload native bone hierarchy cycle/depth");
   chain.push_back(current);if(current==root.Get())return chain;}
  throw std::runtime_error("reload bone/Renderer Transform outside owning prefab root");
 }
 std::string Relative(const std::vector<void*>& chain,void* anchor){auto at=std::find(chain.begin(),chain.end(),anchor);
  Require(at!=chain.end(),"reload bone is outside candidate skeleton anchor");std::string path;
  while(at!=chain.begin()){--at;if(!path.empty())path+='/';path+=Name(*at);}return path;
 }
 void Verify(void* renderer,const std::vector<void*>& bones,void* rootBone,const EiemResourceReloadPlan::Part& part){
  Require(bones.size()==part.bones.size()&&!bones.empty(),"reload native bone slot coverage differs");
  Require(std::set<void*>(bones.begin(),bones.end()).size()==bones.size()&&std::count(bones.begin(),bones.end(),rootBone)==1,"reload per-Mesh root/duplicate bone slot differs: "+part.id);
  auto owner=Ref::Make(Call(RuntimeShapes::api.transform,renderer),false);auto ownerChain=Chain(owner.Get()),rootChain=Chain(rootBone);
  std::vector<std::vector<void*>> chains;for(auto bone:bones)chains.push_back(Chain(bone));
  size_t matches=0;std::string mismatch;
  for(auto anchor:rootChain){if(std::find(ownerChain.begin(),ownerChain.end(),anchor)==ownerChain.end())continue;
   bool correct=true;for(size_t i=0;i<chains.size();++i){
    if(std::find(chains[i].begin(),chains[i].end(),anchor)==chains[i].end()){correct=false;break;}
    auto actual=Relative(chains[i],anchor);if(actual!=part.bones[i]){correct=false;
     if(mismatch.empty())mismatch=" slot="+std::to_string(i)+" expected="+part.bones[i]+" actual="+actual;break;}
   }if(correct)++matches;
  }
  Require(matches==1,"reload per-Mesh bone paths require one unique owning hierarchy anchor: "+part.id+mismatch);
 }
};
// The offline descriptor is an address map, not just a list of marker names.
// Verify those addresses against the GameObject created by Unity before any
// live owner is touched. This also checks 0/1/N Renderer expansion and the
// exact per-Mesh bone slot order in the candidate prefab.
struct AssemblyTree {
 Ref root;std::map<std::string,Ref> nodes;std::set<void*> seen;
 explicit AssemblyTree(void* gameObject):root(Ref::Make(Call(api.goTransform,gameObject),false)){
  Require(RuntimeShapes::Alive(root.Get()),"reload assembly root Transform absent");Index(root.Get(),"",0);
 }
 void Index(void* node,const std::string& path,unsigned depth){
  Require(seen.insert(node).second,"reload assembly hierarchy cycle/limit");
  Require(nodes.emplace(path,Ref::Make(node,false)).second,"reload assembly duplicate address");
  int count=RuntimeShapes::Scalar<int>(api.childCount,node);Require(count>=0,"reload assembly child extent");
  std::map<std::string,unsigned> ordinals;
  for(int i=0;i<count;++i){void* args[]={&i};auto child=Call(api.getChild,node,args);
   Require(RuntimeShapes::Alive(child),"reload assembly child retired");
   auto name=String(Call(RuntimeShapes::api.name,child));
   Index(child,path+EiemRenderAssemblyDescription::Segment(name,ordinals[name]++),depth+1);
  }
 }
 void* At(const std::string& path)const{auto it=nodes.find(path);Require(it!=nodes.end(),"reload assembly address absent: "+path);return it->second.Get();}
 void* Renderer(const EiemRenderAssemblyDescription::Address& address)const{
  auto go=Call(api.componentGo,At(address.path));void* args[]={rendererType.Get()};
  auto list=Ref::Make(Call(api.directComponents,go,args),false);auto values=ReferenceArray(list.Get());
  values.erase(std::remove_if(values.begin(),values.end(),[](void* r){return !RuntimeShapes::RendererKind(r);}),values.end());
  std::stable_partition(values.begin(),values.end(),[](void* r){return RuntimeShapes::RendererKind(r)==137;});
  Require(address.component<values.size(),"reload assembly Renderer component ordinal absent: "+address.path);
  return values[address.component];
 }
};
void VerifyAssemblyPrefab(void* gameObject,const EiemResourceReloadPlan::Root& expected){
 AssemblyTree tree(gameObject);size_t checked=0;
 for(const auto& source:expected.assembly.sources){
  Require(RuntimeShapes::Alive(tree.Renderer(source.renderer)),"reload assembly native Renderer anchor absent");
  for(const auto& target:source.targets){auto renderer=tree.Renderer(target.renderer);
   Require(RuntimeShapes::Alive(renderer),"reload assembly target Renderer absent");
   auto mesh=RuntimeShapes::SourceMesh(renderer);
   if(!target.marker.empty())Require(RuntimeShapes::Alive(mesh)&&String(Call(RuntimeShapes::api.name,mesh))==target.marker,"reload assembly target Mesh marker differs: "+target.marker);
   if(RuntimeShapes::RendererKind(renderer)==23){Require(target.bones.empty()&&!target.hasRoot&&!target.hasSkin,"reload static assembly contains skinning bindings");}
   else {auto actual=Ref::Make(Call(RuntimeShapes::api.bones,renderer),false);
   auto bones=ReferenceArray(actual.Get(),target.bones.size());
   for(size_t i=0;i<bones.size();++i)Require(bones[i]==tree.At(target.bones[i]),"reload assembly bone slot address differs: "+target.marker+"/"+std::to_string(i));
   auto check=[&](void* method,const std::string& path,bool present,const char* name){auto value=Call(method,renderer);
    Require(value==(present?tree.At(path):nullptr),std::string("reload assembly ")+name+" address differs: "+target.marker);
   };
   check(RuntimeShapes::api.rootBone,target.root,target.hasRoot,"rootBone");
   if(RuntimeShapes::api.skinningRoot)check(RuntimeShapes::api.skinningRoot,target.skin,target.hasSkin,"skinningRoot");
   }
   ++checked;
  }
 }
 Log("[FULL-RELOAD-ASSEMBLY] prefabId=%d nativeSources=%zu targetRenderers=%zu addressAndBoneSlotsVerified=1",Id(gameObject),expected.assembly.sources.size(),checked);
}
struct VerifyWalk {
 Ref root;const Plan& plan;const EiemResourceReloadPlan::Root& expectedRoot;bool live;
 std::map<std::string,unsigned> found;std::vector<Ref> renderers;BoneHierarchy hierarchy;size_t cursor=0,checked=0,textureChecks=0;
 VerifyWalk(void* value,const Plan& p,const EiemResourceReloadPlan::Root& expected,bool isLive=false):root(Ref::Make(value,false)),plan(p),expectedRoot(expected),live(isLive),renderers(Components(value)),hierarchy(value){}
 bool Step(){
 if(cursor==renderers.size()){
  Require((checked>0 || plan.composed&&expectedRoot.markers.empty())&&found==expectedRoot.markers,"reload native prefab Renderer coverage differs from serialized root");
  if(!expectedRoot.assembly.sources.empty())VerifyAssemblyPrefab(root.Get(),expectedRoot);
  else Log("[FULL-RELOAD-ASSEMBLY] markerCoverageVerified=1 perMeshBonesVerified=1 serializedAddressMapAvailable=0");
  Log("[FULL-RELOAD-PREFAB] prefab=%p nativeId=%d renderers=%zu authored=%zu textureProperties=%zu perMeshBonesVerified=1 nativeShapesPreserved=1",root.Get(),Id(root.Get()),renderers.size(),checked,textureChecks);return true;
 }
 auto renderer=renderers.at(cursor++).Get();auto meshRef=Ref::Make(RuntimeShapes::SourceMesh(renderer),false);auto mesh=meshRef.Get();if(!RuntimeShapes::Alive(mesh))return false;
  auto marker=String(Call(RuntimeShapes::api.name,mesh));auto spec=plan.parts.find(marker);if(spec==plan.parts.end())return false;
  const auto& p=spec->second;++found[marker];++checked;
  Require(RuntimeShapes::Scalar<int>(RuntimeShapes::api.vertexCount,mesh)==int(p.vertices),"reload native vertex count differs: "+p.id);
  Require(RuntimeShapes::Scalar<int>(RuntimeShapes::api.shapeCount,mesh)==int(p.shapes.size()),"reload native shape count differs: "+p.id);
  for(size_t i=0;i<p.shapes.size();++i){int index=int(i);void* args[]={&index};Require(String(Call(RuntimeShapes::api.shapeName,mesh,args))==p.shapes[i],"reload shape channel order differs");}
  if(RuntimeShapes::RendererKind(renderer)==23){
    Require(p.bones.empty()&&p.shapes.empty(),"reload static part contains skinning/shape bindings");
    auto bindposes=Ref::Make(Call(RuntimeShapes::api.bindPoses,mesh),false);size_t count=0;
    Require(ArrayLength(bindposes.Get(),&count)&&count==0,"reload static Mesh contains bindposes");
  }else {auto array=Ref::Make(Call(RuntimeShapes::api.bones,renderer),false);auto bones=ReferenceArray(array.Get(),p.bones.size());
  auto bindposes=Call(RuntimeShapes::api.bindPoses,mesh);size_t count=0;Require(ArrayLength(bindposes,&count)&&count==bones.size(),"reload bone/bindpose counts differ");
  auto rootBone=Ref::Make(Call(RuntimeShapes::api.rootBone,renderer),false);Require(RuntimeShapes::Alive(rootBone.Get()),"reload root bone absent");hierarchy.Verify(renderer,bones,rootBone.Get(),p);
  }
  auto mats=Ref::Make(Call(api.materials,renderer),false);auto material=ReferenceArray(mats.Get(),1).front();const auto& expected=plan.materials.at(p.material);
  auto materialName=RuntimeShapes::Alive(material)?String(Call(RuntimeShapes::api.name,material)):"";
  Require(RuntimeShapes::Alive(material)&&(materialName==expected.name||(live&&materialName==expected.name+" (Instance)"))&&RuntimeShapes::Alive(Call(api.shader,material)),"reload Material/Shader absent or wrong");
  for(const auto& slot:expected.slots){auto property=Ref::Make(il2cpp_string_new(slot.property.c_str()),false);void* args[]={property.Get()};auto tex=Call(api.texture,material,args);
   if(!slot.nonnull){Require(!RuntimeShapes::Alive(tex),"reload null Texture override differs");continue;}
   Require(RuntimeShapes::Alive(tex),"reload shared/custom Texture missing: "+p.id+"/"+slot.property);
   if(slot.local){Require(String(Call(RuntimeShapes::api.name,tex))==slot.texture.name,"reload original/custom Texture selection differs: "+p.id+"/"+slot.property);
    Require(RuntimeShapes::Scalar<int>(api.width,tex)==slot.texture.width&&RuntimeShapes::Scalar<int>(api.height,tex)==slot.texture.height&&RuntimeShapes::Scalar<int>(api.mips,tex)==slot.texture.mips,"reload native Texture dimensions/mips differ");}
   ++textureChecks;
  }
 return false;
 }
};
void VerifyPrefab(void* root,const Plan& plan,const EiemResourceReloadPlan::Root& expectedRoot,bool live=false){
 VerifyWalk walk(root,plan,expectedRoot,live);while(!walk.Step()){}
}
struct SourceReadScope {bool previous=readingSource;SourceReadScope(){readingSource=true;}~SourceReadScope(){readingSource=previous;}};
Lease LeaseObserved(const std::string& path,void* source){
 auto it=sourceLeases.find(path);if(it!=sourceLeases.end()&&RuntimeShapes::Alive(it->second.source.Get()))return it->second;
 // This is a lease of the prefab the game has just returned, not a request
 // for unvisited roots. Its existing ResourceManager cache owns dependencies.
 SourceReadScope scope;Lease l;l.pathHash=Path(path);void* args[]={Self(l.pathHash,api.pathClass),prefabType.Get(),api.rootCategory.Data(),api.priority.Data()};
 l.handle=Ref::Make(Call(api.load,nullptr,args),false);Require(l.handle.Get(),"observed prefab dependency lease null");
 l.done=RuntimeShapes::Scalar<unsigned char>(api.done,Self(l.handle,api.handleClass))!=0;l.source=Ref::Make(source,false);l.shared=true;sourceLeases[path]=l;return l;
}
void RouteObserved(Attempt& a,size_t index,void* source){
 auto& asset=a.assets.at(index);Require(RuntimeShapes::Alive(source)&&RuntimeShapes::Alive(asset.prefab.Get())&&Id(source)!=Id(asset.prefab.Get()),"observed prefab candidate aliases source");
 auto next=std::make_shared<Routes>(*std::atomic_load(&routes));auto it=std::find_if(next->begin(),next->end(),[&](const auto& r){return r.source.Get()==source;});
 Route route{Ref::Make(source,false),asset.prefab,Id(source),Id(asset.prefab.Get())};if(it==next->end())next->push_back(std::move(route));else *it=std::move(route);
 std::atomic_store(&routes,std::shared_ptr<const Routes>(next));
}
void* CandidateForObserved(Attempt& a,size_t index,void* source){
 const auto& expected=a.prepared->plan->roots.at(index);auto& asset=a.assets.at(index);
 if(!RuntimeShapes::Alive(asset.prefab.Get())){
  auto lease=LeaseObserved(expected.path,source);a.leases.at(index)=lease;
  auto label=Ref::Make(il2cpp_string_new(expected.path.c_str()),false);void* args[]={label.Get(),prefabType.Get()};
  auto candidate=Ref::Make(Call(api.assetSync,a.bundles.at(expected.input).object.Get(),args),false);
  Require(RuntimeShapes::Alive(candidate.Get()),"observed candidate prefab absent");VerifyPrefab(candidate.Get(),*a.prepared->plan,expected);
  asset.prefab=std::move(candidate);asset.done=true;
  Log("[FULL-RELOAD-LAZY-PREFAB] path=%s existingGameRequest=1 offlineTarget=1",expected.path.c_str());
 }
 RouteObserved(a,index,source);return asset.prefab.Get();
}
void* RouteProxySource(void* source){
 if(source!=proxySource)return source;
 auto current=std::atomic_load(&activeNative);if(!current||current->phase!=Phase::Passed)return source;
 auto root=current->prepared->rootHashes.find(proxyPathHash);if(root==current->prepared->rootHashes.end())return source;
 return CandidateForObserved(*current,root->second,source);
}
void ProxyPreload(void* self,bool asynchronous,void* info){
 // The game's prefab proxy reads its AssetProxy directly; IL2CPP may inline
 // FAssetProxyHandle.Get. Scope the actual sync/async instantiate entry too.
 struct Scope {bool prior=proxyContext;uint64_t hash=proxyPathHash;void* source=proxySource;Ref held,uiCandidate=proxyUIOverride;~Scope(){proxyContext=prior;proxyPathHash=hash;proxySource=source;proxyUIOverride=uiCandidate;}}scope;
 proxyContext=false;proxyUIOverride={};
 try{auto asset=Call(api.proxyAsset,self);if(Inherits(asset,"","AssetProxy")){auto source=Call(api.proxyGet,asset);
   if(Inherits(source,"UnityEngine","GameObject")&&RuntimeShapes::Alive(source)){scope.held=Ref::Make(source,false);proxySource=source;proxyPathHash=RuntimeShapes::Scalar<uint64_t>(api.proxyHash,asset);proxyContext=true;proxyUIOverride=CandidateUIProxy(self,proxyPathHash);}}}
 catch(const std::exception& e){proxyContext=false;Log("[FULL-RELOAD-PROXY-GAP] reason=%s gameFlowPreserved=1",e.what());}
 originalProxyPreload(self,asynchronous,info);
}
void* ResourceGet(void* self,void* info){
 auto source=originalResourceGet(self,info);if(readingSource||!enabled||!RuntimeShapes::Main()||!source)return source;
 try{
  // Exact resource path comes from the game's existing handle. Offline roots
  // define every eligible target; no name guessing or scene traversal.
  if(!Inherits(source,"UnityEngine","GameObject"))return source;
  // The same handle also returns already-instantiated GameObjects. Route
  // raw AssetProxy prefabs only; an instance handle must keep its instance.
  auto boxed=Ref::Make(il2cpp_value_box(api.handleClass,self),false);uint32_t alignment=0;
  auto size=il2cpp_class_value_size(api.handleUntrackedClass,&alignment);Require(size>0,"untracked resource handle size differs");
  Bytes untracked(size);il2cpp_field_get_value(boxed.Get(),api.handleUntrackedField,untracked.data());
  if(!Inherits(Call(api.untrackedAssetGet,untracked.data()),"","AssetProxy"))return source;
  auto path=NormalizeAssetPath(String(Call(api.handlePath,self)));auto current=std::atomic_load(&activeNative);
  auto rules=std::atomic_load(&resourceRules);const Plan* plan=current?current->prepared->plan.get():rules?rules->plan.get():nullptr;
  if(!plan||!FindRoot(*plan,path))return source;
  auto observed=observedPrefabs.find(path);if(observed==observedPrefabs.end()||observed->second.Get()!=source){observedPrefabs[path]=Ref::Make(source,true);++ownerRevision;}
  if(rebuilding)return Routed(source,"resource-handle");
  if(!current||current->phase!=Phase::Passed)return source;
  auto expected=FindRoot(*current->prepared->plan,path);return CandidateForObserved(*current,size_t(expected-current->prepared->plan->roots.data()),source);
 }catch(const std::exception& e){Log("[FULL-RELOAD-RESOURCE-GAP] reason=%s gameResourcePreserved=1",e.what());return source;}
}
// Disk worker: prepares and validates candidates without any Unity calls.
bool AuthorMatches(const Package& package){
 if(!std::filesystem::is_regular_file(package.root/"mod.ini"))return false;
 auto ini=EiemDirectory::Read(package.root/"mod.ini");
 if(Sha(ini.data(),ini.size())!=package.iniHash)return false;
 for(const auto& resource:package.resources){auto path=package.root/std::filesystem::u8path(resource.first);if(!std::filesystem::is_regular_file(path))return false;auto bytes=EiemDirectory::Read(path);
  if(bytes.size()!=resource.second->bytes.size()||EiemResourceReloadPlan::Hash(bytes)!=EiemResourceReloadPlan::Hash(resource.second->bytes))return false;
 }return true;
}
bool AuthorMatches(const std::vector<Package>& packages){
 if(!InstalledAuthorsMatch(packages))return false;
 for(const auto& p:packages)if(!AuthorMatches(p))return false;return true;
}
std::map<std::filesystem::path,InputFileStamp> AuthorFiles(const Package& package){
 std::map<std::filesystem::path,InputFileStamp> files;files.emplace(package.root/"mod.ini",FileStamp(package.root/"mod.ini"));
 for(const auto& resource:package.resources){auto path=package.root/std::filesystem::u8path(resource.first);files.emplace(path,FileStamp(path));}return files;
}
std::map<std::filesystem::path,InputFileStamp> AuthorFiles(const std::vector<Package>& packages){
 std::map<std::filesystem::path,InputFileStamp> result;for(const auto& p:packages){auto files=AuthorFiles(p);result.merge(files);}return result;
}
bool AuthorFilesUnchanged(const std::map<std::filesystem::path,InputFileStamp>& files){for(const auto& file:files)if(!std::filesystem::is_regular_file(file.first)||!SameStamp(FileStamp(file.first),file.second))return false;return true;}
bool AuthorFilesUnchanged(const Prepared& p){return (!p.graph||InstalledAuthorsMatch(*p.graph))&&AuthorFilesUnchanged(p.authorFiles);}
RuntimeControls::State CompiledControls(const std::shared_ptr<const std::vector<Package>>& graph,const Plan& plan,const EiemGlobalConfig& config){
 Require(bool(graph),"offline control graph unavailable");RuntimeControls::State state;state.graph=graph;
 std::map<std::string,std::pair<const Package*,std::string>> owners;
 for(const auto& package:*graph){
  state.variables[package.id]=package.defaults;state.keys[package.id]=package.bindings;
  for(const auto& key:package.bindings)Require(key.key.vk!=config.reload.vk&&key.key.vk!=config.gui.vk,"Mod key overlaps reserved global shortcut: "+package.id+"/"+key.id);
  for(const auto& part:package.parts)Require(owners.emplace(plan.composed?EiemComposedPackage::Name(package.id,part.first):part.first,std::make_pair(&package,part.first)).second,"compiled part owner collision");
 }
 // Provenance markers were already computed by the compiler. Do not hash the
 // same Mesh payload again for each LOD on every F10 control preparation.
 for(const auto& row:plan.parts){const auto& expected=row.second;auto owner=owners.at(expected.id);const auto& package=*owner.first;const auto& part=package.parts.at(owner.second);const auto& mesh=package.meshes.at(part.mesh);
  Require(expected.marker==row.first&&expected.vertices==mesh.vertices&&expected.bones==mesh.bones&&expected.shapes==mesh.shapes&&(plan.composed||expected.material==part.material),"offline controls/part descriptor differs");
  RuntimeControls::Spec spec{package.id,owner.second,expected.marker,mesh.vertices,mesh.shapes,part.shapes,part.visible};
  Require(state.specs.emplace(spec.marker,std::move(spec)).second,"offline control provenance collision");
 }RuntimeControls::PreviewPreferences(state);RuntimeControls::PreparePanel(state);return state;
}
bool CompiledMode(){
 if(preparedCompiledSet)return true;for(const auto& root:DiscoverModRoots())if(std::filesystem::is_regular_file(root/"compiled.bin"))return true;return false;
}
void PollCompiled(bool trigger){
 if(!enabled||!active)return;
 static std::string lastError,failedStamp;
 auto clock=trigger&&reloadClock.load()?reloadClock.load():NativeBundle::Clock();
 try{
  auto stamp=EiemCompiledModSet::DirectoryStamp(gameDirectory/"plugin/mods");if(!trigger&&!failedStamp.empty()&&stamp==failedStamp)return;
  if(!preparedCompiledSet||!EiemCompiledModSet::Unchanged(*preparedCompiledSet,gameDirectory/"plugin/mods"))preparedCompiledSet=EiemCompiledModSet::Read(gameDirectory,preparedCompiledSet);
  if(!preparedCompiledSet)return;auto pointer=preparedCompiledSet->pointer;auto failed=std::atomic_load(&failedKey);bool retry=trigger&&failed&&*failed==pointer.key;
  if(pointer!=lastPrepared||retry||!diskPrepared){
   auto p=std::make_shared<Prepared>();p->pointer=pointer;p->base=generation.load();p->graph=std::make_shared<const std::vector<Package>>(preparedCompiledSet->manifest.packages);p->authorFiles=preparedCompiledSet->files;p->plan=std::make_shared<const Plan>(preparedCompiledSet->manifest.plan);p->rules=EiemCompiledModSet::Context(preparedCompiledSet);p->ruleInputs=std::move(preparedCompiledSet->isolated);
   if(retry)for(auto index:p->plan->closure)if(!p->ruleInputs.count(index))p->ruleInputs.emplace(index,std::make_shared<const Bytes>(p->rules->Materialize(index,true)));
   auto config=EiemEntrySettings::Read(gameDirectory);p->controls=CompiledControls(p->graph,*p->plan,config);p->config=EiemEntrySettings::Prepare(config);Require(AuthorFilesUnchanged(*p),"Mod changed during compiled rule validation");
   {std::lock_guard<std::mutex> lock(mutex);pending=p;}lastPrepared=pointer;diskPrepared=p;Log("[FULL-RELOAD-PREPARED] key=%s workerMs=%.6f compiledPerMod=1 authorCompilation=0 nativeSubmission=pendingUnityFrame",pointer.key.c_str(),NativeBundle::Ms(clock));
  }
  std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(pointer));
  if(trigger){auto ticket=std::make_shared<Ticket>();ticket->pointer=pointer;ticket->clock=clock;auto config=EiemEntrySettings::Read(gameDirectory);ticket->controls=CompiledControls(diskPrepared->graph,*diskPrepared->plan,config);ticket->config=EiemEntrySettings::Prepare(config);std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));Log("[FULL-RELOAD-REQUEST] key=%s compiledPerMod=1 authorCompilation=0",pointer.key.c_str());}
  lastError.clear();failedStamp.clear();
 }catch(const std::exception& e){try{failedStamp=EiemCompiledModSet::DirectoryStamp(gameDirectory/"plugin/mods");}catch(...){failedStamp.clear();}std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});std::atomic_store(&requested,std::shared_ptr<const Ticket>{});if(trigger||lastError!=e.what()){lastError=e.what();Log("[FULL-RELOAD-REJECT] phase=compiled-mod reason=%s previousGenerationRetained=%u",e.what(),generation.load());}}
}
void PollOffline(bool trigger=false,bool allowBuild=true){
 (void)allowBuild;if(CompiledMode()){PollCompiled(trigger);return;}
 if(!enabled||!active)return;
 static std::string baselineHash,lastError;
 auto clock=trigger&&reloadClock.load()?reloadClock.load():NativeBundle::Clock();
 try{
  auto cache=gameDirectory/"plugin/resource-reload-cache";
  if(!std::filesystem::exists(cache/"current.tsv")&&DiscoverModRoots().empty())return;
  auto pointer=EiemOfflinePackage::ReadCurrent(cache);
  std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(pointer));
  auto ticket=std::atomic_load(&requested);bool waiting=ticket&&ticket->pointer.key.empty();
  auto failed=std::atomic_load(&failedKey);
  bool retry=trigger&&failed&&*failed==pointer.key;
  std::shared_ptr<Prepared> prepared;
  if(pointer!=lastPrepared||retry||trigger||waiting){
   EiemOfflinePackage::Manifest manifest;
   if(pointer!=lastPrepared||retry||!diskPrepared){manifest=EiemOfflinePackage::ReadManifest(cache,pointer,gameDirectory/"plugin/mods");}
   auto packages=manifest.plan.key.empty()?*diskPrepared->graph:EiemOfflinePackage::Authors(manifest);
   bool matches=manifest.plan.key.empty()?AuthorFilesUnchanged(*diskPrepared):AuthorMatches(packages);
   std::map<std::filesystem::path,InputFileStamp> authorFiles;
   // A converter may rename a .tex and update INI in the same publication.
   // Detect the changed INI first; missing old resource paths then mean a new
   // author build, rather than an exception that skips F10's build request.
   if(matches){authorFiles=manifest.plan.key.empty()?diskPrepared->authorFiles:AuthorFiles(packages);matches=InstalledAuthorsMatch(packages)&&AuthorFilesUnchanged(authorFiles);}
   if(trigger){
    auto next=std::make_shared<Ticket>();next->clock=clock;if(matches)next->pointer=pointer;
    std::atomic_store(&requested,std::shared_ptr<const Ticket>(next));
    Log("[FULL-RELOAD-REQUEST] key=%s offlineOnly=1 awaitingAuthorBuild=%d",pointer.key.c_str(),int(!matches));
   }else if(ticket&&matches&&ticket->pointer!=pointer){auto next=std::make_shared<Ticket>();next->clock=ticket->clock;next->pointer=pointer;std::atomic_store(&requested,std::shared_ptr<const Ticket>(next));}
   if(!matches){std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});
    if(trigger)throw std::runtime_error("Mod declarations differ from compiled rules; export the Mod with Blender first");
    else if(ticket)std::atomic_compare_exchange_strong(&requested,&ticket,std::shared_ptr<const Ticket>{});
    return;}
   if(pointer!=lastPrepared||retry){
    baselineHash=EiemResourceReloadPlan::Hash(EiemDirectory::Read(gameDirectory/"plugin/resource-reload-baseline/baseline.tsv"));
    Require(manifest.baseline==baselineHash,"offline candidate source baseline content differs");
    prepared=std::make_shared<Prepared>();prepared->base=generation.load();prepared->pointer=pointer;
    auto graph=std::make_shared<const std::vector<Package>>(std::move(packages));prepared->graph=graph;
    prepared->authorFiles=std::move(authorFiles);Require(AuthorFilesUnchanged(*prepared),"author changed during offline validation");
    auto config=EiemEntrySettings::Read(gameDirectory);prepared->controls=CompiledControls(graph,manifest.plan,config);prepared->config=EiemEntrySettings::Prepare(config);
    auto plan=std::make_shared<Plan>(std::move(manifest.plan));
    if(plan->rulesOnly){prepared->rules=std::make_shared<EiemPrefabRules::Context>(plan,std::move(manifest.package));for(auto index:plan->closure)prepared->ruleInputs.emplace(index,std::make_shared<const Bytes>(prepared->rules->Materialize(index,true)));}
    else for(auto index:plan->closure){auto hold=std::make_shared<NativeBundle::FileHold>(plan->inputs.at(index).path);
     Require(EiemResourceReloadPlan::Hash(hold->BytesLocked())==plan->inputs.at(index).hash,"offline native input SHA differs");prepared->files.emplace(index,std::move(hold));}
    Require(EiemOfflinePackage::ReadCurrent(cache)==pointer&&AuthorFilesUnchanged(*prepared),"offline publication/author changed during validation");prepared->plan=std::move(plan);
    {std::lock_guard<std::mutex> lock(mutex);pending=prepared;}
    lastPrepared=pointer;diskPrepared=prepared;
    Log("[FULL-RELOAD-PREPARED] key=%s workerMs=%.6f offlineValidated=1 rulesMaterializedOnWorker=%d nativeSubmission=pendingUnityFrame",pointer.key.c_str(),NativeBundle::Ms(clock),int(bool(prepared->rules)));
   }
   auto wanted=std::atomic_load(&requested);if(wanted&&wanted->pointer==pointer&&diskPrepared){
    auto next=std::make_shared<Ticket>(*wanted);auto config=EiemEntrySettings::Read(gameDirectory);
    next->controls=CompiledControls(diskPrepared->graph,*diskPrepared->plan,config);next->config=EiemEntrySettings::Prepare(config);
    std::atomic_store(&requested,std::shared_ptr<const Ticket>(next));
   }
  }
  lastError.clear();
 }catch(const std::exception& e){
  // A failed/new publication must never make F10 commit an older ready graph.
  std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});
  auto wanted=std::atomic_load(&requested);
  if(trigger)std::atomic_store(&requested,std::shared_ptr<const Ticket>{});
  else if(wanted)std::atomic_compare_exchange_strong(&requested,&wanted,std::shared_ptr<const Ticket>{});
  if(trigger||lastError!=e.what()){lastError=e.what();Log("[FULL-RELOAD-REJECT] phase=offline-manifest reason=%s previousGenerationRetained=%u",e.what(),generation.load());}
 }
}
void Prepare(){
 std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});std::atomic_store(&requested,std::shared_ptr<const Ticket>{});
 try{PollOffline(true,true);}
 catch(const std::exception& e){Log("[FULL-RELOAD-REJECT] phase=static-compile reason=%s previousGenerationRetained=1",e.what());}
}
void Install(void** assemblies,size_t count){
 configured=true;
 try{Require(RuntimeShapes::enabled&&NativeBundle::enabled&&il2cpp_value_box,"reload public runtime/boxing API absent");
  auto path=FindClass("Beyond.Resource","StringPathHash",assemblies,count),handle=FindClass("Beyond.Resource","FAssetProxyHandle",assemblies,count);
  Require(path&&handle,"reload resource lease classes absent");
  api.pathClass=path;api.handleClass=handle;uint32_t alignment=0;Require(il2cpp_class_value_size(path,&alignment)>0&&il2cpp_class_value_size(handle,&alignment)>0,"reload resource lease value types absent");
  auto manager=FindClass("Beyond.Resource","ResourceManager",assemblies,count);
  api.toPath=Method(path,"op_Implicit",{"System.String"},"Beyond.Resource.StringPathHash",true);
  api.load=Method(manager,"LoadAsync",{"Beyond.Resource.StringPathHash","System.Type","Beyond.Resource.RootCategory","Beyond.Resource.EResourceRequestPriority"},"Beyond.Resource.FAssetProxyHandle",true);
  api.done=Method(handle,"get_isDone",{},"System.Boolean",false);api.error=Method(handle,"get_hasError",{},"System.Boolean",false);api.get=Method(handle,"Get",{},"UnityEngine.Object",false);api.dispose=Method(handle,"Dispose",{},"System.Void",false);
  api.handlePath=Method(handle,"get_pathOrName",{},"System.String",false);
  api.handleUntrackedField=Field(handle,"m_untrackedHandle","Beyond.Resource.FAssetProxyUntrackedHandle");api.handleUntrackedClass=il2cpp_class_from_type(il2cpp_field_get_type(api.handleUntrackedField));
  api.untrackedAssetGet=Method(api.handleUntrackedClass,"get_assetProxy",{},"Beyond.Resource.IAssetProxy",false);
  api.assetSync=Method(FindClass("UnityEngine","AssetBundle",assemblies,count),"LoadAsset",{"System.String","System.Type"},"UnityEngine.Object",false);
  Require(Entry::Install(handle,"Get",{},"UnityEngine.Object",false,(void*)ResourceGet,(void**)&originalResourceGet),"game prefab resource observation hook unavailable");
  api.pathHashField=Field(path,"hash",nullptr);auto hashType=il2cpp_type_get_name(il2cpp_field_get_type(api.pathHashField));
  bool hash64=hashType&&(!strcmp(hashType,"System.Int64")||!strcmp(hashType,"System.UInt64"));if(hashType)il2cpp_free((void*)hashType);Require(hash64,"game prefab path hash is not a 64-bit scalar");
  auto proxy=FindClass("Beyond.Resource.Runtime","PrefabInstantiateProxy",assemblies,count),assetProxy=FindClass("","AssetProxy",assemblies,count);
  api.proxyAsset=Method(proxy,"get_assetHandles",{},"Beyond.Resource.IAssetProxy",false);api.proxyGet=Method(assetProxy,"Get",{},"UnityEngine.Object",false);
  api.proxyHash=Exact(assetProxy,"get_pathOrNameHash",{},"System.Int64",false);if(!api.proxyHash)api.proxyHash=Method(assetProxy,"get_pathOrNameHash",{},"System.UInt64",false);
  Require(Entry::Install(proxy,"_PreloadPrefab",{"System.Boolean"},"System.Void",false,(void*)ProxyPreload,(void**)&originalProxyPreload),"game prefab instantiate lifecycle hook unavailable");
  api.rootCategory=Enum(FindClass("Beyond.Resource","RootCategory",assemblies,count),"Main");api.priority=Enum(FindClass("Beyond.Resource","EResourceRequestPriority",assemblies,count),"Default");
  auto go=FindClass("UnityEngine","GameObject",assemblies,count),renderer=FindClass("UnityEngine","Renderer",assemblies,count);
  prefabType=Type(go);rendererType=Type(renderer);
  api.components=Method(go,"GetComponentsInChildren",{"System.Type","System.Boolean"},"UnityEngine.Component[]",false);api.goTransform=Method(go,"get_transform",{},"UnityEngine.Transform",false);
  auto component=FindClass("UnityEngine","Component",assemblies,count);
  api.componentGo=Method(component,"get_gameObject",{},"UnityEngine.GameObject",false);
  auto material=FindClass("UnityEngine","Material",assemblies,count);
  api.materials=Method(renderer,"get_sharedMaterials",{},"UnityEngine.Material[]",false);api.texture=Method(material,"GetTexture",{"System.String"},"UnityEngine.Texture",false);api.shader=Method(material,"get_shader",{},"UnityEngine.Shader",false);
  BindMaterialTextureProperties(material);
  auto texture=FindClass("UnityEngine","Texture",assemblies,count);
  api.width=Method(texture,"get_width",{},"System.Int32",false);api.height=Method(texture,"get_height",{},"System.Int32",false);api.mips=Method(texture,"get_mipmapCount",{},"System.Int32",false);
  auto entity=FindClass("Beyond.Gameplay.Core","Entity",assemblies,count),model=FindClass("Beyond.Gameplay.View","BaseModelComponent",assemblies,count);
  api.modelCom=Method(entity,"get_modelCom",{},"Beyond.Gameplay.View.ModelComponent",false);api.released=Method(entity,"get_markReleased",{},"System.Boolean",false);
  api.modelEntity=Method(FindClass("Beyond.Gameplay.Core","BaseComponent",assemblies,count),"get_entity",{},"Beyond.Gameplay.Core.Entity",false);
  api.delegateTarget=Method(FindClass("System","Delegate",assemblies,count),"get_Target",{},"System.Object",false);
  api.getModel=Method(model,"GetModelGo",{},"UnityEngine.GameObject",false);api.getPath=Method(model,"get_modelPath",{},"System.String",false);
  api.remove=Method(model,"RemoveMainPart",{},"System.Void",false);api.loadModel=Method(model,"LoadMainModelSync",{},"System.Void",false);
  auto animation=FindClass("Beyond.Gameplay.View","ComplexAnimatorComponent",assemblies,count);
  api.animationCom=Method(entity,"get_animatorCom",{},"Beyond.Gameplay.View.ComplexAnimatorComponent",false);
  api.animationData=Method(animation,"get_data",{},"Beyond.Gameplay.View.ComplexAnimatorComponentData",false);
  api.animationChange=Method(animation,"ChangeAnimationConfig",{"Beyond.Gameplay.View.ComplexAnimatorComponentData","System.Boolean"},"System.Boolean",false);
  api.animationAnimator=Method(animation,"get_animator",{},"UnityEngine.Animator",false);api.animationValid=Method(animation,"IsValid",{},"System.Boolean",false);
  auto weapon=FindClass("Beyond.Gameplay.View","WeaponComponent",assemblies,count);
  api.weaponCom=Method(entity,"get_weaponCom",{},"Beyond.Gameplay.View.WeaponComponent",false);
  api.weaponUnload=Method(weapon,"UnloadWeapons",{"System.Boolean"},"System.Void",false);
  api.weaponReload=Method(weapon,"ReloadWeapon",{"System.Boolean"},"System.Void",false);api.weaponMounts=Method(weapon,"UpdateAllWeaponMountPoint",{},"System.Void",false);
  auto audio=FindClass("Beyond.Gameplay.Core","CharacterAudioComponent",assemblies,count);
  api.componentLookup=Method(entity,"TryGetComponent",{"System.Type","Beyond.Gameplay.Core.BaseComponent&","System.Boolean"},"System.Boolean",false);
  characterAudioType=Type(audio);api.audioInvalidate=Method(audio,"InvalidateTransformReference",{},"System.Void",false);
  auto instance=FindClass("Beyond.Gameplay","GameInstance",assemblies,count);api.instance=Method(instance,"get_instance",{},"Beyond.Gameplay.GameInstance",true);
  api.modelManager=BindObjectField(instance,"modelManager","Beyond.Gameplay.View.ModelManager");
  api.effectManager=BindObjectField(instance,"effectManager","Beyond.Gameplay.EffectManager");
  auto modelManager=FindClass("Beyond.Gameplay.View","ModelManager",assemblies,count);api.discard=Method(modelManager,"DiscardPooledObjects",{"Beyond.Resource.StringPathHash"},"System.Void",false);
  auto basePart=FindClass("Beyond.Gameplay.View","BaseModelViewPart",assemblies,count),complexPart=FindClass("Beyond.Gameplay.View","ComplexModelViewPart",assemblies,count);
  Require(il2cpp_object_get_virtual_method,"reload public virtual dispatch unavailable");
  api.partRelease=Method(basePart,"ReleaseModel",{},"System.Void",false);api.partLoad=Method(basePart,"StartSyncLoad",{},"System.Boolean",false);
  api.partSetup=Method(basePart,"SetupData",{"Beyond.Gameplay.View.BaseModelViewPartData"},"System.Void",false);
  Require(Entry::Install(model,"AddPart",{"Beyond.Gameplay.View.BaseModelViewPart"},"System.Void",false,(void*)PartAdded,(void**)&originalPartAdded),"model main-part attachment lifecycle hook unavailable");
  Require(Entry::Install(basePart,"SetupData",{"Beyond.Gameplay.View.BaseModelViewPartData"},"System.Void",false,(void*)PartConfigured,(void**)&originalPartConfigured),"model part configuration lifecycle hook unavailable");
  Require(Entry::Install(basePart,"PostDealLoadedModel",{},"System.Void",false,(void*)BasePartFinished,(void**)&originalBasePartFinished),"base view part completion lifecycle hook unavailable");
  Require(Entry::Install(basePart,"ReleaseModel",{},"System.Void",false,(void*)PartReleased,(void**)&originalPartReleased),"model part release lifecycle hook unavailable");
  auto callbackType=PartCallbackType(basePart);
  Require(Entry::Install(basePart,"LoadModel",{"Beyond.Gameplay.View.BaseModelViewPartData",callbackType.c_str()},"System.Void",false,(void*)PartLoading,(void**)&originalPartLoading),"model part owner lifecycle hook unavailable");
  Require(Method(complexPart,"PostDealLoadedModel",{},"System.Void",false),"model part completion method absent");
  Require(Entry::Install(complexPart,"PostDealLoadedModel",{},"System.Void",false,(void*)PartFinished,(void**)&originalPartFinished),"model part lifecycle hook unavailable");
  auto effect=FindClass("Beyond.Gameplay","EffectManager",assemblies,count);api.follow=Method(effect,"ChangeEntityFollowEffect",{"Beyond.Gameplay.Core.Entity"},"System.Void",false);
  auto object=FindClass("UnityEngine","Object",assemblies,count);api.instanceId=Method(object,"GetInstanceID",{},"System.Int32",false);
  auto transform=FindClass("UnityEngine","Transform",assemblies,count);api.isChild=Method(transform,"IsChildOf",{"UnityEngine.Transform"},"System.Boolean",false);api.parent=Method(transform,"get_parent",{},"UnityEngine.Transform",false);
  api.childCount=Method(transform,"get_childCount",{},"System.Int32",false);api.getChild=Method(transform,"GetChild",{"System.Int32"},"UnityEngine.Transform",false);
  api.directComponents=Method(go,"GetComponents",{"System.Type"},"UnityEngine.Component[]",false);
  for(auto m:{api.toPath,api.load,api.done,api.error,api.get,api.dispose,api.components,api.goTransform,api.materials,api.texture,api.shader,api.width,api.height,api.mips,api.modelCom,api.released,api.getModel,api.getPath,api.remove,api.loadModel,api.instance,api.discard,api.follow,api.instanceId,api.isChild,api.parent,api.childCount,api.getChild,api.componentGo,api.directComponents})Require(m,"reload exact managed signature missing");
  Require(Method(object,"Internal_CloneSingle",{"UnityEngine.Object"},"UnityEngine.Object",true)&&Method(object,"Internal_CloneSingleWithParent",{"UnityEngine.Object","UnityEngine.Transform","System.Boolean"},"UnityEngine.Object",true),"reload prefab clone signatures absent");
  Require(LoadingObservation::Hook("UnityEngine.Object::Internal_CloneSingle(UnityEngine.Object)",(void*)Clone,(void**)&originalClone)&&LoadingObservation::Hook("UnityEngine.Object::Internal_CloneSingleWithParent(UnityEngine.Object,UnityEngine.Transform,System.Boolean)",(void*)CloneParent,(void**)&originalCloneParent),"reload registered prefab clone bindings absent");
  Method(object,"Internal_InstantiateSingle_Injected",{"UnityEngine.Object","UnityEngine.Vector3&","UnityEngine.Quaternion&"},"UnityEngine.Object",true);
  Method(object,"Internal_InstantiateSingleWithParent_Injected",{"UnityEngine.Object","UnityEngine.Transform","UnityEngine.Vector3&","UnityEngine.Quaternion&"},"UnityEngine.Object",true);
  Method(object,"Internal_InstantiateAsyncImmediatelyWithParent",{"UnityEngine.Object","UnityEngine.Transform","System.Boolean"},"UnityEngine.Object",true);
  Method(object,"Internal_InstantiateAsyncWithParent",{"UnityEngine.Object","System.Int32","UnityEngine.Transform","System.IntPtr","System.IntPtr"},"UnityEngine.AsyncInstantiateOperation",true);
  Require(LoadingObservation::Hook("UnityEngine.Object::Internal_InstantiateSingle_Injected(UnityEngine.Object,UnityEngine.Vector3&,UnityEngine.Quaternion&)",(void*)Instantiate,(void**)&originalInstantiate)&&
          LoadingObservation::Hook("UnityEngine.Object::Internal_InstantiateSingleWithParent_Injected(UnityEngine.Object,UnityEngine.Transform,UnityEngine.Vector3&,UnityEngine.Quaternion&)",(void*)InstantiateParent,(void**)&originalInstantiateParent)&&
          LoadingObservation::Hook("UnityEngine.Object::Internal_InstantiateAsyncImmediatelyWithParent(UnityEngine.Object,UnityEngine.Transform,System.Boolean)",(void*)InstantiateImmediate,(void**)&originalInstantiateImmediate)&&
          LoadingObservation::Hook("UnityEngine.Object::Internal_InstantiateAsyncWithParent(UnityEngine.Object,System.Int32,UnityEngine.Transform,System.IntPtr,System.IntPtr)",(void*)InstantiateAsync,(void**)&originalInstantiateAsync),"reload registered prefab Instantiate bindings absent");
  InstallUIModels(assemblies,count);
  enabled=true;Log("[FULL-RELOAD-READY] shortcut=reloadCurrentMod nativePrepareBeforeShortcut=1 unusedRootsLazy=1 nativeMainPartOwners=1 resources=Mesh-Material-Texture-Prefab privateNativeWrites=0");
 }catch(const std::exception& e){Log("[FULL-RELOAD-REJECT] phase=install reason=%s configurationFallbackRefused=1",e.what());}
}
void Dispose(Lease& lease){if(lease.shared||lease.disposed||!lease.handle.Get())return;Require(lease.done,"reload lease disposed before completion");
 auto self=Self(lease.handle,api.handleClass);lease.disposed=true;Call(api.dispose,self);}
void Drain(Attempt& a){
 if(a.prepared)a.prepared->ruleInputs.clear(); // Native arrays own copies; unsent worker bytes have no native consumer.
 for(const auto& owner:a.owners)if(owner.uiRequest&&owner.uiRequest->submitted&&!owner.uiRequest->completed)return;
 // Never drop requests/file locks while native work can still use them.
 for(auto& l:a.leases)if(l.handle.Get()&&!l.done){if(!RuntimeShapes::Scalar<unsigned char>(api.done,Self(l.handle,api.handleClass)))return;l.done=true;}
 for(auto& b:a.bundles){auto& v=b.second;if(!v.done){if(!v.request.Get()){v.done=true;continue;}if(!RuntimeShapes::Scalar<unsigned char>(NativeBundle::api.done,v.request.Get()))return;
   v.bytes={};v.object=Ref::Make(Call(NativeBundle::api.bundle,v.request.Get()),false);v.done=true;
   // Without a successful ownership audit this Bundle is quarantined.
  }}
 for(auto& v:a.assets)if(v.request.Get()&&!v.done){if(!RuntimeShapes::Scalar<unsigned char>(NativeBundle::api.done,v.request.Get()))return;v.done=true;}
 if(a.exposed){a.quarantined=true;a.phase=Phase::Drained;Log("[FULL-RELOAD-QUARANTINE] candidateWasExposed=1 nativeRequestsDrained=1 bundlesAndLeasesRetained=1");return;}
 bool owned=true;for(const auto& b:a.bundles)owned=owned&&(!b.second.object.Get()||b.second.owned);
 if(!owned){a.quarantined=true;a.phase=Phase::Drained;Log("[FULL-RELOAD-QUARANTINE] ownershipUnverified=1 nativeRequestsDrained=1 bundlesAndLeasesRetained=1");return;}
 for(auto& b:a.bundles)if(!b.second.unloadCalled&&RuntimeShapes::Alive(b.second.object.Get())){
  unsigned char all=0;void* args[]={&all};b.second.unloadCalled=true;
  try{Call(NativeBundle::api.unload,b.second.object.Get(),args);}catch(...){a.quarantined=true;a.phase=Phase::Drained;Log("[FULL-RELOAD-QUARANTINE] unloadCallFailed=1 leasesRetained=1 repeatedUnloadRefused=1");throw;}
 }
 for(auto& l:a.leases)try{Dispose(l);}catch(...){a.quarantined=true;a.phase=Phase::Drained;Log("[FULL-RELOAD-QUARANTINE] leaseDisposeCallFailed=1 remainingLeasesRetained=1 repeatedDisposeRefused=1");throw;}
 a.phase=Phase::Drained;
 Log("[FULL-RELOAD-DRAINED] ownedBundleUnloadFalse=1 leasesDisposedOnce=1 oldGenerationRetained=1");
}
void RetireGeneration(Attempt& a){
 // Unload(false) is Unity's container retirement API, not a claim that all
 // skinning jobs finished. Never destroy Mesh/Material/Texture/capture objects.
 for(const auto& b:a.bundles)Require(b.second.done&&b.second.owned,"retirement requires completed owned Bundles");
 for(auto& b:a.bundles)if(!b.second.unloadCalled&&RuntimeShapes::Alive(b.second.object.Get())){
  unsigned char destroyObjects=0;void* args[]={&destroyObjects};b.second.unloadCalled=true;
  Call(NativeBundle::api.unload,b.second.object.Get(),args);
 }
 for(auto& l:a.leases)Dispose(l);
 a.assets.clear();a.bundles.clear();a.leases.clear();a.owners.clear();a.verifier.reset();a.prepared.reset();
 Log("[FULL-RELOAD-RETIRED] bundleContainersUnloadedFalse=1 strongGenerationRefsReleased=1 sharedSourceLeases=%zu captureObjectsOwnedByNativeRelease=1",sourceLeases.size());
}
void RetireSuperseded(){
 auto currentRoutes=std::atomic_load(&routes);
 for(auto it=retained.begin();it!=retained.end();){auto& old=**it;
  bool used=std::atomic_load(&activeNative).get()==&old;for(const auto& asset:old.assets)for(const auto& route:*currentRoutes)used=used||(asset.prefab.Get()&&route.candidate.Get()==asset.prefab.Get());
  if(used||old.quarantined){++it;continue;}
  try{RetireGeneration(old);it=retained.erase(it);}catch(const std::exception& e){old.quarantined=true;Log("[FULL-RELOAD-RETIRE-GAP] reason=%s retryUnloadRefused=1",e.what());++it;}
 }
}
void VerifyAnimation(const Owner& owner,void* root){
 if(!owner.animation.Get())return;
 Require(RuntimeShapes::Scalar<unsigned char>(api.animationValid,owner.animation.Get()),"reload animation component is not valid");
 auto animator=Ref::Make(Call(api.animationAnimator,owner.animation.Get()),false);Require(RuntimeShapes::Alive(animator.Get()),"reload Animator absent");
 auto transform=Ref::Make(Call(RuntimeShapes::api.transform,animator.Get()),false),rootTransform=Ref::Make(Call(api.goTransform,root),false);void* args[]={rootTransform.Get()};
 Require(transform.Get()==rootTransform.Get()||RuntimeShapes::Scalar<unsigned char>(api.isChild,transform.Get(),args),"reload Animator still bound outside the current model root");
}
void CaptureConsumers(Owner& owner){
 if(owner.viewPart)return;
 owner.animation=Ref::Make(Call(api.animationCom,owner.entity.Get()),false);
 if(owner.animation.Get()){owner.animationData=Ref::Make(Call(api.animationData,owner.animation.Get()),false);Require(owner.animationData.Get(),"reload animation data unavailable");VerifyAnimation(owner,owner.root.Get());}
 owner.weapons=Ref::Make(Call(api.weaponCom,owner.entity.Get()),false);
 // Entity.audioCom is the separate generic AudioComponent. The game's mode
 // change queries CharacterAudioComponent through its component registry.
 void* audio=nullptr;unsigned char quiet=0;void* args[]={characterAudioType.Get(),&audio,&quiet};
 if(RuntimeShapes::Scalar<unsigned char>(api.componentLookup,owner.entity.Get(),args)){
  Require(audio&&Inherits(audio,"Beyond.Gameplay.Core","CharacterAudioComponent"),"reload typed character audio query returned another component");owner.audio=Ref::Make(audio,false);
 }
}
void ReleaseConsumers(const Owner& owner){
 if(owner.weapons.Get()){unsigned char all=1;void* args[]={&all};Call(api.weaponUnload,owner.weapons.Get(),args);}
}
void ReloadConsumers(const Owner& owner,void* root){
 // AbilitySystem::_ApplyModeChange rebuilds model, then calls the game's
 // ChangeAnimationConfig(data, tryResumeAnimations=true), then remounts
 // weapons and invalidates audio's Transform references. Reuse that chain;
 // no Animator state, controller pointers, bones or jobs are edited here.
 if(owner.animation.Get()){unsigned char resume=1;void* args[]={owner.animationData.Get(),&resume};
  Require(RuntimeShapes::Scalar<unsigned char>(api.animationChange,owner.animation.Get(),args),"game animation rebind failed");VerifyAnimation(owner,root);}
 if(owner.weapons.Get()){unsigned char all=1;void* args[]={&all};Call(api.weaponReload,owner.weapons.Get(),args);Call(api.weaponMounts,owner.weapons.Get());}
 if(owner.audio.Get())Call(api.audioInvalidate,owner.audio.Get());
 if(!owner.viewPart)Log("[FULL-RELOAD-CONSUMERS] path=%s animation=%d nativeConfigRebind=%d weapons=%d audio=%d",owner.path.c_str(),int(bool(owner.animation.Get())),int(bool(owner.animation.Get())),int(bool(owner.weapons.Get())),int(bool(owner.audio.Get())));
}
using AssetIdentity=std::map<std::string,std::pair<int,std::map<std::string,int>>>;
AssetIdentity PrefabAssetIdentity(void* root,const Plan& plan){
 AssetIdentity identity;size_t ordinal=0;
 for(const auto& r:Components(root)){auto renderer=r.Get();auto key="renderer/"+std::to_string(ordinal++);auto mesh=Ref::Make(RuntimeShapes::SourceMesh(renderer),false);
  if(!RuntimeShapes::Alive(mesh.Get())){identity.emplace(key,std::make_pair(0,std::map<std::string,int>{}));continue;}
  auto marker=String(Call(RuntimeShapes::api.name,mesh.Get()));auto part=plan.parts.find(marker);
  if(part==plan.parts.end()){
   // Renderers outside the candidate's authored part table still need complete
   // Material/Texture identity checks. Enumerate their public properties.
   std::map<std::string,int> textures;auto materials=Ref::Make(Call(api.materials,renderer),false);size_t slot=0;
   for(auto material:ReferenceArray(materials.Get())){auto prefix="material/"+std::to_string(slot++)+"/";
    if(!RuntimeShapes::Alive(material)){textures.emplace(prefix+"null",0);continue;}
    auto shader=Call(api.shader,material);textures.emplace(prefix+"shader",RuntimeShapes::Alive(shader)?Id(shader):0);
    auto ids=Ref::Make(Call(api.textureIds,material),false);size_t count=0;
    Require(ArrayLength(ids.Get(),&count),"reload public Texture property ID array absent");
    for(auto property:RuntimeShapes::Array<int32_t>(ids.Get(),count)){void* args[]={&property};auto texture=Call(api.textureById,material,args);
     Require(textures.emplace(prefix+"textureId/"+std::to_string(property),RuntimeShapes::Alive(texture)?Id(texture):0).second,"reload duplicate public Texture property ID");}
   }identity.emplace(key,std::make_pair(Id(mesh.Get()),std::move(textures)));continue;
  }
  auto materials=Ref::Make(Call(api.materials,renderer),false);auto material=ReferenceArray(materials.Get(),1).front();std::map<std::string,int> textures;
  for(const auto& slot:plan.materials.at(part->second.material).slots){if(!slot.local)continue;auto property=Ref::Make(il2cpp_string_new(slot.property.c_str()),false);void* args[]={property.Get()};textures.emplace(slot.property,slot.nonnull?Id(Call(api.texture,material,args)):0);}
  identity.emplace(key,std::make_pair(Id(mesh.Get()),std::move(textures)));
 }return identity;
}
void DiscoverOwners(Attempt& a){
 std::set<void*> seen,coveredRoots;
 auto world=[&](void* model,void* entity,void* expectedRoot=nullptr,void* expectedPart=nullptr){
  Require(model&&entity&&Call(api.modelEntity,model)==entity&&Call(api.modelCom,entity)==model,"reload component/entity owner differs");
  if(!seen.insert(model).second)return;
  auto path=String(Call(api.getPath,model));auto root=Call(api.getModel,model);if(!RuntimeShapes::Alive(root))return;
  if(!FindRoot(*a.prepared->plan,path)){Log("[FULL-RELOAD-OWNER-SKIP] modelPath=%s exactPrefabRootAbsent=1",path.c_str());return;}
  Require(!RuntimeShapes::Scalar<unsigned char>(api.released,entity),"reload entity already released");
  auto part=ReadObject(model,"m_mainPart","Beyond.Gameplay.View.ComplexModelViewPart");Require(part,"reload main model part missing");
  Require((!expectedRoot||expectedRoot==root)&&(!expectedPart||expectedPart==part),"reload recorded model part/root owner differs");
  Require(ReadObject(part,"m_model","UnityEngine.GameObject")==root,"reload main part/model root differs");
  auto cfg=PartConfig(part);unsigned char persistent=0;il2cpp_field_get_value(cfg.Get(),Field(il2cpp_object_get_class(cfg.Get()),"usePersistentPool","System.Boolean"),&persistent);
  Require(!persistent,"persistent model pool requires a separate verified invalidation entry");
  Owner owner;owner.entity=Ref::Make(entity,false);owner.component=Ref::Make(model,false);owner.root=Ref::Make(root,false);owner.path=path;owner.pathHash=Path(path);owner.oldRootId=Id(root);owner.originalMeshes=MeshProfile(root);CaptureConsumers(owner);a.owners.push_back(std::move(owner));coveredRoots.insert(root);
 };
 // Game load callbacks record vanilla and authored instances alike, before
 // any Mod is added. Enumerate those model parts, never scene Renderers or
 // helper links from unrelated weapon/effect/capture consumers.
 for(auto it=liveParts.begin();it!=liveParts.end();){auto part=it->second.part.Get(),root=it->second.root.Get();
  // An asynchronous creation may already be using the previous prefab.
  // Keep the weak record, but never commit over an affected unfinished load;
  // otherwise its old root could appear after the new generation publishes.
  // Other paths are independent and do not block this transaction.
  if(part&&!it->second.root.handle){auto cfg=PartConfig(part);auto path=String(ReadObject(cfg.Get(),"modelPath","System.String"));
   Require(!FindRoot(*a.prepared->plan,path),"reload matching model part is still loading; retry F10 after it finishes");++it;continue;
  }
  if(!part||!RuntimeShapes::Alive(root)||ReadObject(part,"m_model","UnityEngine.GameObject")!=root){it=liveParts.erase(it);continue;}
  auto& live=it++->second;if(coveredRoots.count(root)||!FindRoot(*a.prepared->plan,live.path))continue;
  Require(live.ownerRecorded,"reload model part load ownership was not recorded");
  Require(live.ownershipError.empty(),"reload model part ownership unavailable: "+live.ownershipError);
  if(live.model.handle){auto model=live.model.Get();Require(model,"reload recorded model owner retired");
   world(model,Call(api.modelEntity,model),root,part);continue;
  }
  Require(!live.persistent,"display persistent pool invalidation unavailable");
  Owner owner;owner.viewPart=true;owner.component=Ref::Make(part,false);owner.root=Ref::Make(root,false);owner.path=live.path;owner.pathHash=Path(live.path);owner.oldRootId=Id(root);owner.originalMeshes=MeshProfile(root);owner.config=PartConfig(part);a.owners.push_back(std::move(owner));coveredRoots.insert(root);
 }
 // Every currently bound authored Renderer must belong to a captured game
 // lifecycle owner. No direct GameObject replacement behind a UI controller.
 for(const auto& request:pendingUIModels)Require(!FindRoot(*a.prepared->plan,request.second),"reload matching UI model is still loading; retry F10 after it finishes");
 for(auto it=liveUIModels.begin();it!=liveUIModels.end();){auto root=it->second.root.Get(),loader=it->second.loader.Get();
  if(!loader||!RuntimeShapes::Alive(root)||!UIOwns(loader,root)){it=liveUIModels.erase(it);continue;}
  const auto& live=it++->second;if(!FindRoot(*a.prepared->plan,live.path)||coveredRoots.count(root))continue;
  Require(!live.parent.handle||RuntimeShapes::Alive(live.parent.Get()),"UI model parent retired");
  Owner owner;owner.uiModel=true;owner.viewPart=true;owner.component=Ref::Make(loader,false);owner.root=Ref::Make(root,false);owner.uiLoadedRoot=owner.root;
  owner.uiParent=live.parent;owner.uiCallback=live.callback;owner.path=live.path;owner.pathHash=Path(live.path);owner.oldRootId=Id(root);owner.originalMeshes=MeshProfile(root);
  owner.uiPage=CaptureUIPage(loader,root);Require(BindUIPage(owner.uiPage),"UI page does not own its recorded model");
  a.owners.push_back(std::move(owner));coveredRoots.insert(root);
 }
 for(const auto& binding:RuntimeShapes::bound){auto renderer=binding.second.renderer.Get();if(!RuntimeShapes::Alive(renderer))continue;auto transform=Call(RuntimeShapes::api.transform,renderer);bool covered=false;
  for(auto root:coveredRoots){auto rt=Call(api.goTransform,root);void* args[]={rt};covered=covered||transform==rt||RuntimeShapes::Scalar<unsigned char>(api.isChild,transform,args);}
  Require(covered,"live Renderer has no recorded game model-part lifecycle");
 }
 auto worlds=std::count_if(a.owners.begin(),a.owners.end(),[](const auto& owner){return !owner.viewPart;});
 Log("[FULL-RELOAD-OWNERS] currentWorldOwners=%zu liveDisplayOwners=%zu recordedPartOwners=1 sceneRendererDiscovery=0 typedOwnerAndParentVerified=1",size_t(worlds),a.owners.size()-size_t(worlds));
}
void* ModelManager(){return ReadManager(api.modelManager);}
void Follow(const Owner& owner){if(owner.viewPart)return;auto effects=Ref::Make(ReadManager(api.effectManager),false);Require(effects.Get(),"reload EffectManager unavailable");void* args[]={owner.entity.Get()};Call(api.follow,effects.Get(),args);}
void AbortUIStaging(Attempt& a){
 if(!a.uiStaged)return;
 if(a.previousRoutes)std::atomic_store(&routes,a.previousRoutes);
 CancelUIModels(a);a.uiStaged=false;
}
void CheckUIOwners(const Attempt& a){
 auto covered=[&](void* root){return std::any_of(a.owners.begin(),a.owners.end(),[&](const auto& owner){return owner.root.Get()==root;});};
 for(const auto& owner:a.owners){Require(RuntimeShapes::Alive(owner.root.Get())&&CurrentRoot(owner)==owner.root.Get(),"reload owner changed while UI creation was pending");
  if(owner.uiModel)Require(UIOwns(owner.component.Get(),owner.root.Get())&&BindUIPage(owner.uiPage)&&(!owner.uiParent.handle||RuntimeShapes::Alive(owner.uiParent.Get())),"UI page released its displayed model while reload was pending");}
 for(const auto& live:liveParts){auto part=live.second.part.Get(),root=live.second.root.Get();if(!part)continue;
  if(!live.second.root.handle){auto cfg=PartConfig(part);Require(!FindRoot(*a.prepared->plan,String(ReadObject(cfg.Get(),"modelPath","System.String"))),"matching model began loading while UI reload was pending");}
  else if(RuntimeShapes::Alive(root)&&FindRoot(*a.prepared->plan,live.second.path))Require(covered(root),"matching model appeared while UI reload was pending");
 }
 for(const auto& live:liveUIModels)if(RuntimeShapes::Alive(live.second.root.Get())&&FindRoot(*a.prepared->plan,live.second.path))Require(covered(live.second.root.Get()),"matching UI model appeared while reload was pending");
 for(const auto& request:pendingUIModels)if(!uiReloadRequests.count(request.first))Require(!FindRoot(*a.prepared->plan,request.second),"matching UI page began loading while reload was pending");
}
void Rebuild(Attempt& a){
 if(a.uiStaged)try{
  CheckUIOwners(a);bool waiting=false;
  for(const auto& owner:a.owners)if(owner.uiModel){Require(owner.uiRequest&&!owner.uiRequest->cancelled,"game cancelled the UI reload request");Require(owner.uiRequest->error.empty(),owner.uiRequest->error);waiting=waiting||!owner.uiRequest->completed;}
  if(waiting){a.phase=Phase::UIWait;return;}
 }catch(...){AbortUIStaging(a);throw;}
 Require(a.prepared->base==generation.load(),"reload publication generation is stale");
 // Only directory membership/file metadata is inspected here; native input
 // parsing and hashing remain on the disk worker. Never publish a set whose
 // authors were added/removed after F10's offline preflight.
 Require(AuthorFilesUnchanged(*a.prepared),"reload author set/files changed after preflight");
 auto previous=a.uiStaged?a.previousRoutes:std::atomic_load(&routes);auto next=std::make_shared<Routes>(*previous);
 for(size_t i=0;i<a.assets.size();++i){auto source=a.leases[i].source.Get(),candidate=a.assets[i].prefab.Get();if(!candidate)continue;Require(Id(source)!=Id(candidate),"reload native prefab aliases original");
  auto it=std::find_if(next->begin(),next->end(),[&](const auto& r){return r.source.Get()==source;});Route route{Ref::Make(source,false),Ref::Make(candidate,false),Id(source),Id(candidate)};
  if(it==next->end())next->push_back(std::move(route));else *it=std::move(route);
 }
 // Preallocate everything used after publication, and preserve live controls.
 RuntimeControls::MergeLive(a.prepared->controls);auto manager=Ref::Make(ModelManager(),false);Require(manager.Get(),"reload ModelManager unavailable");
 auto effectManager=Ref::Make(ReadManager(api.effectManager),false);
 Require(effectManager.Get(),"reload EffectManager unavailable");
 std::vector<Ref> poolPaths;for(const auto& root:a.prepared->plan->roots)poolPaths.push_back(Path(root.path));
 for(const auto& owner:a.owners){Require(CurrentRoot(owner)==owner.root.Get()&&Id(owner.root.Get())==owner.oldRootId&&(owner.viewPart||!RuntimeShapes::Scalar<unsigned char>(api.released,owner.entity.Get())),"reload owner changed after preflight");}
 if(!a.uiStaged){
  a.previousRoutes=previous;a.nextRoutes=next;a.uiStaged=true;
  // Source leases are already complete. Submit UI instantiation through the
  // candidate route, then restore public routes while Unity finishes its job.
  // The old UI instance and page callback are untouched during this wait.
  auto before=rebuilding;rebuilding=&a;std::atomic_store(&routes,a.nextRoutes);
  try{for(auto& owner:a.owners)if(owner.uiModel){auto expected=FindRoot(*a.prepared->plan,owner.path);Require(expected,"UI request has no prepared prefab root");auto index=size_t(expected-a.prepared->plan->roots.data());owner.uiRequest=LoadUIModel(owner.component.Get(),owner.path,owner.uiParent.Get(),nullptr,a.assets.at(index).prefab);}}
  catch(...){rebuilding=before;AbortUIStaging(a);throw;}
  rebuilding=before;std::atomic_store(&routes,previous);
 }
 try{
  CheckUIOwners(a);
  bool pendingUI=false;
  for(const auto& owner:a.owners)if(owner.uiModel){
   const auto& request=owner.uiRequest;Require(request&&!request->cancelled,"game cancelled the UI reload request");
   Require(request->error.empty(),request->error);pendingUI=pendingUI||!request->completed;
  }
  if(pendingUI){a.phase=Phase::UIWait;Log("[FULL-RELOAD-UI-WAIT] oldModelsRetained=1 frameCanContinue=1");return;}
  // Verify all new UI models before releasing any world or UI instance and
  // before allowing a panel callback to change its model reference.
  for(const auto& owner:a.owners)if(owner.uiModel){
   auto root=owner.uiRequest->root.Get();Require(RuntimeShapes::Alive(root)&&Id(root)!=owner.oldRootId,"UI reload returned the displayed root");
   const auto* expected=FindRoot(*a.prepared->plan,owner.path);Require(expected,"UI reload root disappeared");
   VerifyPrefab(root,*a.prepared->plan,*expected,true);
   auto index=size_t(expected-&a.prepared->plan->roots.front());
   Require(PrefabAssetIdentity(root,*a.prepared->plan)==PrefabAssetIdentity(a.assets.at(index).prefab.Get(),*a.prepared->plan),"UI reload references old Mesh/Texture resources");
  }
 }catch(...){AbortUIStaging(a);throw;}
 a.exposed=true;std::atomic_store(&routes,a.nextRoutes);
 struct Scope {bool paused=RuntimeShapes::registrationPaused;Scope(Attempt* a){rebuilding=a;RuntimeShapes::registrationPaused=true;}
  void Resume(){rebuilding=nullptr;RuntimeShapes::registrationPaused=paused;}~Scope(){Resume();}}scope(&a);
 try{
  // Release every live owner before invalidating any shared path pool. Also
  // discard matched idle-only paths, so opening the UI later cannot reuse an
  // old pooled root that escaped the currently visible owner enumeration.
  for(size_t i=0;i<a.owners.size();++i){const auto& owner=a.owners[i];if(owner.uiModel)continue;a.mutated=i+1;ReleaseConsumers(owner);RemoveOwner(owner);}
  for(const auto& path:poolPaths){void* args[]={Self(path,api.pathClass)};Call(api.discard,manager.Get(),args);}
  for(auto& owner:a.owners){
   if(owner.uiModel)continue;
   // This pair and follow-effect refresh are the audited game's own same-ID
   // lifecycle, excluding SetModelId's same-ID early return. No field edits.
   auto before=a.cloneCount;LoadOwner(owner);
   auto root=CurrentRoot(owner);Require(RuntimeShapes::Alive(root)&&Id(root)!=owner.oldRootId,"reload model pool returned old root");
   Require(a.cloneCount>before,"game-owned rebuild did not instantiate routed prefab");
   const auto* expected=FindRoot(*a.prepared->plan,owner.path);Require(expected,"reload owner root disappeared after preflight");
   VerifyPrefab(root,*a.prepared->plan,*expected,true);
   auto index=size_t(expected-&a.prepared->plan->roots.front());
   Require(PrefabAssetIdentity(root,*a.prepared->plan)==PrefabAssetIdentity(a.assets.at(index).prefab.Get(),*a.prepared->plan),"reload model references old Mesh/Texture resources");
   ReloadConsumers(owner,root);Follow(owner);
   Log("[FULL-RELOAD-MIGRATED] path=%s oldRootId=%d newRootId=%d candidateCloneObserved=1 gameOwnsInstance=1",owner.path.c_str(),owner.oldRootId,Id(root));
  }
  for(const auto& owner:a.owners)if(owner.uiModel){
   LoadOwner(owner);
   Log("[FULL-RELOAD-MIGRATED] path=%s oldRootId=%d newRootId=%d verifiedAsyncCandidate=1 gameOwnsInstance=1",owner.path.c_str(),owner.oldRootId,Id(CurrentRoot(owner)));
  }
 }catch(...){
  std::atomic_store(&routes,previous);bool restored=true;
  CancelUIModels(a);a.uiStaged=false;
  for(auto& owner:a.owners)if(owner.uiModel&&CurrentRoot(owner)!=owner.root.Get())try{
   Require(UIOwns(owner.component.Get(),owner.root.Get())&&BindUIPage(owner.uiPage,owner.root.Get()),"reload rollback UI page did not retain its original instance");
   RemoveOwner(owner);owner.uiLoadedRoot=owner.root;
   RecordUIModel(owner.component.Get(),owner.root.Get(),owner.path,owner.uiParent.Get(),owner.uiCallback.Get());
  }catch(const std::exception& e){restored=false;Log("[FULL-RELOAD-ROLLBACK-GAP] uiPage=%s rootsAndLeasesRetained=1",e.what());}
  for(size_t i=0;i<a.mutated;++i)try{
   auto& owner=a.owners[i];if(owner.uiModel)continue;void* args[]={Self(owner.pathHash,api.pathClass)};ReleaseConsumers(owner);RemoveOwner(owner);Call(api.discard,manager.Get(),args);LoadOwner(owner);
   auto restoredRoot=CurrentRoot(owner);Require(RuntimeShapes::Alive(restoredRoot),"reload rollback original root missing");ReloadConsumers(owner,restoredRoot);Follow(owner);
   Require(MeshProfile(restoredRoot)==owner.originalMeshes,"reload rollback original native Mesh identity/coverage differs");
  }catch(const std::exception& e){restored=false;Log("[FULL-RELOAD-ROLLBACK-GAP] reason=%s rootsAndLeasesRetained=1",e.what());}
  scope.Resume();
  for(const auto& owner:a.owners)if(owner.uiModel)try{RuntimeShapes::RegisterModel(CurrentRoot(owner));}catch(const std::exception& e){restored=false;Log("[FULL-RELOAD-ROLLBACK-GAP] uiControlRegistration=%s",e.what());}
  for(size_t i=0;i<a.mutated;++i)try{RuntimeShapes::RegisterModel(CurrentRoot(a.owners[i]));}catch(const std::exception& e){restored=false;Log("[FULL-RELOAD-ROLLBACK-GAP] controlRegistration=%s",e.what());}
  Log("[FULL-RELOAD-ROLLBACK] previousRoutesRestored=1 previousControlsRetained=1 ownersRestored=%d",int(restored));throw;
 }
 auto graph=a.prepared->controls.graph;RuntimeControls::Swap(a.prepared->controls);std::atomic_store(&published,graph);settings.PublishPrepared(a.prepared->config);
 RuntimeShapes::bound.clear();RuntimeShapes::cached.clear();RuntimeShapes::boundCount=0;
 // Installed original-namespace substitutions stay frozen. Future instances
 // receive generation prefabs through the exact native source route.
 auto current=a.prepared->base+1;RuntimeShapes::packageGeneration=current;RuntimeShapes::checkedGeneration=current;generation.store(current);
 scope.Resume();
 // Release the old UI root only after the game's page item accepted the new
 // root and the new controls were published. The page may have released
 // it itself; ask its actual ownership dictionary before unloading.
 for(const auto& owner:a.owners)if(owner.uiModel&&UIOwns(owner.component.Get(),owner.root.Get()))try{void* args[]={owner.root.Get()};Call(api.uiUnload,owner.component.Get(),args);}catch(const std::exception& e){Log("[FULL-RELOAD-UI-GAP] oldInstanceRelease=%s",e.what());}
 a.uiStaged=false;
 a.previousRoutes.reset();a.nextRoutes.reset();
 for(const auto& owner:a.owners)try{RuntimeShapes::RegisterModel(CurrentRoot(owner));}catch(const std::exception& e){Log("[CONTROL-RUNTIME-GAP] modelCompletionRegistration=%s",e.what());}
 if(a.prepared->testAuthor)testAuthorCommitted=true;
 if(a.prepared->rules)std::atomic_store(&resourceRules,a.prepared->rules);
 if(attempt.get()==&a)std::atomic_store(&activeNative,attempt);
 auto loadedRoots=std::count_if(a.assets.begin(),a.assets.end(),[](const auto& asset){return bool(asset.prefab.Get());});
 a.phase=Phase::Passed;Log("[FULL-RELOAD-COMMIT] generation=%u key=%s nativeRoots=%zu offlineRoots=%zu migrated=%zu meshParts=%zu rendererParts=%zu textures=%zu originalDependenciesLeased=1 nativeShapesPreserved=1 oldCaptureConsumersRetained=%zu gfx=0",current,a.prepared->plan->key.c_str(),size_t(loadedRoots),a.assets.size(),a.owners.size(),a.prepared->plan->meshParts,a.prepared->plan->rendererParts,a.prepared->plan->textures,RuntimeShapes::groups.size());
}
bool SelectObservedRoots(Attempt& a){
 if(a.selectionRevision==ownerRevision)return false;a.selectionRevision=ownerRevision;bool added=false;const auto& plan=*a.prepared->plan;
 for(size_t i=0;i<plan.roots.size();++i){auto observed=observedPrefabs.find(NormalizeAssetPath(plan.roots[i].path));
  bool needed=observed!=observedPrefabs.end()&&RuntimeShapes::Alive(observed->second.Get());
  for(const auto& part:liveParts)needed=needed||(RuntimeShapes::Alive(part.second.root.Get())&&SameAssetPath(part.second.path,plan.roots[i].path));
  for(const auto& model:liveUIModels)needed=needed||(RuntimeShapes::Alive(model.second.root.Get())&&SameAssetPath(model.second.path,plan.roots[i].path));
  if(needed&&a.selectedRoots.insert(i).second){a.leases[i].done=false;a.assets[i].done=false;added=true;}
 }return added;
}
void Begin(std::shared_ptr<Prepared> p){
 Require(p&&p->base==generation.load(),"reload prepared graph stale");
 auto value=std::make_shared<Attempt>();value->prepared=std::move(p);value->started=GetTickCount64();value->leases.resize(value->prepared->plan->roots.size());value->assets.resize(value->leases.size());
 value->prepared->rootHashes.clear();for(size_t i=0;i<value->prepared->plan->roots.size();++i){auto path=Path(value->prepared->plan->roots[i].path);uint64_t hash=0;il2cpp_field_get_value(path.Get(),api.pathHashField,&hash);
  Require(value->prepared->rootHashes.emplace(hash,i).second,"offline prefab root path hash collision");}
 value->selectionRevision=~ownerRevision;SelectObservedRoots(*value);attempt=std::move(value);
 Log("[FULL-RELOAD-BEGIN] key=%s inputs=%zu closure=%zu directSharedCABs=%zu nativeSubmission=awaitShortcut",attempt->prepared->plan->key.c_str(),attempt->prepared->plan->inputs.size(),attempt->prepared->plan->closure.size(),attempt->prepared->plan->shared.size());
}
void CheckBundle(Attempt& a,size_t index,Bundle& b){
 Require(RuntimeShapes::Alive(b.object.Get()),"reload candidate Bundle load failed");b.id=Id(b.object.Get());
 {std::lock_guard<std::mutex> lock(LoadingObservation::mutex);for(const auto& original:LoadingObservation::bundles)if(original.second.weak.Get()==original.first&&RuntimeShapes::Alive(original.first))Require(Id(original.first)!=b.id,"reload candidate Bundle aliases observed original");}
 for(const auto& previous:a.bundles)if(previous.first!=index&&previous.second.done)Require(previous.second.id!=b.id,"reload two candidate inputs alias native Bundle");
 for(const auto& r:retained)for(const auto& old:r->bundles)if(old.second.done)Require(old.second.id!=b.id,"reload generation native Bundle cache collision");
 b.done=true;b.owned=true;Log("[FULL-RELOAD-BUNDLE] file=%s nativeId=%d namespaceIsolated=1",a.prepared->plan->inputs[index].file.c_str(),b.id);
}
void Step(Attempt& a){
 const auto& plan=*a.prepared->plan;Require(a.prepared->base==generation.load(),"reload active generation became stale");
 if(a.phase==Phase::Owners||a.phase==Phase::Rebuild||a.phase==Phase::UIWait){auto current=std::atomic_load(&latest);auto ticket=std::atomic_load(&requested);
  Require(current&&*current==a.prepared->pointer&&(!ticket||ticket->clock<=a.requestClock),"reload candidate/request superseded before live mutation");
 }
 const auto start=NativeBundle::Clock();auto budget=[&]{return NativeBundle::Ms(start)<2.;};
 switch(a.phase){
 case Phase::AwaitRequest:return;
 case Phase::Leases:{
  size_t completed=0;for(size_t i=0;i<a.leases.size()&&budget();++i){auto& l=a.leases[i];if(l.done){++completed;continue;}
   if(!a.selectedRoots.count(i)){l.done=true;a.assets[i].done=true;++completed;continue;}
   if(!l.handle.Get()){
    auto existing=sourceLeases.find(plan.roots[i].path);
    if(existing!=sourceLeases.end()&&existing->second.done&&RuntimeShapes::Alive(existing->second.source.Get())){l=existing->second;++completed;continue;}
    l.pathHash=Path(plan.roots[i].path);void* args[]={Self(l.pathHash,api.pathClass),prefabType.Get(),api.rootCategory.Data(),api.priority.Data()};l.handle=Ref::Make(Call(api.load,nullptr,args),false);Require(l.handle.Get(),"reload resource lease handle null");}
   if(!RuntimeShapes::Scalar<unsigned char>(api.done,Self(l.handle,api.handleClass)))continue;
   l.done=true;Require(!RuntimeShapes::Scalar<unsigned char>(api.error,Self(l.handle,api.handleClass)),"reload original prefab game resource lease failed: "+plan.roots[i].path);
   {SourceReadScope scope;l.source=Ref::Make(Call(api.get,Self(l.handle,api.handleClass)),false);}Require(RuntimeShapes::Alive(l.source.Get()),"reload source prefab missing after game lease");
   l.shared=true;sourceLeases[plan.roots[i].path]=l;
   Log("[FULL-RELOAD-LEASE] path=%s sourcePrefabId=%d gameTrackedHandle=1 retained=1",plan.roots[i].path.c_str(),Id(l.source.Get()));++completed;
  }if(completed==a.leases.size()){a.cursor=0;a.phase=Phase::Bundles;}return;
 }
 case Phase::Bundles:{
  // Multiple in-flight Bundle opens; Prefab asset loads begin only after the
  // entire dependency closure is registered. No two-frame floor per Bundle.
  size_t inFlight=0;for(const auto& row:a.bundles)if(row.second.request.Get()&&!row.second.done)++inFlight;
  for(auto index:plan.closure){if(!budget())break;auto& b=a.bundles[index];if(b.done)continue;
   if(b.request.Get()&&RuntimeShapes::Scalar<unsigned char>(NativeBundle::api.done,b.request.Get())){
    b.bytes={};b.object=Ref::Make(Call(NativeBundle::api.bundle,b.request.Get()),false);CheckBundle(a,index,b);--inFlight;
   }
  }
  for(auto index:plan.closure){if(!budget()||inFlight>=4)break;auto& b=a.bundles[index];if(b.request.Get())continue;
   if(a.prepared->rules){auto bytes=a.prepared->ruleInputs.at(index);b.bytes=NativeBundle::Array(*bytes);b.request=Ref::Make(NativeBundle::LoadMemory(b.bytes.Get(),0,true),false);}
   else{b.file=a.prepared->files.at(index);Require(bool(b.file),"reload candidate worker file lock lost");b.path=Ref::Make(il2cpp_string_new(plan.inputs[index].path.u8string().c_str()),false);
    uint32_t crc=0;uint64_t offset=0;void* args[]={b.path.Get(),&crc,&offset};b.request=Ref::Make(Call(NativeBundle::api.load,nullptr,args),false);}
   Require(b.request.Get(),"reload native bundle request null");a.prepared->ruleInputs.erase(index);++inFlight;
  }
  bool complete=true;for(auto index:plan.closure)complete=complete&&a.bundles.count(index)&&a.bundles.at(index).done;
  if(complete){a.cursor=0;a.phase=Phase::Assets;}return;
 }
 case Phase::Assets:{
  size_t completed=0;for(size_t i=0;i<a.assets.size()&&budget();++i){auto& asset=a.assets[i];if(asset.done){++completed;continue;}
   if(!asset.request.Get()){asset.label=Ref::Make(il2cpp_string_new(plan.roots[i].path.c_str()),false);void* args[]={asset.label.Get(),prefabType.Get()};asset.request=Ref::Make(Call(NativeBundle::api.assetLoad,a.bundles.at(plan.roots[i].input).object.Get(),args),false);Require(asset.request.Get(),"reload candidate prefab request null");}
   if(!RuntimeShapes::Scalar<unsigned char>(NativeBundle::api.done,asset.request.Get()))continue;
   asset.prefab=Ref::Make(Call(NativeBundle::api.result,asset.request.Get()),false);asset.done=true;
   Require(RuntimeShapes::Alive(asset.prefab.Get())&&Id(asset.prefab.Get())!=Id(a.leases[i].source.Get()),"reload candidate prefab missing or original cache reused");++completed;
  }if(completed==a.assets.size()){a.cursor=0;a.phase=Phase::Verify;}return;
 }
 case Phase::Verify:{
  while(a.cursor<a.assets.size()&&budget()){
   if(!a.assets[a.cursor].prefab.Get()){++a.cursor;continue;}
   if(!a.verifier)a.verifier=std::make_shared<VerifyWalk>(a.assets[a.cursor].prefab.Get(),plan,plan.roots.at(a.cursor));
   if(a.verifier->Step()){a.verifier.reset();++a.cursor;}
  }
  if(a.cursor==a.assets.size()){a.cursor=0;a.phase=Phase::Ready;Log("[FULL-RELOAD-LOADED] key=%s nativeLoadMs=%llu oldModelVisible=1",plan.key.c_str(),(unsigned long long)(GetTickCount64()-a.nativeStarted));}
  return;
 }
 case Phase::Owners:DiscoverOwners(a);a.phase=Phase::Rebuild;return;
 case Phase::Rebuild:case Phase::UIWait:Rebuild(a);return;
 default:return;
 }
}
void Frame(){
 if(!enabled||!RuntimeShapes::Main())return;
 NativeBundle::CollectMemory();
 RetireSuperseded();
 std::shared_ptr<Prepared> next;{std::lock_guard<std::mutex> lock(mutex);next=pending;}
 if(next){
  if(attempt&&attempt->prepared->pointer==next->pointer&&attempt->phase!=Phase::Drained&&attempt->phase!=Phase::Reject){std::lock_guard<std::mutex> lock(mutex);if(pending==next)pending.reset();}
  else if(attempt&&attempt->phase!=Phase::Passed&&attempt->phase!=Phase::Drained){AbortUIStaging(*attempt);attempt->phase=Phase::Reject;}
  else{if(attempt){if(attempt->exposed||attempt->quarantined)retained.push_back(std::move(attempt));else attempt.reset();}
   {std::lock_guard<std::mutex> lock(mutex);if(pending==next)pending.reset();}
   auto key=next->pointer.key;
   try{next->base=generation.load();Begin(std::move(next));}catch(const std::exception& e){std::atomic_store(&failedKey,std::make_shared<const std::string>(key));Log("[FULL-RELOAD-REJECT] phase=begin reason=%s",e.what());}
  }
 }
 if(!attempt)return;auto& a=*attempt;
 if(a.phase==Phase::Reject){try{Drain(a);}catch(const std::exception& e){Log("[FULL-RELOAD-DRAIN-GAP] reason=%s nativeRootsRetained=1",e.what());}return;}
 if(a.phase==Phase::Drained)return;
 auto ticket=std::atomic_load(&requested);auto current=std::atomic_load(&latest);
 if(a.phase!=Phase::Passed&&a.phase!=Phase::Reject&&a.phase!=Phase::Drained&&a.phase!=Phase::Owners&&a.phase!=Phase::Rebuild&&a.phase!=Phase::UIWait){
  if(SelectObservedRoots(a)&&a.phase!=Phase::AwaitRequest){a.cursor=0;a.verifier.reset();a.phase=Phase::Leases;}
 }
 if(a.phase==Phase::AwaitRequest&&(!liveParts.empty()||!liveUIModels.empty())&&current&&*current==a.prepared->pointer){
  bool complete=false;for(const auto& part:liveParts)complete=complete||RuntimeShapes::Alive(part.second.root.Get());
  for(const auto& model:liveUIModels)complete=complete||RuntimeShapes::Alive(model.second.root.Get());
  if(complete){a.nativeStarted=GetTickCount64();a.phase=Phase::Leases;
   Log("[FULL-RELOAD-PREWARM] key=%s loadedRoots=%zu offlineRoots=%zu resourcesNotPublished=1",a.prepared->pointer.key.c_str(),a.selectedRoots.size(),a.prepared->plan->roots.size());}
 }
 if(ticket&&ticket->config&&ticket->pointer==a.prepared->pointer&&current&&*current==ticket->pointer&&a.phase==Phase::AwaitRequest){
  a.requestClock=ticket->clock;a.nativeStarted=GetTickCount64();a.phase=Phase::Leases;
  Log("[FULL-RELOAD-LOAD] key=%s reloadShortcutReceived=1 stableWorldGate=0",a.prepared->pointer.key.c_str());
 }
 if(ticket&&ticket->config&&ticket->pointer==a.prepared->pointer&&current&&*current==ticket->pointer&&(a.phase==Phase::Ready||a.phase==Phase::Passed)){
  a.prepared->base=generation.load();a.prepared->controls=ticket->controls;a.prepared->config=ticket->config;
  a.owners.clear();a.mutated=0;a.cloneCount=0;a.previousRoutes.reset();a.nextRoutes.reset();a.uiStaged=false;a.requestClock=ticket->clock;a.phase=Phase::Owners;
  std::shared_ptr<const Ticket> expected=ticket;std::atomic_compare_exchange_strong(&requested,&expected,std::shared_ptr<const Ticket>{});
 }
 if(a.phase==Phase::Ready||a.phase==Phase::Passed)return;
 auto start=NativeBundle::Clock();
 try{
  for(unsigned i=0;i<16&&NativeBundle::Ms(start)<2.;++i){auto phase=a.phase;auto operation=NativeBundle::Clock();Step(a);a.costs[int(phase)]+=NativeBundle::Ms(operation);
   if(a.phase==Phase::Ready||a.phase==Phase::Passed||a.phase==phase)break;
  }
  if(a.phase==Phase::Passed&&a.requestClock){Log("[FULL-RELOAD-LATENCY] key=%s requestToCommitMs=%.6f frameMainMs=%.6f rulesMaterializedOnWorker=%d",a.prepared->plan->key.c_str(),NativeBundle::Ms(a.requestClock),NativeBundle::Ms(start),int(bool(a.prepared->rules)));
   for(const auto& cost:a.costs)Log("[FULL-RELOAD-NATIVE-COST] phase=%d mainMs=%.6f",cost.first,cost.second);a.requestClock=0;}
 }catch(const std::exception& e){AbortUIStaging(a);a.error=e.what();
  // A subsequent F10 can fail owner preflight on an already committed graph.
  // Its routes, lazy templates and Bundle containers are still the active
  // generation. Keep them reusable; rebuilding the same CABs would collide.
  if(std::atomic_load(&activeNative).get()==&a){a.phase=Phase::Passed;a.requestClock=0;a.owners.clear();a.mutated=0;}
  else{a.phase=Phase::Reject;std::atomic_store(&failedKey,std::make_shared<const std::string>(a.prepared->pointer.key));}
  Log("[FULL-RELOAD-REJECT] phase=native reason=%s previousGenerationRetained=%u",e.what(),generation.load());}
 auto ms=NativeBundle::Ms(start);if(ms>4)Log("[FULL-RELOAD-FRAME] phase=%d mainMs=%.6f",int(a.phase),ms);
}
} // namespace FullResourceReload
