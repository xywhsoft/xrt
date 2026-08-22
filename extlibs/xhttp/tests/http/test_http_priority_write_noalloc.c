#include "../test_allocator.h"

#include <xrt/http_priority.h>



/* Priority 规范写出和回读必须保持零堆分配。 */
int main(void)
{
	xhttppriority Input = {
		0, 1u, XHTTP_PRIORITY_HAS_URGENCY |
		XHTTP_PRIORITY_HAS_INCREMENTAL
	};
	xhttppriority Output;
	char arrValue[32];
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"Priority writer failure allocator install failed"
	);
	testRequire(
		xrtHttpPriorityWrite(
			&Input, arrValue, sizeof(arrValue), &iSize
		) && xrtHttpPriorityValueParse(
			(xstrview){ arrValue, iSize }, &Output
		) && (memcmp(&Input, &Output, sizeof(Input)) == 0),
		"Priority writer allocated"
	);
	printf("[PASS] http_priority_write_noalloc\n");
	return 0;
}
