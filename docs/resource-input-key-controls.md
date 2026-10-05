# Format-2 runtime key controls

The serialized-input entry registers all `[Key...]` declarations automatically
for every authored part, including constant `visible=` expressions. Diagnostic/GfxTrace markers are not required for controls.

## Input and state

- `cycle`: advances through the declared ordered values once per physical press.
- `press`: assigns one target once per physical press.
- `hold`: moves toward one target at `speed` units per Unity game-time second.
- Ctrl/Alt/Shift chords use the existing key parser. Sampling occurs once per
  Unity frame while the game owns the foreground window. Focus changes do not
  manufacture cycle/press edges from already-held chords.
- All instances of the same Mod share variables. Each controlled Renderer has
  its own validated binding. New instances receive current visibility during
  controller registration; public scene discovery covers other actor/UI/NPC
  consumers once per second, including inactive objects when that public
  overload is available.
- Declared `persist` values are stored as numeric `[Values]` entries in
  `state.ini`. Cycle domains and shape ranges are checked on load; malformed
  saved data uses the declared defaults and reports an error. The Win32 poller
  batches atomic writes every 250 ms without holding the input lock during I/O.
- `state.ini` and its numeric PID temporary file are preferences, excluded from
  the author resource list and package digest by both C++ and Python readers.

## Visibility and native ownership

Each authored Mesh part has a serialized provenance marker
derived from Mod ID, part, source path/asset and author Mesh digest. Shape-free
parts use `__EFF_PART_`; Mesh-native shape channels keep `__EFF_SHAPE_`, independently of current runtime bindings. Source
geometry, indices, bindposes, weights, materials and texture references remain
unchanged during provenance preparation.

The renderer binding evaluates `Part.visible` against the shared Mod state and
combines it with the game's native enabled request through the public
`Renderer.enabled` binding. Showing a part cannot re-enable a Renderer that the
game requested disabled. No GameObject activation, bone changes, base Mesh
replacement or private Renderer/Job writes are performed during a switch.
Registration always inspects the original source getter, including inside the
afterimage capture scope. Shape snapshots and native palettes retain v24's
`skinningRoot` frame and lifetime behavior.

## LZY current bindings

| Key | Controlled Mesh suffixes | Declared cycle |
| --- | --- | --- |
| NUMPAD5 | cloth03.lian, cloth04.lian, cloth06.baohsi | 0/1 |
| NUMPAD2 | cloth03.neiku, cloth04.neiku | 0/1 |
| NUMPAD3 | cloth03.bar, cloth04.bar | 0/1 |
| NUMPAD4 | cloth03.dou | 0/1 |
| NUMPAD1 | cloth03.xiu | 0/1/2 |

The current INI shows `xiu` only at value 1, so values 0 and 2 intentionally
have identical visibility. Arrow shape holds keep their existing assignments.

## Verification and remaining stage

The real LZY package integration covers nine controlled garment parts, eighteen
initial Renderer instances, every cycle, modifiers/press, holds/focus, native
enabled arbitration, late instances and persistent restart round-trip. A
reparsed package retains its digest after preference save. The capture bridge
regression checks frame updates, public shape writes, native matrix requests,
slot order, skinningRoot frame, exact extents and native snapshot lifetime.

The user accepted the v25 garment keys and body afterimage. GfxTrace remains off.
The v26 regression retains the accepted palette/frame/snapshot behavior and
registers all 43 authored part/LOD identities, including fixed `toushi` and
cloth06.neiku. The 60 prepared loading inputs are verified against the accepted
v25 inputs: only three files have different hashes; all geometry, native shape
fields, Renderer fields, material and texture fields are preserved.

## F10 configuration transaction (v26)

The reload shortcut is read from game `plugin/eff.ini`; the default is F10.
`Ctrl`, `Shift` and `Alt` are supported. After changing the shortcut, invoke the
old shortcut once to commit it. Reload and GUI reserve their base keys, and a
Mod binding on either base key is rejected because modifier sets can overlap.

The worker reads global settings and all Mod packages, checks identity conflicts
and the native input contract, and prepares a candidate without publishing it.
The regular Unity frame consumes that candidate once per frame. It reads the
existing Renderer properties, prepares all changes, applies public visibility
and shape setters with readback, and publishes the graph, loading generation
and settings only after success. Failed writes are rolled back, including the
attempted write that failed. Rollback failures are reported separately; they
cannot be claimed to have restored engine appearance.

Supported live edits:

- key chords, cycle/press/hold declarations, speeds and value domains;
- constants, defaults and `persist` declarations;
- `Part.visible`, including constant/dynamic changes on previously fixed parts;
- shape-to-variable bindings for channels already present in the authored Mesh;
- `eff.ini` reload shortcut and independent camera anti fade flag.

Valid values of variables that remain persistent survive the commit. Ordinary
variables start from new defaults; removed persistence or incompatible ranges
use the new defaults. New held-key edge state is sampled physically to avoid a
synthetic cycle/press on reload. Formerly controlled native shape channels are
reset to zero when their binding is removed; other native channels are retained.
Afterimage adaptation follows the Mesh's native channels, so removing runtime
bindings does not bypass shaped capture handling. Active immutable capture
snapshots stay alive until native Release; reloading never clears their groups.

Loading table version 3 binds an explicit native input contract digest instead
of the full INI digest. Runtime-only edits can reuse the already verified,
rooted loading paths. Serialized bytes, resource declarations, material/texture
bindings and sampling fields, Mesh/material references, rule/part ordering,
source identities, LODs and skip handling remain part of that contract. A change
to these fields is rejected in v26 and retains the previous graph/settings.
Legacy table versions remain readable for startup; configuration reload requires
version 3. The author EFFRES2 resource format remains unchanged.

## Remaining DLL stage

Full Mesh/material/texture resource hot reload is not implemented by the v26
configuration transaction. It still needs native input rebuilding, asset/cache
lifetime management, and migration of existing actor/Renderer instances as one
verified operation. It must not silently use the old serialized inputs for new
resources. Finish and verify that DLL path before updating Blender/unpack export.

Game verification of v26 camera fade and F10 is pending. Current-entry tests
exercise the actual LZY INI/v3 table, live state, native setter failures, frame
commit/deduplication, invalid/stale/resource/reserved-key rejection and retained
capture snapshots. The camera fixture installs the production Exact adapter
with real MinHook and checks per-instance order, MethodInfo, setting/manager
gates and original exception propagation. Simulation is not game visual proof.

## Full-resource verification observation (v27)

The optional `full-reload-observation.enabled` marker records natural game
loading, model assembly/release, per-Mesh bone/bindpose counts, and current
configuration F10 timings. It initiates no resource/model refresh and retains
the v26 rejection of native input contract changes. GFX remains disabled.
See [the staged validation protocol](full-resource-reload-validation.md).
