# EFF v1.2.4

This release contains the EFF runtime DLL and its proxy loaders.

- F10 reloads keep world, NPC, and character UI instances isolated.
- Invalid Mod files are skipped without removing the last working generation.
- Native sprint material changes and complete submesh restoration remain active.

## Package contents

The release archive is laid out for direct extraction next to `Endfield.exe`:

```text
d3dcompiler_47.dll
vulkan-1.dll
plugin/
  eff.dll
  eff.ini
  mods/
```

`plugin/mods/` is intentionally empty so a fresh install has the Mod directory ready.

## Install

Exit the game, extract `EFF_v1.2.4_dll.zip` next to `Endfield.exe`, put each Mod folder containing `mod.ini` under `plugin/mods/`, and press F10 after adding or editing Mods.
