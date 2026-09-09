gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION azure_openai_stateless.c -o azure_openai_stateless.exe -lws2_32 -liphlpapi -lshell32
gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION azure_openai_session.c -o azure_openai_session.exe -lws2_32 -liphlpapi -lshell32
gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION azure_openai_stream.c -o azure_openai_stream.exe -lws2_32 -liphlpapi -lshell32
gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION azure_openai_json_schema.c -o azure_openai_json_schema.exe -lws2_32 -liphlpapi -lshell32
gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION azure_openai_tool_loop.c -o azure_openai_tool_loop.exe -lws2_32 -liphlpapi -lshell32
gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION azure_openai_multimodal.c -o azure_openai_multimodal.exe -lws2_32 -liphlpapi -lshell32
