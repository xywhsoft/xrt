/* 使用同一组 HTTP CONNECT 拨号契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_PROXY_DIAL_BACKEND XNET_PORT_URING
	#define TEST_PROXY_DIAL_HTTP_CONNECT 1
	#include "test_net_proxy_dial.c"
#else



/* 非 Linux 平台不执行 io_uring HTTP CONNECT 拨号契约。 */
int main(void)
{
	return 0;
}
#endif
