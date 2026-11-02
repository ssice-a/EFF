# Package Resource Paths Release, 2026-11-03

EFF v2.0.5 and EFF Blender v0.50.2 are public. Format 3 remains the only runtime format. The release adds `packages/` identities alongside `assets/` for Mesh targets and Material/Texture sources, with traversal, backslash, drive-prefix and empty-segment rejection.

| Repository | Version | Source commit | Release | Successful workflow |
| --- | --- | --- | --- | --- |
| [ssice-a/EFF](https://github.com/ssice-a/EFF) | v2.0.5 | `c889a7f5ef5d964aab761a60a43519373b5ba036` | [Release](https://github.com/ssice-a/EFF/releases/tag/v2.0.5) | [Build and Release #41485510141](https://github.com/ssice-a/EFF/actions/runs/41485510141) |
| [ssice-a/EFF-blender](https://github.com/ssice-a/EFF-blender) | v0.50.2 | `893a7eb16632b6dade51b8eb0653c1f350d69cee` | [Release](https://github.com/ssice-a/EFF-blender/releases/tag/v0.50.2) | [Release #41487687608](https://github.com/ssice-a/EFF-blender/actions/runs/41487687608), [CI #41487687600](https://github.com/ssice-a/EFF-blender/actions/runs/41487687600) |

All workflow runs completed successfully. The Format 3 directory-package suite passed 39 tests, the Blender export core passed 51 tests with one intentional skip, the Format 3 author DLL ABI smoke passed, and the runtime author-manifest smoke accepted package identities while rejecting traversal. The EFF Release workflow also passed its DLL, index/source, migration, visibility, reload, and packaging checks. Blender's Release and CI workflows passed against the published EFF v2.0.5 shared-tools manifest.

| Repository | Asset | Bytes | SHA256 |
| --- | --- | ---: | --- |
| EFF | `EFF_v2.0.5_dll.zip` | 1,588,312 | `ff22427ae3bb0e81542d428215c52681400833ddfc07875ac20aad52ea7f783e` |
| EFF | `EFF_Shared_Native_Tools_v2.0.5.zip` | 96,114,313 | `e0e8592d8e60a8f8d71754210426c9dc7aeac7070eba28d85babddd0341b4ede` |
| EFF | `eff-native-tools.json` | 770 | `2e447a564da13096b01db54247159c181d28c4d2dbc23401cf42877d7b930688` |
| EFF Blender | `EFF_Blender_v0.50.2.zip` | 367,947 | `77d96bbf79894f019f60c987dff04a2201dedcfe9e71f0465708126187e4395b` |

Each archive was downloaded and its byte length and SHA256 matched GitHub's asset metadata. The DLL ZIP contains the two proxy loaders, `plugin/eff.dll`, configuration, protocol receipt and empty Mods directory. The Blender ZIP contains the add-on source and resources without native tool binaries.
