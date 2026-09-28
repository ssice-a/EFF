<#
Extract Endfield's VFS with the current external EndfieldUnpacker tool.

The extractor is deliberately outside eff.dll. It only reads the game's VFS
payloads and writes a versioned working directory.

Example:
  .\tools\extract_endfield_vfs.ps1 `
    -GamePath 'D:\Hypergryph Launcher\games\Endfield Game' `
    -ExtractorPath 'C:\tools\EndfieldUnpacker' `
    -OutputPath 'D:\EndfieldExtracts\v1'
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string] $GamePath,

  [Parameter(Mandatory = $true)]
  [string] $ExtractorPath,

  [Parameter(Mandatory = $true)]
  [string] $OutputPath,

  [string] $PythonPath = 'python'
)

$ErrorActionPreference = 'Stop'

function Resolve-ExistingPath([string] $Path, [string] $Description) {
  $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
  if ($null -eq $resolved) {
    throw "$Description was not found: $Path"
  }
  return $resolved.Path
}

$gameRoot = Resolve-ExistingPath $GamePath 'Game directory'
$extractorRoot = Resolve-ExistingPath $ExtractorPath 'EndfieldUnpacker directory'
$scriptPath = Resolve-ExistingPath (Join-Path $extractorRoot 'decrypt_vfs.py') 'decrypt_vfs.py'
$gameVfs = Join-Path $gameRoot 'Endfield_Data\StreamingAssets\VFS'
if (-not (Test-Path -LiteralPath $gameVfs -PathType Container)) {
  throw "StreamingAssets VFS was not found: $gameVfs"
}

# decrypt_vfs.py reads this cache through its sibling config.py.  Keeping the
# cache in the external tool directory avoids putting game paths in EIEM.
Set-Content -LiteralPath (Join-Path $extractorRoot '.game_dir') -Value $gameRoot -Encoding ascii
$outputRoot = [System.IO.Path]::GetFullPath($OutputPath)
$extractorOutput = Join-Path $extractorRoot 'DecryptOutput'
if (Test-Path -LiteralPath $extractorOutput) {
  $existingOutput = Get-Item -LiteralPath $extractorOutput -Force
  if (($existingOutput.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0) {
    throw "Extractor output path exists and is not a junction: $extractorOutput"
  }
  [System.IO.Directory]::Delete($extractorOutput, $false)
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

# decrypt_vfs.py has a fixed DecryptOutput name. Point that name at the caller's
# output directory so a full extraction never consumes the extractor's volume.
New-Item -ItemType Junction -Path $extractorOutput -Target $outputRoot | Out-Null

Push-Location $extractorRoot
try {
  & $PythonPath $scriptPath extract
  if ($LASTEXITCODE -ne 0) {
    throw "EndfieldUnpacker failed with exit code $LASTEXITCODE"
  }
}
finally {
  Pop-Location
  if (Test-Path -LiteralPath $extractorOutput) {
    $createdOutput = Get-Item -LiteralPath $extractorOutput -Force
    if (($createdOutput.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
      [System.IO.Directory]::Delete($extractorOutput, $false)
    }
  }
}

$files = @(Get-ChildItem -LiteralPath $outputRoot -Recurse -File)

$report = [ordered]@{
  schemaVersion = 1
  gamePath = $gameRoot
  extractor = $scriptPath
  layer = 'StreamingAssets'
  fileCount = $files.Count
  byteCount = ($files | Measure-Object Length -Sum).Sum
  generatedAtUtc = [DateTime]::UtcNow.ToString('o')
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputRoot 'extraction-report.json') -Encoding utf8

Write-Host ("Extraction complete: files={0}, bytes={1}, output={2}" -f
  $files.Count, $report.byteCount, $outputRoot)
