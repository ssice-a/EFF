@echo off
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-EFFPerformanceTest.ps1" %*
pause
