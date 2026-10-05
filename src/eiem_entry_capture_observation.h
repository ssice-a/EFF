#pragma once
// Optional read-only observation of the game's own capture eligibility test.
// This never forces completion, writes a Mesh, or supplies alternate geometry.
namespace CaptureObservation {
using FinishedFn = bool (*)(void*, void*);
FinishedFn originalFinished = nullptr;
using FinishedObserver = void (*)(void*, bool);
FinishedObserver afterFinished = nullptr;
void *nameMethod = nullptr, *meshMethod = nullptr, *shapeMethod = nullptr, *readableMethod = nullptr;
std::atomic<unsigned> samples{0};
std::mutex seenMutex;
std::set<std::pair<void*, bool>> seen;
bool Finished(void* self, void* methodInfo) {
  void* caller = _ReturnAddress();
  bool result = originalFinished(self, methodInfo);
  if (afterFinished) afterFinished(self, result);
  if (!self || samples.load() >= 256) return result;
  bool first;
  { std::lock_guard<std::mutex> guard(seenMutex); first = seen.emplace(self, result).second; }
  if (!first || samples.fetch_add(1) >= 256) return result;
  try {
    auto mesh = SkinningObservation::Invoke(meshMethod, self);
    auto rendererName = String(SkinningObservation::Invoke(nameMethod, self));
    auto meshName = mesh ? String(SkinningObservation::Invoke(nameMethod, mesh)) : std::string("<null>");
    auto shapes = mesh ? SkinningObservation::Scalar(shapeMethod, mesh) : -1;
    auto readable = mesh && SkinningObservation::Boolean(readableMethod, mesh);
    Log("[CAPTURE-ELIGIBILITY] renderer=%p name=%s mesh=%p meshName=%s shapes=%d readable=%d "
        "SkinMatricesRequestFinished=%d callerRva=%llX readonly=1",
        self, rendererName.c_str(), mesh, meshName.c_str(), shapes, readable, result,
        (unsigned long long)((uintptr_t)caller-(uintptr_t)hGA));
  } catch (const std::exception& error) { Log("[CAPTURE-OBSERVATION] read-failed=%s originalResult=%d", error.what(), result); }
  return result;
}
void InstallOptional(void** assemblies, size_t count) {
  if (!afterFinished && !std::filesystem::exists(gameDirectory/"plugin/resource-input-diagnostics/capture-observation.enabled")) return;
  try {
    auto object = FindClass("UnityEngine", "Object", assemblies, count);
    auto skin = FindClass("UnityEngine", "SkinnedMeshRenderer", assemblies, count);
    auto mesh = FindClass("UnityEngine", "Mesh", assemblies, count);
    nameMethod = Exact(object, "get_name", {}, "System.String", false);
    meshMethod = Exact(skin, "get_sharedMesh", {}, "UnityEngine.Mesh", false);
    shapeMethod = Exact(mesh, "get_blendShapeCount", {}, "System.Int32", false);
    readableMethod = Exact(mesh, "get_isReadable", {}, "System.Boolean", false);
    Require(nameMethod && meshMethod && shapeMethod && readableMethod, "capture readback signature unavailable");
    Install(skin, "SkinMatricesRequestFinished", {}, "System.Boolean", false,
            (void*)Finished, (void**)&originalFinished);
    Log("[CAPTURE-OBSERVATION] installed=%d sampleLimit=256 readonly=1 resourceSubmissionUnaffected=1", !!originalFinished);
  } catch (const std::exception& error) { Log("[CAPTURE-OBSERVATION] optional-install-failed=%s resourceSubmissionUnaffected=1", error.what()); }
}
}
