@echo off
setlocal

rem Queue-focused GCC regression:
rem normal runtime + trim runtime + singlehead + size guard.

if not exist release\x64 (
	mkdir release\x64 || goto :eof
)

gcc -m64 test.c -I . -lWs2_32 -lIPHLPAPI -O0 -g -o release/x64/test_queue.exe || goto :eof

release\x64\test_queue.exe queue_core || goto :eof
release\x64\test_queue.exe thread_core || goto :eof
release\x64\test_queue.exe xnet2_engine || goto :eof

gcc -m64 test_trim.c -I . -lWs2_32 -lIPHLPAPI -O0 -g -o release/x64/test_trim.exe || goto :eof
release\x64\test_trim.exe || goto :eof

if not exist singlehead\single_head_maker.exe (
	echo [ERROR] singlehead\single_head_maker.exe not found.
	goto :eof
)

singlehead\single_head_maker.exe -o singlehead\xrt.h -c xrt.c -s . || goto :eof
copy /y singlehead\xrt.h tools\single_head_maker\xrt.h >nul || goto :eof

gcc -m64 singlehead\test_singlehead.c -I singlehead -lWs2_32 -lIPHLPAPI -O0 -g -o release/x64/test_singlehead_queue.exe || goto :eof
release\x64\test_singlehead_queue.exe || goto :eof

gcc -m64 singlehead\test_singlehead_trim.c -I singlehead -lWs2_32 -lIPHLPAPI -O0 -g -o release/x64/test_singlehead_trim.exe || goto :eof
release\x64\test_singlehead_trim.exe || goto :eof

call :run_noqueue_probe release\x64\__trim_noqueue_probe.c xrt.h normal || goto :eof
call :run_noqueue_probe release\x64\__singlehead_trim_noqueue_probe.c singlehead/xrt.h singlehead || goto :eof
call build_GCC_SIZE_GUARD_x64.bat || goto :eof

echo.
echo build_GCC_QUEUE_REGRESSION_x64.bat: PASS
goto :eof

:run_noqueue_probe
set PROBE_FILE=%~1
set PROBE_HEADER=%~2
set PROBE_NAME=%~3

> "%PROBE_FILE%" echo #define XRT_NO_QUEUE
>> "%PROBE_FILE%" echo #define XRT_NO_NETWORK
>> "%PROBE_FILE%" echo #include "%PROBE_HEADER%"
>> "%PROBE_FILE%" echo int main(void) { xqueue_config cfg; return (int)sizeof(cfg); }

gcc -m64 "%PROBE_FILE%" -I . -c -o NUL > "%PROBE_FILE%.log" 2>&1
if not errorlevel 1 (
	echo [ERROR] %PROBE_NAME% no-queue probe compiled unexpectedly.
	type "%PROBE_FILE%.log"
	del /f /q "%PROBE_FILE%" "%PROBE_FILE%.log" >nul 2>nul
	exit /b 1
)

findstr /c:"xqueue_config" "%PROBE_FILE%.log" >nul
if errorlevel 1 (
	echo [ERROR] %PROBE_NAME% no-queue probe failed for an unexpected reason.
	type "%PROBE_FILE%.log"
	del /f /q "%PROBE_FILE%" "%PROBE_FILE%.log" >nul 2>nul
	exit /b 1
)

del /f /q "%PROBE_FILE%" "%PROBE_FILE%.log" >nul 2>nul
exit /b 0
