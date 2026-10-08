@echo off
setlocal EnableDelayedExpansion
pushd "%~dp0..\.."
set "ui_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%ui_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\ui-page-bind mkdir bin\ui-page-bind
rem Compile the existing Lua sources for a standalone diagnostic only.
set "ui_sources="
for %%i in (deps\lua\*.c) do if /i not "%%~nxi"=="luac.c" set "ui_sources=!ui_sources! %%i"
cl /nologo /O2 /MD /DLUA_USE_WINDOWS /Ideps\lua /Fo"bin\ui-page-bind\\" !ui_sources! /Fe"bin\ui-page-bind\lua.exe" /link /INCREMENTAL:NO >bin\ui-page-bind\build.log 2>&1
if errorlevel 1 (type bin\ui-page-bind\build.log & popd & exit /b 1)
bin\ui-page-bind\lua.exe tools\diagnostics\ui_page_bind_smoke.lua src\eiem_entry_ui_page.h
set "ui_result=%errorlevel%"
popd
exit /b %ui_result%
