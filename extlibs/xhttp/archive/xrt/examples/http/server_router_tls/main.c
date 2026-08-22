#include <xrt/http_server_router_tls.h>



/* HTTPS 路由直接返回固定 JSON，不要求构建 Reply 对象。 */
static void exampleHttpRouterTlsRoute(
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
		XRT_BYTES_LITERAL("{\"secure\":true}")
	);
}



/* 从调用方的 TLS 身份配置启动冻结 Router。 */
static xhttpserver* exampleHttpRouterTlsStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls
)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);
	xhttpserver* pServer = NULL;

	if ( (pRouter != NULL) && xrtHttpServerGet(
		pRouter,
		XRT_STR_LITERAL("/health"),
		exampleHttpRouterTlsRoute,
		NULL
	) && xrtHttpServerRouterFreeze(pRouter) ) {
		pServer = xrtHttpServerRouterStartTls(
			pEngine,
			pConfig,
			pTls,
			pRouter,
			NULL
		);
	}
	xrtHttpServerRouterDestroy(pRouter);
	return pServer;
}



/* Engine、监听配置与 TLS 身份由宿主应用提供。 */
int main(void)
{
	(void)exampleHttpRouterTlsStart;
	return 0;
}
