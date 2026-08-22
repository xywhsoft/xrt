/* 使用同一组边界测试验证 Windows IOCP 后端。 */
#define TEST_UDP_BACKEND XNET_PORT_IOCP
#define TEST_UDP_BACKEND_NAME "IOCP"
#define TEST_UDP_COMPLETION 1
#include "test_net_udp_edges.c"
