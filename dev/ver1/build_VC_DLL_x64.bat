@echo off
setlocal
cd /d "%~dp0"

call "%~dp0build_VC_ENV_x64.bat" || exit /b 1

if not exist release\x64 (
	mkdir release\x64 || exit /b 1
)

del /f /q release\x64\xrt.dll >nul 2>nul
del /f /q release\x64\xrt.lib >nul 2>nul
del /f /q release\x64\xrt.exp >nul 2>nul

cl /nologo /TC /utf-8 /std:c11 /W3 /O2 /DBUILD_DLL /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS /D_WINSOCK_DEPRECATED_NO_WARNINGS /LD xrt.c /link /OUT:release\x64\xrt.dll /IMPLIB:release\x64\xrt.lib Ws2_32.lib IPHLPAPI.lib Shell32.lib || exit /b 1

echo.
echo Build successful: release\x64\xrt.dll
exit /b 0
