@echo off
setlocal
pushd "%~dp0..\.."
set "panel_audit_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%panel_audit_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Fo"bin\resource-directory-entry\imgui_device_failure_smoke.obj" tools\diagnostics\imgui_device_failure_smoke.cpp "deps\minhook_lib\lib\libMinHook.x64.lib" /Fe"bin\resource-directory-entry\imgui_device_failure_smoke.exe" /link user32.lib
set "panel_audit_result=%errorlevel%"
popd
exit /b %panel_audit_result%
