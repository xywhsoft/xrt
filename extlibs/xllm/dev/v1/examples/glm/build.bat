@echo off
setlocal
cd /d "%~dp0\..\.."


if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\glm\glm_stateless.c ^
    -o build\glm_native_stateless.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\glm_native_stateless.exe

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\glm\glm_session.c ^
    -o build\glm_native_session.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\glm_native_session.exe

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\glm\glm_reasoning.c ^
    -o build\glm_native_reasoning.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\glm_native_reasoning.exe

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\glm\glm_json_schema.c ^
    -o build\glm_native_json_schema.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\glm_native_json_schema.exe

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\glm\glm_tool_loop.c ^
    -o build\glm_native_tool_loop.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\glm_native_tool_loop.exe

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\glm\glm_multimodal.c ^
    -o build\glm_native_multimodal.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\glm_native_multimodal.exe

gcc -std=c11 -Wall -Wextra -I. -Ilib ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\glm\glm_stream.c ^
    -o build\glm_native_stream.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\glm_native_stream.exe
