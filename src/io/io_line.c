#include "../internal/xrt_io.h"

#include <xrt/buffer.h>



#if defined(XRT_FEATURE_IO_LINE)

/* 每次按需扩展读取窗口，不在对象中保留固定大小数组。 */
#define XRT_IO_LINE_READ_CHUNK (4u * 1024u)



/* Line Reader 保存尚未发布的字节和底层 Reader 所有权。 */
struct xlinereader {
	xreader* Reader;
	xbuffer Buffer;
	size_t Offset;
	size_t MaxLine;
	bool Own;
	bool SourceEnd;
	bool Failed;
};



/* 验证对象、输出槽和内部借用区域不会互相覆盖。 */
static bool __xrtLineReaderOutputValid(
	const xlinereader* pLines,
	const xlineview* pLine
)
{
	if ( (pLines == NULL) || (pLine == NULL) ||
		 !__xrtRangeValid(pLines, sizeof(*pLines)) ||
		 !__xrtRangeValid(pLine, sizeof(*pLine)) ||
		 __xrtRangesOverlap(
			pLine,
			sizeof(*pLine),
			pLines,
			sizeof(*pLines)
		 ) ||
		 __xrtRangesOverlap(
			pLine,
			sizeof(*pLine),
			pLines->Buffer.Data,
			pLines->Buffer.Capacity
		 ) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 创建借用或接管底层 Reader 的共享实现。 */
static xlinereader* __xrtLineReaderCreate(
	xreader* pReader,
	size_t iMaxLine,
	bool bOwn
)
{
	xlinereader* pLines;

	if ( (pReader == NULL) ||
		 !__xrtRangeValid(pReader, sizeof(*pReader)) ||
		 (iMaxLine == 0u) || (iMaxLine > (SIZE_MAX - 2u)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pLines = (xlinereader*)xrtMalloc(sizeof(xlinereader));
	if ( pLines == NULL ) {
		return NULL;
	}
	memset(pLines, 0, sizeof(*pLines));
	if ( !xrtBufferInit(&pLines->Buffer) ) {
		xrtFree(pLines);
		return NULL;
	}
	pLines->Reader = pReader;
	pLines->MaxLine = iMaxLine;
	pLines->Own = bOwn;
	return pLines;
}



/* 下一次迭代开始时丢弃上一条借用视图之前的字节。 */
static void __xrtLineReaderCompact(xlinereader* pLines)
{
	size_t iRemain;

	if ( pLines->Offset == 0u ) {
		return;
	}
	if ( pLines->Offset >= pLines->Buffer.Size ) {
		xrtBufferClear(&pLines->Buffer);
		pLines->Offset = 0u;
		return;
	}
	iRemain = pLines->Buffer.Size - pLines->Offset;
	memmove(
		pLines->Buffer.Data,
		pLines->Buffer.Data + pLines->Offset,
		iRemain
	);
	pLines->Buffer.Size = iRemain;
	pLines->Offset = 0u;
}



/* 设置超限错误并把不可恢复的流式迭代标记为失败。 */
static xlinenext __xrtLineReaderLimit(xlinereader* pLines)
{
	pLines->Failed = true;
	__xrtIoError(
		XERR_RANGE,
		XIO_ERROR_LIMIT,
		"read-line",
		"line content exceeds the configured byte limit"
	);
	return XLINE_NEXT_ERROR;
}



/* 发布以 LF 结束的行并按需剥离前置 CR。 */
static xlinenext __xrtLineReaderPublishLf(
	xlinereader* pLines,
	size_t iLf,
	xlineview* pLine
)
{
	size_t iSize = iLf;
	xlineend End = XLINE_END_LF;

	if ( (iSize != 0u) && (pLines->Buffer.Data[iSize - 1u] == '\r') ) {
		iSize--;
		End = XLINE_END_CRLF;
	}
	if ( iSize > pLines->MaxLine ) {
		return __xrtLineReaderLimit(pLines);
	}
	pLine->Text.Data = (cstr)pLines->Buffer.Data;
	pLine->Text.Size = iSize;
	pLine->End = End;
	pLines->Offset = iLf + 1u;
	return XLINE_NEXT_LINE;
}



/* 创建借用 Reader 的 Line Reader。 */
XRT_API xlinereader* xrtLineReaderCreate(
	xreader* pReader,
	size_t iMaxLine
)
{
	return __xrtLineReaderCreate(pReader, iMaxLine, false);
}



/* 原子接管 Reader 槽并创建 Line Reader。 */
XRT_API xlinereader* xrtLineReaderTake(
	xreader** ppReader,
	size_t iMaxLine
)
{
	xreader* pReader;
	xlinereader* pLines;

	if ( (ppReader == NULL) ||
		 !__xrtRangeValid(ppReader, sizeof(*ppReader)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pReader = *ppReader;
	if ( (pReader == NULL) ||
		 !__xrtRangeValid(pReader, sizeof(*pReader)) ||
		 __xrtRangesOverlap(
			ppReader,
			sizeof(*ppReader),
			pReader,
			sizeof(*pReader)
		 ) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pLines = __xrtLineReaderCreate(pReader, iMaxLine, true);
	if ( pLines != NULL ) {
		*ppReader = NULL;
	}
	return pLines;
}



/* 按 LF、CRLF 或输入结束边界返回下一条借用行。 */
XRT_API xlinenext xrtLineReaderNext(
	xlinereader* pLines,
	xlineview* pLine
)
{
	const void* pFound;
	size_t iCapacity;
	size_t iRead;
	size_t iRequest;

	if ( !__xrtLineReaderOutputValid(pLines, pLine) ) {
		return XLINE_NEXT_ERROR;
	}
	if ( pLines->Failed ) {
		__xrtErrorSetInvalidState();
		return XLINE_NEXT_ERROR;
	}
	__xrtLineReaderCompact(pLines);

	for ( ;; ) {
		/* 优先从已经缓冲的字节发布完整行。 */
		pFound = pLines->Buffer.Size != 0u ?
			memchr(pLines->Buffer.Data, '\n', pLines->Buffer.Size) : NULL;
		if ( pFound != NULL ) {
			return __xrtLineReaderPublishLf(
				pLines,
				(size_t)((cbytes)pFound - pLines->Buffer.Data),
				pLine
			);
		}

		/* 输入结束时，剩余字节构成唯一一条无终止符末行。 */
		if ( pLines->SourceEnd ) {
			if ( pLines->Buffer.Size == 0u ) {
				return XLINE_NEXT_END;
			}
			if ( pLines->Buffer.Size > pLines->MaxLine ) {
				return __xrtLineReaderLimit(pLines);
			}
			pLine->Text.Data = (cstr)pLines->Buffer.Data;
			pLine->Text.Size = pLines->Buffer.Size;
			pLine->End = XLINE_END_NONE;
			pLines->Offset = pLines->Buffer.Size;
			return XLINE_NEXT_LINE;
		}

		/* 只有内容上限后的单个 CR 仍可能与下一字节组成合法 CRLF。 */
		if ( (pLines->Buffer.Size > pLines->MaxLine) &&
			 ((pLines->Buffer.Size != (pLines->MaxLine + 1u)) ||
			  (pLines->Buffer.Data[pLines->Buffer.Size - 1u] != '\r')) ) {
			return __xrtLineReaderLimit(pLines);
		}

		/* 扩容成功后再消费底层输入，避免 OOM 丢失尚未缓冲的字节。 */
		iCapacity = pLines->MaxLine + 2u;
		iRequest = iCapacity - pLines->Buffer.Size;
		if ( iRequest > XRT_IO_LINE_READ_CHUNK ) {
			iRequest = XRT_IO_LINE_READ_CHUNK;
		}
		if ( iRequest == 0u ) {
			return __xrtLineReaderLimit(pLines);
		}
		if ( !xrtBufferReserve(
			&pLines->Buffer,
			pLines->Buffer.Size + iRequest
		) ) {
			pLines->Failed = true;
			return XLINE_NEXT_ERROR;
		}
		iRead = 0u;
		if ( !xrtReaderRead(
			pLines->Reader,
			pLines->Buffer.Data + pLines->Buffer.Size,
			iRequest,
			&iRead
		) ) {
			pLines->Failed = true;
			return XLINE_NEXT_ERROR;
		}
		if ( iRead == 0u ) {
			pLines->SourceEnd = true;
		} else {
			pLines->Buffer.Size += iRead;
		}
	}
}



/* 释放动态缓冲，并按所有权契约销毁底层 Reader。 */
XRT_API bool xrtLineReaderDestroy(xlinereader* pLines)
{
	bool bResult = true;
	xreader* pReader;

	if ( pLines == NULL ) {
		return true;
	}
	pReader = pLines->Reader;
	xrtBufferUnit(&pLines->Buffer);
	if ( pLines->Own ) {
		bResult = xrtReaderDestroy(pReader);
	}
	pLines->Reader = NULL;
	xrtFree(pLines);
	return bResult;
}

#endif
