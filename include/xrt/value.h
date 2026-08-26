#ifndef XRT_VALUE_H
#define XRT_VALUE_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_VALUE) && !defined(XRT_FEATURE_HASH64)
	#error "XRT_FEATURE_VALUE requires XRT_FEATURE_HASH64"
#endif

#if defined(XRT_FEATURE_VALUE_CONTAINER) && !defined(XRT_FEATURE_VALUE)
	#error "XRT_FEATURE_VALUE_CONTAINER requires XRT_FEATURE_VALUE"
#endif

#if defined(XRT_FEATURE_VALUE_CONTAINER) && !defined(XRT_FEATURE_PTR_ARRAY)
	#error "XRT_FEATURE_VALUE_CONTAINER requires XRT_FEATURE_PTR_ARRAY"
#endif

#if defined(XRT_FEATURE_VALUE_CONTAINER) && !defined(XRT_FEATURE_INT_MAP)
	#error "XRT_FEATURE_VALUE_CONTAINER requires XRT_FEATURE_INT_MAP"
#endif

#if defined(XRT_FEATURE_VALUE_CONTAINER) && !defined(XRT_FEATURE_MAP)
	#error "XRT_FEATURE_VALUE_CONTAINER requires XRT_FEATURE_MAP"
#endif

#if defined(XRT_FEATURE_VALUE_CONTAINER) && !defined(XRT_FEATURE_SET)
	#error "XRT_FEATURE_VALUE_CONTAINER requires XRT_FEATURE_SET"
#endif

#if defined(XRT_FEATURE_VALUE_GRAPH) && !defined(XRT_FEATURE_VALUE_CONTAINER)
	#error "XRT_FEATURE_VALUE_GRAPH requires XRT_FEATURE_VALUE_CONTAINER"
#endif

#if defined(XRT_FEATURE_VALUE_COLLECTION) && !defined(XRT_FEATURE_VALUE_CONTAINER)
	#error "XRT_FEATURE_VALUE_COLLECTION requires XRT_FEATURE_VALUE_CONTAINER"
#endif



#define XRT_VALUE_DEPTH_MAX 256u



#if defined(XRT_FEATURE_VALUE)

/* 动态值类型保持紧凑稳定，语言运行时类型在独立模块扩展。 */
typedef enum xvaluetype {
	XVALUE_INVALID = -1,
	XVALUE_NULL = 0,
	XVALUE_BOOL,
	XVALUE_INT,
	XVALUE_FLOAT,
	XVALUE_STRING,
	XVALUE_BYTES,
	XVALUE_TIME,
	XVALUE_POINTER,
	XVALUE_HANDLE,
	XVALUE_ARRAY,
	XVALUE_INT_MAP,
	XVALUE_SET,
	XVALUE_OBJECT
} xvaluetype;



/* 动态值结构保持不透明，所有权通过 Retain、Release 和 Take 系列表达。 */
typedef struct xvalue xvalue;



/* 句柄克隆器创建独立句柄；失败时必须设置错误且不得在输出中遗留资源。 */
typedef bool (*xvaluehandleclone)(ptr pHandle, ptr* pClone, ptr pUserData);



/* 句柄释放器销毁 Value 独占的一个句柄。 */
typedef void (*xvaluehandledrop)(ptr pHandle, ptr pUserData);



/* 句柄哈希器必须与相等器成对提供、保持一致且不得重入父 Value。 */
typedef uint64 (*xvaluehandlehash)(ptr pHandle, ptr pUserData);



/* 句柄相等器只借用两个句柄，必须与哈希器成对提供且不得重入父 Value。 */
typedef bool (*xvaluehandleequal)(ptr pLeft, ptr pRight, ptr pUserData);



/* 句柄策略是静态不可变描述，其生命周期必须覆盖全部关联值。 */
typedef struct xvaluehandleops {
	xvaluehandleclone Clone;
	xvaluehandledrop Drop;
	xvaluehandlehash Hash;
	xvaluehandleequal Equal;
} xvaluehandleops;



XRT_EXTERN_C_BEGIN



/* 返回进程期不可变的 null 单例。 */
XRT_API xvalue* xrtValueNull(void);



/* 返回进程期不可变的布尔单例。 */
XRT_API xvalue* xrtValueBool(bool bValue);



/* 创建不可变的 64 位整数值。 */
XRT_API xvalue* xrtValueInt(int64 iValue);



/* 创建不可变的双精度浮点值。 */
XRT_API xvalue* xrtValueFloat(double fValue);



/* 复制字节并创建带末尾零但允许内嵌零的字符串值。 */
XRT_API xvalue* xrtValueString(xstrview Text);



