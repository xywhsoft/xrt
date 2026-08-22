/* 使用统一失败原子性契约验证 Darwin/BSD kqueue 后端。 */
#define TEST_PORT_BACKEND XNET_PORT_KQUEUE

#if defined(__APPLE__) || defined(__FreeBSD__) || \
	defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
	#define TEST_PORT_AVAILABLE 1
#else
	#define TEST_PORT_AVAILABLE 0
#endif

#include "test_net_port_oom.c"
