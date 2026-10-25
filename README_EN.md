# EFF: Endfield resource replacement framework

[中文](README.md)

AnimeStudio exports source assets and templates. Blender creates and compiles each independent Mod. EFF applies the compiled rules before the game creates resources and uses the game's model lifecycle for reloads.

Paired releases: **EFF v1.10.1, EFF Blender v0.49.1, AnimeStudio v1.8.1**. There is one production DLL package. Compatibility follows byte structures, fields, source hashes and references.

See the illustrated quick tutorial for installation, extraction, Blender editing and export: [English](Mod%20Tutorial_EN.md) · [中文](Mod%20Tutorial.md).

## Install

Download `EFF_v1.10.1_dll.zip` from [Releases](https://github.com/ssice-a/EFF/releases), quit the game and extract beside `Endfield.exe`. The current proxy forwarders support Windows installations at `C:\Windows`; other system directories are not supported yet. Preserve `plugin/eff.ini`, your Mods and their `state.ini` when upgrading.

```text
d3dcompiler_47.dll
vulkan-1.dll
plugin/
  eff.dll
  eff.ini
  package-protocol.json
  mods/
    YourMod/
      mod.ini
      compiled.bin
      source-inputs.bin
      meshes/
      materials/
      textures/
```

The production DLL and proxies statically link their own C/C++ runtime implementation and do not depend on the game's older MSVCP/VCRUNTIME files. Do not replace the game's runtime files. The DLL distribution has no compiler, source tools or AnimeStudio libraries. Previous `plugin/tools/eff_resource_pack.exe` and `plugin/tools/static-sources/` are unused and can be removed. Move complete Mod folders directly into `plugin/mods/`. Re-export older Mods with the paired Blender add-on to produce `compiled.bin`. Relative game source paths make the output portable. After game updates, players can use the Mod repair feature in EFF Mod Manager; it matches current game resources and backs up the original package without Blender or manual extraction. Authors can instead refresh extracted sources in AnimeStudio and export their original projects again.

## Features and controls

- Independent character Mods; characters without a Mod retain original assets.
- Skinned Mesh and static MeshFilter/MeshRenderer replacement, materials, textures and visibility. Static Renderers do not support runtime shape controls.
- One source Renderer can expand into multiple author parts. Unspecified targets retain original content. Materials and textures may use game templates or author-created data.
- **Ins** opens independent keyboard checkboxes for each Mod, all enabled by default. Multiple Mods can respond to the same key. The dropdown selects the Mod whose buttons and sliders are shown. Key switches, shape controls and saved player values remain supported.
- **F10** reads the full current Mod set, handles additions/removals/changes and the empty set, and reloads supported world/display owners with animation, weapon and audio rebinding. Failed preparation or submission keeps the current generation.
- Shape Keys clips have independent playback groups and play/stop shortcuts. Ins provides pause and persistent live speed controls without resetting playback phase.
- Effects can ship as standalone Mods or attach to a character Mesh bone. They follow the model lifecycle, can be toggled by key, reload through F9, and cover all LODs.
- Animated material parameters and effect resources use the same Mod export and reload pipeline; Mods are discovered recursively below `plugin/mods`.

UI and world instances of a Mod share control values. UI reload uses the game's
loader and page item binding, retaining the displayed model until candidate
verification, page binding and control publication complete. Instances without
a verified page owner reject reload before mutation.

Set `key_switch_enabled=0` or `1` under `[Mod]` in each `mod.ini`, then press F10 to apply manual edits. An omitted flag defaults to enabled. The Ins checkboxes apply immediately and save this same field; disabling keys preserves the Mod's resources and current values.

DLL updates require a game restart. Wait for active model loads to finish before requesting reload. Global shortcuts and camera fade behavior are configured in `plugin/eff.ini`.

## Author workflow

1. Export an EFF source package using [AnimeStudio](https://github.com/ssice-a/AnimeStudio). Keep source templates, manifest and author index.
2. Import its `mod.ini` with [EFF Blender](https://github.com/ssice-a/EFF-blender), edit and export selected Meshes. Start with all resources; partial exports update an existing complete Mod.
3. Blender uses only its bundled author core, compiler and small source preparer. It resolves all consumers of the declared source Mesh offline and compiles this one staged Mod before publishing it.
4. EFF reads the independent results, combines compiled edits on shared inputs and applies proven byte recipes. It does not compile geometry/materials/textures, query the global asset index or scan scene Renderers.

Exports under the game's `plugin/mods/` locate the installation automatically; other output directories require `EFF_RELOAD_GAME`. Players need neither author tools nor source snapshots. `mod.ini` remains the author rule entry; `compiled.bin` is derived output. Both short texture names and older hash names use their package's own references.

Complete original bundles are not saved in the Mod. Compiled outputs include native input views, dependency and instance-binding indices; size follows actual author resources and shared edits and is not an estimate of Unity memory usage. See the [loading flow](docs/runtime-resource-discovery.md) and [output contract](docs/offline-resource-package-format.md).

## Validation limits

The current update has regression coverage for BLC v5, portable full indexes, transactional extracted-source refresh, an existing Blender project and old-Mod repair. Update adaptation runs outside the game. The DLL performs no background discovery, compilation or Mod writes. Broad in-game visual coverage, active-animation performance and substantial skeleton changes still require separate validation.

Regression checks cover shared-PFB composition, removals, stream relocation, static Meshes, sequential world/display reloads, animation consumers and rollback. The latest in-game two-character F10 took about 107 ms from request to commit, with idle attachments visually confirmed. This measured an already prepared candidate, not every scene's latency. Independent crowd NPC/scene-object refresh, complete generation retirement and 20–30 Mod pressure remain unverified. Exporting all LOD targets does not generate reduced geometry. New BoneCloth chains and sphere/capsule colliders are connected to offline compilation. Animation blending, complete physics teardown and F10 still require in-game acceptance. This release build passed automated regressions; new in-game acceptance has not been performed for these binaries.

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
