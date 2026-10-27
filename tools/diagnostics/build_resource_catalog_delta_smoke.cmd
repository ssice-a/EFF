@echo off
setlocal
pushd "%~dp0..\.."
set "catalog_delta_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%catalog_delta_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto catalog_delta_failed
if not exist bin\resource-catalog-delta mkdir bin\resource-catalog-delta
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /Fo"bin\resource-catalog-delta\smoke.obj" tools\diagnostics\resource_catalog_delta_smoke.cpp /Fe"bin\resource-catalog-delta\smoke.exe" /link bcrypt.lib
set "catalog_delta_result=%errorlevel%"
popd
exit /b %catalog_delta_result%
:catalog_delta_failed
popd
exit /b 1
