$ErrorActionPreference = 'Stop'
$workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$gameRoot = 'D:\Hypergryph Launcher\games\Endfield Game'
$pluginRoot = Join-Path $gameRoot 'plugin'
$evidenceRoot = Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v32c-reload-preparation'
$previous = Get-Content -LiteralPath (Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v32b-bone-path-validation\deployment.json') -Raw | ConvertFrom-Json
$original = Get-Content -LiteralPath (Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v32-full-resource-reload\deployment.json') -Raw | ConvertFrom-Json
$build = 'resource-directory-entry-v32c-reload-preparation'
$installedDll = Join-Path $pluginRoot 'eff.dll'
$buildDll = Join-Path $workspaceRoot 'bin\resource-directory-entry\eff.dll'
$rollbackRoot = Join-Path $pluginRoot 'resource-input-diagnostics\rollback\v32c-reload-preparation'
$marker = Join-Path $pluginRoot 'resource-input-diagnostics\full-reload-observation.enabled'
function Digest([string] $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
  $running = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^Endfield.*\.exe$' -or
    ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($gameRoot + '\',[StringComparison]::OrdinalIgnoreCase))
  })
  if ($running.Count) { throw 'Deployment refused: a game-directory or Endfield process is running.' }
}
function AssertOfficial {
  foreach ($property in $original.officialAuthorHashes.PSObject.Properties) {
    if ((Digest (Join-Path (Join-Path $pluginRoot 'mods\LZY') $property.Name)) -ne $property.Value) { throw "Official author changed: $($property.Name)" }
  }
}
AssertStopped
if ((Digest $installedDll) -ne $previous.dllSha256) { throw 'Installed v32b DLL differs from recorded deployment.' }
AssertOfficial
$comparison = Get-Content -LiteralPath (Join-Path $evidenceRoot 'isolated-comparison.json') -Raw | ConvertFrom-Json
if (-not $comparison.verified -or -not $comparison.coldRebuild -or -not $comparison.allOutputBytesIdentical -or $comparison.inputs -ne 68) { throw 'Cold full resource output comparison failed.' }
if ($comparison.candidate.bounds.hits -ne 2384 -or $comparison.candidate.bounds.misses -ne 17 -or $comparison.candidate.steps.'mesh-repeat') { throw 'Expected elimination of repeated bounds/Mesh work differs.' }
if (-not (Get-Content -LiteralPath (Join-Path $evidenceRoot 'transaction-smoke.txt') -Raw).StartsWith('PASS full resource transaction')) { throw 'Native transaction/rollback mock regression failed.' }
if (-not (Get-Content -LiteralPath (Join-Path $evidenceRoot 'namespace-smoke.txt') -Raw).StartsWith('LOADING-GENERATION-SMOKE')) { throw 'Namespace hidden identity/overlap regression failed.' }
if ((Get-Content -LiteralPath (Join-Path $evidenceRoot 'build.log') -Raw) -match 'error C\d+|fatal error|error LNK') { throw 'Ordinary DLL build failed.' }
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($buildDll)).Contains($build)) { throw 'Final DLL build marker differs.' }
if ((Digest (Join-Path $gameRoot 'vulkan-1.dll')) -ne $previous.proxySha256 -or (Digest (Join-Path $pluginRoot 'resource-input-substitutions.tsv')) -ne $previous.inputTableSha256) { throw 'Ordinary proxy or source table differs.' }
if (-not ((Get-Content -LiteralPath (Join-Path $pluginRoot 'eff.ini') -Raw) -match '(?m)^\s*disable_camera_fade\s*=\s*true\s*$')) { throw 'Camera anti-fade setting differs.' }
foreach ($captureMarker in @('capture-observation.enabled','shape-capture-boundaries.enabled','shape-capture-gpu.enabled')) {
  if (Test-Path -LiteralPath (Join-Path $pluginRoot "resource-input-diagnostics\$captureMarker")) { throw "Capture unexpectedly enabled: $captureMarker" }
}
$staging = Join-Path $pluginRoot 'eff.dll.v32c-staging'
foreach ($path in @($rollbackRoot,$staging)) { if (Test-Path -LiteralPath $path) { throw "Deployment destination exists: $path" } }
$preserved = [ordered]@{}
foreach ($relative in @('resource-reload-baseline\validation-author.tsv','resource-reload-baseline\baseline.tsv','eff.ini','resource-input-diagnostics\native-candidate-validation.tsv')) { $preserved[$relative] = Digest (Join-Path $pluginRoot $relative) }
New-Item -ItemType Directory -Path $rollbackRoot | Out-Null
Copy-Item -LiteralPath $installedDll -Destination $rollbackRoot
Copy-Item -LiteralPath $marker -Destination $rollbackRoot
Copy-Item -LiteralPath $buildDll -Destination $staging
$dllHash = Digest $buildDll
if ((Digest $staging) -ne $dllHash) { throw 'Staging DLL hash differs.' }
AssertStopped
[IO.File]::Replace($staging,$installedDll,(Join-Path $rollbackRoot 'eff.atomic-backup.dll'),$false)
if ((Digest $installedDll) -ne $dllHash) { throw 'Installed DLL does not match final build.' }
[IO.File]::WriteAllText($marker,"build=$build`r`nstage=full-resource-world-transaction-test`r`ngfxRequested=false`r`nnativeMigrationAccepted=false`r`n",[Text.UTF8Encoding]::new($false))
AssertOfficial
foreach ($relative in $preserved.Keys) { if ((Digest (Join-Path $pluginRoot $relative)) -ne $preserved[$relative]) { throw "Preserved configuration changed: $relative" } }
if ((Digest (Join-Path $gameRoot 'vulkan-1.dll')) -ne $previous.proxySha256 -or (Digest (Join-Path $pluginRoot 'resource-input-substitutions.tsv')) -ne $previous.inputTableSha256) { throw 'Post-deployment proxy/table differs.' }
$result = [ordered]@{
  format='EFF_FULL_RESOURCE_RELOAD_PREPARATION_DEPLOYMENT';version=1;build=$build;installed=$true;deployedAt=(Get-Date -Format o)
  dll=$installedDll;dllSha256=$dllHash;previousDllSha256=$previous.dllSha256;rollback=$rollbackRoot;gameStoppedAtDeployment=$true
  ordinaryBuild=$true;gfxtraceLoadDisabled=$true;cameraAntiFadeEnabled=$true;proxySha256=$previous.proxySha256;inputTableSha256=$previous.inputTableSha256
  authorResourcesUnchanged=$true;baselineUnchanged=$true;visualConfigUnchanged=$true;blenderChanged=$false
  nativeAPIsAndLifecycleUnchanged=$true;namespaceHiddenIdentityChecksPreserved=$true;perRendererBoneMappingUnchanged=$true
  coldResourceInputsVerified=68;coldOutputBytesIdentical=$true;boundsComputeCount=17;boundsReuseCount=2384;meshRebuiltOncePerInput=$true
  referenceColdWallMs=$comparison.reference.wallMs;candidateColdWallMs=$comparison.candidate.wallMs
  referenceColdUserCpuMs=$comparison.reference.userCpuMs;candidateColdUserCpuMs=$comparison.candidate.userCpuMs
  preparationStageProgressEnabled=$true;workerThreadCpuCountersEnabled=$true;unchangedResourceShortcutAdded=$false
  runtimeObservationReceived=$false;inGamePreparationPerformanceMeasured=$false;existingInstanceMigrationVerified=$false;completeResourceHotReloadAccepted=$false
}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'deployment.json') -Encoding utf8
$result | ConvertTo-Json -Depth 5
