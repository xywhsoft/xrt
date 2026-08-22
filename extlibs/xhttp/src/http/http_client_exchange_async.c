#include "../internal/xrt_http_exchange.h"



#if defined(XHTTP_FEATURE_HTTP_EXCHANGE_ASYNC)

/* 把正文来源的可读性 Future 安全提升到 Exchange 层。 */
XRT_API xfuture* xrtHttp1ExchangeOutputWait(
	xhttp1exchange* pExchange
)
{
	const xerror* pCause;
	xfuture* pFuture;
	xerrkind Kind;

	if ( pExchange == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
		return NULL;
	}
	if ( pExchange->OutputStopped ||
		!pExchange->OutputAgain ||
		(pExchange->Reader == NULL) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pFuture = xrtHttpBodyReaderWait(pExchange->Reader);
	if ( pFuture != NULL ) {
		pExchange->OutputAgain = false;
		return pFuture;
	}
	pCause = xrtHttpBodyReaderError(pExchange->Reader);
	Kind = pCause != NULL ?
		xrtErrorKind(pCause) : XERR_IO;
	(void)__xrtHttp1ExchangeFailCause(
		pExchange,
		XHTTP1_EXCHANGE_ERROR_REQUEST_BODY,
		Kind,
		"wait-http-request-body",
		"HTTP request body readiness wait failed",
		pCause
	);
	return NULL;
}

#endif

