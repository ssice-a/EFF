#pragma once

// Public AssetBundle bindings and file ownership used by the generation
// transaction. Disk preparation never submits a native load; the caller owns
// GC roots and waits for native requests before releasing these file locks.
namespace NativeBundle {
using RuntimeShapes::Ref;
using FileHold=EiemNativeInput::FileHold;
struct Api {
  void *load = nullptr, *done = nullptr, *bundle = nullptr, *assetLoad = nullptr;
  void *result = nullptr, *unload = nullptr;
  void *stream=nullptr,*streamAsync=nullptr,*byteClass=nullptr;
  void *streamClass=nullptr,*streamCtor=nullptr;
  using StreamFn=void*(*)(void*,uint32_t,uint32_t);
  StreamFn streamIcall=nullptr,streamAsyncIcall=nullptr;
} api;
std::atomic<bool> enabled{false};
struct MemoryRequest {Ref request,stream;};
struct MemoryBundle {Ref bundle,stream;};
std::mutex memoryMutex;
std::map<void*,MemoryRequest> memoryRequests;
std::map<void*,MemoryBundle> memoryBundles;
void RetainMemory(void* request,const Ref& stream){
 auto rooted=Ref::Make(request,false);std::lock_guard<std::mutex> lock(memoryMutex);memoryRequests.emplace(request,MemoryRequest{std::move(rooted),stream});
}
void CompleteMemory(void* request,void* bundle){
 std::lock_guard<std::mutex> lock(memoryMutex);
 auto it=memoryRequests.find(request);if(it==memoryRequests.end())return;
 if(bundle)memoryBundles.emplace(bundle,MemoryBundle{Ref::Make(bundle,false),it->second.stream});
 else Log("[ENTRY-RULE-LOAD-FAILED] request=%p archiveStream=1 nativeBundleNull=1",request);
 memoryRequests.erase(it);
}
void CollectMemory(){
 // Public getters can reenter completion observers. Never hold this mutex
 // across a managed call, and retain seekable streams beyond request completion.
 std::map<void*,MemoryRequest> requests;std::map<void*,MemoryBundle> bundles;
 {std::lock_guard<std::mutex> lock(memoryMutex);requests=memoryRequests;bundles=memoryBundles;}
 for(const auto& item:requests){auto request=item.second.request.Get();
  if(RuntimeShapes::Scalar<unsigned char>(api.done,request))CompleteMemory(request,RuntimeShapes::Call(api.bundle,request));
 }
 for(const auto& item:bundles)if(!RuntimeShapes::Alive(item.second.bundle.Get())){
  std::lock_guard<std::mutex> lock(memoryMutex);auto it=memoryBundles.find(item.first);
  if(it!=memoryBundles.end()&&it->second.bundle.handle==item.second.bundle.handle)memoryBundles.erase(it);
 }
}
Ref Array(const EiemNativeInput::Bytes& bytes){
 EiemDirectory::Require(api.byteClass&&il2cpp_array_new,"rule native Byte array binding absent");auto value=Ref::Make(il2cpp_array_new(api.byteClass,bytes.size()),false);EiemDirectory::Require(value.Get(),"rule native Byte array allocation failed");
 size_t length=0;EiemDirectory::Require(Entry::ArrayLength(value.Get(),&length)&&length==bytes.size(),"rule native Byte array layout differs");if(length)memcpy((char*)value.Get()+32,bytes.data(),length);return value;
}
void BindMemoryStream(void* bundle,void** assemblies,size_t count){
 api.streamClass=FindClass("System.IO","MemoryStream",assemblies,count);
 api.streamCtor=Exact(api.streamClass,".ctor",{"System.Byte[]","System.Boolean"},"System.Void",false);
 auto helpers=FindClass("UnityEngine","ManagedStreamHelpers",assemblies,count);
 Require(Exact(helpers,"ManagedStreamRead",{"System.Byte[]","System.Int32","System.Int32","System.IO.Stream","System.IntPtr"},"System.Void",true)&&
             Exact(helpers,"ManagedStreamSeek",{"System.Int64","System.UInt32","System.IO.Stream","System.IntPtr"},"System.Void",true)&&
             Exact(helpers,"ManagedStreamLength",{"System.IO.Stream","System.IntPtr"},"System.Void",true),"rule managed archive stream callbacks absent");
 api.stream=Exact(bundle,"LoadFromStreamInternal",{"System.IO.Stream","System.UInt32","System.UInt32"},"UnityEngine.AssetBundle",true);
 api.streamAsync=Exact(bundle,"LoadFromStreamAsyncInternal",{"System.IO.Stream","System.UInt32","System.UInt32"},"UnityEngine.AssetBundleCreateRequest",true);
 api.streamIcall=api.streamAsyncIcall=nullptr;
 if(il2cpp_resolve_icall){
  if(!api.stream)api.streamIcall=(Api::StreamFn)il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadFromStreamInternal");
  if(!api.streamAsync)api.streamAsyncIcall=(Api::StreamFn)il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadFromStreamAsyncInternal");
 }
}
void* LoadMemory(void* array,uint32_t crc,bool asynchronous){
 Require(api.streamClass&&api.streamCtor&&il2cpp_object_new&&
             (asynchronous?(api.streamAsync||api.streamAsyncIcall):(api.stream||api.streamIcall)),"rule archive stream bindings absent");
 auto stream=Ref::Make(il2cpp_object_new(api.streamClass),false);Require(stream.Get(),"rule MemoryStream allocation failed");
 bool writable=false;void* constructor[]={array,&writable};RuntimeShapes::Call(api.streamCtor,stream.Get(),constructor);
 uint32_t bufferSize=0;auto icall=asynchronous?api.streamAsyncIcall:api.streamIcall;
 void* args[]={stream.Get(),&crc,&bufferSize};
 // The archive stream entry preserves CAB members and external resource data.
 // LoadFromMemory eagerly decompresses through a different game reader.
 auto result=icall?icall(stream.Get(),crc,bufferSize):RuntimeShapes::Call(asynchronous?api.streamAsync:api.stream,nullptr,args);
 if(result){if(asynchronous)RetainMemory(result,stream);else{
  auto rooted=Ref::Make(result,false);std::lock_guard<std::mutex> lock(memoryMutex);memoryBundles.emplace(result,MemoryBundle{std::move(rooted),std::move(stream)});
 }}return result;
}
inline uint64_t Clock() {
  LARGE_INTEGER value{};
  QueryPerformanceCounter(&value);
  return uint64_t(value.QuadPart);
}
inline double Ms(uint64_t start) {
  LARGE_INTEGER frequency{};
  QueryPerformanceFrequency(&frequency);
  return frequency.QuadPart ? double(Clock() - start) * 1000.0 / double(frequency.QuadPart) : 0;
}
void Install(void** assemblies, size_t count) {
  Observe([&] {
    Require(RuntimeShapes::enabled && il2cpp_runtime_invoke && il2cpp_gchandle_new && il2cpp_object_new &&
                il2cpp_string_new && il2cpp_class_get_type && il2cpp_type_get_object,
            "generation public load APIs unavailable");
    auto bundle = FindClass("UnityEngine", "AssetBundle", assemblies, count);
    auto request = FindClass("UnityEngine", "AssetBundleCreateRequest", assemblies, count);
    auto assetRequest = FindClass("UnityEngine", "AssetBundleRequest", assemblies, count);
    auto operation = FindClass("UnityEngine", "AsyncOperation", assemblies, count);
    api.load = Exact(bundle, "LoadFromFileAsync",
                     {"System.String", "System.UInt32", "System.UInt64"},
                     "UnityEngine.AssetBundleCreateRequest", true);
    api.byteClass=FindClass("System","Byte",assemblies,count);
    BindMemoryStream(bundle,assemblies,count);
    api.done = Exact(operation, "get_isDone", {}, "System.Boolean", false);
    api.bundle = Exact(request, "get_assetBundle", {}, "UnityEngine.AssetBundle", false);
    api.assetLoad = Exact(bundle, "LoadAssetAsync_Internal",
                          {"System.String", "System.Type"}, "UnityEngine.AssetBundleRequest", false);
    api.result = Exact(assetRequest, "GetResult", {}, "UnityEngine.Object", false);
    api.unload = Exact(bundle, "Unload", {"System.Boolean"}, "System.Void", false);
    Require(api.load && api.done && api.bundle && api.assetLoad && api.result && api.unload&&
                (api.stream||api.streamIcall)&&(api.streamAsync||api.streamAsyncIcall)&&api.streamClass&&api.streamCtor&&api.byteClass,
            "generation exact public load signatures incomplete");
    enabled = true;
    Log("[NATIVE-BUNDLE-READY] publicAsyncBindings=1 archiveMemoryStream=1 syncStreamIcall=%d asyncStreamIcall=%d diskPreparationNativeCalls=0",int(bool(api.streamIcall)),int(bool(api.streamAsyncIcall)));
  });
}
} // namespace NativeBundle
