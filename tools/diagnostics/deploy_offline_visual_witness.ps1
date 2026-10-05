param([switch] $Restore)
$ErrorActionPreference = 'Stop'
$visualWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$visualGame = 'D:\Hypergryph Launcher\games\Endfield Game'
$visualPlugin = Join-Path $visualGame 'plugin'
$visualAuthor = Join-Path $visualPlugin 'mods\LZY'
$visualEvidence = Join-Path $visualWorkspace 'diagnostics\resource-directory-entry-20261005\v33-different-resources'
$visualCache = Join-Path $visualPlugin 'resource-reload-cache'
$visualCompiler = Join-Path $visualPlugin 'tools\eff_resource_pack.exe'
$visualBuild = Get-Content -LiteralPath (Join-Path $visualWorkspace 'diagnostics\resource-directory-entry-20261005\v33-offline-prewarm\deployment.json') -Raw | ConvertFrom-Json
$visualOfficial = Get-Content -LiteralPath (Join-Path $visualWorkspace 'diagnostics\resource-directory-entry-20261004\v32-full-resource-reload\deployment.json') -Raw | ConvertFrom-Json
function Digest([string] $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
  $processes = @(Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^Endfield.*\.exe$' -or ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($visualGame+'\',[StringComparison]::OrdinalIgnoreCase)) })
  if ($processes.Count) { throw 'Initial witness deployment requires stopped game.' }
}
function CompileInstalled {
  & $visualCompiler $visualAuthor (Join-Path $visualPlugin 'resource-reload-baseline') $visualCache (Join-Path $visualGame 'GameAssembly.dll')
  if ($LASTEXITCODE) { throw 'Installed offline compiler rejected author.' }
}
if ((Digest (Join-Path $visualPlugin 'eff.dll')) -ne $visualBuild.dllSha256 -or (Digest $visualCompiler) -ne $visualBuild.compilerSha256) { throw 'Installed v33 binaries differ.' }
$witness = Get-Content -LiteralPath (Join-Path $visualEvidence 'visual-test\visual-test.json') -Raw | ConvertFrom-Json
$resource = $witness.onlyChangedFile
$target = Join-Path $visualAuthor $resource
$changed = Join-Path (Join-Path $visualEvidence 'LZY') $resource
$backup = Join-Path $visualEvidence 'body-texture.original.tex'
$originalHash = $visualOfficial.officialAuthorHashes.PSObject.Properties | Where-Object { $_.Name.Replace('\','/') -eq $resource.Replace('\','/') } | Select-Object -ExpandProperty Value
if (-not $originalHash) { throw 'Witness resource absent from original author record.' }
$changedHash = Digest $changed
foreach ($row in $visualOfficial.officialAuthorHashes.PSObject.Properties) {
  $expected = $row.Value
  if ($Restore -and $row.Name.Replace('\','/') -eq $resource.Replace('\','/')) { $expected = $changedHash }
  if ((Digest (Join-Path $visualAuthor $row.Name)) -ne $expected) { throw "Author has unrelated changes: $($row.Name)" }
}
$startupBefore = Digest (Join-Path $visualCache 'startup.tsv')
$inputBefore = Digest (Join-Path $visualPlugin 'resource-input-substitutions.tsv')
$proxyBefore = Digest (Join-Path $visualGame 'vulkan-1.dll')
if ($Restore) {
  if (-not (Test-Path -LiteralPath (Join-Path $visualEvidence 'visual-deployment.json')) -or (Digest $backup) -ne $originalHash) { throw 'Verified original witness backup missing.' }
  $restoreStaging = $target + '.v33-restore-staging'
  if (Test-Path -LiteralPath $restoreStaging) { throw 'Restore staging exists.' }
  Copy-Item -LiteralPath $backup -Destination $restoreStaging
  $restoredVisualBackup = Join-Path $visualPlugin 'resource-input-diagnostics\rollback\v33-offline-prewarm\body-texture.cyan-before-restore.tex'
  if (Test-Path -LiteralPath $restoredVisualBackup) { throw 'Previous restore backup exists.' }
  # Atomic backup stays on D: and outside the strict author directory.
  [IO.File]::Replace($restoreStaging,$target,$restoredVisualBackup,$false)
  CompileInstalled
  if ((Get-Content -LiteralPath (Join-Path $visualCache 'current.tsv'))[1] -ne $visualBuild.frozenStartupAndInitialCandidate) { throw 'Restored current is not original generation.' }
  [ordered]@{ restored=$true;restoredAt=(Get-Date -Format o);authorHash=$originalHash;current=(Get-Content -LiteralPath (Join-Path $visualCache 'current.tsv'))[1];inGameCommitVerified=$false } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $visualEvidence 'visual-restoration.json') -Encoding utf8
} else {
  AssertStopped
  $local = Get-Content -LiteralPath (Join-Path $visualEvidence 'visual-local-verification.json') -Raw | ConvertFrom-Json
  if (-not $local.verified -or -not $local.otherAuthorResourcesByteIdentical -or -not $local.alphaPreserved) { throw 'Visible witness offline verification failed.' }
  if ((Get-Content -LiteralPath (Join-Path $visualCache 'current.tsv'))[1] -ne $visualBuild.frozenStartupAndInitialCandidate) { throw 'Unexpected current generation before witness.' }
  $pointer = (Get-Content -LiteralPath (Join-Path $visualEvidence 'cache\current.tsv'))[1]
  $key = $pointer.Split("`t")[0]
  if ($key -eq $visualBuild.frozenStartupAndInitialCandidate.Split("`t")[0]) { throw 'Witness generation equals original.' }
  $sourceGeneration = Join-Path $visualEvidence "cache\generations\$key"
  $installedGeneration = Join-Path $visualCache "generations\$key"
  if ((Test-Path -LiteralPath $backup) -or (Test-Path -LiteralPath $installedGeneration)) { throw 'Witness staging already exists.' }
  Copy-Item -LiteralPath $target -Destination $backup
  Copy-Item -LiteralPath $sourceGeneration -Destination $installedGeneration -Recurse
  $staging = $target + '.v33-cyan-staging'
  if (Test-Path -LiteralPath $staging) { throw 'Witness resource staging exists.' }
  Copy-Item -LiteralPath $changed -Destination $staging
  if ((Digest $staging) -ne $changedHash -or (Digest $backup) -ne $originalHash) { throw 'Witness staging hash differs.' }
  AssertStopped
  # Backup remains outside the strict author directory, on the same D: volume.
  $atomicBackup = Join-Path $visualPlugin 'resource-input-diagnostics\rollback\v33-offline-prewarm\body-texture.before-visual.tex'
  if (Test-Path -LiteralPath $atomicBackup) { throw 'Atomic visual backup exists.' }
  [IO.File]::Replace($staging,$target,$atomicBackup,$false)
  try {
    CompileInstalled
    if ((Get-Content -LiteralPath (Join-Path $visualCache 'current.tsv'))[1] -ne $pointer) { throw 'Published visual generation differs from verified candidate.' }
  } catch {
    $undo = $target + '.v33-undo'
    Copy-Item -LiteralPath $backup -Destination $undo
    [IO.File]::Replace($undo,$target,(Join-Path $visualPlugin 'resource-input-diagnostics\rollback\v33-offline-prewarm\body-texture.rejected-visual.tex'),$false)
    CompileInstalled
    throw
  }
  [ordered]@{
    format='EFF_OFFLINE_VISUAL_WITNESS_DEPLOYMENT';version=1;deployedAt=(Get-Date -Format o);installed=$true
    onlyChangedAuthorFile=$resource;originalHash=$originalHash;installedHash=$changedHash;backup=$backup
    originalPointer=$visualBuild.frozenStartupAndInitialCandidate;candidatePointer=$pointer;startupFrozenOriginal=$true
    allMipsCyan=$true;alphaPreserved=$true;meshBonesShapesAndControlsUnchanged=$true;toushiSelectionUnchanged=$true
    dllUnchanged=$true;compilerUnchanged=$true;originalInputTableUnchanged=$true;gfxDisabled=$true;antiFadeEnabled=$true
    nativeMigrationVerified=$false;visualAcceptanceReported=$false
    restoreCommand='powershell -File tools/diagnostics/deploy_offline_visual_witness.ps1 -Restore'
  } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $visualEvidence 'visual-deployment.json') -Encoding utf8
}
if ((Digest (Join-Path $visualCache 'startup.tsv')) -ne $startupBefore -or (Digest (Join-Path $visualPlugin 'resource-input-substitutions.tsv')) -ne $inputBefore -or (Digest (Join-Path $visualGame 'vulkan-1.dll')) -ne $proxyBefore) { throw 'Frozen startup/proxy/input table changed.' }
'PASS offline visual witness: resource and complete candidate published; game COMMIT pending.'
