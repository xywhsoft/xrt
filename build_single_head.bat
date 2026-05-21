@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   XRT Single Header Builder
echo ========================================
echo.

set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

set TOOLS_DIR=%SCRIPT_DIR%\tools\single_head_maker
set OUTPUT_DIR=%SCRIPT_DIR%\singlehead
set SOURCE_DIR=%SCRIPT_DIR%

echo Script directory: %SCRIPT_DIR%
echo Tools directory: %TOOLS_DIR%
echo Output directory: %OUTPUT_DIR%
echo Source directory: %SOURCE_DIR%
echo.

if not exist "%OUTPUT_DIR%" (
    echo Creating output directory: %OUTPUT_DIR%
    mkdir "%OUTPUT_DIR%"
)

echo.
echo Step 1: Building single_head_maker tool...
echo.

cd /d "%TOOLS_DIR%"

if exist "%OUTPUT_DIR%\single_head_maker.exe" (
    del /f /q "%OUTPUT_DIR%\single_head_maker.exe"
)

echo Compiling single_head_maker.c...

tcc single_head_maker.c -lShell32 -lWs2_32 -lIPHLPAPI -o "%OUTPUT_DIR%\single_head_maker.exe"

if errorlevel 1 (
    echo.
    echo [ERROR] Failed to compile single_head_maker.c
    echo.
    cd /d "%SCRIPT_DIR%"
    pause
    exit /b 1
)

echo.
echo Step 2: Generating single header file...
echo.

%OUTPUT_DIR%\single_head_maker.exe

if errorlevel 1 (
    echo.
    echo [ERROR] Failed to generate single header file
    echo.
    cd /d "%SCRIPT_DIR%"
    pause
    exit /b 1
)

echo.
echo Step 3: Verifying output...
echo.

cd /d "%SCRIPT_DIR%"

if exist "%OUTPUT_DIR%\xrt.h" (
	for %%A in ("%OUTPUT_DIR%\xrt.h") do set SIZE=%%~zA
	echo Generated file size: !SIZE! bytes
    
    echo.
    echo ========================================
    echo   Build Successful!
    echo ========================================
    echo.
    echo Single header file created at:
    echo   %OUTPUT_DIR%\xrt.h
    echo.
    echo Usage example:
    echo   #define XRT_IMPLEMENTATION
    echo   #include "xrt.h"
    echo.
) else (
    echo.
    echo [ERROR] Output file not found!
    echo.
    pause
    exit /b 1
)

echo.
pause
