/* 使用同一连接定时器回收契约验证 Linux io_uring。 */
#if defined(__linux__)
	#define TEST_TCP_BACKEND XNET_PORT_URING
	#define TEST_TCP_BACKEND_NAME "io_uring"
	#include "test_net_tcp_connect_timer.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring TCP 连接定时器回收测试。 */
int main(void)
{
	testRequire(true,
		"non-Linux io_uring TCP connect timer placeholder");
	return 0;
}
#endif
