/* 使用同一组跨线程契约测试验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_UDP_BACKEND XNET_PORT_URING
	#define TEST_UDP_BACKEND_NAME "io_uring"
	#include "test_net_udp_threads.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring UDP 并发契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring UDP thread placeholder");
	return 0;
}
#endif
