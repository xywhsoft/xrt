#include "../test_allocator.h"

#include <xrt/http_forward.h>



/* HTTP 转发基础解析、更新和直接写出不得分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("close, X-Hop")
		}
	};
	char sOutput[24];
	uint64 iNext;
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTTP forwarding failure allocator install failed");
	testRequire(
		(xrtHttpHopField(
			Fields, 1u, XRT_STR_LITERAL("X-Hop")
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpMaxForwardsUpdate(
			XRT_STR_LITERAL("8"), 4u, &iNext
		) == XHTTP_FORWARD_NEXT) && (iNext == 4u) &&
		xrtHttpMaxForwardsWrite(
			iNext, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 1u) && (sOutput[0] == '4'),
		"HTTP forwarding direct path allocated"
	);
	printf("[PASS] http_forward_noalloc\n");
	return 0;
}
