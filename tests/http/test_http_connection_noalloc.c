#include "../test_allocator.h"

#include <xrt/http_connection.h>



/* Connection 的迭代、校验与查询路径不得分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("keep-alive, TE")
		},
		{
			XRT_STR_INIT("connection"),
			XRT_STR_INIT("Upgrade")
		}
	};
	xhttpfieldtokencursor Cursor;
	xstrview Option;
	size_t iCount = 0;
	size_t iMeasured;

	testRequire(testInstallFailAllocator(),
		"HTTP Connection failure allocator install failed");
	xrtHttpConnectionCursorInit(&Cursor);
	while ( xrtHttpConnectionNext(
		Fields, 2u, &Cursor, &Option
	) == XHTTP_NEXT_ITEM ) {
		iCount++;
	}
	testRequire(
		(iCount == 3u) &&
		xrtHttpConnectionCount(
			Fields, 2u, &iMeasured
		) && (iMeasured == 3u) &&
		(xrtHttpConnectionFind(
			Fields, 2u, XRT_STR_LITERAL("te")
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_1, Fields, 2u, 0
		) == XHTTP_CONNECTION_PERSIST),
		"HTTP Connection parser allocated or lost an option"
	);
	printf("[PASS] http_connection_noalloc\n");
	return 0;
}