/* 接管 XRT 字符串并清空独立来源槽；来源槽不得位于被接管内存中。 */
XRT_API xvalue* xrtValueStringTake(str* pText, size_t iSize);



/* 复制任意字节并创建二进制值。 */
XRT_API xvalue* xrtValueBytes(xbytesview Data);



/* 接管 XRT 二进制块并清空独立来源槽；来源槽不得位于被接管内存中。 */
XRT_API xvalue* xrtValueBytesTake(bytes* pData, size_t iSize);



/* 创建使用 Unix Epoch 微秒表示的时间值。 */
XRT_API xvalue* xrtValueTime(xtime Time);



/* 创建不拥有目标生命周期的裸指针值。 */
XRT_API xvalue* xrtValuePointer(ptr pPointer);



/* 接管句柄并清空独立来源槽；Hash 和 Equal 必须同时提供或同时省略。 */
XRT_API xvalue* xrtValueHandleTake(
	ptr* pHandle,
	const xvaluehandleops* pOps,
	ptr pUserData
);



/* 增加值外壳引用并返回原指针。 */
XRT_API xvalue* xrtValueRetain(const xvalue* pValue);



/* 释放值外壳引用，允许传入空指针。 */
XRT_API void xrtValueRelease(xvalue* pValue);



/* 标量增加引用，容器创建共享 backing 的独立 COW 外壳。 */
XRT_API xvalue* xrtValueClone(const xvalue* pValue);



/* 返回值类型，空指针返回 INVALID。 */
XRT_API xvaluetype xrtValueType(const xvalue* pValue);



/* 返回稳定的类型名称。 */
XRT_API cstr xrtValueTypeName(xvaluetype Type);



/* 判断值是否具有指定类型。 */
XRT_API bool xrtValueIs(const xvalue* pValue, xvaluetype Type);



/* 判断值是否为整数或浮点数。 */
XRT_API bool xrtValueIsNumber(const xvalue* pValue);



/* 判断值是否为四种基础容器之一。 */
XRT_API bool xrtValueIsContainer(const xvalue* pValue);



/* 按 xlang 语义返回值的真值。 */
XRT_API bool xrtValueTruthy(const xvalue* pValue);



/* 精确读取布尔值，类型不匹配时失败。 */
XRT_API bool xrtValueGetBool(const xvalue* pValue, bool* pResult);



/* 精确读取整数值，类型不匹配时失败。 */
XRT_API bool xrtValueGetInt(const xvalue* pValue, int64* pResult);



/* 精确读取浮点值，类型不匹配时失败。 */
XRT_API bool xrtValueGetFloat(const xvalue* pValue, double* pResult);



/* 借用字符串视图，值释放后视图失效。 */
XRT_API bool xrtValueGetString(const xvalue* pValue, xstrview* pResult);



/* 借用二进制视图，值释放后视图失效。 */
XRT_API bool xrtValueGetBytes(const xvalue* pValue, xbytesview* pResult);



/* 精确读取时间值。 */
XRT_API bool xrtValueGetTime(const xvalue* pValue, xtime* pResult);



/* 精确读取不拥有目标的裸指针。 */
XRT_API bool xrtValueGetPointer(const xvalue* pValue, ptr* pResult);



/* 借用句柄及其策略数据。 */
XRT_API bool xrtValueGetHandle(
	const xvalue* pValue,
	ptr* pHandle,
	const xvaluehandleops** pOps,
	ptr* pUserData
);



/* 为可哈希标量计算一致哈希；指针和句柄哈希只在当前进程内有效。 */
XRT_API bool xrtValueHash(const xvalue* pValue, uint64* pHash);



/* 按数值与标量内容判断相等；不可比较句柄和容器报告类型错误。 */
XRT_API bool xrtValueScalarEqual(
	const xvalue* pLeft,
	const xvalue* pRight
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_VALUE_CONTAINER)

#include <xrt/array.h>
#include <xrt/map.h>
#include <xrt/set.h>



/* 通用迭代键区分数组索引、稀疏整数键、对象字符串键和无键集合。 */
typedef enum xvaluekeytype {
	XVALUE_KEY_NONE = 0,
	XVALUE_KEY_INDEX,
	XVALUE_KEY_INT,
	XVALUE_KEY_STRING
} xvaluekeytype;



/* 迭代键只在下一次推进或迭代结束前有效。 */
typedef struct xvaluekey {
	xvaluekeytype Type;
	union {
		size_t Index;
		int64 Integer;
		xstrview String;
	};
} xvaluekey;



