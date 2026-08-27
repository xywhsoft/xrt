#include "../internal/xrt_runtime_convert.h"

#include <xrt/value.h>

#if defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING)
	#include <xrt/runtime_type_string.h>
#endif



#if defined(XRUNTIME_FEATURE_VALUE_CONVERT)

/* 归一化保存动态 Value 可以直接借用的标量。 */
typedef union __xrt_value_convert_scalar {
	bool Bool;
	int64 Integer;
	uint64 Unsigned;
	double Float;
	xtime Time;
	ptr Pointer;
} __xrt_value_convert_scalar;



/* 包装动态值读取失败，保留 Value 层给出的具体原因。 */
static bool __xrtValueConvertReadFailed(void)
{
	__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
		"value-convert", "the dynamic scalar could not be read");
	return false;
}



#if defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING)

/*
	把动态字符串作为规范拥有型字符串的借用来源参与转换。
	Value 保证末尾零，内嵌零会使 str 语义丢失长度，因此明确拒绝。
*/
static bool __xrtValueConvertString(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
)
{
	xstrview Text;
	str sBorrowed;

	if ( !xrtValueGetString(pSource, &Text) ) {
		return __xrtValueConvertReadFailed();
	}
	if ( (Text.Size != 0u) &&
		 (memchr(Text.Data, 0, Text.Size) != NULL) ) {
		__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"value-convert", "a runtime string cannot represent embedded zero bytes");
		return false;
	}
	sBorrowed = (str)Text.Data;
	return xrtTypeConvert(
		xrtTypeString(), &sBorrowed, pTargetType, pTarget, Mode
	);
}

#endif



/* 把动态 Value 标量转换后写入已经初始化的目标值。 */
XRT_API bool xrtValueConvertTo(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
)
{
	__xrt_value_convert_scalar Scalar;
	const xrttype* pSourceType;
	const void* pValue;
	xvaluetype Type;

	if (
		(pSource == NULL) || (pTargetType == NULL) ||
		((pTarget == NULL) && (pTargetType->Size != 0u))
	) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"value-convert", "the source, target, or runtime type is invalid");
		return false;
	}
	if ( !__xrtTypeConvertModeValid(Mode) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_MODE,
			"value-convert", "the conversion mode is invalid");
		return false;
	}
	if ( !xrtTypeValidate(pTargetType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"value-convert", "the target runtime type is invalid");
		return false;
	}
	Type = xrtValueType(pSource);
	if ( Type == XVALUE_INVALID ) {
		return __xrtValueConvertReadFailed();
	}
	memset(&Scalar, 0, sizeof(Scalar));
	pSourceType = NULL;
	pValue = NULL;
	switch ( Type ) {
		case XVALUE_NULL:
			pSourceType = xrtTypeNull();
			break;
		case XVALUE_BOOL:
			if ( !xrtValueGetBool(pSource, &Scalar.Bool) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeBool();
			pValue = &Scalar.Bool;
			break;
		case XVALUE_INT:
			if ( !xrtValueGetInt(pSource, &Scalar.Integer) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeInt64();
			pValue = &Scalar.Integer;
			break;
		case XVALUE_UINT:
			if ( !xrtValueGetUInt(pSource, &Scalar.Unsigned) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeUInt64();
			pValue = &Scalar.Unsigned;
			break;
		case XVALUE_FLOAT:
			if ( !xrtValueGetFloat(pSource, &Scalar.Float) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeFloat64();
			pValue = &Scalar.Float;
			break;
#if defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING)
		case XVALUE_STRING:
			return __xrtValueConvertString(
				pSource, pTargetType, pTarget, Mode
			);
#endif
		case XVALUE_TIME:
			if ( !xrtValueGetTime(pSource, &Scalar.Time) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypeTime();
			pValue = &Scalar.Time;
			break;
		case XVALUE_POINTER:
			if ( !xrtValueGetPointer(pSource, &Scalar.Pointer) ) {
				return __xrtValueConvertReadFailed();
			}
			pSourceType = xrtTypePointer();
			pValue = &Scalar.Pointer;
			break;
		default:
			__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
				"value-convert", "the dynamic value is not a supported scalar");
			return false;
	}
	return xrtTypeConvert(
		pSourceType, pValue, pTargetType, pTarget, Mode
	);
}

#endif
