#pragma once
// Included in FullResourceReload after the common typed reflection helpers.
// UIModelLoader owns these instances independently of BaseModelViewPart.
struct LiveUIModel {Ref loader,root,parent,callback;std::string path;};
std::map<void*,LiveUIModel> liveUIModels;
std::map<std::pair<void*,int>,std::string> pendingUIModels;
using UILoadFn=void*(*)(void*,void*,void*,void*);
using UIAsyncFn=int(*)(void*,void*,void*,void*,void*);
using UICompleteFn=void(*)(void*,void*,void*);
using UIUnloadFn=void(*)(void*,void*,void*);
using UICancelFn=void(*)(void*,int,void*);
using UIHandleCallbackFn=void(*)(void*,void*,void*);
UILoadFn originalUILoad=nullptr;UIAsyncFn originalUIAsync=nullptr;
UICompleteFn originalUIComplete=nullptr;
UIUnloadFn originalUIUnload=nullptr;UICancelFn originalUICancel=nullptr;
PartFinishedFn originalUIDispose=nullptr;
UIHandleCallbackFn originalUIHandleCallback=nullptr;
std::atomic<bool> uiLifecycleReady{false};
std::map<std::pair<void*,int>,std::shared_ptr<UIModelRequest>> uiReloadRequests;
std::map<void*,std::weak_ptr<UIModelRequest>> uiProxyRequests;
#include "eiem_entry_ui_page.h"
Ref CandidateUIProxy(void* proxy,uint64_t hash){
 auto found=uiProxyRequests.find(proxy);if(found==uiProxyRequests.end())return {};
 auto requestState=found->second.lock();if(!requestState){uiProxyRequests.erase(found);return {};}
 if(requestState->cancelled||requestState->delivered)return {};
 Require(requestState->pathHash==hash,"UI request proxy path differs from its prepared root");
 Require(RuntimeShapes::Alive(requestState->candidatePrefab.Get()),"UI request lost its prepared prefab");
 return requestState->candidatePrefab;
}
struct UIRequestScope {
 void* loader;std::string path;bool completed=false;UIRequestScope* previous;
 std::shared_ptr<UIModelRequest> requestState;
 static thread_local UIRequestScope* current;
 UIRequestScope(void* value,std::string source):loader(value),path(std::move(source)),previous(current){current=this;}
 ~UIRequestScope(){current=previous;}
};
inline thread_local UIRequestScope* UIRequestScope::current=nullptr;
void UICancel(void* self,int request,void* info);
void UIHandleCallback(void* self,void* callback,void* info){
 originalUIHandleCallback(self,callback,info);
 if(!RuntimeShapes::Main()||!UIRequestScope::current)return;
 bool needed=false;for(auto scope=UIRequestScope::current;scope;scope=scope->previous)needed=needed||(scope->requestState&&!scope->requestState->completed);if(!needed)return;
 try{
  auto target=Ref::Make(Call(api.delegateTarget,callback),false);if(!target.Get())return;
  for(auto scope=UIRequestScope::current;scope;scope=scope->previous){auto requestState=scope->requestState;if(!requestState||requestState->completed)continue;
   if(ReadObject(target.Get(),"<>4__this","Beyond.UI.UIModelLoader")!=scope->loader||!SameAssetPath(scope->path,String(ReadObject(target.Get(),"modelPath","System.String")))||ReadObject(target.Get(),"callback",nullptr)!=requestState->callback.Get())continue;
   auto boxed=Ref::Make(il2cpp_value_box(api.handleClass,self),false);uint32_t alignment=0;auto size=il2cpp_class_value_size(api.handleUntrackedClass,&alignment);Require(size>0,"UI handle structure is unavailable");
   Bytes untracked(size);il2cpp_field_get_value(boxed.Get(),api.handleUntrackedField,untracked.data());
   requestState->instantiateProxy=Ref::Make(Call(api.untrackedAssetGet,untracked.data()),false);Require(requestState->instantiateProxy.Get(),"UI request has no game instantiation proxy");
   uiProxyRequests[requestState->instantiateProxy.Get()]=requestState;break;
  }
 }catch(const std::exception& e){Log("[FULL-RELOAD-UI-GAP] proxyAssociation=%s",e.what());}
}
std::shared_ptr<UIModelRequest> LoadUIModel(void* loader,const std::string& path,void* parent,void* callback,Ref candidate){
 auto requestState=std::make_shared<UIModelRequest>();requestState->loader=Ref::Make(loader,false);requestState->callback=Ref::Make(callback,false);requestState->path=path;
 requestState->candidatePrefab=std::move(candidate);auto hash=Path(path);il2cpp_field_get_value(hash.Get(),api.pathHashField,&requestState->pathHash);
 UIRequestScope scope(loader,path);scope.requestState=requestState;
 Log("[FULL-RELOAD-UI-REQUEST] path=%s callbackProvided=%d originalDisplayRetained=1",path.c_str(),int(callback!=nullptr));
 auto source=Ref::Make(il2cpp_string_new(path.c_str()),false);void* args[]={source.Get(),parent,callback};
 requestState->id=RuntimeShapes::Scalar<int>(api.uiAsyncLoad,loader,args);requestState->submitted=true;
 uiReloadRequests[{loader,requestState->id}]=requestState;pendingUIModels[{loader,requestState->id}]=path;
 return requestState;
}
bool UIOwns(void* loader,void* root){
 auto dictionary=Ref::Make(ReadObject(loader,"m_instance2AssetHandle",nullptr),false);if(!dictionary.Get())return false;
 void* args[]={root};return RuntimeShapes::Scalar<unsigned char>(api.uiDictionaryContains,dictionary.Get(),args)!=0;
}
void RecordUIModel(void* loader,void* root,const std::string& path,void* parent,void* callback){
 if(!uiLifecycleReady||!RuntimeShapes::Main()||!RuntimeShapes::Alive(root)||!UIOwns(loader,root))return;
 LiveUIModel live{Ref::Make(loader,true),Ref::Make(root,true),Ref::Make(parent,true),Ref::Make(callback,false),path};
 liveUIModels[root]=std::move(live);++ownerRevision;auto controlsReady=RuntimeShapes::RegisterModel(root);
 ProbeModelMaterials(root,"ui-complete");
 Log("[FULL-RELOAD-UI-READY] path=%s rootId=%d originalCallbackRecorded=%d parentProvided=%d hierarchyActive=%d controlsReady=%zu gameOwnedInstance=1",path.c_str(),Id(root),int(callback!=nullptr),int(parent!=nullptr),int(RuntimeShapes::Scalar<unsigned char>(api.uiActiveHierarchy,root)!=0),controlsReady);
}
void* UILoad(void* self,void* path,void* parent,void* info){
 auto root=originalUILoad(self,path,parent,info);
 try{RecordUIModel(self,root,String(path),parent,nullptr);}catch(const std::exception& e){Log("[FULL-RELOAD-UI-GAP] load=%s",e.what());}
 return root;
}
int UIAsync(void* self,void* path,void* parent,void* callback,void* info){
 if(!uiLifecycleReady||!RuntimeShapes::Main())return originalUIAsync(self,path,parent,callback,info);
 UIRequestScope scope(self,String(path));auto request=originalUIAsync(self,path,parent,callback,info);
 if(RuntimeShapes::Main()&&!scope.completed)pendingUIModels[{self,request}]=scope.path;
 return request;
}
void UIComplete(void* self,void* proxy,void* info){
 if(!uiLifecycleReady){originalUIComplete(self,proxy,info);return;}
 // Capture the loader's actual closure, not a generated class number.
 // Ordinary loads finish natively; staged F10 loads retain that completion.
 Ref loader,parent,callback;std::string path;int request=0;bool captured=false;
 try{if(RuntimeShapes::Main()){
  loader=Ref::Make(ReadObject(self,"<>4__this","Beyond.UI.UIModelLoader"),false);
  parent=Ref::Make(ReadObject(self,"parent","UnityEngine.Transform"),false);
  callback=Ref::Make(ReadObject(self,"callback",nullptr),false);path=String(ReadObject(self,"modelPath","System.String"));
  il2cpp_field_get_value(self,Field(il2cpp_object_get_class(self),"requestId","System.Int32"),&request);captured=true;
 }}catch(const std::exception& e){Log("[FULL-RELOAD-UI-GAP] completionCapture=%s",e.what());}
 if(captured){
  std::shared_ptr<UIModelRequest> requestState;
  auto found=uiReloadRequests.find({loader.Get(),request});if(found!=uiReloadRequests.end())requestState=found->second;
  if(!requestState)for(auto scope=UIRequestScope::current;scope;scope=scope->previous)
   if(scope->requestState&&scope->loader==loader.Get()&&SameAssetPath(scope->path,path)&&scope->requestState->callback.Get()==callback.Get()){requestState=scope->requestState;break;}
  if(requestState&&requestState->cancelled){
   originalUIComplete(self,proxy,info);requestState->completed=requestState->delivered=true;
   uiProxyRequests.erase(requestState->instantiateProxy.Get());
   pendingUIModels.erase({loader.Get(),request});uiReloadRequests.erase({loader.Get(),request});return;
  }
  if(requestState){
   // Unity's instantiate job can complete on a later frame. Retain its real
   // completion until every candidate is verified. F10 uses a fresh request
   // with no one-shot continuation; page binding happens separately at commit.
   requestState->id=request;requestState->closure=Ref::Make(self,false);requestState->completionInfo=info;
   try{
    requestState->proxy=Ref::Make(il2cpp_value_box(api.handleUntrackedClass,proxy),false);Require(requestState->proxy.Get(),"UI completion handle could not be retained");
    requestState->root=Ref::Make(Call(api.uiUntrackedGet,proxy),false);Require(RuntimeShapes::Alive(requestState->root.Get()),"UI async creation returned no model");
    // Material Init may have observed this provisional instance. Its page
    // completion has not run, so keep only the old display's control bindings.
    for(const auto& renderer:Components(requestState->root.Get()))RuntimeShapes::bound.erase(renderer.Get());
    RuntimeShapes::boundCount.store(unsigned(RuntimeShapes::bound.size()));
   }catch(const std::exception& e){requestState->error=e.what();}
   requestState->completed=true;
   Log("[FULL-RELOAD-UI-CANDIDATE] path=%s gameCompletionRetained=1 validInstance=%d",path.c_str(),int(requestState->error.empty()));
   for(auto scope=UIRequestScope::current;scope;scope=scope->previous)if(scope->loader==loader.Get()&&SameAssetPath(scope->path,path))scope->completed=true;
   return;
  }
 }
 originalUIComplete(self,proxy,info);
 if(!captured)return;
 pendingUIModels.erase({loader.Get(),request});uiReloadRequests.erase({loader.Get(),request});
 for(auto scope=UIRequestScope::current;scope;scope=scope->previous)if(scope->loader==loader.Get()&&SameAssetPath(scope->path,path))scope->completed=true;
 try{auto root=Ref::Make(Call(api.uiUntrackedGet,proxy),false);RecordUIModel(loader.Get(),root.Get(),path,parent.Get(),callback.Get());}
 catch(const std::exception& e){Log("[FULL-RELOAD-UI-GAP] completion=%s",e.what());}
}
void FinishUIModel(const Owner& owner){
 auto requestState=owner.uiRequest;Require(requestState&&requestState->completed&&!requestState->cancelled&&requestState->error.empty(),"UI reload completion unavailable");
 Require(!requestState->delivered,"UI reload completion delivered twice");requestState->delivered=true;
 originalUIComplete(requestState->closure.Get(),Self(requestState->proxy,api.handleUntrackedClass),requestState->completionInfo);
 uiProxyRequests.erase(requestState->instantiateProxy.Get());
 pendingUIModels.erase({requestState->loader.Get(),requestState->id});uiReloadRequests.erase({requestState->loader.Get(),requestState->id});
 Require(UIOwns(requestState->loader.Get(),requestState->root.Get()),"game UI loader did not accept the completed model");
 try{Require(BindUIPage(owner.uiPage,requestState->root.Get()),"game UI page did not accept the completed model");}
 catch(...){void* args[]={requestState->root.Get()};Call(api.uiUnload,requestState->loader.Get(),args);throw;}
 owner.uiLoadedRoot=requestState->root;
 RecordUIModel(requestState->loader.Get(),requestState->root.Get(),owner.path,owner.uiParent.Get(),nullptr);
 Log("[FULL-RELOAD-UI-PAGE] path=%s pageItemRebound=1 oldContinuationReplayed=0",owner.path.c_str());
}
void CancelUIModels(Attempt& a){
 for(auto& owner:a.owners){auto requestState=owner.uiRequest;if(!requestState||requestState->delivered)continue;
  if(!requestState->cancelled&&requestState->submitted)UICancel(requestState->loader.Get(),requestState->id,nullptr);
  requestState->cancelled=true;
  uiProxyRequests.erase(requestState->instantiateProxy.Get());
  // Cancel removes the game's request token. A retained completed callback
  // must still run its native cancellation branch to dispose its own handle.
  if(requestState->completed&&requestState->closure.Get()){requestState->delivered=true;originalUIComplete(requestState->closure.Get(),Self(requestState->proxy,api.handleUntrackedClass),requestState->completionInfo);uiReloadRequests.erase({requestState->loader.Get(),requestState->id});}
 }
}

