@echo off
setlocal
pushd "%~dp0..\.."
set "compat_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%compat_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\game-update-compat mkdir bin\game-update-compat
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /bigobj /W4 /Fo"bin\game-update-compat\probe.obj" tools\diagnostics\game_update_compat_probe.cpp /Fe"bin\game-update-compat\probe.exe" /link bcrypt.lib
set "compat_result=%errorlevel%"
popd
exit /b %compat_result%
