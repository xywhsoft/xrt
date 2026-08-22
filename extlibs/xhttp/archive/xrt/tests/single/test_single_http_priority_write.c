#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 Priority 规范序列化。 */
int main(void)
{
	xhttppriority Priority = {
		1u, 1u, XHTTP_PRIORITY_HAS_URGENCY |
		XHTTP_PRIORITY_HAS_INCREMENTAL
	};
	char arrValue[16];
	size_t iSize;

	return xrtHttpPriorityWrite(
		&Priority, arrValue, sizeof(arrValue), &iSize
	) && (iSize == 6u) &&
		(memcmp(arrValue, "u=1, i", 6u) == 0) ? 0 : 1;
}
