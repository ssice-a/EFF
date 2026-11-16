@echo off
setlocal
pushd "%~dp0..\.."
set "eff_dependency_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_dependency_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\effect-game-dependencies mkdir bin\effect-game-dependencies
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /Isrc /Fobin\effect-game-dependencies\smoke.obj tools\diagnostics\effect_game_dependencies_smoke.cpp /Febin\effect-game-dependencies\smoke.exe /link bcrypt.lib user32.lib
set "eff_dependency_result=%errorlevel%"
popd
exit /b %eff_dependency_result%
