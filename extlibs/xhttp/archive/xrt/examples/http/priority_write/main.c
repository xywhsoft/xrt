#include <stdio.h>

#include <xrt/http_priority.h>



/* 为可增量处理的后台响应生成规范 Priority 字段值。 */
int main(void)
{
	xhttppriority Priority = {
		7u, 1u, XHTTP_PRIORITY_HAS_URGENCY |
		XHTTP_PRIORITY_HAS_INCREMENTAL
	};
	char arrValue[32];
	size_t iSize;

	if ( !xrtHttpPriorityWrite(
		&Priority, arrValue, sizeof(arrValue), &iSize
	) ) {
		return 1;
	}
	printf("Priority: %.*s\n", (int)iSize, arrValue);
	return 0;
}
