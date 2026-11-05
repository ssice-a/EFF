[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v\d+\.\d+\.\d+$')]
    [string]$Tag,
    [switch]$SkipDeployment
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$runId = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$finalRoot = Join-Path $repoRoot "bin\local-releases\$Tag"
$stageRoot = Join-Path $repoRoot "bin\.release-preparation-$Tag-$runId"
$assets = Join-Path $stageRoot 'assets'
$reportRoot = Join-Path $repoRoot 'bin\local-release-receipts'
$transcript = Join-Path $reportRoot "$Tag-$runId.log"
New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null
Start-Transcript -LiteralPath $transcript | Out-Null

function Invoke-Checked([string]$File, [string[]]$Arguments, [string]$Name, [string]$WorkingDirectory = $repoRoot) {
    Write-Host "==> $Name" -ForegroundColor Cyan
    Push-Location $WorkingDirectory
    try {
        & $File @Arguments
        if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}

function Assert-CleanGitTree([string]$Path, [string]$Name) {
    $status = @(& git -C $Path status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $Name worktree" }
    if ($status.Count -gt 0) { throw "$Name has uncommitted changes; commit the local source before preparing a release.`n$($status -join "`n")" }
}

function Get-Revision([string]$Path) {
    $revision = (& git -C $Path rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $revision -notmatch '^[0-9a-f]{40}$') { throw "Cannot read source revision: $Path" }
    return $revision
}

function Invoke-BuiltSmoke([string]$BuildScript, [string]$Binary, [string[]]$Arguments = @()) {
    Invoke-Checked 'cmd.exe' @('/d', '/c', $BuildScript) "Build $([IO.Path]::GetFileName($Binary))"
    $binaryPath = if ([IO.Path]::IsPathRooted($Binary)) { $Binary } else { Join-Path $repoRoot $Binary }
    Invoke-Checked $binaryPath $Arguments "Run $([IO.Path]::GetFileName($Binary))"
}

function Get-AssetReceipt([string]$Path) {
    return [ordered]@{
        name = [IO.Path]::GetFileName($Path)
        bytes = (Get-Item -LiteralPath $Path).Length
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

try {
    if (Test-Path -LiteralPath $finalRoot) { throw "Local release directory already exists: $finalRoot" }
    $header = Get-Content -LiteralPath (Join-Path $repoRoot 'src\eiem_version.h') -Raw
    $versionParts = foreach ($part in 'MAJOR', 'MINOR', 'PATCH') {
        $match = [regex]::Match($header, "(?m)^#define EIEM_VERSION_$part\s+(\d+)\s*$")
        if (-not $match.Success) { throw "Missing EIEM_VERSION_$part in src/eiem_version.h" }
        $match.Groups[1].Value
    }
    $sourceVersion = 'v' + ($versionParts -join '.')
    if ($Tag -ne $sourceVersion) { throw "Requested $Tag does not match local DLL source $sourceVersion" }

    $rootRevision = Get-Revision $repoRoot
    $tagRevision = (& git -C $repoRoot rev-parse "$Tag^{commit}").Trim()
    if ($LASTEXITCODE -ne 0 -or $tagRevision -ne $rootRevision) { throw "Local tag $Tag must point at local HEAD ($rootRevision)" }
    Assert-CleanGitTree $repoRoot 'EFF'
    foreach ($path in @('tools/Blender', 'tools/AnimeStudio', 'EFF-Mod-Manager')) {
        $fullPath = Join-Path $repoRoot $path
        Assert-CleanGitTree $fullPath $path
        $line = (& git -C $repoRoot ls-tree HEAD $path).Trim()
        if ($LASTEXITCODE -ne 0 -or $line -notmatch ('\s' + [regex]::Escape($path) + '$')) { throw "Missing committed submodule gitlink: $path" }
        $gitlink = ($line -split '\s+')[2]
        if ((Get-Revision $fullPath) -ne $gitlink) { throw "$path checkout does not match the EFF commit's pinned gitlink" }
    }

    if (-not $SkipDeployment) {
        $gamePlugin = 'D:\Hypergryph Launcher\games\Endfield Game\plugin'
        if (-not (Test-Path -LiteralPath $gamePlugin -PathType Container)) { throw "Game plugin directory not found: $gamePlugin" }
        $gameRoot = [IO.Path]::GetFullPath((Split-Path -Parent $gamePlugin)).TrimEnd('\') + '\'
        $gameProcesses = @(Get-Process -Name @('NTEGame', 'Endfield', 'EndfieldGame') -ErrorAction SilentlyContinue | Where-Object {
            $processPath = $null
            try { $processPath = $_.Path } catch { }
            if (-not $processPath) { return $true }
            return [IO.Path]::GetFullPath($processPath).StartsWith($gameRoot, [StringComparison]::OrdinalIgnoreCase)
        })
        if ($gameProcesses.Count -gt 0) {
            throw "Close the installed Endfield Game process normally before local DLL deployment. Running process: $($gameProcesses.ProcessName -join ', ')"
        }
    }

    New-Item -ItemType Directory -Force -Path $assets | Out-Null
    $frameworkCommits = [ordered]@{}
    foreach ($path in @('tools/Blender', 'tools/AnimeStudio', 'EFF-Mod-Manager')) {
        $frameworkCommits[$path] = Get-Revision (Join-Path $repoRoot $path)
    }

    Invoke-Checked 'cmd.exe' @('/d', '/c', 'build.bat') 'Production Format 3 DLL and proxy build'

    $pythonVersion = (& python -c 'import sys; print(str(sys.version_info.major) + chr(46) + str(sys.version_info.minor))' | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $pythonVersion -ne '3.11') { throw "Release tool packaging requires Python 3.11; found $pythonVersion" }
    Invoke-Checked 'python' @('-m', 'pip', 'install', '--disable-pip-version-check', 'PyInstaller==6.21.0', 'Pillow==12.3.0', 'numpy==2.3.5') 'Pinned migration packager dependencies'

    $legacyRoot = Join-Path $stageRoot 'legacy-migrator'
    $legacyDist = Join-Path $legacyRoot 'dist'
    $legacyWork = Join-Path $legacyRoot 'build'
    $legacySpec = Join-Path $legacyRoot 'spec'
    $blenderTools = Join-Path $repoRoot 'tools\Blender'
    $nativeExport = Join-Path $blenderTools 'eiem_native_export.py'
    $formatModule = Join-Path $blenderTools 'eiem_format.py'
    $effectDocument = Join-Path $blenderTools 'eiem_effect_document.py'
    $sharedResources = Join-Path $blenderTools 'eiem_shared_resources.py'
    $textureCompression = Join-Path $blenderTools 'eiem_texture_compression.py'
    $pyInstallerArgs = @(
        '-m', 'PyInstaller', '--clean', '--noconfirm', '--onefile', '--console', '--name', 'eff_migrate_mods',
        '--paths', '.', '--paths', $blenderTools,
        '--add-data', "$nativeExport;tools/Blender",
        '--add-data', "$formatModule;tools/Blender",
        '--add-data', "$effectDocument;tools/Blender",
        '--add-data', "$sharedResources;tools/Blender",
        '--add-data', "$textureCompression;tools/Blender",
        '--collect-all', 'PIL', '--hidden-import', 'eiem_native_export',
        '--hidden-import', 'eiem_effect_document',
        '--hidden-import', 'eiem_texture_compression', '--hidden-import', 'eiem_shared_resources',
        '--hidden-import', 'eiem_format', '--distpath', $legacyDist, '--workpath', $legacyWork,
        '--specpath', $legacySpec, 'tools/nativepack/migrate_launcher.py'
    )
    Invoke-Checked 'python' $pyInstallerArgs 'Standalone legacy Mod migrator build'
    $migrator = Join-Path $legacyDist 'eff_migrate_mods.exe'
    Invoke-Checked $migrator @('--help') 'Standalone migrator startup check'

    $sourceExporter = Join-Path $stageRoot 'legacy-source-exporter'
    Invoke-Checked 'dotnet' @('publish', 'tools/AnimeStudio/AnimeStudio.GUI/AnimeStudio.GUI.csproj', '-c', 'Release', '-f', 'net9.0-windows', '-r', 'win-x64', '--self-contained', 'false', '-p:TargetFrameworks=net9.0-windows', '-o', $sourceExporter) 'AnimeStudio migration source exporter build'
    Copy-Item -LiteralPath (Join-Path $sourceExporter 'AnimeStudio.GUI.exe') -Destination (Join-Path $sourceExporter 'eff_migrate_sources.exe') -Force
    foreach ($name in @('eff_migrate_sources.exe', 'AnimeStudio.GUI.dll', 'AnimeStudio.GUI.deps.json', 'AnimeStudio.GUI.runtimeconfig.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $sourceExporter $name) -PathType Leaf)) { throw "Incomplete migration source exporter payload: $name" }
    }

    $wheelRoot = Join-Path $repoRoot 'bin\pillow-wheels'
    New-Item -ItemType Directory -Force -Path $wheelRoot | Out-Null
    Invoke-Checked 'python' @('-m', 'pip', 'download', '--only-binary=:all:', '--no-deps', '--platform', 'win_amd64', '--implementation', 'cp', '--python-version', '3.11', '--abi', 'cp311', 'Pillow==12.3.0', '--dest', $wheelRoot) 'Blender 5.0 Pillow wheel download'
    Invoke-Checked 'python' @('-m', 'pip', 'download', '--only-binary=:all:', '--no-deps', '--platform', 'win_amd64', '--implementation', 'cp', '--python-version', '3.13', '--abi', 'cp313', 'Pillow==12.3.0', '--dest', $wheelRoot) 'Blender 5.2 Pillow wheel download'
    $cp311 = Get-ChildItem -LiteralPath $wheelRoot -Filter 'Pillow-12.3.0-cp311-cp311-win_amd64.whl' | Select-Object -First 1 -ExpandProperty FullName
    $cp313 = Get-ChildItem -LiteralPath $wheelRoot -Filter 'pillow-12.3.0-cp313-cp313-win_amd64.whl' | Select-Object -First 1 -ExpandProperty FullName
    if (-not $cp313) { $cp313 = Get-ChildItem -LiteralPath $wheelRoot -Filter 'Pillow-12.3.0-cp313-cp313-win_amd64.whl' | Select-Object -First 1 -ExpandProperty FullName }
    if (-not $cp311 -or -not $cp313) { throw 'Pillow wheels for Blender 5.0 and 5.2 are required' }
    Invoke-Checked 'python' @('tools/diagnostics/prepare_pillow_vendor.py', '--cp311-wheel', $cp311, '--cp313-wheel', $cp313, '--destination', 'bin/author-core/vendor') 'Prepare dual-ABI Pillow runtime'
    Invoke-Checked 'python' @('tools/diagnostics/prepare_texconv.py') 'Prepare pinned DirectXTex runtime'
    Invoke-Checked 'cmd.exe' @('/d', '/c', 'tools\diagnostics\build_author_core.cmd') 'Author core build'
    Invoke-Checked 'cmd.exe' @('/d', '/c', 'tools\build_resource_pack.cmd') 'Resource compiler build'
    Invoke-Checked 'cmd.exe' @('/d', '/c', 'tools\build_source_index.cmd') 'Source index helper build'
    Invoke-Checked 'cmd.exe' @('/d', '/c', 'tools\diagnostics\build_author_assets.cmd format3') 'Format 3 author-assets DLL build'
    Invoke-Checked 'python' @('tools/diagnostics/author_assets_abi_smoke.py', 'bin/author-assets/eff_author_assets_format3.dll') 'Format 3 author-assets ABI check'

    Invoke-Checked 'python' @('tools/diagnostics/verify_production_dll.py', 'bin/resource-directory-entry-format3/eff.dll') 'Production DLL isolation check'
    Invoke-BuiltSmoke 'tools\diagnostics\build_model_pool_discard_smoke.cmd' 'bin/model-pool-discard/smoke.exe'
    Invoke-Checked 'dotnet' @('run', '--project', 'tools/AnimeStudio/AnimeStudio.IndexTests/AnimeStudio.IndexTests.csproj', '-c', 'Release', '-p:TargetFrameworks=net9.0-windows') 'Public index and BLC regression suite'
    Invoke-Checked 'dotnet' @('run', '--project', 'tools/AnimeStudio/AnimeStudio.UpdateTests/AnimeStudio.UpdateTests.csproj', '-c', 'Release', '-p:TargetFrameworks=net9.0-windows') 'AnimeStudio source update regression suite'
    Invoke-Checked 'python' @('-m', 'unittest', 'discover', '-s', 'tools/release', '-v') 'Release packaging regression suite'
    Invoke-BuiltSmoke 'tools\diagnostics\build_full_resource_reload_smoke.cmd' 'bin/resource-directory-entry-v35/full_resource_reload_smoke.exe'
    Invoke-BuiltSmoke 'tools\diagnostics\build_runtime_author_manifest_smoke.cmd' 'bin/runtime-author-manifest/runtime_author_manifest_smoke.exe'
    Invoke-Checked 'python' @('tools/Blender/test_author_format3.py') 'Logical Mesh route export and publication regressions'
    Invoke-Checked 'cmd.exe' @('/d', '/c', 'tools\diagnostics\build_demand_generation_diff_smoke.cmd') 'Demand generation diff regression build and run'
    Invoke-Checked 'cmd.exe' @('/d', '/c', 'tools\diagnostics\build_demand_resource_regressions.cmd') 'Demand resource regression build'
    Invoke-Checked 'cmd.exe' @('/d', '/c', 'tools\diagnostics\build_native_random_access_smoke.cmd') 'Demand native stream and live author replacement regressions'
    Invoke-BuiltSmoke 'tools\diagnostics\build_runtime_shape_bridge_smoke.cmd' 'bin/resource-directory-entry/runtime_shape_bridge_smoke.exe'
    Invoke-BuiltSmoke 'tools\diagnostics\build_production_dll_load_smoke.cmd' 'bin/production-dll-load/production_dll_load_smoke.exe' @((Resolve-Path 'bin/resource-directory-entry-format3/eff.dll').Path, "$env:WINDIR\System32")
    Invoke-BuiltSmoke 'tools\diagnostics\build_compiled_mod_set_smoke.cmd' 'bin/compiled-mod-set/compiled_mod_set_smoke.exe'
    Invoke-Checked 'cmd.exe' @('/d', '/c', 'tools\diagnostics\build_ui_page_bind_smoke.cmd') 'Game UI page binding regression build'
    Invoke-BuiltSmoke 'tools\diagnostics\build_control_panel_smoke.cmd' 'bin/resource-directory-entry/control_panel_smoke.exe'
    Invoke-Checked 'cmd.exe' @('/d', '/c', 'tools\diagnostics\build_static_mesh_assembly_smoke.cmd') 'Static Mesh assembly regression build and run'

    $nativeArchive = Join-Path $assets "EFF_Shared_Native_Tools_$Tag.zip"
    $manifest = Join-Path $assets 'eff-native-tools.json'
    Invoke-Checked 'python' @('tools/diagnostics/package_shared_native_tools.py', '--output', $assets, '--framework-tag', $Tag, '--manifest-output', $manifest, '--source-exporter', $sourceExporter, '--migrator', $migrator) 'Shared native tools package'
    Invoke-Checked 'python' @('tools/release/package_runtime.py', '--tag', $Tag, '--output', $assets) 'Clean game runtime package'
    Invoke-Checked 'python' @('tools/release/verify_release_assets.py', '--directory', $assets, '--tag', $Tag) 'Final release asset and source exclusion verification'

    $assetReceipts = @(
        (Get-AssetReceipt (Join-Path $assets "EFF_${Tag}_dll.zip")),
        (Get-AssetReceipt $nativeArchive),
        (Get-AssetReceipt $manifest)
    )
    $receipt = [ordered]@{
        format = 'EFF_LOCAL_RELEASE_PREPARATION/1'
        tag = $Tag
        sourceCommit = $rootRevision
        submodules = $frameworkCommits
        preparedAt = (Get-Date).ToString('o')
        assets = $assetReceipts
        tests = 'Completed once locally before packaging; upload script does not build or run tests.'
        transcript = $transcript
        deploymentSkipped = [bool]$SkipDeployment
    }
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $stageRoot 'preparation.json') -Encoding UTF8

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $finalRoot) | Out-Null
    Move-Item -LiteralPath $stageRoot -Destination $finalRoot
    if (-not $SkipDeployment) {
        & (Join-Path $PSScriptRoot 'build_deploy_local.ps1') -Component Dll -UseExistingDllBuild
        if (-not $?) { throw 'Local DLL deployment failed after release preparation' }
        if ($LASTEXITCODE -ne 0) { throw "Local DLL deployment failed with exit code $LASTEXITCODE" }
    }
    Write-Host "Local release assets are ready: $(Join-Path $finalRoot 'assets')" -ForegroundColor Green
    Write-Host "Local preparation receipt: $(Join-Path $finalRoot 'preparation.json')" -ForegroundColor Green
} finally {
    Stop-Transcript | Out-Null
}
