# EFF v1.2.4

- Keeps F10 reloads isolated across world, NPC, and UI model owners.
- Preserves per-instance LOD and skinning palette resolution during reload.
- Skips invalid Mod files without preventing other Mods from loading.
- Keeps native sprint material and complete submesh restoration paths active.

## Package contents

The release archive is laid out for direct extraction next to `Endfield.exe`:

```text
d3dcompiler_47.dll
vulkan-1.dll
└─ plugin/
   ├─ eff.dll
   ├─ eff.ini
   └─ mods/
```

`plugin/mods/` is intentionally empty so a fresh install has the Mod directory ready.

## Install

Exit the game, extract `EFF_v1.2.4_dll.zip` next to `Endfield.exe`, put each Mod folder containing `mod.ini` under `plugin/mods/`, and press F10 after adding or editing Mods.
