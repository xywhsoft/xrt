#include "../internal/xrt_http_server_runtime.h"
#include <xrt/http_server_raw.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_RAW)

/* 检查原始报文长度、标志、Worker 和唯一最终响应门。 */
static bool __xrtHttpConnRawValidate(
	xhttpconn* pConnection,
	uint64 iLength,
	uint32 iFlags
)
{
	if ( (iLength == 0) ||
		(iLength == XHTTP_BODY_UNKNOWN) ||
		((iFlags & ~XHTTP_SERVER_RAW_KEEP_ALIVE) != 0) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response requires a known nonzero length and valid flags",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpConnCanRespond(
		pConnection,
		"respond-raw-http-connection",
		true
	) ) {
		return false;
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"respond-raw-http-connection",
			"HTTP connection already committed its final response",
			NULL
		);
		return false;
	}
	return true;
}



/* 计算原始报文排空后能否安全进入下一条 HTTP 请求。 */
static bool __xrtHttpConnRawKeepAlive(
	xhttpconn* pConnection,
	uint32 iFlags
)
{
	const xhttpserverrequest* pRequest =
		xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		);

	return ((iFlags & XHTTP_SERVER_RAW_KEEP_ALIVE) != 0) &&
		(pRequest != NULL) &&
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_KEEP_ALIVE) != 0) &&
		xrtHttp1ServerExchangeComplete(
			pConnection->Exchange
		) &&
		!pConnection->ForceClose &&
		!pConnection->InputEnded &&
		(xrtHttpServerState(pConnection->Server) ==
		 XHTTP_SERVER_RUNNING);
}



/* 冻结原始 Body 为不附加任何协议字节的定长响应计划。 */
static xhttp1serverresponse* __xrtHttpConnRawPlan(
	xhttpconn* pConnection,
	xhttpbody* pBody,
	uint64 iLength,
	uint32 iFlags
)
{
	xhttp1serverresponse* pResponse =
		__xrtHttpConnWireResponse(
			pBody,
			iLength,
			!__xrtHttpConnRawKeepAlive(
				pConnection,
				iFlags
			),
			false
		);

	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_MEMORY,
			XHTTP_SERVER_ERROR_RESPONSE,
			"respond-raw-http-connection",
			"Raw HTTP response plan could not be created",
			xrtGetError()
		);
		return NULL;
	}
	return pResponse;
}



/* 验证并计算一组完整线缆引用的总长度。 */
static bool __xrtHttpConnRawRefsMeasure(
	const xnetref* pRefs,
	size_t iCount,
	uint64* pLength
)
{
	uint64 iLength = 0;

	if ( (pRefs == NULL) || (iCount == 0) ||
		(iCount > (SIZE_MAX / sizeof(*pRefs))) ||
		!__xrtRangeValid(pRefs, iCount * sizeof(*pRefs)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response references are invalid",
			NULL
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pRefs[i].Size == 0 ) {
			continue;
		}
		if ( !__xrtRangeValid(
			pRefs[i].Data,
			pRefs[i].Size
		) || (pRefs[i].Release == NULL) ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"respond-raw-http-connection",
				"Raw HTTP response reference requires valid bytes and release",
				NULL
			);
			return false;
		}
		#if SIZE_MAX > UINT64_MAX
			if ( pRefs[i].Size > (size_t)UINT64_MAX ) {
				__xrtHttpServerSetError(
					XERR_RANGE,
					XHTTP_SERVER_ERROR_ARGUMENT,
					"respond-raw-http-connection",
					"Raw HTTP response reference length overflowed",
					NULL
				);
				return false;
			}
		#endif
		if ( (uint64)pRefs[i].Size >
			(UINT64_MAX - iLength) ) {
			__xrtHttpServerSetError(
				XERR_RANGE,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"respond-raw-http-connection",
				"Raw HTTP response reference length overflowed",
				NULL
			);
			return false;
		}
		iLength += (uint64)pRefs[i].Size;
	}
	if ( iLength == 0 ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response references are empty",
			NULL
		);
		return false;
	}
	*pLength = iLength;
	return true;
}



