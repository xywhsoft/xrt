@echo off
cd /d "%~dp0"

echo ========================================
echo   XRT Single Header Test Builder
echo ========================================
echo.

echo Compiling test_singlehead.c...
tcc -m64 test_singlehead.c -lWs2_32 -lIPHLPAPI -lShell32 -o test_singlehead.exe

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
echo.
pause
