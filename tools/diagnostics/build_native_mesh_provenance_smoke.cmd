@echo off
setlocal
pushd "%~dp0..\.."
set "eff_provenance_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_provenance_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\native-mesh-provenance mkdir bin\native-mesh-provenance
cl /nologo /utf-8 /Od /MD /EHsc /std:c++17 /bigobj /Fo"bin\native-mesh-provenance\smoke.obj" tools\diagnostics\native_mesh_provenance_smoke.cpp /Fe"bin\native-mesh-provenance\smoke.exe" /link bcrypt.lib
if errorlevel 1 exit /b 1
bin\native-mesh-provenance\smoke.exe
set "eff_provenance_result=%errorlevel%"
popd
exit /b %eff_provenance_result%
