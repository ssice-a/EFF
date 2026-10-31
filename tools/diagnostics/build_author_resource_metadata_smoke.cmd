@echo off
setlocal
pushd "%~dp0..\.."
set "eff_metadata_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_metadata_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto failed
if not exist bin\author-resource-metadata mkdir bin\author-resource-metadata
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /I src /Fo"bin\author-resource-metadata\author_resource_metadata_smoke.obj" tools\diagnostics\author_resource_metadata_smoke.cpp /Fe"bin\author-resource-metadata\author_resource_metadata_smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\author-resource-metadata\author_resource_metadata_smoke.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
