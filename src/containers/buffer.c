#include "../internal/xrt_array.h"



#if defined(XRT_FEATURE_BUFFER)

/* 把轻量缓冲布局映射为数组字节存储，并验证公开状态。 */
static bool __xrtBufferArray(const xbuffer* pBuffer, xarray* pArray)
{
	if ( (pBuffer == NULL) || (pArray == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	pArray->Data = pBuffer->Data;
	pArray->Allocation = pBuffer->Data;
	pArray->ItemSize = 1u;
	pArray->Count = pBuffer->Size;
	pArray->Capacity = pBuffer->Capacity;
	pArray->Alignment = XRT_ARRAY_ALIGNMENT_DEFAULT;
	return __xrtArrayValid(pArray);
}



/* 把数组存储变更同步回公开缓冲布局。 */
static void __xrtBufferSync(xbuffer* pBuffer, const xarray* pArray)
{
	pBuffer->Data = pArray->Data;
	pBuffer->Size = pArray->Count;
	pBuffer->Capacity = pArray->Capacity;
}



/* 验证字节视图及其与缓冲分配区的关系，并返回有效区偏移。 */
static bool __xrtBufferSource(
	const xbuffer* pBuffer,
	xbytesview Data,
	bool* pAlias,
	size_t* pOffset
)
{
	uintptr_t iBuffer;
	uintptr_t iSource;

	*pAlias = false;
	*pOffset = 0;
	if ( (Data.Data == NULL) && (Data.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Data.Size == 0) || (pBuffer->Data == NULL) ) {
		return true;
	}
	if ( !__xrtRangesOverlap(
		pBuffer->Data,
		pBuffer->Capacity,
		Data.Data,
		Data.Size
	) ) {
		return true;
	}

	iBuffer = (uintptr_t)pBuffer->Data;
	iSource = (uintptr_t)Data.Data;
	if (
		(iSource < iBuffer) ||
		((iSource - iBuffer) > pBuffer->Size) ||
		(Data.Size > (pBuffer->Size - (size_t)(iSource - iBuffer)))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	*pAlias = true;
	*pOffset = (size_t)(iSource - iBuffer);
	return true;
}



/* 验证所有权槽没有位于任一被释放或接管的内存中。 */
static bool __xrtBufferTakeSlotValid(
	const xbuffer* pBuffer,
	const bytes* pData,
	bytes pOwned,
	size_t iCapacity
)
{
	if (
		__xrtRangesOverlap(
			pData,
			sizeof(*pData),
			pBuffer->Data,
			pBuffer->Capacity
		) ||
		__xrtRangesOverlap(
			pData,
			sizeof(*pData),
			pOwned,
			iCapacity
		) ||
		__xrtRangesOverlap(
			pBuffer->Data,
			pBuffer->Capacity,
			pOwned,
			iCapacity
		)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	return true;
}



/* 初始化调用方持有的空缓冲。 */
XRT_API bool xrtBufferInit(xbuffer* pBuffer)
{
	if ( pBuffer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	memset(pBuffer, 0, sizeof(xbuffer));
	return true;
}



/* 创建空缓冲。 */
XRT_API xbuffer* xrtBufferCreate(void)
{
	xbuffer* pBuffer = (xbuffer*)xrtMalloc(sizeof(xbuffer));

	if ( pBuffer == NULL ) {
		return NULL;
	}
	(void)xrtBufferInit(pBuffer);
	return pBuffer;
}



/* 释放缓冲持有的连续内存，但不释放缓冲结构。 */
XRT_API void xrtBufferUnit(xbuffer* pBuffer)
{
	if ( pBuffer == NULL ) {
		return;
	}

	xrtFree(pBuffer->Data);
	memset(pBuffer, 0, sizeof(xbuffer));
}



/* 释放缓冲持有的连续内存和缓冲结构。 */
XRT_API void xrtBufferDestroy(xbuffer* pBuffer)
{
	if ( pBuffer == NULL ) {
		return;
	}

	xrtBufferUnit(pBuffer);
	xrtFree(pBuffer);
}



/* 清空有效内容但保留容量。 */
XRT_API void xrtBufferClear(xbuffer* pBuffer)
{
	xarray tArray;

	if ( !__xrtBufferArray(pBuffer, &tArray) ) {
		return;
	}
	pBuffer->Size = 0;
}



/* 返回当前有效内容的借用视图。 */
XRT_API xbytesview xrtBufferView(const xbuffer* pBuffer)
{
	xarray tArray;

	if ( !__xrtBufferArray(pBuffer, &tArray) ) {
		return (xbytesview){ NULL, 0 };
	}
	return (xbytesview){ pBuffer->Data, pBuffer->Size };
}



/* 保证缓冲至少具有指定容量，实际容量可以按几何策略增长。 */
XRT_API bool xrtBufferReserve(xbuffer* pBuffer, size_t iCapacity)
{
	xarray tArray;

	if ( !__xrtBufferArray(pBuffer, &tArray) ||
		 !__xrtArrayReserveValid(&tArray, iCapacity) ) {
		return false;
	}
	__xrtBufferSync(pBuffer, &tArray);
	return true;
}



/* 调整有效长度，扩展区域全部填零，缩小时保留容量。 */
XRT_API bool xrtBufferResize(xbuffer* pBuffer, size_t iSize)
{
	xarray tArray;

	if ( !__xrtBufferArray(pBuffer, &tArray) ||
		 !xrtArrayResize(&tArray, iSize) ) {
		return false;
	}
	__xrtBufferSync(pBuffer, &tArray);
	return true;
}



/* 把容量精确裁剪到有效长度，空缓冲会释放存储。 */
XRT_API bool xrtBufferTrim(xbuffer* pBuffer)
{
	xarray tArray;

	if ( !__xrtBufferArray(pBuffer, &tArray) ||
		 !xrtArrayTrim(&tArray) ) {
		return false;
	}
	__xrtBufferSync(pBuffer, &tArray);
	return true;
}



/* 在末尾增加未初始化字节并返回首地址。 */
XRT_API bytes xrtBufferAdd(xbuffer* pBuffer, size_t iSize)
{
	xarray tArray;
	bytes pData;

	if ( !__xrtBufferArray(pBuffer, &tArray) ) {
		return NULL;
	}
	pData = (bytes)__xrtArrayAddValid(&tArray, iSize);
	if ( pData == NULL ) {
		return NULL;
	}
	__xrtBufferSync(pBuffer, &tArray);
	return pData;
}



/* 在指定位点插入未初始化字节并返回首地址。 */
XRT_API bytes xrtBufferInsertSpace(
	xbuffer* pBuffer,
	size_t iOffset,
	size_t iSize
)
{
	xarray tArray;
	bytes pData;

	if ( !__xrtBufferArray(pBuffer, &tArray) ) {
		return NULL;
	}
	pData = (bytes)xrtArrayInsertSpace(&tArray, iOffset, iSize);
	if ( pData == NULL ) {
		return NULL;
	}
	__xrtBufferSync(pBuffer, &tArray);
	return pData;
}



/* 用字节视图替换全部有效内容，失败时保留原缓冲。 */
XRT_API bool xrtBufferAssign(xbuffer* pBuffer, xbytesview Data)
{
	xarray tArray;
	bool bAlias;
	size_t iOffset;
	cbytes pSource;

	if ( !__xrtBufferArray(pBuffer, &tArray) ||
		 !__xrtBufferSource(pBuffer, Data, &bAlias, &iOffset) ) {
		return false;
	}
	if ( Data.Size == 0 ) {
		pBuffer->Size = 0;
		return true;
	}
	if ( !__xrtArrayReserveValid(&tArray, Data.Size) ) {
		return false;
	}
	pSource = bAlias ? tArray.Data + iOffset : Data.Data;
	memmove(tArray.Data, pSource, Data.Size);
	tArray.Count = Data.Size;
	__xrtBufferSync(pBuffer, &tArray);
	return true;
}



/* 复制追加字节视图，允许来源是缓冲自身的有效子视图。 */
XRT_API bool xrtBufferAppend(xbuffer* pBuffer, xbytesview Data)
{
	xarray tArray;

	if ( !__xrtBufferArray(pBuffer, &tArray) ||
		 !xrtArrayAppend(&tArray, Data.Data, Data.Size) ) {
		return false;
	}
	__xrtBufferSync(pBuffer, &tArray);
	return true;
}



/* 追加一个字节。 */
XRT_API bool xrtBufferAppendByte(xbuffer* pBuffer, uint8 iByte)
{
	return xrtBufferAppend(
		pBuffer,
		(xbytesview){ (const unsigned char*)&iByte, 1u }
	);
}



/* 在指定位点复制插入字节，允许来源是缓冲自身的有效子视图。 */
XRT_API bool xrtBufferInsert(
	xbuffer* pBuffer,
	size_t iOffset,
	xbytesview Data
)
{
	xarray tArray;

	if ( !__xrtBufferArray(pBuffer, &tArray) ||
		 !xrtArrayInsert(&tArray, iOffset, Data.Data, Data.Size) ) {
		return false;
	}
	__xrtBufferSync(pBuffer, &tArray);
	return true;
}



/* 覆盖或稀疏扩展字节，扩展产生的空洞全部填零。 */
XRT_API bool xrtBufferWrite(
	xbuffer* pBuffer,
	size_t iOffset,
	xbytesview Data
)
{
	xarray tArray;
	bool bAlias;
	size_t iSourceOffset;
	size_t iEnd;
	cbytes pSource;

	if ( !__xrtBufferArray(pBuffer, &tArray) ||
		 !__xrtBufferSource(pBuffer, Data, &bAlias, &iSourceOffset) ) {
		return false;
	}
	if ( Data.Size == 0 ) {
		return true;
	}
	if ( Data.Size > (SIZE_MAX - iOffset) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iEnd = iOffset + Data.Size;
	if ( (iEnd > tArray.Count) && !xrtArrayResize(&tArray, iEnd) ) {
		return false;
	}
	pSource = bAlias ? tArray.Data + iSourceOffset : Data.Data;
	memmove(tArray.Data + iOffset, pSource, Data.Size);
	__xrtBufferSync(pBuffer, &tArray);
	return true;
}



/* 删除完整有效区间，不会静默截断到末尾。 */
XRT_API bool xrtBufferRemove(
	xbuffer* pBuffer,
	size_t iOffset,
	size_t iSize
)
{
	xarray tArray;

	if ( !__xrtBufferArray(pBuffer, &tArray) ||
		 !xrtArrayRemove(&tArray, iOffset, iSize) ) {
		return false;
	}
	__xrtBufferSync(pBuffer, &tArray);
	return true;
}



/* 接管由 xrtMalloc 家族分配的连续内存。 */
XRT_API bool xrtBufferSetTake(
	xbuffer* pBuffer,
	bytes* pData,
	size_t iSize,
	size_t iCapacity
)
{
	xarray tArray;
	bytes pOwned;
	bytes pOld;

	if ( !__xrtBufferArray(pBuffer, &tArray) ) {
		return false;
	}
	if ( pData == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pOwned = *pData;
	if (
		(iSize > iCapacity) ||
		((iCapacity == 0) && (pOwned != NULL)) ||
		((iCapacity != 0) && (pOwned == NULL)) ||
		((pOwned != NULL) &&
		 (((uintptr_t)pOwned & (XRT_ARRAY_ALIGNMENT_DEFAULT - 1u)) != 0))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtBufferTakeSlotValid(pBuffer, pData, pOwned, iCapacity) ) {
		return false;
	}

	pOld = pBuffer->Data;
	pBuffer->Data = pOwned;
	pBuffer->Size = iSize;
	pBuffer->Capacity = iCapacity;
	*pData = NULL;
	xrtFree(pOld);
	return true;
}



/* 取走连续内存并把缓冲重置为空。 */
XRT_API bytes xrtBufferTake(
	xbuffer* pBuffer,
	size_t* pSize,
	size_t* pCapacity
)
{
	xarray tArray;
	bytes pData;

	if ( !__xrtBufferArray(pBuffer, &tArray) ) {
		return NULL;
	}
	if (
		((pSize != NULL) &&
		 (__xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			pBuffer,
			sizeof(*pBuffer)
		 ) || __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			pBuffer->Data,
			pBuffer->Capacity
		 ))) ||
		((pCapacity != NULL) &&
		 (__xrtRangesOverlap(
			pCapacity,
			sizeof(*pCapacity),
			pBuffer,
			sizeof(*pBuffer)
		 ) || __xrtRangesOverlap(
			pCapacity,
			sizeof(*pCapacity),
			pBuffer->Data,
			pBuffer->Capacity
		 ))) ||
		((pSize != NULL) && (pCapacity != NULL) &&
		 __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			pCapacity,
			sizeof(*pCapacity)
		))
	) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}

	pData = pBuffer->Data;
	if ( pSize != NULL ) {
		*pSize = pBuffer->Size;
	}
	if ( pCapacity != NULL ) {
		*pCapacity = pBuffer->Capacity;
	}
	memset(pBuffer, 0, sizeof(xbuffer));
	return pData;
}



/* 创建字节视图的独立副本。 */
XRT_API xbuffer* xrtBufferFrom(xbytesview Data)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( pBuffer == NULL ) {
		return NULL;
	}
	if ( !xrtBufferAssign(pBuffer, Data) ) {
		xrtBufferDestroy(pBuffer);
		return NULL;
	}
	return pBuffer;
}



/* 创建缓冲并接管来源槽。 */
XRT_API xbuffer* xrtBufferCreateTake(
	bytes* pData,
	size_t iSize,
	size_t iCapacity
)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( pBuffer == NULL ) {
		return NULL;
	}
	if ( !xrtBufferSetTake(pBuffer, pData, iSize, iCapacity) ) {
		xrtBufferDestroy(pBuffer);
		return NULL;
	}
	return pBuffer;
}

#endif
