/* 使用同一组拉取和 Future 契约验证 Windows IOCP。 */
#define TEST_TCP_FUTURE_BACKEND XNET_PORT_IOCP
#define TEST_TCP_FUTURE_BACKEND_NAME "IOCP"
#include "test_net_tcp_future.c"
