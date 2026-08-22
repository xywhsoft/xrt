#include "../internal/xrt_http_server_file.h"
#include "../internal/xrt_future.h"



#if defined(XRT_FEATURE_HTTP_SERVER_FILE)

/* 延续上下文只在 Reply 尚未转移给输出 Future 时拥有它。 */
typedef struct xrt_http_server_file_context {
	xhttpreply* Reply;
} xrt_http_server_file_context;



/* Future 最后释放文件 Reply 时销毁完整响应对象。 */
static void __xrtHttpServerFileReplyFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpReplyDestroy((xhttpreply*)pValue);
}



/* 释放延续上下文仍然拥有的半成品 Reply。 */
static void __xrtHttpServerFileContextFree(
	ptr pValue,
	ptr pData
)
{
	xrt_http_server_file_context* pContext =
		(xrt_http_server_file_context*)pValue;

	(void)pData;
	if ( pContext != NULL ) {
		xrtHttpReplyDestroy(pContext->Reply);
		xrtFree(pContext);
	}
}



/* 用当前结构化错误完成输出 Promise；没有错误时补内部错误。 */
static void __xrtHttpServerFileReject(xpromise* pOutput)
{
	xerror* pError = xrtTakeError();

	if ( pError == NULL ) {
		__xrtErrorSetInternal();
		pError = xrtTakeError();
	}
	if ( pError != NULL ) {
		(void)xrtPromiseReject(pOutput, pError);
	} else {
		(void)xrtPromiseClose(pOutput);
	}
	xrtErrorFree(pError);
}



/* 把成功准备的文件 Body 安装到 Reply，并转移给输出 Future。 */
static void __xrtHttpServerFileBodyReady(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	xrt_http_server_file_context* pContext =
		(xrt_http_server_file_context*)pData;
	xhttpbody* pBody = pInput != NULL ?
		(xhttpbody*)pInput->Value : NULL;

	if ( (pContext == NULL) ||
		(pContext->Reply == NULL) ||
		(pBody == NULL) ) {
		__xrtErrorSetInternal();
		__xrtHttpServerFileReject(pOutput);
		return;
	}
	if ( !xrtHttpReplySetBody(
		pContext->Reply,
		pBody
	) ) {
		__xrtHttpServerFileReject(pOutput);
		return;
	}
	if ( xrtPromiseResolveOwned(
		pOutput,
		pContext->Reply,
		__xrtHttpServerFileReplyFree,
		NULL
	) ) {
		pContext->Reply = NULL;
	}
}



/* 创建 Reply、提交文件准备，并用通用 Future 延续组合两层所有权。 */
static xfuture* __xrtHttpReplyFileFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength,
	bool bRange
)
{
	xrt_http_server_file_context* pContext;
	xfuture* pBodyFuture;
	xfuture* pReplyFuture;

	if ( (pPool == NULL) ||
		(sPath == NULL) ||
		(sPath[0] == '\0') ||
		((ContentType.Size != 0) &&
		 (ContentType.Data == NULL)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pContext = (xrt_http_server_file_context*)xrtCalloc(
		1,
		sizeof(xrt_http_server_file_context)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Reply = xrtHttpReplyCreate(iStatus);
	if ( (pContext->Reply == NULL) ||
		((ContentType.Size != 0) &&
		 !xrtHttpReplySetHeader(
			pContext->Reply,
			XRT_STR_LITERAL("Content-Type"),
			ContentType
		 )) ) {
		__xrtHttpServerFileContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	pBodyFuture = bRange ?
		xrtHttpBodyFileRangeFuture(
			pPool,
			sPath,
			iOffset,
			iLength,
			NULL
		) :
		xrtHttpBodyFileFuture(pPool, sPath, NULL);
	if ( pBodyFuture == NULL ) {
		__xrtHttpServerFileContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	pReplyFuture = __xrtFutureThenOwnedCancelSource(
		pBodyFuture,
		__xrtHttpServerFileBodyReady,
		pContext,
		__xrtHttpServerFileContextFree,
		NULL
	);
	xrtFutureDestroy(pBodyFuture);
	if ( pReplyFuture == NULL ) {
		__xrtHttpServerFileContextFree(
			pContext,
			NULL
		);
	}
	return pReplyFuture;
}



/* 绑定文件 Reply Future，并释放 Helper 持有的本地 Future 引用。 */
static bool __xrtHttpConnFile(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength,
	bool bRange
)
{
	xfuture* pFuture;
	bool bResult;

	if ( pConnection == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pFuture = __xrtHttpReplyFileFuture(
		pPool,
		iStatus,
		ContentType,
		sPath,
		iOffset,
		iLength,
		bRange
	);
	if ( pFuture == NULL ) {
		return false;
	}
	bResult = xrtHttpConnRespondFuture(
		pConnection,
		pFuture
	);
	xrtFutureDestroy(pFuture);
	return bResult;
}



/* 异步构建完整文件 Reply。 */
XRT_API xfuture* xrtHttpReplyFileFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath
)
{
	return __xrtHttpReplyFileFuture(
		pPool,
		iStatus,
		ContentType,
		sPath,
		0,
		0,
		false
	);
}



/* 异步构建严格文件区间 Reply。 */
XRT_API xfuture* xrtHttpReplyFileRangeFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength
)
{
	return __xrtHttpReplyFileFuture(
		pPool,
		iStatus,
		ContentType,
		sPath,
		iOffset,
		iLength,
		true
	);
}



/* 一次调用准备完整文件并绑定最终响应。 */
XRT_API bool xrtHttpConnFile(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath
)
{
	return __xrtHttpConnFile(
		pConnection,
		pPool,
		iStatus,
		ContentType,
		sPath,
		0,
		0,
		false
	);
}



/* 一次调用准备严格文件区间并绑定最终响应。 */
XRT_API bool xrtHttpConnFileRange(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength
)
{
	return __xrtHttpConnFile(
		pConnection,
		pPool,
		iStatus,
		ContentType,
		sPath,
		iOffset,
		iLength,
		true
	);
}

#endif
