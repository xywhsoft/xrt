#include "../internal/xrt_runtime_type.h"
#include "../internal/xrt_typed_container.h"
#include <xrt/typed_value.h>

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)
	#include <xrt/runtime_type_string.h>
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
	#include <xrt/runtime_value.h>
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

#include <float.h>



#if defined(XRUNTIME_FEATURE_TYPED_VALUE)

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

#define XRT_TYPED_VALUE_SCRATCH_INLINE_SIZE 64u



typedef union __xrt_typed_value_inline {
	long double Float;
	ptr Pointer;
	uint64 Integer;
	void (*Function)(void);
	uint8 Data[XRT_TYPED_VALUE_SCRATCH_INLINE_SIZE];
} __xrt_typed_value_inline;



typedef struct __xrt_typed_value_scratch {
	ptr Allocation;
	ptr Value;
	__xrt_typed_value_inline Inline;
} __xrt_typed_value_scratch;

#endif



/* 设置动态值转换层结构化错误。 */
static void __xrtTypedValueError(
	xerrkind Kind,
	xtypedvalueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-value";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为类型、Value 或用户转换器错误补充转换上下文。 */
static void __xrtTypedValueWrap(
	xerrkind DefaultKind,
	xtypedvalueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-value";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 检查转换参数和运行时类型描述。 */
static bool __xrtTypedValueArguments(
	const xrttype* pType,
	const void* pValue,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_TYPE,
			sOperation, "the runtime type is invalid");
		return false;
	}
	if ( (pValue == NULL) && (pType->Size != 0u) ) {
		__xrtTypedValueError(XERR_ARGUMENT, XTYPED_VALUE_ERROR_ARGUMENT,
			sOperation, "the typed value storage is null");
		return false;
	}
	return true;
}



/* 在清理资源后恢复进入清理阶段时持有的根错误。 */
static void __xrtTypedValueRestoreError(xerror* pError)
{
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 按类型对齐申请一个可重复使用的临时值槽。 */
static bool __xrtTypedValueScratchCreate(
	const xrttype* pType,
	__xrt_typed_value_scratch* pScratch
)
{
	size_t iSize;
	uintptr_t iAddress;

	memset(pScratch, 0, sizeof(*pScratch));
	if ( (pType->Size <= XRT_TYPED_VALUE_SCRATCH_INLINE_SIZE) &&
		 (pType->Align <= XRT_INTERNAL_ALIGNOF(__xrt_typed_value_inline)) ) {
		pScratch->Value = pScratch->Inline.Data;
		return true;
	}
	if ( pType->Size > (SIZE_MAX - (pType->Align - 1u)) ) {
		__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
			"scratch", "the aligned typed value size overflows");
		return false;
	}
	iSize = pType->Size + (pType->Align - 1u);
	pScratch->Allocation = xrtMalloc(iSize != 0u ? iSize : 1u);
	if ( pScratch->Allocation == NULL ) {
		return false;
	}
	iAddress = (uintptr_t)pScratch->Allocation;
	pScratch->Value = (ptr)((iAddress + (pType->Align - 1u)) &
		~((uintptr_t)pType->Align - 1u));
	return true;
}



/* 释放临时值槽，不改变当前错误。 */
static void __xrtTypedValueScratchDestroy(
	__xrt_typed_value_scratch* pScratch
)
{
	xerror* pError = xrtTakeError();

	if ( pScratch->Allocation != NULL ) {
		xrtFree(pScratch->Allocation);
	}
	memset(pScratch, 0, sizeof(*pScratch));
	__xrtTypedValueRestoreError(pError);
}



/* 结束动态容器快照迭代，同时保留根错误。 */
static void __xrtTypedValueIteratorEnd(xvalueiter* pIterator)
{
	xerror* pError = xrtTakeError();

	xrtValueIterEnd(pIterator);
	__xrtTypedValueRestoreError(pError);
}



/* 释放动态值结果，同时保留根错误。 */
static void __xrtTypedValueRelease(xvalue* pValue)
{
	xerror* pError = xrtTakeError();

	xrtValueRelease(pValue);
	__xrtTypedValueRestoreError(pError);
}

#endif



/* 销毁一个已初始化临时值，并保留进入函数时的错误。 */
static void __xrtTypedValueDropPreserveError(
	const xrttype* pType,
	ptr pValue
)
{
	xerror* pError = xrtTakeError();

	xrtTypeDropValue(pType, pValue);
	__xrtTypedValueRestoreError(pError);
}



/* 使用 XRT 内建安全标量规则解码动态值。 */
static bool __xrtTypedValueDecodeBuiltin(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	bool* pHandled
)
{
	int64 iInteger;
	double fNumber;

	*pHandled = true;
	switch ( pTargetType->Kind ) {
		case XRT_TYPE_NULL:
			if ( xrtValueType(pSource) != XVALUE_NULL ) {
				break;
			}
			return true;
		case XRT_TYPE_BOOL: {
			bool bValue;

			if ( xrtValueGetBool(pSource, &bValue) &&
				 __xrtTypeWriteBool(
					bValue, pTargetType->Size, pTarget
				 ) ) {
				return true;
			}
			break;
		}
		case XRT_TYPE_SIGNED_INT:
			if ( !xrtValueGetInt(pSource, &iInteger) ) {
				break;
			}
			if ( !__xrtTypeWriteSigned(
				iInteger, pTargetType->Size, pTarget
			) ) {
				__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
					"to-typed", "the integer exceeds the target signed range");
				return false;
			}
			return true;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( !xrtValueGetInt(pSource, &iInteger) ) {
				break;
			}
			if ( (iInteger < 0) || !__xrtTypeWriteUnsigned(
				(uint64)iInteger, pTargetType->Size, pTarget
			) ) {
				__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
					"to-typed", "the integer exceeds the target unsigned range");
				return false;
			}
			return true;
		case XRT_TYPE_FLOAT:
			if ( !xrtValueGetFloat(pSource, &fNumber) ) {
				break;
			}
			if ( !__xrtTypeWriteFloat(
				fNumber, pTargetType->Size, true, pTarget
			) ) {
				__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
					"to-typed", "the float is not losslessly representable");
				return false;
			}
			return true;
		case XRT_TYPE_TIME: {
			xtime Time;

			if ( (pTargetType->Size == sizeof(Time)) &&
				 xrtValueGetTime(pSource, &Time) ) {
				memcpy(pTarget, &Time, sizeof(Time));
				return true;
			}
			break;
		}
		case XRT_TYPE_POINTER: {
			ptr pPointer;

			if ( (pTargetType->Size == sizeof(pPointer)) &&
				 xrtValueGetPointer(pSource, &pPointer) ) {
				memcpy(pTarget, &pPointer, sizeof(pPointer));
				return true;
			}
			break;
		}
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)
		case XRT_TYPE_CALLABLE: {
			xrtcallable* pCallable;

			if ( pTargetType->Size != sizeof(pCallable) ) {
				break;
			}
			if ( xrtValueType(pSource) == XVALUE_NULL ) {
				return true;
			}
			if ( !xrtValueIsCallable(pSource) ) {
				break;
			}
			pCallable = xrtValueGetCallable(pSource);
			return (pCallable != NULL) && xrtTypeCopyValue(
				pTargetType, pTarget, &pCallable
			);
		}
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
		case XRT_TYPE_FUTURE: {
			xfuture* pFuture;

			if ( pTargetType->Size != sizeof(pFuture) ) {
				break;
			}
			if ( xrtValueType(pSource) == XVALUE_NULL ) {
				return true;
			}
			if ( !xrtValueIsFuture(pSource) ) {
				break;
			}
			pFuture = xrtValueGetFuture(pSource);
			return (pFuture != NULL) && xrtTypeCopyValue(
				pTargetType, pTarget, &pFuture
			);
		}
