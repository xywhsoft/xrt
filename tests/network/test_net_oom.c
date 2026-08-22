#include "../test_allocator.h"



/* 地址值操作必须零分配，拥有文本 Helper 必须原样暴露 OOM。 */
int main(void)
{
	xnetaddr Addr;
	char sText[64];

	testRequire(testInstallFailAllocator(),
		"failure allocator install failed");
	testRequire(xrtNetAddrParse(&Addr, "2001:db8::1", 443),
		"zero-allocation address parse failed under OOM");
	testRequire(xrtNetAddrEndpointText(&Addr, sText, sizeof(sText)) == 17,
		"zero-allocation endpoint format failed under OOM");
	testRequire(strcmp(sText, "[2001:db8::1]:443") == 0,
		"endpoint text mismatch under OOM");
	testRequire(xrtNetAddrString(&Addr) == NULL,
		"owned address text unexpectedly survived OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"owned address text OOM error mismatch");
	return 0;
}
