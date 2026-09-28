# EIEM DLL source map

The DLL builds from `eiem.cpp` as one translation unit. Implementation headers depend on the include order in `il2cpp_trace.h`; extraction into separate `.cpp` files requires an explicit interface pass.

| Responsibility | Files |
|---|---|
| Host, IL2CPP and hooks | `eiem.cpp`, `il2cpp_api.h`, `globals.h`, `init.h`, `il2cpp_trace.h` |
| Mod parsing, input and persistence | `eiem_mod_document.h`, `eiem_mods.h`, `eiem_keys.h`, `eiem_persistent_state.h` |
| Update transaction | `eiem_mod_update.h`, `eiem_mod_reconcile.h` |
| Model ownership and world/UI/NPC adapters | `eiem_model_lifecycle.h`, `eiem_model_registry.h`, `eiem_world_ui_owner.h`, `eiem_npc_model_owner.h` |
| Renderer transaction and restore | `eiem_render_executor.h`, `eiem_render_override.h`, `eiem_render_state.h`, `eiem_render_replay.h` |
| Per-instance structural bone binding | `eiem_skin_resolver.h`, `eiem_assembly_binding.h`, `eiem_skin_binding.h` |
| Mesh, Material, Texture resources | `eiem_resource_backend.h` |
| Optional authoring features | `eiem_shape_*`, `eiem_skeleton_*`, `eiem_physics_*`, `eiem_native_physics_*` |
| UI and camera fade | `eiem_ui_host.h`, `eiem_lua_ui.h`, `gui.h`, `eiem_camera_fade.h` |

World, UI and NPC routes share the same renderer executor. Each model instance builds one child-index bone table, and every LOD resolves against that table. F10 rebuilds the resource generation on every press and restores previous renderer state transactionally. A failed binding leaves the original renderer intact.

The current interchange contract is EIEMESH v6, EIESKEL v2 and author EIEPHYS v5. Native source graph EIEPHYS v2 is a separate document kind. Older author resources are rejected and must be re-exported. The generated skinned Mesh uses `InternalSetBoneWeights` and the validated native four-slot metadata correction; source Mesh objects are never patched.

See [the architecture document](../docs/code-architecture.md) for ownership and verification boundaries.
