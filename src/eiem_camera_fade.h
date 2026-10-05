#pragma once
#include <atomic>
#include "il2cpp_api.h"
#include "eiem_config.h"

// Managed zero-argument instance methods receive self and MethodInfo.
// Retain metadata only, never Unity camera/Renderer objects.
#include "eiem_camera_fade_core.h"
static EiemCameraMethod s_eiemCameraPitchOriginal = nullptr;
static EiemCameraMethod s_eiemCameraForceClear = nullptr;
static void *s_eiemCameraForceClearInfo = nullptr;
static std::atomic_bool s_eiemCameraFadeInstalled{false};
static std::atomic<uint64_t> s_eiemCameraEvaluations{0}, s_eiemCameraClears{0};

static void EiemReportCameraFade() {
  Log("[CAMERA-FADE] status installed=%d enabled=%d evaluations=%llu clears=%llu",
      s_eiemCameraFadeInstalled.load(),
      s_eiemCameraFadeInstalled.load() &&
          g_pluginActive && EiemGetGlobalConfig().disableCameraFade,
      s_eiemCameraEvaluations.load(), s_eiemCameraClears.load());
}

static void EiemCameraPitchHook(void *camera, void *methodInfo) {
  EiemEvaluateCameraFade(camera,methodInfo,s_eiemCameraPitchOriginal,s_eiemCameraForceClear,
      s_eiemCameraForceClearInfo,[] {return g_pluginActive && EiemGetGlobalConfig().disableCameraFade;},
      Log,s_eiemCameraEvaluations,s_eiemCameraClears);
}

static bool InitEiemCameraFade(void **assemblies, size_t count) {
  if (s_eiemCameraFadeInstalled.load()) return true;
  void *klass = FindClass("Beyond.Gameplay.View", "CameraMono", assemblies, count);
  void *pitch = FindMethod(klass, "_ProcessDitherByPitch", 0);
  void *clear = FindMethod(klass, "ForceClearDither", 0);
  if (!pitch || !clear || !((MInfo *)pitch)->mp || !((MInfo *)clear)->mp) {
    Log("[CAMERA-FADE] Unavailable: CameraMono/pitch/clear metadata or address missing class=%p pitch=%p clear=%p",
        klass, pitch, clear);
    return false;
  }
  s_eiemCameraForceClearInfo = clear;
  s_eiemCameraForceClear = (EiemCameraMethod)((MInfo *)clear)->mp;
  void *target = ((MInfo *)pitch)->mp;
  MH_STATUS status = MH_CreateHook(target, (void *)EiemCameraPitchHook,
                                  (void **)&s_eiemCameraPitchOriginal);
  if (status == MH_OK) {
    status = MH_EnableHook(target);
    if (status != MH_OK) MH_RemoveHook(target);
  }
  if (status != MH_OK) {
    s_eiemCameraPitchOriginal = nullptr;
    s_eiemCameraForceClear = nullptr;
    s_eiemCameraForceClearInfo = nullptr;
    Log("[CAMERA-FADE] Unavailable: CameraMono hook failed status=%d", (int)status);
    return false;
  }
  s_eiemCameraFadeInstalled.store(true);
  Log("[CAMERA-FADE] Installed CameraMono._ProcessDitherByPitch -> ForceClearDither; gameplay visual result unverified");
  EiemReportCameraFade();
  return true;
}
