$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$utilityProject = Join-Path $taskRoot 'tools/AnimeStudio/AnimeStudio.Utility/AnimeStudio.Utility.csproj'
& dotnet build $utilityProject -c Release -f net9.0 -p:TargetFrameworks=net9.0
if ($LASTEXITCODE -ne 0) { throw 'AnimeStudio source dependencies failed to build' }
$taskOutput = [IO.Path]::GetFullPath((Join-Path $taskRoot 'bin/static-sources'))
& dotnet publish (Join-Path $PSScriptRoot 'EndfieldVfsProbe/EndfieldVfsProbe.csproj') -c Release -r win-x64 --self-contained true -p:DebugType=None -o $taskOutput
if ($LASTEXITCODE -ne 0) { throw 'Static source tool failed to publish' }
foreach ($taskSymbol in Get-ChildItem -LiteralPath $taskOutput -Filter '*.pdb' -File -Recurse) {
    if (-not $taskSymbol.FullName.StartsWith($taskOutput + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Symbol path escaped build output' }
    Remove-Item -LiteralPath $taskSymbol.FullName
}
