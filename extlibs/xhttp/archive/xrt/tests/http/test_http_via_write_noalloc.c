#include "../test_allocator.h"

#include <xrt/http_via.h>



/* Via 直接写出和长度查询不得分配。 */
int main(void)
{
	static const xhttpviavalue Via = {
		XRT_STR_INIT("HTTP"),
		XRT_STR_INIT("1.1"),
		XRT_STR_INIT("edge"),
		XRT_STR_INIT("443"),
		XRT_STR_INIT("west (blue)"),
		XHTTP_VIA_HAS_PROTOCOL_NAME |
		XHTTP_VIA_HAS_PORT |
		XHTTP_VIA_HAS_COMMENT
	};
	char sOutput[64];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"Via writer failure allocator install failed");
	testRequire(
		xrtHttpViaWrite(
			&Via, 1u, NULL, 0, &iSize
		) && xrtHttpViaWrite(
			&Via, 1u, sOutput, sizeof(sOutput), &iSize
		),
		"Via direct writer allocated"
	);
	printf("[PASS] http_via_write_noalloc\n");
	return 0;
}
