#ifndef EXAMPLE_HTTP_ASYNC_BODY_H
#define EXAMPLE_HTTP_ASYNC_BODY_H

#include <string.h>

#include <xhttp.h>



/* 示例正文第一次返回 AGAIN，Future 完成后再发布固定字节。 */
typedef struct example_http_async_body {
	xbytesview Data;
	bool Ready;
	bool Sent;
} example_http_async_body;



/* 固定字节由示例状态长期持有，Chunk 释放时无需回收资源。 */
static void exampleHttpAsyncBodyRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 返回当前可用的正文租约。 */
static xhttpbodystatus exampleHttpAsyncBodyNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	example_http_async_body* pBody =
		(example_http_async_body*)pContext;

	(void)iMaxBytes;
	if ( !pBody->Ready ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pBody->Sent ) {
		return XHTTP_BODY_EOF;
	}
	pBody->Sent = true;
	pChunk->Data = pBody->Data.Data;
	pChunk->Size = pBody->Data.Size;
	pChunk->Release = exampleHttpAsyncBodyRelease;
	return XHTTP_BODY_DATA;
}



/* 用已完成 Future 模拟事件循环通知正文已经可读。 */
static xfuture* exampleHttpAsyncBodyWait(ptr pContext)
{
	example_http_async_body* pBody =
		(example_http_async_body*)pContext;
	xfuture* pFuture = NULL;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);

	if ( pPromise == NULL ) {
		return NULL;
	}
	pBody->Ready = true;
	if ( !xrtPromiseResolve(pPromise, NULL) ) {
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 为每次请求打开独立的示例 Reader 操作表。 */
static bool exampleHttpAsyncBodyOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = exampleHttpAsyncBodyNext;
	pOps->Wait = exampleHttpAsyncBodyWait;
	*ppReader = pFactory;
	return true;
}



/* 创建先等待一次、随后发送固定字节的可重放正文。 */
static xhttpbody* exampleHttpAsyncBodyCreate(
	example_http_async_body* pState,
	xbytesview Data
)
{
	static const xhttpbodyops Ops = {
		exampleHttpAsyncBodyOpen,
		NULL
	};

	pState->Data = Data;
	pState->Ready = false;
	pState->Sent = false;
	return xrtHttpBodyCreate(
		&Ops,
		pState,
		(uint64)Data.Size,
		XHTTP_BODY_REPLAYABLE
	);
}

#endif

