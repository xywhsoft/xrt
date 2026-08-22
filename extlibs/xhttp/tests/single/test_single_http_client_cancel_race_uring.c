/* 使用完整单头实现验证 Linux io_uring 高层并发终态契约。 */
#if defined(__linux__)
	#define XHTTP_IMPLEMENTATION
	#include "../../single/xhttp.h"



	#define TEST_HTTP_CLIENT_RACE_BACKEND XNET_PORT_URING
	#define TEST_HTTP_CLIENT_RACE_BACKEND_NAME "single io_uring"
	#include "../http/test_http_client_cancel_race.c"
#else



/* 非 Linux 平台不执行单头 io_uring 高层并发终态契约。 */
int main(void)
{
	return 0;
}
#endif
