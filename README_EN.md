# EFF: Endfield Framework

[中文](README.md)

EFF (Endfield Framework) is an in-game resource replacement framework for *Arknights: Endfield*. It compiles format-2 author declarations into native loading inputs and replaces meshes, materials, textures and material parameters by source identity.

Current source uses one resource entry and defaults to v36 builds. On 2026-10-06 the game installation was updated to the current paired v35 DLL/compiler with F10 reload and Insert controls. The historical v35c world-instance texture update passed; the new build, complete assembly changes, reverse reload and pool lifetimes still need game acceptance. See the [documentation index](docs/README.md) and [current game test](docs/hot-reload-testing.md).

## Features

- Multiple material slots and submeshes, plus mesh, material, texture, and shape-key resources.
- Key switches, held shape controls and numeric preferences, with native Renderer visibility arbitration.
- Validated offline candidates load through native resources and supported game owner lifecycles; preparation failure retains the active generation.
- Current source composes Mods for multiple characters; characters without a Mod retain original resources. F10 commits additions, removals and edits. Each Mod carries a static source description; game acceptance is pending.
- Configurable reload shortcut and camera fade behavior. The Lua UI adapter is not wired to the current host.
- Current source includes an Insert control panel with a Mod selector, source targets, key buttons and shape sliders. Physical bindings affect only the selected Mod.

## Install

Download the package from [EFF Releases](https://github.com/ssice-a/EFF/releases). Exit the game and extract the ZIP next to `Endfield.exe`:

```text
d3dcompiler_47.dll         # DirectX proxy loader
vulkan-1.dll               # Vulkan proxy loader
└─ plugin/
   ├─ eff.dll
   ├─ eff.ini              # Global settings template
   ├─ package-protocol.json # DLL/compiler protocol
   ├─ tools/
   │  └─ eff_resource_pack.exe # Paired offline compiler
   └─ mods/
      └─ ExampleMod/
         ├─ mod.ini
         └─ ...
```

Put each character's folder **containing `mod.ini`** directly in `plugin/mods/`. Do not add another nesting level. The paired compiler combines the full installed set in one candidate. Native compilation uses each Mod's `source-inputs.bin`; initial author preparation reuses an existing AnimeStudio index; see the [package contract](docs/offline-resource-package-format.md). Keep a proxy loader only when it is the loader used by your graphics setup. Applepie Manager is optional.

## In-game use

1. Prepare the complete offline package with the paired compiler, then start the game and enter the target character's scene.
2. Press **Insert** to select the controlled Mod and use panel buttons/sliders; close the panel to use that Mod's keys. The first controllable Mod is selected automatically. Selection does not disable other characters' resources. The installed v35 DLL includes this panel; game interaction still needs acceptance.
3. Add or remove Mod folders, or export edits from Blender, then press **F10** to reload the complete set. Failure retains the current generation; removing the last Mod restores original resources. The installed DLL still uses game `plugin/eff.ini` for its actual shortcut.

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

Use compatible data structures across AnimeStudio, Blender and EFF. Current production author packages contain independent Mesh, Material and Texture resources. Skeleton/Physics codecs remain author support and are not wired to the current game simulation chain. See [scoped Blender export](docs/blender-scoped-resource-export.md).

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
