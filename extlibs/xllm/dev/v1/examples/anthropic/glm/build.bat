gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION glm_stateless.c -o glm_stateless.exe -lws2_32 -liphlpapi -lshell32
gcc -std=c11 -Wall -Wextra -I..\..\..\singlehead -I..\..\..\lib -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION glm_session.c -o glm_session.exe -lws2_32 -liphlpapi -lshell32
