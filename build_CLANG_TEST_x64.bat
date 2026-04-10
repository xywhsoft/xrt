@echo off
setlocal
cd /d "%~dp0"

call "%~dp0build_CLANG_ENV_x64.bat" || exit /b 1

if not exist release\x64 (
	mkdir release\x64 || exit /b 1
)

del /f /q release\x64\test.exe >nul 2>nul

"%XRT_CLANG_CL%" /nologo /TC /utf-8 /std:c11 /W3 /Fe:release\x64\test.exe /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS /D_WINSOCK_DEPRECATED_NO_WARNINGS test.c /link Ws2_32.lib IPHLPAPI.lib Shell32.lib || exit /b 1

release\x64\test.exe preset:runtime_smoke || exit /b 1
release\x64\test.exe xurl_core || exit /b 1
release\x64\test.exe preset:xnet2_stage || exit /b 1

echo.
echo build_CLANG_TEST_x64.bat: PASS
exit /b 0
