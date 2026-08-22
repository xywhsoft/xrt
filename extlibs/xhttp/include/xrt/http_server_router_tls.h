#ifndef XRT_HTTP_SERVER_ROUTER_TLS_H
#define XRT_HTTP_SERVER_ROUTER_TLS_H

#include <xrt/http_server_router.h>
#include <xrt/http_server_tls.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_TLS))
	#error "XRT HTTP server router TLS requires server router and TLS support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS)

/* 启动由冻结 Router 分发的 TLS HTTP/1 Server。 */
XRT_API xhttpserver* xrtHttpServerRouterStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents
);

#endif



XRT_EXTERN_C_END

#endif
