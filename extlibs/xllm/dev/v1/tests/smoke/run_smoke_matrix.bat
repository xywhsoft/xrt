@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_smoke_matrix.ps1" %*
exit /b %errorlevel%
