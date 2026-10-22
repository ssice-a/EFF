$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../bin/multi-mod-collector-test'))
$fixture = Join-Path $root ([Guid]::NewGuid().ToString('N'))
$game = Join-Path $fixture 'game'
New-Item -ItemType Directory -Path (Join-Path $game 'plugin/mods/A/resources') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $game 'plugin/mods/B/resources') -Force | Out-Null
$resource = New-Object byte[] 84
[Text.Encoding]::ASCII.GetBytes('EFFRES').CopyTo($resource,0)
[BitConverter]::GetBytes([uint32]3).CopyTo($resource,12)
[BitConverter]::GetBytes([uint64]84).CopyTo($resource,40)
$resource[80] = 1
$sha = [Security.Cryptography.SHA256]::Create()
try {$sha.ComputeHash([byte[]]$resource[80..83]).CopyTo($resource,48)} finally {$sha.Dispose()}
foreach($mod in @('A','B')){
    $modRoot = Join-Path $game ('plugin/mods/'+$mod)
    [IO.File]::WriteAllText((Join-Path $modRoot 'mod.ini'),"[Mod]`nformat=2`nid=Duplicate`n[ResourceTexture]`npath=resources/shared.tex`n")
    [IO.File]::WriteAllBytes((Join-Path $modRoot 'resources/shared.tex'),$resource)
    [IO.File]::WriteAllBytes((Join-Path $modRoot 'compiled.bin'),[byte[]](1,2,3))
}
$log = Join-Path $game 'plugin/eff_log.txt'
[IO.File]::WriteAllText($log,'[ENTRY-FATAL] startup rejected reason=duplicate/composed Mod id: Duplicate')
$before = (Get-FileHash -LiteralPath $log).Hash
$zip = Join-Path $fixture 'test.zip'
& (Join-Path $PSScriptRoot 'Collect-EFFMultiMod.ps1') -GameDirectory $game -OutputZip $zip -NonInteractive
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $entry=$archive.GetEntry('report.json')
    $reader=New-Object IO.StreamReader ($entry.Open())
    try {$report=$reader.ReadToEnd()|ConvertFrom-Json} finally {$reader.Dispose()}
    if($report.mods.Count -ne 2 -or @($report.issues | Where-Object type -EQ duplicate-mod-id).Count -ne 1){throw 'Duplicate Mod IDs were not recorded'}
    if($report.sharedDeclaredPayloads.Count -ne 1){throw 'Identical author payload group was not recorded'}
    if(@($archive.Entries | Where-Object {$_.FullName -match '(\.tex|compiled\.bin)$'}).Count){throw 'Collector copied author payloads'}
    if(-not $archive.GetEntry('000-mod.ini') -or -not $archive.GetEntry('eff_log.txt')){throw 'Collector omitted source evidence'}
} finally {$archive.Dispose()}
if((Get-FileHash -LiteralPath $log).Hash -ne $before){throw 'Collector changed game input'}
Write-Output 'PASS collector: duplicate IDs and identical payload headers detected; evidence ZIP verified; game files unchanged; no author payloads copied'
