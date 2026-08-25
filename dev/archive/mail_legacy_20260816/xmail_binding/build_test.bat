@echo off
setlocal

set GCC_WARN=-Wall -Wextra

if not exist .\bin (
	mkdir .\bin || exit /b 1
)

gcc -m64 xmail_xlang.c xmail_xlang_test.c -I . -I ..\..\singlehead -I ..\xmail_mime -I ..\xsmtp -I ..\xpop3 -I ..\ximap -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xmail_xlang_test.exe || exit /b 1
gcc -m64 xmail_aggregate_test.c -I . -I ..\..\singlehead -I ..\xmail_mime -I ..\xsmtp -I ..\xpop3 -I ..\ximap -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xmail_aggregate_test.exe || exit /b 1
gcc -m64 xmail_live_test.c -I . -I ..\..\singlehead -I ..\xmail_mime -I ..\xsmtp -I ..\xpop3 -I ..\ximap -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g %GCC_WARN% -o .\bin\xmail_live_test.exe || exit /b 1

.\bin\xmail_xlang_test.exe || exit /b 1
.\bin\xmail_aggregate_test.exe || exit /b 1

echo.
echo build_test.bat: PASS
exit /b 0
