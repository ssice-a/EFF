# Unified assembly status (2026-09-26)

This page records the current implementation and the remaining acceptance work. Historical experiments and raw observations are in [the archive](archive/cpu-skin-random-pose-current-20260925.md) and `analysis/`.

## Evidence and scope

- Typhoea body, cloth01 and cloth02 use four serialized BoneWeight slots per vertex in the tested EFFMESH v6 package, including zero-weight slots. The Endfield native Mesh field `m_BonesPerVertex` must be four for these plugin-created skinned Meshes. The production build validates the known UnityPlayer layout and native getter before writing that field. It does not change game-owned Meshes or assume that every future Mesh has four slots.
- A previous experimental DLL survived 50 manual F10 reloads in the world scene without the cloth falling over. This is evidence for the field correction, not proof of full stability. UI F10 crashed in a separate run; NPC initially had disappearing cloth and original body texture. The latest user observation says NPC cloth01/02 overlap has been corrected. The rebuilt candidate still needs in-game verification.
- Calling `InternalSetBoneWeights` alone did not populate the Endfield native field. Earlier clone, in-place, GPU and flag probes did not establish a safer production repair. Their diagnostic hooks are disabled in normal builds.

## Current assembly

`mod.ini` declares resources and Render rules. All matching world, UI and NPC model owners use the same Render executor at the completed `EntityRenderHelper` assembly boundary. Replacement Meshes are built from EFFMESH, validated and submitted through the game's Renderer, LOD and skin registration path. Skeleton and Physics have separate resource handling. No character-name condition is required for normal assembly.

F10 parses a complete candidate Mod generation, invalidates resource caches, then reapplies it to existing model instances. It must do this on every press, even when source files are unchanged. A failed parse leaves the current generation in place. The manager routes ordinary Mod keys only to the selected Mod. Shape controls can exist without a key binding; their values can be changed with manager sliders. The global manager shortcut and camera fade setting live in `plugin/eff.ini`.

## Verification completed outside the game

- Normal MSVC DLL and proxy build passed. With Blender 5.2 and MSVC available, the combined Python and Blender suite passed: 275 tests, 2 skipped because optional external evidence was absent. The architecture follow-up covers six-owner retention and mixed control/native-skin refresh requests.
- AnimeStudio GUI built for `net9.0-windows`. It still writes EFFMESH v3; the Blender-side Mod export adds shape control declarations without changing the mesh format.
- Resource paths are required to be relative to the Mod declaration. Mesh, Material and Texture use one disk-path resolver. Native Mesh field correction remains enabled independently of diagnostic logging.
- UTF-8 Mod directory names and resource paths now use Windows Unicode disk APIs. Isolated tests cover a Chinese game directory, Chinese Mod folder and typed section names, Mesh/Material/Texture/Skeleton/Physics reads, cache file stamps, `state.ini`, global INI and F10 reparse. Game-side verification is still pending.
- Current candidate `eff.dll` SHA-256: `8AF9C60545BA0C1CBCAF2A4947AF74F1913F1E2975806D88A7EAF77BE8E57BF5`. It includes the default Mod selection and manager hotkey fix plus UTF-8 disk paths. It was deployed after confirming the game was closed; the immediately preceding DLL is backed up as `eff.dll.pre-unicode-path-20260926-190056.bak`. The existing `plugin/eff.ini` and Mod files were retained. Game-side verification of the hotkey and Unicode changes is pending.

The subsequent local DLL work adds per-Renderer failure rollback, F10 generation gating, direct NPC/UI Renderer ownership adoption, and a distinction between missing and unreadable `mod.ini`. The local build passed `build.bat` and the Python suite (286 tests, 14 skipped; the skips require unavailable optional external evidence). SHA-256 of `bin/eff.dll`: `39710BE9176122468516E2CF24DCB407ADD4419B13DBF254D7A7461AD364D965`. It is **not deployed or verified in game**. The earlier hash above identifies the installed candidate only.

## In-game acceptance still required

1. Start with the new DLL and inspect body, cloth01 and cloth02 in the world, UI and NPC views. Include multiple independent cold starts.
2. Before opening the manager, test the default Mod key. Open the manager, choose another Mod, and test its key both while the manager has focus and after closing it. Repeat after F10. Test shape sliders, including a shape with no key, and confirm the unselected Mod's keys do not respond.
3. Modify a Mesh, Material, Texture and skeleton binding separately; press F10 after each edit and verify the new content. Check recovery from a malformed Mod file without losing the previous generation.
4. Manually press F10 at least 100 times across normal gameplay, watching for falling cloth, missing material, overlap, tearing, hangs and crashes. Do not automate key presses. Record any first failing press and preserve its log.

The candidate is not a verified stable fix until these observations succeed.
