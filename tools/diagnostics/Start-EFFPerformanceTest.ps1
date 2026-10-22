param(
    [string]$GameDirectory = 'D:\Hypergryph Launcher\games\Endfield Game',
    [string]$OutputDirectory,
    [string]$Notes = '',
    [switch]$SnapshotOnly
)
$ErrorActionPreference = 'Stop'
if (-not ('EFFPerformanceClock' -as [type])) { Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class EFFPerformanceClock {
    [DllImport("kernel32.dll")] public static extern ulong GetTickCount64();
}
'@
}
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$gameRoot = [IO.Path]::GetFullPath($GameDirectory)
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repoRoot ('bin\performance-runs\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
}
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $outputRoot) { throw 'Output directory already exists; choose a new folder.' }
[void](New-Item -ItemType Directory -Path $outputRoot)
$markerPath = Join-Path $outputRoot 'markers.jsonl'
$utf8 = New-Object Text.UTF8Encoding($false)
$sourceLog = Join-Path $gameRoot 'plugin\eff_log.txt'
$activeCase = $null
$cases = @(
    '单角色静置', '多角色同时可见', '多 Mod 安装但场上角色不变',
    'UI 角色反复切换', '大地图往返切换', 'NPC 多实例',
    '固定战斗与技能序列', 'F9 特效更新', 'F10 完整重载', '长时间往返与回收'
)
function Get-GameProcessId {
    $processes = @(Get-Process -Name Endfield -ErrorAction SilentlyContinue)
    if ($processes.Count -gt 1) { throw 'More than one Endfield process; cannot assign a test session safely.' }
    if ($processes.Count -eq 1) { return $processes[0].Id }
    return 0
}
function Write-Marker([hashtable]$value) {
    $value.time = [DateTimeOffset]::Now.ToString('o')
    $value.uptimeMs = [EFFPerformanceClock]::GetTickCount64()
    $value.processId = Get-GameProcessId
    [IO.File]::AppendAllText($markerPath, (($value | ConvertTo-Json -Compress -Depth 8) + "`n"), $utf8)
}
function Save-Log {
    if (-not (Test-Path -LiteralPath $sourceLog -PathType Leaf)) { return }
    $name = 'eff-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [EFFPerformanceClock]::GetTickCount64() + '.log'
    $destination = Join-Path $outputRoot $name
    $inputFile = $null; $outputFile = $null
    try {
        $inputFile = [IO.File]::Open($sourceLog, [IO.FileMode]::Open, [IO.FileAccess]::Read, ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        $outputFile = [IO.File]::Open($destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
        $inputFile.CopyTo($outputFile)
    } finally {
        if ($outputFile) { $outputFile.Dispose() }
        if ($inputFile) { $inputFile.Dispose() }
    }
    [IO.File]::WriteAllText((Join-Path $outputRoot 'latest-log.txt'), $destination, $utf8)
}
function Stop-Case([bool]$aborted = $false) {
    if ($script:activeCase) {
        Write-Marker @{kind='end';runId=$script:activeCase.runId;caseId=$script:activeCase.caseId;aborted=$aborted}
        $script:activeCase = $null
        Save-Log
    }
}
function Get-Sha256([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
    $algorithm = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $stream.Dispose(); $algorithm.Dispose() }
}
$dll = Join-Path $gameRoot 'plugin\eff.dll'
$metadata = @{format='EFF_PERFORMANCE_SESSION';version=1;time=[DateTimeOffset]::Now.ToString('o');game=$gameRoot;
    notes=$Notes;logicalProcessors=[Environment]::ProcessorCount;os=[Environment]::OSVersion.ToString();
    dllSha256=(Get-Sha256 $dll);
    sourceLog=$sourceLog;wholeProcessCounters=$true;changesGameFiles=$false}
[IO.File]::WriteAllText((Join-Path $outputRoot 'session.json'), ($metadata | ConvertTo-Json -Depth 8), $utf8)
Write-Marker @{kind='session';notes=$Notes}
Save-Log
if ($SnapshotOnly) { Write-Output $outputRoot; exit 0 }
Write-Host "记录目录：$outputRoot"
Write-Host '此工具只记录标记并备份日志，不更换 DLL、不改 Mod，也不控制游戏。'
Write-Host '建议先记下 FPS 上限、分辨率、画质、抗锯齿和本轮 Mod/角色数量。'
try {
    while ($true) {
        Write-Host ''
        for ($index = 0; $index -lt $cases.Count; $index++) { Write-Host ("{0}. {1}" -f ($index + 1), $cases[$index]) }
        Write-Host '0. 结束当前场景；N 备注. 标记卡顿/动作；Q. 保存日志并退出'
        $answer = Read-Host '输入编号开始场景'
        if ($answer -match '^[qQ]$') { break }
        if ($answer -eq '0') { Stop-Case; continue }
        if ($answer -match '^[nN]\s+(.+)$') {
            Write-Marker @{kind='note';runId=$(if ($activeCase) {$activeCase.runId} else {''});notes=$Matches[1]}
            continue
        }
        $choice = 0
        if (-not [int]::TryParse($answer,[ref]$choice) -or $choice -lt 1 -or $choice -gt $cases.Count) { continue }
        Stop-Case
        $activeCase = @{runId=[Guid]::NewGuid().ToString('N');caseId=('S{0:00}' -f $choice);name=$cases[$choice - 1]}
        Write-Marker @{kind='begin';runId=$activeCase.runId;caseId=$activeCase.caseId;name=$activeCase.name;notes=$Notes}
        Write-Host '已开始标记。返回游戏先等待 10 秒，再测试；结束后保持场景 10 秒，然后回来输入 0。'
    }
} finally {
    Stop-Case $true
    Save-Log
    Write-Marker @{kind='session-end'}
    Write-Host "已保存：$outputRoot"
}
