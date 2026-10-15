@echo off
setlocal
pushd "%~dp0..\.."
set "pool_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%pool_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\model-pool-discard mkdir bin\model-pool-discard
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /Fo"bin\model-pool-discard\smoke.obj" tools\diagnostics\model_pool_discard_smoke.cpp /Fe"bin\model-pool-discard\smoke.exe"
set "pool_result=%errorlevel%"
popd
exit /b %pool_result%
