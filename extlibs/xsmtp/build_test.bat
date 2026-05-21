@echo off
setlocal

set GCC_WARN=-Wall -Wextra

if not exist .\bin (
	mkdir .\bin || exit /b 1
)

gcc -m64 xsmtp_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g -DXSMTP_DEBUG %GCC_WARN% -o .\bin\xsmtp_test.exe || exit /b 1
gcc -m64 xsmtp_mime_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g -DXSMTP_DEBUG %GCC_WARN% -o .\bin\xsmtp_mime_test.exe || exit /b 1

.\bin\xsmtp_mime_test.exe || exit /b 1

echo.
echo build_test.bat: PASS
exit /b 0
