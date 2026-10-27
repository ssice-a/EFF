@echo off
setlocal
pushd "%~dp0..\.."
set "vfs_layers_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%vfs_layers_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto vfs_layers_failed
if not exist bin\vfs-layers mkdir bin\vfs-layers
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /Fo"bin\vfs-layers\smoke.obj" tools\diagnostics\vfs_layers_native_smoke.cpp /Fe"bin\vfs-layers\smoke.exe" /link bcrypt.lib
set "vfs_layers_result=%errorlevel%"
popd
exit /b %vfs_layers_result%
:vfs_layers_failed
popd
exit /b 1
