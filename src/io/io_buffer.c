#include "../internal/xrt_io.h"



#if defined(XRT_FEATURE_IO_BUFFER)

/* ReadAll 每轮直接扩展结果 Buffer，避免额外中间堆缓冲。 */
#define XRT_IO_READ_ALL_CHUNK (64u * 1024u)



/* Buffer Reader 保持借用或接管的 Buffer 及独立游标。 */
typedef struct __xrt_buffer_reader {
	const xbuffer* Buffer;
	size_t Position;
	bool Own;
} __xrt_buffer_reader;



/* Buffer Writer 借用可变 Buffer 并维护独立游标。 */
typedef struct __xrt_buffer_writer {
	xbuffer* Buffer;
	size_t Position;
} __xrt_buffer_writer;



/* 验证公开 Buffer 布局和完整分配范围。 */
static bool __xrtIoBufferValid(const xbuffer* pBuffer)
{
	if ( (pBuffer == NULL) ||
		 (pBuffer->Size > pBuffer->Capacity) ||
		 !__xrtRangeValid(pBuffer->Data, pBuffer->Capacity) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 从 Buffer 当前有效区域读取一个数据片段。 */
static bool __xrtBufferRead(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	__xrt_buffer_reader* pReader = (__xrt_buffer_reader*)pContext;
	size_t iRemain;
	size_t iDone;

	if ( !__xrtIoBufferValid(pReader->Buffer) ) {
		return false;
	}
	if ( pReader->Position > pReader->Buffer->Size ) {
		__xrtIoError(
			XERR_STATE,
			XIO_ERROR_READ,
			"read-buffer",
			"borrowed buffer changed while a reader was active"
		);
		return false;
	}
	iRemain = pReader->Buffer->Size - pReader->Position;
	iDone = iRequest < iRemain ? iRequest : iRemain;
	if ( iDone != 0u ) {
		memmove(
			pBuffer,
			pReader->Buffer->Data + pReader->Position,
			iDone
		);
		pReader->Position += iDone;
	}
	*pRead = iDone;
	return true;
}



/* 在 Buffer 当前有效大小内移动 Reader 游标。 */
static bool __xrtBufferReaderSeek(
	ptr pContext,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
)
{
	__xrt_buffer_reader* pReader = (__xrt_buffer_reader*)pContext;
	uint64 iBase;
	uint64 iPosition;

	if ( !__xrtIoBufferValid(pReader->Buffer) ||
		 (pReader->Position > pReader->Buffer->Size) ) {
		return false;
	}
	iBase = Origin == XSEEK_START ? 0u :
		(Origin == XSEEK_CURRENT ? (uint64)pReader->Position :
		 (uint64)pReader->Buffer->Size);
	if ( !__xrtIoMove(
		iBase,
		iOffset,
		(uint64)pReader->Buffer->Size,
		&iPosition
	) ) {
		return false;
	}
	pReader->Position = (size_t)iPosition;
	*pPosition = iPosition;
	return true;
}



/* 查询 Buffer Reader 游标。 */
static bool __xrtBufferReaderTell(ptr pContext, uint64* pPosition)
{
	*pPosition = (uint64)((__xrt_buffer_reader*)pContext)->Position;
	return true;
}



/* 查询 Buffer Reader 当前大小。 */
static bool __xrtBufferReaderSize(ptr pContext, uint64* pSize)
{
	__xrt_buffer_reader* pReader = (__xrt_buffer_reader*)pContext;

	if ( !__xrtIoBufferValid(pReader->Buffer) ) {
		return false;
	}
	*pSize = (uint64)pReader->Buffer->Size;
	return true;
}



/* 释放接管的 Buffer，借用模式没有资源动作。 */
static bool __xrtBufferReaderClose(ptr pContext)
{
	__xrt_buffer_reader* pReader = (__xrt_buffer_reader*)pContext;

	if ( pReader->Own ) {
		xrtBufferDestroy((xbuffer*)pReader->Buffer);
		pReader->Buffer = NULL;
	}
	return true;
}



/* 创建借用或接管 Buffer 的共享 Reader 实现。 */
static xreader* __xrtReaderBufferCreate(
	const xbuffer* pBuffer,
	bool bOwn
)
{
	xreaderops Ops;
	xreader* pResult;
	__xrt_buffer_reader* pReader;
	ptr pContext;

	if ( !__xrtIoBufferValid(pBuffer) ) {
		return NULL;
	}
	memset(&Ops, 0, sizeof(Ops));
	Ops.Read = __xrtBufferRead;
	Ops.Seek = __xrtBufferReaderSeek;
	Ops.Tell = __xrtBufferReaderTell;
	Ops.Size = __xrtBufferReaderSize;
	Ops.Close = __xrtBufferReaderClose;
	pResult = __xrtReaderCreateInline(
		&Ops,
		sizeof(__xrt_buffer_reader),
		&pContext
	);
	if ( pResult == NULL ) {
		return NULL;
	}
	pReader = (__xrt_buffer_reader*)pContext;
	pReader->Buffer = pBuffer;
	pReader->Own = bOwn;
	return pResult;
}



/* 创建借用 Buffer 的 Reader。 */
XRT_API xreader* xrtReaderFromBuffer(const xbuffer* pBuffer)
{
	return __xrtReaderBufferCreate(pBuffer, false);
}



/* 原子接管 Buffer 并创建 Reader。 */
XRT_API xreader* xrtReaderTakeBuffer(xbuffer** ppBuffer)
{
	xbuffer* pBuffer;
	xreader* pReader;

	if ( (ppBuffer == NULL) ||
		 !__xrtRangeValid(ppBuffer, sizeof(*ppBuffer)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pBuffer = *ppBuffer;
	if ( !__xrtIoBufferValid(pBuffer) ||
		 __xrtRangesOverlap(
			ppBuffer,
			sizeof(*ppBuffer),
			pBuffer,
			sizeof(*pBuffer)
		) ||
		 __xrtRangesOverlap(
			ppBuffer,
			sizeof(*ppBuffer),
			pBuffer->Data,
			pBuffer->Capacity
		) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pReader = __xrtReaderBufferCreate(pBuffer, true);
	if ( pReader != NULL ) {
		*ppBuffer = NULL;
	}
	return pReader;
}



/* 覆盖或稀疏扩展 Buffer，并在成功后推进 Writer 游标。 */
static bool __xrtBufferWrite(
	ptr pContext,
	const void* pData,
	size_t iRequest,
	size_t* pWritten
)
{
	__xrt_buffer_writer* pWriter = (__xrt_buffer_writer*)pContext;

	if ( !__xrtIoBufferValid(pWriter->Buffer) ) {
		return false;
	}
	if ( iRequest > (SIZE_MAX - pWriter->Position) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( !xrtBufferWrite(
		pWriter->Buffer,
		pWriter->Position,
		(xbytesview){ (cbytes)pData, iRequest }
	) ) {
		return false;
	}
	pWriter->Position += iRequest;
	*pWritten = iRequest;
	return true;
}



/* 在 size_t 地址空间内移动 Buffer Writer 游标。 */
static bool __xrtBufferWriterSeek(
	ptr pContext,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
)
{
	__xrt_buffer_writer* pWriter = (__xrt_buffer_writer*)pContext;
	uint64 iBase;
	uint64 iPosition;

	if ( !__xrtIoBufferValid(pWriter->Buffer) ) {
		return false;
	}
	iBase = Origin == XSEEK_START ? 0u :
		(Origin == XSEEK_CURRENT ? (uint64)pWriter->Position :
		 (uint64)pWriter->Buffer->Size);
	if ( !__xrtIoMove(iBase, iOffset, (uint64)SIZE_MAX, &iPosition) ) {
		return false;
	}
	pWriter->Position = (size_t)iPosition;
	*pPosition = iPosition;
	return true;
}



/* 查询 Buffer Writer 游标。 */
static bool __xrtBufferWriterTell(ptr pContext, uint64* pPosition)
{
	*pPosition = (uint64)((__xrt_buffer_writer*)pContext)->Position;
	return true;
}



/* 查询 Buffer Writer 当前逻辑大小。 */
static bool __xrtBufferWriterSize(ptr pContext, uint64* pSize)
{
	__xrt_buffer_writer* pWriter = (__xrt_buffer_writer*)pContext;

	if ( !__xrtIoBufferValid(pWriter->Buffer) ) {
		return false;
	}
	*pSize = (uint64)pWriter->Buffer->Size;
	return true;
}



/* 创建从 Buffer 末尾开始的借用 Writer。 */
XRT_API xwriter* xrtWriterFromBuffer(xbuffer* pBuffer)
{
	xwriterops Ops;
	xwriter* pResult;
	__xrt_buffer_writer* pWriter;
	ptr pContext;

	if ( !__xrtIoBufferValid(pBuffer) ) {
		return NULL;
	}
	memset(&Ops, 0, sizeof(Ops));
	Ops.Write = __xrtBufferWrite;
	Ops.Seek = __xrtBufferWriterSeek;
	Ops.Tell = __xrtBufferWriterTell;
	Ops.Size = __xrtBufferWriterSize;
	pResult = __xrtWriterCreateInline(
		&Ops,
		sizeof(__xrt_buffer_writer),
		&pContext
	);
	if ( pResult == NULL ) {
		return NULL;
	}
	pWriter = (__xrt_buffer_writer*)pContext;
	pWriter->Buffer = pBuffer;
	pWriter->Position = pBuffer->Size;
	return pResult;
}



/* 在硬上限内直接把 Reader 累积到新 Buffer。 */
XRT_API xbuffer* xrtReaderReadAll(xreader* pReader, size_t iLimit)
{
	xbuffer* pResult;

	if ( pReader == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pResult = xrtBufferCreate();
	if ( pResult == NULL ) {
		return NULL;
	}
	while ( pResult->Size < iLimit ) {
		size_t iRemain = iLimit - pResult->Size;
		size_t iRequest = iRemain < XRT_IO_READ_ALL_CHUNK
			? iRemain
			: XRT_IO_READ_ALL_CHUNK;
		size_t iStart = pResult->Size;
		size_t iRead = 0;
		bytes pOutput = xrtBufferAdd(pResult, iRequest);

		if ( pOutput == NULL ) {
			xrtBufferDestroy(pResult);
			return NULL;
		}
		if ( !xrtReaderRead(pReader, pOutput, iRequest, &iRead) ) {
			pResult->Size = iStart;
			xrtBufferDestroy(pResult);
			return NULL;
		}
		pResult->Size = iStart + iRead;
		if ( iRead == 0u ) {
			return pResult;
		}
	}

	{
		uint8 iProbe;
		size_t iRead = 0;

		if ( !xrtReaderRead(pReader, &iProbe, 1u, &iRead) ) {
			xrtBufferDestroy(pResult);
			return NULL;
		}
		if ( iRead == 0u ) {
			return pResult;
		}
	}
	__xrtIoError(
		XERR_RANGE,
		XIO_ERROR_LIMIT,
		"read-all",
		"reader exceeds the configured read-all limit"
	);
	xrtBufferDestroy(pResult);
	return NULL;
}



/* 完整写入 Buffer 当前有效内容。 */
XRT_API bool xrtWriterWriteBuffer(
	xwriter* pWriter,
	const xbuffer* pBuffer
)
{
	if ( !__xrtIoBufferValid(pBuffer) ) {
		return false;
	}
	return xrtWriterWriteFull(
		pWriter,
		pBuffer->Data,
		pBuffer->Size,
		NULL
	);
}

#endif
