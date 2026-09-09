@echo off
setlocal
set XLLM_REPO_ROOT=%~dp0..\..\..
for %%I in ("%XLLM_REPO_ROOT%") do set XLLM_REPO_ROOT=%%~fI
cd /d "%XLLM_REPO_ROOT%"
if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib -Ilib\sqlite -Ilib\onnxruntime ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION -DXLLM_MEMORY_IMPLEMENTATION ^
    examples\smoke_memory_sync_workspace.c ^
    lib\sqlite\sqlite3.c ^
    -o build\smoke_memory_sync_workspace.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if not "%errorlevel%"=="0" exit /b %errorlevel%

build\smoke_memory_sync_workspace.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%

echo Built and ran build\smoke_memory_sync_workspace.exe
