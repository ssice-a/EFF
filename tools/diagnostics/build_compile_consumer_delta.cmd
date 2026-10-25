@echo off
setlocal
pushd "%~dp0..\.."
set "consumer_delta_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%consumer_delta_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto consumer_delta_failed
if not exist bin\consumer-delta mkdir bin\consumer-delta
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /bigobj /W4 /DEFF_OFFLINE_PACKAGE_V35=1 /Fo"bin\consumer-delta\delta.obj" tools\diagnostics\compile_consumer_delta.cpp /Fe"bin\consumer-delta\delta.exe" /link bcrypt.lib
set "consumer_delta_result=%errorlevel%"
popd
exit /b %consumer_delta_result%
:consumer_delta_failed
popd
exit /b 1
