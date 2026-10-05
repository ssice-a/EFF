@echo off
setlocal
pushd "%~dp0..\.."
set "reload_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%reload_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Ideps\minhook_lib\include /Fo"bin\resource-directory-entry\full_resource_reload_smoke.obj" tools\diagnostics\full_resource_reload_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"bin\resource-directory-entry\full_resource_reload_smoke.exe" /link bcrypt.lib user32.lib
set "reload_result=%errorlevel%"
popd
exit /b %reload_result%
