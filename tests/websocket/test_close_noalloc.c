#include "../test_allocator.h"



/* Close 状态码、解析和写出不得依赖堆分配。 */
int main(void)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	xwsclose Close;
	xbytesview Input;
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"WebSocket Close failure allocator install failed"
	);
	testRequire(
		xrtWsCloseCodeValid(XWS_CLOSE_NORMAL),
		"WebSocket Close code predicate allocated"
	);
	testRequire(
		xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			XRT_STR_LITERAL("no allocation"),
			Payload,
			sizeof(Payload),
			&iSize
		),
		"WebSocket Close writer allocated"
	);
	Input.Data = Payload;
	Input.Size = iSize;
	testRequire(
		xrtWsCloseParse(Input, &Close) &&
		(Close.Code == XWS_CLOSE_NORMAL),
		"WebSocket Close parser allocated"
	);
	printf("[PASS] websocket_close_noalloc\n");
	return 0;
}
