/* 使用同一组 readiness 契约测试验证 Linux epoll 后端。 */
#define TEST_PORT_BACKEND XNET_PORT_EPOLL
#define TEST_PORT_BACKEND_NAME "epoll"

#if defined(__linux__)
	#define TEST_PORT_AVAILABLE 1
#else
	#define TEST_PORT_AVAILABLE 0
#endif

#include "test_net_port_select.c"
