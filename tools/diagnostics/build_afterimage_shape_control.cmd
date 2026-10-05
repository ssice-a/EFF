@echo off
setlocal
pushd "%~dp0..\.."
set "capture_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%capture_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\author-core mkdir bin\author-core
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Fo"bin\author-core\afterimage_shape_control.obj" tools\diagnostics\afterimage_shape_control.cpp /Fe"bin\author-core\afterimage_shape_control.exe" /link bcrypt.lib
set "capture_result=%errorlevel%"
popd
exit /b %capture_result%
