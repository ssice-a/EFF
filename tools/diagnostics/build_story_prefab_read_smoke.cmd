@echo off
setlocal
pushd "%~dp0..\.."
set "eff_story_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_story_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\story-prefab-read mkdir bin\story-prefab-read
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\story-prefab-read\smoke.obj" tools\diagnostics\story_prefab_read_smoke.cpp /Fe"bin\story-prefab-read\smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
set "eff_story_result=%errorlevel%"
popd
exit /b %eff_story_result%
