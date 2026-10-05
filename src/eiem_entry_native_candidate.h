#pragma once
#include "eiem_loading_generation.h"

// R1a: opt-in, dependency-free body Mesh load witness. It never publishes an
// input table, creates a Renderer, writes Mesh/Renderer data or migrates owners.
namespace NativeCandidate {
using namespace EiemNativeInput;
using RuntimeShapes::Ref;
struct FileHold {
  HANDLE handle = INVALID_HANDLE_VALUE;
  explicit FileHold(const std::filesystem::path& path) {
    handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    Require(handle != INVALID_HANDLE_VALUE, "candidate file lock unavailable");
  }
  ~FileHold() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
  FileHold(const FileHold&) = delete;
  Bytes BytesLocked(size_t maximum = 4 * 1024 * 1024) {
    LARGE_INTEGER size{}; Require(GetFileSizeEx(handle, &size) && size.QuadPart > 0 &&
      uint64_t(size.QuadPart) <= maximum && uint64_t(size.QuadPart) <= MAXDWORD, "candidate locked input size limit");
    LARGE_INTEGER zero{}; Require(SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN), "candidate read seek");
    Bytes bytes(size_t(size.QuadPart)); DWORD read = 0;
    Require(ReadFile(handle, bytes.data(), DWORD(bytes.size()), &read, nullptr) && read == bytes.size(),
            "candidate locked file read failed"); return bytes;
  }
};
struct Plan {
  std::string key, contract, input, originalHash, candidateHash, assetPath, marker;
  std::string originalCab, candidateCab;
  std::filesystem::path path;
  std::shared_ptr<FileHold> file;
  RuntimeControls::Spec spec;
  size_t bones = 0, indices = 0, submeshes = 0;
};
std::atomic<bool> enabled{false}, busy{false};
std::mutex pendingMutex;
std::shared_ptr<const Plan> pending;
struct Api {
  void *load = nullptr, *done = nullptr, *bundle = nullptr, *assetLoad = nullptr, *result = nullptr;
  void *unload = nullptr, *instanceId = nullptr, *submeshes = nullptr, *meshClass = nullptr;
  Ref meshType;
} api;
enum class Phase { Bundle, Asset, SourceBase, CandidateBase, SourceShapes, CandidateShapes, Coexist, Passed, Rejected };
struct Attempt {
  std::shared_ptr<const Plan> plan;
  Ref path, label, createRequest, assetRequest, bundle, candidate, source, renderer, originalBundle;
  Phase phase = Phase::Bundle;
  ULONGLONG started = 0, coexistStarted = 0;
  int oldId = 0, newId = 0, originalBundleId = 0, polls = 0, coexistSamples = 0;
  std::string baseDigest, shapeDigest;
  bool unloadCalled = false, nativeFailed = false, bundleOwned = false;
};
std::unique_ptr<Attempt> attempt;

