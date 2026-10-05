@echo off
setlocal
pushd "%~dp0..\.."
set "entry_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "entry_vs_install="
if exist "%entry_vswhere%" (
 for /f "usebackq tokens=*" %%i in (`"%entry_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "entry_vs_install=%%i"
)
if defined entry_vs_install call "%entry_vs_install%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Isrc /Fo"bin\resource-directory-entry\loading_graph_audit_smoke.obj" tools\diagnostics\loading_graph_audit_smoke.cpp /Fe"bin\resource-directory-entry\loading_graph_audit_smoke.exe" /link bcrypt.lib
set "entry_result=%errorlevel%"
popd
exit /b %entry_result%