#endif
		default:
			*pHandled = false;
			return false;
	}
	__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_TYPE,
		"to-typed", "the dynamic value cannot be represented by the target type");
	return false;
}



/* 使用 XRT 内建安全标量规则编码类型值。 */
static xvalue* __xrtTypedValueEncodeBuiltin(
	const xrttype* pSourceType,
	const void* pSource,
	bool* pHandled
)
{
	int64 iInteger;
	uint64 iUnsigned;
	double fNumber;

	*pHandled = true;
	switch ( pSourceType->Kind ) {
		case XRT_TYPE_NULL:
			return xrtValueNull();
		case XRT_TYPE_BOOL: {
			bool bValue;

			if ( __xrtTypeReadBool(
				pSource, pSourceType->Size, &bValue
			) ) {
				return xrtValueBool(bValue);
			}
			break;
		}
		case XRT_TYPE_SIGNED_INT:
			if ( __xrtTypeReadSigned(
				pSource, pSourceType->Size, &iInteger
			) ) {
				return xrtValueInt(iInteger);
			}
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( __xrtTypeReadUnsigned(
				pSource, pSourceType->Size, &iUnsigned
			) && (iUnsigned <= INT64_MAX) ) {
				return xrtValueInt((int64)iUnsigned);
			}
			break;
		case XRT_TYPE_FLOAT:
			if ( __xrtTypeReadFloat(
				pSource, pSourceType->Size, &fNumber
			) ) {
				return xrtValueFloat(fNumber);
			}
			break;
		case XRT_TYPE_TIME: {
			xtime Time;

			if ( pSourceType->Size == sizeof(Time) ) {
				memcpy(&Time, pSource, sizeof(Time));
				return xrtValueTime(Time);
			}
			break;
		}
		case XRT_TYPE_POINTER: {
			ptr pPointer;

			if ( pSourceType->Size == sizeof(pPointer) ) {
				memcpy(&pPointer, pSource, sizeof(pPointer));
				return xrtValuePointer(pPointer);
			}
			break;
		}
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)
		case XRT_TYPE_CALLABLE: {
			xrtcallable* pCallable;

			if ( pSourceType->Size != sizeof(pCallable) ) {
				break;
			}
			memcpy(&pCallable, pSource, sizeof(pCallable));
			return pCallable != NULL ?
				xrtValueCallable(pCallable) : xrtValueNull();
		}
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
		case XRT_TYPE_FUTURE: {
			xfuture* pFuture;

			if ( pSourceType->Size != sizeof(pFuture) ) {
				break;
			}
			memcpy(&pFuture, pSource, sizeof(pFuture));
			return pFuture != NULL ?
				xrtValueFuture(pFuture) : xrtValueNull();
		}
#endif
		default:
			*pHandled = false;
			return NULL;
	}
	__xrtTypedValueError(XERR_RANGE, XTYPED_VALUE_ERROR_RANGE,
		"from-typed", "the typed value cannot be represented by a dynamic value");
	return NULL;
}



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)

/* 把动态字符串复制到规范拥有型 str 槽。 */
static bool __xrtTypedValueDecodeString(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	bool* pHandled
)
{
	xstrview Text;
	str sResult;

	*pHandled = xrtTypeSame(pTargetType, xrtTypeString());
	if ( !*pHandled ) {
		return false;
	}
	if ( !xrtValueGetString(pSource, &Text) ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_TYPE,
			"to-typed", "the dynamic value is not a string");
		return false;
	}
	if ( (Text.Size != 0u) &&
		 (memchr(Text.Data, 0, Text.Size) != NULL) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			"to-typed", "an owned str cannot represent embedded zero bytes");
		return false;
	}
	sResult = xrtStrDupView(Text);
	if ( sResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONVERT,
			"to-typed", "the dynamic string could not be copied");
		return false;
	}
	memcpy(pTarget, &sResult, sizeof(sResult));
	return true;
}



/* 把规范拥有型 str 槽复制为独立动态字符串。 */
static xvalue* __xrtTypedValueEncodeString(
	const xrttype* pSourceType,
	const void* pSource,
	bool* pHandled
)
{
	str sSource;

	*pHandled = xrtTypeSame(pSourceType, xrtTypeString());
	if ( !*pHandled ) {
		return NULL;
	}
	memcpy(&sSource, pSource, sizeof(sSource));
	return xrtValueString(xrtStrView(sSource));
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)

