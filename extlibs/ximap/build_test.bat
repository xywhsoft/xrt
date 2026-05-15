@echo off
setlocal

set GCC_WARN=-Wall -Wextra

if not exist .\bin (
	mkdir .\bin || exit /b 1
)

gcc -m64 ximap_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\ximap_test.exe || exit /b 1

.\bin\ximap_test.exe || exit /b 1

echo.
echo build_test.bat: PASS
exit /b 0
