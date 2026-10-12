#define XHTTP_POLICY_IMPLEMENTATION
#define XHTTP_CLIENT_IMPLEMENTATION
/* 使用完整单头实现验证 Linux io_uring 高层并发终态契约。 */
#if defined(__linux__)
	#define XHTTP_IMPLEMENTATION
	#include "../../include/xhttp-client/features.h"
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
	#ifndef XRT_IMPLEMENTATION
	#define XRT_IMPLEMENTATION
	#endif
	#include "../../../../single/xrt.h"
	#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-policy.h"
#include "../../../../single/extlibs/xhttp-client.h"




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
