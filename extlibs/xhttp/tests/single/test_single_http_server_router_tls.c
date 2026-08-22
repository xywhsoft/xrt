#define XHTTP_MODULE_HTTP_SERVER_ROUTER_TLS
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头文件测试断言失败时结束独立进程。 */
static void testSingleHttpServerRouterTlsRequire(
	bool bCondition,
	cstr sMessage
)
{
	if ( !bCondition ) {
		fprintf(stderr, "[FAIL] %s\n", sMessage);
		exit(1);
	}
}



/* 单头文件入口验证 TLS Router 的裁剪闭包和失败清理。 */
static void testSingleHttpServerRouterTlsHandler(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
}



/* 冻结 Router 后，空 Engine 必须通过 TLS 入口稳定失败。 */
int main(void)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);

	testSingleHttpServerRouterTlsRequire(
		(pRouter != NULL) &&
		xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL("/secure"),
			testSingleHttpServerRouterTlsHandler,
			NULL
		) && xrtHttpServerRouterFreeze(pRouter),
		"single HTTPS server router fixture failed"
	);
	testSingleHttpServerRouterTlsRequire(
		(xrtHttpServerRouterStartTls(
			NULL, NULL, NULL, pRouter, NULL
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XHTTP_SERVER_ROUTER_ERROR_START) &&
		xrtHttpServerRouterFrozen(pRouter),
		"single HTTPS server router start contract mismatch"
	);
	xrtClearError();
	xrtHttpServerRouterDestroy(pRouter);
	puts("[PASS] single HTTPS server router");
	return 0;
}
