$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOutput = [IO.Path]::GetFullPath((Join-Path $taskRoot 'bin/source-preparer'))
& dotnet publish (Join-Path $PSScriptRoot 'StaticModSources/StaticModSources.csproj') -c Release -r win-x64 -p:DebugType=None -p:RestoreSources=https://api.nuget.org/v3/index.json -o $taskOutput
if ($LASTEXITCODE -ne 0) { throw 'Author source preparer failed to publish' }
