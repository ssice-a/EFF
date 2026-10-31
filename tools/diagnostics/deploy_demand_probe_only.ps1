$ErrorActionPreference = 'Stop'
$workspace = 'E:\vscode\EIEM'
$plugin = 'D:\Hypergryph Launcher\games\Endfield Game\plugin'
$sourceDll = Join-Path $workspace 'bin\demand-loading-probe\eff.dll'
$targetDll = Join-Path $plugin 'eff.dll'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$receiptRoot = Join-Path $workspace ('bin\local-deployments\demand-probe-only-' + $stamp)
$stagedDll = Join-Path $plugin ('.eff-observe-' + $stamp + '.pending')
$swapBackup = Join-Path $plugin ('.eff-previous-' + $stamp + '.pending')
$hash = (Get-FileHash -LiteralPath $sourceDll -Algorithm SHA256).Hash

if (Get-Process Endfield* -ErrorAction SilentlyContinue) { throw 'Endfield is running; deployment was not performed.' }
$pluginDlls = @(Get-ChildItem -LiteralPath $plugin -File -Filter '*.dll')
if ($pluginDlls.Count -ne 1 -or $pluginDlls[0].Name -ne 'eff.dll') { throw 'The plugin directory must contain only eff.dll.' }
& python (Join-Path $workspace 'tools\diagnostics\verify_demand_loading_probe.py') $sourceDll
if ($LASTEXITCODE -ne 0) { throw 'The candidate DLL is not an observation-only build.' }

New-Item -ItemType Directory -Path $receiptRoot | Out-Null
$staleStages = @(Get-ChildItem -LiteralPath $plugin -File | Where-Object { $_.Name -match '^\.eff-(observe|previous|rollback|failed)-\d{8}-\d{6}\.pending$' })
if ($staleStages.Count) {
    $staleRoot = Join-Path $receiptRoot 'stale-stages'
    New-Item -ItemType Directory -Path $staleRoot | Out-Null
    foreach ($stage in $staleStages) { Move-Item -LiteralPath $stage.FullName -Destination $staleRoot }
}
$oldDll = Join-Path $receiptRoot 'previous-eff.dll'
$previousHash = (Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash
Copy-Item -LiteralPath $targetDll -Destination $oldDll
if (Test-Path -LiteralPath (Join-Path $plugin 'eff_log.txt')) {
    Copy-Item -LiteralPath (Join-Path $plugin 'eff_log.txt') -Destination (Join-Path $receiptRoot 'previous-eff_log.txt')
}
Copy-Item -LiteralPath $sourceDll -Destination $stagedDll
if ((Get-FileHash -LiteralPath $stagedDll -Algorithm SHA256).Hash -ne $hash) { throw 'Staged DLL hash differs.' }

try {
    [IO.File]::Replace($stagedDll, $targetDll, $swapBackup)
    if ((Get-FileHash -LiteralPath $swapBackup -Algorithm SHA256).Hash -ne $previousHash) { throw 'Atomic swap backup differs from the pre-deployment DLL.' }
    Remove-Item -LiteralPath $swapBackup
    if ((Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash -ne $hash) { throw 'Deployed DLL hash differs.' }
    & python (Join-Path $workspace 'tools\diagnostics\verify_demand_loading_probe.py') $targetDll
    if ($LASTEXITCODE -ne 0) { throw 'Post-deployment DLL is not an observation-only build.' }
} catch {
    if (Test-Path -LiteralPath $oldDll) {
        $rollback = Join-Path $plugin ('.eff-rollback-' + $stamp + '.pending')
        $failedCandidate = Join-Path $plugin ('.eff-failed-' + $stamp + '.pending')
        Copy-Item -LiteralPath $oldDll -Destination $rollback
        [IO.File]::Replace($rollback, $targetDll, $failedCandidate)
        if (Test-Path -LiteralPath $failedCandidate) { Move-Item -LiteralPath $failedCandidate -Destination (Join-Path $receiptRoot 'failed-eff.dll') }
    }
    throw
}

$receipt = [ordered]@{
    kind = 'demand-shared-loading-observation-dll-only'
    deployedAt = (Get-Date).ToString('o')
    target = $targetDll
    sha256 = $hash
    previousSha256 = $previousHash
    previousDll = $oldDll
    savedLog = (Join-Path $receiptRoot 'previous-eff_log.txt')
    authorAssetsChanged = $false
    receiptRoot = $receiptRoot
}
$receipt | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $receiptRoot 'deployment.json') -Encoding UTF8
$receipt | ConvertTo-Json -Depth 4
