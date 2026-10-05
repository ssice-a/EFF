$ErrorActionPreference = 'Stop'
$taskWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$taskGame = [IO.Path]::GetFullPath('D:\Hypergryph Launcher\games\Endfield Game')
$taskPlugin = [IO.Path]::GetFullPath((Join-Path $taskGame 'plugin'))
$taskSource = [IO.Path]::GetFullPath((Join-Path $taskWorkspace 'bin\v35c-lifecycle-probe\eff.dll'))
$taskTarget = [IO.Path]::GetFullPath((Join-Path $taskPlugin 'eff.dll'))
$taskEvidence = Join-Path $taskWorkspace 'diagnostics\f10-native-lifecycle-audit-20261005\probe-v35c-deployment-v2.json'
$taskRollback = Join-Path $taskPlugin ('resource-input-diagnostics\rollback\v35c-lifecycle-probe-v2-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$taskStage = Join-Path $taskPlugin 'eff.v35c-probe-staging.dll'
if (-not $taskTarget.StartsWith($taskPlugin + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Target escaped plugin directory.' }
if (-not $taskRollback.StartsWith($taskPlugin + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Rollback escaped plugin directory.' }
function Digest([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
  $running = Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^Endfield.*\.exe$|^eff_resource_pack\.exe$' -or
    ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($taskGame + '\', [StringComparison]::OrdinalIgnoreCase))
  }
  if ($running) { throw 'Game or resource compiler is running; deployment refused.' }
}
AssertStopped
if (-not (Test-Path -LiteralPath $taskSource) -or -not (Test-Path -LiteralPath $taskTarget)) { throw 'Built or installed DLL absent.' }
if (Test-Path -LiteralPath $taskStage) { throw 'Unexpected old staging file.' }
$taskBefore = Digest $taskTarget
if ($taskBefore -ne '992194A6FE77062DFFE71E742325C5D2FB0E889236C6B405D555FABE0B0E4D83') { throw 'Installed DLL differs from recorded v35c probe.' }
$taskBuilt = Digest $taskSource
if ($taskBuilt -ne '5B5F7F0BA06422B538BBBC341D963967056ABE63D14239B6543DC394AF51F812') { throw 'Built DLL differs from verified v2 probe.' }
$taskBytes = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($taskSource))
if (-not $taskBytes.Contains('EFF_NATIVE_PACK_V35_1') -or $taskBytes.Contains('EFF_NATIVE_PACK_V36_ASSEMBLY_1') -or
    -not $taskBytes.Contains('resource-directory-entry-v35c-lifecycle-probe')) { throw 'Probe ABI or build marker differs.' }
$taskProtected = [ordered]@{}
foreach ($relative in @('eff.ini', 'tools\eff_resource_pack.exe', 'resource-reload-cache\current.tsv', 'mods\LZY\mod.ini', 'mods\LZY\textures\body.tex')) {
  $taskProtected[$relative] = Digest (Join-Path $taskPlugin $relative)
}
$taskProtected['..\vulkan-1.dll'] = Digest (Join-Path $taskGame 'vulkan-1.dll')
New-Item -ItemType Directory -Path $taskRollback | Out-Null
Copy-Item -LiteralPath $taskSource -Destination $taskStage
if ((Digest $taskStage) -ne $taskBuilt) { throw 'Staged DLL hash differs.' }
AssertStopped
$taskBackup = Join-Path $taskRollback 'eff.v35c-before.dll'
[IO.File]::Replace($taskStage, $taskTarget, $taskBackup, $false)
if ((Digest $taskTarget) -ne $taskBuilt -or (Digest $taskBackup) -ne $taskBefore) { throw 'Installed or backup DLL hash differs.' }
foreach ($relative in $taskProtected.Keys) {
  $path = if ($relative -eq '..\vulkan-1.dll') { Join-Path $taskGame 'vulkan-1.dll' } else { Join-Path $taskPlugin $relative }
  if ((Digest $path) -ne $taskProtected[$relative]) { throw "Protected file changed: $relative" }
}
AssertStopped
$taskReport = [ordered]@{
  build = 'resource-directory-entry-v35c-lifecycle-probe-v2'
  installed = $true
  installedPath = $taskTarget
  previousSHA256 = $taskBefore
  dllSHA256 = $taskBuilt
  rollback = $taskBackup
  protectedHashes = $taskProtected
  nativeLifecycleAcceptance = $false
  timestamp = (Get-Date -Format o)
}
$taskReport | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $taskEvidence -Encoding utf8
Write-Output "DEPLOYED v35c lifecycle probe SHA256=$taskBuilt"
