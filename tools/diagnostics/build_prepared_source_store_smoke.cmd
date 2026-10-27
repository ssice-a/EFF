@echo off
setlocal
pushd "%~dp0..\.."
set "prepared_source_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%prepared_source_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\consumer-delta mkdir bin\consumer-delta
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\consumer-delta\prepared_source_store_smoke.obj" tools\diagnostics\prepared_source_store_smoke.cpp /Fe"bin\consumer-delta\prepared_source_store_smoke.exe" /link bcrypt.lib cabinet.lib
set "prepared_source_result=%errorlevel%"
popd
exit /b %prepared_source_result%
