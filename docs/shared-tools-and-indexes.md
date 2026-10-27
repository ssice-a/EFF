# Shared Tools and Indexes

EFF, AnimeStudio, EFF Blender and EFF Mod Manager remain independently versioned repositories. Shared authoring executables and index locations are owned by the EFF repository; the applications read user-selected paths instead of copying large indexes between projects.

## User paths

`%LOCALAPPDATA%\EFF\Resources\settings.json` stores only the selected paths:

```json
{
  "format": "EFF_SHARED_RESOURCES/1",
  "resourceRoot": "R:\\EFFResources",
  "indexWorkspace": "I:\\AnimeStudioIndex"
}
```

The resource root `R` is user-selected in Blender, AnimeStudio, or EFF Mod Manager. The controls are in Blender's **N > EFF > EFF 工具存储** panel, AnimeStudio's **EFF > Check / download shared EFF tools** menu, and Mod Manager's **Settings > Resources** section. Each client can check or download the same EFF Release package. The first client downloads it; the others recognize and reuse the verified SHA256 cache under `R/tools/<sha256>/`, selected by `R/tools/current.json`. No compiler or index is placed in C drive by the shared configuration.

Selecting `R` in any one application makes it available to the other two through this shared setting; users do not need to select or download the same tool package again. Each application reads the shared setting and looks for the cache at that path. If no shared path is registered, the user chooses it once in any client. The download action checks the latest manifest, reuses a matching verified cache, and downloads only when needed. Applications do not scan all drives for arbitrary folders, which avoids slow and ambiguous discovery. Changing `R` later changes the shared location but does not move the old cache; prepare the tools at the new path to download or reuse a package there.

The index workspace is a separate user-selected location. AnimeStudio creates, imports, or downloads its index there and records the workspace path in `%LOCALAPPDATA%\EFF\Resources\registry.json`; the index itself stays where the user put it. Blender's **N > EFF > AnimeStudio 解包索引** panel selects the location containing AnimeStudio's unpack index. This setting is not part of the Blender installation dialog. Blender does not need the extracted-output directory and does not copy or download the full index.

Blender accepts the AnimeStudio workspace, its `index` folder, or `endfield_assets.eidx` as the selected location. The configured index is a location hint only: the compiler validates the index against the selected game before reuse. If the game build changed, the configured index must be refreshed in AnimeStudio or by the supported index updater.

AnimeStudio registers each completed index workspace against its game installation. Blender and Mod Manager first recognize the selected workspace and registered workspaces, then validate an index against the active game before using it. Blender keeps a manual selector in its EFF sidebar for an index that was downloaded or copied without AnimeStudio registering it; selecting it records the same shared workspace setting for the other clients. It does not ask for the extraction-output directory.

## Shared tool package

Each EFF Release builds one `EFF_Shared_Native_Tools_<tag>.zip` and publishes `eff-native-tools.json`. The manifest contains the immutable release URL, byte length, SHA256, file layout and EFF framework tag. Any of the three clients can fetch this manifest from the latest published EFF Release, verify the archive, then cache it under `R/tools/<sha256>/`. A failed download or digest check is not installed. Blender author export only reads the installed cache; it never blocks the export UI on a first-time download.

The package currently contains:

- `eff_resource_pack.exe`: offline author resource compilation and source discovery.
- `eff_source_index.exe`: incremental index refresh for Blender export.
- `eff_author_core.dll`: native authoring codecs used by Blender.
- `package-protocol.json`: capabilities that are checked against the game DLL and author code.
- `vendor/`: Python dependencies used by the native author core.

The DLL does not load these authoring tools. It reads already compiled Mod output at runtime. AnimeStudio builds and validates its index in-process, then registers the workspace for Blender and the manager. The manager uses the shared compiler only for its offline repair workflow.

The Blender add-on may include the small Python codecs and data models needed to edit source resources. It must not include a second copy of shared `.exe`, `.dll`, `.pyd`, or the full source index. Its package validator rejects native executables and libraries.

## Build and release

Build EFF on Windows with MSVC, .NET 9 and Python 3.11. The release workflow builds the production DLL and protocol first, then builds the shared authoring tools and verifies the complete archive against that protocol before creating a draft. Pillow is assembled from its official Windows x64 cp311 and cp313 wheels because Blender 5.0 and 5.2 use different Python ABIs. Publishing the EFF Release makes both the runtime DLL and shared-tools manifest available.

AnimeStudio can be built independently with .NET 9 and .NET 10. Its output is self-contained by framework; a successful build does not publish it. Blender targets Blender 5.2 and Python 3.13 on Windows x64. Its shared Pillow vendor also carries cp311 extensions for Blender 5.0 clients. Blender CI reads the latest published EFF shared-tools manifest and checks out its framework tag for the author-code contract tests. Mod Manager targets .NET 9 Windows and resolves the same EFF manifest when repair needs the compiler.

Each repository keeps its own history, version and Release workflow. A new EFF tool protocol must be published before Blender or Mod Manager can consume it. AnimeStudio-only changes remain independently releasable. Do not publish from a dirty tree or claim a local build is a public Release.

## Local installations

Install locations are machine-specific and are not part of the shared resource settings.

- Blender 5.2 add-on installation follows Blender's per-user add-on directory.
- EFF DLL and proxy installation locations depend on the game installation.
Preserve user workspaces and Mod data when updating an application.

Local deployment receipts should record each binary's source revision, output path and validation result. Restart Blender after replacing an add-on whose Python modules are already loaded.
