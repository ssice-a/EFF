# EFF: Endfield Framework

[中文](README.md)

EFF (Endfield Framework) is an in-game resource replacement framework for *Arknights: Endfield*. It compiles format-2 author declarations into native loading inputs and replaces meshes, materials, textures and material parameters by source identity.

Paired releases: **EFF v1.3.0, EFF Blender v0.40.0, and AnimeStudio v1.3.0**. The default DLL archive uses the v35 layout tested in game; an additional v36 build is available for validation. Compatibility follows actual data structures and tool protocol declarations, rather than application version numbers.

## Features

- Multiple material slots and submeshes, plus mesh, material, texture, and shape-key resources.
- Key switches, held shape controls and numeric preferences, with native Renderer visibility arbitration.
- Validated offline candidates load through native resources and supported game owner lifecycles; preparation failure retains the active generation.
- Compose Mods for multiple characters; characters without a Mod retain original resources. F10 reads the complete installed set and commits additions, removals and edits.
- Configurable reload shortcut and camera fade behavior. The Lua UI adapter is not wired to the current host.
- Insert opens a control panel with a Mod selector, source targets, key buttons and shape sliders. Physical bindings affect only the selected Mod; the first controllable Mod is selected by default.

`mod.ini` is the author rule entry. Offline tools locate every Prefab/Renderer using the declared source Mesh and generate `source-inputs.bin`. The compiler publishes replacement rules referencing author payloads; the DLL applies them before Unity creates resources and instances. Undeclared resources keep their original contents. Players do not need a full game extraction or the author's global index. There is no runtime whole-scene Renderer scan. See the [resource flow](docs/runtime-resource-discovery.md).

## Install

Download `EFF_v1.3.0_dll.zip` from [EFF Releases](https://github.com/ssice-a/EFF/releases). Exit the game and extract the ZIP next to `Endfield.exe`. When upgrading, preserve your `plugin/eff.ini`, `plugin/mods/` and Mod `state.ini` files:

```text
d3dcompiler_47.dll         # DirectX proxy loader
vulkan-1.dll               # Vulkan proxy loader
└─ plugin/
   ├─ eff.dll
   ├─ eff.ini              # Global settings template
   ├─ package-protocol.json # DLL/compiler protocol
   ├─ tools/
   │  ├─ eff_resource_pack.exe # Paired offline compiler
   │  └─ static-sources/    # Source tool and bundled runtime
   └─ mods/
      └─ ExampleMod/
         ├─ mod.ini
         └─ ...
```

Put each character's folder **containing `mod.ini`** directly in `plugin/mods/`. Do not add another nesting level. A complete Mod includes authored Mesh/Material/Texture payloads and `source-inputs.bin`. The paired compiler combines the full installed set in one candidate; see the [package contract](docs/offline-resource-package-format.md). Check your graphics backend and plugin loading method if a proxy loader with the same filename is already installed.

## In-game use

1. Install a complete Mod and start the game. Replacement follows the game's resource loading; wait for initial preparation to finish.
2. Press **Insert** to select the controlled Mod and use panel buttons/sliders; close the panel to use that Mod's keys. Selection does not disable other characters' resources.
3. Add or remove Mod folders, or export edits from Blender, then press **F10** to reload the complete set. Failure retains the current generation; an empty set restores original resources. The actual shortcut comes from game `plugin/eff.ini`.

F10 updates Mod resources and rules. Updating DLL code requires exiting and restarting the game. A reload affecting an unfinished asynchronous model load is refused; retry once loading completes.

Global settings are stored in `plugin/eff.ini`:

```ini
[Hotkeys]
reload=F10
gui=INSERT

[Graphics]
disable_camera_fade=true
```

## Create a Mod

1. Open the game VFS in [AnimeStudio](https://github.com/ssice-a/AnimeStudio), select a Prefab, and export an EFF source package.
2. Install the [EFF Blender add-on](https://github.com/ssice-a/EFF-blender), then import the package's `mod.ini`.
3. Edit meshes, materials, textures, styles, and shape keys, then export the Mod folder.
4. Export into `plugin/mods/`, wait for the paired offline compiler, then use the configured reload key in game.

Keep the extracted source package and source index for initial author preparation. Blender generates the per-Mod source description automatically. Newly extracted textures use short filenames; existing hashed filenames remain valid through their own INI/manifest references. Current production author packages contain independent Mesh, Material and Texture resources. Skeleton/Physics codecs remain author support and are not wired to the complete game simulation chain. See [scoped Blender export](docs/blender-scoped-resource-export.md).

## Validation and performance

On 2026-10-07 the user confirmed the current v35 build's sleeve and secondary-part switches work correctly. Native regression tests cover repeated world/display reloads, animation rebinding, multi-Mod composition and rollback. Complete in-game coverage of other characters/NPCs and performance with 20–30 Mods remain unmeasured. All-LOD export can map one authored geometry to several targets; it does not automatically generate lower-poly meshes. See the [game test record](docs/hot-reload-testing.md).

## Credits and license

AnimeStudio and its contributors provide the asset browsing, extraction, and export foundation. Third-party licenses are listed in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES). EFF is released under AGPL-3.0. The project contains no game assets; game assets remain the property of Hypergryph.

## User Agreement & Disclaimer

<details>
<summary>Please read this agreement before downloading, installing, or using EFF. By using EFF, you acknowledge that you have read, understood, and agreed to the following terms.</summary>

### 1. Open Source License & End User Rights

- EFF is open-sourced under the **AGPL-3.0** license. You may use, modify, and distribute the source code in compliance with that license.
- End users may use and distribute the unmodified software itself.

### 2. Anti-Fraud Statement

- Do not sell the EFF software itself on an online marketplace without providing the repository address and necessary support.
- EFF is freely available on GitHub. If you paid for the software itself, it can still be obtained from GitHub for free.

### 3. Content Compliance & Conduct

- EFF contains no game art assets. Official animations, scenes, models, and other assets in *Arknights: Endfield* belong to Hypergryph and are not covered by AGPL-3.0.
- Do not use EFF or official game assets to create, play, or distribute content that violates laws, the game's rules, or community standards.

### 4. Risk & Disclaimer

- EFF is provided for learning, technical research, and discussion. Use of this tool may violate the game's terms of service and may carry risks including account suspension or data loss.
- Users bear all direct and indirect risks and losses arising from use of EFF. The project assumes no legal or financial liability. Test accounts are recommended.

</details>
