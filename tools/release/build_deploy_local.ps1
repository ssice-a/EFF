[CmdletBinding()]
param(
    [ValidateSet('Dll', 'Blender', 'AnimeStudio', 'Manager', 'All')]
    [string]$Component = 'Dll',
    [switch]$UseExistingDllBuild,
    [switch]$SkipAnimeStudioTests
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$runId = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$runRoot = Join-Path $repoRoot "bin\local-deployments\$runId"
New-Item -ItemType Directory -Force -Path $runRoot | Out-Null

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

function Get-Revision([string]$Path) {
    $revision = (& git -C $Path rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $revision -notmatch '^[0-9a-f]{40}$') {
        throw "Cannot read source revision for $Path"
    }
    return $revision
}

function Get-FileDigest([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-ProcessClosed([string[]]$Names, [string]$Purpose) {
    $running = @(Get-Process -Name $Names -ErrorAction SilentlyContinue)
    if ($running.Count -gt 0) {
        $details = ($running | ForEach-Object { "$($_.ProcessName) (PID $($_.Id))" }) -join ', '
        throw "$Purpose is in use: $details. Close it normally, then rerun; this script never terminates applications."
    }
}

function Assert-GameClosed([string]$PluginDirectory) {
    $gameRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PluginDirectory)).TrimEnd('\') + '\'
    $running = @(Get-Process -Name @('NTEGame', 'Endfield', 'EndfieldGame') -ErrorAction SilentlyContinue | Where-Object {
        $processPath = $null
        try { $processPath = $_.Path } catch { }
        # An elevated/protected game can hide Path. Treat a matching process
        # name as running rather than replacing its DLL under that process.
        if (-not $processPath) { return $true }
        $fullProcessPath = [IO.Path]::GetFullPath($processPath)
        return $fullProcessPath.StartsWith($gameRoot, [StringComparison]::OrdinalIgnoreCase)
    })
    if ($running.Count -gt 0) {
        $details = ($running | ForEach-Object { "$($_.ProcessName) (PID $($_.Id), $($_.Path))" }) -join ', '
        throw "Game DLL deployment requires the Endfield Game process to exit normally: $details"
    }
}

function New-Receipt([string]$Name, [string]$Revision, [string]$Target, [string]$Backup, [string[]]$Files, [hashtable]$Details = @{}) {
    $entries = foreach ($file in $Files) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Deployment output is missing: $file" }
        [ordered]@{ path = $file; bytes = (Get-Item -LiteralPath $file).Length; sha256 = Get-FileDigest $file }
    }
    $receipt = [ordered]@{
        format = 'EFF_LOCAL_DEPLOYMENT/1'
        component = $Name
        revision = $Revision
        target = $Target
        backup = $Backup
        createdAt = (Get-Date).ToString('o')
        files = @($entries)
        details = $Details
    }
    $path = Join-Path $runRoot "$Name.json"
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
    Write-Host "Installed $Name; receipt: $path" -ForegroundColor Green
}

function Deploy-Dll([string]$BuildRoot, [string]$Revision) {
    $plugin = 'D:\Hypergryph Launcher\games\Endfield Game\plugin'
    $gameRoot = Split-Path -Parent $plugin
    if (-not (Test-Path -LiteralPath $plugin -PathType Container)) { throw "Game plugin directory not found: $plugin" }
    Assert-GameClosed $plugin

    $sources = @(
        (Join-Path $BuildRoot 'eff.dll'),
        (Join-Path $BuildRoot 'package-protocol.json')
    )
    foreach ($source in $sources) {
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Built DLL payload is missing: $source" }
    }

    $backupRoot = Join-Path $plugin '.eff-deployment-backups'
    $backup = Join-Path $backupRoot "local-$runId"
    New-Item -ItemType Directory -Force -Path $backup | Out-Null
    $installed = @()
    $newNames = @()
    try {
        foreach ($source in $sources) {
            $name = Split-Path -Leaf $source
            $destination = Join-Path $plugin $name
            if (Test-Path -LiteralPath $destination -PathType Leaf) {
                Copy-Item -LiteralPath $destination -Destination (Join-Path $backup $name)
            }
            $temporary = "$destination.$runId.tmp"
            Copy-Item -LiteralPath $source -Destination $temporary
            Move-Item -LiteralPath $temporary -Destination $destination -Force
            if ((Get-FileDigest $source) -ne (Get-FileDigest $destination)) { throw "Installed $name does not match the build output" }
            $installed += $destination
            $newNames += $name
        }
    } catch {
        foreach ($name in $newNames) {
            $destination = Join-Path $plugin $name
            $saved = Join-Path $backup $name
            if (Test-Path -LiteralPath $saved -PathType Leaf) {
                Copy-Item -LiteralPath $saved -Destination $destination -Force
            } else {
                Remove-Item -LiteralPath $destination -Force -ErrorAction SilentlyContinue
            }
        }
        throw
    }
    New-Receipt 'Dll' $Revision $plugin $backup $installed @{ buildDirectory = $BuildRoot; preservedUserIni = $true }
}

function Publish-Directory([string]$Candidate, [string]$Target, [string]$Name, [string[]]$PreserveFiles = @()) {
    $target = [IO.Path]::GetFullPath($Target)
    $candidate = [IO.Path]::GetFullPath($Candidate)
    $candidateRoot = [IO.Path]::GetPathRoot($candidate)
    if ($candidateRoot -ne [IO.Path]::GetPathRoot($target)) { throw "$Name staging and target must be on the same volume" }
    if (-not (Test-Path -LiteralPath $candidate -PathType Container)) { throw "$Name staging directory not found: $candidate" }
    if (Test-Path -LiteralPath $target) {
        $targetItem = Get-Item -LiteralPath $target -Force
        if (-not $targetItem.PSIsContainer -or ($targetItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to replace a non-directory or linked $Name target: $target"
        }
        foreach ($relative in $PreserveFiles) {
            $old = Join-Path $target $relative
            $new = Join-Path $candidate $relative
            if ((Test-Path -LiteralPath $old -PathType Leaf) -and -not (Test-Path -LiteralPath $new -PathType Leaf)) {
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $new) | Out-Null
                Copy-Item -LiteralPath $old -Destination $new
            } elseif ((Test-Path -LiteralPath $old -PathType Leaf) -and (Test-Path -LiteralPath $new -PathType Leaf)) {
                Copy-Item -LiteralPath $old -Destination $new -Force
            }
        }
    }

    $parent = Split-Path -Parent $target
    $backupRoot = Join-Path $parent '.eff-local-deployment-backups'
    New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
    $backup = Join-Path $backupRoot "$Name-$runId"
    $hadTarget = Test-Path -LiteralPath $target -PathType Container
    if ($hadTarget) { Move-Item -LiteralPath $target -Destination $backup }
    try {
        Move-Item -LiteralPath $candidate -Destination $target
    } catch {
        if ($hadTarget -and -not (Test-Path -LiteralPath $target)) { Move-Item -LiteralPath $backup -Destination $target }
        throw
    }
    return $backup
}

function Build-Dll {
    $buildRoot = Join-Path $repoRoot 'bin\resource-directory-entry-format3'
    if (-not $UseExistingDllBuild) {
        Invoke-Checked 'cmd.exe' @('/d', '/c', 'build.bat') 'Production Format 3 DLL build'
    }
    Invoke-Checked 'python' @('tools/diagnostics/verify_production_dll.py', (Join-Path $buildRoot 'eff.dll')) 'Production DLL verification'
    Deploy-Dll $buildRoot (Get-Revision $repoRoot)
}

function Build-Blender {
    $source = Join-Path $repoRoot 'tools\Blender'
    $target = Join-Path ([Environment]::GetFolderPath('ApplicationData')) 'Blender Foundation\Blender\5.2\scripts\addons\EFF_Blender'
    if (-not (Test-Path -LiteralPath $target -PathType Container)) { throw "Blender 5.2 add-on directory not found: $target" }
    if ((Get-Item -LiteralPath $target -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Blender add-on target is linked; refusing deployment: $target" }

    $revision = Get-Revision $source
    $output = Join-Path $runRoot 'blender-build'
    Invoke-Checked 'python' @('-m', 'compileall', '-q', '.') 'Blender Python syntax check' $source
    Invoke-Checked 'python' @('ci/package_update.py', '--output', $output, '--revision', $revision, '--framework-root', $repoRoot) 'Blender continuous package build' $source
    $oldPythonPath = $env:PYTHONPATH
    try {
        $env:PYTHONPATH = @($source, (Join-Path $repoRoot 'tools')) -join [IO.Path]::PathSeparator
        Invoke-Checked 'python' @('-m', 'unittest', 'test_author_format3', 'test_eiem_bone_animation', 'test_eiem_native_export_core', 'test_eiem_native_publish', 'test_eiem_release', 'test_eiem_shape_animation_package', 'test_eiem_material_features', 'test_eiem_effect_clipboard', 'test_eiem_update') 'Blender author/update regression suite' $source
    } finally {
        $env:PYTHONPATH = $oldPythonPath
    }

    Assert-ProcessClosed @('blender') 'Blender add-on deployment'
    $package = Join-Path $output 'package\EFF_Blender'
    $installer = Join-Path $PSScriptRoot 'install_blender_package.py'
    $result = (& python $installer --package $package --target $target | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "Blender local installation failed with exit code $LASTEXITCODE" }
    $installResult = $result | ConvertFrom-Json
    New-Receipt 'Blender' $revision $target $installResult.backup @((Join-Path $target 'eff-build.json')) @{ version = ($installResult.revision); channel = 'continuous'; archive = (Join-Path $output ("EFF_Blender_main-$revision.zip")) }
}

function Build-AnimeStudio {
    $source = Join-Path $repoRoot 'tools\AnimeStudio'
    $target = 'E:\AnimeStudio-net10-1ccfbc16bf7fe625e8295bac8074ac3b1b9a065b'
    if (-not (Test-Path -LiteralPath $target -PathType Container)) { throw "AnimeStudio target directory not found: $target" }
    $revision = Get-Revision $source
    $installedSdks = @(& dotnet --list-sdks)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot query installed .NET SDKs' }
    if (-not ($installedSdks | Where-Object { $_ -match '^\s*9\.' })) {
        throw 'AnimeStudio local deployment requires the .NET 9 SDK.'
    }

    if (-not $SkipAnimeStudioTests) {
        Invoke-Checked 'dotnet' @('run', '--project', 'AnimeStudio.IndexTests/AnimeStudio.IndexTests.csproj', '-c', 'Release', '-p:TargetFrameworks=net9.0-windows') 'AnimeStudio index regression suite' $source
        Invoke-Checked 'dotnet' @('run', '--project', 'AnimeStudio.UpdateTests/AnimeStudio.UpdateTests.csproj', '-c', 'Release', '-p:TargetFrameworks=net9.0-windows') 'AnimeStudio transactional source-update suite' $source
    } else {
        Write-Host 'Skipping AnimeStudio regression tests because they already passed for this source revision.' -ForegroundColor Yellow
    }
    $buildOutput = Join-Path $runRoot 'AnimeStudio'
    Push-Location $source
    try {
        & (Join-Path $source 'build.ps1') -Frameworks @('net9.0-windows') -OutputRoot $buildOutput
        if (-not $?) { throw 'AnimeStudio net9 build failed' }
        if ($LASTEXITCODE -ne 0) { throw "AnimeStudio net9 build failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }

    Assert-ProcessClosed @('AnimeStudio.GUI', 'AnimeStudio.CLI') 'AnimeStudio deployment'
    $candidate = Join-Path $buildOutput 'net9.0-windows'
    $backup = Publish-Directory $candidate $target 'AnimeStudio' @('CNKeys.json', 'bin\log.txt', 'bin\log_prev.txt')
    $files = @((Join-Path $target 'AnimeStudio.GUI.exe'), (Join-Path $target 'AnimeStudio.CLI.exe'), (Join-Path $target 'bin\AnimeStudio.GUI.dll'), (Join-Path $target 'bin\AnimeStudio.CLI.dll'))
    New-Receipt 'AnimeStudio' $revision $target $backup $files @{ framework = 'net9.0-windows'; buildOutput = $candidate }
}

function Build-Manager {
    $source = Join-Path $repoRoot 'EFF-Mod-Manager'
    $project = Join-Path $source 'EFF.ModManager\EFF.ModManager.csproj'
    $target = 'E:\EFF-Mod-Manager'
    if (-not (Test-Path -LiteralPath $target -PathType Container)) { throw "Mod Manager target directory not found: $target" }
    $revision = Get-Revision $source

    Invoke-Checked 'dotnet' @('run', '--project', 'tests/EFF.ModManager.Tests/EFF.ModManager.Tests.csproj', '-c', 'Release') 'Mod Manager regression suite' $source
    $candidate = Join-Path $runRoot 'EFF-Mod-Manager'
    New-Item -ItemType Directory -Force -Path $candidate | Out-Null
    Invoke-Checked 'dotnet' @('publish', $project, '-c', 'Release', '-r', 'win-x64', '--self-contained', 'false', '-p:PublishSingleFile=true', '-o', $candidate) 'Mod Manager Windows publish' $repoRoot
    Get-ChildItem -LiteralPath $candidate -Filter '*.pdb' -File -Recurse | Remove-Item -Force

    Assert-ProcessClosed @('EFF.ModManager') 'Mod Manager deployment'
    $backup = Publish-Directory $candidate $target 'Mod-Manager'
    $files = @(Join-Path $target 'EFF.ModManager.exe')
    New-Receipt 'Manager' $revision $target $backup $files @{ runtime = 'win-x64'; selfContained = $false; singleFile = $true }
}

$requested = if ($Component -eq 'All') { @('Dll', 'Blender', 'AnimeStudio', 'Manager') } else { @($Component) }
if ($UseExistingDllBuild -and $Component -ne 'Dll') { throw '-UseExistingDllBuild is only valid with -Component Dll' }
if ($SkipAnimeStudioTests -and $Component -ne 'AnimeStudio') { throw '-SkipAnimeStudioTests is only valid when retrying the AnimeStudio component' }
foreach ($item in $requested) {
    switch ($item) {
        'Dll' { Build-Dll }
        'Blender' { Build-Blender }
        'AnimeStudio' { Build-AnimeStudio }
        'Manager' { Build-Manager }
    }
}

Write-Host "Local build/deploy complete. Receipts: $runRoot" -ForegroundColor Green
