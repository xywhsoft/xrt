#include "../internal/xrt_http_body.h"



#if defined(XRT_FEATURE_HTTP_BODY)

/* 创建并设置正文域错误。 */
static void __xrtHttpBodyError(
	xerrkind Kind,
	xhttpbodyerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "http.body";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 把来源发布的新错误装回当前上下文，并释放被替换的旧错误。 */
void __xrtHttpBodySourceErrorCommit(xerror* pSourceError)
{
	xerror* pPrevious;

	if ( pSourceError == NULL ) {
		return;
	}
	pPrevious = __xrtErrorSwapOwned(pSourceError);
	xrtErrorFree(pPrevious);
}



/* 保存当前上下文错误并固定 Reader 的失败终态。 */
static void __xrtHttpBodyReaderCaptureCurrent(
	xhttpbodyreader* pReader
)
{
	xrtErrorFree(pReader->Error);
	pReader->Error = xrtErrorRef(xrtGetError());
	pReader->Failed = true;
	pReader->Again = false;
}



/* 消费来源发布的新错误；来源未设置错误时补充正文域错误。 */
void __xrtHttpBodyReaderCaptureSource(
	xhttpbodyreader* pReader,
	xerror* pSourceError,
	xhttpbodyerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	if ( pSourceError == NULL ) {
		__xrtHttpBodyError(
			XERR_IO, Code, sOperation, sMessage
		);
	} else {
		__xrtHttpBodySourceErrorCommit(pSourceError);
	}
	__xrtHttpBodyReaderCaptureCurrent(pReader);
}



/* 执行来源 Close，并恢复调用方进入清理前持有的当前错误。 */
static void __xrtHttpBodyClosePreserveError(
	xhttpbodycloseproc pClose,
	ptr pContext
)
{
	xerror* pPrevious;
	xerror* pDiscard;

	if ( pClose == NULL ) {
		return;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	pClose(pContext);
	pDiscard = __xrtErrorSwapOwned(pPrevious);
	xrtErrorFree(pDiscard);
}



/* 发布并保存正文域错误。 */
xhttpbodystatus __xrtHttpBodyReaderFail(
	xhttpbodyreader* pReader,
	xerrkind Kind,
	xhttpbodyerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtHttpBodyError(Kind, Code, sOperation, sMessage);
	if ( pReader != NULL ) {
		__xrtHttpBodyReaderCaptureCurrent(pReader);
	}
	return XHTTP_BODY_ERROR;
}



/* 释放由 xrtHttpBodyTake 接管的数据。 */
static void __xrtHttpBodyFreeData(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* Chunk 保存一个正文引用，释放时归还该引用。 */
static void __xrtHttpBodyReleaseChunk(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	xrtHttpBodyDestroy((xhttpbody*)pContext);
}



/* 验证字节视图能够安全转换为公开 uint64 长度。 */
static bool __xrtHttpBodyBytesValid(xbytesview Data)
{
	if ( (Data.Data == NULL) && (Data.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
#if SIZE_MAX > UINT64_MAX
	if ( Data.Size > (size_t)UINT64_MAX ) {
		__xrtErrorSetRange();
		return false;
	}
#endif
	return true;
}



/* 初始化已经清零并分配完成的固定字节正文。 */
static void __xrtHttpBodyBytesInit(
	xhttpbody* pBody,
	xbytesview Data,
	xhttpbodyreleaseproc pRelease,
	ptr pContext
)
{
	pBody->RefCount = 1;
	pBody->Kind = XRT_HTTP_BODY_BYTES;
	pBody->Flags = XHTTP_BODY_REPLAYABLE;
	pBody->Length = (uint64)Data.Size;
	pBody->Bytes = Data;
	pBody->Release = pRelease;
	pBody->ReleaseContext = pContext;
}



/* 创建固定字节正文，不在失败路径接管数据释放责任。 */
static xhttpbody* __xrtHttpBodyBytesCreate(
	xbytesview Data,
	xhttpbodyreleaseproc pRelease,
	ptr pContext
)
{
	xhttpbody* pBody;

	if ( !__xrtHttpBodyBytesValid(Data) ) {
		return NULL;
	}
	pBody = (xhttpbody*)xrtCalloc(1, sizeof(*pBody));
	if ( pBody == NULL ) {
		return NULL;
	}
	__xrtHttpBodyBytesInit(
		pBody,
		Data,
		pRelease,
		pContext
	);
	return pBody;
}



/* 验证自定义正文工厂和公开标志。 */
static bool __xrtHttpBodyFactoryValid(
	const xhttpbodyops* pOps,
	uint32 iFlags
)
{
	if ( (pOps == NULL) || (pOps->Open == NULL) ||
		( (iFlags & ~(uint32)XHTTP_BODY_REPLAYABLE) != 0 ) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证自定义 Reader 操作完整且适配当前裁剪能力。 */
static bool __xrtHttpBodyReaderOpsValid(
	const xhttpbodyreaderops* pOps
)
{
	if ( (pOps == NULL) || (pOps->Next == NULL) ) {
		return false;
	}
	return true;
}



/* 创建自定义正文源。 */
XRT_API xhttpbody* xrtHttpBodyCreate(
	const xhttpbodyops* pOps,
	ptr pFactory,
	uint64 iLength,
	uint32 iFlags
)
{
	xhttpbody* pBody;

	if ( !__xrtHttpBodyFactoryValid(pOps, iFlags) ) {
		return NULL;
	}
	pBody = (xhttpbody*)xrtCalloc(1, sizeof(*pBody));
	if ( pBody == NULL ) {
		return NULL;
	}
	pBody->RefCount = 1;
	pBody->Kind = XRT_HTTP_BODY_CUSTOM;
	pBody->Flags = iFlags;
	pBody->Length = iLength;
	pBody->Ops = *pOps;
	pBody->Factory = pFactory;
	return pBody;
}



/* 创建可重放的空正文。 */
XRT_API xhttpbody* xrtHttpBodyEmpty(void)
{
	return __xrtHttpBodyBytesCreate(
		(xbytesview){ NULL, 0 }, NULL, NULL
	);
}



/* 创建拥有正文副本的可重放正文。 */
XRT_API xhttpbody* xrtHttpBodyCopy(xbytesview Data)
{
	xhttpbody* pBody;
	bytes pCopy;
	size_t iTotal;

	if ( !__xrtHttpBodyBytesValid(Data) ) {
		return NULL;
	}
	if ( Data.Size > (SIZE_MAX - sizeof(*pBody)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iTotal = sizeof(*pBody) + Data.Size;
	pBody = (xhttpbody*)xrtCalloc(1, iTotal);
	if ( pBody == NULL ) {
		return NULL;
	}
	pCopy = (bytes)(pBody + 1);
	if ( Data.Size != 0 ) {
		memcpy(pCopy, Data.Data, Data.Size);
	}
	__xrtHttpBodyBytesInit(
		pBody,
		(xbytesview){ pCopy, Data.Size },
		NULL,
		NULL
	);
	return pBody;
}



/* 创建借用外部内存的可重放正文。 */
XRT_API xhttpbody* xrtHttpBodyBorrow(xbytesview Data)
{
	return __xrtHttpBodyBytesCreate(Data, NULL, NULL);
}



/* 成功时接管由 xrtMalloc 分配的数据。 */
XRT_API xhttpbody* xrtHttpBodyTake(ptr pData, size_t iSize)
{
	return __xrtHttpBodyBytesCreate(
		(xbytesview){ (cbytes)pData, iSize },
		__xrtHttpBodyFreeData,
		NULL
	);
}



/* 创建带自定义释放过程的可重放正文。 */
XRT_API xhttpbody* xrtHttpBodyReference(
	xbytesview Data,
	xhttpbodyreleaseproc pRelease,
	ptr pContext
)
{
	if ( pRelease == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtHttpBodyBytesCreate(Data, pRelease, pContext);
}



/* 增加正文对象引用并返回原指针。 */
XRT_API xhttpbody* xrtHttpBodyRef(xhttpbody* pBody)
{
	if ( (pBody == NULL) ||
		(xrtRefRetain(&pBody->RefCount) < 0) ) {
		if ( pBody == NULL ) {
			__xrtErrorSetInvalidArgument();
		} else {
			__xrtErrorSetInvalidState();
		}
		return NULL;
	}
	return pBody;
}



/* 释放正文对象的最后一个引用及其工厂资产。 */
XRT_API void xrtHttpBodyDestroy(xhttpbody* pBody)
{
	xerror* pPrevious;
	xerror* pDiscard;

	if ( (pBody == NULL) ||
		(xrtRefRelease(&pBody->RefCount) != 0) ) {
		return;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	if ( pBody->Kind == XRT_HTTP_BODY_CUSTOM ) {
		if ( pBody->Ops.Destroy != NULL ) {
			pBody->Ops.Destroy(pBody->Factory);
		}
	} else if ( pBody->Release != NULL ) {
		pBody->Release(
			pBody->ReleaseContext,
			pBody->Bytes.Data,
			pBody->Bytes.Size
		);
	}
	pDiscard = __xrtErrorSwapOwned(pPrevious);
	xrtErrorFree(pDiscard);
	memset(pBody, 0, sizeof(*pBody));
	xrtFree(pBody);
}



/* 返回正文线性字节长度或未知长度哨兵。 */
XRT_API uint64 xrtHttpBodyLength(const xhttpbody* pBody)
{
	if ( pBody == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_UNKNOWN;
	}
	return pBody->Length;
}



/* 返回正文源公开能力标志。 */
XRT_API uint32 xrtHttpBodyFlags(const xhttpbody* pBody)
{
	if ( pBody == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_NONE;
	}
	return pBody->Flags;
}



/* 判断正文能否重新 Open 并从头读取。 */
XRT_API bool xrtHttpBodyReplayable(const xhttpbody* pBody)
{
	return (pBody != NULL) &&
		( (pBody->Flags & XHTTP_BODY_REPLAYABLE) != 0 );
}



/* 借用固定正文的连续字节视图，自定义来源没有连续视图。 */
XRT_API bool xrtHttpBodyView(
	const xhttpbody* pBody,
	xbytesview* pData
)
{
	xbytesview Data = { NULL, 0 };

	if ( (pBody == NULL) || (pData == NULL) ||
		__xrtRangesOverlap(
			pBody, sizeof(*pBody), pData, sizeof(*pData)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pBody->Kind == XRT_HTTP_BODY_BYTES) &&
		__xrtRangesOverlap(
			pData,
			sizeof(*pData),
			pBody->Bytes.Data,
			pBody->Bytes.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBody->Kind != XRT_HTTP_BODY_BYTES ) {
		*pData = Data;
		return false;
	}
	Data = pBody->Bytes;
	*pData = Data;
	return true;
}



/* 为固定字节或自定义来源打开一个独立 Reader。 */
XRT_API xhttpbodyreader* xrtHttpBodyOpen(xhttpbody* pBody)
{
	xhttpbodyreader* pReader;
	xhttpbodyreaderops Ops;
	ptr pContext = NULL;
	bool bOpened = false;
	xerror* pPrevious;
	xerror* pSourceError;

	if ( pBody == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pBody->Flags & XHTTP_BODY_REPLAYABLE) == 0 ) {
		if ( __xrtAtomicRefCompareExchange(
			&pBody->Opened, 1, 0
		) != 0 ) {
			__xrtHttpBodyError(
				XERR_STATE,
				XHTTP_BODY_ERROR_REOPEN,
				"open",
				"non-replayable HTTP body was already opened"
			);
			return NULL;
		}
	}
	if ( xrtHttpBodyRef(pBody) == NULL ) {
		return NULL;
	}
	memset(&Ops, 0, sizeof(Ops));
	if ( pBody->Kind == XRT_HTTP_BODY_CUSTOM ) {
		pPrevious = __xrtErrorSwapOwned(NULL);
		bOpened = pBody->Ops.Open(
			pBody->Factory, &Ops, &pContext
		);
		pSourceError = __xrtErrorSwapOwned(pPrevious);
		if ( !bOpened ) {
			if ( pSourceError == NULL ) {
				__xrtHttpBodyError(
					XERR_IO,
					XHTTP_BODY_ERROR_SOURCE,
					"open",
					"HTTP body source failed to open"
				);
			} else {
				__xrtHttpBodySourceErrorCommit(pSourceError);
			}
			xrtHttpBodyDestroy(pBody);
			return NULL;
		}
		__xrtHttpBodySourceErrorCommit(pSourceError);
		if ( !__xrtHttpBodyReaderOpsValid(&Ops) ) {
			__xrtHttpBodyClosePreserveError(
				Ops.Close, pContext
			);
			xrtHttpBodyDestroy(pBody);
			__xrtHttpBodyError(
				XERR_INTERNAL,
				XHTTP_BODY_ERROR_CONTRACT,
				"open",
				"HTTP body source returned invalid reader operations"
			);
			return NULL;
		}
	}
	pReader = (xhttpbodyreader*)xrtCalloc(1, sizeof(*pReader));
	if ( pReader == NULL ) {
		if ( bOpened ) {
			__xrtHttpBodyClosePreserveError(
				Ops.Close, pContext
			);
		}
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	pReader->Body = pBody;
	pReader->Ops = Ops;
	pReader->Context = pContext;
	return pReader;
}



/* 发布固定字节正文的下一段独立租约。 */
static xhttpbodystatus __xrtHttpBodyBytesNext(
	xhttpbodyreader* pReader,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	size_t iRemaining;
	size_t iSize;
	xhttpbody* pBody = pReader->Body;

	if ( pReader->Offset == pBody->Bytes.Size ) {
		return XHTTP_BODY_EOF;
	}
	iRemaining = pBody->Bytes.Size - pReader->Offset;
	iSize = iRemaining < iMaxBytes ? iRemaining : iMaxBytes;
	if ( xrtHttpBodyRef(pBody) == NULL ) {
		return XHTTP_BODY_ERROR;
	}
	pChunk->Data = pBody->Bytes.Data + pReader->Offset;
	pChunk->Size = iSize;
	pChunk->Release = __xrtHttpBodyReleaseChunk;
	pChunk->Context = pBody;
	pReader->Offset += iSize;
	return XHTTP_BODY_DATA;
}



/* 验证来源发布的 Chunk 及状态组合。 */
static bool __xrtHttpBodyResultValid(
	xhttpbodystatus Status,
	size_t iMaxBytes,
	const xhttpbodychunk* pChunk
)
{
	if ( Status == XHTTP_BODY_DATA ) {
		return (pChunk->Size != 0) &&
			__xrtRangeValid(pChunk->Data, pChunk->Size) &&
			(pChunk->Size <= iMaxBytes) &&
			(pChunk->Release != NULL);
	}
	return (Status >= XHTTP_BODY_ERROR) &&
		(Status <= XHTTP_BODY_AGAIN) &&
		(pChunk->Data == NULL) &&
		(pChunk->Size == 0) &&
		(pChunk->Release == NULL) &&
		(pChunk->Context == NULL);
}



/* 校验已知正文长度不会被来源提前结束或越界发布。 */
static bool __xrtHttpBodyLengthValid(
	const xhttpbodyreader* pReader,
	xhttpbodystatus Status,
	size_t iSize
)
{
	uint64 iLength = pReader->Body->Length;

	if ( iLength == XHTTP_BODY_UNKNOWN ) {
		return true;
	}
	if ( Status == XHTTP_BODY_EOF ) {
		return pReader->Bytes == iLength;
	}
	if ( Status == XHTTP_BODY_DATA ) {
		return (pReader->Bytes <= iLength) &&
			( (uint64)iSize <= (iLength - pReader->Bytes) );
	}
	return true;
}



/* 判断输出范围是否会覆盖 Reader 或正文对象的内部状态。 */
static bool __xrtHttpBodyReaderStateOverlaps(
	const xhttpbodyreader* pReader,
	const void* pOutput,
	size_t iSize
)
{
	if ( (pReader == NULL) || (pOutput == NULL) ||
		(iSize == 0) ) {
		return false;
	}
	return __xrtRangesOverlap(
		pOutput, iSize, pReader, sizeof(*pReader)
	) || ( (pReader->Body != NULL) && __xrtRangesOverlap(
		pOutput, iSize, pReader->Body, sizeof(*pReader->Body)
	) );
}



/* 判断输出范围是否会覆盖固定正文的数据字节。 */
static bool __xrtHttpBodyReaderBytesOverlap(
	const xhttpbodyreader* pReader,
	const void* pOutput,
	size_t iSize
)
{
	return (pReader != NULL) && (pOutput != NULL) &&
		(iSize != 0) && (pReader->Body != NULL) &&
		(pReader->Body->Kind == XRT_HTTP_BODY_BYTES) &&
		__xrtRangesOverlap(
			pOutput,
			iSize,
			pReader->Body->Bytes.Data,
			pReader->Body->Bytes.Size
		);
}



/* 读取下一个拥有型 Chunk 并稳定推进 Reader 终态。 */
XRT_API xhttpbodystatus xrtHttpBodyNext(
	xhttpbodyreader* pReader,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xhttpbodystatus Status;
	xerror* pPrevious;
	xerror* pSourceError = NULL;

	if ( pChunk == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_ERROR;
	}
	if ( __xrtHttpBodyReaderStateOverlaps(
		pReader, pChunk, sizeof(*pChunk)
	) || __xrtHttpBodyReaderBytesOverlap(
		pReader, pChunk, sizeof(*pChunk)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_ERROR;
	}
	memset(pChunk, 0, sizeof(*pChunk));
	if ( (pReader == NULL) ||
		(iMaxBytes == 0) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_ERROR;
	}
	if ( pReader->Failed ) {
		xrtSetError(pReader->Error);
		return XHTTP_BODY_ERROR;
	}
	if ( pReader->Done ) {
		return XHTTP_BODY_EOF;
	}
	if ( pReader->Body->Kind == XRT_HTTP_BODY_BYTES ) {
		Status = __xrtHttpBodyBytesNext(
			pReader, iMaxBytes, pChunk
		);
	} else {
		pPrevious = __xrtErrorSwapOwned(NULL);
		Status = pReader->Ops.Next(
			pReader->Context, iMaxBytes, pChunk
		);
		pSourceError = __xrtErrorSwapOwned(pPrevious);
	}
	if ( !__xrtHttpBodyResultValid(Status, iMaxBytes, pChunk) ) {
		xrtErrorFree(pSourceError);
		xrtHttpBodyChunkRelease(pChunk);
		return __xrtHttpBodyReaderFail(
			pReader,
			XERR_INTERNAL,
			XHTTP_BODY_ERROR_CONTRACT,
			"next",
			"HTTP body source returned an invalid status or chunk"
		);
	}
	if ( !__xrtHttpBodyLengthValid(
		pReader, Status, pChunk->Size
	) ) {
		xrtErrorFree(pSourceError);
		xrtHttpBodyChunkRelease(pChunk);
		return __xrtHttpBodyReaderFail(
			pReader,
			XERR_PROTOCOL,
			XHTTP_BODY_ERROR_LENGTH,
			"next",
			"HTTP body source did not match its declared length"
		);
	}
	if ( Status == XHTTP_BODY_ERROR ) {
		__xrtHttpBodyReaderCaptureSource(
			pReader,
			pSourceError,
			XHTTP_BODY_ERROR_SOURCE,
			"next",
			"HTTP body source failed while reading"
		);
		return XHTTP_BODY_ERROR;
	}
	__xrtHttpBodySourceErrorCommit(pSourceError);
	if ( Status == XHTTP_BODY_EOF ) {
		pReader->Done = true;
		pReader->Again = false;
		return XHTTP_BODY_EOF;
	}
	if ( Status == XHTTP_BODY_AGAIN ) {
#if !defined(XRT_FEATURE_HTTP_BODY_ASYNC)
		return __xrtHttpBodyReaderFail(
			pReader,
			XERR_UNSUPPORTED,
			XHTTP_BODY_ERROR_CONTRACT,
			"next",
			"HTTP body source returned AGAIN without async body support"
		);
#else
		if ( pReader->Ops.Wait == NULL ) {
			return __xrtHttpBodyReaderFail(
				pReader,
				XERR_INTERNAL,
				XHTTP_BODY_ERROR_CONTRACT,
				"next",
				"HTTP body source returned AGAIN without a wait operation"
			);
		}
		pReader->Again = true;
		return XHTTP_BODY_AGAIN;
#endif
	}
	pReader->Again = false;
	pReader->Bytes += (uint64)pChunk->Size;
	return XHTTP_BODY_DATA;
}



/* 复制读取一个 Chunk 并立即归还数据租约。 */
XRT_API xhttpbodystatus xrtHttpBodyRead(
	xhttpbodyreader* pReader,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;

	if ( pSize == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_ERROR;
	}
	if ( __xrtHttpBodyReaderStateOverlaps(
		pReader, pOutput, iCapacity
	) || __xrtHttpBodyReaderStateOverlaps(
		pReader, pSize, sizeof(*pSize)
	) || __xrtHttpBodyReaderBytesOverlap(
		pReader, pOutput, iCapacity
	) || __xrtHttpBodyReaderBytesOverlap(
		pReader, pSize, sizeof(*pSize)
	) || ( (pOutput != NULL) && (iCapacity != 0) &&
		__xrtRangesOverlap(
		pOutput, iCapacity, pSize, sizeof(*pSize)
	) ) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_ERROR;
	}
	*pSize = 0;
	if ( (pReader == NULL) || (pOutput == NULL) ||
		(iCapacity == 0) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_ERROR;
	}
	Status = xrtHttpBodyNext(
		pReader, iCapacity, &Chunk
	);
	if ( Status == XHTTP_BODY_DATA ) {
		memmove(pOutput, Chunk.Data, Chunk.Size);
		*pSize = Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	return Status;
}



/* 释放 Chunk 数据租约并清空结构。 */
XRT_API void xrtHttpBodyChunkRelease(xhttpbodychunk* pChunk)
{
	xhttpbodyreleaseproc pRelease;
	ptr pContext;
	cbytes pData;
	size_t iSize;
	xerror* pPrevious;
	xerror* pDiscard;

	if ( pChunk == NULL ) {
		return;
	}
	pRelease = pChunk->Release;
	pContext = pChunk->Context;
	pData = pChunk->Data;
	iSize = pChunk->Size;
	memset(pChunk, 0, sizeof(*pChunk));
	if ( pRelease != NULL ) {
		pPrevious = __xrtErrorSwapOwned(NULL);
		pRelease(pContext, pData, iSize);
		pDiscard = __xrtErrorSwapOwned(pPrevious);
		xrtErrorFree(pDiscard);
	}
}



/* 返回 Reader 已发布的正文总字节数。 */
XRT_API uint64 xrtHttpBodyReaderBytes(const xhttpbodyreader* pReader)
{
	if ( pReader == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pReader->Bytes;
}



/* 返回失败 Reader 借用的稳定错误。 */
XRT_API const xerror* xrtHttpBodyReaderError(
	const xhttpbodyreader* pReader
)
{
	return ( (pReader != NULL) && pReader->Failed ) ?
		pReader->Error : NULL;
}



/* 关闭 Reader 并释放正文引用。 */
XRT_API void xrtHttpBodyReaderDestroy(xhttpbodyreader* pReader)
{
	if ( pReader == NULL ) {
		return;
	}
	if ( (pReader->Body != NULL) &&
		(pReader->Body->Kind == XRT_HTTP_BODY_CUSTOM) &&
		(pReader->Ops.Close != NULL) ) {
		__xrtHttpBodyClosePreserveError(
			pReader->Ops.Close,
			pReader->Context
		);
	}
	xrtErrorFree(pReader->Error);
	xrtHttpBodyDestroy(pReader->Body);
	memset(pReader, 0, sizeof(*pReader));
	xrtFree(pReader);
}

#endif
