@echo off
setlocal
pushd "%~dp0..\.."
set "source_names_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%source_names_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto source_names_failed
if not exist bin\source-namespace-keys mkdir bin\source-namespace-keys
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /DEFF_OFFLINE_PACKAGE_V35=1 /Fo"bin\source-namespace-keys\smoke.obj" tools\diagnostics\source_namespace_keys_smoke.cpp /Fe"bin\source-namespace-keys\smoke.exe" /link bcrypt.lib
set "source_names_result=%errorlevel%"
popd
exit /b %source_names_result%
:source_names_failed
popd
exit /b 1
