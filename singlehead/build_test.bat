@echo off
cd /d "%~dp0"

echo ========================================
echo   XRT Single Header Test Builder
echo ========================================
echo.

echo Compiling test_singlehead.c...
tcc test_singlehead.c -o test_singlehead.exe

if errorlevel 1 (
    echo.
    echo [ERROR] Compilation failed!
    echo.
    pause
    exit /b 1
)

echo.
echo Compiling test_singlehead_trim.c...
tcc test_singlehead_trim.c -o test_singlehead_trim.exe

if errorlevel 1 (
    echo.
    echo [ERROR] Trim compilation failed!
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Compilation Successful!
echo ========================================
echo.

echo Running test_singlehead.exe...
echo.

test_singlehead.exe

if errorlevel 1 (
    echo.
    echo [ERROR] test_singlehead.exe failed!
    echo.
    pause
    exit /b 1
)

echo.
echo Running test_singlehead_trim.exe...
echo.

test_singlehead_trim.exe

if errorlevel 1 (
    echo.
    echo [ERROR] test_singlehead_trim.exe failed!
    echo.
    pause
    exit /b 1
)

echo.
echo.
pause
