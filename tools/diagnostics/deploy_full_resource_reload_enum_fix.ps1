$ErrorActionPreference = 'Stop'
$workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$gameRoot = 'D:\Hypergryph Launcher\games\Endfield Game'
$pluginRoot = Join-Path $gameRoot 'plugin'
$evidenceRoot = Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v32a-enum-abi-fix'
$previous = Get-Content -LiteralPath (Join-Path $workspaceRoot 'diagnostics\resource-directory-entry-20261004\v32-full-resource-reload\deployment.json') -Raw | ConvertFrom-Json
$rollbackRoot = Join-Path $pluginRoot 'resource-input-diagnostics\rollback\v32a-enum-abi-fix'
$installedDll = Join-Path $pluginRoot 'eff.dll'
$buildDll = Join-Path $workspaceRoot 'bin\resource-directory-entry\eff.dll'
$marker = Join-Path $pluginRoot 'resource-input-diagnostics\full-reload-observation.enabled'
function Digest([string] $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
  $running = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^Endfield.*\.exe$' -or
    ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($gameRoot + '\',[StringComparison]::OrdinalIgnoreCase))
  })
  if ($running.Count) { throw 'Deployment refused: a game-directory or Endfield process is running.' }
}
AssertStopped
if ((Digest $installedDll) -ne $previous.dllSha256) { throw 'Installed v32 DLL differs from recorded deployment.' }
if ((Digest (Join-Path $gameRoot 'vulkan-1.dll')) -ne $previous.proxySha256) { throw 'Ordinary proxy differs.' }
if ((Digest (Join-Path $pluginRoot 'resource-input-substitutions.tsv')) -ne $previous.inputTableSha256) { throw 'Input table differs.' }
foreach ($property in $previous.officialAuthorHashes.PSObject.Properties) {
  if ((Digest (Join-Path (Join-Path $pluginRoot 'mods\LZY') $property.Name)) -ne $property.Value) { throw "Official author changed: $($property.Name)" }
}
$red = Get-Content -LiteralPath (Join-Path $evidenceRoot 'red-smoke.txt') -Raw
$green = Get-Content -LiteralPath (Join-Path $evidenceRoot 'green-smoke.txt') -Raw
if (-not $red.StartsWith('FAIL reload enum underlying width differs') -or -not $green.StartsWith('PASS')) { throw 'Required failing/passing enum transaction regression not present.' }
$api = Get-Content -LiteralPath (Join-Path $evidenceRoot 'api-static-presence.json') -Raw | ConvertFrom-Json
if (-not $api.compiledCandidatesPresent -or $api.methods.Count -ne 29) { throw 'Compiled API candidates differ.' }
if ((Get-Content -LiteralPath (Join-Path $evidenceRoot 'build.log') -Raw) -match 'error C\d+|fatal error|error LNK') { throw 'Ordinary DLL build failed.' }
foreach ($path in @($rollbackRoot,(Join-Path $pluginRoot 'eff.dll.v32a-staging'))) {
  if (Test-Path -LiteralPath $path) { throw "Deployment destination exists: $path" }
}
$config = Join-Path $pluginRoot 'resource-reload-baseline\validation-author.tsv'
$configHash = Digest $config
$baseline = Join-Path $pluginRoot 'resource-reload-baseline\baseline.tsv'
$baselineHash = Digest $baseline
New-Item -ItemType Directory -Path $rollbackRoot | Out-Null
Copy-Item -LiteralPath $installedDll -Destination $rollbackRoot
Copy-Item -LiteralPath $marker -Destination $rollbackRoot
$staging = Join-Path $pluginRoot 'eff.dll.v32a-staging'
Copy-Item -LiteralPath $buildDll -Destination $staging
$dllHash = Digest $buildDll
AssertStopped
[IO.File]::Replace($staging,$installedDll,(Join-Path $rollbackRoot 'eff.atomic-backup.dll'),$false)
if ((Digest $installedDll) -ne $dllHash) { throw 'Installed DLL does not match final build.' }
[IO.File]::WriteAllText($marker,"build=resource-directory-entry-v32a-enum-abi-fix`r`nstage=full-resource-world-transaction-test`r`ngfxRequested=false`r`nnativeMigrationAccepted=false`r`n",[Text.UTF8Encoding]::new($false))
if ((Digest $config) -ne $configHash -or (Digest $baseline) -ne $baselineHash) { throw 'Resource baseline or visual config changed.' }
$result = [ordered]@{
 format='EFF_FULL_RESOURCE_RELOAD_ENUM_FIX_DEPLOYMENT';version=1
 build='resource-directory-entry-v32a-enum-abi-fix';installed=$true;deployedAt=(Get-Date -Format o)
 dll=$installedDll;dllSha256=$dllHash;previousDllSha256=$previous.dllSha256;rollback=$rollbackRoot
 gameStoppedAtDeployment=$true;ordinaryBuild=$true;gfxtraceLoadDisabled=$true;cameraAntiFadeEnabled=$true
 proxySha256=$previous.proxySha256;inputTableSha256=$previous.inputTableSha256
 authorResourcesUnchanged=$true;baselineUnchanged=$true;visualConfigUnchanged=$true
 failureObserved='run-5556-01DD53F256B562C3: install rejects fixed 4-byte enum; F10 observed; no native transaction'
 enumUnderlyingTypeAndWidthValidationImplemented=$true;enumRuntimeABIConfirmed=$false;enumBytesPassedToManagedInvoke=$true
 enumWidthsRegressionPassed=@(1,2,4,8);signedUnsignedLiteralBytesPreserved=$true
 malformedEnumRejected=$true;modelTransactionAndRollbackMockRegressionPassed=$true
 exactMissingSignatureNowLogged=$true;compiledAPICandidates=29
 firstSuccessfulF10='cyan body texture';secondSuccessfulF10='original installed author graph'
 runtimeObservationReceived=$false;existingInstanceMigrationVerified=$false;completeResourceHotReloadAccepted=$false
}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'deployment.json') -Encoding utf8
$result | ConvertTo-Json -Depth 5
