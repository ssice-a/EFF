@echo off
setlocal
pushd "%~dp0..\.."
set "probe_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "probe_vs_install="
if exist "%probe_vswhere%" (
 for /f "usebackq tokens=*" %%i in (`"%probe_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "probe_vs_install=%%i"
)
if defined probe_vs_install call "%probe_vs_install%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\v35c-lifecycle-probe mkdir bin\v35c-lifecycle-probe
cl /nologo /utf-8 /O2 /Zi /MD /LD /EHsc /std:c++17 /W4 ^
 /Isrc /Ideps\minhook_lib\include ^
 /Fo"bin\v35c-lifecycle-probe\eff_resource_entry.obj" ^
 /Fd"bin\v35c-lifecycle-probe\eff_compile.pdb" ^
 diagnostics\f10-native-lifecycle-audit-20261005\probe-v35c-source\eiem_resource_entry.cpp ^
 deps\minhook_lib\lib\libMinHook.x64.lib ^
 /Fe"bin\v35c-lifecycle-probe\eff.dll" ^
 /link /DLL /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO bcrypt.lib user32.lib ^
 /PDB:"bin\v35c-lifecycle-probe\eff.pdb"
set "probe_result=%errorlevel%"
popd
exit /b %probe_result%
