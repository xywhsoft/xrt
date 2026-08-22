#include "../internal/xrt_array.h"



#if defined(XRT_FEATURE_ARRAY)

/* 检查数组公开状态是否自洽。 */
bool __xrtArrayValid(const xarray* pArray)
{
	size_t iCapacityBytes;
	size_t iAllocationBytes;
	uintptr_t iAllocation;
	uintptr_t iAllocationEnd;
	uintptr_t iData;
	uintptr_t iDataEnd;

	if ( pArray == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(pArray->ItemSize == 0) ||
		(pArray->Count > pArray->Capacity) ||
		(pArray->Alignment == 0) ||
		((pArray->Alignment & (pArray->Alignment - 1u)) != 0) ||
		((pArray->Capacity == 0) && ((pArray->Data != NULL) || (pArray->Allocation != NULL))) ||
		((pArray->Capacity != 0) && ((pArray->Data == NULL) || (pArray->Allocation == NULL)))
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pArray->Capacity > (SIZE_MAX / pArray->ItemSize) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if (
		(pArray->Data != NULL) &&
		(((uintptr_t)pArray->Data & (pArray->Alignment - 1u)) != 0)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pArray->Capacity == 0 ) {
		return true;
	}

	/* 验证公开结构描述的完整分配区间和活动数据区间。 */
	iCapacityBytes = pArray->Capacity * pArray->ItemSize;
	iAllocationBytes = iCapacityBytes;
	if ( pArray->Alignment > XRT_ARRAY_ALIGNMENT_DEFAULT ) {
		if ( iAllocationBytes > (SIZE_MAX - (pArray->Alignment - 1u)) ) {
			__xrtErrorSetInvalidState();
			return false;
		}
		iAllocationBytes += pArray->Alignment - 1u;
	} else if ( pArray->Data != (bytes)pArray->Allocation ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	iAllocation = (uintptr_t)pArray->Allocation;
	iData = (uintptr_t)pArray->Data;
	if (
		(iAllocation > (UINTPTR_MAX - iAllocationBytes)) ||
		(iData > (UINTPTR_MAX - iCapacityBytes))
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iAllocationEnd = iAllocation + iAllocationBytes;
	iDataEnd = iData + iCapacityBytes;
	if (
		(iData < iAllocation) ||
		(iDataEnd > iAllocationEnd) ||
		((iData - iAllocation) >= pArray->Alignment)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 检查对齐参数是否可用于每一个连续元素。 */
static bool __xrtArrayAlignmentValid(size_t iItemSize, size_t iAlignment)
{
	if (
		(iItemSize == 0) ||
		(iAlignment == 0) ||
		((iAlignment & (iAlignment - 1u)) != 0) ||
		((iItemSize % iAlignment) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	return true;
}



/* 计算指定容量对应的字节数。 */
static bool __xrtArrayBytes(const xarray* pArray, size_t iCapacity, size_t* pBytes)
{
	if ( iCapacity > (SIZE_MAX / pArray->ItemSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}

	*pBytes = iCapacity * pArray->ItemSize;
	return true;
}



/* 计算满足需求且不过度浪费小数组空间的几何容量。 */
static bool __xrtArrayGrowth(const xarray* pArray, size_t iNeed, size_t* pCapacity)
{
	size_t iCapacity = pArray->Capacity >= 8u ? pArray->Capacity : 8u;
	size_t iBytes;

	while ( iCapacity < iNeed ) {
		size_t iGrowth = (iCapacity / 2u) + 8u;

		/* 接近上限时直接使用精确需求，避免增长计算回绕。 */
		if ( iGrowth > (SIZE_MAX - iCapacity) ) {
			iCapacity = iNeed;
			break;
		}
		iCapacity += iGrowth;
	}
	if ( !__xrtArrayBytes(pArray, iCapacity, &iBytes) ) {
		return false;
	}

	(void)iBytes;
	*pCapacity = iCapacity;
	return true;
}



/* 为过对齐数组分配原始块并计算对齐数据地址。 */
static bool __xrtArrayAlignedAlloc(
	const xarray* pArray,
	size_t iBytes,
	ptr* pAllocation,
	bytes* pData
)
{
	size_t iExtra = pArray->Alignment - 1u;
	uintptr_t iAddress;
	ptr pMemory;

	if ( iBytes > (SIZE_MAX - iExtra) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pMemory = xrtMalloc(iBytes + iExtra);
	if ( pMemory == NULL ) {
		return false;
	}
	if ( (uintptr_t)pMemory > (UINTPTR_MAX - iExtra) ) {
		xrtFree(pMemory);
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iAddress = ((uintptr_t)pMemory + iExtra) & ~((uintptr_t)pArray->Alignment - 1u);

	*pAllocation = pMemory;
	*pData = (bytes)iAddress;
	return true;
}



/* 精确设置容量，失败时保留数组原有数据和状态。 */
static bool __xrtArraySetCapacity(xarray* pArray, size_t iCapacity)
{
	size_t iBytes;
	ptr pAllocation;
	bytes pData;

	if ( iCapacity == pArray->Capacity ) {
		return true;
	}
	if ( iCapacity < pArray->Count ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( iCapacity == 0 ) {
		xrtFree(pArray->Allocation);
		pArray->Data = NULL;
		pArray->Allocation = NULL;
		pArray->Capacity = 0;
		return true;
	}
	if ( !__xrtArrayBytes(pArray, iCapacity, &iBytes) ) {
		return false;
	}

	/* 默认对齐块可以直接使用全局堆的重分配快路径。 */
	if (
		(pArray->Alignment <= XRT_ARRAY_ALIGNMENT_DEFAULT) &&
		((pArray->Data == NULL) || (pArray->Data == (bytes)pArray->Allocation))
	) {
		pAllocation = xrtRealloc(pArray->Allocation, iBytes);
		if ( pAllocation == NULL ) {
			return false;
		}
		pArray->Allocation = pAllocation;
		pArray->Data = (bytes)pAllocation;
		pArray->Capacity = iCapacity;
		return true;
	}

	/* 过对齐块先成功取得新内存，再替换旧块。 */
	if ( !__xrtArrayAlignedAlloc(pArray, iBytes, &pAllocation, &pData) ) {
		return false;
	}
	if ( pArray->Count != 0 ) {
		memcpy(pData, pArray->Data, pArray->Count * pArray->ItemSize);
	}
	xrtFree(pArray->Allocation);
	pArray->Allocation = pAllocation;
	pArray->Data = pData;
	pArray->Capacity = iCapacity;
	return true;
}



/* 判断复制来源是否与数组存储重叠，并返回有效来源偏移。 */
static bool __xrtArraySource(
	const xarray* pArray,
	const void* pItems,
	size_t iCount,
	bool* pAlias,
	size_t* pOffset
)
{
	size_t iCopyBytes;
	size_t iLiveBytes;
	size_t iCapacityBytes;
	size_t iAllocationBytes;
	uintptr_t iAllocation;
	uintptr_t iAllocationEnd;
	uintptr_t iData;
	uintptr_t iLiveEnd;
	uintptr_t iSource;
	uintptr_t iSourceEnd;

	*pAlias = false;
	*pOffset = 0;
	if ( pItems == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		!__xrtArrayBytes(pArray, iCount, &iCopyBytes) ||
		!__xrtArrayBytes(pArray, pArray->Count, &iLiveBytes) ||
		!__xrtArrayBytes(pArray, pArray->Capacity, &iCapacityBytes)
	) {
		return false;
	}
	if ( pArray->Data == NULL ) {
		return true;
	}

	/* 计算完整原始块，连同过对齐产生的前后填充区一起检查。 */
	iAllocationBytes = iCapacityBytes;
	if ( pArray->Alignment > XRT_ARRAY_ALIGNMENT_DEFAULT ) {
		if ( iAllocationBytes > (SIZE_MAX - (pArray->Alignment - 1u)) ) {
			__xrtErrorSetInvalidState();
			return false;
		}
		iAllocationBytes += pArray->Alignment - 1u;
	}
	iAllocation = (uintptr_t)pArray->Allocation;
	if ( iAllocation > (UINTPTR_MAX - iAllocationBytes) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iAllocationEnd = iAllocation + iAllocationBytes;

	/* 使用整数区间判断，避免比较无关 C 指针产生未定义行为。 */
	iData = (uintptr_t)pArray->Data;
	if ( iData > (UINTPTR_MAX - iLiveBytes) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iLiveEnd = iData + iLiveBytes;
	iSource = (uintptr_t)pItems;
	if ( iSource > (UINTPTR_MAX - iCopyBytes) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iSourceEnd = iSource + iCopyBytes;

	/* 任何触及原始分配块的来源都必须完整位于当前活动元素区。 */
	if ( (iSource < iAllocationEnd) && (iSourceEnd > iAllocation) ) {
		if (
			(iSource < iData) ||
			(iSourceEnd > iLiveEnd) ||
			(((size_t)(iSource - iData) % pArray->ItemSize) != 0)
		) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		*pAlias = true;
		*pOffset = (size_t)(iSource - iData);
	}

	return true;
}



/* 交换两个已验证且不重叠的元素。 */
static void __xrtArraySwapItems(xarray* pArray, size_t iLeft, size_t iRight)
{
	unsigned char pTemp[256];
	bytes pLeft = pArray->Data + (iLeft * pArray->ItemSize);
	bytes pRight = pArray->Data + (iRight * pArray->ItemSize);
	size_t iOffset = 0;

	while ( iOffset < pArray->ItemSize ) {
		size_t iRemain = pArray->ItemSize - iOffset;
		size_t iChunk = iRemain < sizeof(pTemp) ? iRemain : sizeof(pTemp);

		memcpy(pTemp, pLeft + iOffset, iChunk);
		memcpy(pLeft + iOffset, pRight + iOffset, iChunk);
		memcpy(pRight + iOffset, pTemp, iChunk);
		iOffset += iChunk;
	}
}



/* 使用全局堆的默认对齐初始化空数组。 */
XRT_API bool xrtArrayInit(xarray* pArray, size_t iItemSize)
{
	if ( (pArray == NULL) || (iItemSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	memset(pArray, 0, sizeof(xarray));
	pArray->ItemSize = iItemSize;
	pArray->Alignment = XRT_ARRAY_ALIGNMENT_DEFAULT;
	return true;
}



/* 初始化显式过对齐数组，元素大小必须是对齐值的倍数。 */
XRT_API bool xrtArrayInitAligned(xarray* pArray, size_t iItemSize, size_t iAlignment)
{
	if ( (pArray == NULL) || !__xrtArrayAlignmentValid(iItemSize, iAlignment) ) {
		if ( pArray == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	memset(pArray, 0, sizeof(xarray));
	pArray->ItemSize = iItemSize;
	pArray->Alignment = iAlignment;
	return true;
}



/* 创建使用全局堆默认对齐的空数组。 */
XRT_API xarray* xrtArrayCreate(size_t iItemSize)
{
	xarray* pArray;

	if ( iItemSize == 0 ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pArray = (xarray*)xrtMalloc(sizeof(xarray));
	if ( pArray == NULL ) {
		return NULL;
	}
	if ( !xrtArrayInit(pArray, iItemSize) ) {
		xrtFree(pArray);
		return NULL;
	}

	return pArray;
}



/* 创建显式过对齐的空数组。 */
XRT_API xarray* xrtArrayCreateAligned(size_t iItemSize, size_t iAlignment)
{
	xarray* pArray;

	if ( !__xrtArrayAlignmentValid(iItemSize, iAlignment) ) {
		return NULL;
	}
	pArray = (xarray*)xrtMalloc(sizeof(xarray));
	if ( pArray == NULL ) {
		return NULL;
	}
	if ( !xrtArrayInitAligned(pArray, iItemSize, iAlignment) ) {
		xrtFree(pArray);
		return NULL;
	}

	return pArray;
}



/* 释放数组持有的元素内存，但不释放数组结构。 */
XRT_API void xrtArrayUnit(xarray* pArray)
{
	if ( pArray == NULL ) {
		return;
	}

	xrtFree(pArray->Allocation);
	memset(pArray, 0, sizeof(xarray));
}



/* 释放数组持有的全部资源和数组结构。 */
XRT_API void xrtArrayDestroy(xarray* pArray)
{
	if ( pArray == NULL ) {
		return;
	}

	xrtArrayUnit(pArray);
	xrtFree(pArray);
}



/* 清空元素但保留已有容量。 */
XRT_API void xrtArrayClear(xarray* pArray)
{
	if ( !__xrtArrayValid(pArray) ) {
		return;
	}

	pArray->Count = 0;
}



/* 在状态已经验证后保证数组至少具有指定元素容量。 */
bool __xrtArrayReserveValid(xarray* pArray, size_t iCapacity)
{
	size_t iNewCapacity;

	if ( iCapacity <= pArray->Capacity ) {
		return true;
	}
	if ( !__xrtArrayGrowth(pArray, iCapacity, &iNewCapacity) ) {
		return false;
	}

	return __xrtArraySetCapacity(pArray, iNewCapacity);
}



/* 保证数组至少具有指定元素容量。 */
XRT_API bool xrtArrayReserve(xarray* pArray, size_t iCapacity)
{
	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}

	return __xrtArrayReserveValid(pArray, iCapacity);
}



/* 调整元素数量，新增元素全部清零，缩小时保留容量。 */
XRT_API bool xrtArrayResize(xarray* pArray, size_t iCount)
{
	size_t iOldCount;

	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}
	iOldCount = pArray->Count;
	if ( iCount <= iOldCount ) {
		pArray->Count = iCount;
		return true;
	}
	if ( !__xrtArrayReserveValid(pArray, iCount) ) {
		return false;
	}

	/* 安全扩展路径保证所有新元素都有确定的零值。 */
	memset(
		pArray->Data + (iOldCount * pArray->ItemSize),
		0,
		(iCount - iOldCount) * pArray->ItemSize
	);
	pArray->Count = iCount;
	return true;
}



/* 将容量裁剪到当前元素数量。 */
XRT_API bool xrtArrayTrim(xarray* pArray)
{
	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}

	return __xrtArraySetCapacity(pArray, pArray->Count);
}



/* 返回指定 0 基索引处的可写元素，越界时返回空指针。 */
XRT_API ptr xrtArrayGet(xarray* pArray, size_t iIndex)
{
	return (ptr)xrtArrayConstGet(pArray, iIndex);
}



/* 返回指定 0 基索引处的只读元素，越界时返回空指针。 */
XRT_API const void* xrtArrayConstGet(const xarray* pArray, size_t iIndex)
{
	if ( !__xrtArrayValid(pArray) ) {
		return NULL;
	}
	if ( iIndex >= pArray->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}

	return pArray->Data + (iIndex * pArray->ItemSize);
}



/* 在状态已经验证后增加未初始化尾部元素。 */
ptr __xrtArrayAddValid(xarray* pArray, size_t iCount)
{
	ptr pItems;

	if ( iCount == 0 ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iCount > (SIZE_MAX - pArray->Count) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if ( !__xrtArrayReserveValid(pArray, pArray->Count + iCount) ) {
		return NULL;
	}

	pItems = pArray->Data + (pArray->Count * pArray->ItemSize);
	pArray->Count += iCount;
	return pItems;
}



/* 在末尾增加未初始化元素，并返回第一个新增元素。 */
XRT_API ptr xrtArrayAdd(xarray* pArray, size_t iCount)
{
	if ( !__xrtArrayValid(pArray) ) {
		return NULL;
	}

	return __xrtArrayAddValid(pArray, iCount);
}



/* 在状态已经验证后插入未初始化元素。 */
static ptr __xrtArrayInsertSpaceValid(
	xarray* pArray,
	size_t iIndex,
	size_t iCount
)
{
	bytes pInsert;

	if ( iCount == 0 ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iIndex > pArray->Count ) {
		__xrtErrorSetRange();
		return NULL;
	}
	if ( iCount > (SIZE_MAX - pArray->Count) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if ( !__xrtArrayReserveValid(pArray, pArray->Count + iCount) ) {
		return NULL;
	}

	pInsert = pArray->Data + (iIndex * pArray->ItemSize);
	if ( iIndex < pArray->Count ) {
		memmove(
			pInsert + (iCount * pArray->ItemSize),
			pInsert,
			(pArray->Count - iIndex) * pArray->ItemSize
		);
	}
	pArray->Count += iCount;
	return pInsert;
}



/* 在指定 0 基位点插入未初始化元素，并返回第一个新增元素。 */
XRT_API ptr xrtArrayInsertSpace(xarray* pArray, size_t iIndex, size_t iCount)
{
	if ( !__xrtArrayValid(pArray) ) {
		return NULL;
	}

	return __xrtArrayInsertSpaceValid(pArray, iIndex, iCount);
}



/* 复制一个元素到数组末尾。 */
XRT_API bool xrtArrayPush(xarray* pArray, const void* pItem)
{
	return xrtArrayAppend(pArray, pItem, 1);
}



/* 复制一段连续元素到数组末尾，允许来源是数组自身的有效区间。 */
XRT_API bool xrtArrayAppend(xarray* pArray, const void* pItems, size_t iCount)
{
	bool bAlias;
	size_t iOffset;
	size_t iCopyBytes;
	ptr pTarget;
	const void* pSource;

	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}
	if ( iCount == 0 ) {
		return true;
	}
	if (
		!__xrtArraySource(pArray, pItems, iCount, &bAlias, &iOffset) ||
		!__xrtArrayBytes(pArray, iCount, &iCopyBytes)
	) {
		return false;
	}
	pTarget = __xrtArrayAddValid(pArray, iCount);
	if ( pTarget == NULL ) {
		return false;
	}
	pSource = bAlias ? pArray->Data + iOffset : pItems;
	memmove(pTarget, pSource, iCopyBytes);
	return true;
}



/* 在指定 0 基位点复制插入连续元素，允许来源是数组自身的有效区间。 */
XRT_API bool xrtArrayInsert(xarray* pArray, size_t iIndex, const void* pItems, size_t iCount)
{
	bool bAlias;
	size_t iOffset;
	size_t iCopyBytes;
	size_t iInsertOffset;
	size_t iSourceEnd;
	size_t iPrefixBytes;
	ptr pTarget;

	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}
	if ( iIndex > pArray->Count ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( iCount == 0 ) {
		return true;
	}
	if (
		!__xrtArraySource(pArray, pItems, iCount, &bAlias, &iOffset) ||
		!__xrtArrayBytes(pArray, iCount, &iCopyBytes)
	) {
		return false;
	}

	iInsertOffset = iIndex * pArray->ItemSize;
	iSourceEnd = iOffset + iCopyBytes;
	pTarget = __xrtArrayInsertSpaceValid(pArray, iIndex, iCount);
	if ( pTarget == NULL ) {
		return false;
	}

	/* 外部来源与数组分配区不重叠，可以直接复制。 */
	if ( !bAlias ) {
		memcpy(pTarget, pItems, iCopyBytes);
		return true;
	}

	/* 插入点后的来源已经随尾部整体右移。 */
	if ( iOffset >= iInsertOffset ) {
		memmove(
			pTarget,
			pArray->Data + iOffset + iCopyBytes,
			iCopyBytes
		);
		return true;
	}

	/* 插入点前的完整来源仍保留在原偏移。 */
	if ( iSourceEnd <= iInsertOffset ) {
		memmove(pTarget, pArray->Data + iOffset, iCopyBytes);
		return true;
	}

	/* 跨越插入点的来源在移动后分为相邻的前后两段。 */
	iPrefixBytes = iInsertOffset - iOffset;
	memmove(pTarget, pArray->Data + iOffset, iPrefixBytes);
	memmove(
		(bytes)pTarget + iPrefixBytes,
		pArray->Data + iInsertOffset + iCopyBytes,
		iCopyBytes - iPrefixBytes
	);
	return true;
}



/* 覆盖指定 0 基索引处的一个元素。 */
XRT_API bool xrtArraySet(xarray* pArray, size_t iIndex, const void* pItem)
{
	bool bAlias;
	size_t iOffset;
	const void* pSource;

	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}
	if ( iIndex >= pArray->Count ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( !__xrtArraySource(pArray, pItem, 1, &bAlias, &iOffset) ) {
		return false;
	}

	pSource = bAlias ? pArray->Data + iOffset : pItem;
	memmove(pArray->Data + (iIndex * pArray->ItemSize), pSource, pArray->ItemSize);
	return true;
}



/* 删除指定 0 基索引开始的精确元素区间。 */
XRT_API bool xrtArrayRemove(xarray* pArray, size_t iIndex, size_t iCount)
{
	size_t iRemain;

	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}
	if (
		(iCount == 0) ||
		(iIndex >= pArray->Count) ||
		(iCount > (pArray->Count - iIndex))
	) {
		__xrtErrorSetRange();
		return false;
	}

	iRemain = pArray->Count - iIndex - iCount;
	if ( iRemain != 0 ) {
		memmove(
			pArray->Data + (iIndex * pArray->ItemSize),
			pArray->Data + ((iIndex + iCount) * pArray->ItemSize),
			iRemain * pArray->ItemSize
		);
	}
	pArray->Count -= iCount;
	return true;
}



/* 使用末尾元素覆盖指定元素并删除末尾，元素顺序不会保留。 */
XRT_API bool xrtArrayRemoveSwap(xarray* pArray, size_t iIndex)
{
	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}
	if ( iIndex >= pArray->Count ) {
		__xrtErrorSetRange();
		return false;
	}

	if ( iIndex != (pArray->Count - 1u) ) {
		memcpy(
			pArray->Data + (iIndex * pArray->ItemSize),
			pArray->Data + ((pArray->Count - 1u) * pArray->ItemSize),
			pArray->ItemSize
		);
	}
	pArray->Count--;
	return true;
}



/* 删除末尾元素，并可将元素内容复制到输出地址。 */
XRT_API bool xrtArrayPop(xarray* pArray, ptr pItem)
{
	bool bAlias;
	size_t iOffset;

	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}
	if ( pArray->Count == 0 ) {
		__xrtErrorSetRange();
		return false;
	}

	if ( pItem != NULL ) {
		if ( !__xrtArraySource(pArray, pItem, 1, &bAlias, &iOffset) ) {
			return false;
		}
		if ( bAlias ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memmove(
			pItem,
			pArray->Data + ((pArray->Count - 1u) * pArray->ItemSize),
			pArray->ItemSize
		);
	}
	pArray->Count--;
	return true;
}



/* 交换两个 0 基索引处的元素，不进行动态分配。 */
XRT_API bool xrtArraySwap(xarray* pArray, size_t iLeft, size_t iRight)
{
	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}
	if ( (iLeft >= pArray->Count) || (iRight >= pArray->Count) ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( iLeft != iRight ) {
		__xrtArraySwapItems(pArray, iLeft, iRight);
	}

	return true;
}



/* 原地反转元素顺序。 */
XRT_API bool xrtArrayReverse(xarray* pArray)
{
	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}

	for ( size_t i = 0; i < (pArray->Count / 2u); i++ ) {
		__xrtArraySwapItems(pArray, i, pArray->Count - i - 1u);
	}
	return true;
}



/* 使用不稳定快速排序原地排列元素。 */
XRT_API bool xrtArraySort(xarray* pArray, xarraycompare pCompare)
{
	if ( !__xrtArrayValid(pArray) ) {
		return false;
	}
	if ( pCompare == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pArray->Count < 2u ) {
		return true;
	}

	qsort(pArray->Data, pArray->Count, pArray->ItemSize, pCompare);
	return true;
}



/* 按元素字节查找第一个完全相同的元素。 */
XRT_API size_t xrtArrayFind(const xarray* pArray, const void* pItem)
{
	if ( !__xrtArrayValid(pArray) ) {
		return XRT_NPOS;
	}
	if ( pItem == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}

	for ( size_t i = 0; i < pArray->Count; i++ ) {
		if ( memcmp(pArray->Data + (i * pArray->ItemSize), pItem, pArray->ItemSize) == 0 ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 使用比较器线性查找第一个匹配元素，比较器接收 key 和元素。 */
XRT_API size_t xrtArrayFindBy(const xarray* pArray, const void* pKey, xarraycompare pCompare)
{
	if ( !__xrtArrayValid(pArray) ) {
		return XRT_NPOS;
	}
	if ( (pKey == NULL) || (pCompare == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}

	for ( size_t i = 0; i < pArray->Count; i++ ) {
		if ( pCompare(pKey, pArray->Data + (i * pArray->ItemSize)) == 0 ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 在已按同一比较器排序的数组中二分查找元素。 */
XRT_API size_t xrtArrayBSearch(const xarray* pArray, const void* pKey, xarraycompare pCompare)
{
	const void* pItem;

	if ( !__xrtArrayValid(pArray) ) {
		return XRT_NPOS;
	}
	if ( (pKey == NULL) || (pCompare == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( pArray->Count == 0 ) {
		return XRT_NPOS;
	}

	pItem = bsearch(pKey, pArray->Data, pArray->Count, pArray->ItemSize, pCompare);
	if ( pItem == NULL ) {
		return XRT_NPOS;
	}
	return (size_t)(((cbytes)pItem - pArray->Data) / pArray->ItemSize);
}

#endif
