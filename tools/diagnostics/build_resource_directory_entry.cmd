@echo off
rem Compatibility path for callers; production build lives at repository root.
if /i "%~1"=="capture" (
 call "%~dp0..\..\build.bat" v36 capture
) else (
 call "%~dp0..\..\build.bat" %*
)
exit /b %errorlevel%
