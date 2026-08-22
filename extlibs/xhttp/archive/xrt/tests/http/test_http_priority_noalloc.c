#include "../test_allocator.h"

#include <xrt/http_priority.h>



/* Priority 解析、重复字段处理和覆盖必须保持零堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Priority"), XRT_STR_INIT("u=5") },
		{ XRT_STR_INIT("priority"), XRT_STR_INIT("i") }
	};
	xhttppriority Base;
	xhttppriority Update;
	xhttppriority Result;

	testRequire(
		testInstallFailAllocator(),
		"Priority failure allocator install failed"
	);
	testRequire(
		xrtHttpPriorityParse(Fields, 2, &Base) &&
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=1"), &Update
		) && xrtHttpPriorityOverlay(
			&Base, &Update, &Result
		) && (Result.Urgency == 1u) &&
		(Result.Incremental == 1u),
		"Priority parsing allocated"
	);
	printf("[PASS] http_priority_noalloc\n");
	return 0;
}
