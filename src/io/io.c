#include "../internal/xrt_io.h"



#if defined(XRT_FEATURE_IO)

/* 默认复制缓冲保持较小栈占用，同时足以摊薄回调成本。 */
#define XRT_IO_COPY_BUFFER (16u * 1024u)



/* 内存 Reader 借用固定输入并维护独立游标。 */
typedef struct __xrt_memory_reader {
	cbytes Data;
	size_t Size;
	size_t Position;
} __xrt_memory_reader;



/* 内存 Writer 借用固定容量并区分游标与逻辑大小。 */
typedef struct __xrt_memory_writer {
	bytes Data;
	size_t Capacity;
	size_t Size;
	size_t Position;
} __xrt_memory_writer;



/* 丢弃 Writer 只保留已经消费的字节数。 */
typedef struct __xrt_discard_writer {
	uint64 Written;
} __xrt_discard_writer;



/* 复制模式分别表示直到 EOF、精确长度和硬上限。 */
typedef enum __xrt_copy_mode {
	__XRT_COPY_ALL = 0,
	__XRT_COPY_EXACT,
	__XRT_COPY_LIMIT
} __xrt_copy_mode;



/* 设置带稳定域、代码和操作名的 IO 错误。 */
void __xrtIoError(
	xerrkind Kind,
	xioerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.io";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 回调没有提供错误时补充 IO 层上下文。 */
static void __xrtIoFallback(
	const xerror* pPrevious,
	xioerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	if ( (xrtGetError() == NULL) || (xrtGetError() == pPrevious) ) {
		__xrtIoError(XERR_IO, Code, sOperation, sMessage);
	}
}



/* 在有限无符号位置范围内执行带符号相对移动。 */
bool __xrtIoMove(
	uint64 iBase,
	int64 iOffset,
	uint64 iLimit,
	uint64* pPosition
)
{
	uint64 iDistance;
	uint64 iResult;

	if ( iBase > iLimit ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( iOffset < 0 ) {
		iDistance = (uint64)(-(iOffset + 1)) + 1u;
		if ( iDistance > iBase ) {
			__xrtErrorSetRange();
			return false;
		}
		iResult = iBase - iDistance;
	} else {
		iDistance = (uint64)iOffset;
		if ( iDistance > (iLimit - iBase) ) {
			__xrtErrorSetRange();
			return false;
		}
		iResult = iBase + iDistance;
	}
	*pPosition = iResult;
	return true;
}



/* 验证可选 size_t 输出槽不会覆盖对象或数据范围。 */
static bool __xrtIoSizeOutput(
	const void* pObject,
	size_t iObjectSize,
	const void* pData,
	size_t iDataSize,
	size_t* pValue
)
{
	if ( pValue == NULL ) {
		return true;
	}
	if ( !__xrtRangeValid(pValue, sizeof(*pValue)) ||
		 __xrtRangesOverlap(pValue, sizeof(*pValue), pObject, iObjectSize) ||
		 __xrtRangesOverlap(pValue, sizeof(*pValue), pData, iDataSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证可选 uint64 输出槽不会覆盖一个或两个对象。 */
static bool __xrtIoUint64Output(
	const void* pFirst,
	size_t iFirstSize,
	const void* pSecond,
	size_t iSecondSize,
	uint64* pValue
)
{
	if ( pValue == NULL ) {
		return true;
	}
	if ( !__xrtRangeValid(pValue, sizeof(*pValue)) ||
		 __xrtRangesOverlap(pValue, sizeof(*pValue), pFirst, iFirstSize) ||
		 __xrtRangesOverlap(pValue, sizeof(*pValue), pSecond, iSecondSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 创建带有内联上下文的 Reader。 */
xreader* __xrtReaderCreateInline(
	const xreaderops* pOps,
	size_t iContextSize,
	ptr* ppContext
)
{
	xreader* pReader;

	if ( (pOps == NULL) || (pOps->Read == NULL) ||
		 !__xrtRangeValid(pOps, sizeof(*pOps)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iContextSize > (SIZE_MAX - sizeof(xreader)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pReader = (xreader*)xrtMalloc(sizeof(xreader) + iContextSize);
	if ( pReader == NULL ) {
		return NULL;
	}
	pReader->Ops = *pOps;
	pReader->Context = iContextSize != 0u ? (ptr)(pReader + 1) : NULL;
	pReader->AtEnd = false;
	if ( iContextSize != 0u ) {
		memset(pReader->Context, 0, iContextSize);
	}
	if ( ppContext != NULL ) {
		*ppContext = pReader->Context;
	}
	return pReader;
}



/* 创建带有内联上下文的 Writer。 */
xwriter* __xrtWriterCreateInline(
	const xwriterops* pOps,
	size_t iContextSize,
	ptr* ppContext
)
{
	xwriter* pWriter;

	if ( (pOps == NULL) || (pOps->Write == NULL) ||
		 !__xrtRangeValid(pOps, sizeof(*pOps)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iContextSize > (SIZE_MAX - sizeof(xwriter)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pWriter = (xwriter*)xrtMalloc(sizeof(xwriter) + iContextSize);
	if ( pWriter == NULL ) {
		return NULL;
	}
	pWriter->Ops = *pOps;
	pWriter->Context = iContextSize != 0u ? (ptr)(pWriter + 1) : NULL;
	if ( iContextSize != 0u ) {
		memset(pWriter->Context, 0, iContextSize);
	}
	if ( ppContext != NULL ) {
		*ppContext = pWriter->Context;
	}
	return pWriter;
}



/* 创建使用外部上下文的自定义 Reader。 */
XRT_API xreader* xrtReaderCreate(
	const xreaderops* pOps,
	ptr pContext
)
{
	xreader* pReader = __xrtReaderCreateInline(pOps, 0u, NULL);

	if ( pReader != NULL ) {
		pReader->Context = pContext;
	}
	return pReader;
}



/* 创建使用外部上下文的自定义 Writer。 */
XRT_API xwriter* xrtWriterCreate(
	const xwriterops* pOps,
	ptr pContext
)
{
	xwriter* pWriter = __xrtWriterCreateInline(pOps, 0u, NULL);

	if ( pWriter != NULL ) {
		pWriter->Context = pContext;
	}
	return pWriter;
}



/* 从固定内存读取一个允许短读的数据片段。 */
static bool __xrtMemoryRead(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	__xrt_memory_reader* pMemory = (__xrt_memory_reader*)pContext;
	size_t iRemain = pMemory->Size - pMemory->Position;
	size_t iDone = iRequest < iRemain ? iRequest : iRemain;

	if ( iDone != 0u ) {
		memmove(pBuffer, pMemory->Data + pMemory->Position, iDone);
		pMemory->Position += iDone;
	}
	*pRead = iDone;
	return true;
}



/* 在固定内存 Reader 范围内移动游标。 */
static bool __xrtMemoryReaderSeek(
	ptr pContext,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
)
{
	__xrt_memory_reader* pMemory = (__xrt_memory_reader*)pContext;
	uint64 iBase;
	uint64 iPosition;

	iBase = Origin == XSEEK_START ? 0u :
		(Origin == XSEEK_CURRENT ? (uint64)pMemory->Position :
		 (uint64)pMemory->Size);
	if ( !__xrtIoMove(iBase, iOffset, (uint64)pMemory->Size, &iPosition) ) {
		return false;
	}
	pMemory->Position = (size_t)iPosition;
	*pPosition = iPosition;
	return true;
}



/* 查询固定内存 Reader 游标。 */
static bool __xrtMemoryReaderTell(ptr pContext, uint64* pPosition)
{
	*pPosition = (uint64)((__xrt_memory_reader*)pContext)->Position;
	return true;
}



/* 查询固定内存 Reader 大小。 */
static bool __xrtMemoryReaderSize(ptr pContext, uint64* pSize)
{
	*pSize = (uint64)((__xrt_memory_reader*)pContext)->Size;
	return true;
}



/* 创建只借用输入视图的内存 Reader。 */
XRT_API xreader* xrtReaderFromMemory(xbytesview Data)
{
	xreaderops Ops;
	xreader* pReader;
	__xrt_memory_reader* pMemory;
	ptr pContext;

	if ( !__xrtRangeValid(Data.Data, Data.Size) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memset(&Ops, 0, sizeof(Ops));
	Ops.Read = __xrtMemoryRead;
	Ops.Seek = __xrtMemoryReaderSeek;
	Ops.Tell = __xrtMemoryReaderTell;
	Ops.Size = __xrtMemoryReaderSize;
	pReader = __xrtReaderCreateInline(
		&Ops,
		sizeof(__xrt_memory_reader),
		&pContext
	);
	if ( pReader == NULL ) {
		return NULL;
	}
	pMemory = (__xrt_memory_reader*)pContext;
	pMemory->Data = Data.Data;
	pMemory->Size = Data.Size;
	return pReader;
}



/* 向固定容量写入一个允许短写的数据片段。 */
static bool __xrtMemoryWrite(
	ptr pContext,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
)
{
	__xrt_memory_writer* pMemory = (__xrt_memory_writer*)pContext;
	size_t iRemain = pMemory->Capacity - pMemory->Position;
	size_t iDone = iRequest < iRemain ? iRequest : iRemain;
	size_t iStart = pMemory->Position;

	if ( iDone == 0u ) {
		__xrtErrorSetRange();
		return false;
	}
	memmove(pMemory->Data + iStart, pBuffer, iDone);
	if ( iStart > pMemory->Size ) {
		memset(pMemory->Data + pMemory->Size, 0, iStart - pMemory->Size);
	}
	pMemory->Position += iDone;
	if ( pMemory->Position > pMemory->Size ) {
		pMemory->Size = pMemory->Position;
	}
	*pWritten = iDone;
	return true;
}



/* 在固定内存 Writer 容量内移动游标。 */
static bool __xrtMemoryWriterSeek(
	ptr pContext,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
)
{
	__xrt_memory_writer* pMemory = (__xrt_memory_writer*)pContext;
	uint64 iBase;
	uint64 iPosition;

	iBase = Origin == XSEEK_START ? 0u :
		(Origin == XSEEK_CURRENT ? (uint64)pMemory->Position :
		 (uint64)pMemory->Size);
	if ( !__xrtIoMove(iBase, iOffset, (uint64)pMemory->Capacity, &iPosition) ) {
		return false;
	}
	pMemory->Position = (size_t)iPosition;
	*pPosition = iPosition;
	return true;
}



/* 查询固定内存 Writer 游标。 */
static bool __xrtMemoryWriterTell(ptr pContext, uint64* pPosition)
{
	*pPosition = (uint64)((__xrt_memory_writer*)pContext)->Position;
	return true;
}



/* 查询固定内存 Writer 逻辑大小。 */
static bool __xrtMemoryWriterSize(ptr pContext, uint64* pSize)
{
	*pSize = (uint64)((__xrt_memory_writer*)pContext)->Size;
	return true;
}



/* 创建只借用调用方容量的内存 Writer。 */
XRT_API xwriter* xrtWriterFromMemory(ptr pData, size_t iCapacity)
{
	xwriterops Ops;
	xwriter* pWriter;
	__xrt_memory_writer* pMemory;
	ptr pContext;

	if ( !__xrtRangeValid(pData, iCapacity) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memset(&Ops, 0, sizeof(Ops));
	Ops.Write = __xrtMemoryWrite;
	Ops.Seek = __xrtMemoryWriterSeek;
	Ops.Tell = __xrtMemoryWriterTell;
	Ops.Size = __xrtMemoryWriterSize;
	pWriter = __xrtWriterCreateInline(
		&Ops,
		sizeof(__xrt_memory_writer),
		&pContext
	);
	if ( pWriter == NULL ) {
		return NULL;
	}
	pMemory = (__xrt_memory_writer*)pContext;
	pMemory->Data = (bytes)pData;
	pMemory->Capacity = iCapacity;
	return pWriter;
}



/* 消费并统计丢弃 Writer 收到的字节。 */
static bool __xrtDiscardWrite(
	ptr pContext,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
)
{
	__xrt_discard_writer* pDiscard = (__xrt_discard_writer*)pContext;

	(void)pBuffer;
	if ( (uint64)iRequest > (UINT64_MAX - pDiscard->Written) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pDiscard->Written += (uint64)iRequest;
	*pWritten = iRequest;
	return true;
}



/* 查询丢弃 Writer 已经消费的字节数。 */
static bool __xrtDiscardSize(ptr pContext, uint64* pSize)
{
	*pSize = ((__xrt_discard_writer*)pContext)->Written;
	return true;
}



/* 创建不保存数据的统计 Writer。 */
XRT_API xwriter* xrtWriterDiscard(void)
{
	xwriterops Ops;
	xwriter* pWriter;
	ptr pContext;

	memset(&Ops, 0, sizeof(Ops));
	Ops.Write = __xrtDiscardWrite;
	Ops.Tell = __xrtDiscardSize;
	Ops.Size = __xrtDiscardSize;
	pWriter = __xrtWriterCreateInline(
		&Ops,
		sizeof(__xrt_discard_writer),
		&pContext
	);
	return pWriter;
}



/* 执行一次经过契约验证的同步读取。 */
XRT_API bool xrtReaderRead(
	xreader* pReader,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	size_t iDone = 0;
	const xerror* pPrevious;

	if ( (pReader == NULL) ||
		 !__xrtRangeValid(pReader, sizeof(*pReader)) ||
		 !__xrtRangeValid(pBuffer, iRequest) ||
		 !__xrtIoSizeOutput(
			pReader,
			sizeof(*pReader),
			pBuffer,
			iRequest,
			pRead
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pRead != NULL ) {
		*pRead = 0u;
	}
	if ( iRequest == 0u ) {
		return true;
	}
	if ( pReader->AtEnd ) {
		return true;
	}
	pPrevious = xrtGetError();
	if ( !pReader->Ops.Read(pReader->Context, pBuffer, iRequest, &iDone) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_READ,
			"read",
			"reader callback failed"
		);
		return false;
	}
	if ( iDone > iRequest ) {
		__xrtIoError(
			XERR_INTERNAL,
			XIO_ERROR_CALLBACK,
			"read",
			"reader callback returned more bytes than requested"
		);
		return false;
	}
	pReader->AtEnd = iDone == 0u;
	if ( pRead != NULL ) {
		*pRead = iDone;
	}
	return true;
}



/* 持续读取直到填满输出或报告提前 EOF。 */
XRT_API bool xrtReaderReadFull(
	xreader* pReader,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	size_t iTotal = 0;

	if ( (pReader == NULL) ||
		 !__xrtRangeValid(pBuffer, iRequest) ||
		 !__xrtIoSizeOutput(
			pReader,
			sizeof(*pReader),
			pBuffer,
			iRequest,
			pRead
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	while ( iTotal < iRequest ) {
		size_t iDone = 0;

		if ( !xrtReaderRead(
			pReader,
			(bytes)pBuffer + iTotal,
			iRequest - iTotal,
			&iDone
		) ) {
			if ( pRead != NULL ) {
				*pRead = iTotal;
			}
			return false;
		}
		if ( iDone == 0u ) {
			if ( pRead != NULL ) {
				*pRead = iTotal;
			}
			__xrtIoError(
				XERR_IO,
				XIO_ERROR_EOF,
				"read-full",
				"reader reached EOF before filling the output"
			);
			return false;
		}
		iTotal += iDone;
	}
	if ( pRead != NULL ) {
		*pRead = iTotal;
	}
	return true;
}



/* 执行 Copy、CopyN 和 CopyLimit 的共享固定缓冲循环。 */
static bool __xrtReaderCopyRun(
	xreader* pReader,
	xwriter* pWriter,
	uint64 iValue,
	__xrt_copy_mode Mode,
	uint64* pCopied
)
{
	uint8 arrBuffer[XRT_IO_COPY_BUFFER];
	uint64 iTotal = 0;
	bool bResult = false;

	if ( (pReader == NULL) || (pWriter == NULL) ||
		 !__xrtRangeValid(pReader, sizeof(*pReader)) ||
		 !__xrtRangeValid(pWriter, sizeof(*pWriter)) ||
		 !__xrtIoUint64Output(
			pReader,
			sizeof(*pReader),
			pWriter,
			sizeof(*pWriter),
			pCopied
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( ;; ) {
		size_t iRequest = sizeof(arrBuffer);
		size_t iRead = 0;
		size_t iWritten = 0;

		if ( Mode != __XRT_COPY_ALL ) {
			uint64 iRemain = iValue - iTotal;

			if ( iRemain == 0u ) {
				break;
			}
			if ( iRemain < (uint64)iRequest ) {
				iRequest = (size_t)iRemain;
			}
		}
		if ( !xrtReaderRead(pReader, arrBuffer, iRequest, &iRead) ) {
			goto done;
		}
		if ( iRead == 0u ) {
			if ( Mode == __XRT_COPY_EXACT ) {
				__xrtIoError(
					XERR_IO,
					XIO_ERROR_EOF,
					"copy-n",
					"reader reached EOF before the requested copy size"
				);
				goto done;
			}
			bResult = true;
			goto done;
		}
		if ( !xrtWriterWriteFull(pWriter, arrBuffer, iRead, &iWritten) ) {
			if ( (uint64)iWritten > (UINT64_MAX - iTotal) ) {
				iTotal = UINT64_MAX;
			} else {
				iTotal += (uint64)iWritten;
			}
			goto done;
		}
		if ( (uint64)iRead > (UINT64_MAX - iTotal) ) {
			__xrtErrorSetSizeOverflow();
			goto done;
		}
		iTotal += (uint64)iRead;
	}

	if ( Mode == __XRT_COPY_LIMIT ) {
		uint8 iProbe;
		size_t iRead = 0;

		if ( !xrtReaderRead(pReader, &iProbe, 1u, &iRead) ) {
			goto done;
		}
		if ( iRead != 0u ) {
			__xrtIoError(
				XERR_RANGE,
				XIO_ERROR_LIMIT,
				"copy-limit",
				"reader exceeds the configured copy limit"
			);
			goto done;
		}
	}
	bResult = true;

done:
	if ( pCopied != NULL ) {
		*pCopied = iTotal;
	}
	return bResult;
}



/* 持续复制到输入 EOF。 */
XRT_API bool xrtReaderCopy(
	xreader* pReader,
	xwriter* pWriter,
	uint64* pCopied
)
{
	return __xrtReaderCopyRun(
		pReader,
		pWriter,
		0u,
		__XRT_COPY_ALL,
		pCopied
	);
}



/* 精确复制指定字节数。 */
XRT_API bool xrtReaderCopyN(
	xreader* pReader,
	xwriter* pWriter,
	uint64 iSize,
	uint64* pCopied
)
{
	return __xrtReaderCopyRun(
		pReader,
		pWriter,
		iSize,
		__XRT_COPY_EXACT,
		pCopied
	);
}



/* 在硬上限内复制到 EOF。 */
XRT_API bool xrtReaderCopyLimit(
	xreader* pReader,
	xwriter* pWriter,
	uint64 iLimit,
	uint64* pCopied
)
{
	return __xrtReaderCopyRun(
		pReader,
		pWriter,
		iLimit,
		__XRT_COPY_LIMIT,
		pCopied
	);
}



/* 移动 Reader 游标并在成功后解除 EOF 锁定。 */
XRT_API bool xrtReaderSeek(
	xreader* pReader,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
)
{
	uint64 iResult;
	const xerror* pPrevious;

	if ( (pReader == NULL) ||
		 ((Origin != XSEEK_START) &&
		  (Origin != XSEEK_CURRENT) &&
		  (Origin != XSEEK_END)) ||
		 !__xrtIoUint64Output(
			pReader,
			sizeof(*pReader),
			NULL,
			0u,
			pPosition
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pReader->Ops.Seek == NULL ) {
		__xrtIoError(
			XERR_UNSUPPORTED,
			XIO_ERROR_SEEK,
			"seek",
			"reader does not support seeking"
		);
		return false;
	}
	pPrevious = xrtGetError();
	if ( !pReader->Ops.Seek(
		pReader->Context,
		iOffset,
		Origin,
		&iResult
	) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_SEEK,
			"seek",
			"reader seek failed"
		);
		return false;
	}
	pReader->AtEnd = false;
	if ( pPosition != NULL ) {
		*pPosition = iResult;
	}
	return true;
}



/* 查询 Reader 当前游标。 */
XRT_API bool xrtReaderTell(xreader* pReader, uint64* pPosition)
{
	uint64 iResult;
	const xerror* pPrevious;

	if ( (pReader == NULL) || (pPosition == NULL) ||
		 !__xrtIoUint64Output(
			pReader,
			sizeof(*pReader),
			NULL,
			0u,
			pPosition
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pReader->Ops.Tell == NULL ) {
		__xrtIoError(
			XERR_UNSUPPORTED,
			XIO_ERROR_TELL,
			"tell",
			"reader does not support position queries"
		);
		return false;
	}
	pPrevious = xrtGetError();
	if ( !pReader->Ops.Tell(pReader->Context, &iResult) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_TELL,
			"tell",
			"reader tell failed"
		);
		return false;
	}
	*pPosition = iResult;
	return true;
}



/* 查询 Reader 当前总大小。 */
XRT_API bool xrtReaderSize(xreader* pReader, uint64* pSize)
{
	uint64 iResult;
	const xerror* pPrevious;

	if ( (pReader == NULL) || (pSize == NULL) ||
		 !__xrtIoUint64Output(
			pReader,
			sizeof(*pReader),
			NULL,
			0u,
			pSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pReader->Ops.Size == NULL ) {
		__xrtIoError(
			XERR_UNSUPPORTED,
			XIO_ERROR_SIZE,
			"size",
			"reader does not support size queries"
		);
		return false;
	}
	pPrevious = xrtGetError();
	if ( !pReader->Ops.Size(pReader->Context, &iResult) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_SIZE,
			"size",
			"reader size query failed"
		);
		return false;
	}
	*pSize = iResult;
	return true;
}



/* 判断 Reader 是否提供定位能力。 */
XRT_API bool xrtReaderCanSeek(const xreader* pReader)
{
	return (pReader != NULL) && (pReader->Ops.Seek != NULL);
}



/* 判断 Reader 是否提供大小查询能力。 */
XRT_API bool xrtReaderCanSize(const xreader* pReader)
{
	return (pReader != NULL) && (pReader->Ops.Size != NULL);
}



/* 判断 Reader 是否已经观察到 EOF。 */
XRT_API bool xrtReaderEOF(const xreader* pReader)
{
	return (pReader != NULL) && pReader->AtEnd;
}



/* 关闭并释放 Reader，关闭失败也不泄漏对象。 */
XRT_API bool xrtReaderDestroy(xreader* pReader)
{
	bool bResult = true;
	const xerror* pPrevious;

	if ( pReader == NULL ) {
		return true;
	}
	pPrevious = xrtGetError();
	if ( (pReader->Ops.Close != NULL) &&
		 !pReader->Ops.Close(pReader->Context) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_CLOSE,
			"close",
			"reader close failed"
		);
		bResult = false;
	}
	xrtFree(pReader);
	return bResult;
}



/* 执行一次经过契约验证的同步写入。 */
XRT_API bool xrtWriterWrite(
	xwriter* pWriter,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
)
{
	size_t iDone = 0;
	const xerror* pPrevious;

	if ( (pWriter == NULL) ||
		 !__xrtRangeValid(pWriter, sizeof(*pWriter)) ||
		 !__xrtRangeValid(pBuffer, iRequest) ||
		 !__xrtIoSizeOutput(
			pWriter,
			sizeof(*pWriter),
			pBuffer,
			iRequest,
			pWritten
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pWritten != NULL ) {
		*pWritten = 0u;
	}
	if ( iRequest == 0u ) {
		return true;
	}
	pPrevious = xrtGetError();
	if ( !pWriter->Ops.Write(
		pWriter->Context,
		pBuffer,
		iRequest,
		&iDone
	) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_WRITE,
			"write",
			"writer callback failed"
		);
		return false;
	}
	if ( iDone > iRequest ) {
		__xrtIoError(
			XERR_INTERNAL,
			XIO_ERROR_CALLBACK,
			"write",
			"writer callback consumed more bytes than requested"
		);
		return false;
	}
	if ( iDone == 0u ) {
		__xrtIoError(
			XERR_IO,
			XIO_ERROR_NO_PROGRESS,
			"write",
			"writer callback made no progress"
		);
		return false;
	}
	if ( pWritten != NULL ) {
		*pWritten = iDone;
	}
	return true;
}



/* 持续写入直到消费全部输入。 */
XRT_API bool xrtWriterWriteFull(
	xwriter* pWriter,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
)
{
	size_t iTotal = 0;

	if ( (pWriter == NULL) ||
		 !__xrtRangeValid(pBuffer, iRequest) ||
		 !__xrtIoSizeOutput(
			pWriter,
			sizeof(*pWriter),
			pBuffer,
			iRequest,
			pWritten
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	while ( iTotal < iRequest ) {
		size_t iDone = 0;

		if ( !xrtWriterWrite(
			pWriter,
			(cbytes)pBuffer + iTotal,
			iRequest - iTotal,
			&iDone
		) ) {
			if ( pWritten != NULL ) {
				*pWritten = iTotal;
			}
			return false;
		}
		iTotal += iDone;
	}
	if ( pWritten != NULL ) {
		*pWritten = iTotal;
	}
	return true;
}



/* 移动 Writer 游标。 */
XRT_API bool xrtWriterSeek(
	xwriter* pWriter,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
)
{
	uint64 iResult;
	const xerror* pPrevious;

	if ( (pWriter == NULL) ||
		 ((Origin != XSEEK_START) &&
		  (Origin != XSEEK_CURRENT) &&
		  (Origin != XSEEK_END)) ||
		 !__xrtIoUint64Output(
			pWriter,
			sizeof(*pWriter),
			NULL,
			0u,
			pPosition
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pWriter->Ops.Seek == NULL ) {
		__xrtIoError(
			XERR_UNSUPPORTED,
			XIO_ERROR_SEEK,
			"seek",
			"writer does not support seeking"
		);
		return false;
	}
	pPrevious = xrtGetError();
	if ( !pWriter->Ops.Seek(
		pWriter->Context,
		iOffset,
		Origin,
		&iResult
	) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_SEEK,
			"seek",
			"writer seek failed"
		);
		return false;
	}
	if ( pPosition != NULL ) {
		*pPosition = iResult;
	}
	return true;
}



/* 查询 Writer 当前游标。 */
XRT_API bool xrtWriterTell(xwriter* pWriter, uint64* pPosition)
{
	uint64 iResult;
	const xerror* pPrevious;

	if ( (pWriter == NULL) || (pPosition == NULL) ||
		 !__xrtIoUint64Output(
			pWriter,
			sizeof(*pWriter),
			NULL,
			0u,
			pPosition
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pWriter->Ops.Tell == NULL ) {
		__xrtIoError(
			XERR_UNSUPPORTED,
			XIO_ERROR_TELL,
			"tell",
			"writer does not support position queries"
		);
		return false;
	}
	pPrevious = xrtGetError();
	if ( !pWriter->Ops.Tell(pWriter->Context, &iResult) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_TELL,
			"tell",
			"writer tell failed"
		);
		return false;
	}
	*pPosition = iResult;
	return true;
}



/* 查询 Writer 当前逻辑大小。 */
XRT_API bool xrtWriterSize(xwriter* pWriter, uint64* pSize)
{
	uint64 iResult;
	const xerror* pPrevious;

	if ( (pWriter == NULL) || (pSize == NULL) ||
		 !__xrtIoUint64Output(
			pWriter,
			sizeof(*pWriter),
			NULL,
			0u,
			pSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pWriter->Ops.Size == NULL ) {
		__xrtIoError(
			XERR_UNSUPPORTED,
			XIO_ERROR_SIZE,
			"size",
			"writer does not support size queries"
		);
		return false;
	}
	pPrevious = xrtGetError();
	if ( !pWriter->Ops.Size(pWriter->Context, &iResult) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_SIZE,
			"size",
			"writer size query failed"
		);
		return false;
	}
	*pSize = iResult;
	return true;
}



/* 判断 Writer 是否提供定位能力。 */
XRT_API bool xrtWriterCanSeek(const xwriter* pWriter)
{
	return (pWriter != NULL) && (pWriter->Ops.Seek != NULL);
}



/* 判断 Writer 是否提供大小查询能力。 */
XRT_API bool xrtWriterCanSize(const xwriter* pWriter)
{
	return (pWriter != NULL) && (pWriter->Ops.Size != NULL);
}



/* 显式刷新 Writer，没有回调时为空操作。 */
XRT_API bool xrtWriterFlush(xwriter* pWriter)
{
	const xerror* pPrevious;

	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pWriter->Ops.Flush == NULL ) {
		return true;
	}
	pPrevious = xrtGetError();
	if ( !pWriter->Ops.Flush(pWriter->Context) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_FLUSH,
			"flush",
			"writer flush failed"
		);
		return false;
	}
	return true;
}



/* 关闭并释放 Writer，不把销毁隐式升级为持久化刷新。 */
XRT_API bool xrtWriterDestroy(xwriter* pWriter)
{
	bool bResult = true;
	const xerror* pPrevious;

	if ( pWriter == NULL ) {
		return true;
	}
	pPrevious = xrtGetError();
	if ( (pWriter->Ops.Close != NULL) &&
		 !pWriter->Ops.Close(pWriter->Context) ) {
		__xrtIoFallback(
			pPrevious,
			XIO_ERROR_CLOSE,
			"close",
			"writer close failed"
		);
		bResult = false;
	}
	xrtFree(pWriter);
	return bResult;
}

#endif
