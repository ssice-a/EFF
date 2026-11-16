param([switch]$Restore,[switch]$Update)
$ErrorActionPreference = 'Stop'
$modProbeRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$modProbePlugin = 'D:\Hypergryph Launcher\games\Endfield Game\plugin'
$modProbeData = Join-Path $modProbeRepo 'diagnostics\zfy-native-animation-20261113'
$modProbeInstall = Join-Path $modProbePlugin 'diagnostics\zfy-native-animation'
$modProbeDll = Join-Path $modProbePlugin 'eff.dll'
$modProbeReceiptPath = Join-Path $modProbeInstall 'deployment.json'
$modProbeSource = Join-Path $modProbePlugin 'backup\ZFY'
$modProbeDestination = Join-Path $modProbePlugin 'mods\ZFY_animation_test'
function Digest([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
function AssertClosed {
    if (@(Get-Process -Name Endfield,EndfieldGame,NTEGame -ErrorAction SilentlyContinue).Count) {
        throw 'Exit the game normally before installing/restoring the Mod animation experiment.'
    }
}
function Production([string]$Path) {
    & python (Join-Path $modProbeRepo 'tools\diagnostics\verify_production_dll.py') $Path
    if ($LASTEXITCODE -ne 0) { throw "Production DLL validation failed: $Path" }
}
AssertClosed
if ($Restore -and $Update) { throw 'Choose restoration or update, not both.' }
if ($Restore) {
    $modProbePrevious = Get-Content -LiteralPath $modProbeReceiptPath -Raw | ConvertFrom-Json
    if ((Digest $modProbePrevious.backup) -ne $modProbePrevious.originalSha256) { throw 'Production backup changed.' }
    Production $modProbePrevious.backup
    Copy-Item -LiteralPath $modProbePrevious.backup -Destination $modProbeDll -Force
    if ((Digest $modProbeDll) -ne $modProbePrevious.originalSha256) { throw 'Production restoration differs.' }
    $modProbePrevious.status = 'restored'
    $modProbePrevious | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $modProbeReceiptPath -Encoding utf8
    Write-Host 'Restored production DLL. The valid ZFY test copy remains available with its original player.'
    exit 0
}
foreach ($modProbeRelative in @('.', 'tools\AnimeStudio', 'tools\Blender', 'EFF-Mod-Manager')) {
    $modProbeRepository = Join-Path $modProbeRepo $modProbeRelative
    if ((& git -C $modProbeRepository branch --show-current).Trim() -ne 'main') { throw 'All repositories must remain on main.' }
    & git -C $modProbeRepository fetch origin
    if ($LASTEXITCODE -ne 0) { throw 'Remote preflight failed.' }
    & git -C $modProbeRepository merge-base --is-ancestor origin/main HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Local source is behind origin/main.' }
}
$modProbeRequest = Get-Content -LiteralPath (Join-Path $modProbeData 'request.json') -Raw | ConvertFrom-Json
$modProbeSourceReceipt = Get-Content -LiteralPath (Join-Path $modProbeData 'source-receipt.json') -Raw | ConvertFrom-Json
$modProbeClips = Get-Content -LiteralPath (Join-Path $modProbeData 'native-clips.json') -Raw | ConvertFrom-Json
if ([IO.Path]::GetFullPath($modProbeSourceReceipt.source) -ne $modProbeSource) { throw 'Unexpected source Mod.' }
if ((Test-Path -LiteralPath $modProbeDestination) -and -not $Update) { throw 'Test Mod already exists; use -Update to preserve its user state.' }
$modProbeOtherInis = @(Get-ChildItem -LiteralPath (Join-Path $modProbePlugin 'mods') -Recurse -File -Filter mod.ini)
foreach ($modProbeIni in $modProbeOtherInis) {
    if ($Update -and $modProbeIni.FullName -eq (Join-Path $modProbeDestination 'mod.ini')) { continue }
    $modProbeText = Get-Content -LiteralPath $modProbeIni.FullName -Raw
    if ($modProbeText -match '(?m)^\s*id\s*=\s*ZFY\s*$' -or $modProbeText -match 'assets/beyond/arts/entity/actor/lady/zhuangfy/models/') {
        throw "Enabled Mod already targets ZFY: $($modProbeIni.FullName)"
    }
}
foreach ($modProbeFile in $modProbeSourceReceipt.files.PSObject.Properties) {
    if ((Digest (Join-Path $modProbeSource $modProbeFile.Name)) -ne $modProbeFile.Value) { throw 'Source Mod changed after conversion.' }
}
$modProbeBuild = Join-Path $modProbeRepo 'bin\resource-directory-entry-format3-mod-animation-probe\eff.dll'
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($modProbeBuild)).Contains('[MOD-ANIMATION-PROBE]')) { throw 'Wrong diagnostic DLL.' }
New-Item -ItemType Directory -Force -Path $modProbeInstall | Out-Null
$modProbeLines = [Collections.Generic.List[string]]::new()
$modProbeLines.Add('[Experiment]')
$modProbeLines.Add('package=' + $modProbeRequest.package)
$modProbeLines.Add('animation=' + $modProbeRequest.animation)
$modProbeLines.Add('package_root=' + $modProbeDestination)
$modProbeLines.Add('duration=' + $modProbeRequest.duration.ToString([Globalization.CultureInfo]::InvariantCulture))
$modProbeIndex = 0
foreach ($modProbeTarget in $modProbeClips.targets) {
    if ([IO.Path]::GetFileName($modProbeTarget.file) -ne $modProbeTarget.file) { throw 'Unsafe Clip filename.' }
    $modProbeAsset = Join-Path $modProbeData $modProbeTarget.file
    if ((Digest $modProbeAsset) -ne $modProbeTarget.sha256) { throw 'Offline Clip changed.' }
    if (-not $Update) { Copy-Item -LiteralPath $modProbeAsset -Destination (Join-Path $modProbeInstall $modProbeTarget.file) -Force }
    if ((Digest (Join-Path $modProbeInstall $modProbeTarget.file)) -ne $modProbeTarget.sha256) { throw 'Installed Clip differs.' }
    $modProbeLines.Add('')
    $modProbeLines.Add('[Target' + $modProbeIndex + ']')
    foreach ($modProbeKey in @('part', 'file', 'asset', 'sha256')) { $modProbeLines.Add($modProbeKey + '=' + $modProbeTarget.$modProbeKey) }
    foreach ($modProbeTrack in $modProbeTarget.tracks) { $modProbeLines.Add('shape.' + $modProbeTrack.shape + '=' + $modProbeTrack.variable) }
    $modProbeIndex++
}
if ($Update) {
    $modProbePrevious = Get-Content -LiteralPath $modProbeReceiptPath -Raw | ConvertFrom-Json
    if ($modProbePrevious.target -ne $modProbeDll -or $modProbePrevious.testMod -ne $modProbeDestination -or
        $modProbePrevious.sourceMod -ne $modProbeSource -or $modProbePrevious.animation -ne $modProbeRequest.animation) {
        throw 'Previous installation does not identify this experiment.'
    }
    if ((Digest $modProbePrevious.backup) -ne $modProbePrevious.originalSha256) { throw 'Production backup changed.' }
    Production $modProbePrevious.backup
    $modProbeCurrentHash = Digest $modProbeDll
    if ($modProbeCurrentHash -ne $modProbePrevious.probeSha256 -and $modProbeCurrentHash -ne $modProbePrevious.originalSha256) {
        throw 'Installed DLL changed outside this experiment; preserve it and inspect before updating.'
    }
    $modProbeConfig = Get-Content -LiteralPath (Join-Path $modProbeInstall 'experiment.ini')
    if (($modProbeConfig -join "`n") -cne ($modProbeLines -join "`n")) { throw 'Installed experiment configuration changed.' }
    & (Join-Path $modProbeRepo 'bin\mod-shape-experiment\verify.exe') $modProbeDestination
    if ($LASTEXITCODE -ne 0) { throw 'Existing test Mod failed the current runtime reader.' }
    $modProbeState = Join-Path $modProbeDestination 'state.ini'
    $modProbeStateHash = if (Test-Path -LiteralPath $modProbeState) { Digest $modProbeState } else { '' }
    $modProbeStamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $modProbePriorDll = Join-Path $modProbeInstall ('before-update-' + $modProbeStamp + '.dll')
    $modProbePriorReceipt = Join-Path $modProbeInstall ('before-update-' + $modProbeStamp + '.json')
    Copy-Item -LiteralPath $modProbeDll -Destination $modProbePriorDll
    Copy-Item -LiteralPath $modProbeReceiptPath -Destination $modProbePriorReceipt
    try {
        AssertClosed
        Copy-Item -LiteralPath $modProbeBuild -Destination $modProbeDll -Force
        if ((Digest $modProbeDll) -ne (Digest $modProbeBuild)) { throw 'Installed updated probe differs.' }
        if ($modProbeStateHash -ne '' -and (Digest $modProbeState) -ne $modProbeStateHash) { throw 'Existing player state changed.' }
        $modProbePrevious.status = 'installed-awaiting-real-mod'
        $modProbePrevious.probeSha256 = Digest $modProbeDll
        $modProbePrevious.engineEvaluated = $false
        $modProbePrevious | Add-Member -NotePropertyName updatedAt -NotePropertyValue (Get-Date).ToString('o') -Force
        $modProbePrevious | Add-Member -NotePropertyName previousReceipt -NotePropertyValue $modProbePriorReceipt -Force
        $modProbePrevious | Add-Member -NotePropertyName preservedStateSha256 -NotePropertyValue $modProbeStateHash -Force
        $modProbePrevious | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $modProbeReceiptPath -Encoding utf8
    } catch {
        Copy-Item -LiteralPath $modProbePriorDll -Destination $modProbeDll -Force
        Copy-Item -LiteralPath $modProbePriorReceipt -Destination $modProbeReceiptPath -Force
        throw
    }
    Write-Host "Updated diagnostic DLL; test Mod resources, INI and saved state preserved: $modProbeDestination"
    Write-Host "Receipt: $modProbeReceiptPath"
    exit 0
}
Production $modProbeDll
$modProbeBackup = Join-Path $modProbeInstall ('production-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.dll')
Copy-Item -LiteralPath $modProbeDll -Destination $modProbeBackup
$modProbeOriginalHash = Digest $modProbeBackup
[IO.File]::WriteAllLines((Join-Path $modProbeInstall 'experiment.ini'), $modProbeLines, [Text.UTF8Encoding]::new($false))
AssertClosed
Copy-Item -LiteralPath $modProbeSource -Destination $modProbeDestination -Recurse
foreach ($modProbeFile in $modProbeSourceReceipt.files.PSObject.Properties) {
    if ((Digest (Join-Path $modProbeDestination $modProbeFile.Name)) -ne $modProbeFile.Value) { throw 'Copied Mod differs.' }
}
& (Join-Path $modProbeRepo 'bin\mod-shape-experiment\verify.exe') $modProbeDestination
if ($LASTEXITCODE -ne 0) { throw 'Copied Mod failed the current runtime reader.' }
try {
    AssertClosed
    Copy-Item -LiteralPath $modProbeBuild -Destination $modProbeDll -Force
    if ((Digest $modProbeDll) -ne (Digest $modProbeBuild)) { throw 'Installed Mod probe DLL differs.' }
    [ordered]@{
        status = 'installed-awaiting-real-mod'; installedAt = (Get-Date).ToString('o')
        backup = $modProbeBackup; originalSha256 = $modProbeOriginalHash
        target = $modProbeDll; probeSha256 = (Digest $modProbeDll)
        sourceMod = $modProbeSource; testMod = $modProbeDestination
        animation = $modProbeRequest.animation; targets = $modProbeClips.targets
        engineEvaluated = $false; releaseAllowed = $false
    } | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $modProbeReceiptPath -Encoding utf8
} catch {
    Copy-Item -LiteralPath $modProbeBackup -Destination $modProbeDll -Force
    throw
}
Write-Host "Installed real Mod animation experiment: $modProbeDestination"
Write-Host "Receipt: $modProbeReceiptPath"
