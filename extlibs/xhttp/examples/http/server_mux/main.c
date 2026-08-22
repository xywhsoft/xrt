#include <stdio.h>
#include <string.h>
#include <xrt/http_server_mux.h>



/* Mux 示例只保存处理器地址，不执行网络回调。 */
static void exampleHttpServerMuxRoute(
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



/* 创建一个包含单条 GET 路由的冻结站点 Router。 */
static xhttpserverrouter* exampleHttpServerMuxRouter(cstr sPath)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);

	if ( (pRouter == NULL) ||
		!xrtHttpServerGet(
			pRouter,
			(xstrview){ sPath, strlen(sPath) },
			exampleHttpServerMuxRoute,
			NULL
		) || !xrtHttpServerRouterFreeze(pRouter) ) {
		xrtHttpServerRouterDestroy(pRouter);
		return NULL;
	}
	return pRouter;
}



/* 设置默认站点、精确 Host，并用一次调用热替换 Host Router。 */
int main(void)
{
	xhttpserverrouter* pDefault =
		exampleHttpServerMuxRouter("/");
	xhttpserverrouter* pApi =
		exampleHttpServerMuxRouter("/v1");
	xhttpserverrouter* pNext =
		exampleHttpServerMuxRouter("/v2");
	xhttpserverrouter* pMatched = NULL;
	xhttpservermux* pMux = xrtHttpServerMuxCreate(NULL);
	xhttpservermuxstatus Status;
	int iResult = 1;

	if ( (pDefault == NULL) || (pApi == NULL) ||
		(pNext == NULL) || (pMux == NULL) ||
		!xrtHttpServerMuxDefault(pMux, pDefault) ||
		!xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("api.example.test"),
			pApi
		) || !xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("API.EXAMPLE.TEST"),
			pNext
		) ) {
		goto Cleanup;
	}
	Status = xrtHttpServerMuxMatch(
		pMux,
		XRT_STR_LITERAL("api.example.test"),
		&pMatched
	);
	if ( (Status != XHTTP_SERVER_MUX_HOST) ||
		(pMatched != pNext) ) {
		goto Cleanup;
	}
	printf(
		"host: %u, routes: %u\n",
		(unsigned)Status,
		(unsigned)xrtHttpServerRouterCount(pMatched)
	);
	iResult = 0;

Cleanup:
	xrtHttpServerRouterDestroy(pMatched);
	xrtHttpServerMuxDestroy(pMux);
	xrtHttpServerRouterDestroy(pDefault);
	xrtHttpServerRouterDestroy(pApi);
	xrtHttpServerRouterDestroy(pNext);
	return iResult;
}
