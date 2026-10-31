@echo off
setlocal
pushd "%~dp0..\.."
set "native_smoke_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%native_smoke_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto native_smoke_failed
if not exist bin\mesh-native-acceptance mkdir bin\mesh-native-acceptance
call tools\build_imgui.cmd static
if errorlevel 1 goto native_smoke_failed
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /DEIEM_MESH_NATIVE_ACCEPTANCE=1 /DEIEM_STATIC_RUNTIME=1 /Ideps\minhook_lib\include /Fo"bin\mesh-native-acceptance\smoke.obj" tools\diagnostics\mesh_native_acceptance_smoke.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"bin\mesh-native-acceptance\smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib user32.lib
if errorlevel 1 goto native_smoke_failed
bin\mesh-native-acceptance\smoke.exe %*
set "native_smoke_result=%errorlevel%"
popd
exit /b %native_smoke_result%
:native_smoke_failed
popd
exit /b 1