/* 迭代器持有 backing 快照；活动迭代器必须先 End 才能再次 Begin。 */
typedef struct xvalueiter {
	ptr Backing;
	xvaluetype Type;
	int Direction;
	size_t Index;
	union {
		xmapiter Map;
		xintmapiter IntMap;
		xsetiter Set;
	} State;
} xvalueiter;



/* 三态推进结果显式区分元素、正常结束和迭代错误。 */
typedef enum xvalueiterresult {
	XVALUE_ITER_ERROR = -1,
	XVALUE_ITER_END = 0,
	XVALUE_ITER_ITEM = 1
} xvalueiterresult;



XRT_EXTERN_C_BEGIN



/* 创建空的稠密动态值数组。 */
XRT_API xvalue* xrtValueArray(void);



/* 创建空的 int64 键稀疏映射。 */
XRT_API xvalue* xrtValueIntMap(void);



/* 创建空的可哈希动态值集合。 */
XRT_API xvalue* xrtValueSet(void);



/* 创建保持首次插入顺序的字符串键对象。 */
XRT_API xvalue* xrtValueObject(void);



/* 创建保持首次插入顺序、最终按逆插入顺序释放拥有值的字符串键对象。 */
XRT_API xvalue* xrtValueObjectLifo(void);



/* 返回任一基础容器的元素数。 */
XRT_API size_t xrtValueCount(const xvalue* pValue);



/* 保证容器至少可容纳指定数量的元素。 */
XRT_API bool xrtValueReserve(xvalue* pValue, size_t iCapacity);



/* 释放容器多余容量，保留现有元素。 */
XRT_API bool xrtValueTrim(xvalue* pValue);



/* 清空容器并释放其中持有的全部值引用。 */
XRT_API bool xrtValueClear(xvalue* pValue);



/* 把现有数组元素的正负索引解析为 0 基位置，失败时保持输出不变。 */
XRT_API bool xrtValueArrayResolve(
	const xvalue* pArray,
	int64 iIndex,
	size_t* pResolved
);



/* 返回数组指定 0 基索引处借用的值。 */
XRT_API xvalue* xrtValueArrayGet(const xvalue* pArray, size_t iIndex);



/* 支持负数倒序索引，越界时返回空指针。 */
XRT_API xvalue* xrtValueArrayAt(const xvalue* pArray, int64 iIndex);



/* 返回已经沿 COW 路径分离的可变子容器，标量子项报告类型错误。 */
XRT_API xvalue* xrtValueArrayEdit(xvalue* pArray, size_t iIndex);



/* 增加引用后向数组末尾加入值。 */
XRT_API bool xrtValueArrayAppend(xvalue* pArray, const xvalue* pItem);



/* 成功时把来源引用移交给数组并清空来源。 */
XRT_API bool xrtValueArrayAppendTake(xvalue* pArray, xvalue** pItem);



/* 无论成功失败都消费临时值，适合单行构造与加入。 */
XRT_API bool xrtValueArrayAppendNew(xvalue* pArray, xvalue* pItem);



/* 增加引用后在指定位置插入值。 */
XRT_API bool xrtValueArrayInsert(xvalue* pArray, size_t iIndex, const xvalue* pItem);



/* 成功时把来源引用移交到指定插入位置。 */
XRT_API bool xrtValueArrayInsertTake(xvalue* pArray, size_t iIndex, xvalue** pItem);



/* 无论成功失败都消费临时值并在指定位置插入。 */
XRT_API bool xrtValueArrayInsertNew(xvalue* pArray, size_t iIndex, xvalue* pItem);



/* 增加引用后替换旧值；同一指针是引用平衡的成功无操作。 */
XRT_API bool xrtValueArraySet(xvalue* pArray, size_t iIndex, const xvalue* pItem);



/* 成功时把来源引用移交到指定位置。 */
XRT_API bool xrtValueArraySetTake(xvalue* pArray, size_t iIndex, xvalue** pItem);



/* 无论成功失败都消费临时值并替换指定位置。 */
XRT_API bool xrtValueArraySetNew(xvalue* pArray, size_t iIndex, xvalue* pItem);



/* 删除数组区间并释放其中的值。 */
XRT_API bool xrtValueArrayRemove(xvalue* pArray, size_t iIndex, size_t iCount);



/* 从数组移交指定值，调用方获得一个引用。 */
XRT_API xvalue* xrtValueArrayTake(xvalue* pArray, size_t iIndex);



/* 从数组末尾移交一个值。 */
XRT_API xvalue* xrtValueArrayPop(xvalue* pArray);



