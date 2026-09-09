@echo off
setlocal
set XLLM_REPO_ROOT=%~dp0..\..\..
for %%I in ("%XLLM_REPO_ROOT%") do set XLLM_REPO_ROOT=%%~fI
cd /d "%XLLM_REPO_ROOT%"

gcc -std=c11 -Wall -Wextra -I. -Ilib examples\smoke_real_provider_probe_matrix_provider_tool_skip.c -o build\smoke_real_provider_probe_matrix_provider_tool_skip.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%

build\smoke_real_provider_probe_matrix_provider_tool_skip.exe
exit /b %errorlevel%
