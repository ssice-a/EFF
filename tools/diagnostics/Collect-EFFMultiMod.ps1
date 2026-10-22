#requires -Version 5.1
param(
    [string]$GameDirectory,
    [string]$OutputZip,
    [switch]$NonInteractive
)
$ErrorActionPreference = 'Stop'

function Read-SharedBytes([string]$Path, [int]$Count = 0) {
    $inputStream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        if ($Count -gt 0) {
            $buffer = New-Object byte[] ([Math]::Min($Count, $inputStream.Length))
            $position = 0
            while ($position -lt $buffer.Length) {
                $read = $inputStream.Read($buffer, $position, $buffer.Length - $position)
                if ($read -eq 0) { break }
                $position += $read
            }
            if ($position -ne $buffer.Length) { throw "File changed while reading: $Path" }
            return ,$buffer
        }
        $memory = New-Object IO.MemoryStream
        try { $inputStream.CopyTo($memory); return ,$memory.ToArray() } finally { $memory.Dispose() }
    } finally { $inputStream.Dispose() }
}

function Hex([byte[]]$Bytes) { return [BitConverter]::ToString($Bytes).Replace('-', '') }
function Bytes-Hash([byte[]]$Bytes) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { return Hex ($algorithm.ComputeHash($Bytes)) } finally { $algorithm.Dispose() }
}

