/* 使用同一组 HTTPS Server 契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_HTTP_SERVER_TLS_BACKEND XNET_PORT_URING
	#define TEST_HTTP_SERVER_TLS_BACKEND_NAME "io_uring"
	#include "test_http_server_tls.c"
#else



/* 非 Linux 平台不执行 io_uring HTTPS Server 契约。 */
int main(void)
{
	return 0;
}
#endif
