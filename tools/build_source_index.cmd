@echo off
setlocal
pushd "%~dp0.."
dotnet publish tools\EFF.SourceIndex\EFF.SourceIndex.csproj -c Release -r win-x64 --self-contained true -p:TargetFrameworks=net9.0-windows -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:EnableCompressionInSingleFile=true -o bin\source-index-helper
set "index_build_result=%errorlevel%"
popd
exit /b %index_build_result%
