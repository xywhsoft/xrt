/* 使用同一组托管连接 Future OOM 契约验证 Linux io_uring 后端。 */
#if defined(__linux__)
	#define TEST_TCP_DIAL_BACKEND XNET_PORT_URING
	#include "test_net_tcp_dial_future_oom.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring TCP Dial Future OOM 契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring TCP Dial Future OOM placeholder");
	return 0;
}
#endif
