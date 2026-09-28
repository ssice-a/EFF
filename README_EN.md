# EFF: Endfield Framework

[中文](README.md)

EFF (Endfield Framework) is an in-game resource replacement framework for *Arknights: Endfield*. It replaces meshes, materials, textures, and declared material parameters by source resource identity, with the same Render rules for world characters, character UI, NPCs, and authored LODs.

## Features

- Multiple material slots and submeshes, plus mesh, material, texture, and shape-key resources.
- Independent key-switch and shape state for every Mod.
- Insert opens the manager; F10 hot-reloads Mods. A bad Mod leaves the last working generation in place.
- Native sprint material lifecycle handling with complete submesh restoration.
- Global settings for manager and reload shortcuts and camera fade behavior.

## Install

Download the package from [EFF Releases](https://github.com/ssice-a/EIEM/releases). Exit the game and extract the ZIP next to `Endfield.exe`:

```text
game/
├─ d3dcompiler_47.dll      # DirectX proxy loader
├─ vulkan-1.dll            # Vulkan proxy loader
└─ plugin/
   ├─ eff.dll
   ├─ eff.ini              # Global settings template
   └─ mods/
      └─ ExampleMod/
         ├─ mod.ini
         └─ ...
```

Put each folder **containing `mod.ini`** directly in `plugin/mods/`. Do not add another nesting level. Keep a proxy loader only when it is the loader used by your graphics setup. Applepie Manager is optional.

## In-game use

1. Enter a scene containing the target character; matching Mods apply automatically.
2. Press **Insert** to open the EFF manager and use the selected Mod's style buttons and shape sliders.
3. Press **F10** after adding or editing Mod files.

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
2. Install the [EFF Blender add-on](https://github.com/ssice-a/EIEM-blender), then import the package's `mod.ini`.
3. Edit meshes, materials, textures, styles, and shape keys, then export the Mod folder.
4. Put the folder in `plugin/mods/` and press F10 in game.

Use current versions of AnimeStudio, the Blender add-on, and EFF together. Mesh, skeleton, material, and physics files use the EFF exchange formats; re-export source and Mod packages when an export does not validate.

## Credits and license

AnimeStudio and its contributors provide the asset browsing, extraction, and export foundation. Third-party licenses are listed in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES). EFF is released under AGPL-3.0. The project contains no game assets; game assets remain the property of Hypergryph. Follow the game's terms of service and use the software at your own risk.
