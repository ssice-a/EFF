@echo off
setlocal
pushd "%~dp0..\.."
set "compiled_store_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%compiled_store_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto compiled_store_failed
if not exist bin\compiled-mod-store mkdir bin\compiled-mod-store
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /Fo"bin\compiled-mod-store\smoke.obj" tools\diagnostics\compiled_mod_store_smoke.cpp /Fe"bin\compiled-mod-store\smoke.exe" /link bcrypt.lib
set "compiled_store_result=%errorlevel%"
popd
exit /b %compiled_store_result%
:compiled_store_failed
popd
exit /b 1