static uint64_t Clock() { LARGE_INTEGER value{}; QueryPerformanceCounter(&value); return uint64_t(value.QuadPart); }
static double Ms(uint64_t start) {
  LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
  return frequency.QuadPart ? double(Clock()-start)*1000.0/double(frequency.QuadPart) : 0;
}
inline std::string Hash(const Bytes& bytes) { auto hash = Sha(bytes.data(), bytes.size()); return Hex(hash.data(), hash.size()); }
std::shared_ptr<const Plan> ReadPlan(const std::filesystem::path& config) {
  auto start = Clock(); auto bytes = EiemDirectory::Read(config, 4096);
  std::istringstream lines(Text(bytes.data(), bytes.size())); std::string header, row, extra;
  Require(bool(std::getline(lines, header)) && bool(std::getline(lines, row)), "candidate validation header/row missing");
  if (!header.empty() && header.back() == '\r') header.pop_back();
  if (!row.empty() && row.back() == '\r') row.pop_back();
  while (std::getline(lines, extra)) Require(extra.empty() || extra == "\r", "candidate validation supports exactly one primitive");
  auto h = TabFields(header), r = TabFields(row);
  Require(h.size() == 4 && h[0] == "EFF_NATIVE_CANDIDATE_VALIDATION" && h[1] == "1" &&
      DigestText(h[2]) && DigestText(h[3]), "candidate validation header differs");
  Require(r.size() == 8 && DigestText(r[2]) && DigestText(r[3]), "candidate validation row differs");
  auto graph = std::atomic_load(&published); auto inputs = std::atomic_load(&substitutions);
  Require(graph && graph->size() == 1 && inputs && inputs->control.empty() && inputs->inputContractDigest == h[3] &&
      InputContractDigest(graph->front()) == h[3], "candidate validation production contract differs");
  const auto& package = graph->front(); Require(package.id == r[6], "candidate Mod identity differs");
  Identity identity{r[4], r[5]}; auto target = package.targetIndex.find(identity);
  Require(target != package.targetIndex.end(), "candidate Mesh source identity absent");
  const auto& rule = package.rules.at(target->second);
  Require(!rule.skip && rule.parts.size() == 1 && rule.parts.front() == r[7], "candidate primitive part topology differs");
  const auto& mesh = package.meshes.at(package.parts.at(r[7]).mesh);
  Require(!mesh.shapes.empty() && mesh.vertices <= 20000 && mesh.bones.size() <= 256,
          "candidate validation requires a bounded shaped primitive");
  auto next = std::make_shared<Plan>();
  next->key = h[2]; next->contract = h[3]; next->input = r[0];
  next->originalHash = r[3]; next->candidateHash = r[2]; next->assetPath = r[4];
  next->marker = RuntimePartName(package, r[7], identity);
  auto controls = RuntimeControls::Prepare(graph, EiemEntrySettings::Read(gameDirectory));
  next->spec = controls.specs.at(next->marker); next->bones = mesh.bones.size(); next->indices = mesh.indices;
  std::filesystem::path relative = std::filesystem::u8path(r[1]);
  Require(!relative.is_absolute() && !relative.has_root_name(), "candidate path must be relative to plugin");
  for (const auto& part : relative) Require(part != "..", "candidate path escapes plugin");
  auto root = std::filesystem::weakly_canonical(gameDirectory / "plugin" / "resource-input-candidates");
  next->path = std::filesystem::weakly_canonical(gameDirectory / "plugin" / relative);
  auto inside = InputPath(next->path.u8string()), prefix = InputPath(root.u8string()) + "/";
  Require(inside.rfind(prefix,0) == 0, "candidate path outside dedicated candidate directory");
  next->file = std::make_shared<FileHold>(next->path);
  auto candidateBytes = next->file->BytesLocked(); Require(Hash(candidateBytes) == next->candidateHash, "candidate input SHA changed");
  const InputSubstitution* selected = nullptr;
  for (const auto& input : inputs->entries) if (input.second.output.filename().u8string() == next->input) {
    Require(!selected, "ambiguous original input filename"); selected = &input.second;
  }
  Require(selected && selected->outputHash == next->originalHash, "candidate original source is not the accepted loading input");
  Require(SameStamp(selected->outputStamp,FileStamp(selected->output)), "original output changed since publication");
  auto originalBytes = EiemDirectory::Read(selected->output,4 * 1024 * 1024);
  Require(Hash(originalBytes) == next->originalHash, "original input SHA changed");
  auto original = EndfieldInput::Decode(originalBytes), candidate = EndfieldInput::Decode(candidateBytes);
  Require(original.members.size() == 1 && candidate.members.size() == 1, "candidate primitive has additional members");
  auto& originalMember = original.members.front(); const auto& candidateMember = candidate.members.front();
  auto layout = ParseSerialized(originalMember.bytes);
  Require(layout.externals.empty() && layout.objects.size() == 2, "candidate primitive has dependencies or additional native objects");
  auto assets = SerializedAssets(originalMember.bytes,originalMember.name,layout);
  Require(assets.size() == 1 && assets.front().path == next->assetPath && assets.front().asset == next->marker &&
      assets.front().classId == 43, "candidate primitive logical asset identity differs");
  auto meshValue = ObjectValue(originalMember.bytes,layout,assets.front().pathId);
  auto& meshType = layout.types.at(layout.objects.at(assets.front().pathId).type);
  next->submeshes = ArrayValue(meshType,Child(meshType,meshValue,"m_SubMeshes")).count;
  Require(next->submeshes == 1, "candidate validation primitive submesh count differs");
  auto refs = EiemLoadingGraphAudit::ReadReferences(originalMember.bytes,layout);
  Require(refs.unparsed.empty() && refs.streams.empty(), "candidate primitive contains opaque or streamed dependencies");
  for (const auto& ptr : refs.pointers) Require(ptr.fileId == 0 && layout.objects.count(ptr.pathId), "candidate primitive dependency unresolved");
  std::string oldBundle;
  for (const auto& object : layout.objects) if (layout.types.at(object.second.type).classId == 142) {
    auto fields = Fields(layout.types.at(object.second.type),0,originalMember.bytes,object.second.offset,
                         object.second.offset+object.second.size,object.second.offset);
    oldBundle = FieldString(originalMember.bytes,fields.at("m_Name"));
  }
  Require(!oldBundle.empty(), "candidate original Bundle name absent");
  EiemLoadingGeneration::Names names;
  next->originalCab = originalMember.name;
  next->candidateCab = "CAB-" + EiemLoadingGeneration::Token(next->key,next->originalCab,32);
  names.members.emplace(next->originalCab,next->candidateCab);
  names.bundles.emplace(oldBundle,"main/"+EiemLoadingGeneration::Token(next->key,"bundle:"+oldBundle,24)+".ab");
  auto expected = EiemLoadingGeneration::PrepareSerialized(originalMember.bytes,names);
  Require(candidateMember.name == next->candidateCab && candidateMember.flags == originalMember.flags &&
      candidateMember.bytes == expected.bytes, "candidate changed bytes outside verified namespace fields");
  Require(InputContractDigest(std::atomic_load(&published)->front()) == next->contract &&
      std::atomic_load(&substitutions)->inputContractDigest == next->contract, "candidate production contract changed during preparation");
  Log("[NATIVE-CANDIDATE-PREPARED] key=%s input=%s sha=%s originalSha=%s bytes=%zu asset=%s vertices=%u bones=%zu shapes=%zu externalDependencies=0 allOtherPayloadPreserved=1 elapsedMs=%.6f productionPublished=0",
      next->key.c_str(),next->input.c_str(),next->candidateHash.c_str(),next->originalHash.c_str(),candidateBytes.size(),
      next->assetPath.c_str(),next->spec.vertices,next->bones,next->spec.channels.size(),Ms(start));
  return next;
}
void Prepare() {
  if (!enabled.load() || !active.load()) return;
  if (busy.exchange(true)) { Log("[NATIVE-CANDIDATE-DEDUP] reason=existing-or-inflight-witness productionUnaffected=1"); return; }
  try {
    auto next = ReadPlan(gameDirectory / "plugin/resource-input-diagnostics/native-candidate-validation.tsv");
    std::lock_guard<std::mutex> lock(pendingMutex); pending = std::move(next);
  } catch (const std::exception& e) {
    busy = false; Log("[NATIVE-CANDIDATE-REJECT] phase=prepare reason=%s nativeCalls=0 productionUnaffected=1",e.what());
  }
}
void InstallOptional(void** assemblies,size_t count) {
  auto config = gameDirectory / "plugin/resource-input-diagnostics/native-candidate-validation.tsv";
  if (!std::filesystem::exists(config) && !std::filesystem::exists(gameDirectory/"plugin/resource-reload-baseline/baseline.tsv")) return;
  Observe([&] {
    Require(RuntimeShapes::enabled && il2cpp_runtime_invoke && il2cpp_gchandle_new && il2cpp_string_new &&
       il2cpp_class_get_type && il2cpp_type_get_object, "candidate public APIs unavailable");
    auto bundle = FindClass("UnityEngine","AssetBundle",assemblies,count);
    auto request = FindClass("UnityEngine","AssetBundleCreateRequest",assemblies,count);
    auto assetRequest = FindClass("UnityEngine","AssetBundleRequest",assemblies,count);
    auto operation = FindClass("UnityEngine","AsyncOperation",assemblies,count);
    auto object = FindClass("UnityEngine","Object",assemblies,count);
    api.meshClass = FindClass("UnityEngine","Mesh",assemblies,count);
    api.load = Exact(bundle,"LoadFromFileAsync",{"System.String","System.UInt32","System.UInt64"},"UnityEngine.AssetBundleCreateRequest",true);
    api.done = Exact(operation,"get_isDone",{},"System.Boolean",false);
    api.bundle = Exact(request,"get_assetBundle",{},"UnityEngine.AssetBundle",false);
    api.assetLoad = Exact(bundle,"LoadAssetAsync_Internal",{"System.String","System.Type"},"UnityEngine.AssetBundleRequest",false);
    api.result = Exact(assetRequest,"GetResult",{},"UnityEngine.Object",false);
    api.unload = Exact(bundle,"Unload",{"System.Boolean"},"System.Void",false);
    api.instanceId = Exact(object,"GetInstanceID",{},"System.Int32",false);
    api.submeshes = Exact(api.meshClass,"get_subMeshCount",{},"System.Int32",false);
    Require(api.meshClass && api.load && api.done && api.bundle && api.assetLoad && api.result && api.unload &&
       api.instanceId && api.submeshes, "candidate exact managed signatures incomplete");
    api.meshType = Ref::Make(il2cpp_type_get_object(il2cpp_class_get_type(api.meshClass)),false);
    Require(api.meshType.Get(), "candidate Mesh System.Type unavailable"); enabled = true;
    Log("[NATIVE-CANDIDATE-READY] enabled=1 trigger=F10 scope=dependency-free-shaped-Mesh workerDiskOnly=1 mainThreadAsync=1 instanceMigration=0 gfx=0");
  });
}
void* LiveSource(Attempt& value) {
  auto renderer = value.renderer.Get(), mesh = value.source.Get();
  Require(RuntimeShapes::Alive(renderer) && RuntimeShapes::Alive(mesh) &&
      RuntimeShapes::SourceMesh(renderer) == mesh, "candidate source consumer retired or changed");
  Require(RuntimeShapes::Scalar<int>(api.instanceId,mesh) == value.oldId, "candidate original native identity changed");
  return mesh;
}
void GetSource(Attempt& value) {
  for (const auto& binding : RuntimeShapes::bound) {
    if (binding.second.marker != value.plan->marker) continue;
    auto renderer = binding.second.renderer.Get(), mesh = binding.second.mesh.Get();
    if (!RuntimeShapes::Alive(renderer) || !RuntimeShapes::Alive(mesh) || RuntimeShapes::SourceMesh(renderer) != mesh) continue;
    value.renderer = Ref::Make(renderer,true); value.source = Ref::Make(mesh,false);
    value.oldId = RuntimeShapes::Scalar<int>(api.instanceId,mesh); Require(value.oldId, "candidate original native Mesh ID zero");
    {
      std::lock_guard<std::mutex> lock(LoadingObservation::mutex);
      for (const auto& entry : LoadingObservation::bundles) {
        if (entry.second.input!=value.plan->input || entry.second.serializedHash!=value.plan->originalHash ||
            entry.second.weak.Get()!=entry.first) continue;
        Require(!value.originalBundle.Get(), "candidate original Bundle identity ambiguous");
        value.originalBundle=Ref::Make(entry.first,false);
      }
    }
    Require(RuntimeShapes::Alive(value.originalBundle.Get()), "candidate original Bundle is not available from verified loading provenance");
    value.originalBundleId=RuntimeShapes::Scalar<int>(api.instanceId,value.originalBundle.Get());
    Require(value.originalBundleId,"candidate original Bundle native ID zero");return;
  }
  throw std::runtime_error("candidate body source Renderer is not present; enter the same LZY world scene before F10");
}
void Begin(std::shared_ptr<const Plan> plan) {
  Require(RuntimeShapes::Main(), "candidate load outside Unity thread");
  auto value = std::make_unique<Attempt>(); value->plan = std::move(plan); value->started = GetTickCount64();
  GetSource(*value);
  value->path = Ref::Make(il2cpp_string_new(value->plan->path.u8string().c_str()),false);
  value->label = Ref::Make(il2cpp_string_new(value->plan->assetPath.c_str()),false);
  Require(value->path.Get() && value->label.Get(), "candidate managed path/asset string unavailable");
  attempt = std::move(value); // Own the plan/GC roots before the native operation starts.
  uint32_t crc = 0; uint64_t offset = 0;
  void* args[] = {attempt->path.Get(),&crc,&offset}; auto start = Clock();
  auto request = RuntimeShapes::Call(api.load,nullptr,args);
  Require(request, "candidate native Bundle create request null");
  attempt->createRequest = Ref::Make(request,false);
  Log("[NATIVE-CANDIDATE-BEGIN] key=%s request=%p sourceMesh=%p sourceNativeId=%d phase=Bundle frame=%d elapsedMs=%.6f productionPublished=0",attempt->plan->key.c_str(),request,attempt->source.Get(),attempt->oldId,RuntimeShapes::lastFrame,Ms(start));
}
struct Weight { float weights[4]; int indices[4]; };
static_assert(sizeof(Weight)==32,"BoneWeight public array ABI");
template<class T> void AppendArray(Bytes& bytes,void* array,size_t expected) {
  auto root = Ref::Make(array,false); auto values = RuntimeShapes::Array<T>(root.Get(),expected);
  const auto* raw = reinterpret_cast<const unsigned char*>(values.data());
  bytes.insert(bytes.end(),raw,raw+values.size()*sizeof(T));
}
std::string BaseDigest(void* mesh,const Plan& plan) {
  Require(RuntimeShapes::Alive(mesh) && String(RuntimeShapes::Call(RuntimeShapes::api.name,mesh)) == plan.marker &&
      RuntimeShapes::Scalar<int>(RuntimeShapes::api.vertexCount,mesh) == int(plan.spec.vertices) &&
      RuntimeShapes::Scalar<int>(RuntimeShapes::api.shapeCount,mesh) == int(plan.spec.channels.size()) &&
      RuntimeShapes::Scalar<int>(api.submeshes,mesh) == int(plan.submeshes) &&
      RuntimeShapes::Scalar<unsigned char>(RuntimeShapes::api.readable,mesh), "candidate Mesh native counts/name/readability differ");
  Bytes content;
  AppendArray<EiemShapeSnapshot::Vec3>(content,RuntimeShapes::Call(RuntimeShapes::api.vertices,mesh),plan.spec.vertices);
  AppendArray<EiemShapeSnapshot::Vec3>(content,RuntimeShapes::Call(RuntimeShapes::api.normals,mesh),plan.spec.vertices);
  AppendArray<EiemShapeSnapshot::Vec4>(content,RuntimeShapes::Call(RuntimeShapes::api.tangents,mesh),plan.spec.vertices);
  AppendArray<EiemCaptureSkin::Matrix>(content,RuntimeShapes::Call(RuntimeShapes::api.bindPoses,mesh),plan.bones);
  AppendArray<Weight>(content,RuntimeShapes::Call(RuntimeShapes::api.boneWeights,mesh),plan.spec.vertices);
  int submesh = 0; unsigned char applyBase = 1; void* args[] = {&submesh,&applyBase};
  AppendArray<int>(content,RuntimeShapes::Call(RuntimeShapes::api.indices,mesh,args),plan.indices);
  return Hash(content);
}
std::string ShapeDigest(void* mesh,const Plan& plan) {
  Bytes content;
  for (size_t i=0;i<plan.spec.channels.size();++i) {
    int index=int(i); void* args[] = {&index};
    Require(String(RuntimeShapes::Call(RuntimeShapes::api.shapeName,mesh,args)) == plan.spec.channels[i], "candidate native shape slot order differs");
    int frames = RuntimeShapes::Scalar<int>(RuntimeShapes::api.frameCount,mesh,args);
    Require(frames>0 && frames<=16,"candidate native shape frame count exceeds witness bound");
    AppendInteger(content,unsigned(frames),4);
    for (int frame=0;frame<frames;++frame) {
      void* weightArgs[] = {&index,&frame}; float weight = RuntimeShapes::Scalar<float>(RuntimeShapes::api.frameWeight,mesh,weightArgs);
      Require(std::isfinite(weight),"candidate shape frame weight nonfinite");
      auto raw = reinterpret_cast<const unsigned char*>(&weight); content.insert(content.end(),raw,raw+sizeof(weight));
      auto positions = Ref::Make(RuntimeShapes::VectorArray(plan.spec.vertices),false);
      auto normals = Ref::Make(RuntimeShapes::VectorArray(plan.spec.vertices),false);
      auto tangents = Ref::Make(RuntimeShapes::VectorArray(plan.spec.vertices),false);
      void* frameArgs[] = {&index,&frame,positions.Get(),normals.Get(),tangents.Get()};
      RuntimeShapes::Call(RuntimeShapes::api.frameVertices,mesh,frameArgs);
      for (auto array : {positions.Get(),normals.Get(),tangents.Get()})
        AppendArray<EiemShapeSnapshot::Vec3>(content,array,plan.spec.vertices);
    }
  }
  return Hash(content);
}
bool OwnsBundle(Attempt& value) {
  Require(value.bundle.Get() && RuntimeShapes::Alive(value.bundle.Get()), "candidate Bundle is not a live native object");
  auto id=RuntimeShapes::Scalar<int>(api.instanceId,value.bundle.Get());
  if (!id || id==value.originalBundleId || value.bundle.Get()==value.originalBundle.Get()) return false;
  std::lock_guard<std::mutex> lock(LoadingObservation::mutex);
  for (const auto& entry : LoadingObservation::bundles) if (entry.second.weak.Get()==entry.first && RuntimeShapes::Alive(entry.first))
    if (value.bundle.Get()==entry.first || RuntimeShapes::Scalar<int>(api.instanceId,entry.first)==id) return false;
  return true;
}
void UnloadOwnBundle(Attempt& value) {
  if (value.unloadCalled) return;
  auto bundle = value.bundle.Get(); if (!bundle) return;
  Require(value.bundleOwned, "candidate Bundle ownership is unverified; retain instead of unloading");
  Require(RuntimeShapes::Main(), "candidate Bundle cleanup outside Unity thread");
  unsigned char destroyAssets=0; void* args[] = {&destroyAssets};
  RuntimeShapes::Call(api.unload,bundle,args); value.unloadCalled=true;
  value.bundle={}; value.createRequest={}; value.assetRequest={}; value.path={}; value.label={};
  Log("[NATIVE-CANDIDATE-CLEANUP] unloadOwnBundle=1 unloadAllLoadedObjects=0 candidateMeshWitnessRetained=%d originalConsumerUntouched=1",int(value.candidate.Get()!=nullptr));
}
void Step(Attempt& value) {
  auto source = LiveSource(value); auto start = Clock();
  Require(RuntimeShapes::Alive(value.originalBundle.Get()) &&
      RuntimeShapes::Scalar<int>(api.instanceId,value.originalBundle.Get())==value.originalBundleId,
      "candidate original Bundle native lifetime changed");
  switch (value.phase) {
    case Phase::Bundle: {
      auto request = value.createRequest.Get(); Require(request,"candidate Bundle request root lost");
      if (!RuntimeShapes::Scalar<unsigned char>(api.done,request)) return;
      auto bundle = RuntimeShapes::Call(api.bundle,request); Require(bundle,"candidate native Bundle load failed");
      value.bundle=Ref::Make(bundle,false);value.bundleOwned=OwnsBundle(value);
      Require(value.bundleOwned,"candidate Bundle aliases an observed original native Bundle; retain without unload");
      void* args[] = {value.label.Get(),api.meshType.Get()};
      auto asset = RuntimeShapes::Call(api.assetLoad,bundle,args); Require(asset,"candidate Mesh async request null");
      value.assetRequest=Ref::Make(asset,false);value.phase=Phase::Asset;
      Log("[NATIVE-CANDIDATE-BUNDLE] bundle=%p assetRequest=%p serializedSha=%s phase=Asset elapsedMs=%.6f",bundle,asset,value.plan->candidateHash.c_str(),Ms(start)); return;
    }
    case Phase::Asset: {
      auto request=value.assetRequest.Get(); Require(request,"candidate asset request root lost");
      if (!RuntimeShapes::Scalar<unsigned char>(api.done,request)) return;
      auto mesh=RuntimeShapes::Call(api.result,request);
      Require(mesh && il2cpp_object_get_class(mesh)==api.meshClass && RuntimeShapes::Alive(mesh),"candidate returned asset is not a native Mesh");
      value.candidate=Ref::Make(mesh,false);value.newId=RuntimeShapes::Scalar<int>(api.instanceId,mesh);
      Require(mesh!=source && value.newId && value.newId!=value.oldId,"candidate Mesh aliases original native Mesh");
      value.phase=Phase::SourceBase;
      Log("[NATIVE-CANDIDATE-ASSET] mesh=%p nativeId=%d originalMesh=%p originalNativeId=%d nativeIdentityDistinct=1 originalBundleNativeIdPreserved=1 phase=SourceBase elapsedMs=%.6f",mesh,value.newId,source,value.oldId,Ms(start));return;
    }
    case Phase::SourceBase: value.baseDigest=BaseDigest(source,*value.plan);value.phase=Phase::CandidateBase;break;
    case Phase::CandidateBase: Require(BaseDigest(value.candidate.Get(),*value.plan)==value.baseDigest,"candidate native vertex/index/normal/tangent/bindpose/weight arrays differ");value.phase=Phase::SourceShapes;break;
    case Phase::SourceShapes: value.shapeDigest=ShapeDigest(source,*value.plan);value.phase=Phase::CandidateShapes;break;
    case Phase::CandidateShapes:
      Require(ShapeDigest(value.candidate.Get(),*value.plan)==value.shapeDigest,"candidate native shape names/frame weights/deltas differ");
      value.phase=Phase::Coexist;value.coexistStarted=GetTickCount64();
      Log("[NATIVE-CANDIDATE-CONTENT] vertices=%u bones=%zu indices=%zu shapes=%zu baseDigest=%s shapeDigest=%s nativeArraysEqual=1 immutableSourcePreserved=1 phase=Coexist",
          value.plan->spec.vertices,value.plan->bones,value.plan->indices,value.plan->spec.channels.size(),value.baseDigest.c_str(),value.shapeDigest.c_str());break;
    case Phase::Coexist:
      Require(RuntimeShapes::Alive(value.candidate.Get()) && RuntimeShapes::Scalar<int>(api.instanceId,value.candidate.Get())==value.newId,"candidate native Mesh witness lost");
      ++value.coexistSamples;
      if(GetTickCount64()-value.coexistStarted < 10000)return;
      UnloadOwnBundle(value);LiveSource(value);
      Require(RuntimeShapes::Alive(value.candidate.Get()) &&
          RuntimeShapes::Scalar<int>(api.instanceId,value.candidate.Get())==value.newId,
          "candidate Mesh witness lost after own Bundle Unload(false)");
      value.phase=Phase::Passed;
      Log("[NATIVE-CANDIDATE-PASS] key=%s scope=dependency-free-body-Mesh coexistSamples=%d elapsedMs=%llu originalNativeId=%d candidateNativeId=%d nativeArraysEqual=1 sharedDependenciesTested=0 materialTexturePrefabTested=0 instanceMigration=0 fullResourceReload=0",
          value.plan->key.c_str(),value.coexistSamples,(unsigned long long)(GetTickCount64()-value.started),value.oldId,value.newId);return;
    default:return;
  }
  Log("[NATIVE-CANDIDATE-STEP] nextPhase=%d frame=%d elapsedMs=%.6f",int(value.phase),RuntimeShapes::lastFrame,Ms(start));
}
// Drain abandoned async requests to native completion before releasing their
// roots/file locks or unloading this owned Bundle. There is no guessed timeout
// cancellation and no old-generation unload.
void Drain(Attempt& value) {
  if (value.phase!=Phase::Rejected) return;
  if (value.createRequest.Get() && !value.bundle.Get()) {
    if (!RuntimeShapes::Scalar<unsigned char>(api.done,value.createRequest.Get()))return;
    auto bundle=RuntimeShapes::Call(api.bundle,value.createRequest.Get());if(bundle){value.bundle=Ref::Make(bundle,false);value.bundleOwned=OwnsBundle(value);}
  }
  if (value.assetRequest.Get() && !RuntimeShapes::Scalar<unsigned char>(api.done,value.assetRequest.Get()))return;
  if(value.bundle.Get() && !value.bundleOwned) {
    value.nativeFailed=true;
    Log("[NATIVE-CANDIDATE-QUARANTINE] nativeBundleOwnershipVerified=0 unloadingRefused=1 rootsRetained=1 oldGenerationUnaffected=1");return;
  }
  UnloadOwnBundle(value);
  value.createRequest={};value.assetRequest={};value.path={};value.label={};
  // Retain a loaded witness, if any, without destroying Unity assets.
  if (value.plan) { auto copy=std::make_shared<Plan>(*value.plan);copy->file.reset();value.plan=copy; }
  value.nativeFailed=true;
  Log("[NATIVE-CANDIDATE-DRAINED] inFlightCompleted=1 oldGenerationRetained=1 nativeDestruction=0");
}
void Frame() {
  if(!enabled.load() || !RuntimeShapes::Main())return;
  if(!attempt && active.load()) {
    std::shared_ptr<const Plan> next;{std::lock_guard<std::mutex> lock(pendingMutex);next.swap(pending);}
    if(next)try{Begin(std::move(next));}catch(const std::exception& e){
      if(attempt)attempt->phase=Phase::Rejected;else busy=false;
      Log("[NATIVE-CANDIDATE-REJECT] phase=begin reason=%s originalConsumerUntouched=1",e.what());
    }
  }
  if(!attempt)return;
  if(attempt->phase==Phase::Passed || (attempt->phase==Phase::Rejected && attempt->nativeFailed))return;
  if(attempt->phase==Phase::Rejected){try{Drain(*attempt);}catch(const std::exception& e){if(++attempt->polls<=4)Log("[NATIVE-CANDIDATE-DRAIN-GAP] reason=%s rootsRetained=1",e.what());}return;}
  try{
    Require(active.load(),"candidate validation plugin disabled; drain owned requests");
    Require(GetTickCount64()-attempt->started<60000,"candidate validation deadline exceeded; drain owned requests");
    Step(*attempt);
  }catch(const std::exception& e){attempt->phase=Phase::Rejected;Log("[NATIVE-CANDIDATE-REJECT] phase=runtime reason=%s rootsRetainedUntilNativeCompletion=1 productionUnaffected=1",e.what());}
}
} // namespace NativeCandidate