void ForgetUIModel(void* root){
 if(!liveUIModels.count(root))return;
 if(RuntimeShapes::Alive(root)&&!RuntimeShapes::bound.empty())for(const auto& renderer:Components(root))RuntimeShapes::bound.erase(renderer.Get());
 RuntimeShapes::boundCount.store(unsigned(RuntimeShapes::bound.size()));liveUIModels.erase(root);++ownerRevision;
}
void UIUnload(void* self,void* root,void* info){
 try{if(RuntimeShapes::Main())ForgetUIModel(root);}catch(const std::exception& e){Log("[FULL-RELOAD-UI-GAP] unload=%s",e.what());}
 originalUIUnload(self,root,info);
}
void UICancel(void* self,int request,void* info){
 originalUICancel(self,request,info);if(!RuntimeShapes::Main())return;
 pendingUIModels.erase({self,request});auto requestState=uiReloadRequests.find({self,request});if(requestState!=uiReloadRequests.end())requestState->second->cancelled=true;
}
void UIDispose(void* self,void* info){
 if(RuntimeShapes::Main()){
  for(const auto& requestState:uiReloadRequests)if(requestState.first.first==self)requestState.second->cancelled=true;
  std::vector<void*> roots;for(const auto& live:liveUIModels)if(live.second.loader.Get()==self)roots.push_back(live.first);
  for(auto root:roots)try{ForgetUIModel(root);}catch(const std::exception& e){Log("[FULL-RELOAD-UI-GAP] dispose=%s",e.what());}
  for(auto it=pendingUIModels.begin();it!=pendingUIModels.end();)if(it->first.first==self)it=pendingUIModels.erase(it);else ++it;
 }
 originalUIDispose(self,info);
}
void InstallUIModels(void** assemblies,size_t count){
 try{
  auto loader=FindClass("Beyond.UI","UIModelLoader",assemblies,count);Require(loader,"UI model loader absent");
  auto dictionaryClass=il2cpp_class_from_type(il2cpp_field_get_type(Field(loader,"m_instance2AssetHandle",nullptr)));
  api.uiDictionaryContains=Method(dictionaryClass,"ContainsKey",{"UnityEngine.GameObject"},"System.Boolean",false);
  api.uiLoad=Method(loader,"LoadModel",{"System.String","UnityEngine.Transform"},"UnityEngine.GameObject",false);
  api.uiUnload=Method(loader,"UnloadModel",{"UnityEngine.GameObject"},"System.Void",false);
  api.uiUntrackedGet=Method(api.handleUntrackedClass,"Get",{},"UnityEngine.Object",false);
  api.uiActiveHierarchy=Method(FindClass("UnityEngine","GameObject",assemblies,count),"get_activeInHierarchy",{},"System.Boolean",false);
  void* closure=nullptr;std::string callbackType;void* iterator=nullptr;
  Require(il2cpp_class_get_nested_types,"UI loader nested metadata unavailable");
  while(auto nested=il2cpp_class_get_nested_types(loader,&iterator)){
   void* invoke=nullptr;std::string actualCallback;
   try{
    Field(nested,"<>4__this","Beyond.UI.UIModelLoader");Field(nested,"modelPath","System.String");Field(nested,"parent","UnityEngine.Transform");Field(nested,"requestId","System.Int32");
    auto type=il2cpp_field_get_type(Field(nested,"callback",nullptr));auto klass=il2cpp_class_from_type(type);
    Require(InheritsClass(klass,"System","Delegate"),"UI callback is not a delegate");
    invoke=Method(klass,"Invoke",{"UnityEngine.GameObject"},"System.Void",false);
    Method(nested,"<LoadModelAsync>b__0",{"Beyond.Resource.FAssetProxyUntrackedHandle"},"System.Void",false);
    auto name=il2cpp_type_get_name(type);Require(name,"UI callback type absent");actualCallback=name;il2cpp_free((void*)name);
   }catch(const std::exception&){continue;}
   Require(!closure,"UI completion closure is ambiguous");closure=nested;api.uiCallbackInvoke=invoke;callbackType=std::move(actualCallback);
  }
  Require(closure,"UI completion closure structure absent");
  api.uiAsyncLoad=Method(loader,"LoadModelAsync",{"System.String","UnityEngine.Transform",callbackType.c_str()},"System.Int32",false);
  std::string handleCallbackType;void* methodIterator=nullptr;
  while(auto method=il2cpp_class_get_methods(api.handleClass,&methodIterator)){
   auto name=il2cpp_method_get_name(method);if(!name||strcmp(name,"AddOnProxyCompleted")||il2cpp_method_get_param_count(method)!=1)continue;
   auto type=il2cpp_method_get_param(method,0),klass=il2cpp_class_from_type(type);
   Require(InheritsClass(klass,"System","Delegate"),"UI proxy completion parameter is not a delegate");
   Method(klass,"Invoke",{"Beyond.Resource.FAssetProxyUntrackedHandle"},"System.Void",false);
   auto text=il2cpp_type_get_name(type);Require(text,"UI proxy callback type absent");handleCallbackType=text;il2cpp_free((void*)text);
  }
  Require(!handleCallbackType.empty(),"UI proxy completion registration absent");
  Require(Entry::Install(loader,"LoadModel",{"System.String","UnityEngine.Transform"},"UnityEngine.GameObject",false,(void*)UILoad,(void**)&originalUILoad),"UI sync lifecycle hook unavailable");
  Require(Entry::Install(loader,"LoadModelAsync",{"System.String","UnityEngine.Transform",callbackType.c_str()},"System.Int32",false,(void*)UIAsync,(void**)&originalUIAsync),"UI async lifecycle hook unavailable");
  Require(Entry::Install(closure,"<LoadModelAsync>b__0",{"Beyond.Resource.FAssetProxyUntrackedHandle"},"System.Void",false,(void*)UIComplete,(void**)&originalUIComplete),"UI completion hook unavailable");
  Require(Entry::Install(loader,"UnloadModel",{"UnityEngine.GameObject"},"System.Void",false,(void*)UIUnload,(void**)&originalUIUnload),"UI unload hook unavailable");
  Require(Entry::Install(loader,"Cancel",{"System.Int32"},"System.Void",false,(void*)UICancel,(void**)&originalUICancel),"UI cancel hook unavailable");
  Require(Entry::Install(loader,"Dispose",{},"System.Void",false,(void*)UIDispose,(void**)&originalUIDispose),"UI dispose hook unavailable");
  Require(Entry::Install(api.handleClass,"AddOnProxyCompleted",{handleCallbackType.c_str()},"System.Void",false,(void*)UIHandleCallback,(void**)&originalUIHandleCallback),"UI request proxy observation unavailable");
  uiLifecycleReady=true;
  Log("[FULL-RELOAD-UI-LIFECYCLE] sync=1 async=1 completion=1 unload=1 callbackStructureVerified=1");
 }catch(const std::exception& e){Log("[FULL-RELOAD-UI-GAP] binding=%s",e.what());}
 // Native instance registration and key controls do not require the Lua page
 // transfer adapter. Missing page APIs reject matching F10 owners explicitly.
 try{
  BindUIPageApi(FindClass("Beyond.Lua","LuaManager",assemblies,count),FindClass("XLua","LuaFunction",assemblies,count),
   FindClass("System","Array",assemblies,count),FindClass("System","Object",assemblies,count),FindClass("System","Exception",assemblies,count));
  BindUIPageCodec(FindClass("Beyond.Lua","LuaCypher",assemblies,count),FindClass("Security","XXTEA",assemblies,count),FindClass("System","String",assemblies,count));
  Log("[FULL-RELOAD-UI-PAGE-API] managerInstance=1 doStringInstanceMethod=1 gameCodec=1 exactSignaturesVerified=1");
 }catch(const std::exception& e){Log("[FULL-RELOAD-UI-GAP] pageBinding=%s nativeLifecycleIndependent=1",e.what());}
}
