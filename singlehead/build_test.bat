@echo off
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
echo ========================================
echo   Compilation Successful!
echo ========================================
echo.

echo Running test...
echo.

test_singlehead.exe

echo.
echo.
pause