/* 交换两个数组元素。 */
XRT_API bool xrtValueArraySwap(xvalue* pArray, size_t iLeft, size_t iRight);



/* 返回稀疏整数键借用的值，缺失是正常结果。 */
XRT_API xvalue* xrtValueIntMapGet(const xvalue* pMap, int64 iKey);



/* 返回已经沿 COW 路径分离的可变子容器，标量子项报告类型错误。 */
XRT_API xvalue* xrtValueIntMapEdit(xvalue* pMap, int64 iKey);



/* 增加引用后设置整数键值；同一指针是引用平衡的成功无操作。 */
XRT_API bool xrtValueIntMapSet(xvalue* pMap, int64 iKey, const xvalue* pItem);



/* 成功时把来源引用移交到整数键。 */
XRT_API bool xrtValueIntMapSetTake(xvalue* pMap, int64 iKey, xvalue** pItem);



/* 无论成功失败都消费临时值并设置整数键。 */
XRT_API bool xrtValueIntMapSetNew(xvalue* pMap, int64 iKey, xvalue* pItem);



/* 判断整数键是否存在。 */
XRT_API bool xrtValueIntMapHas(const xvalue* pMap, int64 iKey);



/* 删除整数键并释放对应值。 */
XRT_API bool xrtValueIntMapRemove(xvalue* pMap, int64 iKey);



/* 移交整数键对应值，缺失时返回空指针。 */
XRT_API xvalue* xrtValueIntMapTake(xvalue* pMap, int64 iKey);



/* 返回对象字符串键借用的值，键按完整字节匹配。 */
XRT_API xvalue* xrtValueObjectGet(const xvalue* pObject, xstrview Key);



/* 按首次插入顺序返回借用的键和值；替换已有键不会改变顺序。 */
XRT_API xvalue* xrtValueObjectAt(
	const xvalue* pObject,
	size_t iIndex,
	xstrview* pKey
);



/* 返回已经沿 COW 路径分离的可变子容器，标量子项报告类型错误。 */
XRT_API xvalue* xrtValueObjectEdit(xvalue* pObject, xstrview Key);



/* 增加引用后设置对象键值；同一指针不分离且保留首次键位置。 */
XRT_API bool xrtValueObjectSet(
	xvalue* pObject,
	xstrview Key,
	const xvalue* pItem
);



/* 成功时把来源引用移交到对象键。 */
XRT_API bool xrtValueObjectSetTake(
	xvalue* pObject,
	xstrview Key,
	xvalue** pItem
);



/* 无论成功失败都消费临时值并设置对象键。 */
XRT_API bool xrtValueObjectSetNew(
	xvalue* pObject,
	xstrview Key,
	xvalue* pItem
);



/* 判断对象键是否存在。 */
XRT_API bool xrtValueObjectHas(const xvalue* pObject, xstrview Key);



/* 删除对象键并释放对应值。 */
XRT_API bool xrtValueObjectRemove(xvalue* pObject, xstrview Key);



/* 移交对象键对应值，缺失时返回空指针。 */
XRT_API xvalue* xrtValueObjectTake(xvalue* pObject, xstrview Key);



/* 增加引用后把可哈希标量加入集合。 */
XRT_API bool xrtValueSetAdd(xvalue* pSet, const xvalue* pItem);



/* 成功时消费来源引用；重复元素同样视为成功。 */
XRT_API bool xrtValueSetAddTake(xvalue* pSet, xvalue** pItem);



/* 无论成功失败都消费临时值并尝试加入集合。 */
XRT_API bool xrtValueSetAddNew(xvalue* pSet, xvalue* pItem);



/* 判断等价值是否在集合中。 */
XRT_API bool xrtValueSetHas(const xvalue* pSet, const xvalue* pItem);



/* 删除等价值并释放集合持有的引用。 */
XRT_API bool xrtValueSetRemove(xvalue* pSet, const xvalue* pItem);



/* 移交集合中的规范值，缺失时返回空指针。 */
XRT_API xvalue* xrtValueSetTake(xvalue* pSet, const xvalue* pItem);



/* 启动稳定顺序快照；输出不得覆盖 Value，且不能已处于活动状态。 */
XRT_API bool xrtValueIterBegin(const xvalue* pValue, xvalueiter* pIterator);



/* 启动稳定逆序快照；输出不得覆盖 Value，且不能已处于活动状态。 */
XRT_API bool xrtValueIterRBegin(const xvalue* pValue, xvalueiter* pIterator);



/* 创建按稳定正序推进的拥有式快照迭代器；调用方必须 Destroy。 */
XRT_API xvalueiter* xrtValueIterCreate(const xvalue* pValue);



