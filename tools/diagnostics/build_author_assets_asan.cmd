@echo off
setlocal
pushd "%~dp0..\.."
set "asan_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%asan_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto failed
if not exist bin\author-assets-asan mkdir bin\author-assets-asan
cl /nologo /utf-8 /Od /Zi /MD /EHsc /std:c++17 /fsanitize=address /W4 /Fo"bin\author-assets-asan\eff_author_assets.obj" /LD tools\eff_author_assets.cpp /Fe"bin\author-assets-asan\eff_author_assets.dll" /link /DEBUG /MAP:"bin\author-assets-asan\eff_author_assets.map" bcrypt.lib
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
