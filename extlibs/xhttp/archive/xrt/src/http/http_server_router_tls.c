#include "../internal/xrt_http_server_router.h"

#include <xrt/http_server_router_tls.h>



#if defined(XRT_FEATURE_HTTP_SERVER_ROUTER_TLS)

/* 启动 TLS Router Server；失败时同步归还适配器和 Router 引用。 */
XRT_API xhttpserver* xrtHttpServerRouterStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents
)
{
	xhttpserverevents Events;
	xrt_http_server_router_runtime* pRuntime =
		__xrtHttpServerRouterRuntimeCreate(
			pRouter, pEvents, &Events
		);
	xhttpserver* pServer;

	if ( pRuntime == NULL ) {
		return NULL;
	}
	pServer = xrtHttpServerStartTls(
		pEngine, pConfig, pTls, &Events
	);
	if ( pServer == NULL ) {
		__xrtHttpServerRouterRuntimeDestroy(pRuntime);
		__xrtHttpServerRouterWrapError(
			XERR_IO,
			XHTTP_SERVER_ROUTER_ERROR_START,
			"start-https-server-router",
			"HTTPS server router listener start failed"
		);
	}
	return pServer;
}

#endif
