@echo off
setlocal
pushd "%~dp0..\.."
set "shape_verify_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%shape_verify_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\mod-shape-experiment mkdir bin\mod-shape-experiment
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /Fobin\mod-shape-experiment\verify.obj tools\diagnostics\verify_mod_shape_experiment.cpp /Febin\mod-shape-experiment\verify.exe /link bcrypt.lib
set "shape_verify_result=%errorlevel%"
popd
exit /b %shape_verify_result%
