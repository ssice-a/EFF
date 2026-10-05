#pragma once
#include <atomic>
#include <cstdint>
using EiemCameraMethod = void (*)(void*,void*);
// The original evaluation must return before clearing the same camera. Its
// exception is propagated; clear receives its own MethodInfo, not pitch's.
template<class Enabled,class Log>
void EiemEvaluateCameraFade(void* camera,void* pitchInfo,EiemCameraMethod original,
    EiemCameraMethod clear,void* clearInfo,Enabled enabled,Log log,
    std::atomic<uint64_t>& evaluations,std::atomic<uint64_t>& clears) {
  original(camera,pitchInfo);
  if (evaluations.fetch_add(1)==0) log("[CAMERA-FADE] observed CameraMono._ProcessDitherByPitch returned");
  if (!enabled()) return;
  clear(camera,clearInfo);
  if (clears.fetch_add(1)==0) log("[CAMERA-FADE] cleared CameraMono.ForceClearDither returned");
}
