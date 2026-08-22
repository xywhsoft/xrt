#include <stdio.h>

#include <xrt/http_connection.h>



/* 演示跨重复字段查询选项并判断 HTTP/1.1 持久性。 */
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
	xhttpconnectionstatus Status;
	size_t iCount;

	if ( !xrtHttpConnectionCount(
		Fields, 2u, &iCount
	) || (iCount != 2u) ||
		xrtHttpConnectionFind(
		Fields, 2u, XRT_STR_LITERAL("te")
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	Status = xrtHttpConnectionPersistence(
		XHTTP_VERSION_1_1, Fields, 2u, 0
	);
	if ( Status != XHTTP_CONNECTION_PERSIST ) {
		return 1;
	}
	printf("options=%zu persistent=yes\n", iCount);
	return 0;
}
