/* 使用同一组文件发送契约验证 Linux io_uring splice 路径。 */
#if defined(__linux__)
	#define TEST_TCP_FILE_BACKEND XNET_PORT_URING
	#include "test_net_tcp_file.c"
#else
	#include "../test.h"



/* 非 Linux 平台不执行 io_uring 文件发送契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring file send placeholder");
	return 0;
}
#endif
