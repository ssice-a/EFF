#pragma once
#include "eiem_shape_snapshot.h"
#include "eiem_runtime_shape_identity.h"
#include "eiem_entry_controls.h"

// Main Meshes retain native blend shapes. Only the capture consumer receives
// a local-space, immutable, shape-evaluated copy through its Mesh getter.
// No private native layout, shader patch, forced completion, or count override.
namespace RuntimeShapes {
using namespace EiemShapeSnapshot;
using namespace RuntimeControls;
std::atomic<bool> managedAlive{true};
struct Ref {
  std::shared_ptr<uint32_t> handle;
  void* Get() const { return managedAlive && handle ? il2cpp_gchandle_get_target(*handle) : nullptr; }
  static Ref Make(void* object, bool weak) {
    Ref r;
    if (!object) return r;
    auto h = weak ? weakHandle(object, false) : il2cpp_gchandle_new(object, false);
    Require(h != 0, "runtime shape GC handle failed");
    r.handle = std::shared_ptr<uint32_t>(new uint32_t(h), [](uint32_t* p) {
      if (managedAlive && il2cpp_gchandle_free) il2cpp_gchandle_free(*p);
      delete p;
    });
    return r;
  }
  static inline uint32_t (*weakHandle)(void*, bool) = nullptr;
};
struct Cached {
  Ref mesh;
  Geometry base;
  std::vector<Channel> channels;
};
struct Bound {
  Ref renderer, mesh;
  std::string marker;
  std::map<std::string,int> indices;
  bool nativeEnabled = true;
};
struct Snapshot { Ref mesh; };
struct CaptureScope {
  CaptureScope* previous = nullptr;
  std::map<void*,bool> ready;
  std::map<void*,void*> supplied;
  std::map<void*,EiemCaptureSkin::Palette> capturePalettes;
  std::vector<Snapshot> snapshots;
  unsigned assignments = 0;
  CaptureScope();
  ~CaptureScope();
};
struct Group { Ref owner, entity; std::vector<Snapshot> snapshots; };
struct Api {
  void *main = nullptr, *alive = nullptr, *name = nullptr, *mesh = nullptr, *vertices = nullptr;
  void *normals = nullptr, *tangents = nullptr, *readable = nullptr, *vertexCount = nullptr;
  void *shapeCount = nullptr, *shapeName = nullptr, *frameCount = nullptr, *frameWeight = nullptr, *frameVertices = nullptr;
  void *getWeight = nullptr, *setWeight = nullptr, *instantiate = nullptr, *clear = nullptr;
  void *setChannel = nullptr, *recalculateBounds = nullptr, *destroy = nullptr;
  void *frame = nullptr, *elapsed = nullptr;
  void *getEnabled = nullptr, *modelRenderers = nullptr, *rendererType = nullptr;
  void* vec3Class = nullptr;
  void (*clearIcall)(void*) = nullptr;
  void *bindPoses = nullptr, *boneWeights = nullptr, *indices = nullptr;
  void *bones = nullptr, *rootBone = nullptr, *transform = nullptr;
  void *localToWorld = nullptr, *worldToLocal = nullptr, *skinningRoot = nullptr;
  void* transformClass = nullptr;
  void *attributeCount = nullptr, *attributeDescriptor = nullptr, *bonesPerVertex = nullptr, *filterMesh = nullptr;
} api;
std::atomic<bool> enabled{false};
std::atomic<unsigned> boundCount{0};
unsigned packageGeneration = 0, checkedGeneration = 0;
bool registrationPaused = false;
bool visibilityWriting = false;
using EnabledSetterFn = void (*)(void*,bool);
EnabledSetterFn originalEnabledSetter = nullptr;
std::map<void*,Bound> bound;
std::map<void*,Cached> cached;
std::vector<Group> groups;
thread_local CaptureScope* scope = nullptr;
thread_local bool building = false;
int lastFrame = -1;
unsigned snapshotSequence = 0;
size_t liveSnapshots = 0;
using CaptureFn = void* (*)(void*, void*, int, void*);
using ReleaseFn = void (*)(void*, void*, void*);
using TickFn = void (*)(void*, void*);
using InitFn = void (*)(void*, void*, void*, void*, bool, void*);
using MeshGetterFn = void* (*)(void*);
CaptureFn originalCapture = nullptr;
ReleaseFn originalRelease = nullptr;
TickFn originalTick = nullptr, originalReset = nullptr, originalFrameTick = nullptr;
InitFn originalInit = nullptr;
MeshGetterFn originalMeshGetter = nullptr;
using ShapeCountFn = int (*)(void*);
using FilterSetterFn = void (*)(void*,void*);
using MatrixRequestFn = bool (*)(void*,void*,int);
ShapeCountFn originalShapeCount = nullptr;
FilterSetterFn originalFilterSetter = nullptr;
MatrixRequestFn originalMatrixRequest = nullptr;
bool traceBoundaries = false;
thread_local bool observing = false;
unsigned boundarySamples = 0;
#if defined(EIEM_SHAPE_CAPTURE_GFXTRACE)
RENDERDOC_API_1_6_0* gpuCaptureApi = nullptr;
bool gpuCaptureEnabled = false, gpuCaptureTriggered = false, gpuCaptureReported = false;
unsigned gpuCaptureAttempts = 0, gpuCaptureFirst = 0;
void RequestGpuCapture(unsigned assignments) {
  if (!gpuCaptureEnabled || gpuCaptureTriggered || !assignments || gpuCaptureAttempts>=8) return;
  try {
    ++gpuCaptureAttempts;
    if (!gpuCaptureApi) {
      auto module=GetModuleHandleW(L"gfxtrace.dll");
      auto get=module?(pRENDERDOC_GetAPI)GetProcAddress(module,"RENDERDOC_GetAPI"):nullptr;
      Require(get && get(eRENDERDOC_API_Version_1_6_0,(void**)&gpuCaptureApi)==1 && gpuCaptureApi,"GfxTrace public API not loaded");
      wchar_t path[32768]{}; GetModuleFileNameW(module,path,32768);
      Log("[DEBUG-shape-v24-GPU-MODULE] path=%s",std::filesystem::path(path).u8string().c_str());
    }
    Require(gpuCaptureApi->TriggerCapture && gpuCaptureApi->GetNumCaptures && gpuCaptureApi->GetCapture &&
            gpuCaptureApi->SetCaptureFilePathTemplate && gpuCaptureApi->IsFrameCapturing,"GfxTrace public capture API incomplete");
    auto prefix=gameDirectory/"plugin/resource-input-diagnostics"/
      ("body-afterimage-v24-pid"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    gpuCaptureApi->SetCaptureFilePathTemplate(prefix.u8string().c_str());
    gpuCaptureFirst=gpuCaptureApi->GetNumCaptures();
    bool captureInProgress=gpuCaptureApi->IsFrameCapturing()!=0;
    if (!captureInProgress) gpuCaptureApi->TriggerCapture();
    gpuCaptureTriggered=true;
    Log("[DEBUG-shape-v24-GPU-REQUEST] assignments=%u alreadyActive=%d captureIndex=%u prefix=%s oneShot=1 nativeCapturePreserved=1",
        assignments,int(captureInProgress),gpuCaptureFirst,prefix.u8string().c_str());
  } catch(const std::exception& e) { Log("[DEBUG-shape-v24-GPU-GAP] reason=%s shapeBridgePreserved=1",e.what()); }
}
void PollGpuCapture() {
  if (!gpuCaptureTriggered || gpuCaptureReported || !gpuCaptureApi || gpuCaptureApi->GetNumCaptures()<=gpuCaptureFirst) return;
  try {
    uint32_t length=0;uint64_t timestamp=0;
    Require(gpuCaptureApi->GetCapture(gpuCaptureFirst,nullptr,&length,&timestamp)==1 && length>0 && length<32768,"GfxTrace saved capture metadata");
    std::vector<char> path(size_t(length)+1,0);++length;
    Require(gpuCaptureApi->GetCapture(gpuCaptureFirst,path.data(),&length,&timestamp)==1 && length<=path.size(),"GfxTrace saved capture path");
    Log("[DEBUG-shape-v24-GPU-SAVED] index=%u path=%s timestamp=%llu exists=%d",
        gpuCaptureFirst,path.data(),(unsigned long long)timestamp,int(std::filesystem::is_regular_file(std::filesystem::u8path(path.data()))));
    gpuCaptureReported=true;
  } catch(const std::exception& e) { Log("[DEBUG-shape-v24-GPU-GAP] phase=saved reason=%s",e.what()); }
}
#else
void RequestGpuCapture(unsigned) {}
void PollGpuCapture() {}
#endif
struct ObservationGuard {
  ObservationGuard() { observing=true; }
  ~ObservationGuard() { observing=false; }
};

void* Call(void* method, void* self = nullptr, void** arguments = nullptr) {
  Require(method && il2cpp_runtime_invoke, "runtime shape method missing");
  void* exception = nullptr;
  auto value = il2cpp_runtime_invoke(method,self,arguments,&exception);
  Require(!exception, "runtime shape managed call failed");
  return value;
}
template <class T> T Scalar(void* method, void* self = nullptr, void** arguments = nullptr) {
  auto boxed = Call(method,self,arguments);
  T result{};
  Require(boxed && CopyRaw(il2cpp_object_unbox(boxed),&result,sizeof(result)), "runtime shape scalar result");
  return result;
}
bool Main() { return Scalar<unsigned char>(api.main) != 0; }
bool Alive(void* object) {
  if (!object) return false;
  void* arguments[] = {object};
  return Scalar<unsigned char>(api.alive,nullptr,arguments) != 0;
}
template <class V> std::vector<V> Array(void* object, size_t expected) {
  size_t length = 0;
  Require(ArrayLength(object,&length) && length == expected, "runtime shape array size");
  SkinningObservation::ArrayElementSize(object,sizeof(V));
  std::vector<V> result(length);
  Require(!length || CopyRaw((char*)object+32,result.data(),length*sizeof(V)), "runtime shape array data");
  return result;
}
void* VectorArray(size_t count) {
  auto value = il2cpp_array_new(api.vec3Class,count);
  Require(value != nullptr, "runtime shape delta array allocation");
  return value;
}
void Retire(std::vector<Snapshot>& snapshots) {
  for (auto& snapshot : snapshots) {
    auto mesh = snapshot.mesh.Get();
    if (Alive(mesh)) { void* args[] = {mesh}; Call(api.destroy,nullptr,args); }
    if (liveSnapshots) --liveSnapshots;
  }
  snapshots.clear();
}
CaptureScope::CaptureScope() : previous(scope) { scope = this; }
CaptureScope::~CaptureScope() {
  scope = previous;
  try { if (!snapshots.empty()) Retire(snapshots); }
  catch (const std::exception& e) { Log("[SHAPE-CAPTURE-GAP] retire=%s",e.what()); }
}
void Finished(void* renderer, bool ready) {
  try { if (enabled && scope && !building && renderer) scope->ready[renderer] = ready; }
  catch (const std::exception& e) { Log("[SHAPE-CAPTURE-GAP] readiness=%s",e.what()); }
}
// Registration/ownership checks always inspect the original Renderer Mesh,
// even when Capture's scoped getter is supplying an evaluated snapshot.
void* SourceMesh(void* renderer) { return originalMeshGetter?originalMeshGetter(renderer):Call(api.mesh,renderer); }
const Spec* Match(void* mesh) {
  if (!mesh) return nullptr;
  auto marker = String(Call(api.name,mesh));
  auto it = specs.find(marker);
  return it == specs.end() ? nullptr : &it->second;
}
void Bind(void* renderer, void* mesh, const Spec& spec) {
  auto existing = bound.find(renderer);
  if (existing != bound.end() && existing->second.renderer.Get() == renderer && existing->second.mesh.Get() == mesh) return;
  Require(Alive(renderer) && Alive(mesh) && Scalar<int>(api.vertexCount,mesh) == int(spec.vertices) &&
          Scalar<int>(api.shapeCount,mesh) == int(spec.channels.size()), "runtime shape provenance/layout mismatch");
  // A managed Mesh wrapper can be collected while its native Renderer still
  // uses the asset. Keep that wrapper until this Renderer binding retires;
  // native validity is still checked independently on the main thread.
  Bound b{Ref::Make(renderer,true),Ref::Make(mesh,false),spec.marker,{}};
  if (spec.visible) b.nativeEnabled=Scalar<unsigned char>(api.getEnabled,renderer)!=0;
  for (size_t i=0;i<spec.channels.size();++i) {
    int index = int(i); void* args[] = {&index};
    auto name = String(Call(api.shapeName,mesh,args));
    Require(name == spec.channels[i], "runtime shape authored channel order mismatch");
    b.indices.emplace(name,index);
  }
  bound[renderer] = std::move(b);
  boundCount.store(unsigned(bound.size()));
  Log("[SHAPE-RUNTIME-BIND] renderer=%p mesh=%p part=%s vertices=%u channels=%zu nativeShapesRetained=1",
      renderer,mesh,spec.part.c_str(),spec.vertices,spec.channels.size());
}
Cached& Read(void* mesh, const Spec& spec) {
  auto existing = cached.find(mesh);
  if (existing != cached.end() && existing->second.mesh.Get() == mesh) return existing->second;
  Require(Scalar<unsigned char>(api.readable,mesh), "runtime shape source not readable/cache limit");
  Cached c; c.mesh = Ref::Make(mesh,true);
  c.base.positions = Array<Vec3>(Call(api.vertices,mesh),spec.vertices);
  c.base.normals = Array<Vec3>(Call(api.normals,mesh),spec.vertices);
  c.base.tangents = Array<Vec4>(Call(api.tangents,mesh),spec.vertices);
  for (size_t channelIndex=0;channelIndex<spec.channels.size();++channelIndex) {
    int index=int(channelIndex); void* indexArgs[] = {&index};
    int frames = Scalar<int>(api.frameCount,mesh,indexArgs);
    Require(frames > 0, "runtime shape native frame count");
    Channel channel{spec.channels[channelIndex],{}};
    for (int f=0;f<frames;++f) {
      void* weightArgs[] = {&index,&f};
      Frame frame; frame.weight = Scalar<float>(api.frameWeight,mesh,weightArgs);
      // Strong handles cover the whole managed call/readback sequence.
      auto positions = Ref::Make(VectorArray(spec.vertices),false);
      auto normals = Ref::Make(VectorArray(spec.vertices),false);
      auto tangents = Ref::Make(VectorArray(spec.vertices),false);
      void* args[] = {&index,&f,positions.Get(),normals.Get(),tangents.Get()};
      Call(api.frameVertices,mesh,args);
      frame.positions = Array<Vec3>(positions.Get(),spec.vertices);
      frame.normals = Array<Vec3>(normals.Get(),spec.vertices);
      frame.tangents = Array<Vec3>(tangents.Get(),spec.vertices);
      channel.frames.push_back(std::move(frame));
    }
    c.channels.push_back(std::move(channel));
  }
  Validate(c.base,c.channels);
  return cached[mesh] = std::move(c);
}
void SetChannel(void* mesh, int channel, int dimension, const void* values, int length) {
  int format = 0, start = 0, flags = 0; // Float32; normal notifications/validation.
  void* pointer = const_cast<void*>(values);
  void* args[] = {&channel,&format,&dimension,&pointer,&length,&start,&length,&flags};
  Call(api.setChannel,mesh,args);
}
// Bounded probes at the native gate and actual consumer assignment. They
// preserve every argument/result; eligibility observation alone does not
// prove that Capture's later blendShapeCount branch accepted the snapshot.
bool Supplied(void* mesh) {
  if (!scope) return false;
  for (const auto& item:scope->supplied) if (item.second==mesh) return true;
  return false;
}
void ObserveLayout(void* mesh,const char* phase) {
  if (!traceBoundaries || snapshotSequence>=3 || !api.attributeCount || !api.attributeDescriptor || !api.bonesPerVertex) return;
  try {
    ObservationGuard guard;
    int attributes=Scalar<int>(api.attributeCount,mesh);
    Require(attributes>=0 && attributes<=32,"capture layout attribute count");
    Log("[DEBUG-shape-v20-LAYOUT] seq=%u phase=%s mesh=%p shapes=%d boneEncoding=%d attributes=%d",
        snapshotSequence+1,phase,mesh,Scalar<int>(api.shapeCount,mesh),Scalar<int>(api.bonesPerVertex,mesh),attributes);
    for(int index=0;index<attributes;++index) {
      void* args[]={&index}; auto boxed=Call(api.attributeDescriptor,mesh,args);
      auto type=boxed?il2cpp_object_get_class(boxed):nullptr; uint32_t alignment=0;
      Require(type && il2cpp_class_value_size(type,&alignment)==16,"capture layout descriptor ABI");
      int descriptor[4]{};
      Require(CopyRaw(il2cpp_object_unbox(boxed),descriptor,sizeof(descriptor)),"capture layout descriptor read");
      Log("[DEBUG-shape-v20-ATTRIBUTE] seq=%u phase=%s attr=%d format=%d dimension=%d stream=%d",
          snapshotSequence+1,phase,descriptor[0],descriptor[1],descriptor[2],descriptor[3]);
    }
  } catch(const std::exception& e) { Log("[DEBUG-shape-v20-GAP] phase=%s reason=%s nativeCallUnaffected=1",phase,e.what()); }
}
int ShapeCount(void* mesh) {
  void* caller=_ReturnAddress();
  int result=originalShapeCount(mesh);
  if (!enabled || !traceBoundaries || !scope || building || observing || boundarySamples>=128 || !mesh) return result;
  try {
    ObservationGuard guard;
    auto name=api.name?String(Call(api.name,mesh)):std::string();
    if (!Supplied(mesh) && name.find("__EFF_SHAPE_")==std::string::npos) return result;
    ++boundarySamples;
    Log("[DEBUG-shape-v20-GATE] mesh=%p name=%s actualCount=%d suppliedSnapshot=%d callerRva=%llX originalResultPreserved=1",
        mesh,name.c_str(),result,int(Supplied(mesh)),(unsigned long long)((uintptr_t)caller-(uintptr_t)hGA));
  } catch(const std::exception& e) { Log("[DEBUG-shape-v20-GAP] phase=count reason=%s nativeCallUnaffected=1",e.what()); }
  return result;
}
void SetFilterMesh(void* filter,void* mesh) {
  void* caller=_ReturnAddress();
  originalFilterSetter(filter,mesh);
  if (scope && Supplied(mesh)) ++scope->assignments;
  if (!enabled || !traceBoundaries || !scope || observing || boundarySamples>=128 || !Supplied(mesh)) return;
  try {
    ObservationGuard guard; ++boundarySamples;
    auto actual=api.filterMesh?Call(api.filterMesh,filter):nullptr;
    Log("[DEBUG-shape-v20-ASSIGN] filter=%p submitted=%p readback=%p same=%d callerRva=%llX originalCallPreserved=1",
        filter,mesh,actual,int(actual==mesh),(unsigned long long)((uintptr_t)caller-(uintptr_t)hGA));
  } catch(const std::exception& e) { Log("[DEBUG-shape-v20-GAP] phase=assignment reason=%s nativeCallUnaffected=1",e.what()); }
}
bool RequestMatrices(void* renderer,void* pointer,int size) {
  const bool nativeResult=originalMatrixRequest(renderer,pointer,size);
  bool result=nativeResult;
  if (!enabled || !scope || building || observing || !scope->supplied.count(renderer)) return result;
  // An accepted native request owns this destination until its own completion.
  // A refused request registers no pending work. Supply only the complete
  // frozen palette prepared for this renderer's supplied snapshot; retain the
  // real Finished result and never keep or write a private renderer pointer.
  auto palette=scope->capturePalettes.find(renderer);
  auto ready=scope->ready.find(renderer);
  if (!nativeResult && palette!=scope->capturePalettes.end() && ready!=scope->ready.end() && ready->second) {
    result=EiemCaptureSkin::Deliver(palette->second,pointer,size);
    Log("[SHAPE-CAPTURE-MATRICES] renderer=%p bytes=%d matrices=%zu nativeAccepted=0 delivered=%d synchronous=1 callerBufferOnly=1",
        renderer,size,palette->second.size(),int(result));
    if (result && traceBoundaries && snapshotSequence<=3) try {
      const auto path=gameDirectory/"plugin/resource-input-diagnostics"/
        ("capture-palette-v24-pid"+std::to_string(GetCurrentProcessId())+"-seq"+std::to_string(snapshotSequence)+".bin");
      std::ofstream file(path,std::ios::binary|std::ios::trunc);
      file.write(reinterpret_cast<const char*>(palette->second.data()),size);
      Require(bool(file),"capture palette evidence write failed");
      Log("[DEBUG-shape-v24-PALETTE] path=%s bytes=%d",path.u8string().c_str(),size);
    } catch(const std::exception& e) { Log("[DEBUG-shape-v24-GAP] phase=palette-evidence reason=%s delivered=%d",e.what(),int(result)); }
  }
  if (traceBoundaries && boundarySamples<128) {
    ++boundarySamples;
    Log("[DEBUG-shape-v24-MATRICES] renderer=%p destination=%p bytes=%d nativeAccepted=%d returned=%d nativeRequestForwarded=1",
        renderer,pointer,size,int(nativeResult),int(result));
  }
  return result;
}
EiemCaptureSkin::Palette CapturePaletteFor(void* renderer,void* mesh) {
  Require(Main(),"capture skin palette must be read on Unity thread");
  auto bonesArray=Ref::Make(Call(api.bones,renderer),false);
  size_t count=0;
  Require(ArrayLength(bonesArray.Get(),&count) && count>0 && count<=256,"capture renderer bone slot count");
  // Transform[] is a reference array: class_value_size describes Transform's
  // object data, not its pointer-sized array elements.
  Require(il2cpp_class_get_element_class(il2cpp_object_get_class(bonesArray.Get()))==api.transformClass,
          "capture renderer bone array type");
  std::vector<void*> bones(count);
  Require(CopyRaw((char*)bonesArray.Get()+32,bones.data(),count*sizeof(void*)),"capture renderer bone array read");
  auto posesArray=Ref::Make(Call(api.bindPoses,mesh),false);
  auto poses=Array<Matrix4x4>(posesArray.Get(),count);
  auto transform=Call(api.transform,renderer);
  Require(Alive(transform),"capture renderer Transform absent");
  auto rootBone=Call(api.rootBone,renderer);
  auto captureRoot=Call(api.skinningRoot,renderer);
  const char* frameSource="skinningRoot";
  // Match the actual native Capture parent selection. Renderer/Component
  // matrices can contain the source Mesh's axis conversion; Capture resets
  // its new MeshFilter under skinningRoot, or rootBone when that is absent.
  if (!Alive(captureRoot)) {captureRoot=Alive(rootBone)?rootBone:nullptr;frameSource=captureRoot?"rootBone":"world";}
  auto root=rootBone;
  if (!root) root=transform; // Public rootBone contract permits a null root.
  Require(Alive(root),"capture root Transform expired");
  auto matrix=[&](void* method,void* object) {
    auto value=Scalar<Matrix4x4>(method,object);
    EiemCaptureSkin::Matrix result{};
    static_assert(sizeof(result)==sizeof(value),"public Unity Matrix4x4 extent");
    std::memcpy(result.m,value.m,sizeof(result.m));
    return result;
  };
  const auto rootWorld=matrix(api.localToWorld,root);
  const auto inverse=captureRoot?matrix(api.worldToLocal,captureRoot):EiemSkinProbe::Identity();
  if (traceBoundaries && snapshotSequence<3) {
    const auto rows=EiemCaptureSkin::RowMatrix(inverse);
    Log("[DEBUG-shape-v24-FRAME] renderer=%p inverseSource=%s parent=%p rootBone=%p rows=%.9g,%.9g,%.9g,%.9g;%.9g,%.9g,%.9g,%.9g;%.9g,%.9g,%.9g,%.9g",
        renderer,frameSource,captureRoot,rootBone,rows[0],rows[1],rows[2],rows[3],rows[4],rows[5],rows[6],rows[7],rows[8],rows[9],rows[10],rows[11]);
  }
  std::vector<EiemCaptureSkin::Matrix> world,bindPoses;world.reserve(count);bindPoses.reserve(count);
  for (size_t slot=0;slot<count;++slot) {
    Require(Alive(bones[slot]),"capture bone Transform absent from this renderer");
    world.push_back(matrix(api.localToWorld,bones[slot]));
    EiemCaptureSkin::Matrix pose{};std::memcpy(pose.m,poses[slot].m,sizeof(pose.m));bindPoses.push_back(pose);
  }
  return EiemCaptureSkin::Build(rootWorld,inverse,world,bindPoses);
}
void* SnapshotFor(void* renderer, void* source, const Spec& spec) {
  Require(bool(scope), "runtime shape snapshot budget");
  auto ready = scope->ready.find(renderer);
  Require(ready != scope->ready.end() && ready->second, "runtime shape native skin matrices not finished");
  auto duplicate = scope->supplied.find(renderer);
  if (duplicate != scope->supplied.end()) return duplicate->second;
  Bind(renderer,source,spec);
  auto& data = Read(source,spec);
  std::map<std::string,float> weights;
  for (const auto& item : bound.at(renderer).indices) {
    int index = item.second; void* args[] = {&index};
    weights[item.first] = Scalar<float>(api.getWeight,renderer,args);
  }
  auto geometry = Evaluate(data.base,data.channels,weights);
  auto palette=CapturePaletteFor(renderer,source);
  ObserveLayout(source,"source");
  void* args[] = {source};
  auto mesh = Call(api.instantiate,nullptr,args);
  Require(mesh && mesh != source, "runtime shape snapshot allocation");
  // Attach ownership immediately: any later failure retires this copy.
  auto rooted = Ref::Make(mesh,false);
  scope->snapshots.push_back({rooted}); ++liveSnapshots;
  ObserveLayout(mesh,"clone");
  if (api.clear) Call(api.clear,mesh); else api.clearIcall(mesh);
  ObserveLayout(mesh,"clear");
  SetChannel(mesh,0,3,geometry.positions.data(),int(spec.vertices));
  SetChannel(mesh,1,3,geometry.normals.data(),int(spec.vertices));
  SetChannel(mesh,2,4,geometry.tangents.data(),int(spec.vertices));
  ObserveLayout(mesh,"channels");
  Call(api.recalculateBounds,mesh);
  Require(Scalar<int>(api.shapeCount,mesh) == 0 && Scalar<int>(api.vertexCount,mesh) == int(spec.vertices),
          "runtime shape snapshot native channel contract");
  Require(Array<Vec3>(Call(api.vertices,mesh),spec.vertices) == geometry.positions &&
          Array<Vec3>(Call(api.normals,mesh),spec.vertices) == geometry.normals &&
          Array<Vec4>(Call(api.tangents,mesh),spec.vertices) == geometry.tangents,
          "runtime shape snapshot vertex readback mismatch");
  // Public vertex setters must not disturb skinning or triangle connectivity.
  int submesh=0; unsigned char applyBase=1; void* indexArgs[]={&submesh,&applyBase};
  auto compareArray=[&](void* method,size_t step,void** a=nullptr) {
    auto first=Ref::Make(Call(method,source,a),false), second=Ref::Make(Call(method,mesh,a),false);
    size_t x=0,y=0;
    Require(ArrayLength(first.Get(),&x) && ArrayLength(second.Get(),&y) && x==y,"runtime snapshot skin/index count");
    SkinningObservation::ArrayElementSize(first.Get(),int(step)); SkinningObservation::ArrayElementSize(second.Get(),int(step));
    Bytes left(x*step),right(y*step);
    Require((!x || (CopyRaw((char*)first.Get()+32,left.data(),left.size()) && CopyRaw((char*)second.Get()+32,right.data(),right.size()))) && left==right,
            "runtime snapshot changed topology/skin palette");
  };
  compareArray(api.indices,4,indexArgs); compareArray(api.bindPoses,64); compareArray(api.boneWeights,32);
  Require(Scalar<int>(api.shapeCount,source)==int(spec.channels.size()) &&
          Array<Vec3>(Call(api.vertices,source),spec.vertices)==data.base.positions,
          "runtime snapshot changed its source Mesh");
  scope->capturePalettes.emplace(renderer,std::move(palette));
  scope->supplied.emplace(renderer,mesh);
  Log("[SHAPE-CAPTURE-SNAPSHOT] seq=%u renderer=%p source=%p snapshot=%p vertices=%u sourceShapes=%zu snapshotShapes=0 localSpace=1 immutable=1 live=%zu",
      ++snapshotSequence,renderer,source,mesh,spec.vertices,data.channels.size(),liveSnapshots);
  for (const auto& w : weights) Log("[SHAPE-CAPTURE-WEIGHT] seq=%u channel=%s weight=%.6g",snapshotSequence,w.first.c_str(),w.second);
  return mesh;
}
void* GetMesh(void* renderer) {
  auto mesh = originalMeshGetter(renderer);
  if (!enabled || !scope || building || !renderer || packageGeneration != generation.load()) return mesh;
  try {
    if (!Main()) return mesh;
    const auto* spec = Match(mesh);
    if (!spec || spec->channels.empty()) return mesh; // Uncontrolled/shape-free consumers retain native capture.
    struct Guard { Guard() { building=true; } ~Guard() { building=false; } } guard;
    return SnapshotFor(renderer,mesh,*spec);
  } catch (const std::exception& error) {
    Log("[SHAPE-CAPTURE-GAP] renderer=%p source=%p reason=%s nativeSourcePreserved=1",renderer,mesh,error.what());
    return mesh;
  }
}
void* Capture(void* self, void* root, int mask, void* info) {
  bool main=false;
  try { main=enabled && Main(); } catch(const std::exception& e) { Log("[SHAPE-CAPTURE-GAP] thread-check=%s",e.what()); }
  if (!main) return originalCapture(self,root,mask,info);
  CaptureScope capture;
  auto result = originalCapture(self,root,mask,info);
  if (result && !capture.snapshots.empty()) {
    try {
      groups.push_back({Ref::Make(self,true),Ref::Make(result,true),std::move(capture.snapshots)});
      Log("[SHAPE-CAPTURE-OWNER] controller=%p entity=%p snapshots=%zu groups=%zu",self,result,groups.back().snapshots.size(),groups.size());
      RequestGpuCapture(capture.assignments);
    } catch(const std::exception& e) {
      // The original capture already holds these Meshes. Do not retire them
      // early if ownership bookkeeping fails; retain until process shutdown.
      static std::vector<Snapshot> retained;
      for (auto& snapshot:capture.snapshots) retained.push_back(std::move(snapshot));
      capture.snapshots.clear();
      Log("[SHAPE-CAPTURE-GAP] owner=%s retainedUntilShutdown=1",e.what());
    }
  }
  return result;
}
void Release(void* self, void* entity, void* info) {
  originalRelease(self,entity,info);
  try {
  if (!enabled || !Main()) return;
  for (auto it=groups.begin();it!=groups.end();) {
    if (it->owner.Get()==self && it->entity.Get()==entity) {
      Retire(it->snapshots); it=groups.erase(it);
      Log("[SHAPE-CAPTURE-RELEASE] controller=%p entity=%p live=%zu",self,entity,liveSnapshots);
    } else ++it;
  }
  } catch(const std::exception& e) { Log("[SHAPE-CAPTURE-GAP] release=%s",e.what()); }
}
void Reset(void* self, void* info) {
  originalReset(self,info);
  try {
  if (!enabled || !Main()) return;
  // Reset can discard managed bookkeeping without completing native capture
  // consumers. Only the game's Release call grants snapshot retirement.
  size_t retained=0;
  for (const auto& group:groups) if (group.owner.Get()==self) retained+=group.snapshots.size();
  if (retained) Log("[SHAPE-CAPTURE-RESET] controller=%p retained=%zu awaitingNativeRelease=1",self,retained);
  } catch(const std::exception& e) { Log("[SHAPE-CAPTURE-GAP] reset=%s",e.what()); }
}
// Native intent and author visibility are separate. A game request to disable
// a controlled Renderer remains disabled even when the author's part is shown.
void SetEnabled(void* renderer,bool nativeRequest) {
  bool output=nativeRequest;
  try {
    if (enabled && !visibilityWriting && Main()) {
      auto it=bound.find(renderer);
      if (it!=bound.end() && it->second.renderer.Get()==renderer && Alive(renderer) &&
          SourceMesh(renderer)==it->second.mesh.Get()) {
        const auto& spec=specs.at(it->second.marker);
        if (spec.visible) {
          it->second.nativeEnabled=nativeRequest;
          output=nativeRequest && spec.visible->Evaluate(variables.at(spec.package))!=0;
        }
      }
    }
  } catch(const std::exception& e) {Log("[CONTROL-RUNTIME-GAP] nativeEnabled=%s",e.what());}
  originalEnabledSetter(renderer,output);
}
void ApplyVisibility(void* renderer,Bound& binding,const Spec& spec) {
  if (!spec.visible) return;
  bool authorVisible=spec.visible->Evaluate(variables.at(spec.package))!=0;
  bool target=binding.nativeEnabled && authorVisible;
  if ((Scalar<unsigned char>(api.getEnabled,renderer)!=0)==target) return;
  struct Guard {Guard(){visibilityWriting=true;}~Guard(){visibilityWriting=false;}} guard;
  originalEnabledSetter(renderer,target);
  Require((Scalar<unsigned char>(api.getEnabled,renderer)!=0)==target,"runtime visibility readback mismatch");
  Log("[CONTROL-RUNTIME-VISIBILITY] package=%s part=%s renderer=%p authorVisible=%d nativeEnabled=%d enabled=%d",
      spec.package.c_str(),spec.part.c_str(),renderer,int(authorVisible),int(binding.nativeEnabled),int(target));
}
void RegisterRenderer(void* renderer,void* mesh) {
  if (registrationPaused) return;
  if (auto spec=Match(mesh)) {
    Bind(renderer,mesh,*spec);
    ApplyVisibility(renderer,bound.at(renderer),*spec);
  }
}
bool CurrentGeneration() {
  unsigned current=generation.load();if (current==packageGeneration) return true;
  if (current==checkedGeneration) return false;
  auto graph=std::atomic_load(&published);
  if (current!=generation.load()) return false;
  checkedGeneration=current;
  if (!graph || !controlGraph || graph->size()!=controlGraph->size()) return false;
  for (size_t i=0;i<graph->size();++i)
    if (EiemNativeInput::PackageDigest(graph->at(i))!=EiemNativeInput::PackageDigest(controlGraph->at(i))) {
      Log("[CONTROL-RUNTIME-GENERATION] generation=%u changedPackage=1 requiresPreparedInputsAndRestart=1",current);return false;
    }
  packageGeneration=current;
  Log("[CONTROL-RUNTIME-GENERATION] generation=%u unchangedPackage=1 currentValuesRetained=1",current);
  return true;
}
// The game's model completion callback supplies this exact instance root.
// Include inactive children so later visibility/LOD changes need no scene scan.
void RegisterModel(void* root) {
  if (!enabled || registrationPaused || !Main() || !Alive(root) || !CurrentGeneration()) return;
  Require(api.modelRenderers && api.rendererType,"runtime model registration unavailable");
  unsigned char inactive=1;void* args[]={api.rendererType,&inactive};
  auto objects=Ref::Make(Call(api.modelRenderers,root,args),false);size_t count=0;
  Require(ArrayLength(objects.Get(),&count),"runtime model renderer array absent");
  for (size_t i=0;i<count;++i) {
    void* renderer=nullptr;
    Require(CopyRaw((char*)objects.Get()+32+i*sizeof(void*),&renderer,sizeof(renderer)),"runtime model renderer element");
    if (Alive(renderer)) RegisterRenderer(renderer,SourceMesh(renderer));
  }
}
void Update() {
  if (!enabled || !Main()) return;
  int frame = Scalar<int>(api.frame);
  if (frame == lastFrame) return;
  lastFrame = frame;
  ReloadObservation::Frame(frame);
  LoadingObservation::Frame(frame);
  CommitPendingReload();
  if (!CurrentGeneration()) return;
  PollGpuCapture();
  // Poll once per Unity frame, independent of the number of model instances.
  double elapsed = Scalar<float>(api.elapsed);
  Require(std::isfinite(elapsed) && elapsed >= 0, "runtime shape frame delta");
  auto window = GetForegroundWindow(); DWORD pid=0; GetWindowThreadProcessId(window,&pid);
  bool focused = active.load() && pid == GetCurrentProcessId();
  PanelCommands(focused);
  PollControls(focused,elapsed);
  if (traceBoundaries && frame%300==0)
    Log("[DEBUG-shape-v24-INPUT] frame=%d focusPid=%lu focused=%d packages=%zu bound=%zu delta=%.9g",
        frame,pid,int(focused),keys.size(),bound.size(),elapsed);
  for (auto it=bound.begin();it!=bound.end();) {
    auto renderer=it->second.renderer.Get(), mesh=it->second.mesh.Get();
    if (!Alive(renderer) || !Alive(mesh)) { it=bound.erase(it); continue; }
    auto current=SourceMesh(renderer);
    if (current!=mesh) { it=bound.erase(it); continue; }
    const auto& spec=specs.at(it->second.marker);
    ApplyVisibility(renderer,it->second,spec);
    for (const auto& control : spec.controls) {
      int index=it->second.indices.at(control.first);
      float target=float(variables.at(spec.package).at(control.second)*100.0);
      void* readArgs[]={&index};
      float before=Scalar<float>(api.getWeight,renderer,readArgs);
      if (before!=target) {
        void* writeArgs[]={&index,&target}; Call(api.setWeight,renderer,writeArgs);
        Require(std::abs(Scalar<float>(api.getWeight,renderer,readArgs)-target)<0.0001f,"runtime shape weight readback");
        if (frame%30==0) Log("[SHAPE-RUNTIME-WEIGHT] renderer=%p channel=%s value=%.6g",renderer,control.first.c_str(),target);
      }
    }
    ++it;
  }
  for (auto it=cached.begin();it!=cached.end();) {
    if (!Alive(it->second.mesh.Get())) it=cached.erase(it); else ++it;
  }
  boundCount.store(unsigned(bound.size()));
  // Losing a managed weak reference does not prove native/GPU consumers have
  // retired. Keep their immutable snapshots until an observed native Release.
}
void Tick(void* self, void* info) {
  originalTick(self,info);
  try { Update(); } catch(const std::exception& e) { Log("[SHAPE-RUNTIME-GAP] update=%s",e.what()); }
}
void FrameTick(void* self,void* info) {
  originalFrameTick(self,info);
  try { Update(); } catch(const std::exception& e) { Log("[SHAPE-RUNTIME-GAP] frameUpdate=%s",e.what()); }
}
void Init(void* self, void* renderers, void* configs, void* custom, bool skip, void* info) {
  // Share the existing exact Init detour with optional observation. The
  // observer calls this same original once; no second MinHook layer.
  if(ReloadObservation::ShareMaterialInit()) ReloadObservation::Material(self,renderers,configs,custom,skip,info);
  else originalInit(self,renderers,configs,custom,skip,info);
  try {
    if (!enabled || registrationPaused || !renderers || !Main()) return;
    auto klass=il2cpp_object_get_class(renderers); void *iterator=nullptr,*itemsField=nullptr,*sizeField=nullptr;
    while (auto f=il2cpp_class_get_fields(klass,&iterator)) {
      auto name=il2cpp_field_get_name(f);
      if (name && !strcmp(name,"_items")) itemsField=f;
      if (name && !strcmp(name,"_size")) sizeField=f;
    }
    Require(itemsField && sizeField,"runtime shape renderer list metadata");
    void* items=nullptr; int size=0; size_t capacity=0;
    il2cpp_field_get_value(renderers,itemsField,&items); il2cpp_field_get_value(renderers,sizeField,&size);
    Require(size>=0 && ArrayLength(items,&capacity) && size_t(size)<=capacity,"runtime shape renderer list range");
    for (int i=0;i<size;++i) {
      void* renderer=nullptr; Require(CopyRaw((char*)items+32+i*sizeof(void*),&renderer,sizeof(renderer)),"runtime shape renderer list element");
      if (!Alive(renderer)) continue;
      auto c=il2cpp_object_get_class(renderer); bool skin=false;
      for (;c;c=il2cpp_class_get_parent(c)) { auto n=il2cpp_class_get_name(c); auto ns=il2cpp_class_get_namespace(c); skin|=n && ns && !strcmp(n,"SkinnedMeshRenderer") && !strcmp(ns,"UnityEngine"); }
      if (!skin) continue;
      auto mesh=SourceMesh(renderer);
      RegisterRenderer(renderer,mesh);
    }
  } catch(const std::exception& e) { Log("[SHAPE-RUNTIME-GAP] registration=%s",e.what()); }
}
void InstallOptional(void** assemblies,size_t count) {
  // Format-2 controls are a normal feature. Diagnostic markers only govern
  // probes and GPU captures, not whether author keys work.
  auto graph=std::atomic_load(&published);
  if (!graph) return;
  bool needed=false;
  for (const auto& package:*graph) needed|=!package.parts.empty();
  if (!needed && !std::filesystem::exists(gameDirectory/"plugin/tools/eff_resource_pack.exe") && !std::filesystem::exists(gameDirectory/"plugin/resource-reload-baseline/baseline.tsv")) return;
  try {
    Require(il2cpp_resolve_icall,"runtime shape icall resolver unavailable");
    Require(bool(graph),"runtime control package graph unavailable");
    LoadControls(graph);
    Require(!needed || !specs.empty(),"runtime controls absent");
    auto object=FindClass("UnityEngine","Object",assemblies,count), mesh=FindClass("UnityEngine","Mesh",assemblies,count);
    auto skin=FindClass("UnityEngine","SkinnedMeshRenderer",assemblies,count), time=FindClass("UnityEngine","Time",assemblies,count);
    api.vec3Class=FindClass("UnityEngine","Vector3",assemblies,count);
    api.main=Exact(object,"CurrentThreadIsMainThread",{},"System.Boolean",true);
    api.alive=Exact(object,"op_Implicit",{"UnityEngine.Object"},"System.Boolean",true);
    api.name=Exact(object,"get_name",{},"System.String",false);
    api.mesh=Exact(skin,"get_sharedMesh",{},"UnityEngine.Mesh",false);
    auto renderer=FindClass("UnityEngine","Renderer",assemblies,count);
    api.getEnabled=Exact(renderer,"get_enabled",{},"System.Boolean",false);
    Require(api.getEnabled,"runtime Renderer enabled getter unavailable");
    api.modelRenderers=Exact(FindClass("UnityEngine","GameObject",assemblies,count),"GetComponentsInChildren",{"System.Type","System.Boolean"},"UnityEngine.Component[]",false);
    Require(api.modelRenderers && il2cpp_class_get_type && il2cpp_type_get_object,"runtime model renderer registration unavailable");
    api.rendererType=il2cpp_type_get_object(il2cpp_class_get_type(skin));
    Require(api.rendererType,"runtime SkinnedMeshRenderer type missing");
    Log("[CONTROL-RUNTIME-REGISTRATION] modelCompletion=1 includeInactive=1 controllerInit=1 sceneRendererDiscovery=0");
    api.vertexCount=Exact(mesh,"get_vertexCount",{},"System.Int32",false);
    api.vertices=Exact(mesh,"get_vertices",{},"UnityEngine.Vector3[]",false);
    api.normals=Exact(mesh,"get_normals",{},"UnityEngine.Vector3[]",false);
    api.tangents=Exact(mesh,"get_tangents",{},"UnityEngine.Vector4[]",false);
    api.readable=Exact(mesh,"get_isReadable",{},"System.Boolean",false);
    api.shapeCount=Exact(mesh,"get_blendShapeCount",{},"System.Int32",false);
    api.shapeName=Exact(mesh,"GetBlendShapeName",{"System.Int32"},"System.String",false);
    api.frameCount=Exact(mesh,"GetBlendShapeFrameCount",{"System.Int32"},"System.Int32",false);
    api.frameWeight=Exact(mesh,"GetBlendShapeFrameWeight",{"System.Int32","System.Int32"},"System.Single",false);
    api.frameVertices=Exact(mesh,"GetBlendShapeFrameVertices",{"System.Int32","System.Int32","UnityEngine.Vector3[]","UnityEngine.Vector3[]","UnityEngine.Vector3[]"},"System.Void",false);
    api.getWeight=Exact(skin,"GetBlendShapeWeight",{"System.Int32"},"System.Single",false);
    api.setWeight=Exact(skin,"SetBlendShapeWeight",{"System.Int32","System.Single"},"System.Void",false);
    api.instantiate=Exact(object,"Instantiate",{"UnityEngine.Object"},"UnityEngine.Object",true);
    api.clear=Exact(mesh,"ClearBlendShapes",{},"System.Void",false);
    // This build strips the managed wrapper, but UnityPlayer retains the
    // engine's registered public Mesh binding. Resolve it through IL2CPP,
    // with no file RVA or private native pointer layout.
    if (!api.clear) {
      api.clearIcall=(decltype(api.clearIcall))il2cpp_resolve_icall("UnityEngine.Mesh::ClearBlendShapes()");
      if (!api.clearIcall) api.clearIcall=(decltype(api.clearIcall))il2cpp_resolve_icall("UnityEngine.Mesh::ClearBlendShapes");
    }
    Require(api.clear || api.clearIcall,"runtime shape snapshot ClearBlendShapes binding absent");
    api.bindPoses=Exact(mesh,"get_bindposes",{},"UnityEngine.Matrix4x4[]",false);
    auto component=FindClass("UnityEngine","Component",assemblies,count);
    auto transform=FindClass("UnityEngine","Transform",assemblies,count);
    api.transformClass=transform;
    api.bones=Exact(skin,"get_bones",{},"UnityEngine.Transform[]",false);
    api.rootBone=Exact(skin,"get_rootBone",{},"UnityEngine.Transform",false);
    api.skinningRoot=Exact(skin,"get_skinningRoot",{},"UnityEngine.Transform",false);
    api.transform=Exact(component,"get_transform",{},"UnityEngine.Transform",false);
    api.localToWorld=Exact(transform,"get_localToWorldMatrix",{},"UnityEngine.Matrix4x4",false);
    api.worldToLocal=Exact(transform,"get_worldToLocalMatrix",{},"UnityEngine.Matrix4x4",false);
    api.boneWeights=Exact(mesh,"get_boneWeights",{},"UnityEngine.BoneWeight[]",false);
    api.indices=Exact(mesh,"GetIndicesImpl",{"System.Int32","System.Boolean"},"System.Int32[]",false);
    api.setChannel=Exact(mesh,"SetSizedNativeArrayForChannel",{"UnityEngine.Rendering.VertexAttribute","UnityEngine.Rendering.VertexAttributeFormat","System.Int32","System.IntPtr","System.Int32","System.Int32","System.Int32","UnityEngine.Rendering.MeshUpdateFlags"},"System.Void",false);
    api.recalculateBounds=Exact(mesh,"RecalculateBounds",{},"System.Void",false);
    api.destroy=Exact(object,"Destroy",{"UnityEngine.Object"},"System.Void",true);
    api.frame=Exact(time,"get_frameCount",{},"System.Int32",true);
    api.elapsed=Exact(time,"get_unscaledDeltaTime",{},"System.Single",true);
    for (auto method : {api.main,api.alive,api.name,api.mesh,api.vertexCount,api.vertices,api.normals,api.tangents,api.readable,
      api.shapeCount,api.shapeName,api.frameCount,api.frameWeight,api.frameVertices,api.getWeight,api.setWeight,api.instantiate,
      api.bindPoses,api.boneWeights,api.indices,api.bones,api.rootBone,api.skinningRoot,api.transform,api.localToWorld,api.worldToLocal,api.transformClass,
      api.setChannel,api.recalculateBounds,api.destroy,api.frame,api.elapsed,api.vec3Class}) Require(method!=nullptr,"runtime shape exact API signature absent");
    Ref::weakHandle=(decltype(Ref::weakHandle))GetProcAddress(hGA,"il2cpp_gchandle_new_weakref");
    Require(Ref::weakHandle && il2cpp_array_new && il2cpp_gchandle_new && il2cpp_gchandle_get_target && il2cpp_gchandle_free,
            "runtime shape managed lifetime exports absent");
    Require(il2cpp_resolve_icall && il2cpp_class_get_parent && il2cpp_class_get_name && il2cpp_class_get_namespace &&
            il2cpp_object_get_class && il2cpp_class_get_fields && il2cpp_field_get_name && il2cpp_field_get_value &&
            il2cpp_class_value_size && il2cpp_class_get_element_class && il2cpp_object_unbox,
            "runtime shape metadata/readback exports absent");
    auto mask=FindClass("Beyond.Rendering","EntityVFXRendererMask",assemblies,count); uint32_t alignment=0;
    Require(mask && il2cpp_class_value_size(mask,&alignment)==4,"runtime capture renderer mask ABI mismatch");
    Log("[SHAPE-RUNTIME-API] clearManaged=%d clearRegisteredBinding=%p captureMaskBytes=4",!!api.clear,api.clearIcall);
    auto controller=FindClass("Beyond.Rendering","EntityRenderCaptureMeshController",assemblies,count);
    auto material=FindClass("Beyond.Rendering","EntityRenderHelperMaterialController",assemblies,count);
    Require(Install(controller,"Capture",{"UnityEngine.GameObject","Beyond.Rendering.EntityVFXRendererMask"},"Beyond.Rendering.EntityRenderCapturedEntity",false,(void*)Capture,(void**)&originalCapture),"runtime capture hook absent");
    Require(Install(controller,"Release",{"Beyond.Rendering.EntityRenderCapturedEntity"},"System.Void",false,(void*)Release,(void**)&originalRelease),"runtime capture release hook absent");
    Require(Install(controller,"Reset",{},"System.Void",false,(void*)Reset,(void**)&originalReset),"runtime capture reset hook absent");
    Require(Install(controller,"LateTick",{},"System.Void",false,(void*)Tick,(void**)&originalTick),"runtime shape tick hook absent");
    auto loop=FindClass("Beyond","TailGameLoop",assemblies,count);
    Require(Install(loop,"LateUpdate",{},"System.Void",false,(void*)FrameTick,(void**)&originalFrameTick),"runtime shape regular frame hook absent");
    Log("[SHAPE-RUNTIME-FRAME-API] entry=Beyond.TailGameLoop.LateUpdate perUnityFrame=1 captureIndependent=1");
    Require(Install(material,"Init",{"System.Collections.Generic.List<UnityEngine.Renderer>","System.Collections.Generic.List<Beyond.Rendering.EntityRendererTypeConfig>","Beyond.Rendering.EntityCustomizeRendererPropertyConfig","System.Boolean"},"System.Void",false,(void*)Init,(void**)&originalInit),"runtime shape registration hook absent");
    // Capture's native code calls the resolved icall directly, rather than the
    // managed getter wrapper. Only this getter is adapted inside a TLS scope.
    auto getter=il2cpp_resolve_icall("UnityEngine.SkinnedMeshRenderer::get_sharedMesh()");
    Require(getter && !installed.count(getter),"runtime capture shared Mesh getter unavailable");
    Require(MH_CreateHook(getter,(void*)GetMesh,(void**)&originalMeshGetter)==MH_OK && MH_EnableHook(getter)==MH_OK,"runtime capture getter hook failed");
    installed.insert(getter);
    // This adapter is required for shaped captures whose native renderer has
    // no matrix-provider job. It is independent of the optional debug marker.
    auto installIcall=[&](const char* name,void* hook,void** original) {
      auto target=il2cpp_resolve_icall(name);
      if (!target && !strcmp(name,"UnityEngine.Renderer::set_enabled(System.Boolean)"))
        target=il2cpp_resolve_icall("UnityEngine.Renderer::set_enabled");
      Require(target && !installed.count(target),"capture public binding unavailable");
      Require(MH_CreateHook(target,hook,original)==MH_OK && MH_EnableHook(target)==MH_OK,"capture public hook failed");
      installed.insert(target);
    };
    // Public enabled binding: self, bool; writes retain native intent.
    installIcall("UnityEngine.Renderer::set_enabled(System.Boolean)",(void*)SetEnabled,(void**)&originalEnabledSetter);
    // Registered binding: self, void* destination, int bytes; no MethodInfo.
    installIcall("UnityEngine.SkinnedMeshRenderer::RequestCurrentFrameSkinMatrices",(void*)RequestMatrices,(void**)&originalMatrixRequest);
    Log("[SHAPE-CAPTURE-MATRIX-API] installed=1 perRendererSlots=1 publicTransforms=1 synchronousRefusalFallback=1");
    traceBoundaries=std::filesystem::exists(gameDirectory/"plugin/resource-input-diagnostics/shape-capture-boundaries.enabled");
#if defined(EIEM_SHAPE_CAPTURE_GFXTRACE)
    gpuCaptureEnabled=std::filesystem::exists(gameDirectory/"plugin/resource-input-diagnostics/shape-capture-gpu.enabled");
#endif
    if (traceBoundaries) {
      // Optional evidence must never disable the working runtime shape bridge.
      try {
        api.attributeCount=Exact(mesh,"GetVertexAttributeCountImpl",{},"System.Int32",false);
        api.attributeDescriptor=Exact(mesh,"GetVertexAttribute",{"System.Int32"},"UnityEngine.Rendering.VertexAttributeDescriptor",false);
        api.bonesPerVertex=Exact(mesh,"GetBonesPerVertexValue",{},"System.Int32",false);
        auto filter=FindClass("UnityEngine","MeshFilter",assemblies,count);
        api.filterMesh=Exact(filter,"get_sharedMesh",{},"UnityEngine.Mesh",false);
        installIcall("UnityEngine.Mesh::get_blendShapeCount()",(void*)ShapeCount,(void**)&originalShapeCount);
        installIcall("UnityEngine.MeshFilter::set_sharedMesh(UnityEngine.Mesh)",(void*)SetFilterMesh,(void**)&originalFilterSetter);
        Log("[DEBUG-shape-v24-READY] count=%d assignment=%d matrixRequest=%d layout=%d sampleLimit=128 nativeCountPreserved=1",
            !!originalShapeCount,!!originalFilterSetter,!!originalMatrixRequest,
            int(api.attributeCount && api.attributeDescriptor && api.bonesPerVertex && api.filterMesh));
      } catch(const std::exception& e) { Log("[DEBUG-shape-v20-GAP] phase=install reason=%s shapeBridgePreserved=1",e.what()); }
    }
    packageGeneration=generation.load();
    CaptureObservation::afterFinished=Finished;
    Log("[SHAPE-RUNTIME-PREPARED] controlledMeshes=%zu generation=%u nativeShapeChannelsRetained=1 captureLocalSnapshot=1 captureMatrixFallback=1",specs.size(),packageGeneration);
  } catch(const std::exception& e) {
    enabled=false; CaptureObservation::afterFinished=nullptr;
    Log("[SHAPE-RUNTIME-DISABLED] reason=%s nativeSourcePreserved=1",e.what());
  }
}
void Arm() {
  enabled=CaptureObservation::afterFinished && CaptureObservation::originalFinished && originalCapture && originalRelease && originalReset && originalTick && originalFrameTick && originalInit && originalMeshGetter && originalMatrixRequest && originalEnabledSetter;
  if (CaptureObservation::afterFinished) Log("[SHAPE-RUNTIME-READY] enabled=%d controlsBound=%u sourceShapesRetained=1 captureSnapshotReadyGuard=1",int(enabled.load()),boundCount.load());
}
} // namespace RuntimeShapes
