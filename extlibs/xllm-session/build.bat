@echo off
setlocal

if not defined XLLM_DIR set "XLLM_DIR=..\xllm"
if not defined XRT_DIR set "XRT_DIR=..\.."
set "XRT_INCLUDE=%XRT_DIR%\single"

if not exist build mkdir build || exit /b 1
if not exist release mkdir release || exit /b 1

gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -I. -I"%XLLM_DIR%" -I"%XRT_INCLUDE%" -c xllm-session.c -o release\xllm-session.o || exit /b 1
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -I. -I"%XRT_INCLUDE%" -c xllm-session-xrt.c -o release\xllm-session-xrt.o || exit /b 1
set "XRT_LIBS=-lWs2_32 -lIPHLPAPI -lBcrypt -lCrypt32 -lSecur32 -lAdvapi32"
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections tests\test_xllm_session.c release\xllm-session-xrt.o -I. -I"%XLLM_DIR%" -I"%XRT_INCLUDE%" %XRT_LIBS% -o build\test_xllm_session.exe || exit /b 1
build\test_xllm_session.exe || exit /b 1

echo.
echo xllm-session build: PASS
exit /b 0
