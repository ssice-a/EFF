# DLL architecture

The production translation unit is `src/eiem_resource_entry.cpp`. The retired
`eiem.cpp`, base-resource setter backend, render executor and restore ledger are
no longer part of the working tree or production build. On 2026-10-06 the game
installation uses the production DLL and structure declaration with
F10 reload and Insert controls. Historical v35c results belong to that binary.

## Boundaries and dependency direction

```text
AnimeStudio source exchange + existing author index
  -> Blender-local author compiler + small source preparer
  -> independent Mod: mod.ini + .mesh/.mat/.tex + source-inputs.bin + compiled.bin
  -> DLL compiled-record composition and proven byte rule application
  -> Unity-thread native load, verification and game-owned owner rebuild
  -> runtime graph, controls, settings and future-instance routes
```

| Boundary | Modules | Responsibility |
|---|---|---|
| Host | `eiem_resource_entry.cpp`, `il2cpp_api.h`, `applepie_mgr.h` | Exact managed bindings, loading hooks, worker and host integration |
| Author graph | `eiem_resource_directory.h`, `eiem_expression.h`, `eiem_keys.h` | Strict format-2 resources, references, expressions and keys |
| Source codec | `eiem_native_input.h`, `eiem_serialized_writer.h`, `eiem_endfield_input.h` | Decode/write source objects and loading envelopes |
| Static source selection | `eiem_static_resource_sources.h` (compiler), `StaticModSourceCommand.cs` (author tool) | Reuse the existing author index, read selected game ranges, resolve exact Mesh/Prefab consumers before publication; no runtime catalog queries |
| Offline assembly | `eiem_mesh_input_builder.h`, `eiem_renderer_input_builder.h`, `eiem_texture_input_builder.h` | Build geometry, Renderer expansion and Material/Texture references outside the game frame |
| Package and cache | `eiem_composed_package.h`, `eiem_loading_generation.h`, `eiem_resource_reload_plan.h`, `eiem_offline_resource_package.h`, `eiem_offline_cache.h`, `eiem_package_protocol.h` | Author-set composition, complete generation, field contracts, hashes, assembly expectations, cache ownership and publication |
| Native transaction | `eiem_native_bundle_loader.h`, `eiem_entry_full_resource_reload.h`, `eiem_entry_ui_models.h` | Hold files/GC roots, load isolated Bundle/PFB closure, verify and migrate known game owners |
| Controls/settings | `eiem_entry_controls.h`, `eiem_entry_preferences.h`, `eiem_entry_reload.h`, `eiem_global_settings.h`, `eiem_entry_settings.h` | Shared Mod numbers, independent Renderer bindings, persistent preferences and settings publication |
| Built-in panel | `eiem_control_panel.h`, `eiem_control_panel_win32.h` | Immutable snapshots, bounded versioned commands, selected-Mod input scope and independent native Windows controls |
| Shapes/capture | `eiem_entry_runtime_shapes.h`, `eiem_entry_capture_observation.h` | Public visibility/shape operations and native capture adaptation |
| Observation | `eiem_entry_loading_observation.h`, `eiem_entry_reload_observation.h` | Optional evidence; observers do not establish completion fences |
| Author bridge | `eiem_author_core.cpp` | Source-schema interface for author tools |

Author builders are used only by `eiem_compiled_mod_writer.h` through the Blender compiler. The DLL uses `eiem_compiled_mod_set.h` to combine already compiled records and `eiem_prefab_resource_rules.h` to apply byte recipes. It neither runs author executables nor generates Mesh/Material/Texture records. Shared headers provide codecs; author builders are not called by the runtime. Complete original Prefabs and duplicate author payloads are not persisted in Mod outputs.
The compiled path also accepts opaque `Resource` payloads without author Mesh
parts. `eiem_compiled_object_merge.h` merges proven native type/script
registrations and object additions/deletions for shared members; existing
Renderer controls remain optional validation. See the current scope and tests
in [compiled resource graphs](compiled-resource-graph.md).
Public Renderer visibility and blend-shape controls, and the
capture consumer's shape snapshots, have their own bounded responsibilities;
they do not restore the retired base-resource replacement path. `eiem_lua_ui.h`
is an unwired adapter. Skeleton/Physics codecs remain author-side support.
The built-in panel uses Win32/Common Controls on its own thread, independently
of the game's Vulkan/DirectX swap chain. It does not depend on the Lua adapter.
Panel operations pass graph/selection revision checks on the Unity frame;
physical Mod keys pause while open and otherwise affect only the selected Mod.

## Startup and reload

Blender compiles each Mod independently before publication. Startup and F10 read the full installed 0/1/N compiled set. Shared input edits compose against pristine records and exact original list positions; conflicting Mesh targets or field edits are rejected. Player key state remains per Mod. Old current.tsv publications remain read-only compatibility inputs; new exports do not use the game-side compiler/cache.

