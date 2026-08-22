/* 使用同一组高层多播契约测试验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_UDP_BACKEND XNET_PORT_URING
	#define TEST_UDP_BACKEND_NAME "io_uring"
	#include "test_net_udp_multicast.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring UDP 多播契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring UDP multicast placeholder");
	return 0;
}
#endif
