@echo off
setlocal
pushd "%~dp0..\.."
set "owner_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "owner_vs_install="
if exist "%owner_vswhere%" (
    for /f "usebackq tokens=*" %%i in (`"%owner_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "owner_vs_install=%%i"
)
if defined owner_vs_install call "%owner_vs_install%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\native-mesh-entry-trace mkdir bin\native-mesh-entry-trace
cl /nologo /MD /LD /EHsc ^
    /Fo"bin\native-mesh-entry-trace\read_owner_fixture.obj" ^
    tools\diagnostics\native_mesh_read_owner_fixture.cpp ^
    /Fe"bin\native-mesh-entry-trace\UnityPlayer.dll"
if errorlevel 1 goto owner_failed
cl /nologo /MD /EHsc /std:c++17 /Isrc /Ideps\minhook_lib\include ^
    /Fo"bin\native-mesh-entry-trace\read_owner_smoke.obj" ^
    tools\diagnostics\native_mesh_read_owner_smoke.cpp ^
    deps\minhook_lib\lib\libMinHook.x64.lib ^
    /Fe"bin\native-mesh-entry-trace\read_owner_smoke.exe"
if errorlevel 1 goto owner_failed
bin\native-mesh-entry-trace\read_owner_smoke.exe bin\native-mesh-entry-trace\UnityPlayer.dll
if errorlevel 1 goto owner_failed
bin\native-mesh-entry-trace\read_owner_smoke.exe --verify-install analysis\unityplayer-20260923\UnityPlayer.dll
set "owner_result=%errorlevel%"
popd
exit /b %owner_result%
:owner_failed
popd
exit /b 1
