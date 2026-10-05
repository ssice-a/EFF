@echo off
setlocal
pushd "%~dp0..\.."
set "payload_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%payload_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-pack mkdir bin\resource-pack
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Fo"bin\resource-pack\inspect_payloads.obj" tools\diagnostics\inspect_compiled_resource_payloads.cpp /Fe"bin\resource-pack\inspect_payloads.exe" /link bcrypt.lib
set "payload_result=%errorlevel%"
popd
exit /b %payload_result%
