#include "../internal/xrt_array.h"



#if defined(XRT_FEATURE_PTR_ARRAY)

/* 检查数组确实由指针数组入口初始化，完整状态由底层操作统一校验。 */
static bool __xrtPtrArrayTypeValid(const xptrarray* pArray)
{
	if ( pArray == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pArray->ItemSize != sizeof(ptr) ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 初始化一个不拥有所存指针目标的空指针数组。 */
XRT_API bool xrtPtrArrayInit(xptrarray* pArray)
{
	return xrtArrayInit(pArray, sizeof(ptr));
}



/* 创建一个不拥有所存指针目标的空指针数组。 */
XRT_API xptrarray* xrtPtrArrayCreate(void)
{
	return (xptrarray*)xrtArrayCreate(sizeof(ptr));
}



/* 释放指针存储区，但不释放各指针指向的对象。 */
XRT_API void xrtPtrArrayUnit(xptrarray* pArray)
{
	xrtArrayUnit(pArray);
}



/* 释放指针数组结构，但不释放各指针指向的对象。 */
XRT_API void xrtPtrArrayDestroy(xptrarray* pArray)
{
	xrtArrayDestroy(pArray);
}



/* 清空指针数组但保留容量。 */
XRT_API void xrtPtrArrayClear(xptrarray* pArray)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return;
	}

	xrtArrayClear(pArray);
}



/* 保证指针数组至少具有指定容量。 */
XRT_API bool xrtPtrArrayReserve(xptrarray* pArray, size_t iCapacity)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayReserve(pArray, iCapacity);
}



/* 调整指针数量，新增位置全部设置为空指针。 */
XRT_API bool xrtPtrArrayResize(xptrarray* pArray, size_t iCount)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayResize(pArray, iCount);
}



/* 将容量裁剪到当前指针数量。 */
XRT_API bool xrtPtrArrayTrim(xptrarray* pArray)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayTrim(pArray);
}



/* 返回可直接遍历的可写指针视图，结构性修改后旧视图失效。 */
XRT_API ptr* xrtPtrArrayData(xptrarray* pArray)
{
	if (
		!__xrtPtrArrayTypeValid(pArray) ||
		!__xrtArrayValid(pArray)
	) {
		return NULL;
	}

	return (ptr*)pArray->Data;
}



/* 返回可直接遍历的只读指针视图，结构性修改后旧视图失效。 */
XRT_API ptr const* xrtPtrArrayConstData(const xptrarray* pArray)
{
	if (
		!__xrtPtrArrayTypeValid(pArray) ||
		!__xrtArrayValid(pArray)
	) {
		return NULL;
	}

	return (ptr const*)pArray->Data;
}



/* 返回指定 0 基索引处的指针，越界时返回空指针并报告范围错误。 */
XRT_API ptr xrtPtrArrayGet(const xptrarray* pArray, size_t iIndex)
{
	ptr const* pValue;

	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return NULL;
	}
	pValue = (ptr const*)xrtArrayConstGet(pArray, iIndex);
	return pValue != NULL ? *pValue : NULL;
}



/* 覆盖指定 0 基索引处的指针。 */
XRT_API bool xrtPtrArraySet(xptrarray* pArray, size_t iIndex, ptr pValue)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArraySet(pArray, iIndex, &pValue);
}



/* 向末尾追加一个指针。 */
XRT_API bool xrtPtrArrayPush(xptrarray* pArray, ptr pValue)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayPush(pArray, &pValue);
}



/* 向末尾复制追加一段连续指针。 */
XRT_API bool xrtPtrArrayAppend(xptrarray* pArray, ptr const* pValues, size_t iCount)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayAppend(pArray, pValues, iCount);
}



/* 在指定 0 基位点插入一个指针。 */
XRT_API bool xrtPtrArrayInsert(xptrarray* pArray, size_t iIndex, ptr pValue)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayInsert(pArray, iIndex, &pValue, 1);
}



/* 在指定 0 基位点复制插入一段连续指针。 */
XRT_API bool xrtPtrArrayInsertMany(
	xptrarray* pArray,
	size_t iIndex,
	ptr const* pValues,
	size_t iCount
)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayInsert(pArray, iIndex, pValues, iCount);
}



/* 删除指定 0 基索引开始的精确指针区间。 */
XRT_API bool xrtPtrArrayRemove(xptrarray* pArray, size_t iIndex, size_t iCount)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayRemove(pArray, iIndex, iCount);
}



/* 使用末尾指针覆盖指定位置并删除末尾，指针顺序不会保留。 */
XRT_API bool xrtPtrArrayRemoveSwap(xptrarray* pArray, size_t iIndex)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayRemoveSwap(pArray, iIndex);
}



/* 删除末尾指针并写入输出参数，输出参数不能为空。 */
XRT_API bool xrtPtrArrayPop(xptrarray* pArray, ptr* pValue)
{
	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayPop(pArray, pValue);
}



/* 交换两个 0 基索引处的指针。 */
XRT_API bool xrtPtrArraySwap(xptrarray* pArray, size_t iLeft, size_t iRight)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArraySwap(pArray, iLeft, iRight);
}



/* 原地反转指针顺序。 */
XRT_API bool xrtPtrArrayReverse(xptrarray* pArray)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArrayReverse(pArray);
}



/* 按 qsort 的指针元素参数语义原地排序。 */
XRT_API bool xrtPtrArraySort(xptrarray* pArray, xarraycompare pCompare)
{
	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return false;
	}

	return xrtArraySort(pArray, pCompare);
}



/* 按指针值查找第一个匹配位置。 */
XRT_API size_t xrtPtrArrayFind(const xptrarray* pArray, const void* pValue)
{
	const void* pKey = pValue;

	if ( !__xrtPtrArrayTypeValid(pArray) ) {
		return XRT_NPOS;
	}

	return xrtArrayFind(pArray, &pKey);
}

#endif
