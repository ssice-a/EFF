@echo off
setlocal
pushd "%~dp0..\.."
set "pack_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%pack_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-pack mkdir bin\resource-pack
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Fo"bin\resource-pack\inspect_offline.obj" tools\diagnostics\inspect_offline_resource_package.cpp /Fe"bin\resource-pack\inspect_offline.exe" /link bcrypt.lib
set "pack_result=%errorlevel%"
popd
exit /b %pack_result%
