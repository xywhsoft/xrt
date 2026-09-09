@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\xllm_build\build.ps1" %*
exit /b %errorlevel%
