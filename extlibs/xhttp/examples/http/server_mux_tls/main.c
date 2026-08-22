#include <xrt/http_server_mux_tls.h>



/* Mux 示例路由保留完整回调签名，方便直接替换业务处理。 */
static void exampleHttpMuxTlsRoute(
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
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("secure host")
	);
}



/* 按 Host 把 HTTPS 请求分配给独立冻结 Router。 */
static xhttpserver* exampleHttpMuxTlsStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls
)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);
	xhttpservermux* pMux = xrtHttpServerMuxCreate(NULL);
	xhttpserver* pServer = NULL;

	if ( (pRouter != NULL) && (pMux != NULL) &&
		xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL("/"),
			exampleHttpMuxTlsRoute,
			NULL
		) && xrtHttpServerRouterFreeze(pRouter) &&
		xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("api.example.test"),
			pRouter
		) ) {
		pServer = xrtHttpServerMuxStartTls(
			pEngine,
			pConfig,
			pTls,
			pMux,
			NULL
		);
	}
	xrtHttpServerMuxDestroy(pMux);
	xrtHttpServerRouterDestroy(pRouter);
	return pServer;
}



/* Engine、监听配置与 TLS 身份由宿主应用提供。 */
int main(void)
{
	(void)exampleHttpMuxTlsStart;
	return 0;
}
