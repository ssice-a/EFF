@echo off
setlocal
pushd "%~dp0..\.."
set "payload_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%payload_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto payload_failed
if not exist bin\runtime-resource-payload mkdir bin\runtime-resource-payload
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /Fo"bin\runtime-resource-payload\smoke.obj" tools\diagnostics\runtime_resource_payload_smoke.cpp /Fe"bin\runtime-resource-payload\smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto payload_failed
bin\runtime-resource-payload\smoke.exe %*
set "payload_result=%errorlevel%"
popd
exit /b %payload_result%
:payload_failed
popd
exit /b 1
