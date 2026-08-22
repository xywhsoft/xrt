/* 使用同一组高层多播契约测试验证 Windows IOCP 后端。 */
#define TEST_UDP_BACKEND XNET_PORT_IOCP
#define TEST_UDP_BACKEND_NAME "IOCP"
#include "test_net_udp_multicast.c"
