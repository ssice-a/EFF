@echo off
setlocal
pushd "%~dp0..\.."
set "eff_random_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_random_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto failed
if not exist bin\native-random-access mkdir bin\native-random-access
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\native-random-access\smoke.obj" tools\diagnostics\native_random_access_smoke.cpp /Fe"bin\native-random-access\smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\native-random-access\smoke.exe %*
set "eff_random_result=%errorlevel%"
popd
exit /b %eff_random_result%
:failed
popd
exit /b 1
