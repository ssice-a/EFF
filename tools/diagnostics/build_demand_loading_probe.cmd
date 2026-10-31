@echo off
setlocal
pushd "%~dp0..\.."
call build.bat %*
set "build_result=%errorlevel%"
popd
exit /b %build_result%
