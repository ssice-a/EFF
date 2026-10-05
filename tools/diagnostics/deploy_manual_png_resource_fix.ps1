$ErrorActionPreference = 'Stop'
$taskWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$taskGame = 'D:\Hypergryph Launcher\games\Endfield Game'
$taskPlugin = Join-Path $taskGame 'plugin'
$taskAuthor = Join-Path $taskPlugin 'mods\LZY'
$taskEvidence = Join-Path $taskWorkspace 'diagnostics\resource-directory-entry-20261005\v34-direct-full-reload'
$taskRollback = Join-Path $taskPlugin 'resource-input-diagnostics\rollback\v34b-manual-png-files-final'
$taskBuild = 'resource-directory-entry-v34b-manual-png-files'
function Digest($path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AtomicBackup($target,$name) {
 $folder = if ([IO.Path]::GetPathRoot($target) -eq [IO.Path]::GetPathRoot($taskRollback)) { $taskRollback } else { $taskEvidence }
 Join-Path $folder ('v34b-' + $name)
}
function AssertStopped {
 $running = Get-CimInstance Win32_Process | Where-Object {
  $_.Name -match '^Endfield.*\.exe$|^eff_resource_pack\.exe$|^eff_texture_convert\.exe$' -or
  ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($taskGame + '\', [StringComparison]::OrdinalIgnoreCase))
 }
 if ($running) { throw 'Game or resource tool is running; deployment refused.' }
}
AssertStopped
if (Test-Path -LiteralPath $taskRollback) { throw 'Follow-up deployment already started; inspect its evidence before retrying.' }
$dllSource = Join-Path $taskWorkspace 'bin\resource-directory-entry\eff.dll'
$packSource = Join-Path $taskWorkspace 'bin\resource-pack\eff_resource_pack.exe'
$converterSource = Join-Path $taskWorkspace 'bin\texture-convert\eff_texture_convert.exe'
$dllTarget = Join-Path $taskPlugin 'eff.dll'
if ((Digest $dllTarget) -ne 'B6A70F26B106AD6BEFCB9EB1A71111E265B19852306248DB99DF8095320C6995') { throw 'Installed DLL differs from recorded v34.' }
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($dllSource)).Contains($taskBuild)) { throw 'DLL build marker differs.' }
foreach ($log in @('dll-build-followup.log','compiler-build-followup.log','converter-build-followup.log','loader-build-followup.log','transaction-build-followup.log')) {
 if ((Get-Content -LiteralPath (Join-Path $taskEvidence $log) -Raw) -match 'error C\d+|fatal error|error LNK') { throw "Build failed: $log" }
}
foreach ($test in @('production-loader-followup.txt','transaction-smoke-followup.txt','converter-followup-test.txt','rename-reload-test.txt')) {
 if (-not (Get-Content -LiteralPath (Join-Path $taskEvidence $test) -Raw).StartsWith('PASS ')) { throw "Verification failed: $test" }
}
$renamed = Get-Content -LiteralPath (Join-Path $taskEvidence 'rename-reload-verification.json') -Raw | ConvertFrom-Json
if (-not $renamed.passed -or -not $renamed.installedAuthorUnchanged) { throw 'Texture rename/reload regression failed.' }
$protected = [ordered]@{}
foreach ($relative in @('eff.ini','resource-input-substitutions.tsv','resource-reload-baseline\baseline.tsv','resource-reload-baseline\assembly.sha256','resource-reload-baseline\source-ranges.tsv','resource-reload-cache\current.tsv')) {
 $protected[$relative] = Digest (Join-Path $taskPlugin $relative)
}
$proxySHA = Digest (Join-Path $taskGame 'vulkan-1.dll')
if ($proxySHA -ne '78FB4C43B56DC2D4A50EBC7B4940943A61B3FC3D4EB533AB6911EB2F816FD48D') { throw 'Ordinary proxy differs.' }
if (-not ((Get-Content -LiteralPath (Join-Path $taskPlugin 'eff.ini') -Raw) -match '(?m)^\s*disable_camera_fade\s*=\s*true\s*$')) { throw 'Anti-fade disabled.' }
foreach ($flag in @('gfxtrace-capture.enabled','native-candidate-gfxtrace.enabled','full-reload-gfxtrace.enabled','runtime-shapes-gfxtrace.enabled')) {
 if (Test-Path -LiteralPath (Join-Path $taskPlugin "resource-input-diagnostics\$flag")) { throw "GFX marker enabled: $flag" }
}
$authorBefore = [ordered]@{}
foreach ($file in Get-ChildItem -LiteralPath $taskAuthor -Recurse -File) {
 $relative = $file.FullName.Substring($taskAuthor.Length + 1)
 if ($relative -ne 'textures\eff_texture_convert.exe') { $authorBefore[$relative] = Digest $file.FullName }
}
if ((Digest (Join-Path $taskWorkspace 'tools\nativepack\ini_reader.py')) -ne (Digest (Join-Path $taskWorkspace 'tools\Blender\nativepack\ini_reader.py'))) { throw 'Blender reader differs from canonical source.' }
$pairs = @(
 @($packSource,(Join-Path $taskPlugin 'tools\eff_resource_pack.exe'),'game-compiler.before.exe'),
 @($packSource,(Join-Path $taskWorkspace 'tools\Blender\nativepack\eff_resource_pack.exe'),'blender-compiler.before.exe'),
 @($converterSource,(Join-Path $taskPlugin 'tools\eff_texture_convert.exe'),'tools-converter.before.exe'),
 @($converterSource,(Join-Path $taskAuthor 'textures\eff_texture_convert.exe'),'portable-converter.before.exe'),
 @($dllSource,$dllTarget,'eff.v34.dll')
)
New-Item -ItemType Directory -Path $taskRollback | Out-Null
foreach ($pair in $pairs) { Copy-Item -LiteralPath $pair[1] -Destination (Join-Path $taskRollback $pair[2]) }
$installed = [System.Collections.Generic.List[object]]::new()
try {
 foreach ($pair in $pairs) {
  $stage = $pair[1] + '.v34b-staging'
  if (Test-Path -LiteralPath $stage) { throw "Unexpected staging file: $stage" }
  AssertStopped
  Copy-Item -LiteralPath $pair[0] -Destination $stage
  if ((Digest $stage) -ne (Digest $pair[0])) { throw 'Staged SHA differs.' }
  AssertStopped
  [IO.File]::Replace($stage,$pair[1],(AtomicBackup $pair[1] ($pair[2]+'.atomic-before')),$false)
  $installed.Add($pair)
  if ((Digest $pair[1]) -ne (Digest $pair[0])) { throw 'Installed SHA differs.' }
 }
 AssertStopped
 & (Join-Path $taskPlugin 'tools\eff_resource_pack.exe') $taskAuthor (Join-Path $taskPlugin 'resource-reload-baseline') (Join-Path $taskPlugin 'resource-reload-cache') (Join-Path $taskGame 'GameAssembly.dll') *> (Join-Path $taskEvidence 'compile-installed-auxiliary.log')
 if ($LASTEXITCODE -ne 0) { throw 'Installed compiler rejected actual Mod.' }
 & (Join-Path $taskWorkspace 'bin\resource-directory-entry\offline_loader_smoke.exe') $taskGame *> (Join-Path $taskEvidence 'production-loader-installed.txt')
 if ($LASTEXITCODE -ne 0) { throw 'Installed startup/F10 disk validation failed.' }
 foreach ($relative in $protected.Keys) { if ((Digest (Join-Path $taskPlugin $relative)) -ne $protected[$relative]) { throw "Protected file changed: $relative" } }
 foreach ($relative in $authorBefore.Keys) { if ((Digest (Join-Path $taskAuthor $relative)) -ne $authorBefore[$relative]) { throw "Author file changed: $relative" } }
 if ((Digest (Join-Path $taskGame 'vulkan-1.dll')) -ne $proxySHA) { throw 'Proxy changed.' }
 AssertStopped
 $report = [ordered]@{
  format='EFF_MANUAL_PNG_RESOURCE_FIX_DEPLOYMENT';version=1;build=$taskBuild;installed=$true;deployedAt=(Get-Date -Format o)
  dll=$dllTarget;dllSHA256=(Digest $dllTarget);compiler=(Join-Path $taskPlugin 'tools\eff_resource_pack.exe');compilerSHA256=(Digest $packSource);compilerABI='EFF_NATIVE_PACK_V34_1'
  blenderCompiler=(Join-Path $taskWorkspace 'tools\Blender\nativepack\eff_resource_pack.exe');blenderReaderSHA256=(Digest (Join-Path $taskWorkspace 'tools\Blender\nativepack\ini_reader.py'))
  converter=(Join-Path $taskPlugin 'tools\eff_texture_convert.exe');portableConverter=(Join-Path $taskAuthor 'textures\eff_texture_convert.exe');converterSHA256=(Digest $converterSource)
  auxiliaryFilesAllowed=@('.png','.psd','eff_texture_convert.exe');sameNameTextureOutput=$true;sharedINIPathsUpdated=$true;oldTexturesArchivedOutsideMod=$true
  renamedTextureProductionF10DiskRegressionPassed=$true;startupPreviousCompleteFallback=$true;actualModCompilerPassed=$true;protectedHashes=$protected
  authorResourcesAndPNGUnchanged=$true;userPNGConverted=$false;gameStoppedAtDeployment=$true;gfxtraceLoadDisabled=$true;cameraAntiFadeEnabled=$true;proxySHA256=$proxySHA
  blenderSceneModified=$false;nativeGameAcceptance=$false;rollback=$taskRollback
 }
 $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $taskEvidence 'deployment-auxiliary.json') -Encoding utf8
 Write-Output "DEPLOYED $taskBuild; actual Mod accepted with manual PNG/EXE files; user game test pending."
} catch {
 $failure = $_
 $failure | Out-String | Set-Content -LiteralPath (Join-Path $taskEvidence 'deployment-auxiliary-error.txt') -Encoding utf8
 AssertStopped
 for ($i=$installed.Count-1;$i -ge 0;$i--) {
  $pair=$installed[$i];$stage=$pair[1]+'.v34b-rollback-staging'
  Copy-Item -LiteralPath (Join-Path $taskRollback $pair[2]) -Destination $stage
  [IO.File]::Replace($stage,$pair[1],(AtomicBackup $pair[1] ($pair[2]+'.rejected')),$false)
 }
 throw $failure
}
