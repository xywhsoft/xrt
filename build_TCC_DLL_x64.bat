@echo off
setlocal

echo This compatibility entry updates the tracked release\x64\xrt.dll artifact.
echo For day-to-day local validation, prefer build_TCC_DLL_x64_local.bat.
echo.

call build_TCC_DLL_x64_release.bat
if errorlevel 1 goto :eof

echo.
echo build_TCC_DLL_x64.bat: PASS
pause
