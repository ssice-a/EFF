@echo off
setlocal
pushd "%~dp0..\.."
set "physics_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%physics_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\physics-runtime-policy mkdir bin\physics-runtime-policy
cl /nologo /utf-8 /O2 /EHsc /std:c++17 /W4 /Fo"bin\physics-runtime-policy\smoke.obj" tools\diagnostics\physics_runtime_policy_smoke.cpp /Febin\physics-runtime-policy\smoke.exe
if errorlevel 1 goto failed
bin\physics-runtime-policy\smoke.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
