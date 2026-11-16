@echo off
setlocal
pushd "%~dp0..\.."
set "eff_mount_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%eff_mount_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
call tools\build_imgui.cmd
if errorlevel 1 exit /b 1
if not exist bin\author-transforms mkdir bin\author-transforms
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /bigobj /DEIEM_FORMAT3_RUNTIME=1 /Ideps\minhook_lib\include /Fobin\author-transforms\runtime.obj tools\diagnostics\author_transform_runtime_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Febin\author-transforms\runtime.exe /link bcrypt.lib user32.lib
if errorlevel 1 exit /b 1
bin\author-transforms\runtime.exe
set "eff_mount_result=%errorlevel%"
popd
exit /b %eff_mount_result%