/* xrtMalloc 接管路径使用统一网络引用释放签名。 */
static void __xrtHttpConnRawTakeRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 以已经验证的 Body 提交完整原始响应。 */
static xnetresult __xrtHttpConnRespondRawBody(
	xhttpconn* pConnection,
	xhttpbody* pBody,
	uint64 iLength,
	uint32 iFlags
)
{
	xhttp1serverresponse* pResponse =
		__xrtHttpConnRawPlan(
			pConnection,
			pBody,
			iLength,
			iFlags
		);

	if ( pResponse == NULL ) {
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		"respond-raw-http-connection",
		false
	);
}



/* 复制并提交一条完整的 HTTP/1 线缆响应。 */
XRT_API xnetresult xrtHttpConnRespondRaw(
	xhttpconn* pConnection,
	xbytesview Response,
	uint32 iFlags
)
{
	xhttpbody* pBody;
	xnetresult Result;

	if ( (Response.Size == 0) ||
		!__xrtRangeValid(Response.Data, Response.Size) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response requires a complete nonempty byte range",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtHttpConnRawValidate(
		pConnection,
		(uint64)Response.Size,
		iFlags
	) ) {
		return XNET_RESULT_ERROR;
	}
	pBody = xrtHttpBodyCopy(Response);
	if ( pBody == NULL ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(),
				XERR_MEMORY
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			"respond-raw-http-connection",
			"Raw HTTP response bytes could not be copied",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	Result = __xrtHttpConnRespondRawBody(
		pConnection,
		pBody,
		(uint64)Response.Size,
		iFlags
	);
	xrtHttpBodyDestroy(pBody);
	return Result;
}



/* 原子接管一组完整线缆引用并提交零复制响应计划。 */
XRT_API xnetresult xrtHttpConnRespondRawRefs(
	xhttpconn* pConnection,
	const xnetref* pResponses,
	size_t iCount,
	uint32 iFlags
)
{
	xhttp1serverresponse* pResponse;
	uint64 iLength;

	if ( !__xrtHttpConnRawRefsMeasure(
		pResponses,
		iCount,
		&iLength
	) || !__xrtHttpConnRawValidate(
		pConnection,
		iLength,
		iFlags
	) ) {
		return XNET_RESULT_ERROR;
	}
	pResponse = __xrtHttpConnWireRefsResponse(
		pResponses,
		iCount,
		iLength,
		!__xrtHttpConnRawKeepAlive(pConnection, iFlags),
		false
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_MEMORY,
			XHTTP_SERVER_ERROR_RESPONSE,
			"respond-raw-http-connection",
			"Raw HTTP response reference plan could not be created",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		"respond-raw-http-connection",
		false
	);
}



/* 接管一个完整线缆引用。 */
XRT_API xnetresult xrtHttpConnRespondRawRef(
	xhttpconn* pConnection,
	const xnetref* pResponse,
	uint32 iFlags
)
{
	return xrtHttpConnRespondRawRefs(
		pConnection,
		pResponse,
		1,
		iFlags
	);
}



/* 成功时接管由 xrtMalloc 分配的完整线缆响应。 */
XRT_API xnetresult xrtHttpConnRespondRawTake(
	xhttpconn* pConnection,
	ptr pResponse,
	size_t iSize,
	uint32 iFlags
)
{
	xnetref Ref = {
		(cbytes)pResponse,
		iSize,
		__xrtHttpConnRawTakeRelease,
		NULL
	};

	if ( (pResponse == NULL) || (iSize == 0) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response take requires nonempty xrtMalloc bytes",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	return xrtHttpConnRespondRawRef(
		pConnection,
		&Ref,
		iFlags
	);
}



/* 保留一个已知长度 Body 并把它作为完整 HTTP/1 线缆响应。 */
XRT_API xnetresult xrtHttpConnRespondRawBody(
	xhttpconn* pConnection,
	xhttpbody* pResponse,
	uint32 iFlags
)
{
	uint64 iLength;

	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response Body is null",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	iLength = xrtHttpBodyLength(pResponse);
	if ( !__xrtHttpConnRawValidate(
		pConnection,
		iLength,
		iFlags
	) ) {
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnRespondRawBody(
		pConnection,
		pResponse,
		iLength,
		iFlags
	);
}

#endif
