#include "../test_allocator.h"

#include <xrt/http_forwarded.h>



/* Forwarded 长度查询和直接写出必须保持零堆分配。 */
int main(void)
{
	static const xhttpforwardedvalue Element = {
		XRT_STR_INIT("[2001:db8::1]:443"),
		XRT_STR_INIT("_edge"),
		XRT_STR_INIT("example.com:8443"),
		XRT_STR_INIT("https"),
		NULL,
		0,
		XHTTP_FORWARDED_HAS_FOR |
		XHTTP_FORWARDED_HAS_BY |
		XHTTP_FORWARDED_HAS_HOST |
		XHTTP_FORWARDED_HAS_PROTO
	};
	char sOutput[160];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"Forwarded writer failure allocator install failed");
	testRequire(
		xrtHttpForwardedElementWrite(
			&Element, NULL, 0, &iSize
		) && xrtHttpForwardedElementWrite(
			&Element, sOutput, sizeof(sOutput), &iSize
		),
		"Forwarded writer allocated"
	);
	printf("[PASS] http_forwarded_write_noalloc\n");
	return 0;
}
