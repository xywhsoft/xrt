#include "../test_allocator.h"



/* 扩展解析、计数、参数迭代和直接写出不得依赖堆分配。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL(
		"permessage-deflate; mode=\"web\\socket\", x-test"
	);
	xwsextension Extension;
	xhttpparam Param;
	char Output[128];
	size_t iOffset = 0;
	size_t iParam = 0;
	size_t iCount;
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"WebSocket extension failure allocator install failed"
	);
	testRequire(
		xrtWsExtensionCount(Text, &iCount) &&
		(iCount == 2u),
		"WebSocket extension count allocated"
	);
	testRequire(
		xrtWsExtensionNext(
			Text,
			&iOffset,
			&Extension
		) == XHTTP_NEXT_ITEM,
		"WebSocket extension iteration allocated"
	);
	testRequire(
		xrtWsExtensionParamNext(
			&Extension,
			&iParam,
			&Param
		) == XHTTP_NEXT_ITEM,
		"WebSocket extension parameter iteration allocated"
	);
	testRequire(
		xrtWsExtensionWrite(
			Extension.Name,
			Extension.Parameters,
			Output,
			sizeof(Output),
			&iSize
		),
		"WebSocket extension write allocated"
	);
	printf("[PASS] websocket_extension_noalloc\n");
	return 0;
}
