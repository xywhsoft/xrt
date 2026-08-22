/* 使用同一组托管代理契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_PROXY_DIAL_BACKEND XNET_PORT_URING
	#include "test_net_proxy_dial.c"
#else



/* 非 Linux 平台不执行 io_uring 代理拨号契约。 */
int main(void)
{
	return 0;
}
#endif
