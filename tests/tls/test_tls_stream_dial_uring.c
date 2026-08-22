/* 使用同一组 TLS Dial 契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TLS_DIAL_BACKEND XNET_PORT_URING
	#include "test_tls_stream_dial.c"
#else



/* 非 Linux 平台不执行 io_uring TLS Dial 契约。 */
int main(void)
{
	return 0;
}
#endif
