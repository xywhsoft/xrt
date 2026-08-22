/* 复用完整单头 UDP 回环契约验证 Linux io_uring。 */
#if defined(__linux__)
	#define TEST_SINGLE_UDP_BACKEND XNET_PORT_URING
	#include "test_single_net_udp.c"
#else



/* 非 Linux 平台不执行单头 io_uring UDP 契约。 */
int main(void)
{
	return 0;
}
#endif
