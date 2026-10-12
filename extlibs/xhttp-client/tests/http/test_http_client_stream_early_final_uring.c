/* 使用同一份提前最终响应契约验证 Linux io_uring。 */
#if defined(__linux__)
	#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_URING
	#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "io_uring"
	#include "test_http_client_stream_early_final.c"
#else



/* 非 Linux 平台不执行 io_uring 提前最终响应契约。 */
int main(void)
{
	return 0;
}
#endif
