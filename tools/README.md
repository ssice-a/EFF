# EFF Endfield resource browser

Current authoring/runtime scope: [documentation index](../docs/README.md).

EFF keeps [ssice-a/AnimeStudio](https://github.com/ssice-a/AnimeStudio) and
[ssice-a/EIEM-blender](https://github.com/ssice-a/EIEM-blender) as separate Git
submodules. Develop AnimeStudio in `tools/AnimeStudio` and the Blender add-on in
`tools/Blender`. Initialize both after cloning:

```powershell
git submodule update --init --recursive
```

It does not require a 53 GB decrypted `.ab` tree or a separately generated
AssetMap.

## Run

```text
tools\AnimeStudio\dist\win-x64-vfs-next\AnimeStudio.GUI.exe
```

Choose `File -> Open Endfield VFS...`, then select:

1. `D:\Hypergryph Launcher\games\Endfield Game\Endfield_Data\StreamingAssets\VFS`
2. a workspace outside drive C (the verified workspace is `E:\EFF_Workspace`)

The first open of a new game build reads each logical Bundle directly from its
`.chk` range, decrypts it in memory, records its Unity objects and CAB
dependencies, then immediately releases it. `File -> Abort` pauses safely; the
same VFS/workspace resumes from the last checkpoint. No plaintext Bundle tree
is created.

The result is `index\endfield_assets.eidx`, a versioned compact binary index.
It is reused while the VFS fingerprint is unchanged and rebuilt after a game
update. On the verified client:

```text
logical VFS files: 448,995
logical Bundles:   237,794
indexed resources: 3,385,891
virtual folders:   23,252
index size:        702 MB
cached open:       14.44 seconds
managed memory:    545.8 MB
failed Bundles:    0
```

## Browse and export

The normal AnimeStudio window remains in use. `Scene Hierarchy` is a lazy,
unified Container-path tree; `Asset List`, regex search, column sorting,
`Filter Type`, native preview and native exports remain available.

Selecting a resource performs an on-demand operation:

```text
indexed Unity object
  -> logical Bundle path
  -> BLC range in one CHK
  -> decrypt selected Bundle in workspace cache
  -> follow CAB references and decrypt only required dependency Bundles
  -> AnimeStudio parses the dependency closure
  -> Preview / Export Selected
```

The dependency path was validated with two character Prefabs requiring 13 and
19 Bundles. A real export produced an FBX containing four meshes plus seven PNG
textures and one material. The sample is at:

```text
E:\EFF_Workspace\sample_export\chr_0031_mifu_deco_1
```

The browser/export workflow ends at Blender-readable files. An edited FBX is
not itself a Unity runtime asset. Returning edits to the game still requires a
compatible plaintext AssetBundle at the original logical path; EFF's runtime
DLL already redirects `LoadBundleFromFile` to
`plugin\mods\override\<logical path>` and falls back to the original VFS on a
miss or failed load.

For the EFF workflow use `Export -> Format specific -> EFF`. This writes a
per-asset exchange directory containing an FBX (or OBJ for a standalone Mesh),
converted textures, material JSON, and `eff.json` with the source/path ID
identity. `File -> Export EFF from JSON...` accepts the runtime scene dump or
an EFF selector JSON and exports only the matching records. The current
mesh-only runtime dump is name-matched; a dump containing `source`, `type`, and
`pathId` is matched precisely.

`Export Prefab as EFF mod package` writes the editable package used by the
Blender add-on. When the selected Prefab contains supported BeyondDynamicBone
components, the same export also writes `physics/components.json` plus the
original component bytes, TypeTree schemas and decoded fields under
`physics/`. This source graph is optional authoring data and is deliberately
absent from `mod.ini`; Blender imports it only when **Import physics bones and
colliders** is enabled.

## Build

Build from the AnimeStudio submodule. Generated `bin`, `obj`, and `dist` files are
ignored by its own repository. The local Windows build targets .NET 9; the
project also targets .NET 10, which requires a matching SDK:

```powershell
dotnet restore tools\AnimeStudio\AnimeStudio.GUI\AnimeStudio.GUI.csproj `
  -p:TargetFrameworks=net9.0-windows
dotnet build tools\AnimeStudio\AnimeStudio.GUI\AnimeStudio.GUI.csproj `
  -c Release -p:TargetFrameworks=net9.0-windows --no-restore -t:Rebuild
```

The three repositories have separate working trees and commit histories. Commit
tool changes in their submodules first, then update the EFF gitlinks. Publish
each product from its own repository: the EFF release contains only the DLL
installation ZIP; Blender and AnimeStudio packages belong to their tool releases.

Local analysis captures, one-off diagnostic scripts and old binary backups are
ignored by the root repository. Runtime source and reproducible regression-test
helpers remain tracked; diagnostic output must not be added to release packages.