/* 深复制动态来源到运行时 Value 所有权槽。 */
static bool __xrtTypedValueDecodeRuntimeValue(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	bool* pHandled
)
{
	xvalue* pBorrowed = (xvalue*)pSource;

	*pHandled = xrtTypeSame(pTargetType, xrtTypeValue());
	if ( !*pHandled ) {
		return false;
	}
	if ( !xrtTypeCloneValue(pTargetType, pTarget, &pBorrowed) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONVERT,
			"to-typed", "the dynamic Value graph could not be copied");
		return false;
	}
	return true;
}



/* 从运行时 Value 所有权槽深复制独立动态值。 */
static xvalue* __xrtTypedValueEncodeRuntimeValue(
	const xrttype* pSourceType,
	const void* pSource
)
{
	xvalue* pOwned;
	xvalue* pResult;

	if ( !xrtTypeSame(pSourceType, xrtTypeValue()) ) {
		return NULL;
	}
	memcpy(&pOwned, pSource, sizeof(pOwned));
	if ( pOwned == NULL ) {
		__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONVERT,
			"from-typed", "the runtime Value ownership slot is empty");
		return NULL;
	}
	pResult = xrtValueDeepClone(pOwned);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONVERT,
			"from-typed", "the runtime Value graph could not be copied");
	}
	return pResult;
}

#endif



/* 把动态值转换为一个新初始化的运行时类型值。 */
XRT_API bool xrtValueToTyped(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	const xvalueconverter* pConverter
)
{
	bool bHandled;
	bool bResult;

	if ( pSource == NULL ) {
		__xrtTypedValueError(XERR_ARGUMENT, XTYPED_VALUE_ERROR_ARGUMENT,
			"to-typed", "the source dynamic value is null");
		return false;
	}
	if ( !__xrtTypedValueArguments(pTargetType, pTarget, "to-typed") ) {
		return false;
	}
	if ( !xrtTypeInitValue(pTargetType, pTarget) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONVERT,
			"to-typed", "the target typed value could not be initialized");
		return false;
	}
	bHandled = false;
	bResult = false;
#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)
	bResult = __xrtTypedValueDecodeString(
		pSource, pTargetType, pTarget, &bHandled
	);
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)
	if ( !bHandled ) {
		bResult = __xrtTypedValueDecodeRuntimeValue(
			pSource, pTargetType, pTarget, &bHandled
		);
	}
#endif
	if ( !bHandled ) {
		bResult = __xrtTypedValueDecodeBuiltin(
			pSource, pTargetType, pTarget, &bHandled
		);
	}
	if ( !bHandled && (pConverter != NULL) &&
		 (pConverter->ToTyped != NULL) ) {
		bResult = pConverter->ToTyped(
			pSource, pTargetType, pTarget, pConverter->Context
		);
		if ( !bResult ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"to-typed", "the custom dynamic value decoder failed");
		}
	} else if ( !bHandled ) {
		__xrtTypedValueError(XERR_UNSUPPORTED, XTYPED_VALUE_ERROR_TYPE,
			"to-typed", "the target type requires a custom Value converter");
	}
	if ( !bResult ) {
		__xrtTypedValueDropPreserveError(pTargetType, pTarget);
		return false;
	}
	return true;
}



/* 把运行时类型值转换为一个独立动态值。 */
XRT_API xvalue* xrtValueFromTyped(
	const xrttype* pSourceType,
	const void* pSource,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult;
	bool bHandled;

	if ( !__xrtTypedValueArguments(pSourceType, pSource, "from-typed") ) {
		return NULL;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING_VALUE)
	pResult = __xrtTypedValueEncodeString(
		pSourceType, pSource, &bHandled
	);
	if ( bHandled ) {
		return pResult;
	}
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)
	if ( xrtTypeSame(pSourceType, xrtTypeValue()) ) {
		return __xrtTypedValueEncodeRuntimeValue(pSourceType, pSource);
	}
#endif
	pResult = __xrtTypedValueEncodeBuiltin(
		pSourceType, pSource, &bHandled
	);
	if ( bHandled ) {
		return pResult;
	}
	if ( (pConverter == NULL) || (pConverter->FromTyped == NULL) ) {
		__xrtTypedValueError(XERR_UNSUPPORTED, XTYPED_VALUE_ERROR_TYPE,
			"from-typed", "the source type requires a custom Value converter");
		return NULL;
	}
	pResult = pConverter->FromTyped(
		pSourceType, pSource, pConverter->Context
	);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			"from-typed", "the custom dynamic value encoder failed");
	}
	return pResult;
}



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 把一个动态值解码到新初始化的临时值槽。 */
static bool __xrtTypedValueScratchDecode(
	const xrttype* pType,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedValueError(XERR_ARGUMENT, XTYPED_VALUE_ERROR_ARGUMENT,
			sOperation, "the dynamic item is null");
		return false;
	}
	if ( !__xrtTypedValueScratchCreate(pType, pScratch) ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the temporary typed item could not be allocated");
		return false;
	}
	if ( !xrtValueToTyped(pValue, pType, pScratch->Value, pConverter) ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the dynamic item could not be converted");
		__xrtTypedValueScratchDestroy(pScratch);
		return false;
	}
	return true;
}



/* 销毁临时类型值和对齐存储，同时保留进入清理阶段时的错误。 */
static void __xrtTypedValueScratchDrop(
	const xrttype* pType,
	__xrt_typed_value_scratch* pScratch
)
{
	__xrtTypedValueDropPreserveError(pType, pScratch->Value);
	__xrtTypedValueScratchDestroy(pScratch);
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)

/* 在数组回调门内把动态值解码为临时元素。 */
static bool __xrtTypedValueArrayDecode(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedArrayItemType(pArray);
	bool bResult;

	if ( pType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed array is invalid");
		return false;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	bResult = __xrtTypedValueScratchDecode(
		pType, pValue, pConverter, pScratch, sOperation
	);
	__xrtTypedArrayCallbackEnd(pArray);
	return bResult;
}



/* 在数组回调门内把借用元素编码为独立动态值。 */
static xvalue* __xrtTypedValueArrayEncode(
	const xtypedarray* pArray,
	const void* pItem,
	const xvalueconverter* pConverter,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedArrayItemType(pArray);
	xvalue* pResult;

	__xrtTypedArrayCallbackBegin(pArray);
	pResult = xrtValueFromTyped(pType, pItem, pConverter);
	__xrtTypedArrayCallbackEnd(pArray);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the typed array item could not be converted");
	}
	return pResult;
}



