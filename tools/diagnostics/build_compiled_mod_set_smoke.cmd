@echo off
setlocal
pushd "%~dp0..\.."
set "static_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%static_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\compiled-mod-set mkdir bin\compiled-mod-set
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /Fo"bin\compiled-mod-set\compiled_mod_set_smoke.obj" tools\diagnostics\compiled_mod_set_smoke.cpp /Fe"bin\compiled-mod-set\compiled_mod_set_smoke.exe" /link bcrypt.lib
set "static_result=%errorlevel%"
popd
exit /b %static_result%
