@echo off
setlocal
pushd "%~dp0..\.."
set "eff_author_test_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_author_test_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\author-assets mkdir bin\author-assets
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /Fo"bin\author-assets\directory-smoke.obj" tools\diagnostics\author_asset_directory_smoke.cpp /Fe"bin\author-assets\directory-smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\author-assets\directory-smoke.exe
if errorlevel 1 goto failed
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\author-assets\mesh-smoke.obj" tools\diagnostics\author_mesh_bundle_smoke.cpp /Fe"bin\author-assets\mesh-smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\author-assets\mesh-smoke.exe
if errorlevel 1 goto failed
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\author-assets\material-smoke.obj" tools\diagnostics\author_material_edits_smoke.cpp /Fe"bin\author-assets\material-smoke.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\author-assets\material-smoke.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
