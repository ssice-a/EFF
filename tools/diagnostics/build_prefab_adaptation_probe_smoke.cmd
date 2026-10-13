@echo off
setlocal
pushd "%~dp0..\.."
set "probe_smoke_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%probe_smoke_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto probe_smoke_failed
if not exist bin\prefab-adaptation-probe mkdir bin\prefab-adaptation-probe
call tools\build_imgui.cmd static
if errorlevel 1 goto probe_smoke_failed
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /DEFF_OFFLINE_PACKAGE_V35=1 /DEIEM_STATIC_RUNTIME=1 /Ideps\minhook_lib\include /Fo"bin\prefab-adaptation-probe\smoke.obj" tools\diagnostics\prefab_adaptation_probe_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"bin\prefab-adaptation-probe\smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib user32.lib
if errorlevel 1 goto probe_smoke_failed
bin\prefab-adaptation-probe\smoke.exe
set "probe_smoke_result=%errorlevel%"
popd
exit /b %probe_smoke_result%
:probe_smoke_failed
popd
exit /b 1
