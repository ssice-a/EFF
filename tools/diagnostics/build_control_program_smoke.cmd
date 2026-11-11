@echo off
setlocal
pushd "%~dp0..\.."
set "control_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%control_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\control-program mkdir bin\control-program
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /Fobin\control-program\smoke.obj tools\diagnostics\control_program_smoke.cpp /Febin\control-program\smoke.exe /link bcrypt.lib
set "control_result=%errorlevel%"
popd
exit /b %control_result%
