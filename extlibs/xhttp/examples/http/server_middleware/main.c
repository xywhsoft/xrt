#include <stdio.h>
#include <xrt/http_server_middleware.h>



/* 请求日志层在 Next 前后包裹路由，短路时也可以直接提交响应。 */
static bool exampleHttpMiddlewareLog(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xhttpservernext* pNext,
	ptr pData
)
{
	xstrview Method = xrtHttpServerRequestMethod(pRequest);
	xstrview Target = xrtHttpServerRequestTarget(pRequest);
	bool bResult;

	(void)pServer;
	(void)pConnection;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	printf(
		"> %.*s %.*s\n",
		(int)Method.Size,
		Method.Data,
		(int)Target.Size,
		Target.Data
	);
	bResult = xrtHttpServerNext(pNext);
	printf("< state=%d\n", (int)xrtHttpConnState(pConnection));
	return bResult;
}



/* 常用业务路由直接提交固定 JSON，不要求构造响应对象。 */
static void exampleHttpMiddlewareRoute(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("application/json; charset=utf-8"),
		XRT_BYTES_LITERAL("{\"code\":200,\"msg\":\"OK\"}")
	);
}



/* 展示中间件先注册、路由后注册、最后一次冻结的完整构造顺序。 */
int main(void)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);

	if ( (pRouter == NULL) ||
		!xrtHttpServerUse(
			pRouter,
			exampleHttpMiddlewareLog,
			NULL
		) || !xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL("/health"),
			exampleHttpMiddlewareRoute,
			NULL
		) || !xrtHttpServerRouterFreeze(pRouter) ) {
		xrtHttpServerRouterDestroy(pRouter);
		return 1;
	}
	printf(
		"middleware: %u, routes: %u\n",
		(unsigned)xrtHttpServerMiddlewareCount(pRouter),
		(unsigned)xrtHttpServerRouterCount(pRouter)
	);
	xrtHttpServerRouterDestroy(pRouter);
	return 0;
}