/* 把一个动态值转换后追加到类型数组。 */
XRT_API bool xrtTypedArrayPushValue(
	xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueArrayDecode(
		pArray, pValue, pConverter, &Scratch, "array-push-value"
	) ) {
		return false;
	}
	pType = xrtTypedArrayItemType(pArray);
	bResult = xrtTypedArrayPush(pArray, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-push-value", "the converted array item could not be appended");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把一个动态值转换后插入类型数组的指定下标。 */
XRT_API bool xrtTypedArrayInsertValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueArrayDecode(
		pArray, pValue, pConverter, &Scratch, "array-insert-value"
	) ) {
		return false;
	}
	pType = xrtTypedArrayItemType(pArray);
	bResult = xrtTypedArrayInsert(pArray, iIndex, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-insert-value", "the converted array item could not be inserted");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把一个动态值转换后原子替换类型数组的指定元素。 */
XRT_API bool xrtTypedArraySetValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueArrayDecode(
		pArray, pValue, pConverter, &Scratch, "array-set-value"
	) ) {
		return false;
	}
	pType = xrtTypedArrayItemType(pArray);
	bResult = xrtTypedArraySet(pArray, iIndex, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-set-value", "the converted array item could not replace the target");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把类型数组的指定元素转换为独立动态值。 */
XRT_API xvalue* xrtTypedArrayGetValue(
	const xtypedarray* pArray,
	size_t iIndex,
	const xvalueconverter* pConverter
)
{
	const void* pItem = xrtTypedArrayConstGet(pArray, iIndex);

	return pItem != NULL ? __xrtTypedValueArrayEncode(
		pArray, pItem, pConverter, "array-get-value"
	) : NULL;
}



/* 转换并删除类型数组的指定元素。 */
XRT_API xvalue* xrtTypedArrayTakeValue(
	xtypedarray* pArray,
	size_t iIndex,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult = xrtTypedArrayGetValue(pArray, iIndex, pConverter);

	if ( pResult == NULL ) {
		return NULL;
	}
	if ( !xrtTypedArrayRemove(pArray, iIndex, 1u) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-take-value", "the converted array item could not be removed");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	return pResult;
}



/* 转换并删除类型数组的末尾元素。 */
XRT_API xvalue* xrtTypedArrayPopValue(
	xtypedarray* pArray,
	const xvalueconverter* pConverter
)
{
	size_t iCount = xrtTypedArrayCount(pArray);

	return xrtTypedArrayTakeValue(
		pArray, iCount != 0u ? iCount - 1u : SIZE_MAX, pConverter
	);
}



/* 把动态值转换为元素类型并查找第一处相等元素。 */
XRT_API size_t xrtTypedArrayFindValue(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	size_t iIndex;

	if ( !__xrtTypedValueArrayDecode(
		pArray, pValue, pConverter, &Scratch, "array-find-value"
	) ) {
		return SIZE_MAX;
	}
	pType = xrtTypedArrayItemType(pArray);
	iIndex = xrtTypedArrayFind(pArray, Scratch.Value);
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return iIndex;
}



/* 判断类型数组是否包含与动态值等价的元素。 */
XRT_API bool xrtTypedArrayContainsValue(
	const xtypedarray* pArray,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	return xrtTypedArrayFindValue(pArray, pValue, pConverter) != SIZE_MAX;
}

/* 清理失败的临时类型数组并恢复根错误。 */
static void __xrtTypedValueArrayDestroy(xtypedarray* pArray)
{
	xerror* pError = xrtTakeError();

	xrtTypedArrayDestroy(pArray);
	__xrtTypedValueRestoreError(pError);
}



/* 从动态稠密数组构造同构类型数组。 */
XRT_API xtypedarray* xrtTypedArrayFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	xvalueiter Iterator;
	xvaluekey Key;
	xtypedarray* pResult;
	xvalue* pItem;
	xvalueiterresult IterResult;
	size_t iCount;
	size_t iIndex = 0u;

	if ( (pSource == NULL) || (xrtValueType(pSource) != XVALUE_ARRAY) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-from-value", "the source Value is not an array");
		return NULL;
	}
	pResult = xrtTypedArrayCreate(pItemType);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-from-value", "the typed array could not be created");
		return NULL;
	}
	iCount = xrtValueCount(pSource);
	if ( !xrtTypedArrayReserve(pResult, iCount) ||
		 !__xrtTypedValueScratchCreate(pItemType, &Scratch) ) {
		__xrtTypedValueArrayDestroy(pResult);
		return NULL;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pSource, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-from-value", "the dynamic array snapshot could not be started");
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueArrayDestroy(pResult);
		return NULL;
	}
	while ( (IterResult = xrtValueIterAdvance(
		&Iterator, &Key, &pItem
	)) == XVALUE_ITER_ITEM ) {
		if ( (Key.Type != XVALUE_KEY_INDEX) || (Key.Index != iIndex) ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"array-from-value", "the dynamic array snapshot returned an invalid index");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueArrayDestroy(pResult);
			return NULL;
		}
		if ( !xrtValueToTyped(
			pItem, pItemType, Scratch.Value, pConverter
		) ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"array-from-value", "an array item could not be converted");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueArrayDestroy(pResult);
			return NULL;
		}
		if ( !xrtTypedArrayPush(pResult, Scratch.Value) ) {
			__xrtTypedValueDropPreserveError(pItemType, Scratch.Value);
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"array-from-value", "a converted item could not be appended");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueArrayDestroy(pResult);
			return NULL;
		}
		xrtTypeDropValue(pItemType, Scratch.Value);
		iIndex++;
	}
	if ( IterResult == XVALUE_ITER_ERROR ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-from-value", "the dynamic array snapshot iteration failed");
		__xrtTypedValueIteratorEnd(&Iterator);
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueArrayDestroy(pResult);
		return NULL;
	}
	__xrtTypedValueIteratorEnd(&Iterator);
	__xrtTypedValueScratchDestroy(&Scratch);
	return pResult;
}



