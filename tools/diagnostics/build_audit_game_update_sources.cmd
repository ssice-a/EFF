@echo off
setlocal
pushd "%~dp0..\.."
set "audit_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%audit_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\game-update-repair-20261015 mkdir bin\game-update-repair-20261015
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /bigobj /W4 /Fo"bin\game-update-repair-20261015\audit_sources.obj" tools\diagnostics\audit_game_update_sources.cpp /Fe"bin\game-update-repair-20261015\audit_sources.exe" /link bcrypt.lib
set "audit_result=%errorlevel%"
popd
exit /b %audit_result%