/* 创建按稳定逆序推进的拥有式快照迭代器；调用方必须 Destroy。 */
XRT_API xvalueiter* xrtValueIterRCreate(const xvalue* pValue);



/* 返回下一借用值及其键；键输出不得覆盖迭代器，正常结束返回空指针。 */
XRT_API xvalue* xrtValueIterNext(xvalueiter* pIterator, xvaluekey* pKey);



/* 三态推进快照；成功项写入借用值，正常结束不设置错误。 */
XRT_API xvalueiterresult xrtValueIterAdvance(
	xvalueiter* pIterator,
	xvaluekey* pKey,
	xvalue** ppValue
);



/* 结束迭代并释放 backing 快照。 */
XRT_API void xrtValueIterEnd(xvalueiter* pIterator);



/* 结束并释放拥有式迭代器；允许传入空指针。 */
XRT_API void xrtValueIterDestroy(xvalueiter* pIterator);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_VALUE_COLLECTION)

/* 映射批量合并时对已有键采用明确且互斥的处理策略。 */
typedef enum xvaluemergepolicy {
	XVALUE_MERGE_KEEP = 0,
	XVALUE_MERGE_REPLACE,
	XVALUE_MERGE_ERROR
} xvaluemergepolicy;



XRT_EXTERN_C_BEGIN



/* 失败原子地把来源数组全部追加到目标数组，允许来源与目标相同。 */
XRT_API bool xrtValueArrayExtend(
	xvalue* pTarget,
	const xvalue* pSource
);



/* 创建按左右顺序连接的新数组。 */
XRT_API xvalue* xrtValueArrayConcat(
	const xvalue* pLeft,
	const xvalue* pRight
);



/* 按冲突策略失败原子地合并两个整数键映射。 */
XRT_API bool xrtValueIntMapMerge(
	xvalue* pTarget,
	const xvalue* pSource,
	xvaluemergepolicy Policy
);



/* 按冲突策略失败原子地合并两个对象，并保留目标已有键位置。 */
XRT_API bool xrtValueObjectMerge(
	xvalue* pTarget,
	const xvalue* pSource,
	xvaluemergepolicy Policy
);



/* 失败原子地把来源集合中的缺失元素追加到目标集合。 */
XRT_API bool xrtValueSetMerge(
	xvalue* pTarget,
	const xvalue* pSource
);



/* 创建两个集合的并集，结果先保持左集合顺序。 */
XRT_API xvalue* xrtValueSetUnion(
	const xvalue* pLeft,
	const xvalue* pRight
);



/* 创建两个集合的交集，结果保持左集合顺序。 */
XRT_API xvalue* xrtValueSetIntersection(
	const xvalue* pLeft,
	const xvalue* pRight
);



/* 创建左集合相对右集合的差集。 */
XRT_API xvalue* xrtValueSetDifference(
	const xvalue* pLeft,
	const xvalue* pRight
);



/* 创建两个集合的对称差集，右侧独有元素追加在左侧独有元素之后。 */
XRT_API xvalue* xrtValueSetSymmetricDifference(
	const xvalue* pLeft,
	const xvalue* pRight
);



/* 判断左集合是否为右集合的子集，可选择严格子集。 */
XRT_API bool xrtValueSetIsSubset(
	const xvalue* pLeft,
	const xvalue* pRight,
	bool bProper
);



/* 判断左集合是否为右集合的超集，可选择严格超集。 */
XRT_API bool xrtValueSetIsSuperset(
	const xvalue* pLeft,
	const xvalue* pRight,
	bool bProper
);



/* 判断两个集合是否没有任何共同元素。 */
XRT_API bool xrtValueSetIsDisjoint(
	const xvalue* pLeft,
	const xvalue* pRight
);



/* 判断两个集合是否拥有相同的标量元素。 */
XRT_API bool xrtValueSetEqual(
	const xvalue* pLeft,
	const xvalue* pRight
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_VALUE_GRAPH)

XRT_EXTERN_C_BEGIN



/*
	深度复制完整无环值图，并保留重复子值的共享身份。
	不可变标量只增加引用，Handle 必须提供 Clone 策略。
*/
XRT_API xvalue* xrtValueDeepClone(const xvalue* pValue);



/*
	按数值和容器内容递归判断结构相等，不要求两侧共享拓扑一致。
	两个不同 Handle 只有在同一策略域提供 Equal 时才可比较。
*/
XRT_API bool xrtValueEqual(const xvalue* pLeft, const xvalue* pRight);



XRT_EXTERN_C_END

#endif

#endif
