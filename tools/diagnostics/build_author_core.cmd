@echo off
setlocal
pushd "%~dp0..\.."
set "author_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%author_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\author-core mkdir bin\author-core
cl /nologo /utf-8 /O2 /MD /LD /EHsc /std:c++17 /W4 /Fo"bin\author-core\author_core.obj" src\eiem_author_core.cpp /Fe"bin\author-core\eff_author_core.dll" /link bcrypt.lib
set "author_result=%errorlevel%"
popd
exit /b %author_result%
