# Format 3 Transition Status

The target contract is defined by [ADR-0001](adr/0001-authoritative-mod-directory.md)
and [ADR-0002](adr/0002-runtime-resource-files.md). This page records the
current implementation and acceptance gaps; it does not define another format.

## Current State

| Endpoint | Current implementation | Remaining acceptance |
|---|---|---|
| DLL | Normal build selects the Format 3 demand-loading path only. The runtime reads declared resources on demand and hands modified serialized inputs to the game's normal deserializer. The user has confirmed in-game Mesh replacement and both shape-key controls. | Record a controlled performance baseline; verify DDS replacement followed by F10, Effects, and resource retirement across map/party/UI transitions. GPU memory and Unity async upload require external measurement. |
| Blender | Full export writes Format 3 directly. Python writes the manifest, Material/Physics/Effect/Controls resources and DDS files; the typed author DLL writes Mesh resources and empty-image Texture2D scaffolds. Import accepts Format 3. | Run Blender 5.2 UI round-trip on the current EFFSOURCE project. Blender-dependent registration and round-trip tests have not run in this pass. |
| Mod Manager | Existing repair primitives can discover indexes, read the selected game's VFS/Bundles, and publish files transactionally. | One-click Format 2 to Format 3 migration is not implemented. DLL and Blender do not read legacy Mod formats. |

## Verification

The following checks passed for the current Format 3 changes:

- Blender-independent Format 3 publication tests: 15 passed.
- Native pack and resource codec tests: 52 passed.
- Shared tools tests: 9 passed.
- Catalog, manifest, resource envelope/metadata, native random-access,
  static assembly, shape bridge, and author ABI smoke checks passed.
- `git diff --check` passed in both the main repository and Blender submodule.

The Blender 5.2 UI round-trip was not run; the system Python environment does
not provide `bpy`. A Format 3 `load-perf` development DLL was built and staged
in the game plugin directory for the first controlled in-game baseline; it is
not a production build.

## Performance Readiness

The first in-game baseline can start with `build.bat load-perf` and the existing
process-level sampling. Record DLL and Mod hashes, game version, scene, and
timestamps for each run. The results cover the whole game process and must not
be reported as EFF-only CPU or memory costs.

Current telemetry can compare aggregate demand preparation, file/request
timing, main-thread timing, process CPU/memory/I/O, and frame-interval buckets.
It does not yet split demand preparation into VFS parsing, target selection,
author-file reads, native assembly, and submission, or report bytes per stage.
That limits attribution but does not block a first end-to-end baseline. Add
low-overhead stage counters only if the baseline identifies preparation as a
material cost. GPU memory and Unity deserialization/upload time need external
GPU/frame tooling.

Use the scenario matrix in
[Format 3 architecture and performance preparation](format3-input-architecture-review.md).
