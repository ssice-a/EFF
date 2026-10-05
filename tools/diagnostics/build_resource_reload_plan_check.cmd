@echo off
setlocal
pushd "%~dp0..\.."
set "reload_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%reload_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Isrc /Fo"bin\resource-directory-entry\resource_reload_plan_check.obj" tools\diagnostics\resource_reload_plan_check.cpp /Fe"bin\resource-directory-entry\resource_reload_plan_check.exe" /link bcrypt.lib psapi.lib
set "reload_result=%errorlevel%"
popd
exit /b %reload_result%
