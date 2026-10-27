@echo off
setlocal
pushd "%~dp0..\.."
set "carryover_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%carryover_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto carryover_failed
if not exist bin\compiled-export-carryover mkdir bin\compiled-export-carryover
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /bigobj /W4 /Fo"bin\compiled-export-carryover\smoke.obj" tools\diagnostics\compiled_export_carryover_smoke.cpp /Fe"bin\compiled-export-carryover\smoke.exe" /link bcrypt.lib
set "carryover_result=%errorlevel%"
popd
exit /b %carryover_result%
:carryover_failed
popd
exit /b 1
