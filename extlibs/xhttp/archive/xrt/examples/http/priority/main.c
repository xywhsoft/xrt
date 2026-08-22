#include <stdio.h>

#include <xrt/http_priority.h>



/* 读取客户端 Priority 建议并得到请求的有效默认值。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Priority"), XRT_STR_INIT("u=1, i") }
	};
	xhttppriority Priority;

	if ( !xrtHttpPriorityParse(Fields, 1, &Priority) ) {
		return 1;
	}
	printf(
		"urgency = %u, incremental = %s\n",
		(unsigned int)Priority.Urgency,
		Priority.Incremental != 0 ? "true" : "false"
	);
	return 0;
}
