# Save the live EFF log under a label so two launches can be compared.
#
# The plugin opens plugin\eff_log.txt with CREATE_ALWAYS at every startup, so
# each launch overwrites the previous one. Run this right after you exit the
# game, before starting it again.
#
# Usage:
#   .\tools\save-eff-log.ps1 good
#   .\tools\save-eff-log.ps1 bad
#
# Output goes to E:\EFF_Workspace\diagnostics\logs\<label>-<timestamp>\
#   eff_log.txt   the full log
#   summary.txt    the lines that decide whether this run was healthy

param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Label
)

$ErrorActionPreference = 'Stop'

$pluginLog = 'D:\Hypergryph Launcher\games\Endfield Game\plugin\eff_log.txt'
if (-not (Test-Path $pluginLog)) {
    Write-Error "Not found: $pluginLog"
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$dest = "E:\EFF_Workspace\diagnostics\logs\$Label-$stamp"
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item $pluginLog "$dest\eff_log.txt" -Force

$log = "$dest\eff_log.txt"
$out = "$dest\summary.txt"

$patterns = [ordered]@{
    'BUILD'              = '\[BUILD\]'
    'PARTNER-OWNERSHIP'  = 'MOD-PARTNER-OWNERSHIP'
    'REG-SUCCESS'        = 'MOD-REG\] stage'
    'REG-GAP'            = 'MOD-REG-GAP'
    'REG-FAIL'           = 'MOD-REG-FAIL'
    'BASE-ARRAY'         = 'MOD-BASE-ARRAY'
    'ROOTBONE-INFO'      = 'MOD-ROOTBONE-INFO'
    'ASSEMBLY-PROBE'     = 'ASSEMBLY-PROBE-v114'
    'MODEL-INSTANCE'     = 'INSTANCE-REG-v3\] event=model'
    'PARTNER-COMMIT'     = 'PARTNER-COMMIT-v102'
    'SKELETON-BIND'      = 'SKELETON-BIND\]'
    'SKELETON-FALLBACK'  = 'SKELETON-BIND-FALLBACK'
    'SKELETON-GEN'       = 'event=skeleton-(cache|rebuilt)'
    'MOD-LOD'            = 'MOD-LOD\] reconciled'
}

$summary = New-Object System.Collections.Generic.List[string]
$summary.Add("label   : $Label")
$summary.Add("captured: $stamp")
$summary.Add("source  : $pluginLog")
$summary.Add("size    : {0:N2} MB" -f ((Get-Item $log).Length / 1MB))
$summary.Add('')

foreach ($name in $patterns.Keys) {
    $hits = Select-String -Path $log -Pattern $patterns[$name] -ErrorAction SilentlyContinue
    $count = if ($hits) { @($hits).Count } else { 0 }
    $summary.Add(('{0,-18} {1,6}' -f $name, $count))
}

$summary.Add('')
$summary.Add('--- PARTNER-OWNERSHIP (the decisive lines) ---')
$own = Select-String -Path $log -Pattern 'MOD-PARTNER-OWNERSHIP' -ErrorAction SilentlyContinue
if ($own) { $own | ForEach-Object { $summary.Add($_.Line) } } else { $summary.Add('(none)') }

$summary.Add('')
$summary.Add('--- REG-GAP (distinct) ---')
$gap = Select-String -Path $log -Pattern 'MOD-REG-GAP' -ErrorAction SilentlyContinue
if ($gap) {
    $gap | ForEach-Object { $_.Line } | Sort-Object -Unique | ForEach-Object { $summary.Add($_) }
} else { $summary.Add('(none)') }

$summary | Set-Content -Path $out -Encoding UTF8

Write-Host "saved to $dest"
Write-Host ''
Get-Content $out
