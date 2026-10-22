@echo off
setlocal
pushd "%~dp0..\.."
set "anim_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%anim_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\animation-runtime-20261019 mkdir bin\animation-runtime-20261019
set "anim_define=/DEIEM_ACTIVE_ANIMATION_TEST"
set "anim_name=current"
if /i "%~1"=="baseline" (
 set "anim_define=/DEIEM_ANIMATION_PLAYER_HEADER=\"../../bin/animation-runtime-20261019/baseline.h\""
 set "anim_name=baseline"
)
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /Isrc %anim_define% /Fobin\animation-runtime-20261019\%anim_name%.obj tools\diagnostics\animation_player_smoke.cpp /Febin\animation-runtime-20261019\%anim_name%.exe /link bcrypt.lib user32.lib
set "anim_result=%errorlevel%"
popd
exit /b %anim_result%
