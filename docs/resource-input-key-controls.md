# Format-2 runtime key controls

The serialized-input entry registers all `[Key...]` declarations automatically
for authored parts, including constant `visible=` expressions. Diagnostic or
GfxTrace markers are not required for controls. Current entry and resource
transactions are described in [the architecture](code-architecture.md).

## Input and state

- `cycle`: advances through the declared ordered values once per physical press.
- `press`: assigns one target once per physical press.
- `hold`: moves toward one target at `speed` units per Unity game-time second.
- Ctrl/Alt/Shift chords use the shared key parser. Sampling occurs once per
  Unity frame while the game owns the foreground window. Focus changes do not
  manufacture cycle/press edges from already-held chords.
- Physical bindings apply to every enabled Mod. `[Mod] key_switch_enabled=1`
  is the default, including INIs that omit the field; `0` disables that Mod's
  physical bindings. `true`/`false` are also accepted. Identical keys are allowed
  across and within Mods and run in declaration order. Every binding's edges
  are still sampled while disabled or covered by the panel.
- Instances of the same Mod share variables. Each controlled Renderer has its
  own validated binding. New instances receive current visibility during
  registration. The game's model completion callback registers only that model's
  children, including inactive renderers; native material-controller Init also
  registers its explicit list provisionally. The helper's `InitAll` completion
  activates bindings from its own cached Renderer list, covering NPCs that do
  not use a view part. Base/Complex view-part initialization reads native
  enabled intent in an exact nested scope so its initial snapshot cannot capture
  author hiding; other reads return actual enabled flags. Reused instances retain
  native intent and current author values. F10 registers rebuilt models immediately after
  the new control graph is published and restores bindings during rollback.
  No periodic scene-wide Renderer discovery is used.
- Declared `persist` values are numeric `[Values]` entries in `state.ini`.
  Cycle domains and shape ranges are checked on load; malformed saved values
  use defaults and report an error. The poller batches atomic writes without
  holding the input lock during I/O. Hot additions/readditions read saved values
  in a read-only candidate preview; commit preserves valid live values for the
  same ID/directory. Saved files carry an optional Mod ID comment, with legacy
  untagged files still readable; a runtime ID change cannot inherit untagged
  values from the previous owner. Pending saves check the current author ID.
- `state.ini` and its numeric PID temporary file are preferences, excluded from
  the author resource list and package digest by C++ and Python readers.

## Visibility, shapes and ownership

Each authored part has a serialized provenance marker derived from Mod ID,
part, source path/asset and author Mesh digest. Shape-free parts use
`__EFF_PART_`; Mesh-native shape channels keep `__EFF_SHAPE_`. Marker preparation
preserves geometry, indices, bindposes, weights and resource references.

The binding evaluates `Part.visible` against shared Mod state and combines it
with the game's native enabled request through public `Renderer.enabled`.
Showing a part cannot re-enable a Renderer that the game requested disabled.
Switches do not activate GameObjects, change bones or rebuild base resources.
Shape controls write only their declared channels through public bindings.
Registration inspects the original source getter, including during afterimage
capture. Immutable capture snapshots retain their native palette, frame and
lifetime until actual native Release.

## Shape animation clips

`[ShapeAnimation<ID>] path=resources/shape_animation_<ID>.bin` declares a sampled
Shape Keys clip. Its independent resource stores owner/clip identities, shortcut,
duration, default speed, loop/autoplay and float32 weight tracks. It refers to
the existing part shape variables without duplicating Mesh deltas. Blender's
unified N → EFF → 形态动画组管理 lists every group without requiring an active Mesh,
and edits each member's Shape Keys Action and KEY slot inline;
Object transform Actions, drivers and active NLA tracks are not accepted.

Meshes and channels may belong to multiple groups. Starting a group stops any
playing or paused group sharing its channels; disjoint groups can play together.
Conflicts are indexed once during control preparation, not recomputed per frame.
Physical shortcuts toggle
play/stop under the same per-Mod keyboard and focus rules. Ins exposes play,
pause/resume, stop, a speed slider, direct positive-number input and author-default
reset. Changing speed advances subsequent game time at the new rate without
moving the current phase. Speed preferences persist; sampled outputs and playback
time do not overwrite manual values or cause per-frame preference/UI writes.
Manual shape sliders and keys stop the associated group before applying a value.
Stopped or completed non-looping clips restore manual weights.

Instances of one Mod share clip time. F10 rebuilds playback state, restarting
autoplay clips and stopping the others while retaining valid speed preferences.
Mesh/full exports update clips; material/texture updates retain them. Animations
require `sampled-shape-weight-clips/le32/1` plus the compiler/consumer declaration
`shapeAnimationPlayback=shared-channel-groups/replace-conflicting-playback`.
The locally installed first version has Blender, package and native player
regression coverage; in-game animation rendering remains unverified.

## LZY bindings

| Key | Controlled Mesh suffixes | Declared cycle |
|---|---|---|
| NUMPAD5 | cloth03.lian, cloth04.lian, cloth06.baohsi | 0/1 |
| NUMPAD2 | cloth03.neiku, cloth04.neiku | 0/1 |
| NUMPAD3 | cloth03.bar, cloth04.bar | 0/1 |
| NUMPAD4 | cloth03.dou | 0/1 |
| NUMPAD1 | cloth03.xiu | 0/1/2 |

The current INI shows `xiu` only at value 1, so values 0 and 2 have identical
visibility. Arrow shape holds retain their assignments.

## Configuration and resource transactions

