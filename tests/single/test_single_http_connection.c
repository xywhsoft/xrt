#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留独立 Connection 协议层。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("keep-alive")
		},
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("TE")
		}
	};
	size_t iCount;

	return (xrtHttpConnectionCount(
		Fields, 2u, &iCount
	) && (iCount == 2u) &&
		(xrtHttpConnectionFind(
			Fields, 2u, XRT_STR_LITERAL("te")
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_1, Fields, 2u, 0
		) == XHTTP_CONNECTION_PERSIST)) ? 0 : 1;
}
