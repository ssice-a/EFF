#pragma once

// The game loads and retains the bundles. This hook adds only the new CAB
// edges already proven by the author compiler; it never opens a resource.
namespace StartupDependencies {
using RuntimeShapes::Ref;using RuntimeShapes::Call;
using EiemBundleDependencies::Names;using EiemBundleDependencies::Keys;
using Query=bool(*)(void*,uint64_t,void*,void*);
using InitFn=void(*)(void*,uint64_t,void*,void*);
using ClearFn=void(*)(void*,void*);
Query originalAll=nullptr,originalDirect=nullptr;InitFn originalInit=nullptr;ClearFn originalClear=nullptr;
struct Api {
 void *bundleName=nullptr,*listClass=nullptr,*listCtor=nullptr,*listAdd=nullptr,*allInfo=nullptr;
 void *managerManifest=nullptr,*proxyManagerField=nullptr,*listItems=nullptr,*listSize=nullptr;
 void *scalarClass=nullptr,*poolField=nullptr,*poolClass=nullptr,*poolAllocate=nullptr,*listDispose=nullptr;
 void *manifestData=nullptr,*bundles=nullptr,*mappedBase=nullptr,*mappedLength=nullptr,*mappedData=nullptr;
 size_t arraySize=0,arrayCount=0,arrayHead=0,bundleSize=0,bundleNameOffset=0,bundleKeyOffset=0;
 bool capacityCtor=false,boxedAdd=false;
} api;
struct Snapshot {Ref proxy,manifest;uint64_t hash=0;std::shared_ptr<const Names> graph;};
struct Index {Ref manifest,data;uintptr_t address=0;int64_t length=0;bool ready=false;std::weak_ptr<const Names> graph;Keys keys;std::set<std::pair<uint64_t,bool>> logged;};
std::mutex mutex;std::map<void*,Snapshot> snapshots;std::map<std::pair<void*,const Names*>,std::shared_ptr<Index>> indexes;
std::map<std::pair<void*,uint64_t>,void*> snapshotKeys;
std::atomic<bool> enabled{false};
std::string TypeName(const void* type){auto value=il2cpp_type_get_name((void*)type);Require(value,"dependency API type name absent");std::string name=value;il2cpp_free((void*)value);return name;}
void* Field(void* klass,const char* name){
 for(;klass;klass=il2cpp_class_get_parent(klass)){void* iterator=nullptr;while(auto field=il2cpp_class_get_fields(klass,&iterator)){auto label=il2cpp_field_get_name(field);if(label&&!strcmp(label,name))return field;}}
 throw std::runtime_error(std::string("dependency lifecycle field absent: ")+name);
}
void* Inherited(void* klass,const char* name,std::initializer_list<const char*> args,const char* result){
 for(;klass;klass=il2cpp_class_get_parent(klass))if(auto method=Exact(klass,name,args,result,false))return method;return nullptr;
}
size_t ValueSize(void* klass){uint32_t alignment=0;auto size=il2cpp_class_value_size(klass,&alignment);Require(size>0,"manifest value structure absent");return size_t(size);}
size_t ValueOffset(void* klass,const char* name,size_t width){
 auto field=Field(klass,name);auto offset=il2cpp_field_get_offset(field),size=ValueSize(klass);
 Require(offset>=2*sizeof(void*)&&offset-2*sizeof(void*)<=size&&width<=size-(offset-2*sizeof(void*)),"manifest value field range differs");return offset-2*sizeof(void*);
}
void BindManifestFields(void* manifest){
 api.manifestData=Field(manifest,"m_manifestData");auto data=il2cpp_class_from_type(il2cpp_field_get_type(api.manifestData));
 api.bundles=Field(data,"bundles");api.mappedBase=Field(data,"m_address");api.mappedLength=Field(data,"m_length");api.mappedData=Field(data,"m_dataAddress");
 Require(TypeName(il2cpp_field_get_type(api.mappedLength))=="System.Int64","manifest mapped length type differs");
 auto array=il2cpp_class_from_type(il2cpp_field_get_type(api.bundles));api.arraySize=ValueSize(array);
 auto countType=TypeName(il2cpp_field_get_type(Field(array,"length")));Require(countType=="System.Int32"||countType=="System.UInt32","manifest array count type differs");
 api.arrayCount=ValueOffset(array,"length",sizeof(int32_t));api.arrayHead=ValueOffset(array,"m_head",sizeof(void*));
 // Resolve the value type from the game's own out parameter. Offsets and
 // stride come from IL2CPP metadata, never from the diagnostic file layout.
 void* bundle=nullptr,*iterator=nullptr;
 while(auto method=il2cpp_class_get_methods(data,&iterator)){
  if(strcmp(il2cpp_method_get_name(method),"TryGetBundleByIndex")||il2cpp_method_get_param_count(method)!=2)continue;
  Require(TypeName(il2cpp_method_get_param(method,0))=="System.Int32"&&!bundle,"manifest Bundle lookup shape differs");bundle=il2cpp_class_from_type(il2cpp_method_get_param(method,1));
 }
 Require(bundle,"manifest Bundle value type absent");api.bundleSize=ValueSize(bundle);
 auto keyType=TypeName(il2cpp_field_get_type(Field(bundle,"hashName")));Require(keyType=="System.Int64"||keyType=="System.UInt64","manifest Bundle key type differs");
 api.bundleKeyOffset=ValueOffset(bundle,"hashName",sizeof(uint64_t));
 auto name=il2cpp_class_from_type(il2cpp_field_get_type(Field(bundle,"name")));Require(ValueSize(name)==sizeof(int32_t),"manifest name reference shape differs");
 api.bundleNameOffset=ValueOffset(bundle,"name",sizeof(int32_t))+ValueOffset(name,"offset",sizeof(int32_t));
}
struct ManifestState {void* data=nullptr;uintptr_t address=0;int64_t length=0;};
ManifestState State(void* manifest){
 ManifestState state;il2cpp_field_get_value(manifest,api.manifestData,&state.data);
 if(state.data){il2cpp_field_get_value(state.data,api.mappedBase,&state.address);il2cpp_field_get_value(state.data,api.mappedLength,&state.length);}return state;
}
std::map<std::string,uint64_t> ManifestKeys(void* manifest,const ManifestState& state,const Names& graph){
 auto start=std::chrono::steady_clock::now();
 Require(state.data&&state.address&&state.length>0&&uint64_t(state.length)<=UINTPTR_MAX-state.address,"manifest mapped range absent");
 auto range=[&](uintptr_t p,size_t size){return p>=state.address&&p-state.address<=uint64_t(state.length)&&size<=uint64_t(state.length)-(p-state.address);};
 Bytes array(api.arraySize);il2cpp_field_get_value(state.data,api.bundles,array.data());int32_t count=0;uintptr_t head=0,data=0;
 memcpy(&count,array.data()+api.arrayCount,sizeof(count));memcpy(&head,array.data()+api.arrayHead,sizeof(head));il2cpp_field_get_value(state.data,api.mappedData,&data);
 Require(count>=0&&size_t(count)<=SIZE_MAX/api.bundleSize&&range(head,size_t(count)*api.bundleSize)&&range(data,sizeof(int32_t)),"manifest Bundle array range differs");
 std::map<std::wstring,std::string> wanted;
 auto want=[&](const std::string& name){auto length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.c_str(),int(name.size()),nullptr,0);Require(length>0,"compiled Bundle label encoding differs");std::wstring wide(length,L'\0');Require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.c_str(),int(name.size()),wide.data(),length)==length,"compiled Bundle label decode failed");wanted.emplace(std::move(wide),name);};
 for(const auto& row:graph){want(row.first);for(const auto& name:row.second)want(name);}
 std::map<std::string,uint64_t> result;Bytes records(size_t(count)*api.bundleSize);Require(CopyRaw((void*)head,records.data(),records.size()),"manifest Bundle records unreadable");
 std::wstring label;
 for(int32_t i=0;i<count;++i){
  const auto* row=records.data()+size_t(i)*api.bundleSize;int32_t offset=0,length=0;memcpy(&offset,row+api.bundleNameOffset,sizeof(offset));
  Require(offset>=0&&uintptr_t(offset)<=UINTPTR_MAX-data,"manifest Bundle name offset differs");auto at=data+uintptr_t(offset);
  Require(range(at,sizeof(length))&&CopyRaw((void*)at,&length,sizeof(length))&&length>=0&&length%sizeof(wchar_t)==0,"manifest Bundle name length differs");at+=sizeof(length);
  Require(range(at,size_t(length)),"manifest Bundle name escapes mapped data");label.resize(size_t(length)/sizeof(wchar_t));Require(CopyRaw((void*)at,label.data(),size_t(length)),"manifest Bundle name unreadable");
  auto found=wanted.find(label);if(found==wanted.end())continue;uint64_t key=0;memcpy(&key,row+api.bundleKeyOffset,sizeof(key));
  auto inserted=result.emplace(found->second,key);Require(inserted.second||inserted.first->second==key,"ambiguous manifest Bundle name");
 }
 Require(result.size()==wanted.size(),"compiled Bundle absent from game manifest (matched="+std::to_string(result.size())+" required="+std::to_string(wanted.size())+")");
 for(const auto& row:result){auto value=row.second;void* args[]={&value};Require(String(Call(api.bundleName,manifest,args))==row.first,"manifest Bundle key/name round trip differs: "+row.first);}
 Log("[STARTUP-DEPENDENCY-KEYS] manifestBundles=%d selectedNames=%zu nativeStoredKeys=1 indexMs=%.3f",count,result.size(),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
 return result;
}
std::vector<uint64_t> ReadList(void* list){
 void* array=nullptr;int size=0;size_t capacity=0;
  il2cpp_field_get_value(list,api.listItems,&array);il2cpp_field_get_value(list,api.listSize,&size);
  Require(size>=0&&ArrayLength(array,&capacity)&&size_t(size)<=capacity,"game dependency list range differs");SkinningObservation::ArrayElementSize(array,sizeof(uint64_t));
 std::vector<uint64_t> result(size);Require(!size||CopyRaw((char*)array+32,result.data(),size_t(size)*sizeof(uint64_t)),"game dependency list data unreadable");return result;
}
void BindListMethods(void* klass,const std::string& scalar){
 api.capacityCtor=api.boxedAdd=false;
 api.listCtor=nullptr;
 if(!api.poolAllocate){api.listCtor=Exact(klass,".ctor",{},"System.Void",false);
  if(!api.listCtor){api.listCtor=Exact(klass,".ctor",{"System.Int32"},"System.Void",false);api.capacityCtor=true;}}
 api.listAdd=Inherited(klass,"Add",{scalar.c_str()},"System.Void");
 // The native manifest inlines List<T>.Add. An AOT build may retain only
 // the IList bridge; use its own implementation, including resize/version
 // handling, rather than writing List fields or assuming an uncompiled API.
 if(!api.listAdd){api.listAdd=Inherited(klass,"System.Collections.IList.Add",{"System.Object"},"System.Int32");api.boxedAdd=true;}
 if((!api.poolAllocate&&!api.listCtor)||!api.listAdd){
  void* iterator=nullptr;while(auto method=il2cpp_class_get_methods(klass,&iterator)){
   auto name=il2cpp_method_get_name(method);if(!name||(strcmp(name,".ctor")&&strcmp(name,"Add")&&strcmp(name,"System.Collections.IList.Add")))continue;
   std::string arguments;for(uint32_t i=0;i<il2cpp_method_get_param_count(method);++i){if(i)arguments+=",";arguments+=TypeName(il2cpp_method_get_param(method,i));}
   Log("[STARTUP-DEPENDENCY-LIST-API] method=%s(%s)->%s compiled=%d",name,arguments.c_str(),TypeName(il2cpp_method_get_return_type(method)).c_str(),int(((MInfo*)method)->mp!=nullptr));
  }
 }
 Require(api.poolAllocate||api.listCtor,"game dependency list constructor absent (default/capacity/pool)");
 Require(api.listAdd,"game dependency list append absent (typed/IList)");
}
struct Scratch {
 Ref list;bool pooled=false;
 Scratch(Ref value,bool recycle):list(std::move(value)),pooled(recycle){}
 Scratch(const Scratch&)=delete;Scratch(Scratch&& other)noexcept:list(std::move(other.list)),pooled(other.pooled){other.pooled=false;}
 void* Get()const{return list.Get();}
 ~Scratch(){if(pooled&&list.Get())try{Call(api.listDispose,list.Get());}catch(const std::exception& e){Log("[STARTUP-DEPENDENCY-GAP] lifecycle=scratch-dispose reason=%s",e.what());}}
};
Scratch NewList(void* prototype=nullptr){
 if(api.poolAllocate){void* pool=nullptr;Require(prototype,"pooled dependency list prototype absent");il2cpp_field_get_value(prototype,api.poolField,&pool);
  Require(pool&&il2cpp_object_get_class(pool)==api.poolClass,"dependency list pool type differs");auto value=Ref::Make(Call(api.poolAllocate,pool),false);
  Require(value.Get()&&value.Get()!=prototype&&il2cpp_object_get_class(value.Get())==api.listClass,"native dependency pool returned a different list type");return Scratch(std::move(value),true);
 }
 auto list=Ref::Make(il2cpp_object_new(api.listClass),false);Require(list.Get(),"dependency scratch list allocation failed");
 int capacity=0;void* args[]={&capacity};Call(api.listCtor,list.Get(),api.capacityCtor?args:nullptr);return Scratch(std::move(list),false);
}
void Append(void* list,uint64_t value){
 if(api.boxedAdd){auto boxed=Ref::Make(il2cpp_value_box(api.scalarClass,&value),false);Require(boxed.Get(),"dependency key boxing failed");void* args[]={boxed.Get()};Call(api.listAdd,list,args);}
 else{void* args[]={&value};Call(api.listAdd,list,args);}
}
void VerifyListApi(void* prototype=nullptr){
 auto list=NewList(prototype);Require(ReadList(list.Get()).empty(),"new dependency list is not empty");
 const std::vector<uint64_t> values{0,0x1122334455667788ULL,0xFEDCBA9876543210ULL};
 for(auto value:values)Append(list.Get(),value);
 Require(ReadList(list.Get())==values,"game dependency list key round trip differs");
}
std::atomic<bool> listVerified{false};
std::shared_ptr<const Names> CurrentGraph(){auto rules=std::atomic_load(&resourceRules);return rules?rules->addedDependencies:nullptr;}
void RecordInit(void* proxy,uint64_t hash,void* manager){
 auto manifest=Call(api.managerManifest,manager);Require(manifest,"game dependency manifest absent");
 Snapshot snapshot{Ref::Make(proxy,true),Ref::Make(manifest,true),hash,CurrentGraph()};
 std::lock_guard<std::mutex> lock(mutex);auto old=snapshots.find(proxy);if(old!=snapshots.end())snapshotKeys.erase({old->second.manifest.Get(),old->second.hash});snapshotKeys[{manifest,hash}]=proxy;snapshots[proxy]=std::move(snapshot);
}
void Init(void* self,uint64_t hash,void* manager,void* info){
 originalInit(self,hash,manager,info);
 if(!enabled)return;
 try{RecordInit(self,hash,manager);}catch(const std::exception& e){Log("[STARTUP-DEPENDENCY-GAP] lifecycle=init reason=%s",e.what());}
}
void Clear(void* self,void* info){
 // The game's ClearAll may release its dependencies. Keep the snapshot until
 // that release completes, then permit the pooled proxy's next Init to bind
 // the current Mod graph.
 originalClear(self,info);std::lock_guard<std::mutex> lock(mutex);auto old=snapshots.find(self);if(old!=snapshots.end()){snapshotKeys.erase({old->second.manifest.Get(),old->second.hash});snapshots.erase(old);}
}
std::shared_ptr<const Names> GraphFor(void* manifest,uint64_t hash){
 std::lock_guard<std::mutex> lock(mutex);
 auto key=snapshotKeys.find({manifest,hash});if(key!=snapshotKeys.end()){auto found=snapshots.find(key->second);if(found!=snapshots.end()&&found->second.proxy.Get()==found->first&&found->second.manifest.Get()==manifest)return found->second.graph;
  if(found!=snapshots.end())snapshots.erase(found);snapshotKeys.erase(key);}
 return CurrentGraph();
}
std::shared_ptr<Index> GetIndex(void* manifest,const std::shared_ptr<const Names>& graph){
 if(!graph||graph->empty())return {};
 auto key=std::make_pair(manifest,graph.get());
 auto state=State(manifest);std::lock_guard<std::mutex> lock(mutex);
 auto found=indexes.find(key);if(found!=indexes.end()&&found->second->manifest.Get()==manifest&&found->second->data.Get()==state.data&&found->second->address==state.address&&found->second->length==state.length&&!found->second->graph.expired())return found->second->ready?found->second:nullptr;
 auto result=std::make_shared<Index>();result->manifest=Ref::Make(manifest,true);result->data=Ref::Make(state.data,true);result->address=state.address;result->length=state.length;result->graph=graph;
 // A failed graph must not be rebuilt/logged on every native queue poll.
 // Reconsider it when the mapped manifest or author graph actually changes.
 for(auto it=indexes.begin();it!=indexes.end();)if(!it->second->manifest.Get()||it->second->graph.expired())it=indexes.erase(it);else ++it;indexes[key]=result;
 try{
  auto hashes=ManifestKeys(manifest,state,*graph);
  for(const auto& row:*graph){auto from=hashes.at(row.first);auto& edges=result->keys[from];for(const auto& name:row.second){auto to=hashes.at(name);Require(to!=from,"compiled dependency aliases its source Bundle");edges.push_back(to);}}
  result->ready=true;Log("[STARTUP-DEPENDENCIES] compiledSources=%zu namesVerified=%zu gameOwnsLoadsAndReferences=1",result->keys.size(),hashes.size());return result;
 }catch(const std::exception& e){Log("[STARTUP-DEPENDENCY-GAP] phase=manifest-index reason=%s originalPreserved=1 retryOnManifestOrModChange=1",e.what());return {};}
}
bool Augment(void* manifest,uint64_t hash,void* list,bool direct,bool originalResult){
 if(!enabled)return originalResult;
 EiemLoadPerformance::Scope performance(direct?EiemLoadPerformance::DependencyDirect:EiemLoadPerformance::DependencyAll);
 if(!listVerified){VerifyListApi(list);listVerified=true;Log("[STARTUP-DEPENDENCY-LIST-READY] pooled=%d inheritedAppend=1 keyRoundTrip=1",int(api.poolAllocate!=nullptr));}
 auto graph=GraphFor(manifest,hash);if(!graph||graph->empty())return originalResult;
 auto index=GetIndex(manifest,graph);if(!index||(direct&&!index->keys.count(hash)))return originalResult;auto before=ReadList(list);
 auto edges=[&](uint64_t key){auto own=GraphFor(manifest,key);auto resolved=own==graph?index:GetIndex(manifest,own);if(!resolved)return std::vector<uint64_t>{};auto found=resolved->keys.find(key);return found==resolved->keys.end()?std::vector<uint64_t>{}:found->second;};
 auto native=[&](uint64_t key){auto scratch=NewList(list);originalAll(manifest,key,scratch.Get(),api.allInfo);return ReadList(scratch.Get());};
 auto added=EiemBundleDependencies::Added(hash,before,direct,edges,native);
 performance.hit=!added.empty();
 for(auto value:added)Append(list,value);
 bool report=false;if(!added.empty()){std::lock_guard<std::mutex> lock(mutex);report=index->logged.insert({hash,direct}).second;}
 if(report)Log("[STARTUP-DEPENDENCY-APPEND] bundleHash=%016llX direct=%d added=%zu nativeCount=%zu gameQueued=1",(unsigned long long)hash,int(direct),added.size(),before.size());
 return originalResult||!added.empty();
}
bool All(void* self,uint64_t hash,void* list,void* info){
 auto result=originalAll(self,hash,list,info);
 try{return Augment(self,hash,list,false,result);}catch(const std::exception& e){Log("[STARTUP-DEPENDENCY-GAP] query=all reason=%s originalPreserved=1",e.what());return result;}
}
bool Direct(void* self,uint64_t hash,void* list,void* info){
 auto result=originalDirect(self,hash,list,info);
 try{return Augment(self,hash,list,true,result);}catch(const std::exception& e){Log("[STARTUP-DEPENDENCY-GAP] query=direct reason=%s originalPreserved=1",e.what());return result;}
}
void Install(void** assemblies,size_t count){
 try{
  auto manifest=FindClass("Beyond.ManifestBinary","RuntimeManifestBinary",assemblies,count);Require(manifest,"game dependency manifest class absent");
  std::string scalar,listName;void* queryInfo=nullptr,*iterator=nullptr;
  while(auto method=il2cpp_class_get_methods(manifest,&iterator)){
   auto name=il2cpp_method_get_name(method);uint32_t impl=0;if(!name||strcmp(name,"TryGetBundleDeps")||il2cpp_method_get_param_count(method)!=2||(il2cpp_method_get_flags(method,&impl)&0x10))continue;
   auto first=TypeName(il2cpp_method_get_param(method,0));if(first!="System.UInt64"&&first!="System.Int64")continue;
   Require(!queryInfo,"ambiguous game dependency query");queryInfo=method;scalar=first;listName=TypeName(il2cpp_method_get_param(method,1));api.listClass=il2cpp_class_from_type(il2cpp_method_get_param(method,1));
  }
  Require(queryInfo&&api.listClass,"game dependency query shape absent");
  api.allInfo=queryInfo;api.scalarClass=il2cpp_class_from_type(il2cpp_method_get_param(queryInfo,0));
  api.listItems=Field(api.listClass,"_items");api.listSize=Field(api.listClass,"_size");
  Require(TypeName(il2cpp_field_get_type(api.listItems))==scalar+"[]"&&TypeName(il2cpp_field_get_type(api.listSize))=="System.Int32","game dependency list element types differ");
  // The concrete argument may be the game's PooledList<T>. Its List<T>
  // methods are inherited, while scratch lists must come from the same pool.
  try{api.poolField=Field(api.listClass,"m_objectPool");}catch(const std::exception&){api.poolField=nullptr;}
  if(api.poolField){api.poolClass=il2cpp_class_from_type(il2cpp_field_get_type(api.poolField));
   api.poolAllocate=Exact(api.poolClass,"Allocate",{},listName.c_str(),false);api.listDispose=Inherited(api.listClass,"Dispose",{},"System.Void");
   Require(api.poolAllocate&&api.listDispose,"native dependency pool allocate/dispose API absent");
  }
  BindListMethods(api.listClass,scalar);if(!api.poolAllocate){VerifyListApi();listVerified=true;}
  Log("[STARTUP-DEPENDENCY-LIST-BOUND] pooled=%d capacityCtor=%d boxedAdd=%d",int(api.poolAllocate!=nullptr),int(api.capacityCtor),int(api.boxedAdd));
  api.bundleName=Exact(manifest,"GetBundleNameString",{scalar.c_str()},"System.String",false);
  Require(api.bundleName,"game dependency identity API absent");BindManifestFields(manifest);
  auto proxy=FindClass("","BundleProxy",assemblies,count);Require(proxy,"game Bundle proxy class absent");api.proxyManagerField=Field(proxy,"m_manager");auto manager=il2cpp_class_from_type(il2cpp_field_get_type(api.proxyManagerField));
  api.managerManifest=Exact(manager,"get_getRuntimeManifest",{},"Beyond.ManifestBinary.RuntimeManifestBinary",false);Require(api.managerManifest,"game Bundle manager manifest getter absent");
  auto managerName=TypeName(il2cpp_field_get_type(api.proxyManagerField));
  Require(Entry::Install(proxy,"Init",{scalar.c_str(),managerName.c_str()},"System.Void",false,(void*)Init,(void**)&originalInit),"game Bundle Init observation absent");
  Require(Entry::Install(proxy,"ClearAll",{},"System.Void",false,(void*)Clear,(void**)&originalClear),"game Bundle ClearAll observation absent");
  Require(Entry::Install(manifest,"TryGetBundleDeps",{scalar.c_str(),listName.c_str()},"System.Boolean",false,(void*)All,(void**)&originalAll),"game dependency query hook absent");
  Require(Entry::Install(manifest,"TryGetBundleDirectDeps",{scalar.c_str(),listName.c_str()},"System.Boolean",false,(void*)Direct,(void**)&originalDirect),"game direct dependency query hook absent");
  enabled=true;Log("[STARTUP-DEPENDENCIES-READY] nativeQueue=1 resourceLoadsByPlugin=0 compiledEdgesOnly=1");
 }catch(const std::exception& e){Log("[STARTUP-DEPENDENCY-GAP] phase=bind reason=%s",e.what());}
}
} // namespace StartupDependencies
