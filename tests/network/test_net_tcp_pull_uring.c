/* 使用同一拉取契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TCP_PULL_BACKEND XNET_PORT_URING
	#define TEST_TCP_PULL_BACKEND_NAME "io_uring"
	#include "test_net_tcp_pull.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring TCP 拉取契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring TCP pull placeholder");
	return 0;
}
#endif
