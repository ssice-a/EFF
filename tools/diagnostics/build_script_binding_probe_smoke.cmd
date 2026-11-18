@echo off
setlocal
pushd "%~dp0..\.."
set "binding_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%binding_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /Ideps\minhook_lib\include /Fo"bin\resource-directory-entry\script_binding_probe_smoke.obj" tools\diagnostics\script_binding_probe_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"bin\resource-directory-entry\script_binding_probe_smoke.exe" /link /NODEFAULTLIB:MSVCRT
set "binding_result=%errorlevel%"
popd
exit /b %binding_result%
