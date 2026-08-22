#ifndef XRT_TYPED_VALUE_H
#define XRT_TYPED_VALUE_H

#include <xrt/runtime_type.h>
#include <xrt/value.h>

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)
	#include <xrt/runtime_type_string.h>
#endif

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)
	#include <xrt/typed_array.h>
#endif

#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)
	#include <xrt/typed_list.h>
#endif

#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)
	#include <xrt/typed_set.h>
#endif

#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)
	#include <xrt/typed_dict.h>
#endif



#if defined(XRUNTIME_FEATURE_TYPED_VALUE) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_VALUE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_VALUE) && !defined(XRT_FEATURE_VALUE)
	#error "XRUNTIME_FEATURE_TYPED_VALUE requires XRT_FEATURE_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE) && \
	(!defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) || \
	 !defined(XRUNTIME_FEATURE_TYPED_VALUE))
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE requires RUNTIME_TYPE_STRING and TYPED_VALUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) && \
	(!defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	 !defined(XRUNTIME_FEATURE_TYPED_ARRAY) || \
	 !defined(XRT_FEATURE_VALUE_CONTAINER))
	#error "XRUNTIME_FEATURE_TYPED_ARRAY_VALUE requires TYPED_VALUE, TYPED_ARRAY and VALUE_CONTAINER"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) && \
	(!defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	 !defined(XRUNTIME_FEATURE_TYPED_LIST) || \
	 !defined(XRT_FEATURE_VALUE_CONTAINER))
	#error "XRUNTIME_FEATURE_TYPED_LIST_VALUE requires TYPED_VALUE, TYPED_LIST and VALUE_CONTAINER"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) && \
	(!defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	 !defined(XRUNTIME_FEATURE_TYPED_SET) || \
	 !defined(XRT_FEATURE_VALUE_CONTAINER))
	#error "XRUNTIME_FEATURE_TYPED_SET_VALUE requires TYPED_VALUE, TYPED_SET and VALUE_CONTAINER"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE) && \
	(!defined(XRUNTIME_FEATURE_TYPED_VALUE) || \
	 !defined(XRUNTIME_FEATURE_TYPED_DICT) || \
	 !defined(XRT_FEATURE_VALUE_CONTAINER))
	#error "XRUNTIME_FEATURE_TYPED_DICT_VALUE requires TYPED_VALUE, TYPED_DICT and VALUE_CONTAINER"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_VALUE)

/* 动态值转换层稳定错误代码。 */
typedef enum xtypedvalueerror {
	XTYPED_VALUE_ERROR_ARGUMENT = 1,
	XTYPED_VALUE_ERROR_TYPE,
	XTYPED_VALUE_ERROR_RANGE,
	XTYPED_VALUE_ERROR_CONVERT,
	XTYPED_VALUE_ERROR_CONTAINER
} xtypedvalueerror;



/* 自定义解码器把动态值写入已经初始化的目标，失败时目标仍须可安全销毁。 */
typedef bool (*xvaluetotyped)(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	ptr pContext
);



/* 自定义编码器借用类型值，并返回一个由调用方拥有的动态值。 */
typedef xvalue* (*xvaluefromtyped)(
	const xrttype* pSourceType,
	const void* pSource,
	ptr pContext
);



/* 转换器只借用回调与上下文，允许按应用协议扩展记录、字符串和句柄类型。 */
typedef struct xvalueconverter {
	ptr Context;
	xvaluetotyped ToTyped;
	xvaluefromtyped FromTyped;
} xvalueconverter;



XRT_EXTERN_C_BEGIN



/* 把动态值安全转换为一个新初始化的运行时类型值。 */
XRT_API bool xrtValueToTyped(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	const xvalueconverter* pConverter
);



/* 把运行时类型值转换为一个独立动态值。 */
XRT_API xvalue* xrtValueFromTyped(
	const xrttype* pSourceType,
	const void* pSource,
	const xvalueconverter* pConverter
);



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)

/* 把一个动态值转换后追加到类型数组。 */
XRT_API bool xrtTypedArrayPushValue(
	xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把一个动态值转换后插入类型数组的指定下标。 */
XRT_API bool xrtTypedArrayInsertValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把一个动态值转换后原子替换类型数组的指定元素。 */
XRT_API bool xrtTypedArraySetValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把类型数组的指定元素转换为调用方拥有的动态值。 */
XRT_API xvalue* xrtTypedArrayGetValue(
	const xtypedarray* pArray,
	size_t iIndex,
	const xvalueconverter* pConverter
);



/* 转换并删除类型数组的指定元素；转换失败时数组保持不变。 */
XRT_API xvalue* xrtTypedArrayTakeValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalueconverter* pConverter
);



