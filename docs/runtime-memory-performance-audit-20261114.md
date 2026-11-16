# Runtime Memory and Performance Audit

Date: 2026-11-14

Scope: the latest Endfield crash, demand-driven loading, resource lifetime, and available performance regressions. This audit is read-only except for this report; it does not change or deploy the DLL.

## Later 20:41 Session (Separate Failure)

The later 20:37 DLL deployment crashed during scene entry before F10. Preserve the distinction from the 13:15 Vulkan failure below: the new Player log ends with two `Commit memory failed ... 0x1000000 (:870)` messages and contains no Vulkan image-allocation failure. Current-binary inspection traces this message to the native memory commit path and `VirtualAlloc` with `MEM_COMMIT`. Windows errors 8, 14 and 1455 map to the reported internal value. This is evidence of a failed host-memory commitment, not a crash-time measurement of all system or Mod allocations. The local binary hash and instructions are in `diagnostics/issue6-disappearance-20261114/unity-commit-failure-disassembly.txt`.

The later graph still submits 29 inputs exactly once, 15,955,712 bytes in total; all complete, and the particle attachment starts. Input byte size does not bound Unity's complete resource residency. A reproduced routing defect explains the synthetic manifest misses: unresolved target discovery re-entered already-published synthetic keys during native proxy selection. Both selection and preparation now exclude those inputs from native discovery. The regression first failed, then passed, and the full reload suite passes. That routing correction is not proof that the memory crash is fixed.

`tools/diagnostics/watch_runtime_memory.py` records a half-second system commit/process/GPU time series without injection or game changes. A 20:51 pre-game sample had 31,477,956,608 committed bytes against a 39,135,510,528-byte limit, leaving 7,657,553,920 bytes. Blender alone held 3,744,538,624 private bytes. These describe the sample, not the preceding crash peak. The next live test must correlate the game load with the peak and retained levels before attributing causality.

## Later 20:57 Reproduction with Independent Commit Sampling

The discovery correction eliminated all 35 native synthetic-manifest misses, and the user still reproduced the crash without F10. The half-second trace captured system commitment at 37.433 GiB against a 37.467 GiB limit, with only 34.828 MiB remaining; physical availability was about 6.2 GiB and GPU free memory about 1.97 GiB. The external sampler itself then failed its process snapshot with Windows error 1455 (commitment limit/pagefile too small). This independently confirms the system-wide failure mechanism in this run.

Immediately around graph publication (20:57:16.374 to 20:57:16.870), game private commitment rose from 5.095 to 6.871 GiB. This temporal correlation needs a matched no-EFF baseline; it is not an allocation-stack attribution. The 29 inputs total only 15.2 MiB, but that does not excuse or bound native resource/driver residency. Effects had reached attachment binding only in this later log. Do not copy the previous session's successful play into this one.

Settings name a 17,217–32,228 MiB F: pagefile, but both live WMI usage and native SystemPageFileInformation report only C:. C: had about 3 GiB free. No system configuration was changed; the reason F: was not active remains unverified. The sampler has been hardened to preserve a system sample when process enumeration fails with 1455, and its forced-error check passes. Session artifacts and the temporary no-EFF control journal are in `diagnostics/issue6-disappearance-20261114/`.

## 21:11 No-EFF Control Outcome and Current Paging State

The user confirmed that the no-EFF control also crashed. The preserved `diagnostics/issue6-disappearance-20261114/Player.no-eff-user-confirmed-crash.log` contains the same native commit-memory failure. Only `plugin/eff.dll` was temporarily renamed between 21:02:45 and 21:11:11; Mods, Blender, proxy DLLs and other plugins were retained. This is a no-EFF control, not a fully vanilla configuration. The EFF log did not begin another session during the control. Module enumeration returned no modules, so it provides no successful module-level verification.

The captured control process reached about 8.526 GiB private commitment, and minimum sampled system commit headroom was about 0.360 GiB. The monitor stopped at a process gap before the final user-confirmed report. These are captured control measurements, not exact final-crash peaks; unmatched runs cannot quantify EFF's incremental allocation. The control establishes that the same failure can occur without EFF, while leaving EFF's possible residency/peak contribution unquantified.

At 21:14, C: had 10.81 GiB free, compared with roughly 3 GiB during the earlier failing run. Live paging usage still listed only C:, allocated at 3,452 MiB. F: still had a configured 17,217–32,228 MiB pagefile but no active usage entry. The game was no longer running, and system commitment was approximately 24.12 / 35.11 GiB. No files were deleted, paging settings changed, or restart performed. Freeing disk space alone is not proof that the configured F: pagefile becomes active; activation and resulting commit headroom require verification.

