@echo off
setlocal
pushd "%~dp0..\.."
set "eff_catalog_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_catalog_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto failed
if not exist bin\runtime-author-catalog mkdir bin\runtime-author-catalog
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /I src /Fo"bin\runtime-author-catalog\runtime_author_catalog_smoke.obj" tools\diagnostics\runtime_author_catalog_smoke.cpp /Fe"bin\runtime-author-catalog\runtime_author_catalog_smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\runtime-author-catalog\runtime_author_catalog_smoke.exe %*
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
