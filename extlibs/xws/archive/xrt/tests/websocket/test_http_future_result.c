#include "../test.h"



/* 验证非空但回绕的结果地址在任何公开入口都不会被解引用。 */
static void testWsOpenResultWrapping(void)
{
	xwsopenresult* pWrapping = (xwsopenresult*)(uintptr_t)(
		UINTPTR_MAX - 1u
	);

	xrtClearError();
	testRequire(
		(xrtWsOpenResultRef(pWrapping) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_OPEN_RESULT_ERROR_ARGUMENT) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"xrt.websocket.open-result"
		 ) == 0),
		"WebSocket open result wrapping retain mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsOpenResultConnection(pWrapping) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket open result wrapping query mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsOpenResultTakeConnection(pWrapping) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket open result wrapping take mismatch"
	);
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_FUTURE)
		xrtClearError();
		testRequire(
			(xrtWsOpenResultResponse(pWrapping) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"WebSocket open result wrapping response mismatch"
		);
		xrtClearError();
		testRequire(
			(xrtWsOpenResultTakeResponse(pWrapping) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"WebSocket open result wrapping response take mismatch"
		);
	#endif
	xrtClearError();
	xrtWsOpenResultDestroy(pWrapping);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"WebSocket open result wrapping destroy mismatch"
	);
	xrtClearError();
}



/* 验证公共连接结果不依赖客户端 Response、HTTP 执行或 Future 运行时。 */
int main(void)
{
	testWsOpenResultWrapping();
	xrtWsOpenResultDestroy(NULL);
	xrtClearError();
	testRequire(
		(xrtWsOpenResultRef(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket open result null retain mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsOpenResultConnection(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket open result null query mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsOpenResultTakeConnection(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket open result null take mismatch"
	);
	xrtClearError();
	printf("[PASS] minimal WebSocket open result contract\n");
	return 0;
}
