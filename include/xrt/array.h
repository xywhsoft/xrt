#ifndef XRT_ARRAY_H
#define XRT_ARRAY_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_PTR_ARRAY) && !defined(XRT_FEATURE_ARRAY)
	#error "XRT_FEATURE_PTR_ARRAY requires XRT_FEATURE_ARRAY"
#endif



#define XRT_ARRAY_ALIGNMENT_DEFAULT 16u



/* 排序比较器与 C qsort 保持相同的参数语义。 */
typedef int (*xarraycompare)(const void* pLeft, const void* pRight);



#if defined(XRT_FEATURE_ARRAY)

/* 动态数组连续存储固定大小元素，不接管元素内部资源。 */
typedef struct xarray {
	bytes Data;
	ptr Allocation;
	size_t ItemSize;
	size_t Count;
	size_t Capacity;
	size_t Alignment;
} xarray;



XRT_EXTERN_C_BEGIN



/* 使用全局堆的默认对齐初始化空数组。 */
XRT_API bool xrtArrayInit(xarray* pArray, size_t iItemSize);



/* 初始化显式过对齐数组，元素大小必须是对齐值的倍数。 */
XRT_API bool xrtArrayInitAligned(xarray* pArray, size_t iItemSize, size_t iAlignment);



/* 创建使用全局堆默认对齐的空数组。 */
XRT_API xarray* xrtArrayCreate(size_t iItemSize);



/* 创建显式过对齐的空数组。 */
XRT_API xarray* xrtArrayCreateAligned(size_t iItemSize, size_t iAlignment);



/* 释放数组持有的元素内存，但不释放数组结构。 */
XRT_API void xrtArrayUnit(xarray* pArray);



/* 释放数组持有的全部资源和数组结构。 */
XRT_API void xrtArrayDestroy(xarray* pArray);



/* 清空元素但保留已有容量。 */
XRT_API void xrtArrayClear(xarray* pArray);



/* 保证数组至少具有指定元素容量。 */
XRT_API bool xrtArrayReserve(xarray* pArray, size_t iCapacity);



/* 调整元素数量，新增元素全部清零，缩小时保留容量。 */
XRT_API bool xrtArrayResize(xarray* pArray, size_t iCount);



/* 将容量裁剪到当前元素数量。 */
XRT_API bool xrtArrayTrim(xarray* pArray);



/* 返回指定 0 基索引处的可写元素，越界时返回空指针。 */
XRT_API ptr xrtArrayGet(xarray* pArray, size_t iIndex);



/* 返回指定 0 基索引处的只读元素，越界时返回空指针。 */
XRT_API const void* xrtArrayConstGet(const xarray* pArray, size_t iIndex);



/* 在末尾增加未初始化元素，并返回第一个新增元素。 */
XRT_API ptr xrtArrayAdd(xarray* pArray, size_t iCount);



/* 在指定 0 基位点插入未初始化元素，并返回第一个新增元素。 */
XRT_API ptr xrtArrayInsertSpace(xarray* pArray, size_t iIndex, size_t iCount);



/* 复制一个元素到数组末尾。 */
XRT_API bool xrtArrayPush(xarray* pArray, const void* pItem);



/* 复制一段连续元素到数组末尾，允许来源是数组自身的有效区间。 */
XRT_API bool xrtArrayAppend(xarray* pArray, const void* pItems, size_t iCount);



/* 在指定 0 基位点复制插入连续元素，允许来源是数组自身的有效区间。 */
XRT_API bool xrtArrayInsert(xarray* pArray, size_t iIndex, const void* pItems, size_t iCount);



/* 覆盖指定 0 基索引处的一个元素。 */
XRT_API bool xrtArraySet(xarray* pArray, size_t iIndex, const void* pItem);



/* 删除指定 0 基索引开始的精确元素区间。 */
XRT_API bool xrtArrayRemove(xarray* pArray, size_t iIndex, size_t iCount);



/* 使用末尾元素覆盖指定元素并删除末尾，元素顺序不会保留。 */
XRT_API bool xrtArrayRemoveSwap(xarray* pArray, size_t iIndex);



/* 删除末尾元素，并可将元素内容复制到输出地址。 */
XRT_API bool xrtArrayPop(xarray* pArray, ptr pItem);



/* 交换两个 0 基索引处的元素，不进行动态分配。 */
XRT_API bool xrtArraySwap(xarray* pArray, size_t iLeft, size_t iRight);



/* 原地反转元素顺序。 */
XRT_API bool xrtArrayReverse(xarray* pArray);



