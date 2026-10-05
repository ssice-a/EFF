$ErrorActionPreference = 'Stop'
$taskWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$taskGame = 'D:\Hypergryph Launcher\games\Endfield Game'
$taskPlugin = Join-Path $taskGame 'plugin'
$taskEvidence = Join-Path $taskWorkspace 'diagnostics\resource-directory-entry-20261005\v34-direct-full-reload'
$taskRollback = Join-Path $taskPlugin 'resource-input-diagnostics\rollback\v34-direct-full-reload-final'
$taskBuild = 'resource-directory-entry-v34-direct-full-reload'
function Digest($path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
 $running = Get-CimInstance Win32_Process | Where-Object {
  $_.Name -match '^Endfield.*\.exe$|^eff_resource_pack\.exe$' -or
  ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($taskGame + '\', [StringComparison]::OrdinalIgnoreCase))
 }
 if ($running) { throw 'Game or resource compiler is running; deployment refused.' }
}
function AtomicInstall($source, $target, $outsideBackup = '') {
 $stage = $target + '.v34-staging'
 $backup = if ($outsideBackup) { $outsideBackup } else { $target + '.v34-atomic-before' }
 if ((Test-Path -LiteralPath $stage) -or (Test-Path -LiteralPath $backup)) { throw "Unexpected deployment staging: $target" }
 Copy-Item -LiteralPath $source -Destination $stage
 if ((Digest $stage) -ne (Digest $source)) { throw "Staged SHA differs: $target" }
 AssertStopped
 if (Test-Path -LiteralPath $target) { [IO.File]::Replace($stage, $target, $backup, $false) }
 else { [IO.File]::Move($stage, $target) }
 if ((Digest $target) -ne (Digest $source)) { throw "Installed SHA differs: $target" }
}
AssertStopped
if (Test-Path -LiteralPath $taskRollback) { throw 'Deployment already started; inspect rollback and deployment record before retrying.' }
$dllSource = Join-Path $taskWorkspace 'bin\resource-directory-entry\eff.dll'
$packSource = Join-Path $taskWorkspace 'bin\resource-pack\eff_resource_pack.exe'
$dllTarget = Join-Path $taskPlugin 'eff.dll'
$packTarget = Join-Path $taskPlugin 'tools\eff_resource_pack.exe'
$blenderTarget = Join-Path $taskWorkspace 'tools\Blender\nativepack\eff_resource_pack.exe'
if ((Digest $dllTarget) -ne 'D52931A1A26BD069E28F3F40BBE9C2E8C0DCC984CD7317202AD08023F18DE593') { throw 'Installed DLL differs from recorded v33a.' }
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($dllSource)).Contains($taskBuild)) { throw 'DLL build marker differs.' }
foreach ($log in @('dll-build-final.log', 'compiler-build.log', 'loader-build.log')) {
 if ((Get-Content -LiteralPath (Join-Path $taskEvidence $log) -Raw) -match 'error C\d+|fatal error|error LNK') { throw "Build failed: $log" }
}
$differential = Get-Content -LiteralPath (Join-Path $taskEvidence 'geometry-cache-verification.json') -Raw | ConvertFrom-Json
if (-not $differential.coldWarmBytesEqual -or $differential.comparedFiles -ne 136 -or -not $differential.invalidAuthorRetainsPreviousCurrent) { throw 'Real resource differential failed.' }
foreach ($test in @('transaction-smoke.txt', 'native-payload-verification.txt', 'payload-pixels.txt', 'payload-material.txt', 'payload-controls.txt', 'payload-mesh.txt')) {
 if (-not (Get-Content -LiteralPath (Join-Path $taskEvidence $test) -Raw).StartsWith('PASS ')) { throw "Local validation failed: $test" }
}
$protected = [ordered]@{}
foreach ($relative in @('eff.ini', 'resource-input-substitutions.tsv', 'resource-reload-baseline\baseline.tsv', 'resource-reload-baseline\assembly.sha256', 'resource-reload-baseline\source-ranges.tsv', 'tools\eff_texture_convert.exe')) { $protected[$relative] = Digest (Join-Path $taskPlugin $relative) }
$proxySHA = Digest (Join-Path $taskGame 'vulkan-1.dll')
if ($proxySHA -ne '78FB4C43B56DC2D4A50EBC7B4940943A61B3FC3D4EB533AB6911EB2F816FD48D') { throw 'Ordinary proxy differs.' }
if (-not ((Get-Content -LiteralPath (Join-Path $taskPlugin 'eff.ini') -Raw) -match '(?m)^\s*disable_camera_fade\s*=\s*true\s*$')) { throw 'Camera anti-fade disabled.' }
foreach ($flag in @('gfxtrace-capture.enabled', 'native-candidate-gfxtrace.enabled', 'full-reload-gfxtrace.enabled', 'runtime-shapes-gfxtrace.enabled')) {
 if (Test-Path -LiteralPath (Join-Path $taskPlugin "resource-input-diagnostics\$flag")) { throw "GFX marker enabled: $flag" }
}
$author = Join-Path $taskPlugin 'mods\LZY'
$authorBefore = [ordered]@{}
foreach ($file in Get-ChildItem -LiteralPath $author -Recurse -File) { $authorBefore[$file.FullName.Substring($author.Length + 1)] = Digest $file.FullName }
$texture = Join-Path $author 'textures\body_010de4985b31.tex'
$originalTexture = Join-Path $taskWorkspace 'diagnostics\resource-directory-entry-20261005\v33-different-resources\body-texture.original.tex'
if ((Digest $texture) -ne 'AFA1B74C9AA44A413958E30447E8AE16A13444F357789B793535EEBCA199891C' -or (Digest $originalTexture) -ne 'B230DF4F4AB7675DBCC518BDAB738D5F2D5F0DEBCA6166D0E98324349C3FFA15') { throw 'Temporary texture/original witness changed; restore refused.' }
New-Item -ItemType Directory -Path $taskRollback | Out-Null
Copy-Item -LiteralPath $dllTarget -Destination (Join-Path $taskRollback 'eff.v33a.dll')
Copy-Item -LiteralPath $packTarget -Destination (Join-Path $taskRollback 'game-compiler.before.exe')
Copy-Item -LiteralPath $blenderTarget -Destination (Join-Path $taskRollback 'blender-compiler.before.exe')
Copy-Item -LiteralPath $texture -Destination (Join-Path $taskRollback 'body-texture.cyan.tex')
$cache = Join-Path $taskPlugin 'resource-reload-cache'
Copy-Item -LiteralPath (Join-Path $cache 'current.tsv') -Destination (Join-Path $taskRollback 'current.before.tsv')
try {
 AtomicInstall $originalTexture $texture (Join-Path $taskRollback 'body-texture.atomic-before.tex')
 # Geometry files were independently compared to a fresh cold build. No
 # generation is copied: its absolute loading table must be produced here.
 $geometrySeed = Join-Path $taskEvidence 'cache\geometry'
 $geometryTarget = Join-Path $cache 'geometry'
 New-Item -ItemType Directory -Path $geometryTarget -Force | Out-Null
 foreach ($folder in Get-ChildItem -LiteralPath $geometrySeed -Directory) {
  $target = Join-Path $geometryTarget $folder.Name
  if (-not (Test-Path -LiteralPath $target)) { Copy-Item -LiteralPath $folder.FullName -Destination $target -Recurse }
 }
 & $packSource $author (Join-Path $taskPlugin 'resource-reload-baseline') $cache (Join-Path $taskGame 'GameAssembly.dll') *> (Join-Path $taskEvidence 'compile-production.log')
 if ($LASTEXITCODE -ne 0) { throw 'Production offline package failed.' }
 & (Join-Path $taskWorkspace 'bin\resource-pack\inspect_offline.exe') $cache $author *> (Join-Path $taskEvidence 'production-manifest.txt')
 if ($LASTEXITCODE -ne 0) { throw 'Production manifest failed.' }
 & (Join-Path $taskWorkspace 'bin\resource-directory-entry\offline_loader_smoke.exe') $taskGame *> (Join-Path $taskEvidence 'production-loader.txt')
 if ($LASTEXITCODE -ne 0) { throw 'Actual startup table or offline worker failed.' }
 $pointer = (Get-Content -LiteralPath (Join-Path $cache 'current.tsv'))[1]
 $key = $pointer.Split("`t")[0]
 & (Join-Path $taskWorkspace 'bin\resource-pack\inspect_payloads.exe') (Join-Path $cache "generations\$key") (Split-Path -Parent $author) *> (Join-Path $taskEvidence 'production-native-payloads.txt')
 if ($LASTEXITCODE -ne 0) { throw 'Production native payloads failed.' }
 AtomicInstall $packSource $packTarget
 AtomicInstall $packSource $blenderTarget
 AtomicInstall $dllSource $dllTarget
 foreach ($relative in $protected.Keys) { if ((Digest (Join-Path $taskPlugin $relative)) -ne $protected[$relative]) { throw "Protected file changed: $relative" } }
 foreach ($relative in $authorBefore.Keys) {
  $expected = if ($relative -eq 'textures\body_010de4985b31.tex') { Digest $originalTexture } else { $authorBefore[$relative] }
  if ((Digest (Join-Path $author $relative)) -ne $expected) { throw "Unexpected author mutation: $relative" }
 }
 if ((Digest (Join-Path $taskGame 'vulkan-1.dll')) -ne $proxySHA) { throw 'Proxy changed.' }
 AssertStopped
 $report = [ordered]@{
  format='EFF_DIRECT_FULL_RESOURCE_RELOAD_DEPLOYMENT'; version=1; build=$taskBuild; installed=$true; deployedAt=(Get-Date -Format o)
  dll=$dllTarget; dllSha256=(Digest $dllTarget); compiler=$packTarget; compilerSha256=(Digest $packTarget); compilerABI='EFF_NATIVE_PACK_V34_1'
  blenderCompiler=$blenderTarget; blenderCompilerSHA256=(Digest $blenderTarget); blenderOfflineModule=(Join-Path $taskWorkspace 'tools\Blender\eiem_offline_reload.py')
  current=$pointer; startupTable=(Join-Path $cache "generations\$key\loading-inputs.tsv"); offlineNativeInputs=68
  stableWorldGate=$false; nativeLoadOnlyAfterF10=$true; runtimeTypeTreeBuild=$false; startupUsesLatestCompleteGeneration=$true
  allResourceKindsImplemented=@('Mesh','Material','Texture','INI','keys','visibility','shape bindings'); nativeWorldAndDisplayTransactionMockPassed=$true
  idlePoolPathsInvalidated=$true; oldBundleRetirement='Unity AssetBundle.Unload(false)'; persistentPoolMigrationSupported=$false
  sourcePerMeshBonesShapesAndMaterialTextureSelectionsVerified=$true; temporaryCyanTextureRestored=$true; otherAuthorFilesUnchanged=$true
  cameraAntiFadeEnabled=$true; gfxtraceLoadDisabled=$true; proxySHA256=$proxySHA; protectedFileHashes=$protected; gameStoppedAtDeployment=$true; rollback=$taskRollback
  liveBlenderAvailable=$false; blenderSceneModified=$false; nativeMigrationVerified=$false; completeResourceHotReloadAccepted=$false
 }
 $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $taskEvidence 'deployment.json') -Encoding utf8
 Write-Output "DEPLOYED $taskBuild; F10 native game acceptance pending user test."
} catch {
 $deploymentFailure = $_
 $deploymentFailure | Out-String | Set-Content -LiteralPath (Join-Path $taskEvidence 'deployment-error.txt') -Encoding utf8
 # Roll back only our deployment artifacts; retain immutable evidence/cache.
 AssertStopped
 foreach ($pair in @(@($dllTarget,'eff.v33a.dll'),@($packTarget,'game-compiler.before.exe'),@($blenderTarget,'blender-compiler.before.exe'),@($texture,'body-texture.cyan.tex'),@((Join-Path $cache 'current.tsv'),'current.before.tsv'))) {
  $rollbackStage = $pair[0] + '.v34-rollback-staging'
  Copy-Item -LiteralPath (Join-Path $taskRollback $pair[1]) -Destination $rollbackStage
  [IO.File]::Replace($rollbackStage,$pair[0],(Join-Path $taskRollback ($pair[1]+'.rejected')),$false)
 }
 throw $deploymentFailure
}
