/* 使用同一组契约测试验证 Windows IOCP 后端。 */
#define TEST_TCP_BACKEND XNET_PORT_IOCP
#define TEST_TCP_BACKEND_NAME "IOCP"
#include "test_net_tcp.c"
