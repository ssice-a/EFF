@echo off
setlocal
pushd "%~dp0..\.."
set "adaptation_jobs_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%adaptation_jobs_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto adaptation_jobs_failed
if not exist bin\source-adaptation-jobs mkdir bin\source-adaptation-jobs
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /DEFF_OFFLINE_PACKAGE_V35=1 /Fo"bin\source-adaptation-jobs\smoke.obj" tools\diagnostics\source_adaptation_jobs_smoke.cpp /Fe"bin\source-adaptation-jobs\smoke.exe" /link bcrypt.lib
set "adaptation_jobs_result=%errorlevel%"
popd
exit /b %adaptation_jobs_result%
:adaptation_jobs_failed
popd
exit /b 1
