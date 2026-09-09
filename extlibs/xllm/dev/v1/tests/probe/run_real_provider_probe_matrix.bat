@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_real_provider_probe_matrix.ps1" %*
exit /b %errorlevel%
