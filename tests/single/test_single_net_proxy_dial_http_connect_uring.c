/* 复用完整单头 HTTP CONNECT 拨号契约验证 Linux io_uring。 */
#if defined(__linux__)
	#define TEST_SINGLE_PROXY_DIAL_BACKEND XNET_PORT_URING
	#define TEST_SINGLE_PROXY_HTTP_CONNECT 1
	#include "test_single_net_proxy_dial.c"
#else



/* 非 Linux 平台不执行单头 io_uring HTTP CONNECT 拨号契约。 */
int main(void)
{
	return 0;
}
#endif
