@echo off
setlocal
cd /d "%~dp0\..\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_static_analysis.ps1" %*
exit /b %errorlevel%
