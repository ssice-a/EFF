#pragma once
#include "eiem_input_span_view.h"

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
  void *nativeStreamClass=nullptr,*nativeStreamCtor=nullptr;
  using StreamFn=void*(*)(void*,uint32_t,uint32_t);
  StreamFn streamIcall=nullptr,streamAsyncIcall=nullptr;
} api;
std::atomic<bool> enabled{false};
struct MemoryRequest {Ref request,stream;std::shared_ptr<const void> owner;std::shared_ptr<EiemInputSpanView::Cursor> view;std::shared_ptr<const EiemNativeInput::Bytes> bytes;};
struct MemoryBundle {Ref bundle,stream;std::shared_ptr<const void> owner;std::shared_ptr<EiemInputSpanView::Cursor> view;std::shared_ptr<const EiemNativeInput::Bytes> bytes;};
struct MemoryUnload {Ref request;void* bundle=nullptr;std::shared_ptr<uint32_t> identity;};
std::mutex memoryMutex;
std::map<void*,MemoryRequest> memoryRequests;
std::map<void*,MemoryBundle> memoryBundles;
std::map<void*,MemoryUnload> memoryUnloads;
std::atomic<bool> memoryWork{false};
using UnloadFn=void(*)(void*,bool);
using UnloadAsyncFn=void*(*)(void*,bool);
UnloadFn originalUnload=nullptr;
UnloadAsyncFn originalUnloadAsync=nullptr;
using StreamReadFn=void(*)(void*,int32_t,int32_t,void*,void*,void*);
using StreamSeekFn=void(*)(int64_t,uint32_t,void*,void*,void*);
using StreamLengthFn=void(*)(void*,void*,void*);
StreamReadFn originalStreamRead=nullptr;StreamSeekFn originalStreamSeek=nullptr;StreamLengthFn originalStreamLength=nullptr;
std::mutex viewMutex;std::map<void*,std::weak_ptr<EiemInputSpanView::Cursor>> viewStreams;
std::atomic<bool> viewStreamsPresent{false};bool viewCallbacksReady=false;
std::shared_ptr<EiemInputSpanView::Cursor> StreamView(void* stream){
 if(!viewStreamsPresent.load(std::memory_order_relaxed))return {};
 std::lock_guard<std::mutex> lock(viewMutex);auto found=viewStreams.find(stream);if(found==viewStreams.end())return {};
 auto view=found->second.lock();if(!view){viewStreams.erase(found);viewStreamsPresent=!viewStreams.empty();}return view;
}
void StreamRead(void* array,int32_t offset,int32_t count,void* stream,void* result,void* info){
 std::shared_ptr<EiemInputSpanView::Cursor> view;{EiemLoadPerformance::Scope route(EiemLoadPerformance::StreamRoute);view=StreamView(stream);route.hit=bool(view);}if(!view){originalStreamRead(array,offset,count,stream,result,info);return;}
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::ViewRead);performance.hit=true;
 int32_t read=0;try{size_t length=0;Require(result&&offset>=0&&count>=0&&Entry::ArrayLength(array,&length)&&size_t(offset)<=length&&size_t(count)<=length-size_t(offset),"compiled stream Read buffer range differs");
  read=int32_t(view->Read((unsigned char*)array+32+offset,size_t(count)));
  EiemLoadPerformance::Bytes(EiemLoadPerformance::ViewRead,uint64_t(read));
 }catch(const std::exception& error){Log("[NATIVE-VIEW-READ-FAILED] reason=%s",error.what());}
 if(result)std::memcpy(result,&read,sizeof(read));
}
void StreamSeek(int64_t offset,uint32_t origin,void* stream,void* result,void* info){
 std::shared_ptr<EiemInputSpanView::Cursor> view;{EiemLoadPerformance::Scope route(EiemLoadPerformance::StreamRoute);view=StreamView(stream);route.hit=bool(view);}if(!view){originalStreamSeek(offset,origin,stream,result,info);return;}
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::ViewSeek);performance.hit=true;
 int64_t position=-1;try{Require(result,"compiled stream Seek result absent");position=view->Seek(offset,origin);}catch(const std::exception& error){Log("[NATIVE-VIEW-SEEK-FAILED] reason=%s",error.what());}
 if(result)std::memcpy(result,&position,sizeof(position));
}
void StreamLength(void* stream,void* result,void* info){
 std::shared_ptr<EiemInputSpanView::Cursor> view;{EiemLoadPerformance::Scope route(EiemLoadPerformance::StreamRoute);view=StreamView(stream);route.hit=bool(view);}if(!view){originalStreamLength(stream,result,info);return;}
 int64_t length=view->Length();if(result)std::memcpy(result,&length,sizeof(length));
}
std::shared_ptr<uint32_t> MemoryIdentity(void* bundle){
 std::lock_guard<std::mutex> lock(memoryMutex);auto found=memoryBundles.find(bundle);
 return found==memoryBundles.end()?std::shared_ptr<uint32_t>{}:found->second.bundle.handle;
}
void ReleasedMemory(void* bundle,const std::shared_ptr<uint32_t>& identity){
 if(!identity)return;std::lock_guard<std::mutex> lock(memoryMutex);auto found=memoryBundles.find(bundle);
 if(found!=memoryBundles.end()&&found->second.bundle.handle==identity)memoryBundles.erase(found);
}
void Unload(void* bundle,bool destroy){
 auto identity=MemoryIdentity(bundle);originalUnload(bundle,destroy);ReleasedMemory(bundle,identity);
}
void* UnloadAsync(void* bundle,bool destroy){
 auto identity=MemoryIdentity(bundle);auto request=originalUnloadAsync(bundle,destroy);
 if(identity&&request){auto rooted=Ref::Make(request,false);std::lock_guard<std::mutex> lock(memoryMutex);
  memoryUnloads.emplace(request,MemoryUnload{std::move(rooted),bundle,std::move(identity)});memoryWork=true;
 }return request;
}
void RetainMemory(void* request,const Ref& stream,std::shared_ptr<const void> owner={},std::shared_ptr<EiemInputSpanView::Cursor> view={}){
 auto rooted=Ref::Make(request,false);std::lock_guard<std::mutex> lock(memoryMutex);memoryRequests.emplace(request,MemoryRequest{std::move(rooted),stream,std::move(owner),std::move(view),{}});memoryWork=true;
}
void CompleteMemory(void* request,void* bundle){
 std::lock_guard<std::mutex> lock(memoryMutex);
 auto it=memoryRequests.find(request);if(it==memoryRequests.end())return;
 if(bundle)memoryBundles.emplace(bundle,MemoryBundle{Ref::Make(bundle,false),it->second.stream,it->second.owner,it->second.view,it->second.bytes});
 else Log("[ENTRY-RULE-LOAD-FAILED] request=%p archiveStream=1 nativeBundleNull=1",request);
 memoryRequests.erase(it);
 memoryWork=!memoryRequests.empty()||!memoryUnloads.empty();
}
void CollectMemory(){
#if defined(EIEM_LOAD_PERF)
 static ULONGLONG reported=0;auto now=GetTickCount64();if(now-reported>=5000){reported=now;size_t requests=0,bundles=0,unloads=0,views=0;{std::lock_guard<std::mutex> lock(memoryMutex);requests=memoryRequests.size();bundles=memoryBundles.size();unloads=memoryUnloads.size();}{std::lock_guard<std::mutex> lock(viewMutex);views=viewStreams.size();}Log("[DEBUG-loadperf] nativeLifetime=1 requests=%zu bundles=%zu unloads=%zu viewStreams=%zu consumersRetained=1",requests,bundles,unloads,views);}
#endif
 if(!memoryWork.load(std::memory_order_relaxed))return;
 EiemLoadPerformance::Scope performance(EiemLoadPerformance::StreamLifetime);
 // Public getters can reenter completion observers. Never hold this mutex
 // across a managed call, and retain seekable streams beyond request completion.
 struct Scratch {bool collecting=false;std::vector<std::pair<void*,MemoryRequest>> requests;std::vector<std::pair<void*,MemoryUnload>> unloads;};
 static thread_local Scratch scratch;if(scratch.collecting)return;
 struct Guard {Scratch& value;explicit Guard(Scratch& v):value(v){value.collecting=true;}~Guard(){value.requests.clear();value.unloads.clear();value.collecting=false;}}guard(scratch);
 auto& requests=scratch.requests;auto& unloads=scratch.unloads;
 {std::lock_guard<std::mutex> lock(memoryMutex);requests.reserve(memoryRequests.size());unloads.reserve(memoryUnloads.size());
  for(const auto& item:memoryRequests)requests.push_back(item);for(const auto& item:memoryUnloads)unloads.push_back(item);}
 for(const auto& item:requests){auto request=item.second.request.Get();
  if(RuntimeShapes::Scalar<unsigned char>(api.done,request))CompleteMemory(request,RuntimeShapes::Call(api.bundle,request));
 }
 for(const auto& item:unloads)if(RuntimeShapes::Scalar<unsigned char>(api.done,item.second.request.Get())){
  ReleasedMemory(item.second.bundle,item.second.identity);
  std::lock_guard<std::mutex> lock(memoryMutex);auto it=memoryUnloads.find(item.first);
  if(it!=memoryUnloads.end()&&it->second.request.handle==item.second.request.handle)memoryUnloads.erase(it);
 }
 {std::lock_guard<std::mutex> lock(memoryMutex);memoryWork=!memoryRequests.empty()||!memoryUnloads.empty();}
}
Ref Array(const unsigned char* data,size_t size){
 EiemDirectory::Require(api.byteClass&&il2cpp_array_new&&(!size||data),"rule native Byte array binding/data absent");auto value=Ref::Make(il2cpp_array_new(api.byteClass,size),false);EiemDirectory::Require(value.Get(),"rule native Byte array allocation failed");
 size_t length=0;EiemDirectory::Require(Entry::ArrayLength(value.Get(),&length)&&length==size,"rule native Byte array layout differs");if(length)memcpy((char*)value.Get()+32,data,length);return value;
}
Ref Array(const EiemNativeInput::Bytes& bytes){return Array(bytes.data(),bytes.size());}
void BindMemoryStream(void* bundle,void** assemblies,size_t count){
 api.streamClass=FindClass("System.IO","MemoryStream",assemblies,count);
 api.streamCtor=Exact(api.streamClass,".ctor",{"System.Byte[]","System.Boolean"},"System.Void",false);
 api.nativeStreamClass=FindClass("System.IO","UnmanagedMemoryStream",assemblies,count);
 api.nativeStreamCtor=Exact(api.nativeStreamClass,".ctor",{"System.Byte*","System.Int64"},"System.Void",false);
 if(!Exact(api.nativeStreamClass,"Read",{"System.Byte[]","System.Int32","System.Int32"},"System.Int32",false)||
    !Exact(api.nativeStreamClass,"Seek",{"System.Int64","System.IO.SeekOrigin"},"System.Int64",false)||
    !Exact(api.nativeStreamClass,"get_Length",{},"System.Int64",false))api.nativeStreamCtor=nullptr;
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
void* SubmitStream(Ref stream,std::shared_ptr<const void> owner,uint32_t crc,bool asynchronous,std::shared_ptr<EiemInputSpanView::Cursor> view={}){
 Require(stream.Get()&&
             (asynchronous?(api.streamAsync||api.streamAsyncIcall):(api.stream||api.streamIcall)),"rule archive stream bindings absent");
 uint32_t bufferSize=0;auto icall=asynchronous?api.streamAsyncIcall:api.streamIcall;
 void* args[]={stream.Get(),&crc,&bufferSize};
 // The archive stream entry preserves CAB members and external resource data.
 // LoadFromMemory eagerly decompresses through a different game reader.
 auto result=icall?icall(stream.Get(),crc,bufferSize):RuntimeShapes::Call(asynchronous?api.streamAsync:api.stream,nullptr,args);
 if(result){if(asynchronous)RetainMemory(result,stream,std::move(owner),std::move(view));else{
  auto rooted=Ref::Make(result,false);std::lock_guard<std::mutex> lock(memoryMutex);memoryBundles.emplace(result,MemoryBundle{std::move(rooted),std::move(stream),std::move(owner),std::move(view),{}});
 }}return result;
}
void* LoadMemory(void* array,uint32_t crc,bool asynchronous){
 Require(api.streamClass&&api.streamCtor&&il2cpp_object_new,"rule MemoryStream bindings absent");
 auto stream=Ref::Make(il2cpp_object_new(api.streamClass),false);Require(stream.Get(),"rule MemoryStream allocation failed");
 bool writable=false;void* constructor[]={array,&writable};RuntimeShapes::Call(api.streamCtor,stream.Get(),constructor);
 return SubmitStream(std::move(stream),{},crc,asynchronous);
}
void* LoadView(const std::shared_ptr<const EiemInputSpanView::View>& view,uint32_t crc,bool asynchronous){
 Require(viewCallbacksReady&&view&&view->Size()&&api.streamClass&&api.streamCtor&&il2cpp_object_new,"compiled native view stream bindings absent");
 // The facade is an empty readable MemoryStream. A missed callback returns
 // EOF, rather than exposing fabricated native pointers or a giant allocation.
 auto empty=Array(nullptr,0);auto stream=Ref::Make(il2cpp_object_new(api.streamClass),false);Require(stream.Get(),"compiled view stream allocation failed");
 bool writable=false;void* args[]={empty.Get(),&writable};RuntimeShapes::Call(api.streamCtor,stream.Get(),args);
 auto cursor=std::make_shared<EiemInputSpanView::Cursor>(view);
 {std::lock_guard<std::mutex> lock(viewMutex);for(auto it=viewStreams.begin();it!=viewStreams.end();)if(it->second.expired())it=viewStreams.erase(it);else ++it;
  Require(viewStreams.emplace(stream.Get(),cursor).second,"compiled view stream pointer reused before retirement");viewStreamsPresent=true;
 }
 auto result=SubmitStream(std::move(stream),{},crc,asynchronous,std::move(cursor));return result;
}
void* LoadBytes(const unsigned char* data,size_t size,std::shared_ptr<const void> owner,uint32_t crc,bool asynchronous){
 Require(data&&owner&&size&&size<=INT64_MAX,"rule native stream byte extent differs");
 if(!api.nativeStreamCtor){auto array=Array(data,size);return LoadMemory(array.Get(),crc,asynchronous);}
 auto stream=Ref::Make(il2cpp_object_new(api.nativeStreamClass),false);Require(stream.Get(),"rule native stream allocation failed");
 int64_t length=int64_t(size);
 // IL2CPP passes pointer parameters directly through the argument slot;
 // value parameters such as Int64 are dereferenced by the invoker. Passing
 // &pointer here creates a valid stream over the stack variable, not the input.
 void* constructor[]={const_cast<unsigned char*>(data),&length};
 RuntimeShapes::Call(api.nativeStreamCtor,stream.Get(),constructor);
 // The public two-argument constructor is read-only. The immutable backing
 // survives both async completion and subsequent Bundle seek/read operations.
 return SubmitStream(std::move(stream),std::move(owner),crc,asynchronous);
}
void* LoadBytes(const std::shared_ptr<const EiemNativeInput::Bytes>& bytes,uint32_t crc,bool asynchronous){
 Require(bytes!=nullptr,"rule native stream byte owner absent");auto result=LoadBytes(bytes->data(),bytes->size(),std::shared_ptr<const void>(bytes),crc,asynchronous);if(result){std::lock_guard<std::mutex> lock(memoryMutex);if(asynchronous){auto it=memoryRequests.find(result);if(it!=memoryRequests.end())it->second.bytes=bytes;}else{auto it=memoryBundles.find(result);if(it!=memoryBundles.end())it->second.bytes=bytes;}}return result;
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
    auto bindUnload=[](const char* name,void* detour,void** original){
     auto address=il2cpp_resolve_icall?il2cpp_resolve_icall(name):nullptr;
     Require(address&&MH_CreateHook(address,detour,original)==MH_OK&&MH_EnableHook(address)==MH_OK,"native Bundle release lifecycle hook absent");
    };
    bindUnload("UnityEngine.AssetBundle::Unload(System.Boolean)",(void*)Unload,(void**)&originalUnload);
    bindUnload("UnityEngine.AssetBundle::UnloadAsync(System.Boolean)",(void*)UnloadAsync,(void**)&originalUnloadAsync);
    auto helpers=FindClass("UnityEngine","ManagedStreamHelpers",assemblies,count);
    viewCallbacksReady=Entry::Install(helpers,"ManagedStreamRead",{"System.Byte[]","System.Int32","System.Int32","System.IO.Stream","System.IntPtr"},"System.Void",true,(void*)StreamRead,(void**)&originalStreamRead)&&
     Entry::Install(helpers,"ManagedStreamSeek",{"System.Int64","System.UInt32","System.IO.Stream","System.IntPtr"},"System.Void",true,(void*)StreamSeek,(void**)&originalStreamSeek)&&
     Entry::Install(helpers,"ManagedStreamLength",{"System.IO.Stream","System.IntPtr"},"System.Void",true,(void*)StreamLength,(void**)&originalStreamLength);
    Require(viewCallbacksReady,"compiled native view stream callbacks unavailable");
    enabled = true;
    Log("[NATIVE-BUNDLE-READY] publicAsyncBindings=1 archiveMemoryStream=1 nativeByteStream=%d syncStreamIcall=%d asyncStreamIcall=%d releaseEvents=1 idleBundleQueries=0 diskPreparationNativeCalls=0",int(bool(api.nativeStreamCtor)),int(bool(api.streamIcall)),int(bool(api.streamAsyncIcall)));
  });
}
} // namespace NativeBundle
