@echo off
setlocal
pushd "%~dp0..\.."
set "loader_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%loader_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
call tools\build_imgui.cmd
if errorlevel 1 exit /b 1
set "loader_folder=bin\resource-directory-entry"
set "loader_define="
if /i "%~1"=="v35" set "loader_folder=bin\resource-directory-entry-v35"
if /i "%~1"=="v35" set "loader_define=/DEFF_OFFLINE_PACKAGE_V35=1"
if not exist "%loader_folder%" mkdir "%loader_folder%"
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 %loader_define% /Ideps\minhook_lib\include /Fo"%loader_folder%\offline_loader_smoke.obj" tools\diagnostics\offline_loader_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"%loader_folder%\offline_loader_smoke.exe" /link bcrypt.lib user32.lib
set "loader_result=%errorlevel%"
popd
exit /b %loader_result%
