@echo off
setlocal

if not defined XRT_DIR set "XRT_DIR=..\xrt"
if not defined XLLM_DIR set "XLLM_DIR=..\xllm"
set "XRT_INCLUDE=%XRT_DIR%\single"

if not exist build mkdir build || exit /b 1
if not exist release mkdir release || exit /b 1

gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -I. -I"%XLLM_DIR%" -I"%XRT_INCLUDE%" -c xwork.c -o release\xwork.o || exit /b 1
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -I. -I"%XLLM_DIR%" -I"%XRT_INCLUDE%" -c xwork-xrt.c -o release\xwork-xrt.o || exit /b 1
set "XRT_LIBS=-lWs2_32 -lIPHLPAPI -lBcrypt -lCrypt32 -lSecur32 -lAdvapi32"
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections tests\test_xwork.c release\xwork-xrt.o -I. -I"%XLLM_DIR%" -I"%XRT_INCLUDE%" %XRT_LIBS% -o build\test_xwork.exe || exit /b 1
build\test_xwork.exe || exit /b 1

echo.
echo xwork build: PASS
exit /b 0
