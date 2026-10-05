@echo off
setlocal
pushd "%~dp0.."
set "pack_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%pack_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
set "pack_folder=bin\resource-pack"
set "pack_define="
if /i "%~1"=="v35" (
 set "pack_folder=bin\resource-pack-v35"
 set "pack_define=/DEFF_OFFLINE_PACKAGE_V35=1"
)
if not exist "%pack_folder%" mkdir "%pack_folder%"
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 %pack_define% /Fo"%pack_folder%\eff_resource_pack.obj" tools\eff_resource_pack.cpp /Fe"%pack_folder%\eff_resource_pack.exe" /link bcrypt.lib
set "pack_result=%errorlevel%"
popd
exit /b %pack_result%
