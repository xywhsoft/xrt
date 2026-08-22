/* 复用完整单头 HTTPS Server 契约验证 Linux io_uring。 */
#if defined(__linux__)
	#define TEST_SINGLE_HTTP_SERVER_TLS_BACKEND XNET_PORT_URING
	#define TEST_SINGLE_HTTP_SERVER_TLS_BACKEND_NAME "single io_uring"
	#include "test_single_http_server_tls_transport.c"
#else



/* 非 Linux 平台不实例化 io_uring 单头 HTTPS Server 契约。 */
int main(void)
{
	return 0;
}
#endif
