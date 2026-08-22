@echo off
setlocal

if "%CC%"=="" set CC=gcc
if "%PYTHON%"=="" set PYTHON=python
if "%FUZZ_ROUNDS%"=="" set FUZZ_ROUNDS=1000
if "%STRESS_ROUNDS%"=="" set STRESS_ROUNDS=500

if not exist release\gate mkdir release\gate || exit /b 1

%CC% -m64 test.c -I . -std=c11 -O2 -Wall -Wextra -lWs2_32 -lIPHLPAPI -o release\gate\http_ws_test.exe || exit /b 1
for %%T in (xurl_core http_semantics xnet_http xnet_httpd xweb xnet_ws) do (
    release\gate\http_ws_test.exe %%T || exit /b 1
)

%CC% -m64 -c xrt.c -I . -std=c11 -O2 -Wall -Wextra -Werror -o release\gate\xrt_strict.o || exit /b 1
%CC% -m64 test\test_xhttp_oom.c -I . -std=c11 -O2 -Wall -Wextra -lWs2_32 -lIPHLPAPI -o release\gate\xhttp_oom.exe || exit /b 1
release\gate\xhttp_oom.exe || exit /b 1

%CC% -m64 test\fuzz_xhttp_protocol.c -I . -std=c11 -O2 -Wall -Wextra -Werror -DXRT_FUZZ_STANDALONE -lWs2_32 -lIPHLPAPI -o release\gate\xhttp_fuzz_standalone.exe || exit /b 1
release\gate\xhttp_fuzz_standalone.exe %FUZZ_ROUNDS% || exit /b 1

%CC% -m64 test\fuzz_xws_protocol.c -I . -std=c11 -O2 -Wall -Wextra -Werror -DXRT_FUZZ_STANDALONE -lWs2_32 -lIPHLPAPI -o release\gate\xws_fuzz_standalone.exe || exit /b 1
release\gate\xws_fuzz_standalone.exe %FUZZ_ROUNDS% || exit /b 1

%CC% -m64 test\interop_xws_peer.c xrt.c -I . -std=c11 -O2 -Wall -Wextra -Werror -lWs2_32 -lIPHLPAPI -o release\gate\xws_interop_peer.exe || exit /b 1
%PYTHON% test\run_xws_interop.py --peer release\gate\xws_interop_peer.exe || exit /b 1

%CC% -m64 test\stress_xws_reconnect.c xrt.c -I . -std=c11 -O2 -Wall -Wextra -Werror -lWs2_32 -lIPHLPAPI -o release\gate\xws_reconnect_stress.exe || exit /b 1
release\gate\xws_reconnect_stress.exe %STRESS_ROUNDS% || exit /b 1

%CC% -m64 singlehead\test_singlehead.c -I singlehead -std=c11 -O2 -Wall -Wextra -lWs2_32 -lIPHLPAPI -o release\gate\singlehead_http_ws.exe || exit /b 1
release\gate\singlehead_http_ws.exe || exit /b 1

echo.
echo HTTP/WebSocket release gate: PASS
exit /b 0
