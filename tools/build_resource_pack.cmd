@echo off
setlocal
pushd "%~dp0.."
set "pack_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%pack_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 exit /b 1
set "pack_folder=bin\resource-pack"
set "pack_define="
if /i "%~1"=="v35" (
 set "pack_folder=bin\resource-pack-v35"
 set "pack_define=/DEFF_OFFLINE_PACKAGE_V35=1"
)
if not "%~1"=="" if /i not "%~1"=="v35" if /i not "%~1"=="v36" (
 echo [ERROR] Expected ABI v36 or v35.
 popd
 exit /b 1
)
if not exist "%pack_folder%" mkdir "%pack_folder%"
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 %pack_define% /Fo"%pack_folder%\eff_resource_pack.obj" tools\eff_resource_pack.cpp /Fe"%pack_folder%\eff_resource_pack.exe" /link bcrypt.lib
set "pack_result=%errorlevel%"
popd
exit /b %pack_result%
