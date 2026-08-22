/* 使用同一连接定时器回收契约验证 Windows IOCP。 */
#define TEST_TCP_BACKEND XNET_PORT_IOCP
#define TEST_TCP_BACKEND_NAME "IOCP"
#include "test_net_tcp_connect_timer.c"