/* 把类型数组编码为动态稠密数组。 */
XRT_API xvalue* xrtTypedArrayToValue(
	const xtypedarray* pArray,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult;
	const xrttype* pItemType = xrtTypedArrayItemType(pArray);
	size_t iCount;

	if ( pItemType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			"array-to-value", "the typed array is invalid");
		return NULL;
	}
	pResult = xrtValueArray();
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONTAINER,
			"array-to-value", "the dynamic array could not be created");
		return NULL;
	}
	iCount = xrtTypedArrayCount(pArray);
	if ( !xrtValueReserve(pResult, iCount) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"array-to-value", "the dynamic array capacity could not be reserved");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const void* pTyped = xrtTypedArrayConstGet(pArray, i);
		xvalue* pItem;

		if ( pTyped == NULL ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"array-to-value", "a typed array item could not be borrowed");
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		__xrtTypedArrayCallbackBegin(pArray);
		pItem = xrtValueFromTyped(pItemType, pTyped, pConverter);
		__xrtTypedArrayCallbackEnd(pArray);
		if ( pItem == NULL ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"array-to-value", "a typed array item could not be converted");
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		if ( !xrtValueArrayAppendNew(pResult, pItem) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"array-to-value", "a dynamic array item could not be appended");
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
	}
	return pResult;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)

/* 在列表回调门内把动态值解码为临时元素。 */
static bool __xrtTypedValueListDecode(
	const xtypedlist* pList,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedListItemType(pList);
	bool bResult;

	if ( pType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed list is invalid");
		return false;
	}
	__xrtTypedListCallbackBegin(pList);
	bResult = __xrtTypedValueScratchDecode(
		pType, pValue, pConverter, pScratch, sOperation
	);
	__xrtTypedListCallbackEnd(pList);
	return bResult;
}



/* 在列表回调门内把借用元素编码为独立动态值。 */
static xvalue* __xrtTypedValueListEncode(
	const xtypedlist* pList,
	const void* pItem,
	const xvalueconverter* pConverter,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedListItemType(pList);
	xvalue* pResult;

	__xrtTypedListCallbackBegin(pList);
	pResult = xrtValueFromTyped(pType, pItem, pConverter);
	__xrtTypedListCallbackEnd(pList);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the typed list item could not be converted");
	}
	return pResult;
}



/* 把一个动态值转换后写入类型列表的指定整数键。 */
XRT_API bool xrtTypedListSetValue(
	xtypedlist* pList,
	int64 iKey,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueListDecode(
		pList, pValue, pConverter, &Scratch, "list-set-value"
	) ) {
		return false;
	}
	pType = xrtTypedListItemType(pList);
	bResult = xrtTypedListSet(pList, iKey, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-set-value", "the converted list item could not be stored");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把一个动态值转换后追加到最大键之后。 */
XRT_API bool xrtTypedListAppendValue(
	xtypedlist* pList,
	const xvalue* pValue,
	int64* pKey,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedValueListDecode(
		pList, pValue, pConverter, &Scratch, "list-append-value"
	) ) {
		return false;
	}
	pType = xrtTypedListItemType(pList);
	bResult = xrtTypedListAppend(pList, Scratch.Value, pKey);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-append-value", "the converted list item could not be appended");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把指定整数键的类型值转换为独立动态值。 */
XRT_API xvalue* xrtTypedListGetValue(
	const xtypedlist* pList,
	int64 iKey,
	const xvalueconverter* pConverter
)
{
	const void* pItem = xrtTypedListConstGet(pList, iKey);

	return pItem != NULL ? __xrtTypedValueListEncode(
		pList, pItem, pConverter, "list-get-value"
	) : NULL;
}



/* 转换并删除指定整数键的类型值。 */
XRT_API xvalue* xrtTypedListTakeValue(
	xtypedlist* pList,
	int64 iKey,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult = xrtTypedListGetValue(pList, iKey, pConverter);

	if ( pResult == NULL ) {
		return NULL;
	}
	if ( !xrtTypedListRemove(pList, iKey) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-take-value", "the converted list item could not be removed");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	return pResult;
}



/* 把动态值转换为元素类型并查找第一处相等值。 */
XRT_API bool xrtTypedListFindValue(
	const xtypedlist* pList,
	const xvalue* pValue,
	int64* pKey,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedValueListDecode(
		pList, pValue, pConverter, &Scratch, "list-find-value"
	) ) {
		return false;
	}
	pType = xrtTypedListItemType(pList);
	bResult = xrtTypedListFind(pList, Scratch.Value, pKey);
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 判断类型列表是否包含与动态值等价的元素。 */
XRT_API bool xrtTypedListContainsValue(
	const xtypedlist* pList,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	return xrtTypedListFindValue(pList, pValue, NULL, pConverter);
}



/* 清理失败的临时类型列表并恢复根错误。 */
static void __xrtTypedValueListDestroy(xtypedlist* pList)
{
	xerror* pError = xrtTakeError();

	xrtTypedListDestroy(pList);
	__xrtTypedValueRestoreError(pError);
}



/* 从动态整数映射构造同构稀疏类型列表。 */
XRT_API xtypedlist* xrtTypedListFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	xvalueiter Iterator;
	xvaluekey Key;
	xtypedlist* pResult;
	xvalue* pItem;
	xvalueiterresult IterResult;

	if ( (pSource == NULL) || (xrtValueType(pSource) != XVALUE_INT_MAP) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-from-value", "the source Value is not an integer map");
		return NULL;
	}
	pResult = xrtTypedListCreate(pItemType);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-from-value", "the typed list could not be created");
		return NULL;
	}
	if ( !__xrtTypedValueScratchCreate(pItemType, &Scratch) ) {
		__xrtTypedValueListDestroy(pResult);
		return NULL;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pSource, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-from-value", "the integer map snapshot could not be started");
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueListDestroy(pResult);
		return NULL;
	}
	while ( (IterResult = xrtValueIterAdvance(
		&Iterator, &Key, &pItem
	)) == XVALUE_ITER_ITEM ) {
		if ( Key.Type != XVALUE_KEY_INT ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"list-from-value", "the integer map iterator returned a non-integer key");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueListDestroy(pResult);
			return NULL;
		}
		if ( !xrtValueToTyped(
			pItem, pItemType, Scratch.Value, pConverter
		) ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"list-from-value", "a list item could not be converted");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueListDestroy(pResult);
			return NULL;
		}
		if ( !xrtTypedListSet(
			pResult, Key.Integer, Scratch.Value
		) ) {
			__xrtTypedValueDropPreserveError(pItemType, Scratch.Value);
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"list-from-value", "a converted list item could not be stored");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueListDestroy(pResult);
			return NULL;
		}
		xrtTypeDropValue(pItemType, Scratch.Value);
	}
	if ( IterResult == XVALUE_ITER_ERROR ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-from-value", "the integer map snapshot iteration failed");
		__xrtTypedValueIteratorEnd(&Iterator);
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueListDestroy(pResult);
		return NULL;
	}
	__xrtTypedValueIteratorEnd(&Iterator);
	__xrtTypedValueScratchDestroy(&Scratch);
	return pResult;
}



