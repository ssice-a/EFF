@echo off
setlocal
pushd "%~dp0..\.."
set "startup_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%startup_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
call tools\build_imgui.cmd
if errorlevel 1 exit /b 1
set "startup_folder=bin\resource-directory-entry-v35"
if not exist "%startup_folder%" mkdir "%startup_folder%"
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /bigobj /Ideps\minhook_lib\include /Fo"%startup_folder%\startup_readiness_smoke.obj" tools\diagnostics\startup_readiness_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"%startup_folder%\startup_readiness_smoke.exe" /link bcrypt.lib user32.lib
set "startup_result=%errorlevel%"
popd
exit /b %startup_result%
