@echo off
setlocal enabledelayedexpansion

rem DLL size guard:
rem compare full build against a no-queue/no-network trim build.

if not exist release\x64 (
	mkdir release\x64 || goto :eof
)

if not defined XRT_SIZE_GUARD_MAX_TRIM_PERCENT (
	set XRT_SIZE_GUARD_MAX_TRIM_PERCENT=80
)

if not defined XRT_SIZE_GUARD_MIN_DELTA_BYTES (
	set XRT_SIZE_GUARD_MIN_DELTA_BYTES=131072
)

if not defined XRT_SIZE_GUARD_KEEP_ARTIFACTS (
	set XRT_SIZE_GUARD_KEEP_ARTIFACTS=0
)

set FULL_DLL=release\x64\xrt_size_full.dll
set TRIM_DLL=release\x64\xrt_size_trim.dll
set REPORT_TXT=release\x64\xrt_size_report.txt

del /f /q "%FULL_DLL%" "%TRIM_DLL%" "%REPORT_TXT%" >nul 2>nul

gcc -m64 -shared xrt.c -O2 -s -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections -DBUILD_DLL -lWs2_32 -lIPHLPAPI -o "%FULL_DLL%" || goto :eof
gcc -m64 -shared xrt.c -O2 -s -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections -DBUILD_DLL -DXRT_NO_QUEUE -DXRT_NO_NETWORK -lWs2_32 -lIPHLPAPI -o "%TRIM_DLL%" || goto :eof

for %%A in ("%FULL_DLL%") do set FULL_SIZE=%%~zA
for %%A in ("%TRIM_DLL%") do set TRIM_SIZE=%%~zA

if not defined FULL_SIZE goto :eof
if not defined TRIM_SIZE goto :eof

set /a SIZE_DELTA=!FULL_SIZE!-!TRIM_SIZE!
set /a TRIM_PCT=(100*!TRIM_SIZE!)/!FULL_SIZE!

if !TRIM_SIZE! GEQ !FULL_SIZE! (
	echo [ERROR] Trim DLL is not smaller than full DLL.
	echo Full DLL bytes: !FULL_SIZE!
	echo Trim DLL bytes: !TRIM_SIZE!
	goto :eof
)

if !TRIM_PCT! GTR !XRT_SIZE_GUARD_MAX_TRIM_PERCENT! (
	echo [ERROR] Trim DLL percent exceeded guard threshold.
	echo Full DLL bytes: !FULL_SIZE!
	echo Trim DLL bytes: !TRIM_SIZE!
	echo Trim percent: !TRIM_PCT!%%
	echo Max allowed trim percent: !XRT_SIZE_GUARD_MAX_TRIM_PERCENT!%%
	goto :eof
)

if !SIZE_DELTA! LSS !XRT_SIZE_GUARD_MIN_DELTA_BYTES! (
	echo [ERROR] Full-vs-trim DLL delta fell below guard threshold.
	echo Full DLL bytes: !FULL_SIZE!
	echo Trim DLL bytes: !TRIM_SIZE!
	echo Size delta bytes: !SIZE_DELTA!
	echo Min required delta bytes: !XRT_SIZE_GUARD_MIN_DELTA_BYTES!
	goto :eof
)

(
	echo XRT GCC Size Guard x64
	echo Full DLL: !FULL_DLL!
	echo Full size bytes: !FULL_SIZE!
	echo Trim DLL: !TRIM_DLL!
	echo Trim size bytes: !TRIM_SIZE!
	echo Size delta bytes: !SIZE_DELTA!
	echo Trim size percent of full: !TRIM_PCT!%%
	echo Max allowed trim percent: !XRT_SIZE_GUARD_MAX_TRIM_PERCENT!%%
	echo Min required delta bytes: !XRT_SIZE_GUARD_MIN_DELTA_BYTES!
	echo Keep artifacts: !XRT_SIZE_GUARD_KEEP_ARTIFACTS!
) > "%REPORT_TXT%"

type "%REPORT_TXT%"

if "!XRT_SIZE_GUARD_KEEP_ARTIFACTS!"=="0" (
	del /f /q "%FULL_DLL%" "%TRIM_DLL%" >nul 2>nul
	echo.
	echo Temporary size-guard DLLs removed.
)

echo.
echo build_GCC_SIZE_GUARD_x64.bat: PASS
goto :eof
