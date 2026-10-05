param([switch] $Resume)
$ErrorActionPreference = 'Stop'
$deployWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$deployGame = 'D:\Hypergryph Launcher\games\Endfield Game'
$deployPlugin = Join-Path $deployGame 'plugin'
$deployEvidence = Join-Path $deployWorkspace 'diagnostics\resource-directory-entry-20261005\v33-offline-prewarm'
$deployBuild = 'resource-directory-entry-v33-offline-prewarm'
$deployRollback = Join-Path $deployPlugin 'resource-input-diagnostics\rollback\v33-offline-prewarm'
$deployPrevious = Get-Content -LiteralPath (Join-Path $deployWorkspace 'diagnostics\resource-directory-entry-20261004\v32c-reload-preparation\deployment.json') -Raw | ConvertFrom-Json
$deployOriginal = Get-Content -LiteralPath (Join-Path $deployWorkspace 'diagnostics\resource-directory-entry-20261004\v32-full-resource-reload\deployment.json') -Raw | ConvertFrom-Json
function Digest([string] $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
  $running = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^Endfield.*\.exe$' -or
    ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($deployGame + '\',[StringComparison]::OrdinalIgnoreCase))
  })
  if ($running.Count) { throw 'Deployment refused: game-directory or Endfield process running.' }
}
function AssertAuthor {
  foreach ($row in $deployOriginal.officialAuthorHashes.PSObject.Properties) {
    if ((Digest (Join-Path (Join-Path $deployPlugin 'mods\LZY') $row.Name)) -ne $row.Value) { throw "Author changed: $($row.Name)" }
  }
}
AssertStopped
AssertAuthor
if ((Digest (Join-Path $deployPlugin 'eff.dll')) -ne $deployPrevious.dllSha256) { throw 'Installed DLL differs from recorded v32c.' }
if (Test-Path -LiteralPath $deployRollback) {
  if (-not $Resume -or (Digest (Join-Path $deployRollback 'eff.v32c.dll')) -ne $deployPrevious.dllSha256) { throw 'Rollback exists; valid explicit resume required.' }
} elseif ($Resume) { throw 'Resume has no recorded rollback.' }
$comparison = Get-Content -LiteralPath (Join-Path $deployEvidence 'native-output-comparison.json') -Raw | ConvertFrom-Json
$concurrency = Get-Content -LiteralPath (Join-Path $deployEvidence 'compiler-concurrency\result.json') -Raw | ConvertFrom-Json
if (-not $comparison.verified -or -not $comparison.allNativeOutputBytesIdentical -or $comparison.inputs -ne 68 -or -not $concurrency.passed) { throw 'Offline generation or concurrency regression failed.' }
if (-not (Get-Content -LiteralPath (Join-Path $deployEvidence 'production-loader-final.txt') -Raw).StartsWith('PASS production offline worker:')) { throw 'Actual production disk loader failed.' }
if (-not (Get-Content -LiteralPath (Join-Path $deployEvidence 'transaction-smoke.txt') -Raw).StartsWith('PASS full resource transaction')) { throw 'Native transaction regression failed.' }
if (-not (Get-Content -LiteralPath (Join-Path $deployEvidence 'blender-operator-manifest-verification.txt') -Raw).StartsWith('PASS compiled manifest:')) { throw 'Blender operator compiled manifest validation failed.' }
$buildDll = Join-Path $deployWorkspace 'bin\resource-directory-entry\eff.dll'
$buildCompiler = Join-Path $deployWorkspace 'bin\resource-pack\eff_resource_pack.exe'
foreach ($log in @('build.log','compiler-build.log')) {
  if ((Get-Content -LiteralPath (Join-Path $deployEvidence $log) -Raw) -match 'error C\d+|fatal error|error LNK') { throw "Build failed: $log" }
}
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($buildDll)).Contains($deployBuild)) { throw 'DLL build marker mismatch.' }
if ((Digest (Join-Path $deployGame 'vulkan-1.dll')) -ne $deployPrevious.proxySha256 -or (Digest (Join-Path $deployPlugin 'resource-input-substitutions.tsv')) -ne $deployPrevious.inputTableSha256) { throw 'Ordinary proxy/input table changed.' }
if (-not ((Get-Content -LiteralPath (Join-Path $deployPlugin 'eff.ini') -Raw) -match '(?m)^\s*disable_camera_fade\s*=\s*true\s*$')) { throw 'Anti-fade setting changed.' }
foreach ($marker in @('capture-observation.enabled','shape-capture-boundaries.enabled','shape-capture-gpu.enabled')) {
  if (Test-Path -LiteralPath (Join-Path $deployPlugin "resource-input-diagnostics\$marker")) { throw "GFX capture enabled: $marker" }
}
$pointer = (Get-Content -LiteralPath (Join-Path $deployPlugin 'resource-reload-cache\current.tsv'))[1]
if ($pointer -ne "4DB46C524E7E0DE336DF79D15DEA100AD16518B858238EF31FA258ED84E25E64`t$($comparison.manifestSHA)") { throw 'Installed compiled publication differs.' }
if ((Get-Content -LiteralPath (Join-Path $deployPlugin 'resource-reload-cache\startup.tsv')) -ne $pointer) { throw 'Frozen startup author differs.' }
$preserved = [ordered]@{}
foreach ($relative in @('eff.ini','resource-input-substitutions.tsv','resource-reload-baseline\baseline.tsv','resource-reload-baseline\assembly.sha256')) { $preserved[$relative] = Digest (Join-Path $deployPlugin $relative) }
if (-not $Resume) { New-Item -ItemType Directory -Path $deployRollback | Out-Null }
$dllTarget = Join-Path $deployPlugin 'eff.dll'
$compilerTarget = Join-Path $deployPlugin 'tools\eff_resource_pack.exe'
$blenderTarget = Join-Path $deployWorkspace 'tools\Blender\nativepack\eff_resource_pack.exe'
if (-not $Resume) { Copy-Item -LiteralPath $dllTarget -Destination (Join-Path $deployRollback 'eff.v32c.dll') }
foreach ($target in @($compilerTarget,$blenderTarget)) {
  if (-not $Resume -and (Test-Path -LiteralPath $target)) { Copy-Item -LiteralPath $target -Destination (Join-Path $deployRollback ($(if ($target -eq $compilerTarget) {'game-compiler.before.exe'} else {'blender-compiler.before.exe'}))) }
}
$markerTarget = Join-Path $deployPlugin 'resource-input-diagnostics\full-reload-observation.enabled'
if (-not $Resume -and (Test-Path -LiteralPath $markerTarget)) { Copy-Item -LiteralPath $markerTarget -Destination (Join-Path $deployRollback 'observation.before.enabled') }
$dllSha = Digest $buildDll
$compilerSha = Digest $buildCompiler
foreach ($pair in @(@($buildCompiler,$compilerTarget),@($buildCompiler,$blenderTarget),@($buildDll,$dllTarget))) {
  $source = $pair[0]; $target = $pair[1]; $staging = $target + '.v33-staging'
  if ((Test-Path -LiteralPath $staging) -and (-not $Resume -or (Digest $staging) -ne (Digest $source))) { throw "Unexpected staging exists: $staging" }
  if ((Test-Path -LiteralPath $target) -and (Digest $target) -eq (Digest $source)) {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging }
    continue
  }
  New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
  Copy-Item -LiteralPath $source -Destination $staging
  if ((Digest $staging) -ne (Digest $source)) { throw 'Staging SHA differs.' }
  AssertStopped
  if (Test-Path -LiteralPath $target) {
    # Windows ReplaceFile backup must stay on the target volume.
    $atomicBackup = $target + '.v33-atomic-before'
    if (Test-Path -LiteralPath $atomicBackup) { throw "Atomic backup exists: $atomicBackup" }
    [IO.File]::Replace($staging,$target,$atomicBackup,$false)
  }
  else { [IO.File]::Move($staging,$target) }
}
[IO.File]::WriteAllText($markerTarget,"build=$deployBuild`r`nstage=offline-prewarm-world-transaction-test`r`ngfxRequested=false`r`nnativeMigrationAccepted=false`r`n",[Text.UTF8Encoding]::new($false))
AssertAuthor
foreach ($relative in $preserved.Keys) { if ((Digest (Join-Path $deployPlugin $relative)) -ne $preserved[$relative]) { throw "Preserved file changed: $relative" } }
if ((Digest $dllTarget) -ne $dllSha -or (Digest $compilerTarget) -ne $compilerSha -or (Digest $blenderTarget) -ne $compilerSha) { throw 'Final binary hashes differ.' }
if ((Digest (Join-Path $deployGame 'vulkan-1.dll')) -ne $deployPrevious.proxySha256) { throw 'Final proxy differs.' }
$report = [ordered]@{
  format='EFF_OFFLINE_RESOURCE_RELOAD_DEPLOYMENT';version=1;build=$deployBuild;installed=$true;deployedAt=(Get-Date -Format o)
  dll=$dllTarget;dllSha256=$dllSha;previousDllSha256=$deployPrevious.dllSha256;compiler=$compilerTarget;compilerSha256=$compilerSha
  compilerABI='EFF_NATIVE_PACK_V33_1';blenderCompilerSHA256=(Digest $blenderTarget);blenderExportAligned=$true
  cache=(Join-Path $deployPlugin 'resource-reload-cache');frozenStartupAndInitialCandidate=$pointer
  nativeInputs=68;lockedNativeClosure=18;allNativeOutputBytesIdentical=$true;runtimeTypeTreeBuild=$false;prewarmBeforeF10=$true
  gameStoppedAtDeployment=$true;rollback=$deployRollback;officialAuthorUnchanged=$true;proxySha256=$deployPrevious.proxySha256
  inputTableSha256=$deployPrevious.inputTableSha256;cameraAntiFadeEnabled=$true;gfxtraceLoadDisabled=$true
  concurrencyRegressionPassed=$true;actualProductionDiskLoaderPassed=$true;nativeTransactionMockPassed=$true
  nativeMigrationVerified=$false;completeResourceHotReloadAccepted=$false;visualWitnessAutomaticColorChange=$false
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $deployEvidence 'deployment.json') -Encoding utf8
$report | ConvertTo-Json -Depth 5
