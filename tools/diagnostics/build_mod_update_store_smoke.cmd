@echo off
setlocal
pushd "%~dp0..\.."
set "update_store_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%update_store_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto update_store_failed
if not exist bin\mod-update-store mkdir bin\mod-update-store
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /DEFF_OFFLINE_PACKAGE_V35=1 /Fo"bin\mod-update-store\smoke.obj" tools\diagnostics\mod_update_store_smoke.cpp /Fe"bin\mod-update-store\smoke.exe" /link bcrypt.lib
set "update_store_result=%errorlevel%"
popd
exit /b %update_store_result%
:update_store_failed
popd
exit /b 1
