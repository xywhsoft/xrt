@echo off
setlocal
set XLLM_REPO_ROOT=%~dp0..\..\..
for %%I in ("%XLLM_REPO_ROOT%") do set XLLM_REPO_ROOT=%%~fI
cd /d "%XLLM_REPO_ROOT%"
if not exist build mkdir build
if not exist lib\sqlite-vec\sqlite-vec.dll (
    if exist lib\sqlite-vec\build.bat (
        call lib\sqlite-vec\build.bat
        if not "%errorlevel%"=="0" exit /b %errorlevel%
    )
)

gcc -std=c11 -Wall -Wextra -I. -Ilib -Ilib\sqlite -Ilib\onnxruntime -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION -DXLLM_MEMORY_SCHEME_MODE=XLLM_MEMORY_SCHEME_MODE_ONNX_E5 ^
    examples\smoke_memory_builtin_e5.c ^
    lib\sqlite\sqlite3.c ^
    -o build\smoke_memory_builtin_e5.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if not "%errorlevel%"=="0" exit /b %errorlevel%

build\smoke_memory_builtin_e5.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%

echo smoke_memory_builtin_e5 ok
