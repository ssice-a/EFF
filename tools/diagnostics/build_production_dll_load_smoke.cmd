@echo off
setlocal
pushd "%~dp0..\.."
set "load_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%load_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\production-dll-load\plugin mkdir bin\production-dll-load\plugin
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /Fo"bin\production-dll-load\production_dll_load_smoke.obj" tools\diagnostics\production_dll_load_smoke.cpp /Fe"bin\production-dll-load\production_dll_load_smoke.exe"
set "load_result=%errorlevel%"
popd
exit /b %load_result%
