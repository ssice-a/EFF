@echo off
setlocal
pushd "%~dp0..\.."
set "eff_mount_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_mount_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\author-transforms mkdir bin\author-transforms
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /Isrc /Fobin\author-transforms\smoke.obj tools\diagnostics\author_transform_smoke.cpp /Febin\author-transforms\smoke.exe /link bcrypt.lib user32.lib
if errorlevel 1 exit /b 1
bin\author-transforms\smoke.exe
set "eff_mount_result=%errorlevel%"
popd
exit /b %eff_mount_result%
