/* 验证单头文件在 Linux io_uring 上停止提前拒绝的请求正文。 */
#if defined(__linux__)
	#define XHTTP_IMPLEMENTATION
	#include "../../single/xhttp.h"

	#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_URING
	#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "single io_uring"
	#include "../http/test_http_client_stream_early_final.c"
#else



/* 非 Linux 平台不执行单头 io_uring 提前最终响应契约。 */
int main(void)
{
	return 0;
}
#endif
