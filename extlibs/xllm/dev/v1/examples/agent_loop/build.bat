@echo off
setlocal
cd /d "%~dp0\..\.."

if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -Ilib\sqlite ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\agent_loop\agent_loop.c ^
    lib\sqlite\sqlite3.c ^
    -o build\agent_loop.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

build\agent_loop.exe
