/* 使用同一组取消和关闭竞态验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_UDP_FUTURE_THREADS_BACKEND XNET_PORT_URING
	#define TEST_UDP_FUTURE_THREADS_BACKEND_NAME "io_uring"
	#include "test_net_udp_future_threads.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring UDP Future 并发契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring UDP Future thread placeholder");
	return 0;
}
#endif
