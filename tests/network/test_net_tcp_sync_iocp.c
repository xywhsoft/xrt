/* 使用同一组阻塞 TCP 契约验证 Windows IOCP。 */
#define TEST_TCP_SYNC_BACKEND XNET_PORT_IOCP
#define TEST_TCP_SYNC_BACKEND_NAME "IOCP"
#include "test_net_tcp_sync.c"
