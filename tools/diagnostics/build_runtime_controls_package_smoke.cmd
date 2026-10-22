@echo off
setlocal
pushd "%~dp0..\.."
set "runtime_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%runtime_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\runtime-controls-20261019 mkdir bin\runtime-controls-20261019
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /Fobin\runtime-controls-20261019\package.obj tools\diagnostics\runtime_controls_package_smoke.cpp /Febin\runtime-controls-20261019\package.exe /link bcrypt.lib
set "runtime_result=%errorlevel%"
popd
exit /b %runtime_result%
