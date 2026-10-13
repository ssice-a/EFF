@echo off
setlocal
pushd "%~dp0..\.."
set "perf_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%perf_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto perf_failed
if not exist bin\load-performance mkdir bin\load-performance
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /DEIEM_LOAD_PERF /Fo"bin\load-performance\smoke.obj" tools\diagnostics\load_performance_smoke.cpp /Fe"bin\load-performance\smoke.exe"
set "perf_result=%errorlevel%"
popd
exit /b %perf_result%
:perf_failed
popd
exit /b 1
