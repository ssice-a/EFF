@echo off
setlocal
pushd "%~dp0..\.."
set "reload_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%reload_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
set "reload_folder=bin\resource-directory-entry"
set "reload_define="
if /i "%~1"=="v35" set "reload_folder=bin\resource-directory-entry-v35"
if /i "%~1"=="v35" set "reload_define=/DEFF_OFFLINE_PACKAGE_V35=1"
if not exist "%reload_folder%" mkdir "%reload_folder%"
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 %reload_define% /Ideps\minhook_lib\include /Fo"%reload_folder%\full_resource_reload_smoke.obj" tools\diagnostics\full_resource_reload_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"%reload_folder%\full_resource_reload_smoke.exe" /link bcrypt.lib user32.lib
set "reload_result=%errorlevel%"
popd
exit /b %reload_result%
