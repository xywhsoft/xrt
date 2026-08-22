#include "../internal/xrt_http_server.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE)

/* 返回原因链当前最内层可见类别或保守默认值。 */
static xerrkind __xrtHttp1ServerResponseCauseKind(
	xerrkind Fallback,
	const xerror* pCause
)
{
	const xerror* pCurrent = pCause;
	xerrkind Kind = Fallback;

	while ( pCurrent != NULL ) {
		xerrkind Current = xrtErrorKind(pCurrent);

		if ( Current != XERR_NONE ) {
			Kind = Current;
		}
		pCurrent = xrtErrorCause(pCurrent);
	}
	return Kind;
}



/* 建立并保存唯一 Server Response 终态错误。 */
bool __xrtHttp1ServerResponseFailCause(
	xhttp1serverresponse* pResponse,
	xhttp1serverresponseerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pCauseRef;
	xerror* pError;
	xerrordesc Desc;

	if ( pResponse == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pResponse->Error != NULL ) {
		xrtSetError(pResponse->Error);
		return false;
	}
	pCauseRef = xrtErrorRef(pCause);
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = __xrtHttp1ServerResponseCauseKind(
		Kind, pCauseRef
	);
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http.server.response";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCauseRef;
	pError = xrtErrorBuild(&Desc);
	xrtErrorFree(pCauseRef);
	if ( pError == NULL ) {
		pError = xrtTakeError();
	}
	pResponse->Error = pError;
	pResponse->State = XRT_HTTP_SERVER_RESPONSE_FAILED;
	if ( pResponse->Error != NULL ) {
		xrtSetError(pResponse->Error);
	}
	return false;
}



/* 建立没有额外原因链的 Server Response 终态错误。 */
bool __xrtHttp1ServerResponseFail(
	xhttp1serverresponse* pResponse,
	xhttp1serverresponseerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	return __xrtHttp1ServerResponseFailCause(
		pResponse,
		Code,
		Kind,
		sOperation,
		sMessage,
		NULL
	);
}



/* 保持调用方当前错误并释放一个已经转移的 Wire 引用。 */
static void __xrtHttp1ServerResponseRefRelease(xhttpbodychunk* pRef)
{
	xhttpbodychunk Ref;
	xerror* pPrevious;
	xerror* pDiscard;

	if ( (pRef == NULL) || (pRef->Size == 0) ) {
		return;
	}
	Ref = *pRef;
	memset(pRef, 0, sizeof(*pRef));
	pPrevious = __xrtErrorSwapOwned(NULL);
	Ref.Release(Ref.Context, Ref.Data, Ref.Size);
	pDiscard = __xrtErrorSwapOwned(pPrevious);
	xrtErrorFree(pDiscard);
}



/* 最终响应被连接成功受理后，接管其全部 Wire 引用。 */
void __xrtHttp1ServerResponseOwnRefs(
	xhttp1serverresponse* pResponse
)
{
	if ( pResponse != NULL ) {
		pResponse->WireRefsOwned = true;
	}
}



/* 当前 Wire 引用完整离队后归还调用方资源。 */
void __xrtHttp1ServerResponseReleaseCurrentRef(
	xhttp1serverresponse* pResponse
)
{
	if ( (pResponse == NULL) || !pResponse->WireRefsOwned ||
		(pResponse->WireRefIndex >= pResponse->WireRefCount) ) {
		return;
	}
	__xrtHttp1ServerResponseRefRelease(
		&pResponse->WireRefs[pResponse->WireRefIndex]
	);
	pResponse->WireRefIndex++;
}



/* 销毁响应计划和全部尚未归还的正文资产。 */
XRT_API void xrtHttp1ServerResponseDestroy(
	xhttp1serverresponse* pResponse
)
{
	if ( pResponse == NULL ) {
		return;
	}
	xrtHttpBodyChunkRelease(&pResponse->Chunk);
	xrtHttpBodyReaderDestroy(pResponse->Reader);
	xrtHttpBodyDestroy(pResponse->Body);
	if ( pResponse->WireRefsOwned ) {
		for ( size_t i = pResponse->WireRefIndex;
			i < pResponse->WireRefCount; i++ ) {
			__xrtHttp1ServerResponseRefRelease(
				&pResponse->WireRefs[i]
			);
		}
	}
	xrtErrorFree(pResponse->Error);
	memset(pResponse, 0, sizeof(*pResponse));
	xrtFree(pResponse);
}



/* 返回稳定响应错误。 */
XRT_API const xerror* xrtHttp1ServerResponseError(
	const xhttp1serverresponse* pResponse
)
{
	return pResponse != NULL ? pResponse->Error : NULL;
}



/* 判断全部线路字节是否已经确认消费。 */
XRT_API bool xrtHttp1ServerResponseComplete(
	const xhttp1serverresponse* pResponse
)
{
	return (pResponse != NULL) &&
		((pResponse->State == XRT_HTTP_SERVER_RESPONSE_DONE) ||
		 (pResponse->State == XRT_HTTP_SERVER_RESPONSE_TUNNEL));
}



/* 返回响应后的连接关闭决定。 */
XRT_API bool xrtHttp1ServerResponseClose(
	const xhttp1serverresponse* pResponse
)
{
	return (pResponse != NULL) && pResponse->Close;
}



/* 返回响应后的协议交接决定。 */
XRT_API bool xrtHttp1ServerResponseTunnel(
	const xhttp1serverresponse* pResponse
)
{
	return (pResponse != NULL) && pResponse->Tunnel;
}



/* 判断响应计划是否为不会结束当前请求的信息响应。 */
XRT_API bool xrtHttp1ServerResponseInformational(
	const xhttp1serverresponse* pResponse
)
{
	return (pResponse != NULL) && pResponse->Informational;
}



/* 返回已经确认消费的响应线路字节数。 */
XRT_API uint64 xrtHttp1ServerResponseWireBytes(
	const xhttp1serverresponse* pResponse
)
{
	return pResponse != NULL ? pResponse->WireBytes : 0;
}

#endif

