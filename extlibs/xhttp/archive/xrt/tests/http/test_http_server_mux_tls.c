#include "../test.h"

#include <xrt/http_server_mux_tls.h>



/* 为共用 TLS Router 夹具组装按 Host 分流的 Mux Server。 */
static xhttpserver* testHttpServerMuxTlsStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents
)
{
	xhttpservermux* pMux = xrtHttpServerMuxCreate(NULL);
	xhttpserver* pServer = NULL;

	if ( pMux == NULL ) {
		return NULL;
	}
	if ( xrtHttpServerMuxHost(
		pMux,
		XRT_STR_LITERAL("example.com"),
		pRouter
	) ) {
		pServer = xrtHttpServerMuxStartTls(
			pEngine,
			pConfig,
			pTls,
			pMux,
			pEvents
		);
	}
	xrtHttpServerMuxDestroy(pMux);
	return pServer;
}



#define TEST_HTTP_SERVER_ROUTER_TLS_START \
	testHttpServerMuxTlsStart
#define TEST_HTTP_SERVER_ROUTER_TLS_PASS \
	"[PASS] HTTPS server mux"
#include "test_http_server_router_tls.c"
