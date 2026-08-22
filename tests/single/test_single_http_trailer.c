#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留独立 Trailer 协议层。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Trailer"),
			XRT_STR_INIT("Digest, X-Meta")
		}
	};

	return xrtHttpTrailerFind(
		Fields, 1u, XRT_STR_LITERAL("digest")
	) == XHTTP_NEXT_ITEM ? 0 : 1;
}
