#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 Priority 重复字段解析。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Priority"), XRT_STR_INIT("u=7") },
		{ XRT_STR_INIT("priority"), XRT_STR_INIT("u=1, i") }
	};
	xhttppriority Priority;

	return xrtHttpPriorityParse(Fields, 2, &Priority) &&
		(Priority.Urgency == 1u) &&
		(Priority.Incremental == 1u) ? 0 : 1;
}
