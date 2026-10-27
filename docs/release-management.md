# Release Management

EFF, EFF Blender, AnimeStudio and EFF Mod Manager have separate repositories, versions and Release workflows. EFF is the source of shared native authoring tools; the other applications consume its immutable Release assets when needed.

## Ownership

- EFF builds the game DLL and its protocol declaration. The same workflow builds `eff_resource_pack.exe`, `eff_source_index.exe`, `eff_author_core.dll`, their Python dependency, and `package-protocol.json` into `EFF_Shared_Native_Tools_<tag>.zip`. It publishes `eff-native-tools.json` with the archive's URL, size and SHA256.
- EFF Blender packages Python authoring code and format codecs. It does not ship another compiler, index helper, author-core DLL or full source index. Its CI uses the framework tag named in the latest published EFF shared-tools manifest.
- AnimeStudio builds its index in-process and registers the user-selected workspace. The index remains in that workspace and is not copied into Blender or Mod Manager.
- EFF Mod Manager uses the shared compiler for offline Mod repair and caches it under the user's resource root.

## Release Flow

1. Review and commit each repository's intended source in that repository. Record exact commits and verify the EFF submodule gitlinks before a coordinated release. Do not reset or clean unrelated local work.
2. Build the EFF tag. The workflow checks the DLL protocol against the shared compiler, verifies the tools archive and manifest, runs DLL and index regressions, then creates a draft containing the DLL ZIP, shared-tools ZIP and manifest. Publish the EFF release before consumers need its new tool protocol.
3. Build AnimeStudio independently when its source changes. Run the index and transactional source-update suites, then verify both supported framework packages.
4. Build EFF Blender independently when its source changes. Its CI reads the latest published EFF tools manifest, checks out that EFF tag for author-contract tests, and rejects packages containing native executables or libraries. It targets Blender 5.2.
5. Build EFF Mod Manager independently when its source changes. Run the .NET 9 Windows build and file-operation/shared-settings tests; confirm its package does not contain a private compiler copy.
6. Record each Release URL, asset name, size, SHA256 and CI run. A local deployment is separate evidence and does not publish a GitHub Release.

If only Blender UI or AnimeStudio UI changes, the shared-tools archive need not be rebuilt. If the compiler, index helper, author core, dependency ABI or package protocol changes, publish the compatible EFF shared-tools archive first. A consumer must retain its previously verified cache when a new download fails.

## Local Preflight

Run from the EFF repository and inspect every worktree independently:

```powershell
git status --short --branch
git -C tools/Blender status --short --branch
git -C tools/AnimeStudio status --short --branch
git -C EFF-Mod-Manager status --short --branch
git submodule status
```

Build the production DLL before verifying the native author tools:

```powershell
cmd /c build.bat
cmd /c tools\diagnostics\build_author_core.cmd
cmd /c tools\build_resource_pack.cmd
cmd /c tools\build_source_index.cmd
python -m unittest discover -s tools/release -v
```

The DLL and tools are local build outputs until the Release workflow creates and publishes the exact tagged assets.
