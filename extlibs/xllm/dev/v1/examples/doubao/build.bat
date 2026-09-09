@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\doubao\doubao_stateless.c ^
    -o build\doubao_native_stateless.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
if errorlevel 1 exit /b 1

echo Built build\doubao_native_stateless.exe

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\doubao\doubao_stream.c ^
    -o build\doubao_native_stream.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
if errorlevel 1 exit /b 1

echo Built build\doubao_native_stream.exe

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\doubao\doubao_session.c ^
    -o build\doubao_native_session.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
if errorlevel 1 exit /b 1

echo Built build\doubao_native_session.exe

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\doubao\doubao_json_schema.c ^
    -o build\doubao_native_json_schema.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
if errorlevel 1 exit /b 1

echo Built build\doubao_native_json_schema.exe

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\doubao\doubao_reasoning.c ^
    -o build\doubao_native_reasoning.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
if errorlevel 1 exit /b 1

echo Built build\doubao_native_reasoning.exe

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\doubao\doubao_multimodal.c ^
    -o build\doubao_native_multimodal.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\doubao_native_multimodal.exe

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\doubao\doubao_tool_loop.c ^
    -o build\doubao_native_tool_loop.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
if errorlevel 1 exit /b 1

echo Built build\doubao_native_tool_loop.exe
exit /b 0
