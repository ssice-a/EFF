@echo off
setlocal
pushd "%~dp0..\.."
set "frame_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "frame_vs_install="
if exist "%frame_vswhere%" (
 for /f "usebackq tokens=*" %%i in (`"%frame_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "frame_vs_install=%%i"
)
if defined frame_vs_install call "%frame_vs_install%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\resource-directory-entry mkdir bin\resource-directory-entry
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Fo"bin\resource-directory-entry\shaped_frame_writer_regression.obj" tools\diagnostics\shaped_frame_writer_regression.cpp /Fe"bin\resource-directory-entry\shaped_frame_writer_regression.exe" /link bcrypt.lib
set "frame_result=%errorlevel%"
popd
exit /b %frame_result%
