/* 复用完整单头 TLS Dial 契约验证 Linux io_uring。 */
#if defined(__linux__)
	#define TEST_SINGLE_TLS_DIAL_BACKEND XNET_PORT_URING
	#include "test_single_tls_stream_dial_transport.c"
#else



/* 非 Linux 平台不执行单头 io_uring TLS Dial 契约。 */
int main(void)
{
	return 0;
}
#endif
