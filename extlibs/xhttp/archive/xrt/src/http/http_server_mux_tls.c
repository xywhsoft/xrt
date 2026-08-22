#include "../internal/xrt_http_server_mux.h"

#include <xrt/http_server_mux_tls.h>



#if defined(XRT_FEATURE_HTTP_SERVER_MUX_TLS)

/* 启动 TLS Mux Server；SNI 身份选择仍由独立 TLS Server 配置负责。 */
XRT_API xhttpserver* xrtHttpServerMuxStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents
)
{
	xhttpserverevents Events;
	xrt_http_server_mux_runtime* pRuntime =
		__xrtHttpServerMuxRuntimeCreate(
			pMux, pEvents, &Events
		);
	xhttpserver* pServer;

	if ( pRuntime == NULL ) {
		return NULL;
	}
	pServer = xrtHttpServerStartTls(
		pEngine, pConfig, pTls, &Events
	);
	if ( pServer == NULL ) {
		__xrtHttpServerMuxRuntimeDestroy(pRuntime);
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_START,
			"start-https-server-mux",
			"HTTPS server mux listener start failed"
		);
	}
	return pServer;
}

#endif
