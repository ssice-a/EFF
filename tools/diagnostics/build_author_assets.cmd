@echo off
setlocal
pushd "%~dp0..\.."
set "author_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%author_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\author-assets mkdir bin\author-assets
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /W4 /Fo"bin\author-assets\eff_author_assets.obj" tools\eff_author_assets.cpp /Fe"bin\author-assets\eff_author_assets.exe" /link bcrypt.lib
set "author_result=%errorlevel%"
popd
exit /b %author_result%
