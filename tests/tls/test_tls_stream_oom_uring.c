/* 使用同一组组合对象 OOM 契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TLS_STREAM_BACKEND XNET_PORT_URING
	#include "test_tls_stream_oom.c"
#else



/* 非 Linux 平台不执行 io_uring TLS OOM 契约。 */
int main(void)
{
	return 0;
}
#endif
