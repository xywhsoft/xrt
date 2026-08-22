/* 使用同一组发送并发测试验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_UDP_BACKEND XNET_PORT_URING
	#define TEST_UDP_BACKEND_NAME "io_uring"
	#define TEST_UDP_COMPLETION 1
	#include "test_net_udp_send_concurrency.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring UDP 发送并发契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring UDP concurrency placeholder");
	return 0;
}
#endif
