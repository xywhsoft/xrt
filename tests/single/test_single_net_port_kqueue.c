/* 使用统一单头文件场景验证 Darwin/BSD kqueue 后端。 */
#define TEST_SINGLE_PORT_BACKEND XNET_PORT_KQUEUE

#if defined(__APPLE__) || defined(__FreeBSD__) || \
	defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
	#define TEST_SINGLE_PORT_AVAILABLE 1
#else
	#define TEST_SINGLE_PORT_AVAILABLE 0
#endif

#include "test_single_net_port_epoll.c"
