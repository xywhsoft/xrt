@echo off
setlocal
set XLLM_REPO_ROOT=%~dp0..\..\..
for %%I in ("%XLLM_REPO_ROOT%") do set XLLM_REPO_ROOT=%%~fI
cd /d "%XLLM_REPO_ROOT%"

if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\smoke_ollama_native_stream_adapter.c ^
    -o build\smoke_ollama_native_stream_adapter.exe ^
    -lws2_32 -liphlpapi -lshell32

if not "%errorlevel%"=="0" exit /b %errorlevel%

build\smoke_ollama_native_stream_adapter.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%

echo smoke_ollama_native_stream_adapter ok
