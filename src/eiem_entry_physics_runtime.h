#pragma once
#include <atomic>
#include <mutex>
#include <unordered_map>
#include "eiem_physics_component_identity.h"

namespace PhysicsRuntime {
using WeightSetter = void (*)(void*, float, void*);
struct Component {
  RuntimeShapes::Ref weakObject;
  bool authored = false;
};

std::unordered_map<void*, Component> components;
std::mutex componentsMutex;
ULONGLONG nextPrune = 0;
unsigned weightCalls = 0;
std::atomic<bool> classificationGapReported{false};
constexpr size_t MaximumTrackedComponents = 4096;
constexpr ULONGLONG PruneIntervalMs = 5000;
void* getName = nullptr;
WeightSetter originalSetWeight = nullptr;
WeightSetter originalSetWeightIgnorePlaying = nullptr;
std::atomic<bool> enabled{false};

void PruneExpiredLocked() {
  const auto now = GetTickCount64();
  if (now < nextPrune) return;
  nextPrune = now + PruneIntervalMs;
  for (auto it = components.begin(); it != components.end();) {
    if (!it->second.weakObject.Get()) it = components.erase(it);
    else ++it;
  }
}

bool IsAuthoredGroup(void* self) noexcept {
  if (!self || !enabled.load(std::memory_order_acquire)) return false;

  try {
    {
      std::lock_guard<std::mutex> lock(componentsMutex);
      if ((++weightCalls & 255u) == 0) PruneExpiredLocked();
      auto found = components.find(self);
      if (found != components.end()) {
        if (found->second.weakObject.Get() == self) return found->second.authored;
        components.erase(found);
      }
    }

    const auto name = String(RuntimeShapes::Call(getName, self));
    const bool authored = EiemPhysicsComponentIdentity::IsGeneratedName(name);
    {
      std::lock_guard<std::mutex> lock(componentsMutex);
      auto found = components.find(self);
      if (found != components.end() && found->second.weakObject.Get() == self)
        return found->second.authored;
      if (components.size() >= MaximumTrackedComponents) return authored;
      components.insert_or_assign(self, Component{RuntimeShapes::Ref::Make(self, true), authored});
    }
    if (authored)
      Log("[PHYSICS-RUNTIME-POLICY] component=%s gameSetter=skipped authoredValue=serialized", name.c_str());
    return authored;
  } catch (const std::exception& error) {
    if (!classificationGapReported.exchange(true, std::memory_order_relaxed))
      Log("[PHYSICS-RUNTIME-GAP] phase=component-classification reason=%s policy=gameSetterFallback", error.what());
    return false;
  }
}

void SetWeight(void* self, float value, void* methodInfo) {
  if (!IsAuthoredGroup(self) && originalSetWeight)
    originalSetWeight(self, value, methodInfo);
}

void SetWeightIgnorePlaying(void* self, float value, void* methodInfo) {
  if (!IsAuthoredGroup(self) && originalSetWeightIgnorePlaying)
    originalSetWeightIgnorePlaying(self, value, methodInfo);
}

void Install(void** assemblies, size_t count) {
  try {
    auto cloth = FindClass("BeyondDynamicBone", "BeyondBoneCloth", assemblies, count);
    auto object = FindClass("UnityEngine", "Object", assemblies, count);
    Require(cloth && object, "Physics runtime classes unavailable");
    if (!RuntimeShapes::Ref::weakHandle)
      RuntimeShapes::Ref::weakHandle = (decltype(RuntimeShapes::Ref::weakHandle))GetProcAddress(hGA, "il2cpp_gchandle_new_weakref");
    Require(RuntimeShapes::Ref::weakHandle && il2cpp_gchandle_get_target && il2cpp_gchandle_free,
            "Physics runtime weak object handles unavailable");
    getName = Exact(object, "get_name", {}, "System.String", false);
    Require(getName && il2cpp_runtime_invoke, "UnityEngine.Object.get_name unavailable");
    Require(Entry::Install(cloth, "SetClothSimulateWeight", {"System.Single"}, "System.Void", false,
                           (void*)SetWeight, (void**)&originalSetWeight),
            "BeyondBoneCloth.SetClothSimulateWeight hook unavailable");
    Require(Entry::Install(cloth, "SetClothSimulateWeightIgnorePlaying", {"System.Single"}, "System.Void", false,
                           (void*)SetWeightIgnorePlaying, (void**)&originalSetWeightIgnorePlaying),
            "BeyondBoneCloth.SetClothSimulateWeightIgnorePlaying hook unavailable");
    enabled.store(true, std::memory_order_release);
    Log("[PHYSICS-RUNTIME-READY] authoredGroupName=__EFF_<package>_<group-id> policy=preserveSerializedClothWeight "
        "nativeCloth=gameControlled cache=weak-pruned maxTracked=%zu", MaximumTrackedComponents);
  } catch (const std::exception& error) {
    enabled.store(false, std::memory_order_release);
    Log("[PHYSICS-RUNTIME-GAP] phase=install reason=%s gamePhysicsPreserved=1", error.what());
  }
}
}  // namespace PhysicsRuntime
