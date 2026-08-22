#include "../test_allocator.h"

#include <xrt/http_vary.h>



/* 验证 Vary 计划、迭代、查找和写出不触发堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("Accept-Encoding, User-Agent")
		},
		{
			XRT_STR_INIT("vary"),
			XRT_STR_INIT("accept-encoding, *")
		}
	};
	xhttpvaryplan Plan;
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	char Output[64];
	size_t iSize = 0;
	bool bPass;

	testRequire(
		testInstallFailAllocator(),
		"HTTP Vary failure allocator install failed"
	);
	xrtHttpVaryCursorInit(&Cursor);
	bPass = xrtHttpVaryPlan(
		Fields, 2, &Plan
	) && (Plan.ItemCount == 4) &&
		((Plan.Flags & XHTTP_VARY_MIXED) != 0) &&
		(xrtHttpVaryNext(
			Fields, 2, &Cursor, &Item
		 ) == XHTTP_NEXT_ITEM) &&
		(xrtHttpVaryFind(
			Fields,
			2,
			XRT_STR_LITERAL("User-Agent"),
			&Item
		 ) == XHTTP_NEXT_ITEM) &&
		xrtHttpVaryWrite(
			Fields,
			2,
			Output,
			sizeof(Output),
			&iSize
		) &&
		(iSize != 0);
	testRequire(
		bPass,
		"HTTP Vary processing allocated memory"
	);
	printf("[PASS] http_vary_noalloc\n");
	return 0;
}
