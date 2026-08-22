#ifndef XRT_HTTP_SERVER_MUX_TLS_H
#define XRT_HTTP_SERVER_MUX_TLS_H

#include <xrt/http_server_mux.h>
#include <xrt/http_server_tls.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER_MUX) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_TLS))
	#error "XRT HTTP server mux TLS requires server mux and TLS support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS)

/* 启动按有效请求 Host 选择 Router 的 TLS HTTP/1 Server。 */
XRT_API xhttpserver* xrtHttpServerMuxStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents
);

#endif



XRT_EXTERN_C_END

#endif