The reload shortcut comes from game `plugin/eff.ini`. Current source defaults,
generated configuration, project template and the 2026-10-06 v35 deployment
use F10 (VK_F10). After editing a binding, use the old active shortcut once
to publish the new one. Mod bindings may overlap global shortcuts; users control
which Mods respond through their INI flags and panel checkboxes. `gui=INSERT` toggles
the built-in control panel in current source. The separate Lua adapter is still
not wired to this host.

### Ins control panel

The ImGui panel uses an independent D3D11 composition window for either game
graphics backend. On Windows 11 it uses the system Desktop Acrylic backdrop,
with a translucent dark surface; unsupported systems keep a dark surface.
The graphics device and fonts are created on the first panel opening, and hidden
panels do not render. No game graphics hook is added.
The panel is an independent top-level tool window, without a cross-thread HWND
owner. Visibility still follows game focus/minimization. This avoids a synchronous
hide/activation wait on the game's window thread when closing the panel.

The panel lists independent keyboard checkboxes for Mods with
bindings. Each applies immediately and atomically saves the same INI flag on
the worker, preserving other declarations, comments and line endings. Manual
INI edits apply on F10 without re-exporting compiled resources. The panel also
shows the selected Mod directory/ID and its authored source paths, then exposes
binding buttons and declared shape sliders. It does
not infer the currently active character from a filename or select individual
instances: all instances of one Mod continue sharing that Mod's values.

Selection only chooses which manual controls are shown: initially choose the first Mod with keys,
otherwise the first with shape controls; retain that selection across successful
reload while its ID and author directory remain available. If removed, choose
the next selectable Mod. Selection is session state. It does not enable/disable
resources, hide other characters or discard their saved values.

While the panel is open, physical Mod keys pause; the panel's buttons and sliders
remain usable even for Mods whose physical keys are disabled. Closing restores
keys for every checked Mod. GUI holds advance with Unity game delta once per
frame. Losing focus, closing, changing selection and reloading end GUI holds.
Held cycle/press chords do not manufacture edges when selection/focus changes.
The GUI shortcut also samples physical edges across focus/config changes.

The UI worker owns its Windows controls, using immutable snapshots and a bounded
command queue. Unity applies commands only while focused/open and after checking
the control-graph revision; manual commands also check the selection revision.
Checkbox commands are independent of selection. Removed targets, old widgets,
undeclared slider variables and invalid ranges cannot update another Mod. The
prepared graph includes its panel snapshot before native/property mutation;
publication does not allocate after successful writes.

Control isolation is covered with a two-character Mod fixture, including identical
keys, removal of the selected/last Mod and readdition after an empty set. The
offline compiler now composes the full installed author set into one generation.
Resource loading depends on character baseline coverage, and Ins selection does
not enable/disable a character's Mod. Runtime add/remove/edit commits require F10;
background disk preparation does not publish the runtime graph or panel list.

With a full-resource package configured, reload selects the complete compiled
candidate, prepares controls/settings off-thread and commits them together
with the verified native generation on the Unity thread. Mesh, Material and
Texture edits use that same resource transaction. Current limitations and
game evidence are in [the package contract](offline-resource-package-format.md)
and [hot reload testing](hot-reload-testing.md).

Without a configured full-resource package, `eiem_entry_reload.h` accepts only
configuration/control edits whose native input contract is unchanged:

- key chords, cycle/press/hold declarations, speeds and value domains;
- constants, defaults and persistence;
- constant/dynamic `Part.visible`;
- shape bindings for channels already present in the authored Mesh;
- reload shortcut and camera fade setting.

The worker validates a pending candidate without publishing it. The Unity
frame prepares property changes, applies public visibility/shape writes with
readback, then publishes graph/settings after success. Failed writes roll
back, including the attempted failing write. Restoration gaps are reported
and are not claimed to preserve engine appearance.

Variables that remain persistent and valid survive commit. Other variables
start from new defaults. Held-key state is sampled to avoid synthetic press
edges. Removed shape bindings return previously owned channels to zero;
unowned channels remain unchanged. Active capture snapshots are retained.

Loading table version 3 binds the native input contract separately from the
full INI digest, allowing control-only edits to reuse verified rooted paths.
Resource bytes, references, ordering, source identities and LOD/skip fields
remain in that contract. Changes require a rebuilt full-resource generation;
the configuration-only entry rejects them. This preserves one resource route.

## Verification boundary

Earlier format-2 integration checked garment cycles, modifiers, holds/focus,
native enabled arbitration, late instances and preference restart. The user
accepted garment keys and body afterimages before the current cleanup. Those
results are evidence for their deployed versions, not all new source builds.

Current production fixtures cover default shortcut/discovery, configuration
rollback, stale candidates, full-resource owner rebuild and native retirement
with mock APIs. Optional loading/reload observers record evidence without
establishing native job completion. The world texture update passed on installed
v35c; reverse reload, the new shortcut, display instances and combined topology
changes remain in [the current game test](hot-reload-testing.md).

The control-panel fixture exercises the production scope/queue logic and real
Win32 dropdown/button/trackbar messages. Its UI check injects foreground
observations because a background test process may not acquire desktop focus.
The current paired v35 DLL has been deployed; Ins, cursor/focus behavior and
panel operations still require game acceptance. Build/run the fixture with:

```powershell
cmd /c tools\diagnostics\build_control_panel_smoke.cmd
& .\bin\resource-directory-entry\control_panel_smoke.exe --ui-smoke
```
