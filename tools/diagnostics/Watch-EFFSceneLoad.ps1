[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$GameDirectory='D:\Hypergryph Launcher\games\Endfield Game',
    [ValidateRange(10,1800)][int]$MaxSeconds=600
)
$ErrorActionPreference='Stop'
$outputRoot=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $outputRoot){throw 'Output directory already exists.'}
[void](New-Item -ItemType Directory -Path $outputRoot)
$utf8=New-Object Text.UTF8Encoding($false)
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class EFFSceneCounters {
 [StructLayout(LayoutKind.Sequential)] public struct Memory {
  public uint cb;
  public UIntPtr CommitTotal,CommitLimit,CommitPeak,PhysicalTotal,PhysicalAvailable,SystemCache,KernelTotal,KernelPaged,KernelNonpaged,PageSize;
  public uint HandleCount,ProcessCount,ThreadCount;
 }
 [StructLayout(LayoutKind.Sequential)] public struct Io {
  public ulong ReadOperations,WriteOperations,OtherOperations,ReadBytes,WriteBytes,OtherBytes;
 }
 [DllImport("psapi.dll",SetLastError=true)] public static extern bool GetPerformanceInfo(ref Memory info,uint size);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool GetSystemTimes(out long idle,out long kernel,out long user);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool GetProcessIoCounters(IntPtr process,out Io io);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr OpenProcess(uint access,bool inherit,int processId);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll")] public static extern ulong GetTickCount64();
}
'@
function Write-JsonLine([string]$Name,$Value){
 [IO.File]::AppendAllText((Join-Path $outputRoot $Name),(($Value|ConvertTo-Json -Compress -Depth 8)+"`n"),$utf8)
}
function Save-Logs([string]$Label){
 $sources=@{eff=(Join-Path $GameDirectory 'plugin\eff_log.txt');player=(Join-Path $env:USERPROFILE 'AppData\LocalLow\Hypergryph\Endfield\Player.log')}
 foreach($name in $sources.Keys){
  if(-not(Test-Path -LiteralPath $sources[$name] -PathType Leaf)){continue}
  $source=$null;$destination=$null
  try{
   $source=[IO.File]::Open($sources[$name],[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
   $destination=[IO.File]::Open((Join-Path $outputRoot "$Label-$name.log"),[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read)
   $source.CopyTo($destination)
  }finally{if($destination){$destination.Dispose()};if($source){$source.Dispose()}}
 }
}
$dll=Join-Path $GameDirectory 'plugin\eff.dll'
Write-JsonLine 'events.jsonl' @{kind='collector-start';at=[DateTimeOffset]::Now.ToString('o');uptimeMs=[EFFSceneCounters]::GetTickCount64();logicalProcessors=[Environment]::ProcessorCount;dllSha256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash;changesGameFiles=$false}
Save-Logs 'before'
$watch=[Diagnostics.Stopwatch]::StartNew()
$previous=$null;$previousCpu=$null;$seenGame=$false;$endReason='timeout';$nextCim=0
try{
 while($watch.Elapsed.TotalSeconds -lt $MaxSeconds){
  if(Test-Path -LiteralPath (Join-Path $outputRoot 'stop.request')){$endReason='stop-request';break}
  $sample=[ordered]@{at=[DateTimeOffset]::Now.ToString('o');uptimeMs=[EFFSceneCounters]::GetTickCount64();gamePresent=$false;game=$null;system=$null}
  $memory=New-Object EFFSceneCounters+Memory
  $memory.cb=[Runtime.InteropServices.Marshal]::SizeOf($memory)
  if([EFFSceneCounters]::GetPerformanceInfo([ref]$memory,$memory.cb)){
   $page=$memory.PageSize.ToUInt64()
   $sample.system=@{availableBytes=$memory.PhysicalAvailable.ToUInt64()*$page;totalPhysicalBytes=$memory.PhysicalTotal.ToUInt64()*$page;commitBytes=$memory.CommitTotal.ToUInt64()*$page;commitLimitBytes=$memory.CommitLimit.ToUInt64()*$page;cpuPercent=$null}
  }
  [long]$idle=0;[long]$kernel=0;[long]$user=0
  if([EFFSceneCounters]::GetSystemTimes([ref]$idle,[ref]$kernel,[ref]$user)){
   if($previousCpu -and $sample.system){$total=($kernel-$previousCpu.kernel)+($user-$previousCpu.user);if($total -gt 0){$sample.system.cpuPercent=[Math]::Round(100*(1-($idle-$previousCpu.idle)/$total),2)}}
   $previousCpu=@{idle=$idle;kernel=$kernel;user=$user}
  }
  $games=@(Get-Process -Name Endfield -ErrorAction SilentlyContinue)
  if($games.Count -gt 1){throw 'Multiple game processes; cannot assign samples.'}
  if($games.Count -eq 1){
   $game=$games[0];$sample.gamePresent=$true;$seenGame=$true
   try{
    $game.Refresh();$cpu=$game.TotalProcessorTime.TotalSeconds
    $io=New-Object EFFSceneCounters+Io
    $ioHandle=[EFFSceneCounters]::OpenProcess(0x1000,$false,$game.Id)
    $hasIo=$false
    try{if($ioHandle -ne [IntPtr]::Zero){$hasIo=[EFFSceneCounters]::GetProcessIoCounters($ioHandle,[ref]$io)}}
    finally{if($ioHandle -ne [IntPtr]::Zero){[void][EFFSceneCounters]::CloseHandle($ioHandle)}}
    $sample.game=@{processId=$game.Id;cpuTotalSeconds=$cpu;cpuPercent=$null;privateBytes=$game.PrivateMemorySize64;workingSetBytes=$game.WorkingSet64;readBytes=$null;writeBytes=$null;readBytesPerSecond=$null}
    if($hasIo){$sample.game.readBytes=$io.ReadBytes;$sample.game.writeBytes=$io.WriteBytes}
    if($previous -and $previous.processId -eq $game.Id){
     $seconds=($sample.uptimeMs-$previous.uptimeMs)/1000.0
     if($seconds -gt 0){
      $sample.game.cpuPercent=[Math]::Round(100*($cpu-$previous.cpuSeconds)/$seconds/[Environment]::ProcessorCount,2)
      if($hasIo -and $null -ne $previous.readBytes){$sample.game.readBytesPerSecond=($io.ReadBytes-$previous.readBytes)/$seconds}
     }
    }else{Write-JsonLine 'events.jsonl' @{kind='game-observed';processId=$game.Id;at=$sample.at;uptimeMs=$sample.uptimeMs}}
    $previous=@{processId=$game.Id;uptimeMs=$sample.uptimeMs;cpuSeconds=$cpu;readBytes=$sample.game.readBytes}
   }catch{$sample.game=@{processId=$game.Id;error=$_.Exception.Message};$previous=$null}
   finally{$game.Dispose()}
  }else{
   $previous=$null
   if($seenGame){Write-JsonLine 'samples.jsonl' $sample;$endReason='game-exited';break}
  }
  Write-JsonLine 'samples.jsonl' $sample
  if($watch.Elapsed.TotalSeconds -ge $nextCim){
   $nextCim=$watch.Elapsed.TotalSeconds+5
   try{
    $mem=Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory
    $disks=@(Get-CimInstance Win32_PerfFormattedData_PerfDisk_LogicalDisk | Where-Object {$_.Name -in @('D:','E:')} | Select-Object Name,DiskReadBytesPersec,DiskWriteBytesPersec,CurrentDiskQueueLength)
    Write-JsonLine 'system-io.jsonl' @{at=[DateTimeOffset]::Now.ToString('o');uptimeMs=[EFFSceneCounters]::GetTickCount64();pagesInputPerSecond=$mem.PagesInputPersec;pageReadsPerSecond=$mem.PageReadsPersec;disks=$disks;wholeSystem=$true}
   }catch{Write-JsonLine 'events.jsonl' @{kind='system-io-error';message=$_.Exception.Message}}
  }
  Start-Sleep -Milliseconds 1000
 }
}finally{
 Save-Logs 'after'
 Write-JsonLine 'events.jsonl' @{kind='collector-end';reason=$endReason;at=[DateTimeOffset]::Now.ToString('o');uptimeMs=[EFFSceneCounters]::GetTickCount64()}
}
Write-Output $outputRoot
