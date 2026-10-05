@echo off
setlocal
pushd "%~dp0..\.."
set "shape_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%shape_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\author-core mkdir bin\author-core
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Fo"bin\author-core\prepare_shape_runtime_inputs.obj" tools\diagnostics\prepare_shape_runtime_inputs.cpp /Fe"bin\author-core\prepare_shape_runtime_inputs.exe" /link bcrypt.lib
set "shape_result=%errorlevel%"
popd
exit /b %shape_result%
