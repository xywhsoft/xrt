#ifndef XRT_TYPED_ARRAY_H
#define XRT_TYPED_ARRAY_H

#include <xrt/array.h>
#include <xrt/runtime_type.h>



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY) && !defined(XRT_FEATURE_ARRAY)
	#error "XRUNTIME_FEATURE_TYPED_ARRAY requires XRT_FEATURE_ARRAY"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_ARRAY requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY)

/* 类型数组连续保存 ItemType::Size 字节值，并拥有每一个元素的生命周期。 */
typedef struct xtypedarray {
	xarray Storage;
	const xrttype* ItemType;
	uint32 Flags;
} xtypedarray;



/* 类型数组模块稳定错误代码。 */
typedef enum xtypedarrayerror {
	XTYPED_ARRAY_ERROR_ARGUMENT = 1,
	XTYPED_ARRAY_ERROR_TYPE,
	XTYPED_ARRAY_ERROR_RANGE,
	XTYPED_ARRAY_ERROR_OPERATION,
	XTYPED_ARRAY_ERROR_STATE
} xtypedarrayerror;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个拥有元素值的空类型数组。 */
XRT_API bool xrtTypedArrayInit(
	xtypedarray* pArray,
	const xrttype* pItemType
);
XRT_API xtypedarray* xrtTypedArrayCreate(const xrttype* pItemType);
XRT_API void xrtTypedArrayUnit(xtypedarray* pArray);
XRT_API void xrtTypedArrayDestroy(xtypedarray* pArray);



/* 返回借用元素类型、元素数和当前容量。 */
XRT_API const xrttype* xrtTypedArrayItemType(const xtypedarray* pArray);
XRT_API size_t xrtTypedArrayCount(const xtypedarray* pArray);
XRT_API size_t xrtTypedArrayCapacity(const xtypedarray* pArray);



/* 返回活动元素连续区的可写或只读借用；结构修改后旧地址失效。 */
XRT_API ptr xrtTypedArrayData(xtypedarray* pArray);
XRT_API const void* xrtTypedArrayConstData(const xtypedarray* pArray);



/* 预留、调整、裁剪或清空数组；新增元素按类型初始化。 */
XRT_API bool xrtTypedArrayReserve(xtypedarray* pArray, size_t iCapacity);
XRT_API bool xrtTypedArrayResize(xtypedarray* pArray, size_t iCount);
XRT_API bool xrtTypedArrayTrim(xtypedarray* pArray);
XRT_API void xrtTypedArrayClear(xtypedarray* pArray);



/* 返回指定下标的借用元素地址，结构修改后旧地址失效。 */
XRT_API ptr xrtTypedArrayGet(xtypedarray* pArray, size_t iIndex);
XRT_API const void* xrtTypedArrayConstGet(
	const xtypedarray* pArray,
	size_t iIndex
);



/* 复制追加、插入或替换元素；失败时数组保持原值。 */
XRT_API bool xrtTypedArrayPush(xtypedarray* pArray, const void* pItem);
XRT_API bool xrtTypedArrayInsert(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem
);
XRT_API bool xrtTypedArraySet(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem
);



/* 删除元素，或把一个元素移动到已初始化输出值后删除。 */
XRT_API bool xrtTypedArrayRemove(
	xtypedarray* pArray,
	size_t iIndex,
	size_t iCount
);
XRT_API bool xrtTypedArrayTake(
	xtypedarray* pArray,
	size_t iIndex,
	ptr pValue
);
XRT_API bool xrtTypedArrayPop(xtypedarray* pArray, ptr pValue);



/* 交换、反转、查找或判断元素；查找未命中返回 SIZE_MAX。 */
XRT_API bool xrtTypedArraySwap(
	xtypedarray* pArray,
	size_t iLeft,
	size_t iRight
);
XRT_API bool xrtTypedArrayReverse(xtypedarray* pArray);
XRT_API size_t xrtTypedArrayFind(
	const xtypedarray* pArray,
	const void* pItem
);
XRT_API bool xrtTypedArrayContains(
	const xtypedarray* pArray,
	const void* pItem
);



/* 事务追加同类型数组，或把两个数组深复制并拼接为独立数组。 */
XRT_API bool xrtTypedArrayAppend(
	xtypedarray* pTarget,
	const xtypedarray* pSource
);
XRT_API xtypedarray* xrtTypedArrayClone(const xtypedarray* pArray);
XRT_API xtypedarray* xrtTypedArrayConcat(
	const xtypedarray* pLeft,
	const xtypedarray* pRight
);



/* 比较两个数组的精确类型身份、元素数量和元素顺序。 */
XRT_API bool xrtTypedArrayEquals(
	const xtypedarray* pLeft,
	const xtypedarray* pRight
);



/* 验证对象数组类型描述，并返回其共享实例操作表。 */
XRT_API bool xrtTypedArrayTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedArrayInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif
