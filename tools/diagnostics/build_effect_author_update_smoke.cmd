@echo off
setlocal
pushd "%~dp0..\.."
set "effect_update_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%effect_update_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\effect-author-update-tests mkdir bin\effect-author-update-tests
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fobin\effect-author-update-tests\effect_author_update_smoke.obj tools\diagnostics\effect_author_update_smoke.cpp /Febin\effect-author-update-tests\effect_author_update_smoke.exe /link bcrypt.lib
set "effect_update_result=%errorlevel%"
popd
exit /b %effect_update_result%