/* 把稀疏类型列表编码为动态整数映射。 */
XRT_API xvalue* xrtTypedListToValue(
	const xtypedlist* pList,
	const xvalueconverter* pConverter
)
{
	xtypedlistiter Iterator;
	const xrttype* pItemType = xrtTypedListItemType(pList);
	xvalue* pResult;
	ptr pItem;
	int64 iKey;

	if ( pItemType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			"list-to-value", "the typed list is invalid");
		return NULL;
	}
	pResult = xrtValueIntMap();
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONTAINER,
			"list-to-value", "the dynamic integer map could not be created");
		return NULL;
	}
	if ( !xrtTypedListIterBegin((xtypedlist*)pList, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"list-to-value", "the typed list iterator could not be started");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	while ( (pItem = xrtTypedListIterNext(&Iterator, &iKey)) != NULL ) {
		xvalue* pValue;

		__xrtTypedListCallbackBegin(pList);
		pValue = xrtValueFromTyped(pItemType, pItem, pConverter);
		__xrtTypedListCallbackEnd(pList);
		if ( pValue == NULL ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"list-to-value", "a typed list item could not be converted");
			xrtTypedListIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		if ( !xrtValueIntMapSetNew(pResult, iKey, pValue) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"list-to-value", "a dynamic integer map item could not be stored");
			xrtTypedListIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
	}
	xrtTypedListIterEnd(&Iterator);
	return pResult;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)

/* 在集合回调门内把动态值解码为临时元素。 */
static bool __xrtTypedValueSetDecode(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedSetItemType(pSet);
	bool bResult;

	if ( pType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed set is invalid");
		return false;
	}
	if ( !__xrtTypedSetCallbackBegin(pSet) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed set callback gate could not be entered");
		return false;
	}
	bResult = __xrtTypedValueScratchDecode(
		pType, pValue, pConverter, pScratch, sOperation
	);
	__xrtTypedSetCallbackEnd(pSet);
	return bResult;
}



/* 在集合回调门内把规范元素编码为独立动态值。 */
static xvalue* __xrtTypedValueSetEncode(
	const xtypedset* pSet,
	const void* pItem,
	const xvalueconverter* pConverter,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedSetItemType(pSet);
	xvalue* pResult;

	if ( !__xrtTypedSetCallbackBegin(pSet) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed set callback gate could not be entered");
		return NULL;
	}
	pResult = xrtValueFromTyped(pType, pItem, pConverter);
	__xrtTypedSetCallbackEnd(pSet);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the typed set item could not be converted");
	}
	return pResult;
}



/* 把一个动态值转换后加入类型集合。 */
XRT_API bool xrtTypedSetAddValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-add-value"
	) ) {
		return false;
	}
	pType = xrtTypedSetItemType(pSet);
	bResult = xrtTypedSetAdd(pSet, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-add-value", "the converted set item could not be added");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 返回与动态值等价的规范元素副本。 */
XRT_API xvalue* xrtTypedSetGetValue(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	const void* pStored;
	xvalue* pResult = NULL;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-get-value"
	) ) {
		return NULL;
	}
	pType = xrtTypedSetItemType(pSet);
	pStored = xrtTypedSetGet(pSet, Scratch.Value);
	if ( pStored != NULL ) {
		pResult = __xrtTypedValueSetEncode(
			pSet, pStored, pConverter, "set-get-value"
		);
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return pResult;
}



/* 判断类型集合是否拥有与动态值等价的元素。 */
XRT_API bool xrtTypedSetHasValue(
	const xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-has-value"
	) ) {
		return false;
	}
	pType = xrtTypedSetItemType(pSet);
	bResult = xrtTypedSetHas(pSet, Scratch.Value);
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 删除与动态值等价的元素。 */
XRT_API bool xrtTypedSetRemoveValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-remove-value"
	) ) {
		return false;
	}
	pType = xrtTypedSetItemType(pSet);
	bResult = xrtTypedSetRemove(pSet, Scratch.Value);
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 转换并删除规范元素。 */
XRT_API xvalue* xrtTypedSetTakeValue(
	xtypedset* pSet,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	const void* pStored;
	xvalue* pResult = NULL;

	if ( !__xrtTypedValueSetDecode(
		pSet, pValue, pConverter, &Scratch, "set-take-value"
	) ) {
		return NULL;
	}
	pType = xrtTypedSetItemType(pSet);
	pStored = xrtTypedSetGet(pSet, Scratch.Value);
	if ( pStored != NULL ) {
		pResult = __xrtTypedValueSetEncode(
			pSet, pStored, pConverter, "set-take-value"
		);
		if ( (pResult != NULL) &&
			 !xrtTypedSetRemove(pSet, Scratch.Value) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"set-take-value", "the converted set item could not be removed");
			__xrtTypedValueRelease(pResult);
			pResult = NULL;
		}
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return pResult;
}



