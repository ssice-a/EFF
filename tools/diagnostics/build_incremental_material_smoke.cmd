@echo off
setlocal
pushd "%~dp0..\.."
set "material_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%material_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-pack-v35 mkdir bin\resource-pack-v35
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /DEFF_OFFLINE_PACKAGE_V35=1 /Fo"bin\resource-pack-v35\incremental_material_smoke.obj" tools\diagnostics\incremental_material_smoke.cpp /Fe"bin\resource-pack-v35\incremental_material_smoke.exe" /link bcrypt.lib
set "material_result=%errorlevel%"
popd
exit /b %material_result%
