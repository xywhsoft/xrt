/* 使用同一组拉取和 Future 契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TCP_FUTURE_BACKEND XNET_PORT_URING
	#define TEST_TCP_FUTURE_BACKEND_NAME "io_uring"
	#include "test_net_tcp_future.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring TCP Future 契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring TCP Future placeholder");
	return 0;
}
#endif
