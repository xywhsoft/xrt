#include "../internal/xrt_http_server.h"
#include <xrt/http_server_response_async.h>



#if defined(XRT_FEATURE_HTTP_SERVER_RESPONSE_ASYNC)

/* 把正文来源的可读性 Future 安全提升到 Server Response 层。 */
XRT_API xfuture* xrtHttp1ServerResponseWait(
	xhttp1serverresponse* pResponse
)
{
	const xerror* pCause;
	xfuture* pFuture;
	xerrkind Kind;

	if ( pResponse == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pResponse->Error != NULL ) {
		xrtSetError(pResponse->Error);
		return NULL;
	}
	if ( !pResponse->OutputAgain ||
		(pResponse->Reader == NULL) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pFuture = xrtHttpBodyReaderWait(
		pResponse->Reader
	);
	if ( pFuture != NULL ) {
		pResponse->OutputAgain = false;
		return pFuture;
	}
	pCause = xrtHttpBodyReaderError(
		pResponse->Reader
	);
	Kind = pCause != NULL ?
		xrtErrorKind(pCause) : XERR_IO;
	(void)__xrtHttp1ServerResponseFailCause(
		pResponse,
		XHTTP1_SERVER_RESPONSE_ERROR_BODY,
		Kind,
		"wait-http1-server-body",
		"HTTP server response body readiness wait failed",
		pCause
	);
	return NULL;
}

#endif
