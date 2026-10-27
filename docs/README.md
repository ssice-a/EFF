# EFF Documentation

This index points to the current user and developer documentation in the EFF main repository. Date-stamped reports and files under `diagnostics/` are historical evidence; use the current contracts below for present behavior.

## Users

- [Chinese Mod tutorial](../Mod%20Tutorial.md) and [English Mod tutorial](../Mod%20Tutorial_EN.md): install, unpack, edit and export.
- [Shared Tools and Indexes](shared-tools-and-indexes.md): user-selected storage root, AnimeStudio index workspace, shared compiler package, and the division of work across four endpoints.
- [Material and dynamic effects](../tools/Blender/docs/material-effects.md) and [dynamic material/effect testing](dynamic-material-effects-testing.md).
- [Shape animation](../tools/Blender/docs/shape-animation.md) and [facial animation](../tools/Blender/docs/facial-animation.md).
- [Effect authoring](../tools/Blender/docs/effects-editor.md).

## Architecture

- [Code architecture](code-architecture.md): DLL, author compiler, source index and package ownership.
- [Runtime resource discovery](runtime-resource-discovery.md): source inputs, renderer consumers and runtime matching.
- [Offline resource package format](offline-resource-package-format.md): the compiled author package and protocol boundary.
- [Mod INI format 2](mod-ini-resource-format-v2.md): author rules and resource references.
- [Resource update workflows](update-workflows-20261015.md): game updates, extracted-source refresh and Mod repair.
- [Release management](release-management.md): independent repositories, tool compatibility gates and local preflight.

## Testing

- [Hot reload testing](hot-reload-testing.md): verified behavior and remaining in-game coverage.
- [Scene performance testing](performance-scene-test.md): measurement points and scenario matrix.
- [Blender scoped export](blender-scoped-resource-export.md): full and partial export behavior.
- [AnimeStudio VFS layers](unpacker-vfs-layers.md): index construction and incremental updates.

Historical diagnostics and design records remain available for context. They may describe retired implementations or older local builds; do not treat their dated measurements as current release results.