/* 清理失败的临时类型集合并恢复根错误。 */
static void __xrtTypedValueSetDestroy(xtypedset* pSet)
{
	xerror* pError = xrtTakeError();

	xrtTypedSetDestroy(pSet);
	__xrtTypedValueRestoreError(pError);
}



/* 从动态集合构造同构类型集合。 */
XRT_API xtypedset* xrtTypedSetFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	xvalueiter Iterator;
	xtypedset* pResult;
	xvalue* pItem;
	xvalueiterresult IterResult;

	if ( (pSource == NULL) || (xrtValueType(pSource) != XVALUE_SET) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-from-value", "the source Value is not a set");
		return NULL;
	}
	pResult = xrtTypedSetCreate(pItemType);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-from-value", "the typed set could not be created");
		return NULL;
	}
	if ( !xrtTypedSetReserve(pResult, xrtValueCount(pSource)) ||
		 !__xrtTypedValueScratchCreate(pItemType, &Scratch) ) {
		__xrtTypedValueSetDestroy(pResult);
		return NULL;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pSource, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-from-value", "the dynamic set snapshot could not be started");
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueSetDestroy(pResult);
		return NULL;
	}
	while ( (IterResult = xrtValueIterAdvance(
		&Iterator, NULL, &pItem
	)) == XVALUE_ITER_ITEM ) {
		if ( !xrtValueToTyped(
			pItem, pItemType, Scratch.Value, pConverter
		) ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"set-from-value", "a set item could not be converted");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueSetDestroy(pResult);
			return NULL;
		}
		if ( !xrtTypedSetAdd(pResult, Scratch.Value) ) {
			__xrtTypedValueDropPreserveError(pItemType, Scratch.Value);
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"set-from-value", "a converted set item could not be stored");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueSetDestroy(pResult);
			return NULL;
		}
		xrtTypeDropValue(pItemType, Scratch.Value);
	}
	if ( IterResult == XVALUE_ITER_ERROR ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-from-value", "the dynamic set snapshot iteration failed");
		__xrtTypedValueIteratorEnd(&Iterator);
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueSetDestroy(pResult);
		return NULL;
	}
	__xrtTypedValueIteratorEnd(&Iterator);
	__xrtTypedValueScratchDestroy(&Scratch);
	return pResult;
}



/* 把类型集合编码为动态集合。 */
XRT_API xvalue* xrtTypedSetToValue(
	const xtypedset* pSet,
	const xvalueconverter* pConverter
)
{
	xtypedsetiter Iterator;
	const xrttype* pItemType = xrtTypedSetItemType(pSet);
	xvalue* pResult;
	const void* pItem;

	if ( pItemType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			"set-to-value", "the typed set is invalid");
		return NULL;
	}
	pResult = xrtValueSet();
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONTAINER,
			"set-to-value", "the dynamic set could not be created");
		return NULL;
	}
	if ( !xrtValueReserve(pResult, xrtTypedSetCount(pSet)) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-to-value", "the dynamic set capacity could not be reserved");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	if ( !xrtTypedSetIterBegin((xtypedset*)pSet, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"set-to-value", "the typed set iterator could not be started");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	while ( (pItem = xrtTypedSetIterNext(&Iterator)) != NULL ) {
		xvalue* pValue;

		if ( !__xrtTypedSetCallbackBegin(pSet) ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"set-to-value", "the typed set callback gate could not be entered");
			xrtTypedSetIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		pValue = xrtValueFromTyped(pItemType, pItem, pConverter);
		__xrtTypedSetCallbackEnd(pSet);
		if ( pValue == NULL ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"set-to-value", "a typed set item could not be converted");
			xrtTypedSetIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		if ( !xrtValueSetAddNew(pResult, pValue) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"set-to-value", "a dynamic set item could not be stored");
			xrtTypedSetIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
	}
	xrtTypedSetIterEnd(&Iterator);
	return pResult;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 在字典回调门内把动态值解码为临时元素。 */
static bool __xrtTypedValueDictDecode(
	const xtypeddict* pDict,
	const xvalue* pValue,
	const xvalueconverter* pConverter,
	__xrt_typed_value_scratch* pScratch,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedDictItemType(pDict);
	bool bResult;

	if ( pType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed dictionary is invalid");
		return false;
	}
	if ( !__xrtTypedDictCallbackBegin(pDict) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed dictionary callback gate could not be entered");
		return false;
	}
	bResult = __xrtTypedValueScratchDecode(
		pType, pValue, pConverter, pScratch, sOperation
	);
	__xrtTypedDictCallbackEnd(pDict);
	return bResult;
}



/* 在字典回调门内把借用值编码为独立动态值。 */
static xvalue* __xrtTypedValueDictEncode(
	const xtypeddict* pDict,
	const void* pItem,
	const xvalueconverter* pConverter,
	cstr sOperation
)
{
	const xrttype* pType = xrtTypedDictItemType(pDict);
	xvalue* pResult;

	if ( !__xrtTypedDictCallbackBegin(pDict) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			sOperation, "the typed dictionary callback gate could not be entered");
		return NULL;
	}
	pResult = xrtValueFromTyped(pType, pItem, pConverter);
	__xrtTypedDictCallbackEnd(pDict);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
			sOperation, "the typed dictionary item could not be converted");
	}
	return pResult;
}



/* 把一个动态值转换后写入类型字典的指定文本键。 */
XRT_API bool xrtTypedDictSetValue(
	xtypeddict* pDict,
	xstrview Key,
	const xvalue* pValue,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	const xrttype* pType;
	bool bResult;

	if ( !__xrtTypedValueDictDecode(
		pDict, pValue, pConverter, &Scratch, "dict-set-value"
	) ) {
		return false;
	}
	pType = xrtTypedDictItemType(pDict);
	bResult = xrtTypedDictSet(pDict, Key, Scratch.Value);
	if ( !bResult ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-set-value", "the converted dictionary item could not be stored");
	}
	__xrtTypedValueScratchDrop(pType, &Scratch);
	return bResult;
}



