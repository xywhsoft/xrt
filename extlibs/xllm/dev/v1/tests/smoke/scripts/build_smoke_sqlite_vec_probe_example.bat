@echo off
setlocal
set XLLM_REPO_ROOT=%~dp0..\..\..
for %%I in ("%XLLM_REPO_ROOT%") do set XLLM_REPO_ROOT=%%~fI
cd /d "%XLLM_REPO_ROOT%"
if not exist build mkdir build
if not exist lib\sqlite-vec\sqlite-vec.dll (
    call lib\sqlite-vec\build.bat
    if not "%errorlevel%"=="0" exit /b %errorlevel%
)

gcc -std=c11 -Wall -Wextra -Ilib -Ilib\sqlite ^
    examples\smoke_sqlite_vec_probe.c ^
    lib\sqlite\sqlite3.c ^
    -o build\smoke_sqlite_vec_probe.exe

if not "%errorlevel%"=="0" exit /b %errorlevel%

build\smoke_sqlite_vec_probe.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%

echo smoke_sqlite_vec_probe ok
