@echo off
setlocal
pushd "%~dp0..\.."
set "prefab_profile_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%prefab_profile_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto prefab_profile_failed
if not exist bin\prefab-consumer-survey mkdir bin\prefab-consumer-survey
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /DEFF_OFFLINE_PACKAGE_V35=1 /Fo"bin\prefab-consumer-survey\profile_prefab_consumers.obj" tools\diagnostics\profile_prefab_consumers.cpp /Fe"bin\prefab-consumer-survey\profile_prefab_consumers.exe" /link bcrypt.lib
set "prefab_profile_result=%errorlevel%"
popd
exit /b %prefab_profile_result%
:prefab_profile_failed
popd
exit /b 1