/* 转换并删除类型数组的末尾元素；转换失败时数组保持不变。 */
XRT_API xvalue* xrtTypedArrayPopValue(
	xtypedarray* pArray,
	const xvalueconverter* pConverter
);



/* 把动态值转换为元素类型并查找第一处相等元素。 */
XRT_API size_t xrtTypedArrayFindValue(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 判断类型数组是否包含与动态值等价的元素。 */
XRT_API bool xrtTypedArrayContainsValue(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把动态稠密数组深转换为独立的同构类型数组。 */
XRT_API xtypedarray* xrtTypedArrayFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
);



/* 把类型数组深转换为独立的动态稠密数组。 */
XRT_API xvalue* xrtTypedArrayToValue(
	const xtypedarray* pArray,
	const xvalueconverter* pConverter
);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)

/* 把一个动态值转换后写入类型列表的指定整数键。 */
XRT_API bool xrtTypedListSetValue(
	xtypedlist* pList,
	int64 iKey,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把一个动态值转换后追加到最大键之后。 */
XRT_API bool xrtTypedListAppendValue(
	xtypedlist* pList,
	const xvalue* pValue,
	int64* pKey,
	const xvalueconverter* pConverter
);



/* 把指定整数键的类型值转换为调用方拥有的动态值。 */
XRT_API xvalue* xrtTypedListGetValue(
	const xtypedlist* pList,
	int64 iKey,
	const xvalueconverter* pConverter
);



/* 转换并删除指定整数键的类型值；转换失败时列表保持不变。 */
XRT_API xvalue* xrtTypedListTakeValue(
	xtypedlist* pList,
	int64 iKey,
	const xvalueconverter* pConverter
);



/* 把动态值转换为元素类型并查找第一处相等值。 */
XRT_API bool xrtTypedListFindValue(
	const xtypedlist* pList,
	const xvalue* pValue,
	int64* pKey,
	const xvalueconverter* pConverter
);



/* 判断类型列表是否包含与动态值等价的元素。 */
XRT_API bool xrtTypedListContainsValue(
	const xtypedlist* pList,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把动态整数映射深转换为独立的同构稀疏类型列表。 */
XRT_API xtypedlist* xrtTypedListFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
);



/* 把稀疏类型列表深转换为独立的动态整数映射。 */
XRT_API xvalue* xrtTypedListToValue(
	const xtypedlist* pList,
	const xvalueconverter* pConverter
);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)

/* 把一个动态值转换后加入类型集合。 */
XRT_API bool xrtTypedSetAddValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 返回与动态值等价的规范元素副本。 */
XRT_API xvalue* xrtTypedSetGetValue(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 判断类型集合是否拥有与动态值等价的元素。 */
XRT_API bool xrtTypedSetHasValue(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 删除与动态值等价的元素。 */
XRT_API bool xrtTypedSetRemoveValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 转换并删除规范元素；转换失败时集合保持不变。 */
XRT_API xvalue* xrtTypedSetTakeValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把动态集合深转换为独立的同构类型集合。 */
XRT_API xtypedset* xrtTypedSetFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
);



/* 把类型集合深转换为独立的动态集合。 */
XRT_API xvalue* xrtTypedSetToValue(
	const xtypedset* pSet,
	const xvalueconverter* pConverter
);

#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 把一个动态值转换后写入类型字典的指定文本键。 */
XRT_API bool xrtTypedDictSetValue(
	xtypeddict* pDict,
	xstrview Key,
	const xvalue* pValue,
	const xvalueconverter* pConverter
);



/* 把指定文本键的类型值转换为调用方拥有的动态值。 */
XRT_API xvalue* xrtTypedDictGetValue(
	const xtypeddict* pDict,
	xstrview Key,
	const xvalueconverter* pConverter
);



/* 转换并删除指定文本键的类型值；转换失败时字典保持不变。 */
XRT_API xvalue* xrtTypedDictTakeValue(
	xtypeddict* pDict,
	xstrview Key,
	const xvalueconverter* pConverter
);



/* 把动态字符串键对象深转换为独立的同构类型字典。 */
XRT_API xtypeddict* xrtTypedDictFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
);



/* 把类型字典深转换为独立的动态字符串键对象。 */
XRT_API xvalue* xrtTypedDictToValue(
	const xtypeddict* pDict,
	const xvalueconverter* pConverter
);

#endif



XRT_EXTERN_C_END

#endif

#endif
