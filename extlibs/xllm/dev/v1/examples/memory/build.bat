@echo off
setlocal
cd /d "%~dp0\..\.."

if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -Ilib\sqlite ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\memory\memory_basic.c ^
    lib\sqlite\sqlite3.c ^
    -o build\memory_basic.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\memory_basic.exe
