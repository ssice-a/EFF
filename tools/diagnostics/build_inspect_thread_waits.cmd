@echo off
setlocal
pushd "%~dp0..\.."
set "wait_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%wait_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\thread-waits mkdir bin\thread-waits
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Fo"bin\thread-waits\inspect_thread_waits.obj" tools\diagnostics\inspect_thread_waits.cpp /Fe"bin\thread-waits\inspect_thread_waits.exe" /link advapi32.lib
set "wait_result=%errorlevel%"
popd
exit /b %wait_result%
