# Body weight repair, 2026-11-02

## Scope and matching

The donor was `G:/zmd/lx/洛茜新.blend`, body object
`MeshS_actor_wulfa_body_01_lod0_0.001`. Repair used the installed ModTool's
UV matching algorithm (tolerance 1e-5), with UV multiset/topology refinement
for editable projects. Only identified body vertex weights were transferred.
Positions, normals, tangents, UVs, shapes, indices, materials and controls
were preserved. Unmatched or ambiguous vertices retained their weights.

Legacy installed snapshots needed the opposite V lookup orientation. This
was a correspondence operation only; no UV coordinates were changed.
Ambiguous donor UVs were accepted only if their named weight rows agreed.
Missing corrective bones were merged through the donor hierarchy into the
nearest bone actually present in the target rig. A missing old Mesh palette
slot was not treated as a missing rig bone: required valid bones received
the target rig's calibrated bindposes. Common bindposes agreed exactly with
the installed resources before patching.

## Editable projects

Eight project files under `G:/zmd` were repaired and saved, with original
copies in each project's `EFF_weight_repair_20261102_backup` directory.
Across 179 inspected Mesh objects, 157,268 author vertices matched and
105,025 vertex weight replacements were issued. Cleanup removed 10,848 empty vertex groups while
preserving every positive weight, including very small positive values.
All eight saved projects were reopened in an isolated Blender process and
compared with their backups: geometry, UVs and shapes remained identical,
cleanup-only Mesh weights remained exact, and matched named weights agreed
with their saved repair snapshots. Some assignments round to the original
Float32 value, so replacement counts exceed the number of stored changes.
The Blender exporter also compacts unused serialized bone palettes, and
its undoable empty-group cleanup is included in EFF Blender v0.50.0.

## Installed resources

At 2026-11-02 07:51:12 +08:00, 160 Mesh resources were deployed to
`D:/Hypergryph Launcher/games/Endfield Game/plugin/mods`. Fourteen body
resources received 135,821 runtime/split vertex matches. Twenty-six
ambiguous vertices were left unchanged. Other Mesh changes only compacted
unused bone slots and remapped the corresponding indices/metadata.

| Mod | Patched Mesh resources | Matched body vertices |
| --- | ---: | ---: |
| chen | 1 | 12,924 |
| gly | 29 | 15,611 |
| lino | 34 | 40,255 |
| lx | 20 | 0 (palette cleanup only) |
| LZY | 14 | 13,210 |
| si | 19 | 31,022 |
| Typhoeus | 23 | 9,992 |
| ZFY | 20 | 12,807 |

Total palette slots decreased from 14,317 to 2,943. Mesh resource bytes
decreased by 5,357,124; this is a file-size reduction, not a measurement of
game RAM or GPU residency. No runtime compatibility path was added.

## Verification and rollback

Each resource was decoded before and after patching. All native fields
outside vertex data, bindposes, bone hashes and bone bounds remained
byte-identical. Non-skin vertex streams and unmatched native UNorm16
weight bytes remained exact. Author geometry, UVs and shape arrays remained
exact. Named author/native weights agreed within native quantization
tolerance. Per-bone bounds were recomputed from actual native geometry,
shape envelopes and quantized influences using the compiler's convention.

A complete staged directory and the installed directory both passed the
resource audit and all eight real runtime Catalog tests. The lx owner
palette now has 24 used bones, and its immutable effect publication retains
those ordered paths before opening native geometry. Effect resources,
controls, textures, materials, DLLs and configuration were not modified.
The deployment verified hashes for 504 unchanged files and can roll back
all intended changes if post-install audits fail.

Original installed Mesh files:
`plugin/.eff-deployment-backups/body-weights-20261102-075108/`.

The complete hash receipt and installed audit are under
`bin/local-deployments/body-weights-20261102-075108/`. Source/project and
UV-match reports remain in the ignored
`bin/release-weight-repair-20261102/` directory. User Mod files are never
part of the public release. Game-side visual acceptance of these new
weight repairs is still pending; the earlier lx effect fix was accepted
before this asset deployment.
