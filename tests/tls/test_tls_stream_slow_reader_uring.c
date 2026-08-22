/* 使用同一组慢读方契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TLS_STREAM_BACKEND XNET_PORT_URING
	#define TEST_TLS_STREAM_BACKEND_NAME "io_uring slow reader"
	#define TEST_TLS_STREAM_SLOW_READER
	#include "test_tls_stream.c"
#else



/* 非 Linux 平台不执行 io_uring TLS 慢读方契约。 */
int main(void)
{
	return 0;
}
#endif
