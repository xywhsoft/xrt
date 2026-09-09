@echo off
setlocal
set XLLM_REPO_ROOT=%~dp0..\..\..
for %%I in ("%XLLM_REPO_ROOT%") do set XLLM_REPO_ROOT=%%~fI
cd /d "%XLLM_REPO_ROOT%"

if not exist build (
    mkdir build
)

echo ========================================
echo   XLLM Smoke Example Builder
echo ========================================
echo.

gcc -std=c11 -Wall -Wextra -I. -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\smoke_mock_adapter.c ^
    -o build\smoke_mock_adapter.exe ^
    -lws2_32 -liphlpapi -lshell32

if errorlevel 1 (
    echo.
    echo Build failed.
    exit /b 1
)

echo.
echo Running smoke example...
build\smoke_mock_adapter.exe
set "RC=%errorlevel%"

endlocal & exit /b %RC%
