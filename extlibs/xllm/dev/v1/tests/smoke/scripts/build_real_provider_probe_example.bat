@echo off
setlocal
set XLLM_REPO_ROOT=%~dp0..\..\..
for %%I in ("%XLLM_REPO_ROOT%") do set XLLM_REPO_ROOT=%%~fI
cd /d "%XLLM_REPO_ROOT%"

if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\real_provider_probe.c ^
    -o build\real_provider_probe.exe ^
    -lws2_32 -liphlpapi -lshell32

if errorlevel 1 exit /b 1

if "%XLLM_REAL_RUN%"=="1" (
    build\real_provider_probe.exe
    if errorlevel 1 exit /b 1
)

echo real_provider_probe built
