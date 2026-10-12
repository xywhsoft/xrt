/* 使用同一份高层 HTTP 客户端契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_HTTP_CLIENT_BACKEND XNET_PORT_URING
	#define TEST_HTTP_CLIENT_BACKEND_NAME "io_uring"
	#include "test_http_client.c"
#else



/* 非 Linux 平台不执行 io_uring HTTP Client 契约。 */
int main(void)
{
	return 0;
}
#endif
