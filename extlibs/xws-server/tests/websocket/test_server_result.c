#include "../test.h"



/* 验证非空但回绕的结果地址在任何公开入口都不会被解引用。 */
static void testWsOpenResultWrapping(void)
{
	xwsserverresult* pWrapping = (xwsserverresult*)(uintptr_t)(
		UINTPTR_MAX - 1u
	);

	xrtClearError();
	testRequire(
		(xrtWsServerResultRef(pWrapping) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_SERVER_RESULT_ERROR_ARGUMENT) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"xrt.websocket.open-result"
		 ) == 0),
		"WebSocket open result wrapping retain mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsServerResultConnection(pWrapping) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket open result wrapping query mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsServerResultTakeConnection(pWrapping) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket open result wrapping take mismatch"
	);
	xrtClearError();
	xrtWsServerResultDestroy(pWrapping);
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
	xrtWsServerResultDestroy(NULL);
	xrtClearError();
	testRequire(
		(xrtWsServerResultRef(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket open result null retain mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsServerResultConnection(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket open result null query mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsServerResultTakeConnection(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket open result null take mismatch"
	);
	xrtClearError();
	printf("[PASS] minimal WebSocket open result contract\n");
	return 0;
}
