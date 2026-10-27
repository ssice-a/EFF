@echo off
setlocal
pushd "%~dp0..\.."
set "vfs_names_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%vfs_names_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\vfs-lookup-names mkdir bin\vfs-lookup-names
cl /nologo /utf-8 /O2 /MD /EHsc /std:c++17 /Fo"bin\vfs-lookup-names\inspect.obj" tools\diagnostics\inspect_vfs_lookup_names.cpp /Fe"bin\vfs-lookup-names\inspect.exe" /link bcrypt.lib
set "vfs_names_result=%errorlevel%"
popd
exit /b %vfs_names_result%
