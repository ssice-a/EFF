#pragma once
// Included in Entry after RuntimeShapes and native owner helpers.
namespace RuntimeMaterials {
using RuntimeShapes::Ref;using RuntimeShapes::Call;using RuntimeShapes::Scalar;
struct Api {void *blockClass=nullptr,*ctor=nullptr,*copy=nullptr,*getSlot=nullptr,*getGlobal=nullptr,*empty=nullptr,*propertyId=nullptr,*has=nullptr,*materials=nullptr,*setFloat=nullptr,*setVector=nullptr,*setColor=nullptr;bool(*emptyIcall)(void*)=nullptr;} api;
using SlotSetter=void(*)(void*,void*,int);using GlobalSetter=void(*)(void*,void*);
SlotSetter originalSlot=nullptr;GlobalSetter originalGlobal=nullptr;
SlotSetter originalGetSlot=nullptr;thread_local unsigned internalRead=0;
struct Property {std::string kind,name;int id=0;std::map<unsigned,std::string> channels;};
struct Bound {Ref renderer,mesh,baseline,work;std::string package,marker;std::vector<Property> properties;std::map<std::string,double> lastValues;bool installed=false,baseEmpty=true;};
std::map<void*,Bound> bound;bool enabled=false;uint64_t revision=0;
std::atomic<DWORD> unityThread{0};std::atomic<bool> hasBindings{false};
Ref Block(){auto b=Ref::Make(il2cpp_object_new(api.blockClass),false);Require(b.Get(),"material property block allocation failed");Call(api.ctor,b.Get());return b;}
void Read(void* method,void* renderer,void* block){struct Scope{Scope(){++internalRead;}~Scope(){--internalRead;}}scope;int slot=0;void* args[]={block,&slot};Call(method,renderer,args);}
void Restore(Bound& b){if(b.installed&&RuntimeShapes::Alive(b.renderer.Get()))originalSlot(b.renderer.Get(),b.baseEmpty?nullptr:b.baseline.Get(),0);b.installed=false;}
bool Values(Bound& b){auto player=RuntimeControls::animations.find(b.package);if(player==RuntimeControls::animations.end())return false;bool any=false;
 for(const auto& property:b.properties){bool present=false,complete=true;std::array<float,4> values{};
  for(const auto& c:property.channels){auto value=player->second.output.find(c.second);present|=value!=player->second.output.end();complete&=value!=player->second.output.end();if(value!=player->second.output.end())values[c.first]=float(value->second);}
  if(!present)continue;Require(complete,"material animation property has partial component ownership");void* args[]={const_cast<int*>(&property.id),values.data()};Call(property.kind=="FLOAT"?api.setFloat:property.kind=="COLOR"?api.setColor:api.setVector,b.work.Get(),args);EiemLoadPerformance::Add(EiemLoadPerformance::Operation::MaterialValueWrite);any=true;
 }return any;
}
void Capture(Bound& b){Read(api.getSlot,b.renderer.Get(),b.baseline.Get());b.baseEmpty=api.empty?Scalar<unsigned char>(api.empty,b.baseline.Get())!=0:api.emptyIcall(b.baseline.Get());Read(b.baseEmpty?api.getGlobal:api.getSlot,b.renderer.Get(),b.work.Get());}
// Native setters are intercepted at their icall, including callers that skip
// the managed wrapper. Keep the latest game block, never a static material value.
void SetSlot(void* renderer,void* block,int slot){originalSlot(renderer,block,slot);EiemLoadPerformance::Scope performance(EiemLoadPerformance::MaterialSlot);if(slot||!hasBindings.load(std::memory_order_relaxed)||unityThread.load(std::memory_order_relaxed)!=GetCurrentThreadId())return;auto it=bound.find(renderer);if(it==bound.end()||!it->second.installed)return;performance.hit=true;auto& b=it->second;
 try{Capture(b);if(Values(b))originalSlot(renderer,b.work.Get(),0);else b.installed=false;}catch(const std::exception& e){Restore(b);Log("[MATERIAL-ANIMATION-GAP] gameSlot=%s",e.what());}}
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
 Bound b;b.renderer=Ref::Make(renderer,true);b.mesh=Ref::Make(mesh,false);b.package=spec.package;b.marker=spec.marker;b.baseline=Block();b.work=Block();
 for(auto& row:properties){auto key=Ref::Make(il2cpp_string_new(row.first.c_str()),false);void* args[]={key.Get()};Require(Scalar<unsigned char>(api.has,materials.front(),args),"animated Shader property absent: "+row.first);row.second.id=Scalar<int>(api.propertyId,nullptr,args);
  Require(row.second.channels.size()==(row.second.kind=="FLOAT"?1:4),"animated vector requires four channels");b.properties.push_back(std::move(row.second));}
 bound.emplace(renderer,std::move(b));unityThread.store(GetCurrentThreadId(),std::memory_order_relaxed);hasBindings.store(true,std::memory_order_relaxed);Log("[MATERIAL-ANIMATION-BIND] package=%s part=%s properties=%zu rendererSlot=0 sharedMaterialUnchanged=1",spec.package.c_str(),spec.part.c_str(),properties.size());
}
void Frame(){EiemLoadPerformance::Scope performance(EiemLoadPerformance::MaterialFrame);if(!enabled)return;performance.hit=!bound.empty();if(revision!=RuntimeControls::revision){for(auto& row:bound)Restore(row.second);bound.clear();revision=RuntimeControls::revision;
  for(const auto& row:RuntimeShapes::bound){auto spec=RuntimeControls::specs.find(row.second.marker);if(spec!=RuntimeControls::specs.end()&&row.second.modelReady)Register(row.second.renderer.Get(),row.second.mesh.Get(),spec->second);}}
 for(auto it=bound.begin();it!=bound.end();){auto& b=it->second;
  try{auto player=RuntimeControls::animations.find(b.package);bool playing=false;std::map<std::string,double> current;if(active&&player!=RuntimeControls::animations.end())for(const auto& prop:b.properties)for(const auto& channel:prop.channels){auto value=player->second.output.find(channel.second);if(value!=player->second.output.end()){playing=true;current.emplace(value->first,value->second);}}
   if(!playing&&!b.installed){++it;continue;}auto r=b.renderer.Get();if(!RuntimeShapes::Alive(r)||RuntimeShapes::SourceMesh(r)!=b.mesh.Get()){Restore(b);it=bound.erase(it);continue;}
   if(!playing){Restore(b);++it;continue;}if(!b.installed)Capture(b);
   else if(current==b.lastValues){++it;continue;}
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
 api.propertyId=method(FindClass("UnityEngine","Shader",assemblies,count),"PropertyToID",{"System.String"},"System.Int32",true);
 api.setFloat=method(api.blockClass,"SetFloat",{"System.Int32","System.Single"},"System.Void",false);api.setVector=method(api.blockClass,"SetVector",{"System.Int32","UnityEngine.Vector4"},"System.Void",false);api.setColor=method(api.blockClass,"SetColor",{"System.Int32","UnityEngine.Color"},"System.Void",false);
 auto slot=il2cpp_resolve_icall("UnityEngine.Renderer::Internal_SetPropertyBlockMaterialIndex"),global=il2cpp_resolve_icall("UnityEngine.Renderer::Internal_SetPropertyBlock");Require(slot&&global&&!installed.count(slot)&&!installed.count(global),"material animation native block setters unavailable");
 Require(MH_CreateHook(slot,(void*)SetSlot,(void**)&originalSlot)==MH_OK&&MH_EnableHook(slot)==MH_OK,"material slot override hook failed");installed.insert(slot);
 Require(MH_CreateHook(global,(void*)SetGlobal,(void**)&originalGlobal)==MH_OK&&MH_EnableHook(global)==MH_OK,"material global override hook failed");installed.insert(global);revision=RuntimeControls::revision;
 auto getter=il2cpp_resolve_icall("UnityEngine.Renderer::Internal_GetPropertyBlockMaterialIndex");Require(getter&&!installed.count(getter),"material block native read binding unavailable");
 Require(MH_CreateHook(getter,(void*)GetSlot,(void**)&originalGetSlot)==MH_OK&&MH_EnableHook(getter)==MH_OK,"material block game read hook failed");installed.insert(getter);enabled=true;
 Log("[MATERIAL-ANIMATION-READY] rendererSlotOwnership=1 gameBlockPreserved=1 workerUnityCalls=0");
 }catch(const std::exception& e){enabled=false;Log("[MATERIAL-ANIMATION-GAP] install=%s existingModStartupPreserved=1",e.what());}}
}
