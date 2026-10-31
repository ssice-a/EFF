@echo off
setlocal
pushd "%~dp0..\.."
set "eff_author_test_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_author_test_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto failed
set "output=bin\author-resource-file"
if not exist "%output%" mkdir "%output%"
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /I src /Fo"%output%\author_resource_file_smoke.obj" tools\diagnostics\author_resource_file_smoke.cpp /Fe"%output%\author_resource_file_smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
"%output%\author_resource_file_smoke.exe"
set "result=%errorlevel%"
popd
exit /b %result%
:failed
popd
exit /b 1