if (-not $GameDirectory) {
    # Copying the collector into plugin also works without selecting a folder.
    $candidate = Split-Path -Parent $PSScriptRoot
    if ((Test-Path -LiteralPath (Join-Path $candidate 'Endfield_Data')) -and
        (Test-Path -LiteralPath (Join-Path $candidate 'plugin/mods'))) { $GameDirectory = $candidate }
}
if (-not $GameDirectory -and -not $NonInteractive) {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Select the Endfield game directory containing Endfield.exe and plugin.'
    try { if ($dialog.ShowDialog() -eq 'OK') { $GameDirectory = $dialog.SelectedPath } } finally { $dialog.Dispose() }
}
if (-not $GameDirectory) { throw 'No game directory selected.' }
$gameRoot = [IO.Path]::GetFullPath($GameDirectory).TrimEnd('\', '/')
$pluginRoot = Join-Path $gameRoot 'plugin'
$modsRoot = Join-Path $pluginRoot 'mods'
if (-not (Test-Path -LiteralPath $modsRoot -PathType Container)) { throw 'Selected directory has no plugin/mods folder.' }
if (-not $OutputZip) {
    $desktop = [Environment]::GetFolderPath('Desktop')
    $OutputZip = Join-Path $desktop ('EFF-MultiMod-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,6) + '.zip')
}
$OutputZip = [IO.Path]::GetFullPath($OutputZip)
if ($OutputZip.StartsWith($gameRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Choose an output outside the game directory.' }
if (Test-Path -LiteralPath $OutputZip) { throw 'Output already exists; choose a new ZIP filename.' }
$stageParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
$stage = Join-Path $stageParent ('EFF-MultiMod-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($stage) | Out-Null
try {
    $issues = New-Object 'Collections.Generic.List[object]'
    $mods = New-Object 'Collections.Generic.List[object]'
    $ids = New-Object 'Collections.Generic.Dictionary[string,Collections.Generic.List[string]]' ([StringComparer]::Ordinal)
    foreach ($folder in @(Get-ChildItem -LiteralPath $modsRoot -Directory | Sort-Object Name)) {
        $iniPath = Join-Path $folder.FullName 'mod.ini'
        if (-not (Test-Path -LiteralPath $iniPath -PathType Leaf)) { continue }
        $iniBytes = Read-SharedBytes $iniPath
        $text = [Text.Encoding]::UTF8.GetString($iniBytes).TrimStart([char]0xFEFF)
        $section = ''; $modId = ''; $resourcePaths = @(); $targets = @(); $materials = @()
        foreach ($line in ($text -split '\r?\n')) {
            $clean = $line.Trim()
            if ($clean -match '^\[([^\]]+)\]$') { $section = $Matches[1]; continue }
            if ($clean -match '^(?:;|#)' -or -not $clean) { continue }
            if ($section -eq 'Mod' -and $clean -match '^id\s*=\s*(.+)$') { $modId = $Matches[1].Trim() }
            if ($section -like 'Resource*' -and $clean -match '^path\s*=\s*(.+)$') { $resourcePaths += $Matches[1].Trim() }
            if ($section -like 'Replace*' -and $clean -match '^(?:source|target|mesh|asset|lod\d+)\s*=') { $targets += ($section + ': ' + $clean) }
            if ($section -like 'Material*' -and $clean -match '^(?:source|asset)\s*=') { $materials += ($section + ': ' + $clean) }
        }
        $label = '{0:D3}' -f $mods.Count
        [IO.File]::WriteAllBytes((Join-Path $stage ($label + '-mod.ini')), $iniBytes)
        if (-not $modId) { $issues.Add(@{type='missing-mod-id';folder=$folder.Name}) }
        else {
            if (-not $ids.ContainsKey($modId)) { $ids[$modId] = New-Object 'Collections.Generic.List[string]' }
            $ids[$modId].Add($folder.Name)
        }
        $resources = @()
        foreach ($relative in $resourcePaths) {
            $path = [IO.Path]::GetFullPath((Join-Path $folder.FullName $relative))
            $base = [IO.Path]::GetFullPath($folder.FullName).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
            if (-not $path.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) { $issues.Add(@{type='resource-path-escape';folder=$folder.Name;path=$relative}); continue }
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { $issues.Add(@{type='missing-author-resource';folder=$folder.Name;path=$relative}); continue }
            $header = Read-SharedBytes $path 80
            $valid = $header.Length -eq 80 -and [Text.Encoding]::ASCII.GetString($header,0,6) -eq 'EFFRES'
            $resources += @{path=$relative;bytes=(Get-Item -LiteralPath $path).Length;resourceHeader=$valid;
                kind=$(if($valid){[BitConverter]::ToUInt32($header,12)}else{$null});
                declaredBodySha256=$(if($valid){Hex ([byte[]]$header[48..79])}else{$null});bodyHashVerified=$false}
        }
        $compiledPath = Join-Path $folder.FullName 'compiled.bin'; $compiled = $null
        if (Test-Path -LiteralPath $compiledPath -PathType Leaf) {
            $info = Get-Item -LiteralPath $compiledPath
            $compiled = @{bytes=$info.Length;lastWriteUtc=$info.LastWriteTimeUtc.ToString('O');sha256=(Get-FileHash -LiteralPath $compiledPath -Algorithm SHA256).Hash}
        } else { $issues.Add(@{type='missing-compiled-bin';folder=$folder.Name}) }
        $mods.Add(@{folder=$folder.Name;id=$modId;iniSha256=(Bytes-Hash $iniBytes);compiled=$compiled;resources=$resources;targets=$targets;materialSources=$materials})
    }
    foreach ($id in $ids.Keys) { if($ids[$id].Count -gt 1) { $issues.Add(@{type='duplicate-mod-id';id=$id;folders=@($ids[$id]);meaning='Current runtime rejects the full candidate, not just one object.'}) } }
    $shared = @($mods | ForEach-Object { $mod = $_; foreach($resource in $_.resources){if($resource.declaredBodySha256){[pscustomobject]@{folder=$mod.folder;path=$resource.path;kind=$resource.kind;hash=$resource.declaredBodySha256}}} } |
        Group-Object kind,hash | Where-Object { @($_.Group.folder | Sort-Object -Unique).Count -gt 1 } |
        ForEach-Object { @{kind=$_.Group[0].kind;declaredBodySha256=$_.Group[0].hash;uses=@($_.Group);meaning='Identical author bytes alone are not an ID collision.'} })
    $copied = @()
    foreach ($item in @(
        @{source=(Join-Path $pluginRoot 'eff_log.txt');name='eff_log.txt'},
        @{source=(Join-Path $pluginRoot 'eff.ini');name='eff.ini'},
        @{source=(Join-Path $env:USERPROFILE 'AppData/LocalLow/Hypergryph/Endfield/Player.log');name='Player.log'},
        @{source=(Join-Path $env:USERPROFILE 'AppData/LocalLow/Hypergryph/Endfield/Player-prev.log');name='Player-prev.log'})) {
        if(Test-Path -LiteralPath $item.source -PathType Leaf){
            $info=Get-Item -LiteralPath $item.source
            [IO.File]::WriteAllBytes((Join-Path $stage $item.name),(Read-SharedBytes $item.source))
            $copied += @{name=$item.name;lastWriteUtc=$info.LastWriteTimeUtc.ToString('O');bytes=$info.Length}
        }
    }
    $dllPath = Join-Path $pluginRoot 'eff.dll'; $dll = $null
    if(Test-Path -LiteralPath $dllPath -PathType Leaf){$dll=@{sha256=(Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash;bytes=(Get-Item -LiteralPath $dllPath).Length}}
    $report = @{format='EFF_MULTI_MOD_DIAGNOSTIC/1';timeUtc=[DateTime]::UtcNow.ToString('O');
        gameDirectory=$gameRoot;effDll=$dll;mods=@($mods.ToArray());issues=@($issues.ToArray());sharedDeclaredPayloads=$shared;logs=$copied;
        note='Read-only collection. No game files changed. Compiled/resource payloads are not included; package and shader/lifecycle validation still requires the affected packages and failing session.'}
    [IO.File]::WriteAllText((Join-Path $stage 'report.json'),($report | ConvertTo-Json -Depth 14),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $stage 'READ-ME.txt'),
        "Reproduce with both Mods installed, then run this collector before restarting the game. Send this ZIP with the two character/Mod names, plugin version, whether each Mod works alone, and whether the issue happens before F10. Logs and mod.ini may contain local paths; review before sharing. No model/texture or compiled.bin payload is included.")
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($OutputZip)) | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($stage,$OutputZip)
    Write-Output ("EFF diagnostic ZIP: " + $OutputZip)
    Write-Output ("Mods: " + $mods.Count + "; structural findings: " + $issues.Count + "; matching author payload groups: " + $shared.Count)
} finally {
    # Delete only this explicitly created temporary collection directory.
    $resolvedStage = [IO.Path]::GetFullPath($stage)
    if ([IO.Path]::GetDirectoryName($resolvedStage) -eq $stageParent -and
        [IO.Path]::GetFileName($resolvedStage) -match '^EFF-MultiMod-[a-f0-9]{32}$' -and
        (Test-Path -LiteralPath $resolvedStage)) { Remove-Item -LiteralPath $resolvedStage -Recurse -Force }
}
