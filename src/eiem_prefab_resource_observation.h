#pragma once

#if defined(EIEM_PREFAB_RESOURCE_OBSERVATION_BUILD) && !defined(EIEM_NATIVE_MESH_ENTRY_TRACE_BUILD)
#error PFB resource observation must use the source-only Mesh observation profile.
#endif
#if defined(EIEM_PREFAB_RESOURCE_OBSERVATION_BUILD)
static_assert(!kEiemValidationIdentityProbe,
              "PFB evidence must not enable the broad identity census.");
#endif

// Event evidence, not a resource writer or a per-frame scene census. All
// addresses are logged as values; no native fields are guessed or modified.
// A request/instance event does not prove that an effect was actually drawn.
static void EiemObservePrefabResourceEvent(const char *event, const char *path,
                                           int64_t hash, void *owner,
                                           void *result, uint32_t uid = 0) {
#if defined(EIEM_PREFAB_RESOURCE_OBSERVATION_BUILD)
  if (g_shutdownRequested) return;
  // Capture all PFB paths, including unnamed/common effects. Do not filter
  // by a character/effect name, and keep unresolved hashes as explicit gaps.
  const bool unresolved = !path || !path[0] || path[0] == '<';
  const bool instanceEvent = strstr(event, "prefab-instance") ||
      strstr(event, "persistent-pool");
  const bool relevant = instanceEvent || unresolved || strstr(path, ".prefab") ||
      strstr(path, ".pfb") || strstr(path, "postmodel") ||
      strstr(path, "lizhiyan") || strstr(path, "canying");
  if (!relevant) return;
  static volatile LONG64 sequence = 0;
  Log("[PFB-RESOURCE-EVENT] seq=%lld tick=%llu tid=%lu event=%s "
      "path=\"%s\" pathStatus=%s hash=%lld owner=%p result=%p uid=%u "
      "identityTextMeaning=resource-path-or-proxy-log-name "
      "evidence=resource-or-instance-event drawUseProven=0",
      (long long)InterlockedIncrement64(&sequence),
      (unsigned long long)GetTickCount64(), GetCurrentThreadId(), event,
      unresolved ? "?" : path, unresolved ? "unresolved" : "observed",
      (long long)hash, owner, result, uid);
#else
  (void)event; (void)path; (void)hash; (void)owner; (void)result; (void)uid;
#endif
}
