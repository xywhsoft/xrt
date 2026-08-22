/* 使用同一组阻塞 UDP 契约验证 Windows IOCP。 */
#define TEST_UDP_SYNC_BACKEND XNET_PORT_IOCP
#define TEST_UDP_SYNC_BACKEND_NAME "IOCP"
#include "test_net_udp_sync.c"