/* 使用不稳定快速排序原地排列元素。 */
XRT_API bool xrtArraySort(xarray* pArray, xarraycompare pCompare);



/* 按元素字节查找第一个完全相同的元素。 */
XRT_API size_t xrtArrayFind(const xarray* pArray, const void* pItem);



/* 使用比较器线性查找第一个匹配元素，比较器接收 key 和元素。 */
XRT_API size_t xrtArrayFindBy(const xarray* pArray, const void* pKey, xarraycompare pCompare);



/* 在已按同一比较器排序的数组中二分查找元素。 */
XRT_API size_t xrtArrayBSearch(const xarray* pArray, const void* pKey, xarraycompare pCompare);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_PTR_ARRAY)

/* 指针数组复用动态数组存储核心，仅增加类型友好接口。 */
typedef xarray xptrarray;



XRT_EXTERN_C_BEGIN



/* 初始化一个不拥有所存指针目标的空指针数组。 */
XRT_API bool xrtPtrArrayInit(xptrarray* pArray);



/* 创建一个不拥有所存指针目标的空指针数组。 */
XRT_API xptrarray* xrtPtrArrayCreate(void);



/* 释放指针存储区，但不释放各指针指向的对象。 */
XRT_API void xrtPtrArrayUnit(xptrarray* pArray);



/* 释放指针数组结构，但不释放各指针指向的对象。 */
XRT_API void xrtPtrArrayDestroy(xptrarray* pArray);



/* 清空指针数组但保留容量。 */
XRT_API void xrtPtrArrayClear(xptrarray* pArray);



/* 保证指针数组至少具有指定容量。 */
XRT_API bool xrtPtrArrayReserve(xptrarray* pArray, size_t iCapacity);



/* 调整指针数量，新增位置全部设置为空指针。 */
XRT_API bool xrtPtrArrayResize(xptrarray* pArray, size_t iCount);



/* 将容量裁剪到当前指针数量。 */
XRT_API bool xrtPtrArrayTrim(xptrarray* pArray);



/* 返回可直接遍历的可写指针视图，结构性修改后旧视图失效。 */
XRT_API ptr* xrtPtrArrayData(xptrarray* pArray);



/* 返回可直接遍历的只读指针视图，结构性修改后旧视图失效。 */
XRT_API ptr const* xrtPtrArrayConstData(const xptrarray* pArray);



/* 返回指定 0 基索引处的指针，越界时返回空指针并报告范围错误。 */
XRT_API ptr xrtPtrArrayGet(const xptrarray* pArray, size_t iIndex);



/* 覆盖指定 0 基索引处的指针。 */
XRT_API bool xrtPtrArraySet(xptrarray* pArray, size_t iIndex, ptr pValue);



/* 向末尾追加一个指针。 */
XRT_API bool xrtPtrArrayPush(xptrarray* pArray, ptr pValue);



/* 向末尾复制追加一段连续指针。 */
XRT_API bool xrtPtrArrayAppend(xptrarray* pArray, ptr const* pValues, size_t iCount);



/* 在指定 0 基位点插入一个指针。 */
XRT_API bool xrtPtrArrayInsert(xptrarray* pArray, size_t iIndex, ptr pValue);



/* 在指定 0 基位点复制插入一段连续指针。 */
XRT_API bool xrtPtrArrayInsertMany(xptrarray* pArray, size_t iIndex, ptr const* pValues, size_t iCount);



/* 删除指定 0 基索引开始的精确指针区间。 */
XRT_API bool xrtPtrArrayRemove(xptrarray* pArray, size_t iIndex, size_t iCount);



/* 使用末尾指针覆盖指定位置并删除末尾，指针顺序不会保留。 */
XRT_API bool xrtPtrArrayRemoveSwap(xptrarray* pArray, size_t iIndex);



/* 删除末尾指针并写入输出参数，输出参数不能为空。 */
XRT_API bool xrtPtrArrayPop(xptrarray* pArray, ptr* pValue);



/* 交换两个 0 基索引处的指针。 */
XRT_API bool xrtPtrArraySwap(xptrarray* pArray, size_t iLeft, size_t iRight);



/* 原地反转指针顺序。 */
XRT_API bool xrtPtrArrayReverse(xptrarray* pArray);



/* 按 qsort 的指针元素参数语义原地排序。 */
XRT_API bool xrtPtrArraySort(xptrarray* pArray, xarraycompare pCompare);



/* 按指针值查找第一个匹配位置。 */
XRT_API size_t xrtPtrArrayFind(const xptrarray* pArray, const void* pValue);



XRT_EXTERN_C_END

#endif

#endif
