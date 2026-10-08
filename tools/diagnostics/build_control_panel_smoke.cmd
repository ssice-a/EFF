@echo off
setlocal
pushd "%~dp0..\.."
set "panel_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%panel_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
call tools\build_imgui.cmd
if errorlevel 1 exit /b 1
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Ideps\minhook_lib\include /Fo"bin\resource-directory-entry\control_panel_smoke.obj" tools\diagnostics\control_panel_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"bin\resource-directory-entry\control_panel_smoke.exe" /link bcrypt.lib user32.lib
set "panel_result=%errorlevel%"
popd
exit /b %panel_result%
