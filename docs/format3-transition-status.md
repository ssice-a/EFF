# Format 3 Transition Status

The accepted target is defined by [ADR-0001](adr/0001-authoritative-mod-directory.md)
and [ADR-0002](adr/0002-runtime-resource-files.md). Those documents describe the
destination contract; they do not assert that all endpoints already implement it.

## Current State

| Endpoint | Implemented | Still missing |
|---|---|---|
| DLL | `format3-runtime` development build selects the format 3 startup, demand replacement, immutable candidate generation, F10 publication, and retirement paths. Material preparation resolves the current game Material by its declared logical path/name and applies sparse EFFMAT edits to that object. Smoke coverage includes manifest-only startup, per-request identity, shared dependency routing, Physics composition, lazy resource reads, and native Renderer assembly with unhit resources absent. | The normal production entry still uses the compiled format-2 path. No in-game format 3 acceptance has been recorded. Effects, DDS replacement during F10, loaded-owner refresh, and memory retirement still need integrated game validation. The installed Mods are format 2, so the format 3 DLL must not be deployed yet. |
| Blender | Full export builds the format-2 author records and INI in memory, then passes those bytes, editable snapshots, and exact EFFSOURCE template descriptors to the in-process `eff_author_assets.dll`. The DLL writes and validates the final format-3 directory, which Blender publishes transactionally. No Format 2 resource directory, TSV, AnimeStudio index, or author-snapshot files are created as staging. Standard DDS output/compression and Format-3 import codecs are covered by tests. | Selective Format-3 export remains blocked and reports that the Mod was left unchanged. Physics-only updates still use the format-2 path; Blender UI import/export has not yet been exercised against Blender 5.2 in this phase. The DLL currently copies in-memory Format 2 resources into its existing parser-owned buffers to preserve native field provenance, so peak-memory reduction still needs measurement. |
| Mod Manager | The repair flow has shared-index discovery, repository index download, selected-game VFS access, Bundle reading, and transactional replacement primitives. | One-click format-2 to format-3 migration is not implemented. The current repair entry point still requires a local index and does not invoke repository download. The target migration must reuse a valid selected/registered index, download the repository portable index when absent, then fall back to Manager's own VFS/Bundle index builder when offline. That fallback must work on a fresh install without AnimeStudio or a cached shared tool package. EIDX locates Bundle records but contains no native resource payload; migration reads required templates from local VFS. The repair command is not the migration. |

The format-3 DLL development build was compiled and passed its binary
acceptance check on 2026-10-30 (2,995,712 bytes; SHA-256
`260364442980A7C1755D9C17BCD1B2B659331CADB0607600FE45EE617FA5CE26`). This
artifact was not deployed. Native smoke checks passed for resource envelopes,
typed metadata, manifest-only relationship validation, request identity/shared
dependency routing, DDS byte spans, overlapping Physics targets, and the
format-3 catalog. At that time, the staged Chen directory opened 20 Meshes, 8 Materials, 29
Textures, and 1 Physics resource; its catalog/native-input smoke took about
40 ms on this workspace. That figure excludes game startup, Unity deserialization,
GPU upload, and in-game request latency. No real-game format-3 acceptance has
been recorded.

On 2026-10-31 the demand-loading implementation was tightened at two points:
`Catalog` resolves a Material's Texture dependencies through the same Texture
cache used by direct requests, and a returned Material retains those dependency
leases. Renderer binding metadata is validated only for the `BuiltMeshPart`
records in the current request; unrelated Mod Mesh declarations remain unopened.
The Catalog smoke declares missing, unhit Mesh/Material/Texture/Physics files,
verifies construction succeeds, then verifies those files fail only when their
own resource is explicitly requested. The native random-access smoke now runs
`StageCarrier` and `StageConsumer` with an additional unhit Part whose Mesh file
does not exist, covering the matched Renderer path and 0/1/N Mesh assembly.

