/* 使用同一并发发送关闭契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TCP_BACKEND XNET_PORT_URING
	#define TEST_TCP_BACKEND_NAME "io_uring"
	#include "test_net_tcp_threads.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring TCP 并发契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring TCP thread placeholder");
	return 0;
}
#endif
