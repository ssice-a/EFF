@echo off
setlocal
pushd "%~dp0..\.."
set "prefab_inspect_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%prefab_inspect_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto prefab_inspect_failed
if not exist bin\prefab-adaptation-probe mkdir bin\prefab-adaptation-probe
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /DEFF_OFFLINE_PACKAGE_V35=1 /Fo"bin\prefab-adaptation-probe\inspect_compiled_prefabs.obj" tools\diagnostics\inspect_compiled_prefabs.cpp /Fe"bin\prefab-adaptation-probe\inspect_compiled_prefabs.exe" /link bcrypt.lib
set "prefab_inspect_result=%errorlevel%"
popd
exit /b %prefab_inspect_result%
:prefab_inspect_failed
popd
exit /b 1