The same corrected production DLL was restored at 21:11:11 and independently rechecked at 21:14: SHA256 `6cad5a57f8d6c489f9b9e820bed72a1c1daecee295d0f3bf9721b206b41ade7f`. The temporary disabled filename is absent. F10 and axis acceptance remain pending until the system commitment condition is addressed.

## 21:34 Authorized Cleanup and Live Paging Activation

The user explicitly authorized deleting C: crash reports and historical EFF caches, and activating paging on another drive. Maintenance removed 21,151 files with 5,466,409,422 logical bytes (about 5.09 GiB). This includes identified Windows/user/Endfield crash reports, EFF/EIEM temporary test/release directories, texture/index caches and historical addon update backups. The elevated follow-up checked protected report locations and Windows Temp dump files; no deletion failures were recorded. C: had about 16.76 GiB free at the final snapshot; unrelated disk activity means this is not an exact cleanup-attributable delta.

Potentially referenced Blender effect sources/current build data, 132,493,394 bytes, were copied to `E:/EIEM_Workspace/.cache/blender-preserved-sources/`, verified file-by-file with hashes, and then removed from C:. Directory junctions retain the original paths. Texture compression, AnimeStudio index and native demand-snapshot directories now resolve to E: cache locations. Installed addons, program settings, resource-root metadata, project files and Blender recovery files were retained. Do not follow these junctions when performing a later C:-only cleanup.

F: now has persistent initial/maximum settings of 16,384/32,768 MiB. The existing C: system-managed setting was preserved. An elevated native create operation returned success; live paging usage subsequently showed F: allocated at 16,384 MiB, and the system commitment limit increased from approximately 35.11 to 51.11 GiB without reboot. This is verified live activation, not just a settings write. The final snapshot showed about 29.54 GiB committed against the 51.11 GiB limit.

Scripts, the exact cleanup manifest, paging before/after states and verification are in `diagnostics/system-maintenance-20261114/`. No game or Blender process was forcibly closed, no reboot was issued, and the production DLL hash is unchanged. The improved environment has not yet been followed by a fresh in-game startup/F10/axis acceptance run.

## Original 13:15 Finding

The crash is strongly associated with the `langzhifei` weapon replacement loading a 2048 x 2048 BC7 sRGB texture. The Unity log reports Vulkan result `-2` (`VK_ERROR_OUT_OF_DEVICE_MEMORY`) while creating an image with 2048 x 2048 dimensions, 12 mips, one layer, and Vulkan format 146 (BC7 sRGB). It also reports 3,686,921,328 bytes of VMA graphics-memory blocks. That byte count describes the allocator's reported graphics blocks; it is not the requested size of the failing image.

The loaded `langzhifei` Mod has exactly one DDS with that signature: `textures/mat_30.001_01.dds`, 5,592,580 bytes on disk. At 13:15:45.738, EFF submitted a 5,598,604-byte native input, which is the DDS plus its Texture2D serialization. The Mod's INI binds that DDS as `TextureMat_30_001_01`. The crash occurs in the same game session while the game's `WeaponComponent` loads the weapon. This is a high-confidence match to the image that failed, though the crash log does not print the logical Texture2D name.

The Mod therefore participated in the failing GPU allocation: its matched DDS reached the game's normal Texture2D deserializer and GPU upload. Its encoded size is about 5.6 MB, not gigabytes. The evidence does not show that EFF allocated the reported 3.69 GB itself. It is consistent with the game or driver having too little suitable GPU memory available when this ordinary texture was uploaded. The separate `Commit memory failed` message does not identify a Windows commit failure code or prove system-RAM exhaustion.

At the time of this audit, `nvidia-smi` reports an 8,188 MiB GPU, not 4 GiB; `Win32_VideoController.AdapterRAM` truncates the capacity on this machine. One post-crash snapshot showed 2,790 MiB used and 5,159 MiB free; neither that nor later live readings measure the crash-time peak. Blender PID 23016 is still running, so its possible contribution to GPU usage at crash time cannot be reconstructed from these logs.

## Loading Path

Startup is demand-driven. At 13:13:51, EFF read declarations for three Mods and 16 targets in 57.488 ms; mesh, texture, and effect payload bytes were zero, compiled inputs read were zero, and consumer-PFB enumeration was zero. Repeated startup counters stayed at zero load submissions until the weapon was requested.

