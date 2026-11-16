# EFF source map

The production DLL entry is `eiem_resource_entry.cpp`. The current runtime accepts Format 3 declarations and prepares native inputs only for matched game requests. Run `build.bat` to build; use the fixed-target local deployment script to install and verify it. A successful build is not deployment or in-game acceptance.

| Responsibility | Entry points |
| --- | --- |
| Mod declarations and reusable assembly | `eiem_resource_directory.h`, `eiem_author_controls_ini.h` |
| Lazy authored resources and logical lookup | `eiem_demand_author_resources.h`, `eiem_demand_logical_asset_lookup.h` |
| Native Mesh/material preparation | `eiem_demand_mesh_preparation.h`, `eiem_demand_material_assembly.h`, `eiem_demand_material_build.h` |
| Game loading and F10 generations | `eiem_entry_bundle_dependencies.h`, `eiem_entry_demand_bundle_handoff.h`, `eiem_entry_demand_generation_reload.h`, `eiem_entry_full_resource_reload.h` |
| Native generation identity and references | `eiem_native_generation_identity.h`, `eiem_entry_demand_shared_inputs.h`, `eiem_demand_shared_graph.h` |
| Shared controls and Ins panel | `eiem_control_program.h`, `eiem_entry_controls.h`, `eiem_control_panel_win32.h` |
| Animation payloads and playback | `eiem_animation_samples.h`, `eiem_shape_animation_player.h` |
| Effects and material animation | `eiem_entry_effect_attachments.h`, `eiem_entry_material_animation.h` |
| Native author tools | `../tools/eff_author_assets.cpp`, author metadata/codecs |

The DLL uses the game's deserializer and resource/owner lifecycle. INI owns relationships; DDS owns pixels; binary resources hold geometry, physics, animation samples and native effects. Compatibility code used by migration, author tooling or release tests remains intentional. Standalone retired investigations live under `tools/archive/`.

See the [current architecture](../docs/code-architecture.md), [Format 3 contract](../docs/adr/0002-runtime-resource-files.md), [deployment rules](../docs/release-management.md) and [historical source map](../docs/archive/snapshots/source-map-before-20261112.md). Optional diagnostics/capture builds are not release DLLs.
