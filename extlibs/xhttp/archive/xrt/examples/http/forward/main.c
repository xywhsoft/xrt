#include <stdio.h>

#include <xrt/http_forward.h>



/* 过滤转发字段并更新 TRACE 或 OPTIONS 的 Max-Forwards。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("close, X-Local")
		},
		{
			XRT_STR_INIT("X-Local"),
			XRT_STR_INIT("private")
		}
	};
	uint64 iNext;

	if ( xrtHttpHopField(
		Fields, 2u, Fields[1].Name
	) == XHTTP_NEXT_ITEM ) {
		printf("X-Local is removed before forwarding\n");
	}
	if ( xrtHttpMaxForwardsUpdate(
		XRT_STR_LITERAL("5"), 16u, &iNext
	) == XHTTP_FORWARD_NEXT ) {
		printf("next Max-Forwards = %llu\n",
			(unsigned long long)iNext);
	}
	return 0;
}
