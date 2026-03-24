@echo off
cd /d "%~dp0"

echo This compatibility entry updates the tracked release\x64\xrt.dll artifact.
echo For day-to-day local validation, prefer build_GCC_DLL_x64_local.bat.
echo.

call build_GCC_DLL_x64_release.bat
if errorlevel 1 goto :end

echo.
echo build_GCC_DLL_x64.bat: PASS

:end
pause
