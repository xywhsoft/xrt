/* 使用同一 Accept Future 契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TCP_ACCEPT_BACKEND XNET_PORT_URING
	#define TEST_TCP_ACCEPT_BACKEND_NAME "io_uring"
	#include "test_net_tcp_accept_future.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring Accept Future 契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring Accept Future placeholder");
	return 0;
}
#endif
