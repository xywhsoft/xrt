/* 使用同一终态生命周期契约验证 Windows IOCP。 */
#define TEST_TCP_BACKEND XNET_PORT_IOCP
#define TEST_TCP_BACKEND_NAME "IOCP"
#include "test_net_tcp_lifetime.c"
