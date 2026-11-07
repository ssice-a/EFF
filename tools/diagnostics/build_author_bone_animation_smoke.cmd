@echo off
setlocal
pushd "%~dp0..\.."
set "bone_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 if exist "%bone_vswhere%" for /f "usebackq tokens=*" %%i in (`"%bone_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 (
 echo [ERROR] MSVC x64 tools unavailable.
 popd
 exit /b 1
)
if not exist bin\author-bone-animation mkdir bin\author-bone-animation
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /Fo"bin\author-bone-animation\author_bone_animation_smoke.obj" tools\diagnostics\author_bone_animation_smoke.cpp /Febin\author-bone-animation\author_bone_animation_smoke.exe
if errorlevel 1 (
 popd
 exit /b 1
)
bin\author-bone-animation\author_bone_animation_smoke.exe
set "bone_result=%errorlevel%"
popd
exit /b %bone_result%
