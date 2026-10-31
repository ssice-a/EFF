@echo off
setlocal
pushd "%~dp0..\.."
set "eff_demand_physics_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_demand_physics_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\demand-physics mkdir bin\demand-physics
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\demand-physics\smoke.obj" tools\diagnostics\demand_physics_smoke.cpp /Fe"bin\demand-physics\smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\demand-physics\smoke.exe --overlap-only
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