When the weapon request matched, EFF prepared one author Mod with 28 Parts and streamed 1,862,092 author bytes. It then submitted 29 unique Bundle inputs totalling 15,952,656 bytes. All 29 completed, with zero failures and zero pending loads. The route was `proxy-sync-lifecycle`, and the game BundleProxy retained ownership of each request. This is a concentrated load on a real weapon request, not a startup preload of all Mod payloads.

The implementation wraps the game's request lifecycle rather than inventing a second asset-object loader: it prepares a bounded serialized input graph, exposes file-backed spans through a seekable stream, and asks Unity to load the AssetBundle. The game's BundleProxy continues to own request state and its normal deserialization creates the Texture2D and graphics allocation. EFF retains the stream/view until Unity unload completes, then removes unreachable graph inputs and unused author caches. This follows the game's loading and lifetime boundary, but EFF still owns the Mod-to-game dependency graph and stream adapter.

The 29 input sizes are not 15.95 MB of one EFF heap allocation. `LoadView` uses an empty managed `MemoryStream` facade and serves reads from immutable file-backed spans. Unity may still decompress resources and allocate CPU/GPU objects through its normal pipeline. A replacement can therefore add residency even though EFF does not copy every bundle into one large byte array.

## Risks Found

- The weapon request added 29 Bundle loads within roughly 0.2 seconds. This is proportional to the matched weapon dependency graph, but it can create a short resource-upload peak. The current production log measures counts and input bytes, not per-image GPU residency or headroom before upload.
- The crash-time Player log contains 35 `Can not find bundle hashName` Critical messages. Converting the logged signed 64-bit hashes to the runtime's 16-digit key format maps them to 28 of the 29 submitted synthetic Bundle keys. The same load session has 37 `DEMAND-TARGET-DISCOVERY-GAP` records: 34 `native CAB changed requested bundle within one manifest generation`, two bounded inspection-read-budget failures, and one attempt to open a generated `eff/2/root/...` name from the game VFS. The fallback records `originalInputPreserved=1`; the replacement still completes. This indicates that the game manifest lookup path is seeing synthetic identities and that some discovery paths fall back. The repeated lookups and incomplete native-CAB discovery need focused integration review; this audit does not establish that either caused the GPU allocation failure.
- No `DEMAND-LIFETIME-RECLAIM` event was recorded before the crash, and the final stats show zero proxy retire observations. The session ended shortly after loading the active weapon, so this is not evidence of a leak. It also cannot validate retirement across world/UI owners or a long session.
- The active effect Mod `lizi` attached to the matched weapon in the same session. Its attachment became ready, but the available log does not attribute GPU bytes to the effect versus the weapon materials. `ZFY_animation_test` was declared at startup; the recorded matching author preparation identifies `langzhifei`, but there is no crash-time per-Mod GPU accounting.
- The error is in the GPU allocation path. The post-crash system-memory snapshot alone cannot establish or exclude a transient host commit peak.

## Regression Results

- `tools/diagnostics/build_demand_resource_regressions.cmd`: passed shared dependency expansion, cross-PFB deduplication, dependency closure, cycle rollback, graph retirement, and Material removal checks.
- `tools/diagnostics/build_load_performance_smoke.cmd`: passed timing aggregation, main/background partition, byte accounting, percentile windows, and whole-process memory labeling. A 100,000-scope diagnostic run measured 0.067 microseconds average instrumentation overhead on this machine; this is instrumentation cost, not the cost of a game resource load.
- `python -m unittest test_performance_collector test_analyze_scene_performance` from `tools/diagnostics`: 6 tests passed.
- Runtime counters for the crash session: 33,922 queue calls, 33,124 fast rejects, one relevant queue expansion, 29 submits/completions, zero load failures, and zero pending loads. These show no repeated dependency expansion in the logged session; they do not measure GPU upload duration.

## Limits and Next Measurement

The available logs do not include a GPU-memory time series at the failing frame, the game/driver allocation stack with the logical texture name, or a matched no-Mod baseline. The code regressions exercise the graph and counters, not Unity's live Texture2D upload or GPU resource retirement.

To establish whether the Mod merely crossed an already-low headroom threshold or causes excess residency, compare the same scene and Blender state with `langzhifei` disabled and enabled, sampling total GPU memory at short intervals. Keep `lizi` and other Mods identical between those two runs. In a separate run, compare with `lizi` disabled to isolate its effect. Capture the peak and post-owner-retirement level; do not infer a leak from the active weapon retaining its textures.

No code change or DLL deployment is justified by the available evidence alone. The next code investigation should first explain the synthetic Manifest misses and native-CAB discovery gaps, then add per-request texture identity/estimated GPU bytes and retirement correlation if those misses are not an intentional fallback.
