# Archived Design and Contracts

These documents are retained as historical evidence and migration references. Their format and runtime instructions are superseded as design decisions by [ADR-0001](../adr/0001-authoritative-mod-directory.md) and its concrete [format 3 contract](../adr/0002-runtime-resource-files.md). The target is not fully wired into normal endpoint workflows yet; see [transition status](../format3-transition-status.md).

| Document | Historical scope | Current authority |
|---|---|---|
| [Offline resource package format](../offline-resource-package-format.md) | Generated block store and compiled package | ADR-0001; Mod directory with `mod.ini` as the relationship manifest |
| [Mod INI/resource format v2](../mod-ini-resource-format-v2.md) | `compiled.bin`, `.mesh/.mat/.tex`, and offline consumer preparation | ADR-0001; standard DDS and runtime target matching |
| [Compiled resource graph](../compiled-resource-graph.md) | Offline consumer graph and source proofs | ADR-0001; game-request-driven target matching |
| [DLL code architecture](../code-architecture.md) | Prior production route and component ownership | ADR-0001 for the target; transition status for implementation readiness |
| [Resource update workflows](../update-workflows-20261015.md) | Earlier game-update and Mod-repair workflow | ADR-0001; Mod Manager one-time format migration |

Historical performance measurements and dated diagnostic reports remain in their original locations. They are evidence for the build and game version stated in each report, not current acceptance results.
