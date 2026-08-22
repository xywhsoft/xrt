/* 使用同一组托管连接边界契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TCP_DIAL_BACKEND XNET_PORT_URING
	#include "test_net_tcp_dial_edges.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring TCP Dial 边界契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring TCP Dial edge placeholder");
	return 0;
}
#endif
