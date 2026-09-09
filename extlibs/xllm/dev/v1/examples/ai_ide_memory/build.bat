@echo off
setlocal
cd /d "%~dp0\..\.."

if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -Ilib\sqlite ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\ai_ide_memory\ai_ide_memory.c ^
    lib\sqlite\sqlite3.c ^
    -o build\ai_ide_memory.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

build\ai_ide_memory.exe
