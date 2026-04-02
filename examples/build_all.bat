@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_all.ps1"
exit /b %ERRORLEVEL%