The 2026-10-31 `format3-runtime` development DLL passed its binary acceptance
check (3,034,112 bytes; SHA-256
`3F0072583B53C2EF783346CA8B5CB06D8B038C72BF98C2D0E3917C91537A3191`). It was
not deployed. The Catalog, manifest-only, Physics overlap, and native
random-access/Renderer smoke tests passed. The available local Chen directories
(`bin/format3-verification-chen` and `bin/format3-migration-smoke-20261030/chen`)
fail the current reader with `author resource metadata structure differs`; they
are stale samples and cannot provide current real-resource acceptance. The
standalone resource-payload smoke also needs a valid current Format 3 directory.
No game launch or in-game Format 3 acceptance was performed.

The Blender-independent format-3 publication, native-export core, and texture
compression suites passed 52 tests on 2026-10-30. Mesh `.effres` payloads store
the native Mesh and editable EFFMESH extents with independent SHA-256 values.
Material `.effres` payloads store only the sparse EFFMAT document; their
metadata binds it to a logical source Material and the explicit Texture IDs in
`mod.ini`. Texture resources keep their native template separate from the
standard DDS file. Blender verifies the author snapshots and restores editable
Mesh geometry and EFFMAT properties/bindings. System-Python format and import
tests pass.
Blender UI tests still require Blender's bundled Python (`bpy` is not available
to system Python).

On 2026-10-31 the shared author writer changed from a process executable to an
in-process C ABI DLL. The Blender exporter passes its exact EFFSOURCE template
descriptors in memory; it no longer asks the user to register an AnimeStudio
index or writes a TSV template table. Full export now assembles Format 2
resources and `mod.ini` in memory and submits both with editable author
snapshots. The DLL validates the in-memory package and writes only the final
Format 3 candidate directory before transactional publication. This removes
Format 2 staging writes/reads; the C++ parser still owns a resource copy to
preserve current native provenance behavior, so peak memory and Blender 5.2
full-export time remain to be measured with a real project. Selective export
and Physics-only update have not yet moved to the Format 3 writer.

Format-3 Physics author round-trip stores the editable `EFFPHYS` document and
its exact `EFFSKEL` dependency after the native Physics input, with independent
extent/hash declarations. Blender import rebuilds the shared Rig from this
dependency before creating groups and colliders. Format-3 Effect author data
stores the original `.efffx` source tree together with the editable workspace
and attachment state. Native effect Bundle inputs stay in separate runtime
resources; the DLL does not read the author snapshot. Blender import
materializes the source tree into its user cache and restores the effect graph,
Mesh owner, mount Rig/bone, transform, and keys. Blender-independent codec and
publication tests pass; in-Blender execution and a real Mod export/import
remain outstanding.

The `format3-stage-catalogfix` Chen sample referenced in earlier notes is not
present in the current `bin` tree. Do not infer that the available stale samples
are current deployment inputs.

All installed game Mods observed during the 2026-10-30 development pass use
format 2. A format 3-only DLL must not be installed against them until Manager
migration is available and the runtime route has passed real-Mod acceptance.

## Work Order

1. Design records are archived in ADR-0001/0002. Temporary build products and
   diagnostics were moved into ignored `workspace-cleanup-*` folders without
   deleting source or user data.
2. Continue DLL correctness, request/F10 generation and retirement checks; keep
   the format-3 development DLL out of the game directory while installed Mods
   are format 2.
3. Replace the temporary format-2 full-export staging path with batched typed
   writer input, then validate Mesh/Material/Texture/Physics/Effect import and
   full export in Blender 5.2. Implement selective format-3 export after the
   same writer contract is used by full export. UI-level round-trip acceptance
   remains.
4. Add Mod Manager's one-click transactional format-2 migration. It must work
   without AnimeStudio or Blender installed: reuse a registered current EIDX,
   download the repository's portable index into the user's chosen location,
   or fall back to the selected game directory. EIDX is only a locator, so the
   migration reader must fetch the exact native templates from local VFS
   Bundles as required. Successful conversions retain a rollback copy; missing
   source data leaves the original Mod untouched.
5. Validate the three endpoints together before switching the normal DLL entry
   to format 3 only.
