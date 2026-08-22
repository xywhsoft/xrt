#include "../test_allocator.h"



/* 参数迭代、查找、解码和直接写出不得依赖堆分配。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL("a=1; b=\"x;y\"");
	xhttpparam Param;
	xhttpparam TokenParam;
	xhttpparamvaluecursor Cursor;
	xhttpnext Next;
	uint8 iByte;
	char Output[32];
	size_t iOffset = 0;
	size_t iSize;

	memset(&TokenParam, 0, sizeof(TokenParam));
	TokenParam.Value = XRT_STR_LITERAL("websocket");
	TokenParam.Flags = XHTTP_PARAM_HAS_VALUE;
	testRequire(testInstallFailAllocator(),
		"HTTP parameter failure allocator install failed");
	testRequire(xrtHttpParamNext(
		Text, &iOffset, &Param
	) == XHTTP_NEXT_ITEM, "HTTP parameter iteration allocated");
	testRequire(xrtHttpParamFind(
		Text, XRT_STR_LITERAL("b"), &Param
	) == XHTTP_NEXT_ITEM, "HTTP parameter lookup allocated");
	testRequire(xrtHttpParamValueWrite(
		&Param, Output, sizeof(Output), &iSize
	) && (iSize == 3), "HTTP parameter decode allocated");
	xrtHttpParamValueCursorInit(&Cursor);
	Next = xrtHttpParamValueNext(&Param, &Cursor, &iByte);
	testRequire((Next == XHTTP_NEXT_ITEM) && (iByte == (uint8)'x'),
		"HTTP parameter semantic cursor allocated");
	testRequire(xrtHttpParamTokenValid(&TokenParam),
		"HTTP parameter token validation allocated");
	testRequire(xrtHttpDirectiveFind(
		XRT_STR_LITERAL("max-age=60, no-transform"),
		XRT_STR_LITERAL("no-transform"), &Param
	) == XHTTP_NEXT_ITEM, "HTTP directive lookup allocated");
	testRequire(xrtHttpParamWrite(
		XRT_STR_LITERAL("name"), XRT_STR_LITERAL("value"),
		XHTTP_PARAM_HAS_VALUE, Output, sizeof(Output), &iSize
	), "HTTP parameter write allocated");
	printf("[PASS] http_param_noalloc\n");
	return 0;
}
