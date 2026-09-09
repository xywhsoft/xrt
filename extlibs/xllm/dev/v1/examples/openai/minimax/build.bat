gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION minimax_stateless.c -o minimax_stateless.exe -lws2_32 -liphlpapi -lshell32 -lcrypt32
gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION minimax_session.c -o minimax_session.exe -lws2_32 -liphlpapi -lshell32 -lcrypt32
