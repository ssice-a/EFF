#pragma once
// Independent CameraMono adapter; no Mesh/control/capture object state.
namespace EntryCameraFade {
EiemCameraMethod original=nullptr,clear=nullptr;
void* clearInfo=nullptr;
std::atomic<bool> hooked{false};
std::atomic<uint64_t> evaluations{0},clears{0};
void Report() {
  Log("[CAMERA-FADE] status installed=%d enabled=%d evaluations=%llu clears=%llu",
      int(hooked.load()),int(active && settings.Get().disableCameraFade),evaluations.load(),clears.load());
}
void Evaluate(void* camera,void* pitchInfo) {
  EiemEvaluateCameraFade(camera,pitchInfo,original,clear,clearInfo,
      [] {return active && settings.Get().disableCameraFade;},Log,evaluations,clears);
}
void InstallOptional(void** assemblies,size_t count) {
  if(hooked) return;
  try {
    auto klass=FindClass("Beyond.Gameplay.View","CameraMono",assemblies,count);
    auto pitch=Exact(klass,"_ProcessDitherByPitch",{},"System.Void",false);
    clearInfo=Exact(klass,"ForceClearDither",{},"System.Void",false);
    Require(pitch && clearInfo && ((MInfo*)pitch)->mp && ((MInfo*)clearInfo)->mp && ((MInfo*)pitch)->mp!=((MInfo*)clearInfo)->mp,
            "exact CameraMono pitch/clear signature unavailable");
    clear=(EiemCameraMethod)((MInfo*)clearInfo)->mp;
    Require(Install(klass,"_ProcessDitherByPitch",{},"System.Void",false,(void*)Evaluate,(void**)&original),
            "CameraMono hook unavailable");
    hooked=true;
    Log("[CAMERA-FADE] Installed independent CameraMono original -> ForceClearDither; runtime visual verification pending");
    Report();
  } catch(const std::exception& e) {Log("[CAMERA-FADE] Unavailable reason=%s resourceModulesPreserved=1",e.what());}
}
} // namespace EntryCameraFade
