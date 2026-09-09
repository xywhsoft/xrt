@echo off
setlocal

set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set OUT=%ROOT%\sqlite-vec.dll

gcc -std=c11 -O2 -w -I"%ROOT%\..\sqlite" -shared ^
    "%ROOT%\sqlite-vec.c" ^
    -o "%OUT%"

if errorlevel 1 exit /b %errorlevel%

echo Built %OUT%
