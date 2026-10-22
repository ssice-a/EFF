@echo off
setlocal
pushd "%~dp0..\.."
set "mesh_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%mesh_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-pack-v35 mkdir bin\resource-pack-v35
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /DEFF_OFFLINE_PACKAGE_V35=1 /Fo"bin\resource-pack-v35\incremental_mesh_smoke.obj" tools\diagnostics\incremental_mesh_smoke.cpp /Fe"bin\resource-pack-v35\incremental_mesh_smoke.exe" /link bcrypt.lib
set "mesh_result=%errorlevel%"
popd
exit /b %mesh_result%
