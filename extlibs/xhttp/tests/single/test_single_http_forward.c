#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 HTTP 转发基础语义。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("X-Hop")
		}
	};
	uint64 iNext;

	return (xrtHttpHopField(
		Fields, 1u, XRT_STR_LITERAL("X-Hop")
	) == XHTTP_NEXT_ITEM) &&
		(xrtHttpMaxForwardsUpdate(
			XRT_STR_LITERAL("2"), 8u, &iNext
		) == XHTTP_FORWARD_NEXT) &&
		(iNext == 1u) ? 0 : 1;
}
