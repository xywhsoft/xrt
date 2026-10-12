/* 使用同一组慢读背压契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_HTTP_SERVER_BACKEND XNET_PORT_URING
	#define TEST_HTTP_SERVER_BACKEND_NAME "io_uring"
	#include "test_http_server_write.c"
#else



/* 非 Linux 平台不执行 io_uring 慢读背压契约。 */
int main(void)
{
	return 0;
}
#endif
