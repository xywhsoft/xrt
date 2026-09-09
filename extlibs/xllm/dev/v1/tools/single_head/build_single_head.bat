@echo off
setlocal

echo ========================================
echo   XLLM Single Header Builder
echo ========================================
echo.

set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
for %%I in ("%SCRIPT_DIR%\..\..") do set REPO_ROOT=%%~fI
set OUTPUT_DIR=%REPO_ROOT%\singlehead
set TOOL_SRC=%REPO_ROOT%\tools\single_head_maker\single_head_maker.c
set TOOL_EXE=%OUTPUT_DIR%\single_head_maker.exe
set INPUT_FILE_CORE=%REPO_ROOT%\xllm.h
set OUTPUT_FILE_CORE=%OUTPUT_DIR%\xllm.h
set INPUT_FILE_SESSION=%REPO_ROOT%\xllm-session.h
set OUTPUT_FILE_SESSION=%OUTPUT_DIR%\xllm-session.h
set INPUT_FILE_MEMORY=%REPO_ROOT%\xllm-memory.h
set OUTPUT_FILE_MEMORY=%OUTPUT_DIR%\xllm-memory.h

if not exist "%OUTPUT_DIR%" (
    mkdir "%OUTPUT_DIR%"
)

echo Building tool...
gcc -std=c11 -O2 "%TOOL_SRC%" -o "%TOOL_EXE%"
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to build single_head_maker
    exit /b 1
)

echo.
echo Generating core single header...
"%TOOL_EXE%" -i "%INPUT_FILE_CORE%" -o "%OUTPUT_FILE_CORE%"
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to generate core single header
    exit /b 1
)

echo.
echo Generating session single header...
"%TOOL_EXE%" -i "%INPUT_FILE_SESSION%" -o "%OUTPUT_FILE_SESSION%"
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to generate session single header
    exit /b 1
)

echo.
echo Generating memory single header...
"%TOOL_EXE%" -i "%INPUT_FILE_MEMORY%" -o "%OUTPUT_FILE_MEMORY%"
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to generate memory single header
    exit /b 1
)

echo.
echo Core output:    %OUTPUT_FILE_CORE%
echo Session output: %OUTPUT_FILE_SESSION%
echo Memory output:  %OUTPUT_FILE_MEMORY%
echo Done.
