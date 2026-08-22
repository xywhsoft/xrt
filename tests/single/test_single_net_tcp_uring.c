/* 复用完整单头 TCP 回环契约验证 Linux io_uring。 */
#if defined(__linux__)
	#define TEST_SINGLE_TCP_BACKEND XNET_PORT_URING
	#include "test_single_net_tcp.c"
#else



/* 非 Linux 平台不执行单头 io_uring TCP 契约。 */
int main(void)
{
	return 0;
}
#endif
