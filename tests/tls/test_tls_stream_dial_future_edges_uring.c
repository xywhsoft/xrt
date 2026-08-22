/* 使用同一边界契约验证 Linux io_uring TLS Dial Future。 */
#if defined(__linux__)
	#define TEST_TLS_DIAL_FUTURE_BACKEND XNET_PORT_URING
	#include "test_tls_stream_dial_future_edges.c"
#else



/* 非 Linux 平台不执行 io_uring TLS Dial Future 边界契约。 */
int main(void)
{
	return 0;
}
#endif
