@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Collect-Mode7Profile.ps1"
if errorlevel 1 pause
