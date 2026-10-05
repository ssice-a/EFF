$ErrorActionPreference = 'Stop'
$taskWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$taskGame = 'D:\Hypergryph Launcher\games\Endfield Game'
$taskPlugin = Join-Path $taskGame 'plugin'
$taskAuthor = Join-Path $taskPlugin 'mods\LZY'
$taskEvidence = Join-Path $taskWorkspace 'diagnostics\source-free-textures-20261005\v35'
$taskPrepared = Join-Path $taskEvidence 'migrated\LZY'
$taskBuild = 'resource-directory-entry-v35-blender-texture-resources'
$taskRollback = Join-Path $taskPlugin ('resource-input-diagnostics\rollback\v35-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
function Digest($path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
    $running = Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^Endfield.*\.exe$|^eff_resource_pack\.exe$' -or
        ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($taskGame + '\', [StringComparison]::OrdinalIgnoreCase))
    }
    if ($running) { throw 'Game or resource compiler is running.' }
}
function ReplaceFile($source, $target) {
    AssertStopped
    $temporary = $target + '.v35-staging'
    if (Test-Path -LiteralPath $temporary) { throw "Unexpected staging file: $temporary" }
    Copy-Item -LiteralPath $source -Destination $temporary
    if ((Digest $temporary) -ne (Digest $source)) { throw 'Staging checksum differs.' }
    AssertStopped
    $backupFolder = if ([IO.Path]::GetPathRoot($target) -eq [IO.Path]::GetPathRoot($taskRollback)) { $taskRollback } else { $taskEvidence }
    $atomicBackup = Join-Path $backupFolder ('atomic-' + [guid]::NewGuid().ToString() + '.before')
    [IO.File]::Replace($temporary, $target, $atomicBackup, $false)
    if ((Digest $target) -ne (Digest $source)) { throw 'Installed checksum differs.' }
}
AssertStopped
$proof = Get-Content -LiteralPath (Join-Path $taskEvidence 'verification.json') -Raw | ConvertFrom-Json
$blenderProof = Get-Content -LiteralPath (Join-Path $taskWorkspace 'diagnostics\blender-partial-export-20261005\source-free-v35\verification.json') -Raw | ConvertFrom-Json
if (-not $proof.success -or -not $proof.productionUnchanged -or -not $blenderProof.success -or -not $blenderProof.productionUnchanged) { throw 'Resource regression failed.' }
$dll = Join-Path $taskWorkspace 'bin\resource-directory-entry\eff.dll'
$compiler = Join-Path $taskWorkspace 'bin\resource-pack\eff_resource_pack.exe'
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($dll)).Contains($taskBuild)) { throw 'DLL build marker differs.' }
if ((Digest (Join-Path $taskPlugin 'eff.dll')) -ne 'B953062AA9BB77C3F2259D2D665041A9BA1E9AEBE283B9C218C496E12EBFD521') { throw 'Installed DLL differs from the recorded baseline.' }
foreach ($name in @('ini_package.py','ini_reader.py','texture_fields.py')) {
    if ((Digest (Join-Path $taskWorkspace "tools\nativepack\$name")) -ne (Digest (Join-Path $taskWorkspace "tools\Blender\nativepack\$name"))) { throw 'Blender module differs.' }
}
$protected = [ordered]@{}
foreach ($relative in @('eff.ini','resource-input-substitutions.tsv','resource-reload-baseline\baseline.tsv','resource-reload-baseline\assembly.sha256','resource-reload-baseline\source-ranges.tsv')) {
    $protected[$relative] = Digest (Join-Path $taskPlugin $relative)
}
$proxySHA = Digest (Join-Path $taskGame 'vulkan-1.dll')
if (-not ((Get-Content -LiteralPath (Join-Path $taskPlugin 'eff.ini') -Raw) -match '(?m)^\s*disable_camera_fade\s*=\s*true\s*$')) { throw 'Anti-fade is disabled.' }
foreach ($flag in @('gfxtrace-capture.enabled','native-candidate-gfxtrace.enabled','full-reload-gfxtrace.enabled','runtime-shapes-gfxtrace.enabled')) {
    if (Test-Path -LiteralPath (Join-Path $taskPlugin "resource-input-diagnostics\$flag")) { throw 'GFX flag is enabled.' }
}
$authorBefore = [ordered]@{}
foreach ($file in Get-ChildItem -LiteralPath $taskAuthor -Recurse -File) {
    $relative = $file.FullName.Substring($taskAuthor.Length + 1)
    $authorBefore[$relative] = Digest $file.FullName
    if ($relative -ne 'mod.ini' -and -not $relative.EndsWith('.tex')) {
        if ((Digest (Join-Path $taskPrepared $relative)) -ne $authorBefore[$relative]) { throw "Fixture differs: $relative" }
    }
}
$pairs = [System.Collections.Generic.List[object]]::new()
$pairs.Add(@($dll, (Join-Path $taskPlugin 'eff.dll'), 'eff.before.dll'))
$pairs.Add(@($compiler, (Join-Path $taskPlugin 'tools\eff_resource_pack.exe'), 'game-compiler.before.exe'))
$pairs.Add(@($compiler, (Join-Path $taskWorkspace 'tools\Blender\nativepack\eff_resource_pack.exe'), 'blender-compiler.before.exe'))
foreach ($relative in $authorBefore.Keys) {
    if ($relative -eq 'mod.ini' -or $relative.EndsWith('.tex')) {
        $pairs.Add(@((Join-Path $taskPrepared $relative), (Join-Path $taskAuthor $relative), ('author\' + $relative)))
    }
}
New-Item -ItemType Directory -Path $taskRollback | Out-Null
foreach ($pair in $pairs) {
    $backup = Join-Path $taskRollback $pair[2]
    New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
    Copy-Item -LiteralPath $pair[1] -Destination $backup
}
$current = Join-Path $taskPlugin 'resource-reload-cache\current.tsv'
Copy-Item -LiteralPath $current -Destination (Join-Path $taskRollback 'current.before.tsv')
$installed = [System.Collections.Generic.List[object]]::new()
try {
    AssertStopped
    & $compiler $taskPrepared (Join-Path $taskPlugin 'resource-reload-baseline') (Join-Path $taskPlugin 'resource-reload-cache') (Join-Path $taskGame 'GameAssembly.dll') *> (Join-Path $taskEvidence 'compile-production-path.log')
    if ($LASTEXITCODE -ne 0) { throw 'Native package compilation failed.' }
    foreach ($relative in $authorBefore.Keys) { if ((Digest (Join-Path $taskAuthor $relative)) -ne $authorBefore[$relative]) { throw 'Author changed during preparation.' } }
    foreach ($pair in $pairs) { ReplaceFile $pair[0] $pair[1]; $installed.Add($pair) }
    & (Join-Path $taskWorkspace 'bin\resource-directory-entry\offline_loader_smoke.exe') $taskGame *> (Join-Path $taskEvidence 'installed-loader.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Installed startup/F10 disk validation failed.' }
    $key = ((Get-Content -LiteralPath $current)[1] -split "`t")[0]
    & (Join-Path $taskWorkspace 'bin\resource-pack\inspect_payloads.exe') (Join-Path $taskPlugin "resource-reload-cache\generations\$key") (Join-Path $taskPlugin 'mods') (Join-Path $taskPlugin 'resource-reload-baseline') *> (Join-Path $taskEvidence 'installed-native.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Installed native payload validation failed.' }
    foreach ($relative in $protected.Keys) { if ((Digest (Join-Path $taskPlugin $relative)) -ne $protected[$relative]) { throw "Protected configuration changed: $relative" } }
    if ((Digest (Join-Path $taskGame 'vulkan-1.dll')) -ne $proxySHA) { throw 'Proxy changed.' }
    foreach ($relative in $authorBefore.Keys) {
        if ($relative -ne 'mod.ini' -and -not $relative.EndsWith('.tex') -and (Digest (Join-Path $taskAuthor $relative)) -ne $authorBefore[$relative]) { throw "Protected author file changed: $relative" }
    }
    AssertStopped
    [ordered]@{
        build=$taskBuild;installed=$true;compilerABI='EFF_NATIVE_PACK_V35_1';key=$key
        dllSHA256=(Digest (Join-Path $taskPlugin 'eff.dll'));compilerSHA256=(Digest $compiler)
        rollback=$taskRollback;sourceFreeTextureINI=$true;selfDescribingTEX=$true
        gfxEnabled=$false;antiFadeEnabled=$true;visualAcceptance=$false
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskEvidence 'deployment.json') -Encoding utf8
    Write-Output "PASS deployed $taskBuild key=$key rollback=$taskRollback"
} catch {
    $reason = $_
    for ($index = $installed.Count - 1; $index -ge 0; --$index) {
        $pair = $installed[$index]
        ReplaceFile (Join-Path $taskRollback $pair[2]) $pair[1]
    }
    ReplaceFile (Join-Path $taskRollback 'current.before.tsv') $current
    throw $reason
}
