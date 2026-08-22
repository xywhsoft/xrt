/* 使用统一 readiness 契约验证 Darwin/BSD kqueue 后端。 */
#define TEST_PORT_BACKEND XNET_PORT_KQUEUE
#define TEST_PORT_BACKEND_NAME "kqueue"

#if defined(__APPLE__) || defined(__FreeBSD__) || \
	defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
	#define TEST_PORT_AVAILABLE 1
#else
	#define TEST_PORT_AVAILABLE 0
#endif

#include "test_net_port_select.c"
