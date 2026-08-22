#include "../internal/xrt_http_client.h"

#include <xrt/memory.h>



#if defined(XRT_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION)

/* 回调上下文只在一次同步准备调用内存活。 */
typedef struct xrt_http_digest_prepare {
	xhttpdigestsession* Session;
	xstrview EntityHash;
	xstrview Name;
	xhttpdigestexchange* Exchange;
	str Value;
	size_t ValueSize;
} xrt_http_digest_prepare;



/* 使用准备器已经确定的线路 target 生成 Exchange 和认证字段。 */
static bool __xrtHttp1RequestDigestField(
	xstrview Method,
	xstrview Target,
	ptr pData,
	xhttpfield* pField
)
{
	xrt_http_digest_prepare* pPrepare =
		(xrt_http_digest_prepare*)pData;
	const xhttpdigestauth* pAuth;

	pPrepare->Exchange = xrtHttpDigestSessionAuthorize(
		pPrepare->Session,
		Method,
		Target,
		pPrepare->EntityHash
	);
	if ( pPrepare->Exchange == NULL ) {
		return false;
	}
	pAuth = xrtHttpDigestExchangeAuth(pPrepare->Exchange);
	if ( (pAuth == NULL) || !xrtHttpDigestAuthWrite(
		pAuth, NULL, 0, &pPrepare->ValueSize
	) ) {
		return false;
	}
	pPrepare->Value = (str)xrtMalloc(pPrepare->ValueSize);
	if ( pPrepare->Value == NULL ) {
		return false;
	}
	if ( !xrtHttpDigestAuthWrite(
		pAuth,
		pPrepare->Value,
		pPrepare->ValueSize,
		&pPrepare->ValueSize
	) ) {
		return false;
	}
	*pField = (xhttpfield){
		pPrepare->Name,
		{ pPrepare->Value, pPrepare->ValueSize }
	};
	return true;
}



/* 清除临时线路凭据，并按准备结果决定是否保留 Exchange。 */
static void __xrtHttp1RequestDigestPrepareUnit(
	xrt_http_digest_prepare* pPrepare,
	bool bKeepExchange
)
{
	if ( pPrepare->Value != NULL ) {
		xrtSecureZero(
			pPrepare->Value,
			pPrepare->ValueSize
		);
		xrtFree(pPrepare->Value);
	}
	if ( !bKeepExchange ) {
		xrtHttpDigestExchangeRelease(pPrepare->Exchange);
	}
	pPrepare->Value = NULL;
	pPrepare->Exchange = NULL;
}



/* 校验输出位置，并执行源站或代理 Digest 请求准备。 */
static xhttp1requestplan* __xrtHttp1RequestPrepareDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xstrview Name,
	xhttpdigestexchange** ppExchange
)
{
	xrt_http_digest_prepare Prepare;
	xhttpdigestexchange* pEmpty = NULL;
	xhttp1requestplan* pPlan;

	if ( !__xrtHttpRequestOutputValid(
		pRequest, ppExchange, sizeof(*ppExchange)
	) || !__xrtHttpDigestSessionOutputValid(
		pSession, NULL, ppExchange, sizeof(*ppExchange)
	) ||
		((pOptions != NULL) && !__xrtRangeValid(
			pOptions, sizeof(*pOptions)
		)) ||
		!__xrtHttpViewValid(EntityHash) ||
		((pOptions != NULL) && __xrtRangesOverlap(
			ppExchange, sizeof(*ppExchange),
			pOptions, sizeof(*pOptions)
		)) || __xrtRangesOverlap(
			ppExchange, sizeof(*ppExchange),
			EntityHash.Data, EntityHash.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memcpy(ppExchange, &pEmpty, sizeof(pEmpty));
	memset(&Prepare, 0, sizeof(Prepare));
	Prepare.Session = pSession;
	Prepare.EntityHash = EntityHash;
	Prepare.Name = Name;
	pPlan = __xrtHttp1RequestPrepareField(
		pRequest,
		pOptions,
		__xrtHttp1RequestDigestField,
		&Prepare
	);
	if ( pPlan == NULL ) {
		__xrtHttp1RequestDigestPrepareUnit(&Prepare, false);
		return NULL;
	}
	memcpy(ppExchange, &Prepare.Exchange, sizeof(Prepare.Exchange));
	__xrtHttp1RequestDigestPrepareUnit(&Prepare, true);
	return pPlan;
}



/* 使用最终线路 target 准备源站 Digest 请求。 */
XRT_API xhttp1requestplan* xrtHttp1RequestPrepareDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
)
{
	return __xrtHttp1RequestPrepareDigest(
		pRequest,
		pOptions,
		pSession,
		EntityHash,
		XRT_STR_LITERAL("Authorization"),
		ppExchange
	);
}



/* 使用最终线路 target 准备代理 Digest 请求。 */
XRT_API xhttp1requestplan* xrtHttp1RequestPrepareProxyDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
)
{
	return __xrtHttp1RequestPrepareDigest(
		pRequest,
		pOptions,
		pSession,
		EntityHash,
		XRT_STR_LITERAL("Proxy-Authorization"),
		ppExchange
	);
}

#endif
