@echo off
setlocal

set GCC_WARN=-Wall -Wextra

if not exist release\x64 (
	mkdir release\x64 || exit /b 1
)

gcc -m64 test.c -I . -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections %GCC_WARN% -o release/x64/test.exe || exit /b 1

gcc -m64 test\test_value_negative_index.c xrt.c -I . -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections %GCC_WARN% -o release/x64/test_value_negative_index.exe || exit /b 1
release\x64\test_value_negative_index.exe || exit /b 1

gcc -m64 test\test_value_shared_publish.c xrt.c -I . -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections %GCC_WARN% -o release/x64/test_value_shared_publish.exe || exit /b 1
release\x64\test_value_shared_publish.exe || exit /b 1

gcc -m64 test\test_value_cow.c xrt.c -I . -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections %GCC_WARN% -o release/x64/test_value_cow.exe || exit /b 1
release\x64\test_value_cow.exe || exit /b 1

gcc -m64 test\test_string_split_lines.c xrt.c -I . -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections %GCC_WARN% -o release/x64/test_string_split_lines.exe || exit /b 1
release\x64\test_string_split_lines.exe || exit /b 1

gcc -m64 test\test_typed_special.c xrt.c -I . -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections %GCC_WARN% -o release/x64/test_typed_special.exe || exit /b 1
release\x64\test_typed_special.exe || exit /b 1

gcc -m64 test\test_xnet_async_stress.c xrt.c -I . -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections %GCC_WARN% -o release/x64/test_xnet_async_stress.exe || exit /b 1
release\x64\test_xnet_async_stress.exe 200 || exit /b 1

release\x64\test.exe preset:runtime_smoke || exit /b 1
release\x64\test.exe xurl_core || exit /b 1
release\x64\test.exe preset:xnet2_stage || exit /b 1

gcc -m64 singlehead\test_singlehead.c -I singlehead -lWs2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections %GCC_WARN% -o release/x64/test_singlehead_queue.exe || exit /b 1
release\x64\test_singlehead_queue.exe || exit /b 1

echo.
echo build_GCC_TEST_x64.bat: PASS
exit /b 0