Startup loads the latest complete manifest and its loading table, validates the
field structures and source/output hashes, then submits substituted inputs along
the game's own loading path. Startup files keep original Bundle/CAB names.
Hot reload loads a separately named generation and verifies its complete
closure before mutation. Disk publication and runtime commit are distinct.
F10 (the current source default) commits additions, removals and edits together.
Watchers only prepare disk generations. All baseline roots enter the candidate,
so removal restores pristine resources, including an empty set. Game model load
entries record vanilla and authored owners alike; exact component/entity/main-part
and resource paths decide coverage without scene Renderer discovery. A matching
unfinished model load rejects the request before mutation so its old root cannot
appear after publication. Ins selection only scopes keys,
retains existing owners, and falls back to the first controllable Mod after removal.

The current resource transaction prepares on the worker and loads/verifies on
the Unity thread. It rebuilds known world/display model owners through the
game's lifecycle, including animation, weapons, audio and helper state, and
routes future native prefab creation to the accepted roots. Stale requests
cannot publish. Failed preparation retains the active generation; mutation
failure uses the existing owner rollback and reports restoration gaps.

Controls register inactive SkinnedMeshRenderer children of the exact completed
game model, alongside the native material-controller Init callback. F10 restores
bindings after model/control publication or rollback; native Init does not enter
another frame transaction. Material Init registers provisionally; the helper's
`InitAll` completion activates its cached Renderer list, covering NPC instances
without a view-part completion. During Base/Complex post-processing, a nested
thread-local enabled getter scope preserves the game's native initial snapshot
separately from authored hiding. Normal reads return actual enabled flags.
There is no periodic global Renderer scan or arbitrary
Renderer-list capacity limit.

Compiled member extents and proven namespace sites also locate fields within the
uncompressed native input envelope. `eiem_native_input_offsets.h` applies those
sites without another decode/replay/encode pass; shared immutable pristine roots
and layouts avoid repeated deep copies. Startup and isolated cache entries share
one immutable backing. Optional public `UnmanagedMemoryStream` bindings let Unity
read that backing without a second large managed byte array. Stream ownership
lasts through async completion and native Bundle retirement; the MemoryStream
fallback remains available. No Mod layout or resource size ceiling is added.

The startup rules already publish their exact resource identity through game
loads. The same initial snapshot does not prewarm a second namespace. Once
published by startup or a resource commit, an identical F10 snapshot refreshes
controls/settings without native loads or model reconstruction; author-file and
directory membership preflight remains mandatory. Visibility-only
bindings use control-value revisions and native setter observations; idle weak
reference checks are spread over 64 frames. Shape weights still follow each frame
because native animation may change them. Material texture identities are shared
only within a verification call, never cached across resource generations.

UIModelLoader is a separate game owner. Native completion registers the exact
loaded root, and the loader dictionary confirms handle ownership. UI key control
registration does not depend on the Lua page-transfer API. The adapter obtains
the game's LuaManager instance and calls its instance DoString method. Its
embedded chunk uses the game's current LuaCypher key and XXTEA encoder because
the native loadbuffer decrypts scripts. A full game-codec round trip precedes
the first load; the returned LuaFunction factory is cached. No key or script
file is bundled. Missing page APIs reject matching F10 owners before mutation.
Loader ownership is not
proof that a page owns that root. PhaseBase's loader callback resumes a one-shot
coroutine; BindGameObject, layer setup and the page initializer run after the
wait. Replaying the callback after its coroutine finished cannot reload a page.

F10 records the actual PhaseCharItem which holds the root in the loader's game
phase. Phase item collections also contain panels and scenery, so the adapter
checks the game's HL.Is type before reading character fields, uses actual stack
bounds and HL.TryGet for optional page fields. Lua call failures preserve their
managed exception message. A fresh LoadModelAsync request has no old coroutine callback. The native
completion and exact request proxy are retained across frames while the old
world/UI instances remain visible. After all candidates pass resource checks,
normal native completion accepts the handle and parent. The existing page item
calls its game's BindGameObject API for the new root; public model APIs restore
layer, position, visibility, animation parameters/states, weapon, decoration and
configured light follower. Page and formation-cache item identity is preserved.
Only successful page binding plus control publication permits old-root unload.
A rejected page binding retains the original display; partial UI/world transfers
roll back. Cancelled native jobs drain before candidate retirement. Public routes
stay unchanged during the wait, and only each actual request proxy gets its
prepared prefab. Ordinary loads and unrelated proxies use normal routing.

There are no private native/Animator pointer writes, no replay of finished
coroutines and no global Renderer scan. Instances without an actual page-item
adapter reject before mutation. Game visual/animation validation is still
pending; native and Lua seam regressions are not visual acceptance.

When no full-resource package is configured, `eiem_entry_reload.h` only accepts
configuration/control changes with an unchanged native input contract. This
path rejects resource changes; it is not a second resource replacement route.

Manifest fields select address verification or marker coverage, independently of
compiler/build labels. Product versions do not gate structurally compatible inputs.
The author compiler takes one Mod directory and the game installation path, with no GameAssembly
file dependency. Protocol queries must succeed; executable strings are never used
to guess layouts. Legacy manifests remain readable and are rewritten with the current
self-describing header. Mod/input/root
counts and file sizes have no policy ceilings; structural extents and field widths
are checked. Neither local
verification nor the successful v35c world texture test proves complete 0/1/N
topology migration, all helpers/VFX, hidden consumers, persistent pools or native
job retirement. The remaining transaction target is defined in
[full render assembly reload](full-render-assembly-reload.md).

