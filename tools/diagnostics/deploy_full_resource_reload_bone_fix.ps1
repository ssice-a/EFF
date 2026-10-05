$ErrorActionPreference = 'Stop'
$workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$gameRoot = 'D:\Hypergryph Launcher\games\Endfield Game'
$pluginRoot = Join-Path $gameRoot 'plugin'
$evidenceRoot = Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v32b-bone-path-validation'
$previous = Get-Content -LiteralPath (Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v32a-enum-abi-fix\deployment.json') -Raw | ConvertFrom-Json
$original = Get-Content -LiteralPath (Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v32-full-resource-reload\deployment.json') -Raw | ConvertFrom-Json
$rollbackRoot = Join-Path $pluginRoot 'resource-input-diagnostics\rollback\v32b-bone-path-validation'
$installedDll = Join-Path $pluginRoot 'eff.dll'
$buildDll = Join-Path $workspaceRoot 'bin\resource-directory-entry\eff.dll'
$marker = Join-Path $pluginRoot 'resource-input-diagnostics\full-reload-observation.enabled'
$build = 'resource-directory-entry-v32b-bone-path-validation'
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
if ((Digest $installedDll) -ne $previous.dllSha256) { throw 'Installed v32a DLL differs from recorded deployment.' }
if ((Digest (Join-Path $gameRoot 'vulkan-1.dll')) -ne $previous.proxySha256) { throw 'Ordinary proxy differs.' }
if ((Digest (Join-Path $pluginRoot 'resource-input-substitutions.tsv')) -ne $previous.inputTableSha256) { throw 'Input table differs.' }
AssertOfficial
if (-not ((Get-Content -LiteralPath (Join-Path $pluginRoot 'eff.ini') -Raw) -match '(?m)^\s*disable_camera_fade\s*=\s*true\s*$')) { throw 'Camera anti-fade setting differs.' }
foreach ($captureMarker in @('capture-observation.enabled','shape-capture-boundaries.enabled','shape-capture-gpu.enabled')) {
  if (Test-Path -LiteralPath (Join-Path $pluginRoot "resource-input-diagnostics\$captureMarker")) { throw "Capture unexpectedly enabled: $captureMarker" }
}
$red = Get-Content -LiteralPath (Join-Path $evidenceRoot 'red-smoke.txt') -Raw
$green = Get-Content -LiteralPath (Join-Path $evidenceRoot 'green-smoke.txt') -Raw
if (-not $red.StartsWith('FAIL reload per-Mesh bone slot differs: part/0') -or -not $green.StartsWith('PASS full resource transaction')) { throw 'Required failing/passing bone path transaction regression not present.' }
$audit = Get-Content -LiteralPath (Join-Path $evidenceRoot 'actual-generation-bone-paths.json') -Raw | ConvertFrom-Json
if (-not $audit.verified -or $audit.renderers -ne 67 -or $audit.distinctMarkers -ne 43 -or $audit.orderedSlots -ne 4561 -or $audit.legacyLeafComparisonsRejected -ne 4561) { throw 'Actual candidate bone path/order audit differs.' }
$api = Get-Content -LiteralPath (Join-Path $evidenceRoot 'api-static-presence.json') -Raw | ConvertFrom-Json
if (-not $api.compiledCandidatesPresent -or $api.methods.Count -ne 30) { throw 'Compiled API candidates differ.' }
if ((Get-Content -LiteralPath (Join-Path $evidenceRoot 'build.log') -Raw) -match 'error C\d+|fatal error|error LNK') { throw 'Ordinary DLL build failed.' }
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($buildDll)).Contains($build)) { throw 'Final DLL build marker differs.' }
foreach ($path in @($rollbackRoot,(Join-Path $pluginRoot 'eff.dll.v32b-staging'))) {
  if (Test-Path -LiteralPath $path) { throw "Deployment destination exists: $path" }
}
$preserved = [ordered]@{}
foreach ($relative in @('resource-reload-baseline\validation-author.tsv','resource-reload-baseline\baseline.tsv','eff.ini','resource-input-diagnostics\native-candidate-validation.tsv')) { $preserved[$relative] = Digest (Join-Path $pluginRoot $relative) }
New-Item -ItemType Directory -Path $rollbackRoot | Out-Null
Copy-Item -LiteralPath $installedDll -Destination $rollbackRoot
Copy-Item -LiteralPath $marker -Destination $rollbackRoot
$staging = Join-Path $pluginRoot 'eff.dll.v32b-staging'
Copy-Item -LiteralPath $buildDll -Destination $staging
$dllHash = Digest $buildDll
if ((Digest $staging) -ne $dllHash) { throw 'Staging DLL hash differs.' }
AssertStopped
[IO.File]::Replace($staging,$installedDll,(Join-Path $rollbackRoot 'eff.atomic-backup.dll'),$false)
if ((Digest $installedDll) -ne $dllHash) { throw 'Installed DLL does not match final build.' }
[IO.File]::WriteAllText($marker,"build=$build`r`nstage=full-resource-world-transaction-test`r`ngfxRequested=false`r`nnativeMigrationAccepted=false`r`n",[Text.UTF8Encoding]::new($false))
foreach ($relative in $preserved.Keys) { if ((Digest (Join-Path $pluginRoot $relative)) -ne $preserved[$relative]) { throw "Preserved configuration changed: $relative" } }
AssertOfficial
if ((Digest (Join-Path $gameRoot 'vulkan-1.dll')) -ne $previous.proxySha256 -or (Digest (Join-Path $pluginRoot 'resource-input-substitutions.tsv')) -ne $previous.inputTableSha256) { throw 'Post-deployment proxy/table differs.' }
$result = [ordered]@{
  format='EFF_FULL_RESOURCE_RELOAD_BONE_FIX_DEPLOYMENT';version=1;build=$build;installed=$true;deployedAt=(Get-Date -Format o)
  dll=$installedDll;dllSha256=$dllHash;previousDllSha256=$previous.dllSha256;rollback=$rollbackRoot
  gameStoppedAtDeployment=$true;ordinaryBuild=$true;gfxtraceLoadDisabled=$true;cameraAntiFadeEnabled=$true
  proxySha256=$previous.proxySha256;inputTableSha256=$previous.inputTableSha256
  authorResourcesUnchanged=$true;baselineUnchanged=$true;visualConfigUnchanged=$true;blenderChanged=$false
  failureObserved='run-15744-01DD5408B7966C4F: 68 inputs prepared, 18 native Bundles loaded; leaf-name/full-path mismatch rejected before owner migration'
  previousWorkerMs=77048.1156;fullAuthoredBonePathsValidated=$true;uniqueLocalSkeletonAnchorRequired=$true;perMeshSlotOrderValidated=$true
  duplicatePointerForeignSkeletonAndCycleRejected=$true;gameObjectTransformGetterReceiverCorrected=$true
  actualCandidateRenderersAudited=67;actualCandidateDistinctMarkersAudited=43;actualCandidateOrderedSlotsAudited=4561
  enumAndModelTransactionRollbackMockRegressionPassed=$true;compiledAPICandidates=30
  firstSuccessfulF10='cyan body texture';secondSuccessfulF10='original installed author graph';expectedTestWaitSeconds=120
  runtimeObservationReceived=$false;existingInstanceMigrationVerified=$false;completeResourceHotReloadAccepted=$false
  currentDisplayMigrationSupported=$false;persistentPoolMigrationSupported=$false;generationRetirementFenceVerified=$false
}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'deployment.json') -Encoding utf8
$result | ConvertTo-Json -Depth 5
