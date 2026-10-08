// Temporary read-only diagnostics for actual performance-owned attachments.
// Included inside FullResourceReload after the skin-state readers.
void* ProbeZeroMethod(void* klass,const char* name,bool required=true){
 for(;klass;klass=il2cpp_class_get_parent(klass)){void* it=nullptr;void* found=nullptr;
  while(auto method=il2cpp_class_get_methods(klass,&it)){auto label=il2cpp_method_get_name(method);uint32_t impl=0;
   if(label&&!strcmp(label,name)&&!il2cpp_method_get_param_count(method)&&!(il2cpp_method_get_flags(method,&impl)&0x10)&&((MInfo*)method)->mp){
    Require(!found,"ambiguous diagnostic collection method: "+std::string(name));found=method;
   }
  }
  if(found)return found;
 }
 Require(!required,"diagnostic collection method absent: "+std::string(name));return nullptr;
}
void* ProbeReferenceField(void* klass,const char* name){
 auto field=Field(klass,name,nullptr),type=il2cpp_field_get_type(field);auto code=il2cpp_type_get_type(type);
 Require((code==0xe||code==0x12||code==0x14||code==0x15||code==0x1c||code==0x1d)&&!il2cpp_type_is_byref(type)&&!InheritsClass(il2cpp_class_from_type(type),"System","ValueType"),
         "diagnostic field is not an object reference: "+std::string(name));return field;
}
std::vector<Ref> ProbeReferences(void* collection){
 if(!collection)return {};
 auto klass=il2cpp_object_get_class(collection);auto code=il2cpp_type_get_type(il2cpp_class_get_type(klass));
 if(code==0x14||code==0x1d){std::vector<Ref> values;for(auto value:ReferenceArray(collection))if(value)values.push_back(Ref::Make(value,false));return values;}
 auto valuesMethod=ProbeZeroMethod(klass,"get_Values",false);auto values=Ref::Make(valuesMethod?Call(valuesMethod,collection):collection,false);
 Require(values.Get(),"diagnostic collection values absent");klass=il2cpp_object_get_class(values.Get());
 auto countMethod=Method(klass,"get_Count",{},"System.Int32",false);auto count=RuntimeShapes::Scalar<int>(countMethod,values.Get());Require(count>=0,"diagnostic collection count negative");
 auto enumerator=Ref::Make(Call(ProbeZeroMethod(klass,"GetEnumerator"),values.Get()),false);Require(enumerator.Get(),"diagnostic enumerator absent");
 auto enumeratorClass=il2cpp_object_get_class(enumerator.Get());auto self=InheritsClass(enumeratorClass,"System","ValueType")?il2cpp_object_unbox(enumerator.Get()):enumerator.Get();
 auto next=Method(enumeratorClass,"MoveNext",{},"System.Boolean",false),current=ProbeZeroMethod(enumeratorClass,"get_Current"),dispose=ProbeZeroMethod(enumeratorClass,"Dispose",false);
 std::vector<Ref> result;
 try{while(RuntimeShapes::Scalar<unsigned char>(next,self)){
   Require(result.size()<size_t(count),"diagnostic enumeration exceeds actual collection count");result.push_back(Ref::Make(Call(current,self),false));
  }
  Require(result.size()==size_t(count),"diagnostic collection changed during enumeration");
 }catch(...){if(dispose)Call(dispose,self);throw;}
 if(dispose)Call(dispose,self);return result;
}
struct ProbeRequest {Ref handle;uint32_t source=0;};
ProbeRequest ProbeCurrentRequest(void* animation){
 if(!Inherits(animation,"Beyond.Gameplay.View","CharacterAnimationComponent"))return {};
 Bytes bytes(performProbe.requestSize);il2cpp_field_get_value(animation,performProbe.requestField,bytes.data());
 auto boxed=Ref::Make(il2cpp_value_box(performProbe.requestClass,bytes.data()),false);void* handle=nullptr;
 il2cpp_field_get_value(boxed.Get(),performProbe.handleField,&handle);ProbeRequest result;result.handle=Ref::Make(handle,false);
 if(performProbe.requestSourceField)il2cpp_field_get_value(boxed.Get(),performProbe.requestSourceField,&result.source);return result;
}
Ref ProbeObject(void* object,void* field){void* value=nullptr;if(object)il2cpp_field_get_value(object,field,&value);return Ref::Make(value,false);}
std::vector<Ref> ProbePerformHandles(const Owner& owner){
 std::vector<Ref> handles;std::set<void*> seen;
 auto add=[&](Ref handle){if(handle.Get()&&seen.insert(handle.Get()).second){Require(Inherits(handle.Get(),"Beyond.Gameplay.Core.CharInteractPerform","CharPerformHandleBase"),"diagnostic performance handle type differs");
   if(Call(performProbe.handleEntity,handle.Get())==owner.entity.Get())handles.push_back(std::move(handle));}};
 add(ProbeCurrentRequest(owner.animation.Get()).handle);add(ProbeObject(owner.performManager.Get(),performProbe.mainHandleField));
 for(size_t i=0;i<handles.size();++i)if(Inherits(handles[i].Get(),"Beyond.Gameplay.Core.CharInteractPerform","CharPerformMainHandle")){
  auto children=ProbeObject(handles[i].Get(),performProbe.subHandlesField);for(auto child:ProbeReferences(children.Get()))add(std::move(child));
 }
 return handles;
}
int ProbeUnityId(void* object){return RuntimeShapes::Alive(object)?Id(object):0;}
bool ProbeInside(void* node,void* root){if(!RuntimeShapes::Alive(node)||!RuntimeShapes::Alive(root))return false;
 auto transform=Ref::Make(Call(api.goTransform,root),false);void* args[]={transform.Get()};return node==transform.Get()||RuntimeShapes::Scalar<unsigned char>(api.isChild,node,args)!=0;
}
void ProbeRememberResource(PerformProbe::Watch& watch,void* asset,const std::string& path){
 if(!RuntimeShapes::Alive(asset))return;auto id=Id(asset);
 if(std::none_of(watch.resources.begin(),watch.resources.end(),[&](const auto& old){return old.asset.Get()==asset&&old.id==id;}))
  watch.resources.push_back({Ref::Make(asset,true),path,String(Call(RuntimeShapes::api.name,asset)),id});
}
std::string ProbePerformState(PerformProbe::Watch& watch,const char* phase,bool detail){
 auto& owner=watch.owner;auto root=Call(api.getModel,owner.component.Get());auto current=ProbePerformHandles(owner);std::set<void*> seen;
 for(const auto& handle:current)if(std::none_of(watch.handles.begin(),watch.handles.end(),[&](const Ref& old){return old.Get()==handle.Get();}))watch.handles.push_back(Ref::Make(handle.Get(),true));
 auto request=ProbeCurrentRequest(owner.animation.Get());std::ostringstream state;state<<"model="<<ProbeUnityId(root)<<" requestHandle="<<request.handle.Get()<<" requestSource="<<request.source<<" currentHandles="<<current.size();
 for(const auto& handle:watch.handles)current.push_back(handle);
 for(const auto& handle:current){if(!handle.Get()||!seen.insert(handle.Get()).second)continue;
  auto valid=RuntimeShapes::Scalar<unsigned char>(performProbe.handleValid,handle.Get());auto playing=valid?RuntimeShapes::Scalar<unsigned char>(performProbe.handlePlaying,handle.Get()):0;
  auto label=valid?Ref::Make(Call(performProbe.handlePath,handle.Get()),false):Ref{};state<<" handle="<<handle.Get()<<":"<<(valid?String(label.Get()):"<released>")<<":"<<int(valid)<<":"<<int(playing);
  auto objects=ProbeObject(handle.Get(),performProbe.objectsField);
  for(auto object:ProbeReferences(objects.Get())){Require(Inherits(object.Get(),"Beyond.Gameplay.Core.CharInteractPerform","TmpObjectInst"),"diagnostic temporary object type differs");
   if(std::none_of(watch.objects.begin(),watch.objects.end(),[&](const Ref& old){return old.Get()==object.Get();}))watch.objects.push_back(Ref::Make(object.Get(),true));
  }
 }
 for(const auto& object:watch.objects){if(!object.Get())continue;auto path=ProbeObject(object.Get(),performProbe.pathField),go=ProbeObject(object.Get(),performProbe.goField),follow=ProbeObject(object.Get(),performProbe.followField);
  bool alive=RuntimeShapes::Alive(go.Get()),followAlive=RuntimeShapes::Alive(follow.Get());
  auto animator=alive?Ref::Make(Call(performProbe.objectAnimator,object.Get()),false):Ref{};
  state<<" object="<<object.Get()<<":"<<String(path.Get())<<" go="<<ProbeUnityId(go.Get())<<" active="<<(alive?int(RuntimeShapes::Scalar<unsigned char>(skinStateProbe.active,go.Get())):-1)
       <<" final="<<int(RuntimeShapes::Scalar<unsigned char>(performProbe.objectActive,object.Get()))<<" follow="<<ProbeUnityId(follow.Get())<<" followAlive="<<int(followAlive)
       <<" followCurrent="<<int(ProbeInside(follow.Get(),root))<<" followOld="<<int(ProbeInside(follow.Get(),owner.root.Get()))<<" animator="<<ProbeUnityId(animator.Get());
  if(RuntimeShapes::Alive(animator.Get())){
   if(performProbe.animatorEnabled)state<<" animatorEnabled="<<int(RuntimeShapes::Scalar<unsigned char>(performProbe.animatorEnabled,animator.Get()));
   if(performProbe.animatorController){auto controller=Ref::Make(Call(performProbe.animatorController,animator.Get()),false);state<<" controller="<<ProbeUnityId(controller.Get());ProbeRememberResource(watch,controller.Get(),String(path.Get()));}
  }
  if(alive){auto renderers=ProbeObject(object.Get(),performProbe.renderersField);auto list=ProbeReferences(renderers.Get());std::vector<Ref> skins;
   for(const auto& renderer:list)if(RuntimeShapes::Alive(renderer.Get())){ProbeRememberResource(watch,RuntimeShapes::SourceMesh(renderer.Get()),String(path.Get()));if(detail&&RuntimeShapes::RendererKind(renderer.Get())==137)skins.push_back(renderer);}
   if(detail)TraceSkinStates(go.Get(),String(path.Get()),skins,phase,GetTickCount64()-watch.started);
  }
 }
 for(const auto& resource:watch.resources)if(resource.asset.Get()&&!RuntimeShapes::Alive(resource.asset.Get()))state<<" lostResource="<<resource.id<<":"<<resource.name;
 return state.str();
}
void TracePerformance(const Owner& owner,const char* phase,bool begin=false){
 if(!performProbe.ready||owner.viewPart||!owner.animation.Get())return;
 try{auto& watch=performProbe.watches[owner.component.Get()];
  if(begin){watch={};watch.owner=owner;watch.owner.entity=Ref::Make(owner.entity.Get(),true);watch.owner.component=Ref::Make(owner.component.Get(),true);
   watch.owner.root=Ref::Make(owner.root.Get(),true);watch.owner.animation=Ref::Make(owner.animation.Get(),true);watch.owner.performManager=Ref::Make(owner.performManager.Get(),true);
   watch.owner.animationData={};watch.owner.weapons={};watch.owner.audio={};watch.owner.config={};watch.owner.pathHash={};watch.owner.uiParent={};watch.owner.uiCallback={};watch.owner.uiPage={};watch.owner.uiLoadedRoot={};watch.owner.uiRequest.reset();watch.started=GetTickCount64();
   for(auto handle:ProbePerformHandles(owner))watch.handles.push_back(Ref::Make(handle.Get(),true));
  }
  watch.last=GetTickCount64();watch.signature=ProbePerformState(watch,phase,true);
  Log("[DEBUG-perform-state] phase=%s elapsedMs=%llu entity=%p path=%s %s",phase,(unsigned long long)(watch.last-watch.started),owner.entity.Get(),owner.path.c_str(),watch.signature.c_str());
 }catch(const std::exception& e){Log("[DEBUG-perform-state-GAP] phase=%s reason=%s reloadUnaffected=1",phase,e.what());}
}
void TracePerformanceFrame(){
 if(!performProbe.ready)return;auto now=GetTickCount64();
 for(auto it=performProbe.watches.begin();it!=performProbe.watches.end();){auto& watch=it->second;
  if(!watch.owner.entity.Get()||!watch.owner.component.Get()||!watch.owner.animation.Get()||now-watch.started>=180000){Log("[DEBUG-perform-state-END] path=%s elapsedMs=%llu",watch.owner.path.c_str(),(unsigned long long)(now-watch.started));it=performProbe.watches.erase(it);continue;}
  if(now-watch.last>=1000){watch.last=now;
   try{constexpr ULONGLONG times[]={1000,5000,15000,30000,60000,90000,150000};bool detail=watch.sample<std::size(times)&&now-watch.started>=times[watch.sample];if(detail)++watch.sample;
    auto state=ProbePerformState(watch,"delayed",detail);if(detail||state!=watch.signature){watch.signature=state;Log("[DEBUG-perform-state] phase=delayed elapsedMs=%llu entity=%p path=%s %s",(unsigned long long)(now-watch.started),watch.owner.entity.Get(),watch.owner.path.c_str(),state.c_str());}
   }catch(const std::exception& e){Log("[DEBUG-perform-state-GAP] phase=delayed reason=%s reloadUnaffected=1",e.what());it=performProbe.watches.erase(it);continue;}
  }
  ++it;
 }
}
using ProbeUnloadFn=void(*)(void*,bool);
using ProbeUnloadAsyncFn=void*(*)(void*,bool);
using ProbeUnloadAssetFn=void(*)(void*,void*);
using ProbeUnloadAssetAsyncFn=void*(*)(void*,void*);
ProbeUnloadFn originalProbeUnload=nullptr;
ProbeUnloadAsyncFn originalProbeUnloadAsync=nullptr;
ProbeUnloadAssetFn originalProbeUnloadAsset=nullptr;
ProbeUnloadAssetAsyncFn originalProbeUnloadAssetAsync=nullptr;
struct ProbePendingRelease {Ref request;std::vector<PerformProbe::Resource> resources;std::string operation,stack,bundleName;void* bundle=nullptr;int bundleId=0;bool all=false;};
std::vector<ProbePendingRelease> probePendingReleases;
struct ProbeResourceRelease {
 const char* operation;void* bundle;bool all=false;std::vector<PerformProbe::Resource> before;
 std::string stack,bundleName;int bundleId=0;
 ProbeResourceRelease(const char* label,void* self,bool destroy,void* exactAsset=nullptr):operation(label),bundle(self),all(destroy){
  try{if(!performProbe.ready||!RuntimeShapes::Main()||performProbe.watches.empty())return;
   std::set<int> seen;for(const auto& entry:performProbe.watches)for(const auto& resource:entry.second.resources)
    if((!exactAsset||resource.asset.Get()==exactAsset)&&RuntimeShapes::Alive(resource.asset.Get())&&seen.insert(resource.id).second)before.push_back(resource);
   if(before.empty())return;
   if(RuntimeShapes::Alive(bundle)){bundleId=Id(bundle);bundleName=String(Call(RuntimeShapes::api.name,bundle));}
   void* frames[24]{};auto count=CaptureStackBackTrace(1,WORD(std::size(frames)),frames,nullptr);std::ostringstream trace;
   for(USHORT i=0;i<count;++i){HMODULE module=nullptr;auto address=uintptr_t(frames[i]);
    if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)frames[i],&module)&&module==hGA)trace<<"GA+0x"<<std::hex<<(address-uintptr_t(module))<<",";
   }
   stack=trace.str();
  }catch(const std::exception& e){before.clear();Log("[DEBUG-perform-resource-GAP] phase=before operation=%s reason=%s originalUnaffected=1",operation,e.what());}
 }
 void Report()const{
  for(const auto& resource:before)if(resource.asset.Get()&&!RuntimeShapes::Alive(resource.asset.Get()))
   Log("[DEBUG-perform-resource-DESTROYED] operation=%s bundle=%p bundleId=%d bundleName=%s all=%d resourceId=%d name=%s path=%s stack=%s originalUnaffected=1",operation,bundle,bundleId,bundleName.c_str(),int(all),resource.id,resource.name.c_str(),resource.path.c_str(),stack.c_str());
 }
 void Await(void* request){
  try{if(request&&!before.empty())probePendingReleases.push_back({Ref::Make(request,true),before,operation,stack,bundleName,bundle,bundleId,all});}
  catch(const std::exception& e){Log("[DEBUG-perform-resource-GAP] phase=async operation=%s reason=%s originalUnaffected=1",operation,e.what());}
 }
 ~ProbeResourceRelease()noexcept{
  try{Report();
  }catch(const std::exception& e){Log("[DEBUG-perform-resource-GAP] phase=after operation=%s reason=%s originalUnaffected=1",operation,e.what());}catch(...){}
 }
};
void ProbeUnload(void* self,bool all){ProbeResourceRelease observation("bundle-unload",self,all);originalProbeUnload(self,all);}
void* ProbeUnloadAsync(void* self,bool all){ProbeResourceRelease observation("bundle-unload-async",self,all);auto result=originalProbeUnloadAsync(self,all);observation.Await(result);return result;}
void ProbeUnloadAsset(void* self,void* asset){ProbeResourceRelease observation("asset-unload",self,false);originalProbeUnloadAsset(self,asset);}
void* ProbeUnloadAssetAsync(void* self,void* asset){ProbeResourceRelease observation("asset-unload-async",self,false);auto result=originalProbeUnloadAssetAsync(self,asset);observation.Await(result);return result;}
void TracePerformanceReleases(){
 if(performProbe.watches.empty()){probePendingReleases.clear();return;}
 for(auto it=probePendingReleases.begin();it!=probePendingReleases.end();){
  try{if(!it->request.Get()){it=probePendingReleases.erase(it);continue;}
   if(!RuntimeShapes::Scalar<unsigned char>(NativeBundle::api.done,it->request.Get())){++it;continue;}
   // Async destruction happened after submission; report the completed native
   // request as a correlation, not a synchronous proof of its side effects.
   for(const auto& resource:it->resources)if(resource.asset.Get()&&!RuntimeShapes::Alive(resource.asset.Get()))
    Log("[DEBUG-perform-resource-ASYNC-COMPLETE] operation=%s bundle=%p bundleId=%d bundleName=%s all=%d resourceId=%d name=%s path=%s stack=%s correlationOnly=1",it->operation.c_str(),it->bundle,it->bundleId,it->bundleName.c_str(),int(it->all),resource.id,resource.name.c_str(),resource.path.c_str(),it->stack.c_str());
  }catch(const std::exception& e){Log("[DEBUG-perform-resource-GAP] phase=async-completion reason=%s originalUnaffected=1",e.what());}
  it=probePendingReleases.erase(it);
 }
}
void BindPerformanceProbe(void** assemblies,size_t count){
 try{
  auto character=FindClass("Beyond.Gameplay.View","CharacterAnimationComponent",assemblies,count);
  performProbe.requestField=Field(character,"m_currentRequest",nullptr);performProbe.requestClass=il2cpp_class_from_type(il2cpp_field_get_type(performProbe.requestField));
  Require(InheritsClass(performProbe.requestClass,"System","ValueType"),"diagnostic performance request is not a value type");uint32_t alignment=0;
  performProbe.requestSize=il2cpp_class_value_size(performProbe.requestClass,&alignment);Require(performProbe.requestSize>0,"diagnostic performance request has no extent");
  performProbe.handleField=ProbeReferenceField(performProbe.requestClass,"handle");
  performProbe.requestSourceField=Field(performProbe.requestClass,"source",nullptr);auto sourceClass=il2cpp_class_from_type(il2cpp_field_get_type(performProbe.requestSourceField));
  Require(InheritsClass(sourceClass,"System","ValueType")&&il2cpp_class_value_size(sourceClass,&alignment)==sizeof(uint32_t),"diagnostic performance source is not a four-byte value");
  auto handle=FindClass("Beyond.Gameplay.Core.CharInteractPerform","CharPerformHandleBase",assemblies,count),main=FindClass("Beyond.Gameplay.Core.CharInteractPerform","CharPerformMainHandle",assemblies,count);
  auto manager=FindClass("Beyond.Gameplay.Core.CharInteractPerform","CharInteractPerformManager",assemblies,count),object=FindClass("Beyond.Gameplay.Core.CharInteractPerform","TmpObjectInst",assemblies,count);
  performProbe.mainHandleField=ProbeReferenceField(manager,"m_mainCharHandle");performProbe.subHandlesField=ProbeReferenceField(main,"m_subHandles");performProbe.objectsField=ProbeReferenceField(handle,"m_tmpObjects");
  performProbe.handleEntity=Method(handle,"get_entity",{},"Beyond.Gameplay.Core.Entity",false);performProbe.handleValid=Method(handle,"get_isValid",{},"System.Boolean",false);performProbe.handlePlaying=Method(handle,"get_isPlaying",{},"System.Boolean",false);
  performProbe.handlePath=Method(handle,"get_interactPerformId",{},"System.String",false);
  performProbe.pathField=Field(object,"path","System.String");performProbe.goField=Field(object,"go","UnityEngine.GameObject");performProbe.followField=Field(object,"m_followTarget","UnityEngine.Transform");performProbe.renderersField=ProbeReferenceField(object,"m_renderers");
  performProbe.objectAnimator=Method(object,"get_animator",{},"UnityEngine.Animator",false);performProbe.objectActive=Method(object,"get_finalActive",{},"System.Boolean",false);
  performProbe.animatorEnabled=Exact(FindClass("UnityEngine","Behaviour",assemblies,count),"get_enabled",{},"System.Boolean",false);
  performProbe.animatorController=Exact(FindClass("UnityEngine","Animator",assemblies,count),"get_runtimeAnimatorController",{},"UnityEngine.RuntimeAnimatorController",false);
  Require(skinStateProbe.ready,"diagnostic attachment skin getters unavailable");performProbe.ready=true;Log("[DEBUG-perform-state-API] ready=1 actualHandleAttachments=1 fieldsWritten=0 sceneScan=0");
  auto bundle=FindClass("UnityEngine","AssetBundle",assemblies,count);
  bool unload=Exact(bundle,"Unload",{"System.Boolean"},"System.Void",false)&&LoadingObservation::Hook("UnityEngine.AssetBundle::Unload(System.Boolean)",(void*)ProbeUnload,(void**)&originalProbeUnload);
  bool async=Exact(bundle,"UnloadAsync",{"System.Boolean"},"UnityEngine.AsyncOperation",false)&&LoadingObservation::Hook("UnityEngine.AssetBundle::UnloadAsync(System.Boolean)",(void*)ProbeUnloadAsync,(void**)&originalProbeUnloadAsync);
  bool asset=Exact(bundle,"UnloadAsset_Internal",{"UnityEngine.Object"},"System.Void",false)&&LoadingObservation::Hook("UnityEngine.AssetBundle::UnloadAsset_Internal(UnityEngine.Object)",(void*)ProbeUnloadAsset,(void**)&originalProbeUnloadAsset);
  bool assetAsync=Exact(bundle,"UnloadAssetAsync_Internal",{"UnityEngine.Object"},"UnityEngine.AsyncOperation",false)&&LoadingObservation::Hook("UnityEngine.AssetBundle::UnloadAssetAsync_Internal(UnityEngine.Object)",(void*)ProbeUnloadAssetAsync,(void**)&originalProbeUnloadAssetAsync);
  Log("[DEBUG-perform-resource-API] unload=%d async=%d asset=%d assetAsync=%d originalArgumentsPreserved=1 weakAttachmentResourcesOnly=1",int(unload),int(async),int(asset),int(assetAsync));
 }catch(const std::exception& e){Log("[DEBUG-perform-state-GAP] phase=bind reason=%s reloadUnaffected=1",e.what());}
}
