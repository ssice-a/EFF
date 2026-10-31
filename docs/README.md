# EFF Documentation

This index points to the current user and developer documentation in the EFF main repository. Date-stamped reports and files under `diagnostics/` are historical evidence; use the current contracts below for present behavior.

## Users

- [Chinese Mod tutorial](../Mod%20Tutorial.md) and [English Mod tutorial](../Mod%20Tutorial_EN.md): install, unpack, edit and export.
- [Shared Tools and Indexes](shared-tools-and-indexes.md): user-selected storage root, AnimeStudio index workspace, shared compiler package, and the division of work across four endpoints.
- [Material and dynamic effects](../tools/Blender/docs/material-effects.md) and [dynamic material/effect testing](dynamic-material-effects-testing.md).
- [Shape animation](../tools/Blender/docs/shape-animation.md) and [facial animation](../tools/Blender/docs/facial-animation.md).
- [Effect authoring](../tools/Blender/docs/effects-editor.md).

## Architecture

- [ADR-0001: Authoritative Mod Directory and Demand-Driven Resources](adr/0001-authoritative-mod-directory.md): accepted directory, DDS, runtime, round-trip, migration and timing contract.
- [ADR-0002: Self-Describing Runtime Resource Files](adr/0002-runtime-resource-files.md): format 3 manifest, resource files, DDS split, lazy runtime reads and migration acceptance.
- [Demand-driven runtime design](demand-driven-native-resource-loading.md): implementation evidence and acceptance gaps for the new route.
- [Format 3 transition status](format3-transition-status.md): what is wired into normal DLL, Blender, and Manager paths versus isolated acceptance tooling.
- [Runtime resource discovery](runtime-resource-discovery.md): historical source-input discovery and runtime matching evidence.
- [Release management](release-management.md): independent repositories, tool compatibility gates and local preflight.

## Archived Contracts

The old compiled-package and format-2 documents describe superseded runtime and authoring contracts. Keep them for migration and historical debugging; do not use them to define new output. See the [archive index](archive/README.md) for status and replacement references.

## Testing

- [Hot reload testing](hot-reload-testing.md): verified behavior and remaining in-game coverage.
- [Scene performance testing](performance-scene-test.md): measurement points and scenario matrix.
- [Blender scoped export](blender-scoped-resource-export.md): full and partial export behavior.
- [AnimeStudio VFS layers](unpacker-vfs-layers.md): index construction and incremental updates.

Historical diagnostics and design records remain available for context. They may describe retired implementations or older local builds; do not treat their dated measurements as current release results.

The format 3 transition is still in development. As of 2026-10-30, the
development DLL has a format 3 runtime/F10 path and has passed native request,
resource, physics, shared-dependency, and real-directory smoke tests. Blender
full export writes format 3; import restores Mesh, Material, DDS, Physics, and
Effect author data. Partial-resource export and Mod Manager migration remain
incomplete, and Blender 5.2 plus in-game format 3 acceptance are outstanding.
Do not deploy a format 3-only DLL against the installed format 2 Mods.