/* 把指定文本键的类型值转换为独立动态值。 */
XRT_API xvalue* xrtTypedDictGetValue(
	const xtypeddict* pDict,
	xstrview Key,
	const xvalueconverter* pConverter
)
{
	const void* pItem = xrtTypedDictConstGet(pDict, Key);

	return pItem != NULL ? __xrtTypedValueDictEncode(
		pDict, pItem, pConverter, "dict-get-value"
	) : NULL;
}



/* 转换并删除指定文本键的类型值。 */
XRT_API xvalue* xrtTypedDictTakeValue(
	xtypeddict* pDict,
	xstrview Key,
	const xvalueconverter* pConverter
)
{
	xvalue* pResult = xrtTypedDictGetValue(pDict, Key, pConverter);

	if ( pResult == NULL ) {
		return NULL;
	}
	if ( !xrtTypedDictRemove(pDict, Key) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-take-value", "the converted dictionary item could not be removed");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	return pResult;
}



/* 清理失败的临时类型字典并恢复根错误。 */
static void __xrtTypedValueDictDestroy(xtypeddict* pDict)
{
	xerror* pError = xrtTakeError();

	xrtTypedDictDestroy(pDict);
	__xrtTypedValueRestoreError(pError);
}



/* 从动态字符串键对象构造同构类型字典。 */
XRT_API xtypeddict* xrtTypedDictFromValue(
	const xvalue* pSource,
	const xrttype* pItemType,
	const xvalueconverter* pConverter
)
{
	__xrt_typed_value_scratch Scratch;
	xvalueiter Iterator;
	xvaluekey Key;
	xtypeddict* pResult;
	xvalue* pItem;
	xvalueiterresult IterResult;

	if ( (pSource == NULL) || (xrtValueType(pSource) != XVALUE_OBJECT) ) {
		__xrtTypedValueError(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-from-value", "the source Value is not an object");
		return NULL;
	}
	pResult = xrtTypedDictCreate(pItemType);
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-from-value", "the typed dictionary could not be created");
		return NULL;
	}
	if ( !xrtTypedDictReserve(pResult, xrtValueCount(pSource)) ||
		 !__xrtTypedValueScratchCreate(pItemType, &Scratch) ) {
		__xrtTypedValueDictDestroy(pResult);
		return NULL;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pSource, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-from-value", "the dynamic object snapshot could not be started");
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueDictDestroy(pResult);
		return NULL;
	}
	while ( (IterResult = xrtValueIterAdvance(
		&Iterator, &Key, &pItem
	)) == XVALUE_ITER_ITEM ) {
		if ( Key.Type != XVALUE_KEY_STRING ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"dict-from-value", "the object iterator returned a non-string key");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueDictDestroy(pResult);
			return NULL;
		}
		if ( !xrtValueToTyped(
			pItem, pItemType, Scratch.Value, pConverter
		) ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"dict-from-value", "a dictionary item could not be converted");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueDictDestroy(pResult);
			return NULL;
		}
		if ( !xrtTypedDictSet(
			pResult, Key.String, Scratch.Value
		) ) {
			__xrtTypedValueDropPreserveError(pItemType, Scratch.Value);
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"dict-from-value", "a converted dictionary item could not be stored");
			__xrtTypedValueIteratorEnd(&Iterator);
			__xrtTypedValueScratchDestroy(&Scratch);
			__xrtTypedValueDictDestroy(pResult);
			return NULL;
		}
		xrtTypeDropValue(pItemType, Scratch.Value);
	}
	if ( IterResult == XVALUE_ITER_ERROR ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-from-value", "the dynamic object snapshot iteration failed");
		__xrtTypedValueIteratorEnd(&Iterator);
		__xrtTypedValueScratchDestroy(&Scratch);
		__xrtTypedValueDictDestroy(pResult);
		return NULL;
	}
	__xrtTypedValueIteratorEnd(&Iterator);
	__xrtTypedValueScratchDestroy(&Scratch);
	return pResult;
}



/* 把类型字典编码为动态字符串键对象。 */
XRT_API xvalue* xrtTypedDictToValue(
	const xtypeddict* pDict,
	const xvalueconverter* pConverter
)
{
	xtypeddictiter Iterator;
	const xrttype* pItemType = xrtTypedDictItemType(pDict);
	xvalue* pResult;
	xstrview Key;
	ptr pItem;

	if ( pItemType == NULL ) {
		__xrtTypedValueWrap(XERR_ARGUMENT, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-to-value", "the typed dictionary is invalid");
		return NULL;
	}
	pResult = xrtValueObject();
	if ( pResult == NULL ) {
		__xrtTypedValueWrap(XERR_MEMORY, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-to-value", "the dynamic object could not be created");
		return NULL;
	}
	if ( !xrtValueReserve(pResult, xrtTypedDictCount(pDict)) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-to-value", "the dynamic object capacity could not be reserved");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	if ( !xrtTypedDictIterBegin((xtypeddict*)pDict, &Iterator) ) {
		__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
			"dict-to-value", "the typed dictionary iterator could not be started");
		__xrtTypedValueRelease(pResult);
		return NULL;
	}
	while ( (pItem = xrtTypedDictIterNext(&Iterator, &Key)) != NULL ) {
		xvalue* pValue;

		if ( !__xrtTypedDictCallbackBegin(pDict) ) {
			__xrtTypedValueError(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"dict-to-value", "the typed dictionary callback gate could not be entered");
			xrtTypedDictIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		pValue = xrtValueFromTyped(pItemType, pItem, pConverter);
		__xrtTypedDictCallbackEnd(pDict);
		if ( pValue == NULL ) {
			__xrtTypedValueWrap(XERR_TYPE, XTYPED_VALUE_ERROR_CONVERT,
				"dict-to-value", "a typed dictionary item could not be converted");
			xrtTypedDictIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
		if ( !xrtValueObjectSetNew(pResult, Key, pValue) ) {
			__xrtTypedValueWrap(XERR_STATE, XTYPED_VALUE_ERROR_CONTAINER,
				"dict-to-value", "a dynamic object item could not be stored");
			xrtTypedDictIterEnd(&Iterator);
			__xrtTypedValueRelease(pResult);
			return NULL;
		}
	}
	xrtTypedDictIterEnd(&Iterator);
	return pResult;
}

#endif

#endif
