#include <xhttp.h>

#include <stdio.h>
#include <string.h>



/* 展示文件和网络之外的静态响应协议计划。 */
int main(void)
{
	xhttpfield Fields[] = {
		{
			XRT_STR_INIT("If-Range"),
			XRT_STR_INIT("\"asset-9\"")
		},
		{
			XRT_STR_INIT("Range"),
			XRT_STR_INIT("bytes=100-199, 180-249")
		}
	};
	xhttprepresentation Current;
	xhttpstaticplanconfig Config;
	xhttpbyterange Ranges[16];
	xhttpstaticplan Plan;

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	Current.HasETag = true;
	Current.ETag.Opaque = XRT_STR_LITERAL("asset-9");
	xrtHttpStaticPlanConfigInit(&Config);
	if ( !xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		Fields,
		2,
		&Current,
		UINT64_C(1000),
		Ranges,
		16,
		&Config,
		&Plan
	) ) {
		return 1;
	}
	printf(
		"status=%u ranges=%zu selected=%llu\n",
		(unsigned int)Plan.Status,
		Plan.RangeCount,
		(unsigned long long)Plan.SelectedLength
	);
	return 0;
}
