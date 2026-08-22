/* 使用同一组失败原子性测试验证 Linux epoll 后端。 */
#define TEST_PORT_BACKEND XNET_PORT_EPOLL

#if defined(__linux__)
	#define TEST_PORT_AVAILABLE 1
#else
	#define TEST_PORT_AVAILABLE 0
#endif

#include "test_net_port_oom.c"
