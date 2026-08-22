#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留条件请求与范围响应的完整静态计划。 */
int main(void)
{
	xhttpfield Field = {
		XRT_STR_INIT("Range"),
		XRT_STR_INIT("bytes=10-19")
	};
	xhttprepresentation Current;
	xhttpstaticplanconfig Config;
	xhttpbyterange Ranges[16];
	xhttpstaticplan Plan;

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	xrtHttpStaticPlanConfigInit(&Config);
	if ( !xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		&Field,
		1,
		&Current,
		100,
		Ranges,
		16,
		&Config,
		&Plan
	) || (Plan.Status != XHTTP_STATUS_PARTIAL_CONTENT) ||
		(Plan.RangeCount != 1) ||
		(Ranges[0].First != 10) ||
		(Ranges[0].Last != 19) ) {
		return 1;
	}
	return 0;
}
