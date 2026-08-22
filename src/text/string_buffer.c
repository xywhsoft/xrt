#include "../internal/xrt_string.h"



#if defined(XRT_FEATURE_STRING)

/* 检查字符串构建器公开状态是否自洽。 */
XRT_API bool xrtStrBufValid(const xstrbuf* pBuffer)
{
	if ( pBuffer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pBuffer->Data == NULL) && ((pBuffer->Size != 0) || (pBuffer->Capacity != 0))) ||
		 (pBuffer->Size > pBuffer->Capacity) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 检查追加视图是否来自构建器当前有效内容，并返回原偏移。 */
XRT_API bool xrtStrBufAlias(
	const xstrbuf* pBuffer,
	xstrview Text,
	bool* pAlias,
	size_t* pOffset
)
{
	uintptr_t iBuffer;
	uintptr_t iText;
	size_t iOffset;

	if ( (pAlias == NULL) || (pOffset == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pAlias = false;
	*pOffset = 0;
	if ( !xrtStrBufValid(pBuffer) || !__xrtStrViewValid(Text) ) {
		return false;
	}
	if ( (pBuffer->Data == NULL) || (Text.Data == NULL) ) {
		return true;
	}
	iBuffer = (uintptr_t)pBuffer->Data;
	iText = (uintptr_t)Text.Data;
	if ( iText < iBuffer ) {
		return true;
	}
	iOffset = (size_t)(iText - iBuffer);
	if ( iOffset > pBuffer->Capacity ) {
		return true;
	}
	if ( (iOffset > pBuffer->Size) || (Text.Size > (pBuffer->Size - iOffset)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pAlias = true;
	*pOffset = iOffset;
	return true;
}



/* 计算满足需求的几何增长容量。 */
static bool __xrtStrBufGrowth(size_t iCurrent, size_t iNeed, size_t* pCapacity)
{
	size_t iCapacity = iCurrent >= 64u ? iCurrent : 64u;

	if ( iNeed == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	while ( iCapacity < iNeed ) {
		size_t iGrowth = (iCapacity / 2u) + 16u;

		if ( iGrowth > ((SIZE_MAX - 1u) - iCapacity) ) {
			iCapacity = iNeed;
			break;
		}
		iCapacity += iGrowth;
	}
	*pCapacity = iCapacity;
	return true;
}



/* 初始化空字符串构建器。 */
XRT_API void xrtStrBufInit(xstrbuf* pBuffer)
{
	if ( pBuffer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pBuffer, 0, sizeof(xstrbuf));
}



/* 释放字符串构建器持有的内存。 */
XRT_API void xrtStrBufFree(xstrbuf* pBuffer)
{
	if ( pBuffer == NULL ) {
		return;
	}
	xrtFree(pBuffer->Data);
	memset(pBuffer, 0, sizeof(xstrbuf));
}



/* 清空字符串构建器但保留容量。 */
XRT_API void xrtStrBufClear(xstrbuf* pBuffer)
{
	if ( !xrtStrBufValid(pBuffer) ) {
		return;
	}
	pBuffer->Size = 0;
	if ( pBuffer->Data != NULL ) {
		pBuffer->Data[0] = 0;
	}
}



/* 返回字符串构建器当前内容的借用视图。 */
XRT_API xstrview xrtStrBufView(const xstrbuf* pBuffer)
{
	if ( !xrtStrBufValid(pBuffer) ) {
		return xrtStrViewN(NULL, 0);
	}
	return xrtStrViewN(pBuffer->Data, pBuffer->Size);
}



/* 保证字符串构建器至少具有指定数据容量。 */
XRT_API bool xrtStrBufReserve(xstrbuf* pBuffer, size_t iCapacity)
{
	size_t iNewCapacity;
	str sData;

	if ( !xrtStrBufValid(pBuffer) ) {
		return false;
	}
	if ( iCapacity <= pBuffer->Capacity ) {
		return true;
	}
	if ( !__xrtStrBufGrowth(pBuffer->Capacity, iCapacity, &iNewCapacity) ) {
		return false;
	}
	sData = (str)xrtRealloc(pBuffer->Data, iNewCapacity + 1u);
	if ( sData == NULL ) {
		return false;
	}
	pBuffer->Data = sData;
	pBuffer->Capacity = iNewCapacity;
	pBuffer->Data[pBuffer->Size] = 0;
	return true;
}



/* 调整字符串构建器长度，扩展区域填零。 */
XRT_API bool xrtStrBufResize(xstrbuf* pBuffer, size_t iSize)
{
	size_t iOldSize;

	if ( !xrtStrBufValid(pBuffer) ) {
		return false;
	}
	iOldSize = pBuffer->Size;
	if ( !xrtStrBufReserve(pBuffer, iSize) ) {
		return false;
	}
	if ( iSize > iOldSize ) {
		memset(pBuffer->Data + iOldSize, 0, iSize - iOldSize);
	}
	pBuffer->Size = iSize;
	if ( pBuffer->Data != NULL ) {
		pBuffer->Data[iSize] = 0;
	}
	return true;
}



/* 追加字符串视图，允许追加自身的有效子视图。 */
XRT_API bool xrtStrBufAppend(xstrbuf* pBuffer, xstrview Text)
{
	bool bAlias;
	size_t iOffset;
	size_t iSize;
	cstr sSource;

	if ( !xrtStrBufValid(pBuffer) || !__xrtStrViewValid(Text) ) {
		return false;
	}
	if ( Text.Size == 0 ) {
		return true;
	}
	if ( !xrtStrBufAlias(pBuffer, Text, &bAlias, &iOffset) ) {
		return false;
	}
	if ( Text.Size > (SIZE_MAX - pBuffer->Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iSize = pBuffer->Size + Text.Size;
	if ( !xrtStrBufReserve(pBuffer, iSize) ) {
		return false;
	}
	sSource = bAlias ? pBuffer->Data + iOffset : Text.Data;
	memmove(pBuffer->Data + pBuffer->Size, sSource, Text.Size);
	pBuffer->Size = iSize;
	pBuffer->Data[iSize] = 0;
	return true;
}



/* 追加一个字节。 */
XRT_API bool xrtStrBufAppendByte(xstrbuf* pBuffer, char iByte)
{
	if ( !xrtStrBufValid(pBuffer) ) {
		return false;
	}
	if ( pBuffer->Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( !xrtStrBufReserve(pBuffer, pBuffer->Size + 1u) ) {
		return false;
	}
	pBuffer->Data[pBuffer->Size++] = iByte;
	pBuffer->Data[pBuffer->Size] = 0;
	return true;
}



/* 重复追加字符串视图。 */
XRT_API bool xrtStrBufAppendRepeat(xstrbuf* pBuffer, xstrview Text, size_t iCount)
{
	bool bAlias;
	size_t iOffset;
	size_t iAppendSize;
	size_t iResultSize;
	size_t iStart;
	size_t iWritten;
	cstr sSource;

	if ( !xrtStrBufValid(pBuffer) || !__xrtStrViewValid(Text) ) {
		return false;
	}
	if ( (Text.Size == 0) || (iCount == 0) ) {
		return true;
	}
	if ( !xrtStrBufAlias(pBuffer, Text, &bAlias, &iOffset) ) {
		return false;
	}
	if ( iCount > (SIZE_MAX / Text.Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iAppendSize = Text.Size * iCount;
	if ( iAppendSize > (SIZE_MAX - pBuffer->Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iResultSize = pBuffer->Size + iAppendSize;
	if ( !xrtStrBufReserve(pBuffer, iResultSize) ) {
		return false;
	}
	sSource = bAlias ? pBuffer->Data + iOffset : Text.Data;
	iStart = pBuffer->Size;
	memmove(pBuffer->Data + iStart, sSource, Text.Size);
	iWritten = Text.Size;
	while ( iWritten < iAppendSize ) {
		size_t iTake = iWritten < (iAppendSize - iWritten) ?
			iWritten : iAppendSize - iWritten;

		memcpy(pBuffer->Data + iStart + iWritten,
			pBuffer->Data + iStart, iTake);
		iWritten += iTake;
	}
	pBuffer->Size = iResultSize;
	pBuffer->Data[iResultSize] = 0;
	return true;
}



/* 取走构建器内存并把构建器重置为空。 */
XRT_API str xrtStrBufTake(xstrbuf* pBuffer)
{
	str sResult;

	if ( !xrtStrBufValid(pBuffer) ) {
		return NULL;
	}
	if ( pBuffer->Data == NULL ) {
		return xrtStrDup("");
	}
	sResult = pBuffer->Data;
	memset(pBuffer, 0, sizeof(xstrbuf));
	return sResult;
}

#endif
