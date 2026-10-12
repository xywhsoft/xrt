/* 使用同一组 HTTPS 慢读契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_HTTP_SERVER_TLS_BACKEND XNET_PORT_URING
	#define TEST_HTTP_SERVER_TLS_BACKEND_NAME "io_uring"
	#include "test_http_server_tls_write.c"
#else



/* 非 Linux 平台不执行 io_uring HTTPS 慢读契约。 */
int main(void)
{
	return 0;
}
#endif
