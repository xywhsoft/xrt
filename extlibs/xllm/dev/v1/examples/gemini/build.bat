@echo off
setlocal
cd /d "%~dp0\..\.."
if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\gemini_stateless.c ^
    -o build\gemini_native_stateless.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\gemini_stream.c ^
    -o build\gemini_native_stream.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\gemini_session.c ^
    -o build\gemini_native_session.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\vertex_gemini_stateless.c ^
    -o build\vertex_gemini_native_stateless.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\vertex_gemini_stream.c ^
    -o build\vertex_gemini_native_stream.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\vertex_gemini_session.c ^
    -o build\vertex_gemini_native_session.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\gemini_tool_loop.c ^
    -o build\gemini_native_tool_loop.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\vertex_gemini_tool_loop.c ^
    -o build\vertex_gemini_native_tool_loop.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\gemini_multimodal.c ^
    -o build\gemini_native_multimodal.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\vertex_gemini_multimodal.c ^
    -o build\vertex_gemini_native_multimodal.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\gemini_json_schema.c ^
    -o build\gemini_native_json_schema.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\vertex_gemini_json_schema.c ^
    -o build\vertex_gemini_native_json_schema.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\gemini_reasoning.c ^
    -o build\gemini_native_reasoning.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\gemini\vertex_gemini_reasoning.c ^
    -o build\vertex_gemini_native_reasoning.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32

if errorlevel 1 exit /b %errorlevel%

echo Built build\gemini_native_stateless.exe
echo Built build\gemini_native_stream.exe
echo Built build\gemini_native_session.exe
echo Built build\vertex_gemini_native_stateless.exe
echo Built build\vertex_gemini_native_stream.exe
echo Built build\vertex_gemini_native_session.exe
echo Built build\gemini_native_tool_loop.exe
echo Built build\vertex_gemini_native_tool_loop.exe
echo Built build\gemini_native_multimodal.exe
echo Built build\vertex_gemini_native_multimodal.exe
echo Built build\gemini_native_json_schema.exe
echo Built build\vertex_gemini_native_json_schema.exe
echo Built build\gemini_native_reasoning.exe
echo Built build\vertex_gemini_native_reasoning.exe
