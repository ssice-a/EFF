@echo off
setlocal
pushd "%~dp0..\.."
set "eff_author_test_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_author_test_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\author-assets mkdir bin\author-assets
set "author_test_suffix="
if not "%~1"=="" set "author_test_suffix=_%~1"
if /i "%~2"=="material-only" goto material_only
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /W4 /Fo"bin\author-assets\directory-smoke%author_test_suffix%.obj" tools\diagnostics\author_asset_directory_smoke.cpp /Fe"bin\author-assets\directory-smoke%author_test_suffix%.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\author-assets\directory-smoke%author_test_suffix%.exe
if errorlevel 1 goto failed
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\author-assets\mesh-smoke%author_test_suffix%.obj" tools\diagnostics\author_mesh_bundle_smoke.cpp /Fe"bin\author-assets\mesh-smoke%author_test_suffix%.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
bin\author-assets\mesh-smoke%author_test_suffix%.exe
if errorlevel 1 goto failed
:material_only
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /bigobj /W4 /Fo"bin\author-assets\material-smoke%author_test_suffix%.obj" tools\diagnostics\author_material_edits_smoke.cpp /Fe"bin\author-assets\material-smoke%author_test_suffix%.exe" /link /NODEFAULTLIB:MSVCRT bcrypt.lib
if errorlevel 1 goto failed
if /i "%~2"=="material-only" bin\author-assets\material-smoke%author_test_suffix%.exe --removal-only
if /i "%~2"=="material-only" if errorlevel 1 goto failed
if /i not "%~2"=="material-only" bin\author-assets\material-smoke%author_test_suffix%.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
