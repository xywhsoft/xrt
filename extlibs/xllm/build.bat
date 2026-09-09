@echo off
setlocal

if not defined XRT_DIR set "XRT_DIR=..\xrt"
set "XRT_INCLUDE=%XRT_DIR%\single"

if not exist build mkdir build || exit /b 1
if not exist release mkdir release || exit /b 1

gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -I. -I"%XRT_INCLUDE%" -c xllm.c -o release\xllm.o || exit /b 1
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -I. -I"%XRT_INCLUDE%" -c xllm-session.c -o release\xllm-session.o || exit /b 1
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -I. -I"%XRT_INCLUDE%" -c xllm-memory.c -o release\xllm-memory.o || exit /b 1
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -I. -I"%XRT_INCLUDE%" -c xllm-xrt.c -o release\xllm-xrt.o || exit /b 1
set "XRT_LIBS=-lWs2_32 -lIPHLPAPI -lBcrypt -lCrypt32 -lSecur32 -lAdvapi32"
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections tests\test_xllm.c release\xllm-xrt.o -I. -I"%XRT_INCLUDE%" %XRT_LIBS% -o build\test_xllm.exe || exit /b 1
build\test_xllm.exe || exit /b 1
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections tests\test_xllm_session.c release\xllm-xrt.o -I. -I"%XRT_INCLUDE%" %XRT_LIBS% -o build\test_xllm_session.exe || exit /b 1
build\test_xllm_session.exe || exit /b 1
gcc -m64 -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections tests\test_xllm_memory.c release\xllm-xrt.o -I. -I"%XRT_INCLUDE%" %XRT_LIBS% -o build\test_xllm_memory.exe || exit /b 1
build\test_xllm_memory.exe || exit /b 1

echo.
echo xllm build: PASS
exit /b 0
