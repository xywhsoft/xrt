#include "../test_allocator.h"

#include <xrt/http_origin.h>



/* Origin 直接写出和长度查询必须保持零堆分配。 */
int main(void)
{
	xhttporigin Origin;
	char sOutput[64];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"Origin writer failure allocator install failed");
	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("HTTPS://Example.test:443"), &Origin
		) && xrtHttpOriginWrite(
			&Origin, NULL, 0, &iSize
		) && xrtHttpOriginWrite(
			&Origin, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 20u),
		"Origin direct writer allocated"
	);
	printf("[PASS] http_origin_write_noalloc\n");
	return 0;
}
