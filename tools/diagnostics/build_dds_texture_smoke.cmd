@echo off
setlocal
pushd "%~dp0..\.."
set "dds_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%dds_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\author-assets mkdir bin\author-assets
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 tools\diagnostics\dds_texture_smoke.cpp /Fo"bin\author-assets\dds-smoke.obj" /Fe"bin\author-assets\dds-smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\author-assets\dds-smoke.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
