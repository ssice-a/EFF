# ADR-0001: Authoritative Mod Directory and Demand-Driven Resources

- Status: Accepted
- Date: 2026-10-30
- Scope: EFF DLL, Blender exporter/importer, and EFF Mod Manager

## Context

The current Mod layout mixes author files, generated `compiled.bin` and
`compiled.blocks`, and a second deployed author-resource directory. The DLL's
new demand-driven path is still an acceptance build, while the normal route
continues to consume offline-compiled consumer inputs. Texture compression has
also started in the Blender worktree, but its current output strips the DDS
header and embeds only BCn mip bytes in `.tex` resources.

The target must follow the game's actual resource requests, keep export and
F10 work proportional to changed resources, permit users to organize and
replace resources directly, and preserve enough authored data for Blender to
import and edit every exported resource type.

## Decision

1. A Mod is a directory. It is not required to be one archive file.
2. `mod.ini` is the only persisted relationship and rule manifest. It maps
   stable resource IDs to user-chosen relative file paths, targets, bindings,
   controls, and resource relationships. Users may organize subdirectories and
   reuse one resource from multiple targets.
3. Mesh, material, physics, effect, and other resource files are independent,
   self-describing author resources. Their canonical representation must retain
   the data Blender needs for import and subsequent editing. No generated binary
   catalog or compiled index may be required to keep a Mod valid after a user
   edits `mod.ini` or replaces a resource file. Runtime lookup tables are
   transient and derived from the manifest.
4. Texture files are ordinary DDS files. EFF does not append private data,
   rewrite the DDS header, or wrap the file in a proprietary container. Blender
   chooses compression and mip generation before encoding, using the source
   texture and material binding semantics as defaults. External DDS files are
   bound according to their material/resource semantics; a color-space flag
   mismatch alone is not a rejection condition. DDS format metadata describes
   the encoded block layout; the binding semantics determine how the game uses
   the texture.
5. Expensive author work is offline: image conversion/compression, resource
   encoding, structural validation, and author-data publication. Runtime does
   not recursively scan the Mod, hash or decompress every asset, or rebuild a
   whole package. It reads only declared rules at startup and opens resources
   when the game's corresponding load request needs them.
6. PFB consumers are discovered at runtime from the game's real requests. The
   DLL performs only the local serialized edits required for the currently
   requested hit, then lets the game load, create, share, and retire native
   resources. It does not enumerate all consumers offline or rewrite unrelated
   PFBs. Any source block that must be decoded for a local edit is measured and
   bounded; moving consumer discovery online does not justify whole-package
   decompression.
7. F10 detects changed declared resources and refreshes only affected resources
   and loaded owners. Replacing a DDS does not require Blender re-export.
   Publication uses immutable generations so an in-flight game read can finish
   against the old file while the new version becomes current.
8. Every endpoint records phase timings and relevant bytes. DLL measurements
   distinguish EFF preparation from Unity loading and track process working
   set/private commit; GPU residency is measured separately where available.
   Blender records snapshots, resource encoding, DDS compression, incremental
   writes, validation, and publication. Mod Manager records detection,
   conversion, validation, and commit.
9. Mod Manager provides a one-click, transactional migration from supported
   legacy Mod directories. It stages and validates the new directory before
   publishing it and retains a rollback copy. Legacy runtime reading is not
   kept in the new DLL. If author data needed for a lossless conversion is
   absent, migration reports the exact missing data and leaves the old Mod
   intact. Migration must not require the user to install AnimeStudio or the
   Blender add-on. It first reuses a compatible index workspace already
   registered by any EFF endpoint; if absent, it may download the shared
   portable index from the main EFF repository into the user's chosen index
   location. If the network is unavailable or no compatible index exists, it
   reads the selected local game directory and creates or refreshes the needed
   index there. The index is a locator, not a payload archive: it identifies
   logical assets and their native records, while exact source bytes are read
   from the matching local game Bundles only when migration needs them. The
   user controls the storage location; no fixed C: drive cache is required.

The concrete per-file and `mod.ini` contract is specified in
[ADR-0002](0002-runtime-resource-files.md).

## Consequences

- Users can edit the manifest and replace ordinary DDS files without updating
  a second binary index.
- Selective Blender import follows resource IDs and types in `mod.ini`; it does
  not need to load unrelated resources.
- Loose files add filesystem open/metadata work. Startup must not enumerate or
  open every payload. A packed cache may be considered only after real
  benchmarks and may never be required for correctness or editability.
- Existing v2 author directories and compiled artifacts are migration inputs,
  not the new runtime contract.
- Successful `skip` matching is one verified operation, not acceptance of the
  complete Mesh, material, texture, physics, effect, F10, and lifecycle paths.

## Implementation Order

1. Archive this decision and move only confirmed temporary build outputs into
   the ignored workspace-cleanup area. Preserve all uncommitted source changes.
2. Complete the DLL demand route, runtime resource reads, standard DDS loading,
   F10 updates, lifecycle retirement, and performance instrumentation.
3. Update Blender export and import to the authoritative directory/resource
   contract, including standard DDS output and semantic round-trip tests.
4. Add Mod Manager's one-click legacy migration and transactional rollback.
5. Validate all endpoints together using existing Mods, DDS-only replacement,
   selective resource import/export, cold/incremental export timings, and
   runtime memory/load measurements.

## Superseded Documents

The following documents remain as historical implementation contracts and
evidence. Their current-format statements are superseded by this ADR and the
[demand-driven runtime design](../demand-driven-native-resource-loading.md):

- [Offline resource package format](../offline-resource-package-format.md)
- [Mod INI/resource format v2](../mod-ini-resource-format-v2.md)
- [Compiled resource graph](../compiled-resource-graph.md)
- [DLL code architecture](../code-architecture.md)

