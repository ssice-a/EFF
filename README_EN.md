# EIEM: Endfield Resource Replacement

English | [中文](README.md)

EIEM is an in-game resource replacement plugin for *Arknights: Endfield*. It replaces meshes, materials, and textures by source resource identity. Mod authors can extract source assets with [AnimeStudio](https://github.com/ssice-a/AnimeStudio), then edit and export a Mod with the [EIEM Blender add-on](https://github.com/ssice-a/EIEM-blender). Players installing an existing Mod do not need either authoring tool.

## Features

- Replace meshes, materials, textures, and declared material parameters. One mesh can contain multiple submeshes and material slots.
- Apply the same resource rules to world characters, character UI, and NPCs, including authored LOD rules.
- Keep each Mod's switch and shape state separate. The in-game manager provides key buttons and shape sliders.
- Press F10 to reload Mod configuration and resources. An invalid configuration leaves the last working result in place.
- Configure the manager and reload shortcuts, as well as camera fade behavior, in the global INI.
- Check this repository's Releases when the Mod manager opens; defer or ignore a specific version.

## Download and install

Download the DLL package from **[EIEM Releases](https://github.com/ssice-a/EIEM/releases)**. Exit the game, then extract the ZIP into the directory containing `Endfield.exe`:

```text
game directory/
├─ d3dcompiler_47.dll      # DirectX proxy loader
├─ vulkan-1.dll            # Vulkan proxy loader
└─ plugin/
   ├─ eiem.dll
   ├─ eiem.ini             # Global settings; created with defaults if absent
   └─ mods/
      └─ SomeMod/
         ├─ mod.ini
         └─ ...            # Preserve the Mod's resource folder structure
```

Install either or both proxy loaders as appropriate for your graphics setup. If another plugin already supplies a compatible loader with the same filename, check how it loads plugins before replacing it. Create `plugin` and `mods` if needed. [Applepie Manager](https://github.com/Sasye/ApplepieManager) is optional.

To install a Mod, put the **folder containing `mod.ini`** directly inside `plugin/mods/`. Avoid an extra nested folder after extracting the archive. To update EIEM, exit the game first, replace the DLL and any necessary loader, and keep your own `plugin/eiem.ini` and `plugin/mods/`.

## Use in game

1. Start the game and enter a scene containing the target character. Matching Mods apply automatically.
2. Press **Insert** to open the Mod manager. Select a Mod to use its style buttons and shape sliders. Static Mods apply automatically.
3. Press **F10** after editing or adding Mod files to reload them.

Shortcuts can be changed in `plugin/eiem.ini`:

```ini
[Hotkeys]
reload=F10
gui=INSERT

[Graphics]
disable_camera_fade=true
```

After editing a shortcut, press the **old reload shortcut once** to load the new setting. See the [configuration guide](docs/conditional-keys.md) for Mod rules and additional keys.

## Make a Mod

1. Open the game's VFS in AnimeStudio, select a Prefab, and export an EIEM source package.
2. Install and enable the EIEM Blender add-on, then import the source package's `mod.ini`.
3. Edit meshes, materials, textures, styles, or shapes. Select the target meshes and export an EIEM Mod package. You can turn off key controls during export while keeping the default style and shape sliders.
4. Place the exported folder in `plugin/mods/`, then press F10 in game to check the result.

See the [AnimeStudio guide](tools/README.md) and [Blender add-on guide](https://github.com/ssice-a/EIEM-blender#readme) for detailed steps.

Use current versions of all three tools when making a Mod. Older exported EIEM mesh, skeleton, and physics files need to be exported again.

## TODO

- Improve support for added bones, physics bones, and colliders.
- Expand character and game-version compatibility.

## Acknowledgements

- [AnimeStudio](https://github.com/Escartem/AnimeStudio) and its contributors provide the asset browsing, extraction, and export foundation; EIEM uses an independently maintained fork.
- Copyright and licenses for [MinHook](https://github.com/TsudaKageyu/minhook), [Dear ImGui](https://github.com/ocornut/imgui), and other dependencies are in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES).

## User Agreement & Disclaimer

<details>
<summary>Please read this agreement carefully before downloading, installing, or using this plugin (EIEM). <b>By using this plugin, you acknowledge that you have fully read, understood, and agreed to all of the following terms.</b></summary>

### 1. Open Source License & End User Rights
- This plugin is fully open-sourced on GitHub under the **AGPL-3.0** license. Users may freely use, modify, and distribute the source code of this plugin in compliance with the license.
- End Users may use and distribute this plugin **without any restrictions**, provided they do not modify it. This right is not affected by whether the user violates this agreement.

### 2. Anti-Fraud Statement
- You **must not** openly sell this plugin **itself** on online retail platforms without providing the GitHub repository address and after-sales service.
- This plugin is entirely free and open-source on GitHub. If you obtained it through a paid purchase, please be aware that it is freely available on GitHub.

### 3. Content Compliance & Conduct
- This plugin does not contain any game art assets. Users acknowledge and agree that the official animations, scenes, models, and other assets built into *Arknights: Endfield* are copyrighted by Hypergryph and are not covered by the AGPL-3.0 license. You **should not and must not** use this plugin, or any in-game official assets, to create, play, or distribute any inappropriate motions/animations (including but not limited to pornographic, violent, politically sensitive, or other content that violates laws and regulations or causes community discomfort).

### 4. Risk & Disclaimer
- This project is for educational, technical research, and communication purposes only. All Arknights game data assets used in this plugin are copyrighted by Hypergryph. Using this tool may violate the game's terms of service and carries a risk of account suspension. For any loss directly or indirectly caused by using this plugin (including but not limited to account bans, game data corruption, etc.), **this project assumes no legal or financial liability**. Users bear all risks and are strongly advised to use it on a test account.

</details>
