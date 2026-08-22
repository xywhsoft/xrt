/* 使用同一组契约测试验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_UDP_BACKEND XNET_PORT_URING
	#define TEST_UDP_BACKEND_NAME "io_uring"
	#include "test_net_udp.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring UDP 契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring UDP placeholder");
	return 0;
}
#endif
