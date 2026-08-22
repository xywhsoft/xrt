/* 使用同一组契约测试验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TCP_BACKEND XNET_PORT_URING
	#define TEST_TCP_BACKEND_NAME "io_uring"
	#include "test_net_tcp.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring TCP 契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring TCP placeholder");
	return 0;
}
#endif
