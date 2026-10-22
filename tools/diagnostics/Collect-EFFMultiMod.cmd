@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Collect-EFFMultiMod.ps1"
if errorlevel 1 echo EFF collection failed. Keep this message for diagnosis.
pause
