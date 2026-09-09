@echo off
setlocal
cd /d "%~dp0\..\.."

if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib -Ilib\sqlite -Ilib\onnxruntime -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\conversation_memory\conversation_memory.c ^
    lib\sqlite\sqlite3.c ^
    -o build\conversation_memory.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if not "%errorlevel%"=="0" exit /b %errorlevel%

build\conversation_memory.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%

echo conversation_memory ok