Old generations retire Bundle containers with `Unload(false)` and preserve
resources still referenced by native consumers. Captured afterimages retain
their snapshots to native Release. Unknown/exposed ownership is quarantined;
request completion or a returned release call is not a complete job fence.

## Build and verification

```powershell
cmd /c build.bat v36
cmd /c build.bat v35
```

Each production output contains the DLL, protocol JSON, proxies
and settings. Build commands alone do not change game files. The separately
authorized deployment installed the v35 pair; v36 remains a local build.

Focused checks (build scripts for entry/full-reload fixtures compile only, so
execute the resulting programs as shown):

```powershell
cmd /c tools\diagnostics\build_entry_contract_smoke.cmd v36
& .\bin\entry-contract-v36\entry_contract_smoke.exe
cmd /c tools\diagnostics\build_entry_contract_smoke.cmd v35
& .\bin\entry-contract-v35\entry_contract_smoke.exe
cmd /c tools\diagnostics\build_offline_cache_smoke.cmd
cmd /c tools\diagnostics\build_control_panel_smoke.cmd
& .\bin\resource-directory-entry\control_panel_smoke.exe --ui-smoke
cmd /c tools\diagnostics\build_full_resource_reload_smoke.cmd v36
& .\bin\resource-directory-entry\full_resource_reload_smoke.exe
cmd /c tools\diagnostics\build_full_resource_reload_smoke.cmd v35
& .\bin\resource-directory-entry-v35\full_resource_reload_smoke.exe
dotnet run --project tools/EndfieldVfsProbe/EndfieldVfsProbe.csproj --configuration Release -- --test-package-contracts
```

Entry checks cover zero/multiple Mod discovery, add/remove directory identity, default F10
shortcut and strict relative Lua paths. Cache checks cover aliases and rejection
without publication changes. Full reload fixtures exercise production code with
mock native APIs, including owner lifecycle, address/bone verification, stale
tickets, rollback and retirement. C# checks cover CAB identity, snapshots,
dependency conflicts, full/compact indices, donor palettes and per-CAB Unity
version evidence. These checks do not perform game visual acceptance. Current
game evidence and the next case are in [hot reload testing](hot-reload-testing.md).
Panel checks cover identical keys in two control owners, selection/reload/focus
edges, command revision rejection and real Windows widget messages with injected
foreground observations. Native mocks cover adding to a vanilla world instance,
restoring world/display roots, stale membership rejection and rollback. The real
input fixture splits LZY targets to validate one composed shared CAB/PFB graph,
pristine startup bytes and zero substitutions; it is not a second character's
game acceptance. The installed DLL and production baseline have not been changed.


## Compact runtime index (2026-10-12)

The production path reads only the current per-Mod compiled runtime index. Blender owns the compiler and generates native span views, startup digests, expanded frames, source file stamps, input dependency edges, and shared Prefab/bone-palette dictionaries. Legacy compiled packets and current.tsv/loading-inputs.tsv are not startup or F10 fallbacks.

`eiem_compiled_runtime_index.h` decodes the small runtime section and defers independent compressed object proofs. `eiem_compiled_assembly_index.h` shares immutable descriptions/palettes. `eiem_readonly_file_range.h` holds source ranges read-only and pages original encoded blocks in on demand. `eiem_compiled_mod_set.h` skips shared read-only/single-writer merges and retains general record composition for simultaneous writers. `eiem_entry_full_resource_reload.h` submits only dependencies of observed roots, and routes later native loads through the same generation.

Resource provenance/checksums, bounds, reference relocation, independent key settings, owner animation/physics lifecycle, and rollback remain required. Measurements of these disk-only routines exclude game readiness and all Unity loading; they do not establish an FPS improvement or millisecond latency after a changed Mesh.

## Author edits and input reuse (2026-10-12)

The Blender author compiler treats an export as an immutable author snapshot. Its reader verifies each file once per export and reuses its staging hardlink aliases while Windows read handles deny in-place writes. Manifest decoding can borrow resources from that verified snapshot only for an exact author ID, resource path, kind, native type and content digest; changed resources still read their previous published bytes. This opt-in path is used by the author compiler, not the game reader.

`eiem_incremental_material_compile.h` accepts parameter-only edits after comparing all resource identities, texture bindings, Mesh/physics declarations and the normalized input contract. It replays and rebuilds only inputs owning changed materials, uses their recorded author dependencies as its working set, and keeps unaffected geometry, Prefab descriptions and native views. Previously borrowed bytes from changed material resources become literal spans before the author snapshot changes. The runtime candidate key still follows the current author contract; the reused offline namespace does not conceal edits.

Changes to geometry, texture data/references, source identities or structure take the existing complete compiler path. Complete compilation shares one author span dictionary between member recipes and native views. No compiled protocol change or additional compiler deployment to the game is required. The real material-edit regression compares every native view with a full compile, checks portable roundtrips and excludes Mesh/binding edits from the selective path.
