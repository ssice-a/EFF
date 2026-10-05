$ErrorActionPreference = 'Stop'
$gateWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$gateGame = 'D:\Hypergryph Launcher\games\Endfield Game'
$gatePlugin = Join-Path $gateGame 'plugin'
$gateEvidence = Join-Path $gateWorkspace 'diagnostics\resource-directory-entry-20261005\v33a-startup-prewarm-gate'
$gateBuild = 'resource-directory-entry-v33a-startup-prewarm-gate'
$gateRollback = Join-Path $gatePlugin 'resource-input-diagnostics\rollback\v33a-startup-prewarm-gate'
$gatePrevious = Get-Content -LiteralPath (Join-Path $gateWorkspace 'diagnostics\resource-directory-entry-20261005\v33-offline-prewarm\deployment.json') -Raw | ConvertFrom-Json
$gateOfficial = Get-Content -LiteralPath (Join-Path $gateWorkspace 'diagnostics\resource-directory-entry-20261004\v32-full-resource-reload\deployment.json') -Raw | ConvertFrom-Json
$gateVisual = Get-Content -LiteralPath (Join-Path $gateWorkspace 'diagnostics\resource-directory-entry-20261005\v33-different-resources\visual-deployment.json') -Raw | ConvertFrom-Json
function Digest([string] $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
  $processes = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^Endfield.*\.exe$' -or
    ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($gateGame + '\',[StringComparison]::OrdinalIgnoreCase))
  })
  if ($processes.Count) { throw 'DLL deployment refused: game-directory or Endfield process running.' }
}
function AssertAuthor {
  foreach ($row in $gateOfficial.officialAuthorHashes.PSObject.Properties) {
    $expected = $row.Value
    if ($row.Name.Replace('\','/') -eq $gateVisual.onlyChangedAuthorFile) { $expected = $gateVisual.installedHash }
    if ((Digest (Join-Path (Join-Path $gatePlugin 'mods\LZY') $row.Name)) -ne $expected) { throw "Unexpected author change: $($row.Name)" }
  }
}
AssertStopped
AssertAuthor
$gateDllTarget = Join-Path $gatePlugin 'eff.dll'
$gateDllSource = Join-Path $gateWorkspace 'bin\resource-directory-entry\eff.dll'
if ((Digest $gateDllTarget) -ne $gatePrevious.dllSha256) { throw 'Installed DLL differs from recorded v33.' }
if (Test-Path -LiteralPath $gateRollback) { throw 'Startup-gate rollback already exists; deployment is not repeated automatically.' }
$gateSmoke = Get-Content -LiteralPath (Join-Path $gateEvidence 'green-smoke.txt') -Raw
if (-not $gateSmoke.StartsWith('PASS full resource transaction') -or -not $gateSmoke.Contains('startup/login zero game resource requests')) { throw 'Startup gate or transaction regression did not pass.' }
if (-not (Get-Content -LiteralPath (Join-Path $gateEvidence 'production-loader.txt') -Raw).StartsWith('PASS production offline worker:')) { throw 'Production offline loader regression did not pass.' }
foreach ($log in @('build.log','green-build.log','loader-build.log')) {
  if ((Get-Content -LiteralPath (Join-Path $gateEvidence $log) -Raw) -match 'error C\d+|fatal error|error LNK') { throw "Build failed: $log" }
}
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($gateDllSource)).Contains($gateBuild)) { throw 'DLL build marker differs.' }
if ((Digest (Join-Path $gateGame 'vulkan-1.dll')) -ne $gatePrevious.proxySha256 -or (Digest (Join-Path $gatePlugin 'resource-input-substitutions.tsv')) -ne $gatePrevious.inputTableSha256) { throw 'Ordinary proxy or initial substitution table changed.' }
if ((Digest (Join-Path $gatePlugin 'tools\eff_resource_pack.exe')) -ne $gatePrevious.compilerSha256 -or (Digest (Join-Path $gateWorkspace 'tools\Blender\nativepack\eff_resource_pack.exe')) -ne $gatePrevious.compilerSha256) { throw 'Aligned offline compiler changed.' }
$gateCurrentRows = @(Get-Content -LiteralPath (Join-Path $gatePlugin 'resource-reload-cache\current.tsv'))
if ($gateCurrentRows.Count -ne 2 -or $gateCurrentRows[0] -ne "EFF_RESOURCE_RELOAD_CURRENT`t1" -or $gateCurrentRows[1] -ne $gateVisual.candidatePointer) { throw 'Cyan candidate publication differs.' }
if ((Get-Content -LiteralPath (Join-Path $gatePlugin 'resource-reload-cache\startup.tsv') -Raw).Trim() -ne $gateVisual.originalPointer) { throw 'Frozen initial substitution author differs.' }
if (-not ((Get-Content -LiteralPath (Join-Path $gatePlugin 'eff.ini') -Raw) -match '(?m)^\s*disable_camera_fade\s*=\s*true\s*$')) { throw 'Anti-fade setting changed.' }
foreach ($marker in @('capture-observation.enabled','shape-capture-boundaries.enabled','shape-capture-gpu.enabled')) {
  if (Test-Path -LiteralPath (Join-Path $gatePlugin "resource-input-diagnostics\$marker")) { throw "GFX capture enabled: $marker" }
}
$gatePreserved = [ordered]@{}
foreach ($relative in @('eff.ini','resource-input-substitutions.tsv','resource-reload-cache\startup.tsv','resource-reload-cache\current.tsv','resource-reload-baseline\baseline.tsv','resource-reload-baseline\assembly.sha256','tools\eff_resource_pack.exe')) {
  $gatePreserved[$relative] = Digest (Join-Path $gatePlugin $relative)
}
$gateStaging = $gateDllTarget + '.v33a-staging'
if (Test-Path -LiteralPath $gateStaging) { throw 'Unexpected DLL staging already exists.' }
New-Item -ItemType Directory -Path $gateRollback | Out-Null
$gateMarkerTarget = Join-Path $gatePlugin 'resource-input-diagnostics\full-reload-observation.enabled'
if (Test-Path -LiteralPath $gateMarkerTarget) { Copy-Item -LiteralPath $gateMarkerTarget -Destination (Join-Path $gateRollback 'observation.before.enabled') }
Copy-Item -LiteralPath $gateDllSource -Destination $gateStaging
$gateDllSha = Digest $gateDllSource
if ((Digest $gateStaging) -ne $gateDllSha) { throw 'Staging DLL SHA differs.' }
AssertStopped
# ReplaceFile backup is on the same D: volume and outside the strict author directory.
$gateBackup = Join-Path $gateRollback 'eff.v33.dll'
[IO.File]::Replace($gateStaging,$gateDllTarget,$gateBackup,$false)
if ((Digest $gateBackup) -ne $gatePrevious.dllSha256 -or (Digest $gateDllTarget) -ne $gateDllSha) { throw 'Atomic replacement hashes differ.' }
[IO.File]::WriteAllText($gateMarkerTarget,"build=$gateBuild`r`nstage=await-stable-world-before-extra-prewarm`r`ngfxRequested=false`r`nnativeMigrationAccepted=false`r`n",[Text.UTF8Encoding]::new($false))
AssertAuthor
foreach ($relative in $gatePreserved.Keys) { if ((Digest (Join-Path $gatePlugin $relative)) -ne $gatePreserved[$relative]) { throw "Preserved file changed: $relative" } }
if ((Digest (Join-Path $gateGame 'vulkan-1.dll')) -ne $gatePrevious.proxySha256) { throw 'Final ordinary proxy differs.' }
$gateReport = [ordered]@{
  format='EFF_STARTUP_PREWARM_GATE_DEPLOYMENT';version=1;build=$gateBuild;installed=$true;deployedAt=(Get-Date -Format o)
  dll=$gateDllTarget;dllSha256=$gateDllSha;previousDllSha256=$gatePrevious.dllSha256;rollback=$gateRollback;gameStoppedAtDeployment=$true
  initialSerializedSubstitutionStillAtGameLoad=$true;initialInputTableUnchanged=$true;startupPointer=$gateVisual.originalPointer;candidatePointer=$gateVisual.candidatePointer
  extraPrewarmRequiresStableWorldOwner=$true;worldStableObservationMs=1000;worldPollMs=250;loginGameResourceRequestsMock=0;earlyF10Preserved=$true
  ownerLossBeforeRequestReturnsToWait=$true;inflightOwnerLossDrainsOnce=$true;nativeTransactionMockPassed=$true;actualProductionDiskLoaderPassed=$true
  compilerABI='EFF_NATIVE_PACK_V33_1';compilerSha256=$gatePrevious.compilerSha256;blenderCompilerUnchanged=$true;authorUnchangedFromVisualWitness=$true
  proxySha256=$gatePrevious.proxySha256;inputTableSha256=$gatePrevious.inputTableSha256;cameraAntiFadeEnabled=$true;gfxtraceLoadDisabled=$true
  startupGameAcceptanceVerified=$false;nativeMigrationVerified=$false;completeResourceHotReloadAccepted=$false
}
$gateReport | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $gateEvidence 'deployment.json') -Encoding utf8
$gateReport | ConvertTo-Json -Depth 5
