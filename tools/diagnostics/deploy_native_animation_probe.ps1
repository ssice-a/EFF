param([switch]$Restore)
$ErrorActionPreference = 'Stop'
$animationRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$animationPlugin = 'D:\Hypergryph Launcher\games\Endfield Game\plugin'
$animationInstall = Join-Path $animationPlugin 'diagnostics\native-animation-probe'
$animationDll = Join-Path $animationPlugin 'eff.dll'
$animationReceiptPath = Join-Path $animationInstall 'deployment.json'
$animationProcesses = @(Get-Process -Name Endfield,EndfieldGame,NTEGame -ErrorAction SilentlyContinue)
if ($animationProcesses.Count) { throw 'Exit the game normally before deploying/restoring the animation probe.' }
function AnimationDigest([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function VerifyAnimationProduction([string]$Path) {
    & python (Join-Path $animationRepo 'tools\diagnostics\verify_production_dll.py') $Path
    if ($LASTEXITCODE -ne 0) { throw "Expected a verified production DLL: $Path" }
}
if ($Restore) {
    $animationReceipt = Get-Content -LiteralPath $animationReceiptPath -Raw | ConvertFrom-Json
    if ((AnimationDigest $animationReceipt.backup) -ne $animationReceipt.originalSha256) { throw 'Original DLL backup digest changed.' }
    VerifyAnimationProduction $animationReceipt.backup
    Copy-Item -LiteralPath $animationReceipt.backup -Destination $animationDll -Force
    if ((AnimationDigest $animationDll) -ne $animationReceipt.originalSha256) { throw 'DLL restoration digest differs.' }
    $animationReceipt.status = 'restored'
    $animationReceipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $animationReceiptPath -Encoding utf8
    Write-Host "Restored production DLL: $animationDll"
    exit 0
}
$animationBuild = Join-Path $animationRepo 'bin\resource-directory-entry-format3-animation-probe\eff.dll'
$animationFixture = Join-Path $animationRepo 'diagnostics\native-animation-20261113\mixed-animation.ab'
foreach ($animationFile in @($animationBuild, $animationFixture, $animationDll)) {
    if (-not (Test-Path -LiteralPath $animationFile -PathType Leaf)) { throw "Missing probe input: $animationFile" }
}
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($animationBuild)).Contains('[NATIVE-ANIMATION-PROBE]')) {
    throw 'Selected DLL does not contain the opt-in animation probe.'
}
New-Item -ItemType Directory -Force -Path $animationInstall | Out-Null
$animationPrevious = if (Test-Path -LiteralPath $animationReceiptPath) { Get-Content -LiteralPath $animationReceiptPath -Raw | ConvertFrom-Json } else { $null }
if ($animationPrevious -and $animationPrevious.status -eq 'installed') {
    $animationBackup = $animationPrevious.backup
    $animationOriginalDigest = $animationPrevious.originalSha256
    if ((AnimationDigest $animationBackup) -ne $animationOriginalDigest) { throw 'Original production backup changed.' }
} else {
    VerifyAnimationProduction $animationDll
    $animationBackup = Join-Path $animationInstall ('production-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.dll')
    Copy-Item -LiteralPath $animationDll -Destination $animationBackup
    $animationOriginalDigest = AnimationDigest $animationBackup
}
$animationExpected = AnimationDigest $animationBuild
try {
    Copy-Item -LiteralPath $animationFixture -Destination (Join-Path $animationInstall 'mixed-animation.ab') -Force
    Copy-Item -LiteralPath $animationBuild -Destination $animationDll -Force
    if ((AnimationDigest $animationDll) -ne $animationExpected) { throw 'Installed animation probe DLL digest differs.' }
    if ((AnimationDigest (Join-Path $animationInstall 'mixed-animation.ab')) -ne (AnimationDigest $animationFixture)) {
        throw 'Installed animation fixture digest differs.'
    }
    [ordered]@{
        status = 'installed'; installedAt = (Get-Date).ToString('o'); backup = $animationBackup
        originalSha256 = $animationOriginalDigest; probeSha256 = $animationExpected
        source = $animationBuild; target = $animationDll; asset = (Join-Path $animationInstall 'mixed-animation.ab')
        assetSha256 = (AnimationDigest $animationFixture); productionReleaseAllowed = $false
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $animationReceiptPath -Encoding utf8
} catch {
    Copy-Item -LiteralPath $animationBackup -Destination $animationDll -Force
    throw
}
Write-Host "Installed opt-in native animation probe: $animationDll"
Write-Host "Receipt: $animationReceiptPath"
