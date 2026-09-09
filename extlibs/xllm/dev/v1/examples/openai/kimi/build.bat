gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION kimi_stateless.c -o kimi_stateless.exe -lws2_32 -liphlpapi -lshell32 -lcrypt32
gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION kimi_session.c -o kimi_session.exe -lws2_32 -liphlpapi -lshell32 -lcrypt32
