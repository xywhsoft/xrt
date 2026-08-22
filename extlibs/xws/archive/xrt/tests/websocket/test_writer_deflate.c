#include "../test.h"



/* 验证压缩 Writer 独立裁剪层的公开边界和保守输出预算。 */
int main(void)
{
	size_t iEmpty = 0;
	size_t iPayload = 0;

	testRequire(
		xrtWsDeflaterBound(0, &iEmpty) &&
		xrtWsDeflaterBound(65536u, &iPayload) &&
		(iEmpty >= 5u) &&
		(iPayload > 65536u),
		"WebSocket compressed Writer bound mismatch"
	);
	testRequire(
		!xrtWsDeflaterBound(SIZE_MAX, &iPayload) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"WebSocket compressed Writer bound overflow mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnBeginCompressed(
			NULL,
			XWS_OPCODE_TEXT
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket compressed Writer null Connection mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnBeginCompressed(
			(xwsconn*)(uintptr_t)(UINTPTR_MAX - 1u),
			XWS_OPCODE_BINARY
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket compressed Writer wrapping Connection mismatch"
	);
	xrtClearError();
	printf("[PASS] WebSocket compressed Writer contract\n");
	return 0;
}
