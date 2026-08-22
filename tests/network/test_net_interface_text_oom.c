#include "../test_allocator.h"



/* 所有分配型本机信息便捷函数必须直接传播 OOM。 */
int main(void)
{
	testRequire(testInstallFailAllocator(),
		"failure allocator install failed");
	testRequire(xrtNetLocalAddressString(
		XNET_FAMILY_UNSPEC
	) == NULL, "local address string unexpectedly survived OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"local address string OOM error mismatch");
	testRequire(xrtNetHostNameString() == NULL,
		"host name string unexpectedly survived OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"host name string OOM error mismatch");
	testRequire(xrtNetLocalHardwareString() == NULL,
		"hardware string unexpectedly survived OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"hardware string OOM error mismatch");
	return 0;
}
