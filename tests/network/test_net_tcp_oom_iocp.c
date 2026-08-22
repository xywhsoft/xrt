/* 使用同一内存与生命周期契约验证 Windows IOCP。 */
#define TEST_TCP_OOM_BACKEND XNET_PORT_IOCP
#define TEST_TCP_OOM_BACKEND_NAME "IOCP"
#include "test_net_tcp_oom.c"
