@echo off
setlocal
pushd "%~dp0..\.."
set "bcn_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%bcn_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto bcn_failed
if not exist bin\texture-bcn mkdir bin\texture-bcn
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /Fo"bin\texture-bcn\smoke.obj" tools\diagnostics\texture_bcn_smoke.cpp /Fe"bin\texture-bcn\smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto bcn_failed
bin\texture-bcn\smoke.exe
set "bcn_result=%errorlevel%"
popd
exit /b %bcn_result%
:bcn_failed
popd
exit /b 1
