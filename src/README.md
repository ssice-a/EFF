# EFF source map

The production DLL is one translation unit, `eiem_resource_entry.cpp`. Run `build.bat` for the single current package contract. The old `v35` and `v36` command arguments are compatibility aliases to the same build; builds output the runtime DLL, two proxies and settings, and never deploy into the game. The Blender author compiler has its own build and distribution.

The default build excludes temporary performance/skin/material probes, VFS input capture, periodic observation counters and per-session diagnostic folders. Essential outcomes and failures use one `plugin/eff_log.txt`. `build.bat capture` explicitly enables developer diagnostics and optional GPU capture; this is not a release artifact. Native attachment cleanup and configured idle/dash preloads remain production behavior.

| Module | Interface and implementation |
|---|---|
| Host and IL2CPP loading input hooks | `eiem_resource_entry.cpp`, `il2cpp_api.h`, `applepie_mgr.h` |
| Author format 2 and resource payloads | `eiem_resource_directory.h`, `eiem_expression.h`, `eiem_keys.h` |
| Source input decoding and writing | `eiem_native_input.h`, `eiem_serialized_writer.h`, `eiem_endfield_input.h` |
| Offline Mesh, Renderer, Material and Texture assembly | `eiem_mesh_input_builder.h`, `eiem_renderer_input_builder.h`, `eiem_texture_input_builder.h` |
| Author-set composition, generation, manifest, cache ownership and ABI | `eiem_composed_package.h`, `eiem_loading_generation.h`, `eiem_resource_reload_plan.h`, `eiem_offline_resource_package.h`, `eiem_offline_cache.h`, `eiem_package_protocol.h` |
| Static per-Mod sources and exact Mesh consumers | `eiem_static_resource_sources.h` (compiler only); author tool `StaticModSourceCommand.cs` |
| In-memory application of persisted Prefab rules | `eiem_prefab_resource_rules.h` (shared serialized builders; author payloads by reference) |
| Native generation load and game-owned owner rebuild | `eiem_native_bundle_loader.h`, `eiem_entry_full_resource_reload.h` |
| Controls, preferences and configuration | `eiem_entry_controls.h`, `eiem_entry_preferences.h`, `eiem_entry_reload.h`, `eiem_global_settings.h`, `eiem_entry_settings.h` |
| Built-in Ins panel and versioned UI bridge | `eiem_control_panel.h`, `eiem_control_panel_win32.h` |
| Optional shape capture and loading observation | `eiem_entry_runtime_shapes.h`, `eiem_entry_capture_observation.h`, `eiem_entry_loading_observation.h`, `eiem_entry_reload_observation.h` |
| Blender source schema bridge | `eiem_author_core.cpp` |
| Game-root proxy loaders and build version | `proxy_d3dcompiler.cpp`, `proxy_vulkan_full.cpp`, `eiem_version.h`, `version.rc` |

The compiler validates serialized changes in memory and publishes small compressed rules, without complete Prefab/resource files or duplicated author payloads. The DLL reads exact pristine game ranges, applies the same builders, verifies hashes and submits memory bundles before resource/instance creation. F10 prepares isolated bytes on a worker, checks consumers and asks the game to rebuild owners. Original materials are preserved as templates. This uses TypeTree codecs, not Unity Mesh/Material setters or private native memory patches.

The historical v35 manifest stored marker coverage; v36 added source/target Renderer addresses and bone-order evidence. Both are legacy reader dialects. The current writer always emits the self-described assembly manifest with Renderer addresses and per-Mesh bone order, regardless of the old command alias. Current source composes 0/1/N original author packages, builds shared CAB/PFB inputs once, and binds the cache to the containing mods directory. F10 commits additions/removals/edits; original resources remain for characters without a Mod. Character baseline coverage is required, and the documented in-game evidence has specific coverage limits.

`eiem_lua_ui.h` is a reusable sandboxed Lua/ImGui adapter, with variables from `eiem_expression.h` and strict file paths from `eiem_file_io.h`. It is not wired into the current DLL host. The pure author codecs `eiem_skeleton_document.h`, `eiem_physics_document.h` and `eiem_physics_native_document.h` remain available for author tools; they create no runtime objects. The retired format 1 INI parser and runtime resource replacement entry have been removed.

The built-in Ins panel uses an ImGui-style Windows panel on a separate worker. It reads immutable Mod snapshots and queues commands for Unity-frame application with graph/selection revisions. Each Mod independently enables keyboard handling through `[Mod] key_switch_enabled`; missing flags default to enabled. Ins edits save the same field. Several enabled Mods may respond to one key, while the dropdown selects only the Mod shown by manual controls. Keys pause while the panel is open. F10 preserves settings for surviving Mods and publishes the full current Mod set.

Source builds, automated checks, installed binaries and game acceptance are distinct evidence. See [the current documentation index](../docs/README.md).
