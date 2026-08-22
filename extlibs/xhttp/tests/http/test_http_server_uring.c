/* 使用同一组 HTTP Server 契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_HTTP_SERVER_BACKEND XNET_PORT_URING
	#define TEST_HTTP_SERVER_BACKEND_NAME "io_uring"
	#include "test_http_server.c"
#else



/* 非 Linux 平台不执行 io_uring HTTP Server 契约。 */
int main(void)
{
	return 0;
}
#endif
