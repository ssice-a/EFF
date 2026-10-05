$ErrorActionPreference = 'Stop'
$taskWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$taskGame = 'D:\Hypergryph Launcher\games\Endfield Game'
$taskPlugin = Join-Path $taskGame 'plugin'
$taskAuthor = Join-Path $taskPlugin 'mods\LZY'
$taskEvidence = Join-Path $taskWorkspace 'diagnostics\texture-image-names-20261005'
$taskPrepared = Join-Path $taskEvidence 'prepared\LZY'
$taskRollback = Join-Path $taskPlugin ('resource-input-diagnostics\rollback\image-names-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$taskPending = Join-Path $taskPlugin ('mods\LZY.pending-' + [guid]::NewGuid().ToString())
$taskCurrent = Join-Path $taskPlugin 'resource-reload-cache\current.tsv'
function Digest($path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function AssertStopped {
    if (Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^Endfield.*\.exe$|^eff_resource_pack\.exe$' }) { throw 'Game or compiler running.' }
}
AssertStopped
$proof = Get-Content -LiteralPath (Join-Path $taskEvidence 'rename-verification.json') -Raw | ConvertFrom-Json
$live = Get-Content -LiteralPath (Join-Path $taskEvidence 'live-verification.json') -Raw | ConvertFrom-Json
if (-not $proof.success -or -not $live.success) { throw 'Rename verification failed.' }
foreach ($property in $proof.originalHashes.PSObject.Properties) {
    if ((Digest (Join-Path $taskAuthor $property.Name)) -ne $property.Value) { throw "Author changed: $($property.Name)" }
}
New-Item -ItemType Directory -Path $taskRollback | Out-Null
Copy-Item -LiteralPath $taskCurrent -Destination (Join-Path $taskRollback 'current.before.tsv')
Copy-Item -LiteralPath $taskPrepared -Destination $taskPending -Recurse
$moved = $false
$published = $false
try {
    & (Join-Path $taskPlugin 'tools\eff_resource_pack.exe') $taskPrepared (Join-Path $taskPlugin 'resource-reload-baseline') (Join-Path $taskPlugin 'resource-reload-cache') (Join-Path $taskGame 'GameAssembly.dll') *> (Join-Path $taskEvidence 'compile.log')
    if ($LASTEXITCODE -ne 0) { throw 'Native compilation failed.' }
    AssertStopped
    foreach ($property in $proof.originalHashes.PSObject.Properties) {
        if ((Digest (Join-Path $taskAuthor $property.Name)) -ne $property.Value) { throw "Author changed: $($property.Name)" }
    }
    Move-Item -LiteralPath $taskAuthor -Destination (Join-Path $taskRollback 'LZY')
    $moved = $true
    Move-Item -LiteralPath $taskPending -Destination $taskAuthor
    $published = $true
    & (Join-Path $taskWorkspace 'bin\resource-directory-entry\offline_loader_smoke.exe') $taskGame *> (Join-Path $taskEvidence 'installed-loader.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Installed startup/F10 disk verification failed.' }
    $key = ((Get-Content -LiteralPath $taskCurrent)[1] -split "`t")[0]
    & (Join-Path $taskWorkspace 'bin\resource-pack\inspect_payloads.exe') (Join-Path $taskPlugin "resource-reload-cache\generations\$key") (Join-Path $taskPlugin 'mods') (Join-Path $taskPlugin 'resource-reload-baseline') *> (Join-Path $taskEvidence 'installed-native.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Installed native payload verification failed.' }
    [ordered]@{installed=$true;key=$key;rollback=$taskRollback;texFiles=$proof.texFiles;pixelBytesUnchanged=$true;visualAcceptance=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskEvidence 'deployment.json') -Encoding utf8
    Write-Output "PASS image-name TEX deployed key=$key"
} catch {
    $reason = $_
    AssertStopped
    if ($published) { Move-Item -LiteralPath $taskAuthor -Destination (Join-Path $taskRollback 'failed-LZY') }
    if ($moved) { Move-Item -LiteralPath (Join-Path $taskRollback 'LZY') -Destination $taskAuthor }
    Copy-Item -LiteralPath (Join-Path $taskRollback 'current.before.tsv') -Destination $taskCurrent -Force
    throw $reason
}
