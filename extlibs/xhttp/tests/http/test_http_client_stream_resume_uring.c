/* 使用同一组正文恢复契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_URING
	#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "io_uring"
	#include "test_http_client_stream_resume.c"
#else



/* 非 Linux 平台不执行 io_uring 正文恢复契约。 */
int main(void)
{
	return 0;
}
#endif
