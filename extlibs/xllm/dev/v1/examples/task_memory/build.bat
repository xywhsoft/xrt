@echo off
setlocal
cd /d "%~dp0\..\.."

if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib -Ilib\sqlite -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\task_memory\task_memory.c ^
    lib\sqlite\sqlite3.c ^
    -o build\task_memory.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if not "%errorlevel%"=="0" exit /b %errorlevel%

build\task_memory.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%

echo task_memory ok
