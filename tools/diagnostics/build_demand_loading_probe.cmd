@echo off
setlocal
pushd "%~dp0..\.."
set "demand_probe_folder=bin\demand-loading-probe"
set "demand_probe_define=/DEIEM_DEMAND_LOADING_PROBE=1"
set "demand_probe_extra="
set "demand_probe_verify="
if /i "%~1"=="native-acceptance" (
 set "demand_probe_folder=bin\demand-native-acceptance"
 set "demand_probe_extra=/DEIEM_DEMAND_NATIVE_ACCEPTANCE=1"
 set "demand_probe_verify=--native-acceptance"
) else if /i "%~1"=="format3-runtime" (
 set "demand_probe_folder=bin\format3-runtime"
 set "demand_probe_define="
 set "demand_probe_extra=/DEIEM_FORMAT3_RUNTIME=1"
 set "demand_probe_verify=--format3-runtime"
) else if not "%~1"=="" goto demand_probe_failed
set "demand_probe_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%demand_probe_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto demand_probe_failed
if not exist "%demand_probe_folder%" mkdir "%demand_probe_folder%"
call tools\build_imgui.cmd static
if errorlevel 1 goto demand_probe_failed
cl /nologo /utf-8 /O2 /Zi /MT /LD /EHsc /std:c++17 /bigobj /W4 /we4930 /we5046 %demand_probe_define% %demand_probe_extra% /DEIEM_STATIC_RUNTIME=1 /Ideps\minhook_lib\include /Fo"%demand_probe_folder%\entry.obj" /Fd"%demand_probe_folder%\compile.pdb" src\eiem_resource_entry.cpp deps\minhook_lib\lib\libMinHook.x64.lib /Fe"%demand_probe_folder%\eff.dll" /link /DLL /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT bcrypt.lib user32.lib /PDB:"%demand_probe_folder%\eff.pdb"
if errorlevel 1 goto demand_probe_failed
python tools\diagnostics\verify_demand_loading_probe.py "%demand_probe_folder%\eff.dll" %demand_probe_verify%
set "demand_probe_result=%errorlevel%"
popd
exit /b %demand_probe_result%
:demand_probe_failed
popd
exit /b 1
