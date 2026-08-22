#include "../internal/xrt_http_body.h"



#if defined(XRT_FEATURE_HTTP_BODY_ASYNC)

/* 在 AGAIN 后取得来源可读性 Future。 */
XRT_API xfuture* xrtHttpBodyReaderWait(xhttpbodyreader* pReader)
{
	xfuture* pFuture;
	xerror* pPrevious;
	xerror* pSourceError;

	if ( pReader == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pReader->Failed ) {
		xrtSetError(pReader->Error);
		return NULL;
	}
	if ( pReader->Done || !pReader->Again ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( pReader->Ops.Wait == NULL ) {
		(void)__xrtHttpBodyReaderFail(
			pReader,
			XERR_INTERNAL,
			XHTTP_BODY_ERROR_CONTRACT,
			"wait",
			"HTTP body source has no wait operation"
		);
		return NULL;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	pFuture = pReader->Ops.Wait(pReader->Context);
	pSourceError = __xrtErrorSwapOwned(pPrevious);
	if ( pFuture == NULL ) {
		__xrtHttpBodyReaderCaptureSource(
			pReader,
			pSourceError,
			XHTTP_BODY_ERROR_SOURCE,
			"wait",
			"HTTP body source failed to create a wait Future"
		);
		return NULL;
	}
	__xrtHttpBodySourceErrorCommit(pSourceError);
	pReader->Again = false;
	return pFuture;
}

#endif
