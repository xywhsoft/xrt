/* 使用同一组阻塞 TCP Dial 契约验证 Windows IOCP。 */
#define TEST_TCP_DIAL_SYNC_BACKEND XNET_PORT_IOCP
#define TEST_TCP_DIAL_SYNC_BACKEND_NAME "IOCP"
#include "test_net_tcp_dial_sync.c"
