$ErrorActionPreference = 'Stop'
$files = @(
    Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1' -File
    Get-Item -LiteralPath (Join-Path $PSScriptRoot '..\AnimeStudio\build.ps1')
)
foreach ($file in $files) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($file.FullName, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors.Count -gt 0) {
        $details = $errors | ForEach-Object { "$($file.Name):$($_.Extent.StartLineNumber): $($_.Message)" }
        throw ($details -join [Environment]::NewLine)
    }
    Write-Host "PASS PowerShell parse: $($file.Name)"
}
