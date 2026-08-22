#include "../test_allocator.h"



/* 对象分配失败必须关闭已经创建的平台 Socket，并保留内存错误。 */
int main(void)
{
	size_t i;

	testRequire(testInstallFailAllocator(),
		"failure allocator install failed");
	for ( i = 0; i < 2048; i++ ) {
		testRequire(xrtNetSocketOpen(XNET_FAMILY_IPV4,
			XNET_SOCKET_STREAM, 0) == NULL,
			"socket object unexpectedly survived OOM");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"socket object OOM error mismatch");
	}
	return 0;
}
