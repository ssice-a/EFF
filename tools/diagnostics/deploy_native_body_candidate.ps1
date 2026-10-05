$ErrorActionPreference = 'Stop'
$workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$gameRoot = 'D:\Hypergryph Launcher\games\Endfield Game'
$pluginRoot = Join-Path $gameRoot 'plugin'
$evidenceRoot = Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v31-native-body-candidate'
$diagnosticRoot = Join-Path $pluginRoot 'resource-input-diagnostics'
$rollbackRoot = Join-Path $diagnosticRoot 'rollback\v31-native-body-candidate'
$candidateRoot = Join-Path $pluginRoot 'resource-input-candidates\v31-native-body-candidate'
$installedDll = Join-Path $pluginRoot 'eff.dll'
$buildDll = Join-Path $workspaceRoot 'bin\resource-directory-entry\eff.dll'
$ordinaryProxy = Join-Path $gameRoot 'vulkan-1.dll'
$inputTable = Join-Path $pluginRoot 'resource-input-substitutions.tsv'
$sourceCandidate = Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\r1-candidate-graph\namespace-03\inputs\input-040.vfs'
$sourceSpec = Join-Path $evidenceRoot 'native-candidate-validation.tsv'
$installedSpec = Join-Path $diagnosticRoot 'native-candidate-validation.tsv'
$observationMarker = Join-Path $diagnosticRoot 'full-reload-observation.enabled'
function Digest([string] $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
  $running = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^Endfield.*\.exe$' -or
    ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($gameRoot + '\',[StringComparison]::OrdinalIgnoreCase))
  })
  if ($running.Count) { throw 'Deployment refused: a game-directory or Endfield process is running.' }
}
AssertStopped
$previousDllHash = Digest $installedDll
if ($previousDllHash -ne 'D6BA7C43E37357D38AD14017F55BFC1FD5F5531B0441CDB55978F626FAC386EB') { throw 'Installed v30 DLL differs from accepted baseline.' }
$proxyHash = Digest $ordinaryProxy
if ($proxyHash -ne '78FB4C43B56DC2D4A50EBC7B4940943A61B3FC3D4EB533AB6911EB2F816FD48D') { throw 'Ordinary proxy baseline differs.' }
$tableHash = Digest $inputTable
if ($tableHash -ne '26D4A85C658F5D527A5F36CD65B84EE93ED2BD9B811A82F236BDF05D0A4C8CE9') { throw 'Accepted input table differs.' }
$candidateHash = Digest $sourceCandidate
if ($candidateHash -ne 'D2966770EC7D5EB72BB29A1C9F96775E51D955BE2750BA4E2CE50F723D20E107') { throw 'Offline body candidate SHA differs.' }
foreach ($file in @('candidate-smoke.txt','reload-smoke.txt','shape-smoke.txt','loading-smoke.txt')) {
  $testResult = Get-Content -LiteralPath (Join-Path $evidenceRoot $file) -Raw
  if (-not $testResult.StartsWith('PASS')) { throw "Required regression did not pass: $file" }
}
foreach ($marker in @('capture-observation.enabled','shape-capture-boundaries.enabled','shape-capture-gpu.enabled')) {
  if (Test-Path -LiteralPath (Join-Path $diagnosticRoot $marker)) { throw "Capture marker unexpectedly enabled: $marker" }
}
if ((Test-Path -LiteralPath $rollbackRoot) -or (Test-Path -LiteralPath $candidateRoot) -or (Test-Path -LiteralPath $installedSpec)) {
  throw 'v31 deployment destination already exists; inspect it before retrying.'
}
New-Item -ItemType Directory -Path $rollbackRoot | Out-Null
foreach ($file in @($installedDll,$inputTable,(Join-Path $pluginRoot 'eff.ini'),$observationMarker)) {
  if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $rollbackRoot }
}
New-Item -ItemType Directory -Path $candidateRoot | Out-Null
$candidatePath = Join-Path $candidateRoot 'input-040.vfs'
Copy-Item -LiteralPath $sourceCandidate -Destination $candidatePath
Copy-Item -LiteralPath $sourceSpec -Destination $installedSpec
if ((Digest $candidatePath) -ne $candidateHash -or (Digest $installedSpec) -ne (Digest $sourceSpec)) { throw 'Staged candidate/spec verification failed.' }
[IO.File]::WriteAllText($observationMarker,"build=resource-directory-entry-v31-native-body-candidate`r`nstage=R1a-native-body-candidate`r`ngfxRequested=false`r`ncandidateProductionPublished=false`r`n",[Text.UTF8Encoding]::new($false))
$dllHash = Digest $buildDll
$stagedDll = Join-Path $pluginRoot 'eff.dll.v31-staging'
if (Test-Path -LiteralPath $stagedDll) { throw 'DLL staging destination already exists.' }
Copy-Item -LiteralPath $buildDll -Destination $stagedDll
AssertStopped
[IO.File]::Replace($stagedDll,$installedDll,(Join-Path $rollbackRoot 'eff.atomic-backup.dll'),$false)
if ((Digest $installedDll) -ne $dllHash -or (Digest $inputTable) -ne $tableHash -or (Digest $ordinaryProxy) -ne $proxyHash) {
  throw 'Post-deployment DLL/proxy/table verification failed.'
}
$deployment = [ordered]@{
  format='EFF_NATIVE_BODY_CANDIDATE_DEPLOYMENT';version=1
  build='resource-directory-entry-v31-native-body-candidate';installed=$true
  deployedAt=(Get-Date -Format o);gameStoppedAtDeployment=$true
  dll=$installedDll;dllSha256=$dllHash;previousDllSha256=$previousDllHash
  ordinaryBuild=$true;gfxtraceLoadDisabled=$true;proxySha256=$proxyHash;inputTableSha256=$tableHash
  inputTableHashUnchanged=$true;inputRows=60;rollback=$rollbackRoot
  candidate=$candidatePath;candidateSha256=$candidateHash;candidateBytes=(Get-Item -LiteralPath $candidatePath).Length
  config=$installedSpec;configSha256=(Digest $installedSpec)
  candidateScope='dependency-free body Mesh only; no Renderer or instance migration'
  trigger='F10 after entering the same LZY world scene';requiredSceneStaySeconds=20
  preparationWorkerDiskOnly=$true;nativeOperationsUnityFrameOnly=$true;asyncRequestsHeldToCompletion=$true
  originalNativeBundleAndMeshIdentityChecked=$true;unverifiedBundleOwnershipPreventsUnload=$true
  runtimeNativeArrayAndShapeParityChecked=$true;repeatTriggerDeduplicated=$true
  ownBundleUnloadAllLoadedObjects=$false;candidateMeshWitnessRetained=$true
  candidateLifecycleRegressionPassed=$true;configurationReloadRegressionPassed=$true
  shapeAndCaptureBridgeRegressionPassed=$true;nativeLoadingObservationRegressionPassed=$true
  runtimeObservationReceived=$false;nativeBodyWitnessVerified=$false
  sharedDependencyLifetimeVerified=$false;materialTexturePrefabCandidateVerified=$false
  generationCacheIsolationVerified=$false;existingInstanceResourceMigrationVerified=$false
  completeResourceHotReloadImplemented=$false;productionTablePublished=$false;blenderChanged=$false
}
$deployment | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'deployment.json') -Encoding utf8
$deployment | ConvertTo-Json -Depth 8
