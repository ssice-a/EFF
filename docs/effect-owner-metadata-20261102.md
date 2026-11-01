# Format 3 lx effect owner metadata

## Evidence

The installed `eff_log.txt` from PID 38768 on 2026-11-02 repeatedly reports
`effect owner bone palette differs`, with `expected=0 actual=145`, for lx
attachment `428fc265c91642bab14c33719e961d7f`. The selected owner is
`PartPartS_actor_wulfa_cloth_01_lod0_1.___8c6702d24e8fe635`.
The eight installed Mod catalogs contain effect inputs only in lx (three
native inputs, 22,562,472 bytes). This does not imply other Mods lost effects.

## Cause and fix

The Format 3 Catalog publishes an immutable control Package before native
Mesh demand. Shape-controlled Meshes already receive channel metadata at
this boundary, but bone-mounted effect owners did not receive their ordered
bone paths. Native carrier/consumer compositions fill only their own copies
later; the published effect owner still had an empty palette.

The Catalog now resolves the union of shape-controlled Meshes and explicitly
bone-mounted effect owner Meshes, reads their self-describing metadata once,
and copies the bone palette alongside the channel descriptor before control
publication. Native geometry, editable snapshots and effect input payloads
remain demand loaded. The existing exact Part, palette count, ordered path
and unique skeleton anchor checks remain enforced. There is no format change
or legacy compatibility path, and no Mod re-export is required for this fix.

## Verification and remaining acceptance

The regression uses actual Format 3 resource envelopes, Catalog, Author and
immutable control publication, then calls production ModelReady/Frame. Before
the fix it reproduces `expected=0 actual=1`; afterward it binds and plays the
bone-mounted native instance, rejects an incorrect palette, and leaves a
missing unrelated Mesh unopened. The real lx catalog independently verifies
all 145 owner paths before any native Mesh is opened. All eight installed
catalogs pass their resource audits.

These checks establish the metadata failure and its correction. The installed
game's world/NPC/UI rendering still requires a fresh run with the fixed DLL;
mock playback and file audits alone do not establish visible particles.
The user confirmed visible lx effects in game on 2026-11-02 and authorized
continuing the release and pending Mod weight deployment.

## DLL-only deployment

At 2026-11-02 07:27:49 +08:00, after confirming Endfield was no longer running,
the verified production DLL was installed at `plugin/eff.dll`. SHA-256:
`D5385F2973B9A6681FB59DB136634A90215C75BC3E98DF6B1F044DE1A596651E`.
All 663 protected Mod/configuration/proxy files retained their previous hashes.
The old DLL is in
`plugin/.eff-deployment-backups/eff.dll-before-effect-owner-metadata-20261102-072743.bak`;
the reproduced log and deployment receipt are in
`bin/local-deployments/effect-owner-metadata-20261102-072743/`.
The production binary verifier, full effect suite (including lazy native view
playback), shape/visibility bridge, full resource reload, and demand resource
regressions passed. Restart acceptance is pending.
