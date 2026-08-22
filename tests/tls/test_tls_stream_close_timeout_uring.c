/* 使用同一组认证关闭超时契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TLS_STREAM_BACKEND XNET_PORT_URING
	#include "test_tls_stream_close_timeout.c"
#else



/* 非 Linux 平台不执行 io_uring TLS 认证关闭契约。 */
int main(void)
{
	return 0;
}
#endif
