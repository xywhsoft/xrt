/* 复用完整单头 TLS-over-TCP 契约验证 Linux io_uring。 */
#if defined(__linux__)
	#define TEST_SINGLE_TLS_STREAM_BACKEND XNET_PORT_URING
	#define TEST_SINGLE_TLS_STREAM_BACKEND_NAME "single io_uring"
	#include "test_single_tls_stream_transport.c"
#else



/* 非 Linux 平台不执行单头 io_uring TLS Stream 契约。 */
int main(void)
{
	return 0;
}
#endif
