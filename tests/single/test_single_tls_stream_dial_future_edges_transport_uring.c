/* 复用单头 TLS Dial Future 边界契约验证 Linux io_uring。 */
#if defined(__linux__)
	#define TEST_SINGLE_TLS_DIAL_FUTURE_BACKEND XNET_PORT_URING
	#include "test_single_tls_stream_dial_future_edges_transport.c"
#else



/* 非 Linux 平台只验证包装入口可独立构建。 */
int main(void)
{
	return 0;
}
#endif
