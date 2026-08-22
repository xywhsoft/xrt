/* 使用同一慢对端契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TCP_BACKEND XNET_PORT_URING
	#define TEST_TCP_BACKEND_NAME "io_uring"
	#include "test_net_tcp_slow.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring TCP 慢对端契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring TCP slow-peer placeholder");
	return 0;
}
#endif
