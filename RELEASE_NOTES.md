# EIEM v1.2.3

- Reduces F10 replay work while preserving a fresh resource generation for each reload.
- Keeps model-instance replay independent across world, NPC, and UI owners.
- Retains per-instance LOD and skinning palette resolution during replay.
- Invalid individual Mod files are skipped without preventing other Mods from loading.

**Package contents**

The DLL release contains only `plugin/eiem.dll`, `plugin/eiem.ini`, and an empty `plugin/mods/` directory. It does not include proxy loaders, licenses, symbols, or diagnostic files.

**Install**

Exit the game, extract `EIEM_v1.2.3_dll.zip` into the directory containing `Endfield.exe`, then put each Mod folder containing `mod.ini` under `plugin/mods/`. Keep your existing `plugin/eiem.ini` and Mods when updating.
