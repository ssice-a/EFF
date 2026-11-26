#pragma once
// Included in Entry after RuntimeShapes and native owner helpers.
namespace RuntimeMaterials {
using RuntimeShapes::Ref;using RuntimeShapes::Call;using RuntimeShapes::Scalar;
using Components=std::array<float,4>;
struct Api {void *blockClass=nullptr,*ctor=nullptr,*copy=nullptr,*getSlot=nullptr,*getGlobal=nullptr,*empty=nullptr,*propertyId=nullptr,*has=nullptr,*materials=nullptr,*setFloat=nullptr,*setVector=nullptr,*setColor=nullptr,*getVector=nullptr;
 void *clone=nullptr,*sharedMaterial=nullptr,*setSharedMaterial=nullptr,*materialFloat=nullptr,*materialVector=nullptr,*materialColor=nullptr,*getMaterialVector=nullptr;
 void *getMaterialColor=nullptr,*hasBlockVector=nullptr;
 bool(*emptyIcall)(void*)=nullptr;
 bool(*hasBlockVectorIcall)(void*,int)=nullptr;} api;
using SlotSetter=void(*)(void*,void*,int);using GlobalSetter=void(*)(void*,void*);
SlotSetter originalSlot=nullptr;GlobalSetter originalGlobal=nullptr;
SlotSetter originalGetSlot=nullptr;thread_local unsigned internalRead=0;
struct Property {std::string kind,name;int id=0;std::map<unsigned,std::string> channels;};
struct Bound {Ref renderer,mesh,baseline,work,sourceMaterial,animatedMaterial;std::string package,marker;std::vector<Property> properties;std::map<std::string,double> lastValues;std::vector<std::shared_ptr<EiemAnimationSamples::Owner>> sampleOwners;bool installed=false,baseEmpty=true;ULONGLONG observeAt=0;bool observed=false;};
std::map<void*,Bound> bound;bool enabled=false;uint64_t revision=0;
std::atomic<DWORD> unityThread{0};std::atomic<bool> hasBindings{false};
bool Channel(const std::string& package,const std::string& variable,double& value){
 auto channels=RuntimeControls::continuousChannels.find(package);bool continuous=channels!=RuntimeControls::continuousChannels.end()&&channels->second.count(variable);
 auto player=RuntimeControls::animations.find(package);if(player==RuntimeControls::animations.end())return false;
 if(!continuous&&!player->second.output.count(variable))return false;value=RuntimeControls::EffectiveValues(package).at(variable);return true;
}
Ref Block(){auto b=Ref::Make(il2cpp_object_new(api.blockClass),false);Require(b.Get(),"material property block allocation failed");Call(api.ctor,b.Get());return b;}
void Read(void* method,void* renderer,void* block){struct Scope{Scope(){++internalRead;}~Scope(){--internalRead;}}scope;int slot=0;void* args[]={block,&slot};Call(method,renderer,args);}
void ReleaseMaterial(Bound& b){auto instance=b.animatedMaterial.Get();if(!instance)return;
 if(RuntimeShapes::Alive(b.renderer.Get())&&Call(api.sharedMaterial,b.renderer.Get())==instance){void* args[]={b.sourceMaterial.Get()};Call(api.setSharedMaterial,b.renderer.Get(),args);}
 if(RuntimeShapes::Alive(instance)){void* args[]={instance};Call(RuntimeShapes::api.destroy,nullptr,args);}b.animatedMaterial={};b.sourceMaterial={};}
void Material(Bound& b){auto current=Call(api.sharedMaterial,b.renderer.Get());if(current==b.animatedMaterial.Get()&&current)return;
 ReleaseMaterial(b);Require(RuntimeShapes::Alive(current),"animated renderer Material missing");void* args[]={current};auto clone=Ref::Make(Call(api.clone,nullptr,args),false);
 Require(RuntimeShapes::Alive(clone.Get())&&clone.Get()!=current,"animated Material clone failed");b.sourceMaterial=Ref::Make(current,false);b.animatedMaterial=clone;void* assign[]={clone.Get()};Call(api.setSharedMaterial,b.renderer.Get(),assign);
}
void Restore(Bound& b){if(b.installed&&RuntimeShapes::Alive(b.renderer.Get()))originalSlot(b.renderer.Get(),b.baseEmpty?nullptr:b.baseline.Get(),0);b.installed=false;ReleaseMaterial(b);}
bool HasBlockComponents(void* block,int id,const Components& values){
 void* key[]={&id};
 if(api.hasBlockVector)return Scalar<unsigned char>(api.hasBlockVector,block,key)!=0;
 if(api.hasBlockVectorIcall)return api.hasBlockVectorIcall(block,id);
 // Some player builds strip both presence APIs. GetVector returns zero for
 // absent fields, so only nonzero vectors can be distinguished from absence.
 return std::any_of(values.begin(),values.end(),[](float value){return value!=0;});
}
bool Values(Bound& b){auto player=RuntimeControls::animations.find(b.package);if(player==RuntimeControls::animations.end())return false;bool any=false;
 for(const auto& property:b.properties){
  Components values{};unsigned changed=0;
  for(const auto& c:property.channels){
    double value=0;if(!Channel(b.package,c.second,value))continue;
    Require(std::isfinite(value)&&std::abs(value)<=FLT_MAX,"material property exceeds float range");
    values[c.first]=float(value);changed|=1u<<c.first;
  }
  if(!changed)continue;
  Material(b);
  const bool scalar=property.kind=="FLOAT",color=property.kind=="COLOR";
  auto id=property.id;
  Components materialValues=values,blockValues{};
  bool preserveBlock=false;
  if(!scalar&&changed!=15){
    auto getter=color?api.getMaterialColor:api.getMaterialVector;
    void* key[]={&id};
    materialValues=Scalar<Components>(getter,b.sourceMaterial.Get(),key);
   blockValues=Scalar<Components>(api.getVector,b.work.Get(),key);
   preserveBlock=HasBlockComponents(b.work.Get(),id,blockValues);
   for(unsigned i=0;i<4;++i)if(changed&(1u<<i))materialValues[i]=values[i];
  }
  void* materialArgs[]={&id,materialValues.data()};
  Call(scalar?api.materialFloat:color?api.materialColor:api.materialVector,b.animatedMaterial.Get(),materialArgs);
  // Supply Material constants as well as per-slot PropertyBlocks. A successful
  // block readback alone does not establish which path the draw consumes.
  // Both use the same sample without mutating the shared donor or game block.
  Call(scalar?api.setFloat:color?api.setColor:api.setVector,b.work.Get(),materialArgs);
  if(preserveBlock){
   // Colors and vectors share the native block storage. Let SetColor convert
   // the driven components, then merge without converting the others again.
   void* key[]={&id};auto converted=Scalar<Components>(api.getVector,b.work.Get(),key);
   for(unsigned i=0;i<4;++i)if(changed&(1u<<i))blockValues[i]=converted[i];
   void* blockArgs[]={&id,blockValues.data()};Call(api.setVector,b.work.Get(),blockArgs);
  }
  EiemLoadPerformance::Add(EiemLoadPerformance::Operation::MaterialValueWrite);any=true;
 }return any;
}
void Capture(Bound& b){Read(api.getSlot,b.renderer.Get(),b.baseline.Get());b.baseEmpty=api.empty?Scalar<unsigned char>(api.empty,b.baseline.Get())!=0:api.emptyIcall(b.baseline.Get());Read(b.baseEmpty?api.getGlobal:api.getSlot,b.renderer.Get(),b.work.Get());}
// One bounded readback after binding distinguishes an inactive sample source
// from an applied vector. Never sample/read Unity properties on a worker.
void Observe(Bound& b){if(b.observed||GetTickCount64()<b.observeAt)return;b.observed=true;
 auto player=RuntimeControls::animations.find(b.package);Log("[MATERIAL-ANIMATION-STATE] package=%s renderer=%p installed=%d sampleOwners=%zu outputs=%zu",b.package.c_str(),b.renderer.Get(),int(b.installed),b.sampleOwners.size(),player==RuntimeControls::animations.end()?0:player->second.output.size());
 if(!b.installed||!api.getVector)return;auto readback=Block();originalGetSlot(b.renderer.Get(),readback.Get(),0);
 for(const auto& p:b.properties)if(p.kind=="VECTOR"){void* args[]={const_cast<int*>(&p.id)};auto actual=Scalar<std::array<float,4>>(api.getVector,readback.Get(),args);std::array<double,4> wanted{};for(const auto& c:p.channels)Channel(b.package,c.second,wanted[c.first]);
  auto material=api.getMaterialVector&&b.animatedMaterial.Get()?Scalar<std::array<float,4>>(api.getMaterialVector,b.animatedMaterial.Get(),args):std::array<float,4>{};
  Log("[MATERIAL-ANIMATION-VECTOR] package=%s renderer=%p property=%s wanted=(%.6g,%.6g,%.6g,%.6g) native=(%.6g,%.6g,%.6g,%.6g) material=(%.6g,%.6g,%.6g,%.6g) instance=%p donor=%p",b.package.c_str(),b.renderer.Get(),p.name.c_str(),wanted[0],wanted[1],wanted[2],wanted[3],actual[0],actual[1],actual[2],actual[3],material[0],material[1],material[2],material[3],b.animatedMaterial.Get(),b.sourceMaterial.Get());}}
// Native setters are intercepted at their icall, including callers that skip
// the managed wrapper. Keep the latest game block, never a static material value.
void SetSlot(void* renderer,void* block,int slot){originalSlot(renderer,block,slot);EiemLoadPerformance::Scope performance(EiemLoadPerformance::MaterialSlot);if(slot||!hasBindings.load(std::memory_order_relaxed)||unityThread.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;auto it=bound.find(renderer);if(it==bound.end()||!it->second.installed)return;performance.hit=true;auto& b=it->second;
 try{Capture(b);if(Values(b))originalSlot(renderer,b.work.Get(),0);else Restore(b);}catch(const std::exception& e){Restore(b);Log("[MATERIAL-ANIMATION-GAP] gameSlot=%s",e.what());}}
void SetGlobal(void* renderer,void* block){originalGlobal(renderer,block);EiemLoadPerformance::Scope performance(EiemLoadPerformance::MaterialGlobal);if(!hasBindings.load(std::memory_order_relaxed)||unityThread.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;auto it=bound.find(renderer);if(it==bound.end()||!it->second.installed||!it->second.baseEmpty)return;performance.hit=true;
 try{auto& b=it->second;Read(api.getGlobal,renderer,b.work.Get());if(Values(b))originalSlot(renderer,b.work.Get(),0);else Restore(b);}catch(const std::exception& e){Restore(it->second);Log("[MATERIAL-ANIMATION-GAP] gameGlobal=%s",e.what());}}
void GetSlot(void* renderer,void* block,int slot){originalGetSlot(renderer,block,slot);EiemLoadPerformance::Scope performance(EiemLoadPerformance::MaterialRead);if(internalRead||slot||!hasBindings.load(std::memory_order_relaxed)||unityThread.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;auto it=bound.find(renderer);if(it==bound.end()||!it->second.installed)return;performance.hit=true;
 try{void* args[]={it->second.baseline.Get()};Call(api.copy,block,args);}catch(const std::exception& e){Log("[MATERIAL-ANIMATION-GAP] gameRead=%s",e.what());}}
void Forget(void* renderer){auto it=bound.find(renderer);if(it!=bound.end()){Restore(it->second);bound.erase(it);hasBindings.store(!bound.empty(),std::memory_order_relaxed);}}
void Register(void* renderer,void* mesh,const RuntimeControls::Spec& spec){
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::MaterialRegister);
 if(!enabled||bound.count(renderer))return;auto graph=RuntimeControls::controlGraph;if(!graph)return;const Package* owner=nullptr;for(const auto& p:*graph)if(p.id==spec.package)owner=&p;if(!owner)return;
 std::map<std::string,Property> properties;for(const auto& row:owner->materialTracks)if(row.second.material==spec.material){const auto& t=row.second;auto& property=properties[t.property];Require(property.kind.empty()||property.kind==t.kind,"material animated property type differs");property.kind=t.kind;property.name=t.property;Require(property.channels.emplace(t.component,row.first).second,"material animated component duplicate");}
 if(properties.empty())return;performance.hit=true;
 auto mats=Ref::Make(Call(api.materials,renderer),false);auto materials=FullResourceReload::ReferenceArray(mats.Get(),1);Require(RuntimeShapes::Alive(materials.front()),"material animation target material missing");
 Bound b;b.renderer=Ref::Make(renderer,true);b.mesh=Ref::Make(mesh,false);b.package=spec.package;b.marker=spec.marker;b.baseline=Block();b.work=Block();b.observeAt=GetTickCount64()+1000;
 for(auto& row:properties){auto& property=row.second;auto key=Ref::Make(il2cpp_string_new(row.first.c_str()),false);void* args[]={key.Get()};
  if(!Scalar<unsigned char>(api.has,materials.front(),args)||
     (property.kind!="FLOAT"&&property.channels.size()<4&&
      (!api.getVector||!(property.kind=="COLOR"?api.getMaterialColor:api.getMaterialVector)))){
   Log("[MATERIAL-ANIMATION-GAP] property=%s part=%s skipped=1",row.first.c_str(),spec.part.c_str());continue;
  }
  property.id=Scalar<int>(api.propertyId,nullptr,args);b.properties.push_back(std::move(property));}
 if(b.properties.empty())return;
 // The Renderer consumes these tracks for its entire model lifetime. Bundle
 // proxies are loader objects and can retire before their instantiated model;
 // their leases alone cannot gate a live material's lazy samples.
 std::set<EiemAnimationSamples::Activity*> activities;
 for(const auto& clip:owner->shapeAnimations){bool consumes=false;
  for(const auto& property:b.properties)for(const auto& channel:property.channels)consumes|=clip.second.tracks.count(channel.second)!=0;
  auto source=clip.second.sampleSource;if(consumes&&source&&source->activity&&activities.insert(source->activity.get()).second)b.sampleOwners.push_back(std::make_shared<EiemAnimationSamples::Owner>(source->activity));}
 bound.emplace(renderer,std::move(b));unityThread.store(GetCurrentThreadId(),std::memory_order_relaxed);hasBindings.store(true,std::memory_order_relaxed);Log("[MATERIAL-ANIMATION-BIND] package=%s part=%s properties=%zu rendererSlot=0 sharedMaterialUnchanged=1",spec.package.c_str(),spec.part.c_str(),properties.size());
}
void Frame(){EiemLoadPerformance::Scope performance(EiemLoadPerformance::MaterialFrame);if(!enabled)return;performance.hit=!bound.empty();if(revision!=RuntimeControls::revision){for(auto& row:bound)Restore(row.second);bound.clear();revision=RuntimeControls::revision;
  for(const auto& row:RuntimeShapes::bound){auto spec=RuntimeControls::specs.find(row.second.marker);if(spec!=RuntimeControls::specs.end()&&row.second.modelReady)Register(row.second.renderer.Get(),row.second.mesh.Get(),spec->second);}}
 for(auto it=bound.begin();it!=bound.end();){auto& b=it->second;
   try{auto player=RuntimeControls::animations.find(b.package);bool playing=false;std::map<std::string,double> current;if(active&&player!=RuntimeControls::animations.end())for(const auto& prop:b.properties)for(const auto& channel:prop.channels){double value=0;if(Channel(b.package,channel.second,value)){playing=true;current.emplace(channel.second,value);}}
   auto r=b.renderer.Get();if(!RuntimeShapes::Alive(r)||RuntimeShapes::SourceMesh(r)!=b.mesh.Get()){Restore(b);it=bound.erase(it);continue;}Observe(b);if(!playing&&!b.installed){++it;continue;}
   if(!playing){Restore(b);++it;continue;}if(!b.installed)Capture(b);
   else if(current==b.lastValues){++it;continue;}
   if(b.installed){bool same=current.size()==b.lastValues.size();for(const auto& v:current)same&=b.lastValues.count(v.first)!=0;if(!same)ReleaseMaterial(b);}
   // Rebuild from the saved game block when property ownership changes. This
   // removes stopped track overrides while other material tracks keep playing.
   originalSlot(r,b.baseEmpty?nullptr:b.baseline.Get(),0);Read(b.baseEmpty?api.getGlobal:api.getSlot,r,b.work.Get());
   if(Values(b)){originalSlot(r,b.work.Get(),0);b.installed=true;b.lastValues=std::move(current);}else Restore(b);
  }catch(const std::exception& e){Restore(b);Log("[MATERIAL-ANIMATION-GAP] frame=%s rendererDisabled=1",e.what());it=bound.erase(it);continue;}++it;
 }
}
void Install(void** assemblies,size_t count){try{
 auto method=FullResourceReload::Method;auto renderer=FindClass("UnityEngine","Renderer",assemblies,count),material=FindClass("UnityEngine","Material",assemblies,count);api.blockClass=FindClass("UnityEngine","MaterialPropertyBlock",assemblies,count);Require(api.blockClass&&il2cpp_object_new,"material animation property block type missing");
 api.ctor=method(api.blockClass,".ctor",{},"System.Void",false);api.empty=Exact(api.blockClass,"get_isEmpty",{},"System.Boolean",false);
 api.copy=method(api.blockClass,"CopyFrom",{"UnityEngine.MaterialPropertyBlock"},"System.Void",false);
 if(!api.empty)api.emptyIcall=(decltype(api.emptyIcall))il2cpp_resolve_icall("UnityEngine.MaterialPropertyBlock::get_isEmpty");
 Require(api.empty||api.emptyIcall,"material block emptiness engine binding absent");
 api.getSlot=method(renderer,"GetPropertyBlock",{"UnityEngine.MaterialPropertyBlock","System.Int32"},"System.Void",false);api.getGlobal=method(renderer,"GetPropertyBlock",{"UnityEngine.MaterialPropertyBlock"},"System.Void",false);
 api.materials=method(renderer,"get_sharedMaterials",{},"UnityEngine.Material[]",false);api.has=method(material,"HasProperty",{"System.String"},"System.Boolean",false);
 api.sharedMaterial=method(renderer,"get_sharedMaterial",{},"UnityEngine.Material",false);api.setSharedMaterial=method(renderer,"set_sharedMaterial",{"UnityEngine.Material"},"System.Void",false);
 api.clone=method(FindClass("UnityEngine","Object",assemblies,count),"Instantiate",{"UnityEngine.Object"},"UnityEngine.Object",true);
 api.materialFloat=method(material,"SetFloat",{"System.Int32","System.Single"},"System.Void",false);api.materialVector=method(material,"SetVector",{"System.Int32","UnityEngine.Vector4"},"System.Void",false);api.materialColor=method(material,"SetColor",{"System.Int32","UnityEngine.Color"},"System.Void",false);
 api.getMaterialVector=Exact(material,"GetVector",{"System.Int32"},"UnityEngine.Vector4",false);
 if(!api.getMaterialVector)api.getMaterialVector=Exact(material,"GetColorImpl",{"System.Int32"},"UnityEngine.Color",false);
 api.getMaterialColor=Exact(material,"GetColor",{"System.Int32"},"UnityEngine.Color",false);
 if(!api.getMaterialColor)api.getMaterialColor=Exact(material,"GetColorImpl",{"System.Int32"},"UnityEngine.Color",false);
 api.propertyId=method(FindClass("UnityEngine","Shader",assemblies,count),"PropertyToID",{"System.String"},"System.Int32",true);
 api.setFloat=method(api.blockClass,"SetFloat",{"System.Int32","System.Single"},"System.Void",false);api.setVector=method(api.blockClass,"SetVector",{"System.Int32","UnityEngine.Vector4"},"System.Void",false);api.setColor=method(api.blockClass,"SetColor",{"System.Int32","UnityEngine.Color"},"System.Void",false);
 api.getVector=Exact(api.blockClass,"GetVector",{"System.Int32"},"UnityEngine.Vector4",false);
 if(!api.getVector)api.getVector=Exact(api.blockClass,"GetVectorImpl",{"System.Int32"},"UnityEngine.Vector4",false);
 api.hasBlockVector=Exact(api.blockClass,"HasVector",{"System.Int32"},"System.Boolean",false);
 if(!api.hasBlockVector)api.hasBlockVector=Exact(api.blockClass,"HasVectorImpl",{"System.Int32"},"System.Boolean",false);
 if(!api.hasBlockVector)api.hasBlockVectorIcall=(decltype(api.hasBlockVectorIcall))il2cpp_resolve_icall("UnityEngine.MaterialPropertyBlock::HasVectorImpl(System.Int32)");
 if(!api.hasBlockVector&&!api.hasBlockVectorIcall)api.hasBlockVectorIcall=(decltype(api.hasBlockVectorIcall))il2cpp_resolve_icall("UnityEngine.MaterialPropertyBlock::HasVectorImpl");
 auto slot=il2cpp_resolve_icall("UnityEngine.Renderer::Internal_SetPropertyBlockMaterialIndex"),global=il2cpp_resolve_icall("UnityEngine.Renderer::Internal_SetPropertyBlock");Require(slot&&global&&!installed.count(slot)&&!installed.count(global),"material animation native block setters unavailable");
 Require(Entry::InstallAddress(slot,"Renderer.Internal_SetPropertyBlockMaterialIndex",(void*)SetSlot,(void**)&originalSlot),"material slot override hook failed");
 Require(Entry::InstallAddress(global,"Renderer.Internal_SetPropertyBlock",(void*)SetGlobal,(void**)&originalGlobal),"material global override hook failed");revision=RuntimeControls::revision;
 auto getter=il2cpp_resolve_icall("UnityEngine.Renderer::Internal_GetPropertyBlockMaterialIndex");Require(getter&&!installed.count(getter),"material block native read binding unavailable");
 Require(Entry::InstallAddress(getter,"Renderer.Internal_GetPropertyBlockMaterialIndex",(void*)GetSlot,(void**)&originalGetSlot),"material block game read hook failed");enabled=true;
 Log("[MATERIAL-ANIMATION-READY] rendererSlotOwnership=1 gameBlockPreserved=1 workerUnityCalls=0");
 }catch(const std::exception& e){enabled=false;Log("[MATERIAL-ANIMATION-GAP] install=%s existingModStartupPreserved=1",e.what());}}
}
