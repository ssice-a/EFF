$ErrorActionPreference = 'Stop'
$workspace = 'E:\vscode\EIEM'
$plugin = 'D:\Hypergryph Launcher\games\Endfield Game\plugin'
$sourceDll = Join-Path $workspace 'bin\demand-native-acceptance\eff.dll'
$sourceAssets = Join-Path $workspace 'bin\author-assets\installed-mods-native-logical-20261029'
$targetDll = Join-Path $plugin 'eff.dll'
$targetAssets = Join-Path $plugin 'eff-author-assets'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$receiptRoot = Join-Path $workspace ('bin\local-deployments\demand-logical-materials-' + $stamp)
$stagedAssets = Join-Path $plugin ('.eff-author-assets-stage-' + $stamp)
$backupAssets = Join-Path $plugin ('.eff-author-assets-backup-' + $stamp)
$stagedDll = Join-Path $plugin ('.eff-pending-' + $stamp + '.pending')
$backupNativeDll = Join-Path $receiptRoot 'replaced-eff.dll'
$swapBackup = Join-Path $plugin ('.eff-previous-' + $stamp + '.pending')
$mods = @('chen','gly','lino','lx','LZY','si','Typhoeus','ZFY')

function RequireGameExited {
    if (Get-Process Endfield* -ErrorAction SilentlyContinue) { throw 'Endfield is running; no deployment was performed.' }
}
function Inventory([string]$root) {
    @(Get-ChildItem -LiteralPath $root -File -Recurse | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{ path = $_.FullName.Substring($root.Length + 1); bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
}
function ProtectedFiles {
    $paths = @((Join-Path (Split-Path $plugin) 'd3dcompiler_47.dll'), (Join-Path (Split-Path $plugin) 'vulkan-1.dll'))
    foreach ($mod in $mods) { $paths += Join-Path $plugin ('mods\' + $mod + '\mod.ini') }
    @($paths | Where-Object { Test-Path -LiteralPath $_ } | ForEach-Object {
        [pscustomobject]@{ path = $_; sha256 = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash }
    })
}

RequireGameExited
$unexpectedEffDlls = @(Get-ChildItem -LiteralPath $plugin -File -Filter '*.dll' | Where-Object { $_.Name -match '^\.eff-(pending|previous)-' })
if ($unexpectedEffDlls.Count) { throw 'Old EFF deployment DLLs remain in plugin; move them outside the load directory before deploying.' }
New-Item -ItemType Directory -Path $receiptRoot | Out-Null
$protectedBefore = ProtectedFiles
$sourceHash = (Get-FileHash -LiteralPath $sourceDll -Algorithm SHA256).Hash
$previousHash = (Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash
Copy-Item -LiteralPath $targetDll -Destination (Join-Path $receiptRoot 'previous-eff.dll')
if (Test-Path -LiteralPath (Join-Path $plugin 'eff_log.txt')) { Copy-Item -LiteralPath (Join-Path $plugin 'eff_log.txt') -Destination (Join-Path $receiptRoot 'previous-eff_log.txt') }
& python (Join-Path $workspace 'tools\diagnostics\verify_demand_loading_probe.py') $sourceDll --native-acceptance
if ($LASTEXITCODE -ne 0) { throw 'The candidate DLL is not a native-acceptance build.' }
$files = Inventory $sourceAssets
$files = @($files | Where-Object { ($_.path -split '\\')[0] -in $mods })
if ($files.Count -eq 0) { throw 'No author resources were found.' }
New-Item -ItemType Directory -Path $stagedAssets | Out-Null
foreach ($mod in $mods) {
    if (!(Test-Path -LiteralPath (Join-Path $sourceAssets ($mod + '\assets.bin')))) { throw ('Author declaration missing: ' + $mod) }
    Copy-Item -LiteralPath (Join-Path $sourceAssets $mod) -Destination $stagedAssets -Recurse
}
foreach ($file in $files) {
    $target = Join-Path $stagedAssets $file.path
    if (!(Test-Path -LiteralPath $target) -or (Get-Item -LiteralPath $target).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $file.sha256) { throw ('Author resource copy verification failed: ' + $file.path) }
}
& python (Join-Path $workspace 'tools\diagnostics\verify_demand_deployment.py') $sourceDll $stagedAssets
if ($LASTEXITCODE -ne 0) { throw 'DLL/author directory contract preflight failed.' }
Copy-Item -LiteralPath $sourceDll -Destination $stagedDll
if ((Get-FileHash -LiteralPath $stagedDll -Algorithm SHA256).Hash -ne $sourceHash) { throw 'Staged DLL copy differs.' }
RequireGameExited
$movedOld = $false
$publishedAssets = $false
$publishedDll = $false
try {
    Move-Item -LiteralPath $targetAssets -Destination $backupAssets
    $movedOld = $true
    Move-Item -LiteralPath $stagedAssets -Destination $targetAssets
    $publishedAssets = $true
    [IO.File]::Replace($stagedDll, $targetDll, $swapBackup)
    $publishedDll = $true
    if ((Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash -ne $sourceHash) { throw 'Deployed DLL differs.' }
    if ((Get-FileHash -LiteralPath $swapBackup -Algorithm SHA256).Hash -ne $previousHash) { throw 'Atomic swap backup differs from the pre-deployment DLL.' }
    & python (Join-Path $workspace 'tools\diagnostics\verify_demand_deployment.py') $targetDll $targetAssets
    if ($LASTEXITCODE -ne 0) { throw 'Deployed DLL/author resources no longer match.' }
    & python (Join-Path $workspace 'tools\diagnostics\verify_demand_loading_probe.py') $targetDll --native-acceptance
    if ($LASTEXITCODE -ne 0) { throw 'Deployed DLL is not a native-acceptance build.' }
    Move-Item -LiteralPath $swapBackup -Destination $backupNativeDll
    $protectedAfter = ProtectedFiles
    if (($protectedBefore | ConvertTo-Json -Compress) -ne ($protectedAfter | ConvertTo-Json -Compress)) { throw 'A protected proxy/Mod declaration changed.' }
} catch {
    if ($publishedDll) { Copy-Item -LiteralPath (Join-Path $receiptRoot 'previous-eff.dll') -Destination $targetDll -Force }
    if ($publishedAssets) { Move-Item -LiteralPath $targetAssets -Destination ($targetAssets + '.failed-' + $stamp) }
    if ($movedOld) { Move-Item -LiteralPath $backupAssets -Destination $targetAssets }
    throw
}
$receipt = [ordered]@{
    kind = 'demand-native-logical-material-dependency-acceptance'
    deployedAt = (Get-Date).ToString('o')
    source = $sourceDll
    target = $targetDll
    sha256 = $sourceHash
    previousSha256 = $previousHash
    backupDll = (Join-Path $receiptRoot 'previous-eff.dll')
    backupAuthorAssets = $backupAssets
    authorAssets = $targetAssets
    authorMods = $mods
    authorAssetFiles = $files.Count
    authorAssetBytes = ($files | Measure-Object -Property bytes -Sum).Sum
    authorAudit = @{ mods = 8; meshes = 194; materials = 82; textures = 197; physics = 1; effectBundles = 3 }
    actualReplacementVerified = $false
    completeFeatureMigration = $false
    f10Enabled = $false
    modsChanged = $false
    proxiesChanged = $false
    files = $files
    protectedFiles = $protectedAfter
}
$json = $receipt | ConvertTo-Json -Depth 8
$json | Set-Content -LiteralPath (Join-Path $receiptRoot 'deployment.json') -Encoding UTF8
$json | Set-Content -LiteralPath (Join-Path $workspace 'bin\demand-native-acceptance\latest-deployment.json') -Encoding UTF8
[pscustomobject]@{ target = $targetDll; sha256 = $sourceHash; authorMods = $mods.Count; files = $files.Count; bytes = $receipt.authorAssetBytes; receipt = (Join-Path $receiptRoot 'deployment.json'); backupAuthorAssets = $backupAssets } | ConvertTo-Json -Depth 3
