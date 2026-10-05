@echo off
setlocal
pushd "%~dp0.."
set "texture_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%texture_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\texture-convert mkdir bin\texture-convert
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /Fo"bin\texture-convert\eff_texture_convert.obj" tools\eff_texture_convert.cpp /Fe"bin\texture-convert\eff_texture_convert.exe" /link bcrypt.lib ole32.lib windowscodecs.lib
set "texture_result=%errorlevel%"
popd
exit /b %texture_result%
