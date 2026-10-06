// Exercise production hook orchestration without patching or loading Unity.
#define MH_CreateHook FixtureCreateHook
#define MH_EnableHook FixtureEnableHook
#define MH_RemoveHook FixtureRemoveHook
#include "../../src/eiem_resource_entry.cpp"
#undef MH_CreateHook
#undef MH_EnableHook
#undef MH_RemoveHook
#include <iostream>

namespace StartupFixture {
using namespace Entry;
struct Method { MInfo info; const char* name; const char* result; bool stat;
  std::vector<const char*> args; };
struct Class { const char* ns; const char* name; std::vector<Method> methods; };
std::vector<Class> classes;
unsigned nativeCalls=0, fileHooks=0, unreadyFileHooks=0;
std::set<void*> publicFiles;
std::vector<std::unique_ptr<Bytes>> allocations;
std::map<uint32_t,void*> roots;
uint32_t nextRoot=0;
void* streamObject=(void*)0xC100;void* createRequest=(void*)0xC200;void* bundleObject=(void*)0xC300;
void* backingArray=nullptr;bool requestDone=false,bundleAlive=true;unsigned streamCalls=0,rawMemoryCalls=0;
unsigned char doneValue=0,aliveValue=1;
void* StreamLoad(void* stream,uint32_t crc,uint32_t bufferSize) {
  Require(stream==streamObject&&crc==0&&bufferSize==0,"stream native argument ABI differs");
  Require(backingArray&&!memcmp((char*)backingArray+32,"game-package",12),"stream lost its seekable package bytes");
  ++streamCalls;return createRequest;
}
void Add(const char* name,std::initializer_list<Method> methods,const char* ns="UnityEngine") {
  classes.push_back({ns,name,methods});
}
void Bind() {
  Add("Object",{{{},"get_name","System.String",false,{}}});
  Add("SkinnedMeshRenderer",{{{},"get_sharedMesh","UnityEngine.Mesh",false,{}},
      {{},"SkinMatricesRequestFinished","System.Boolean",false,{}}});
  Add("Mesh",{{{},"get_blendShapeCount","System.Int32",false,{}},
      {{},"get_isReadable","System.Boolean",false,{}}});
  Add("AssetBundle",{
      {{},"LoadFromFile","UnityEngine.AssetBundle",true,{"System.String","System.UInt32","System.UInt64"}},
      {{},"LoadFromFileAsync","UnityEngine.AssetBundleCreateRequest",true,{"System.String","System.UInt32","System.UInt64"}},
      {{},"LoadFromMemoryAsync_Internal","UnityEngine.AssetBundleCreateRequest",true,{"System.Byte[]","System.UInt32"}},
      {{},"LoadAssetAsync_Internal","UnityEngine.AssetBundleRequest",false,{"System.String","System.Type"}},
      {{},"Unload","System.Void",false,{"System.Boolean"}}});
  Add("AssetBundleCreateRequest",{{{},"get_assetBundle","UnityEngine.AssetBundle",false,{}}});
  Add("AssetBundleRequest",{{{},"GetResult","UnityEngine.Object",false,{}}});
  Add("AsyncOperation",{{{},"get_isDone","System.Boolean",false,{}}});
  Add("Byte",{},"System");
  Add("MemoryStream",{{{},".ctor","System.Void",false,{"System.Byte[]","System.Boolean"}}},"System.IO");
  Add("ManagedStreamHelpers",{
      {{},"ManagedStreamRead","System.Void",true,{"System.Byte[]","System.Int32","System.Int32","System.IO.Stream","System.IntPtr"}},
      {{},"ManagedStreamSeek","System.Void",true,{"System.Int64","System.UInt32","System.IO.Stream","System.IntPtr"}},
      {{},"ManagedStreamLength","System.Void",true,{"System.IO.Stream","System.IntPtr"}}});
  uintptr_t address=0x100000;
  for(auto& c:classes)for(auto& m:c.methods) {
    m.info.mp=(void*)(address+=16);
    if(!strcmp(m.name,"LoadFromFile")||!strcmp(m.name,"LoadFromFileAsync"))publicFiles.insert(m.info.mp);
  }
  il2cpp_assembly_get_image=[](void* p)->void* {return p;};
  il2cpp_image_get_class_count=[](void*)->size_t {return classes.size();};
  il2cpp_image_get_class=[](void*,size_t i)->void* {return &classes.at(i);};
  il2cpp_class_get_name=[](void* p)->const char* {return ((Class*)p)->name;};
  il2cpp_class_get_namespace=[](void* p)->const char* {return ((Class*)p)->ns;};
  il2cpp_class_get_methods=[](void* p,void** it)->void* {
    auto& methods=((Class*)p)->methods;auto i=uintptr_t(*it);
    if(i==methods.size())return nullptr;*it=(void*)(i+1);return &methods[i];
  };
  il2cpp_method_get_name=[](void* p)->const char* {return ((Method*)p)->name;};
  il2cpp_method_get_param_count=[](void* p)->uint32_t {return uint32_t(((Method*)p)->args.size());};
  il2cpp_method_get_flags=[](void* p,uint32_t*)->uint32_t {return ((Method*)p)->stat?0x10:0;};
  il2cpp_method_get_return_type=[](void* p)->void* {return (void*)((Method*)p)->result;};
  il2cpp_method_get_param=[](void* p,uint32_t i)->void* {return (void*)((Method*)p)->args.at(i);};
  il2cpp_type_get_name=[](void* p)->const char* {return _strdup((const char*)p);};
  il2cpp_free=[](void* p) {free(p);};
  il2cpp_runtime_invoke=[](void*,void*,void**,void**)->void* {++nativeCalls;throw std::runtime_error("startup initiated a native resource call");};
  il2cpp_gchandle_new=[](void*,bool)->uint32_t {++nativeCalls;return 1;};
  il2cpp_string_new=[](const char*)->void* {++nativeCalls;return nullptr;};
  il2cpp_class_get_type=[](void* p)->void* {return p;};
  il2cpp_type_get_object=[](void* p)->void* {return p;};
  il2cpp_object_new=[](void*)->void* {++nativeCalls;return nullptr;};
  il2cpp_resolve_icall=[](const char* name)->void* {
    return !strcmp(name,"UnityEngine.AssetBundle::LoadFromStreamInternal")||!strcmp(name,"UnityEngine.AssetBundle::LoadFromStreamAsyncInternal")?(void*)StreamLoad:nullptr;
  };
  // The shape bridge has completed metadata preparation, as in the real log.
  // Its completion hook remains uninstalled until production orchestration runs.
#define PREPARED(n) RuntimeShapes::n=reinterpret_cast<decltype(RuntimeShapes::n)>(uintptr_t(0x800000))
  PREPARED(originalCapture);PREPARED(originalRelease);PREPARED(originalReset);
  PREPARED(originalTick);PREPARED(originalFrameTick);PREPARED(originalInit);
  PREPARED(originalMeshGetter);PREPARED(originalMatrixRequest);PREPARED(originalEnabledSetter);
#undef PREPARED
  CaptureObservation::afterFinished=RuntimeShapes::Finished;
  CaptureObservation::originalFinished=nullptr;
  RuntimeShapes::enabled=false;NativeBundle::enabled=false;
}
void BindStreamCalls() {
  il2cpp_runtime_invoke=[](void* p,void* self,void** args,void** exception)->void* {
    *exception=nullptr;
    if(p==(void*)0xA110){aliveValue=uint8_t(args[0]!=bundleObject||bundleAlive);return &aliveValue;}
    const auto name=((Method*)p)->name;
    if(!strcmp(name,"LoadFromMemoryAsync_Internal")){++rawMemoryCalls;throw std::runtime_error("Failed to decompress data for the AssetBundle 'Memory'");}
    if(!strcmp(name,".ctor")){Require(self==streamObject&&args&&args[0]&&!*(bool*)args[1],"read-only MemoryStream constructor ABI differs");backingArray=args[0];return nullptr;}
    if(!strcmp(name,"get_isDone")){doneValue=requestDone;return &doneValue;}
    if(!strcmp(name,"get_assetBundle")){CompleteRuleInput(self,bundleObject);return bundleObject;}
    throw std::runtime_error(std::string("unexpected managed call: ")+name);
  };
  il2cpp_array_new=[](void*,size_t size)->void* {auto bytes=std::make_unique<Bytes>(32+size);uint64_t count=size;memcpy(bytes->data()+24,&count,8);auto value=bytes->data();allocations.push_back(std::move(bytes));return value;};
  il2cpp_object_new=[](void*)->void* {return streamObject;};
  il2cpp_object_unbox=[](void* p)->void* {return p;};
  il2cpp_gchandle_new=[](void* p,bool)->uint32_t {roots[++nextRoot]=p;return nextRoot;};
  il2cpp_gchandle_get_target=[](uint32_t id)->void* {return roots.at(id);};
  il2cpp_gchandle_free=[](uint32_t id) {Require(roots.erase(id)==1,"root released twice");};
  il2cpp_resolve_icall=[](const char* name)->void* {
    if(!strcmp(name,"UnityEngine.AssetBundle::LoadFromMemory_Internal"))return (void*)0x900000;
    if(!strcmp(name,"UnityEngine.AssetBundle::LoadFromStreamAsyncInternal")||!strcmp(name,"UnityEngine.AssetBundle::LoadFromStreamInternal"))return (void*)StreamLoad;
    return nullptr;
  };
  RuntimeShapes::api.alive=(void*)0xA110;
}
struct Files {
  std::filesystem::path root=std::filesystem::temp_directory_path()/
      ("eiem-startup-readiness-"+std::to_string(GetCurrentProcessId()));
  Files() {Require(!std::filesystem::exists(root),"fixture path already exists");
    std::filesystem::create_directories(root/"plugin/tools");
    std::ofstream(root/"plugin/tools/eff_resource_pack.exe").put('x');gameDirectory=root;
  }
  ~Files() {
    std::filesystem::remove(root/"plugin/tools/eff_resource_pack.exe");
    std::filesystem::remove(root/"plugin/tools");std::filesystem::remove(root/"plugin");std::filesystem::remove(root);
  }
};
}
extern "C" MH_STATUS WINAPI FixtureCreateHook(LPVOID target,LPVOID,LPVOID* original) {
  if(original)*original=target;return MH_OK;
}
extern "C" MH_STATUS WINAPI FixtureEnableHook(LPVOID target) {
  using namespace StartupFixture;
  if(publicFiles.count(target)) {
    ++fileHooks;
    if(!Entry::RuntimeShapes::enabled||!Entry::NativeBundle::enabled)++unreadyFileHooks;
  }
  return MH_OK;
}
extern "C" MH_STATUS WINAPI FixtureRemoveHook(LPVOID) {return MH_OK;}
int main(int argc,char**) {
  using namespace Entry;using namespace StartupFixture;
  try {
    Files files;Bind();if(argc>1)BindStreamCalls();void* assembly=(void*)1;
    InstallHooks(&assembly,1);
    Require(CaptureObservation::originalFinished,"production startup did not install capture completion");
    Require(RuntimeShapes::enabled,"production startup left prepared controls disabled");
    Require(NativeBundle::enabled&&NativeBundle::api.streamIcall&&NativeBundle::api.streamAsyncIcall,
        "production startup left rule memory submission disabled");
    Require(fileHooks==2&&!unreadyFileHooks,"file hooks became active before resource submission was ready");
    Require(!nativeCalls,"startup binding initiated native resource requests");
    if(argc>1) {
      auto bytes=NativeBundle::Array(Bytes{'g','a','m','e','-','p','a','c','k','a','g','e'});
      auto result=NativeBundle::LoadMemory(bytes.Get(),0,true);bytes={};
      Require(result==createRequest&&StartupFixture::streamCalls==1&&!rawMemoryCalls,"rule used the eager Memory decoder instead of the archive stream entry");
      NativeBundle::CollectMemory();Require(std::any_of(roots.begin(),roots.end(),[](const auto& r){return r.second==streamObject;}),"in-flight archive stream root lost");
      requestDone=true;NativeBundle::CollectMemory();
      Require(std::any_of(roots.begin(),roots.end(),[](const auto& r){return r.second==streamObject;}),"stream released at request completion while Bundle still needs seek/read");
      bundleAlive=false;NativeBundle::CollectMemory();Require(roots.empty(),"unloaded Bundle retained its archive stream");
      std::cout<<"PASS rule stream transport: archive entry, readonly seekable bytes, async and lazy Bundle lifetime; no raw Memory decoder\n";return 0;
    }
    std::cout<<"PASS production startup: controls and memory submission ready before file hooks; native requests=0\n";
    return 0;
  } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
