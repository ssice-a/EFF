$ErrorActionPreference = 'Stop'
$workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$gameRoot = 'D:\Hypergryph Launcher\games\Endfield Game'
$pluginRoot = Join-Path $gameRoot 'plugin'
$evidenceRoot = Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v32-full-resource-reload'
$diagnosticRoot = Join-Path $pluginRoot 'resource-input-diagnostics'
$rollbackRoot = Join-Path $diagnosticRoot 'rollback\v32-full-resource-reload'
$installedDll = Join-Path $pluginRoot 'eff.dll'
$buildDll = Join-Path $workspaceRoot 'bin\resource-directory-entry\eff.dll'
$ordinaryProxy = Join-Path $gameRoot 'vulkan-1.dll'
$inputTable = Join-Path $pluginRoot 'resource-input-substitutions.tsv'
$baselineRoot = Join-Path $pluginRoot 'resource-reload-baseline'
$testRoot = Join-Path $pluginRoot 'resource-reload-test'
$observationMarker = Join-Path $diagnosticRoot 'full-reload-observation.enabled'
function Digest([string] $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
  $running = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^Endfield.*\.exe$' -or
    ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($gameRoot + '\',[StringComparison]::OrdinalIgnoreCase))
  })
  if ($running.Count) { throw 'Deployment refused: a game-directory or Endfield process is running.' }
}
function AssertTree([string] $source, [string] $destination) {
  $root = [IO.Path]::GetFullPath($source).TrimEnd('\') + '\'
  foreach ($file in Get-ChildItem -LiteralPath $source -File -Recurse) {
    $relative = $file.FullName.Substring($root.Length)
    if ((Digest $file.FullName) -ne (Digest (Join-Path $destination $relative))) { throw "Staged file differs: $relative" }
  }
}
AssertStopped
$previousDllHash = Digest $installedDll
if ($previousDllHash -ne '848EE6D0B8889AFEEC39635FB22C6235559F48D52FBE9A99785BE47A3263678D') { throw 'Installed v31 DLL differs from accepted baseline.' }
$proxyHash = Digest $ordinaryProxy
if ($proxyHash -ne '78FB4C43B56DC2D4A50EBC7B4940943A61B3FC3D4EB533AB6911EB2F816FD48D') { throw 'Ordinary proxy baseline differs.' }
$tableHash = Digest $inputTable
if ($tableHash -ne '26D4A85C658F5D527A5F36CD65B84EE93ED2BD9B811A82F236BDF05D0A4C8CE9') { throw 'Accepted input table differs.' }
$local = Get-Content -LiteralPath (Join-Path $evidenceRoot 'visual-local-verification.json') -Raw | ConvertFrom-Json
if (-not $local.verified -or $local.productionDigest -ne '23EC195A7DDE3EB6295E4CCA256CBC39EDF41FF1E6B92737A6D41088AE32D5A6') { throw 'Visual witness source/local proof differs.' }
$apiPresence = Get-Content -LiteralPath (Join-Path $evidenceRoot 'api-static-presence.json') -Raw | ConvertFrom-Json
if (-not $apiPresence.compiledCandidatesPresent -or $apiPresence.methods.Count -ne 29) { throw 'Required compiled API candidates were not verified.' }
if (-not ((Get-Content -LiteralPath (Join-Path $pluginRoot 'eff.ini') -Raw) -match '(?m)^\s*disable_camera_fade\s*=\s*true\s*$')) { throw 'Accepted camera anti-fade setting differs.' }
foreach ($file in @('transaction-smoke.txt','candidate-smoke.txt','config-smoke.txt','shape-smoke.txt')) {
  if (-not (Get-Content -LiteralPath (Join-Path $evidenceRoot $file) -Raw).StartsWith('PASS')) { throw "Regression did not pass: $file" }
}
$buildLog = Get-Content -LiteralPath (Join-Path $evidenceRoot 'build.log') -Raw
if ($buildLog -match 'error C\d+|fatal error|error LNK') { throw 'Ordinary DLL build failed.' }
if ((Digest (Join-Path $gameRoot 'GameAssembly.dll')) -ne (Get-Content -LiteralPath (Join-Path $evidenceRoot 'baseline\assembly.sha256') -Raw).Trim()) { throw 'GameAssembly lifecycle evidence differs.' }
foreach ($marker in @('capture-observation.enabled','shape-capture-boundaries.enabled','shape-capture-gpu.enabled')) {
  if (Test-Path -LiteralPath (Join-Path $diagnosticRoot $marker)) { throw "Capture marker unexpectedly enabled: $marker" }
}
foreach ($path in @($rollbackRoot,$baselineRoot,$testRoot,(Join-Path $pluginRoot 'eff.dll.v32-staging'))) {
  if (Test-Path -LiteralPath $path) { throw "Deployment destination already exists: $path" }
}
$officialRoot = Join-Path $pluginRoot 'mods\LZY'
$testAuthorSource = Join-Path $evidenceRoot 'visual-test\author'
$authorHashes = [ordered]@{}
foreach ($file in Get-ChildItem -LiteralPath $testAuthorSource -File -Recurse) {
  $relative = $file.FullName.Substring($testAuthorSource.Length + 1)
  $authorHashes[$relative] = Digest (Join-Path $officialRoot $relative)
}
$authorLabel = "LZY:$($authorHashes['mod.ini'])`n"
foreach ($relative in ($authorHashes.Keys | Where-Object { $_ -ne 'mod.ini' } | Sort-Object)) {
  $authorLabel += "$($relative.Replace('\','/')):$($authorHashes[$relative])`n"
}
$shaObject = [Security.Cryptography.SHA256]::Create()
try { $authorDigest = [BitConverter]::ToString($shaObject.ComputeHash([Text.Encoding]::UTF8.GetBytes($authorLabel))).Replace('-','') } finally { $shaObject.Dispose() }
if ($authorDigest -ne $local.productionDigest) { throw 'Official author changed since local verification.' }
New-Item -ItemType Directory -Path $rollbackRoot | Out-Null
foreach ($file in @($installedDll,$inputTable,(Join-Path $pluginRoot 'eff.ini'),$observationMarker,(Join-Path $diagnosticRoot 'native-candidate-validation.tsv'))) {
  if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $rollbackRoot }
}
Copy-Item -LiteralPath (Join-Path $evidenceRoot 'baseline') -Destination $baselineRoot -Recurse
New-Item -ItemType Directory -Path $testRoot | Out-Null
Copy-Item -LiteralPath $testAuthorSource -Destination (Join-Path $testRoot 'author') -Recurse
AssertTree (Join-Path $evidenceRoot 'baseline') $baselineRoot
AssertTree $testAuthorSource (Join-Path $testRoot 'author')
$config = "EFF_RESOURCE_RELOAD_VALIDATION_AUTHOR`t1`nresource-reload-test/author`t$($local.productionDigest)`n"
[IO.File]::WriteAllText((Join-Path $baselineRoot 'validation-author.tsv'),$config,[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($observationMarker,"build=resource-directory-entry-v32-full-resource-reload`r`nstage=full-resource-world-transaction-test`r`ngfxRequested=false`r`nnativeMigrationAccepted=false`r`n",[Text.UTF8Encoding]::new($false))
$dllHash = Digest $buildDll
$stagedDll = Join-Path $pluginRoot 'eff.dll.v32-staging'
Copy-Item -LiteralPath $buildDll -Destination $stagedDll
AssertStopped
[IO.File]::Replace($stagedDll,$installedDll,(Join-Path $rollbackRoot 'eff.atomic-backup.dll'),$false)
if ((Digest $installedDll) -ne $dllHash -or (Digest $inputTable) -ne $tableHash -or (Digest $ordinaryProxy) -ne $proxyHash) { throw 'Post-deployment DLL/proxy/table verification failed.' }
foreach ($relative in $authorHashes.Keys) {
  if ((Digest (Join-Path $officialRoot $relative)) -ne $authorHashes[$relative]) { throw "Official author file changed: $relative" }
}
$deployment = [ordered]@{
  format='EFF_FULL_RESOURCE_RELOAD_DEPLOYMENT';version=1
  build='resource-directory-entry-v32-full-resource-reload';installed=$true
  deployedAt=(Get-Date -Format o);gameStoppedAtDeployment=$true
  dll=$installedDll;dllSha256=$dllHash;previousDllSha256=$previousDllHash
  ordinaryBuild=$true;gfxtraceLoadDisabled=$true;proxySha256=$proxyHash;inputTableSha256=$tableHash
  inputRows=60;rollback=$rollbackRoot;baseline=$baselineRoot;pristineInputs=68
  nativeClosureInputs=18;worldAndUIPrefabRoots=2
  testAuthor=(Join-Path $testRoot 'author');firstSuccessfulF10='cyan body texture';secondSuccessfulF10='installed original author graph'
  productionAuthorDigest=$local.productionDigest;officialAuthorFilesUnchanged=$true;officialAuthorHashes=$authorHashes
  disableCameraFadeEnabled=$true;sourceTableFrozen=$true;blenderChanged=$false
  diskWorkerRebuild=$true;nativeOperationsUnityFrameOnly=$true;asyncLifetimeAndRollbackMockTestPassed=$true
  perMeshBonesAndNativeShapesChecked=$true;originalCustomTextureChoicesChecked=$true
  exactSourcePrefabCloneRoutes=$true;liveWorldMigrationImplementationPresent=$true
  currentDisplayMigrationSupported=$false;persistentPoolMigrationSupported=$false
  generationRetention='process lifetime; maximum 3 attempts';generationRetirementFenceVerified=$false
  runtimeObservationReceived=$false;materialTexturePrefabCandidateVerified=$false
  existingInstanceResourceMigrationVerified=$false;completeResourceHotReloadAccepted=$false
}
$deployment | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'deployment.json') -Encoding utf8
$deployment | ConvertTo-Json -Depth 8
