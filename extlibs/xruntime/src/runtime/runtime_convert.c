#include "../internal/xrt_runtime_convert.h"



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT)

/* 归一化保存所有不分配内存的内建标量来源。 */
typedef struct __xrttypeconvertvalue {
	xrttypekind Kind;
	bool Bool;
	int64 Signed;
	uint64 Unsigned;
	double Float;
	xtime Time;
	ptr Pointer;
} __xrttypeconvertvalue;



/* 设置转换层结构化错误。 */
void __xrtTypeConvertError(
	xerrkind Kind,
	xtypeconverterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.type-convert";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 包装下层转换失败并保留原始错误链。 */
void __xrtTypeConvertWrap(
	xerrkind DefaultKind,
	xtypeconverterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.type-convert";
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



/* 判断转换模式枚举是否有效。 */
bool __xrtTypeConvertModeValid(xtypeconvertmode Mode)
{
	return (Mode == XTYPE_CONVERT_EXACT) ||
		(Mode == XTYPE_CONVERT_WIDEN) ||
		(Mode == XTYPE_CONVERT_EXPLICIT);
}



/* 判断类型是否为可执行算术转换的定宽数值。 */
static bool __xrtTypeConvertNumeric(const xrttype* pType)
{
	return (pType->Kind == XRT_TYPE_BOOL) ||
		(pType->Kind == XRT_TYPE_SIGNED_INT) ||
		(pType->Kind == XRT_TYPE_UNSIGNED_INT) ||
		(pType->Kind == XRT_TYPE_FLOAT) ||
		(pType->Kind == XRT_TYPE_TYPE);
}



/* 返回浮点格式能够连续精确表示的整数有效位数。 */
static size_t __xrtTypeConvertFloatBits(const xrttype* pType)
{
	if ( (pType->Kind != XRT_TYPE_FLOAT) ||
		 ((pType->Size != sizeof(float)) &&
		  (pType->Size != sizeof(double))) ) {
		return 0u;
	}
	return pType->Size == sizeof(float) ? 24u : 53u;
}



/* 判断两个已验证类型是否存在覆盖全部来源值的无损方向。 */
static bool __xrtTypeCanWidenBuiltin(
	const xrttype* pSourceType,
	const xrttype* pTargetType
)
{
	size_t iFloatBits;
	size_t iIntegerBits;

	if ( xrtTypeSame(pSourceType, pTargetType) ) {
		return xrtTypeIsCopyable(pSourceType);
	}
	if ( (pSourceType->Kind == XRT_TYPE_NULL) &&
		 (pTargetType->Kind == XRT_TYPE_POINTER) ) {
		return true;
	}
	if ( pSourceType->Kind == XRT_TYPE_BOOL ) {
		return (pTargetType->Kind == XRT_TYPE_BOOL) ||
			(pTargetType->Kind == XRT_TYPE_SIGNED_INT) ||
			(pTargetType->Kind == XRT_TYPE_UNSIGNED_INT) ||
			(pTargetType->Kind == XRT_TYPE_FLOAT);
	}
	if ( (pSourceType->Kind == XRT_TYPE_SIGNED_INT) &&
		 (pTargetType->Kind == XRT_TYPE_SIGNED_INT) ) {
		return pTargetType->Size >= pSourceType->Size;
	}
	if ( (pSourceType->Kind == XRT_TYPE_UNSIGNED_INT) &&
		 (pTargetType->Kind == XRT_TYPE_UNSIGNED_INT) ) {
		return pTargetType->Size >= pSourceType->Size;
	}
	if ( (pSourceType->Kind == XRT_TYPE_UNSIGNED_INT) &&
		 (pTargetType->Kind == XRT_TYPE_SIGNED_INT) ) {
		return pTargetType->Size > pSourceType->Size;
	}
	if (
		((pSourceType->Kind == XRT_TYPE_SIGNED_INT) ||
		 (pSourceType->Kind == XRT_TYPE_UNSIGNED_INT)) &&
		(pTargetType->Kind == XRT_TYPE_FLOAT)
	) {
		iFloatBits = __xrtTypeConvertFloatBits(pTargetType);
		iIntegerBits = pSourceType->Size * CHAR_BIT;
		if ( pSourceType->Kind == XRT_TYPE_SIGNED_INT ) {
			iIntegerBits--;
		}
		return (iFloatBits != 0u) && (iIntegerBits <= iFloatBits);
	}
	return (pSourceType->Kind == XRT_TYPE_FLOAT) &&
		(pTargetType->Kind == XRT_TYPE_FLOAT) &&
		(pSourceType->Size == sizeof(float)) &&
		(pTargetType->Size == sizeof(double));
}



/* 判断显式模式是否允许指定已验证类型组合。 */
static bool __xrtTypeCanExplicitBuiltin(
	const xrttype* pSourceType,
	const xrttype* pTargetType
)
{
	if ( __xrtTypeCanWidenBuiltin(pSourceType, pTargetType) ) {
		return true;
	}
	if ( __xrtTypeConvertNumeric(pSourceType) &&
		 __xrtTypeConvertNumeric(pTargetType) ) {
		return true;
	}
	if ( pSourceType->Kind == XRT_TYPE_NULL ) {
		if ( __xrtTypeConvertNumeric(pTargetType) ||
			(pTargetType->Kind == XRT_TYPE_TIME) ||
			(pTargetType->Kind == XRT_TYPE_POINTER) ) {
			return true;
		}
	}
	if ( pSourceType->Kind == XRT_TYPE_TIME ) {
		if ( __xrtTypeConvertNumeric(pTargetType) ) {
			return true;
		}
	}
	if ( pTargetType->Kind == XRT_TYPE_TIME ) {
		if ( (pSourceType->Kind == XRT_TYPE_SIGNED_INT) ||
			 (pSourceType->Kind == XRT_TYPE_UNSIGNED_INT) ) {
			return true;
		}
	}
	if ( (pSourceType->Kind == XRT_TYPE_POINTER) &&
		 (pTargetType->Kind == XRT_TYPE_BOOL) ) {
		return true;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)
	return __xrtTypeStringCanConvert(pSourceType, pTargetType);
#else
	return false;
#endif
}



/* 在类型和模式已经验证后判断转换关系，不读取或修改线程错误。 */
static bool __xrtTypeCanConvertValidated(
	const xrttype* pSourceType,
	const xrttype* pTargetType,
	xtypeconvertmode Mode
)
{
	if ( Mode == XTYPE_CONVERT_EXACT ) {
		return xrtTypeSame(pSourceType, pTargetType) &&
			xrtTypeIsCopyable(pSourceType);
	}
	if ( Mode == XTYPE_CONVERT_WIDEN ) {
		return __xrtTypeCanWidenBuiltin(pSourceType, pTargetType);
	}
	return __xrtTypeCanExplicitBuiltin(pSourceType, pTargetType);
}



/* 把来源内建标量安全读取到对齐的归一化槽。 */
static bool __xrtTypeConvertRead(
	const xrttype* pSourceType,
	const void* pSource,
	__xrttypeconvertvalue* pValue
)
{
	memset(pValue, 0, sizeof(*pValue));
	pValue->Kind = pSourceType->Kind;
	switch ( pSourceType->Kind ) {
		case XRT_TYPE_NULL:
			return true;
		case XRT_TYPE_BOOL:
			return __xrtTypeReadBool(
				pSource, pSourceType->Size, &pValue->Bool
			);
		case XRT_TYPE_SIGNED_INT:
			return __xrtTypeReadSigned(
				pSource, pSourceType->Size, &pValue->Signed
			);
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			return __xrtTypeReadUnsigned(
				pSource, pSourceType->Size, &pValue->Unsigned
			);
		case XRT_TYPE_FLOAT:
			return __xrtTypeReadFloat(
				pSource, pSourceType->Size, &pValue->Float
			);
		case XRT_TYPE_TIME:
			if ( pSourceType->Size == sizeof(pValue->Time) ) {
				memcpy(&pValue->Time, pSource, sizeof(pValue->Time));
				return true;
			}
			break;
		case XRT_TYPE_POINTER:
			if ( pSourceType->Size == sizeof(pValue->Pointer) ) {
				memcpy(&pValue->Pointer, pSource, sizeof(pValue->Pointer));
				return true;
			}
			break;
		default:
			break;
	}
	__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
		"convert", "the source type has no supported scalar representation");
	return false;
}



/* 把来源标量按脚本语言真值规则转换为布尔值。 */
static bool __xrtTypeConvertBool(
	const __xrttypeconvertvalue* pValue,
	bool* pResult
)
{
	switch ( pValue->Kind ) {
		case XRT_TYPE_NULL:
			*pResult = false;
			return true;
		case XRT_TYPE_BOOL:
			*pResult = pValue->Bool;
			return true;
		case XRT_TYPE_SIGNED_INT:
			*pResult = pValue->Signed != 0;
			return true;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			*pResult = pValue->Unsigned != 0u;
			return true;
		case XRT_TYPE_FLOAT:
			*pResult = pValue->Float != 0.0;
			return true;
		case XRT_TYPE_TIME:
			*pResult = pValue->Time != 0;
			return true;
		case XRT_TYPE_POINTER:
			*pResult = pValue->Pointer != NULL;
			return true;
		default:
			return false;
	}
}



/* 范围检查后把浮点值截断为目标宽度的有符号整数。 */
static bool __xrtTypeFloatToSigned(
	double fValue,
	size_t iSize,
	int64* pResult
)
{
	long double fMinimum;
	long double fMaximum;
	long double fWide;

	if ( fValue != fValue ) {
		return false;
	}
	switch ( iSize ) {
		case 1u:
			fMinimum = -128.0L;
			fMaximum = 128.0L;
			break;
		case 2u:
			fMinimum = -32768.0L;
			fMaximum = 32768.0L;
			break;
		case 4u:
			fMinimum = -2147483648.0L;
			fMaximum = 2147483648.0L;
			break;
		case 8u:
			fMinimum = -9223372036854775808.0L;
			fMaximum = 9223372036854775808.0L;
			break;
		default:
			return false;
	}
	fWide = (long double)fValue;
	if ( (fWide < fMinimum) || (fWide >= fMaximum) ) {
		return false;
	}
	*pResult = (int64)fValue;
	return true;
}



/* 范围检查后把浮点值截断为目标宽度的无符号整数。 */
static bool __xrtTypeFloatToUnsigned(
	double fValue,
	size_t iSize,
	uint64* pResult
)
{
	long double fMaximum;
	long double fWide;

	if ( fValue != fValue ) {
		return false;
	}
	switch ( iSize ) {
		case 1u:
			fMaximum = 256.0L;
			break;
		case 2u:
			fMaximum = 65536.0L;
			break;
		case 4u:
			fMaximum = 4294967296.0L;
			break;
		case 8u:
			fMaximum = 18446744073709551616.0L;
			break;
		default:
			return false;
	}
	fWide = (long double)fValue;
	if ( (fWide < 0.0L) || (fWide >= fMaximum) ) {
		return false;
	}
	*pResult = (uint64)fValue;
	return true;
}



/* 把归一化来源写成目标有符号整数。 */
static bool __xrtTypeConvertSigned(
	const __xrttypeconvertvalue* pValue,
	size_t iSize,
	ptr pTarget
)
{
	int64 iSigned;

	switch ( pValue->Kind ) {
		case XRT_TYPE_NULL:
			iSigned = 0;
			break;
		case XRT_TYPE_BOOL:
			iSigned = pValue->Bool ? 1 : 0;
			break;
		case XRT_TYPE_SIGNED_INT:
			iSigned = pValue->Signed;
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( pValue->Unsigned > INT64_MAX ) {
				return false;
			}
			iSigned = (int64)pValue->Unsigned;
			break;
		case XRT_TYPE_FLOAT:
			if ( !__xrtTypeFloatToSigned(
				pValue->Float, iSize, &iSigned
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_TIME:
			iSigned = (int64)pValue->Time;
			break;
		default:
			return false;
	}
	return __xrtTypeWriteSigned(iSigned, iSize, pTarget);
}



/* 把归一化来源写成目标无符号整数或类型标识。 */
static bool __xrtTypeConvertUnsigned(
	const __xrttypeconvertvalue* pValue,
	size_t iSize,
	ptr pTarget
)
{
	uint64 iUnsigned;

	switch ( pValue->Kind ) {
		case XRT_TYPE_NULL:
			iUnsigned = 0u;
			break;
		case XRT_TYPE_BOOL:
			iUnsigned = pValue->Bool ? 1u : 0u;
			break;
		case XRT_TYPE_SIGNED_INT:
			if ( pValue->Signed < 0 ) {
				return false;
			}
			iUnsigned = (uint64)pValue->Signed;
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			iUnsigned = pValue->Unsigned;
			break;
		case XRT_TYPE_FLOAT:
			if ( !__xrtTypeFloatToUnsigned(
				pValue->Float, iSize, &iUnsigned
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_TIME:
			if ( pValue->Time < 0 ) {
				return false;
			}
			iUnsigned = (uint64)pValue->Time;
			break;
		default:
			return false;
	}
	return __xrtTypeWriteUnsigned(iUnsigned, iSize, pTarget);
}



/* 把归一化来源写成目标浮点值。 */
static bool __xrtTypeConvertFloat(
	const __xrttypeconvertvalue* pValue,
	size_t iSize,
	bool bLossless,
	ptr pTarget
)
{
	double fValue;

	switch ( pValue->Kind ) {
		case XRT_TYPE_NULL:
			fValue = 0.0;
			break;
		case XRT_TYPE_BOOL:
			fValue = pValue->Bool ? 1.0 : 0.0;
			break;
		case XRT_TYPE_SIGNED_INT:
			fValue = (double)pValue->Signed;
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			fValue = (double)pValue->Unsigned;
			break;
		case XRT_TYPE_FLOAT:
			fValue = pValue->Float;
			break;
		case XRT_TYPE_TIME:
			fValue = (double)pValue->Time;
			break;
		default:
			return false;
	}
	return __xrtTypeWriteFloat(fValue, iSize, bLossless, pTarget);
}



/* 执行不依赖文本模块的内建标量转换。 */
static bool __xrtTypeConvertBuiltin(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
)
{
	__xrttypeconvertvalue Value;
	bool bValue;
	xtime Time;
	ptr pPointer = NULL;
	bool bSuccess = false;

	if ( !__xrtTypeConvertRead(pSourceType, pSource, &Value) ) {
		return false;
	}
	switch ( pTargetType->Kind ) {
		case XRT_TYPE_BOOL:
			bSuccess = __xrtTypeConvertBool(&Value, &bValue);
			if ( bSuccess ) {
				bSuccess = __xrtTypeWriteBool(
					bValue, pTargetType->Size, pTarget
				);
			}
			break;
		case XRT_TYPE_SIGNED_INT:
			bSuccess = __xrtTypeConvertSigned(
				&Value, pTargetType->Size, pTarget
			);
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			bSuccess = __xrtTypeConvertUnsigned(
				&Value, pTargetType->Size, pTarget
			);
			break;
		case XRT_TYPE_FLOAT:
			bSuccess = __xrtTypeConvertFloat(
				&Value, pTargetType->Size,
				Mode != XTYPE_CONVERT_EXPLICIT, pTarget
			);
			break;
		case XRT_TYPE_TIME:
			if ( Value.Kind == XRT_TYPE_NULL ) {
				Time = 0;
				bSuccess = true;
			} else if ( Value.Kind == XRT_TYPE_SIGNED_INT ) {
				Time = (xtime)Value.Signed;
				bSuccess = true;
			} else if (
				(Value.Kind == XRT_TYPE_UNSIGNED_INT) &&
				(Value.Unsigned <= INT64_MAX)
			) {
				Time = (xtime)Value.Unsigned;
				bSuccess = true;
			}
			if ( bSuccess ) {
				memcpy(pTarget, &Time, sizeof(Time));
			}
			break;
		case XRT_TYPE_POINTER:
			if ( Value.Kind == XRT_TYPE_NULL ) {
				memcpy(pTarget, &pPointer, sizeof(pPointer));
				bSuccess = true;
			}
			break;
		default:
			break;
	}
	if ( !bSuccess ) {
		__xrtTypeConvertError(XERR_RANGE, XTYPE_CONVERT_ERROR_RANGE,
			"convert", "the source value is outside the target representation");
	}
	return bSuccess;
}



/* 判断源类型的全部有效值是否都能被目标类型无损表示。 */
XRT_API bool xrtTypeCanWiden(
	const xrttype* pSourceType,
	const xrttype* pTargetType
)
{
	if ( (pSourceType == NULL) || (pTargetType == NULL) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"can-widen", "the source and target types are required");
		return false;
	}
	if ( !xrtTypeValidate(pSourceType) || !xrtTypeValidate(pTargetType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"can-widen", "a runtime type descriptor is invalid");
		return false;
	}
	return __xrtTypeCanWidenBuiltin(pSourceType, pTargetType);
}



/* 判断两个类型在指定模式下是否存在稳定的内建转换路径。 */
XRT_API bool xrtTypeCanConvert(
	const xrttype* pSourceType,
	const xrttype* pTargetType,
	xtypeconvertmode Mode
)
{
	if ( (pSourceType == NULL) || (pTargetType == NULL) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"can-convert", "the source and target types are required");
		return false;
	}
	if ( !__xrtTypeConvertModeValid(Mode) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_MODE,
			"can-convert", "the conversion mode is invalid");
		return false;
	}
	if ( !xrtTypeValidate(pSourceType) || !xrtTypeValidate(pTargetType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"can-convert", "a runtime type descriptor is invalid");
		return false;
	}
	return __xrtTypeCanConvertValidated(pSourceType, pTargetType, Mode);
}



/* 把借用的源值转换后写入已经初始化的目标值。 */
XRT_API bool xrtTypeConvert(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
)
{
	if (
		(pSourceType == NULL) || (pTargetType == NULL) ||
		((pSource == NULL) && (pSourceType->Size != 0u)) ||
		((pTarget == NULL) && (pTargetType->Size != 0u))
	) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"convert", "the source, target, or runtime type is invalid");
		return false;
	}
	if ( !__xrtTypeConvertModeValid(Mode) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_MODE,
			"convert", "the conversion mode is invalid");
		return false;
	}
	if ( !xrtTypeValidate(pSourceType) || !xrtTypeValidate(pTargetType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"convert", "a runtime type descriptor is invalid");
		return false;
	}
	if ( !__xrtTypeCanConvertValidated(pSourceType, pTargetType, Mode) ) {
		__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"convert", "the runtime types do not support this conversion mode");
		return false;
	}
	if ( xrtTypeSame(pSourceType, pTargetType) ) {
		if ( xrtTypeCopyValue(pSourceType, pTarget, pSource) ) {
			return true;
		}
		__xrtTypeConvertWrap(XERR_STATE, XTYPE_CONVERT_ERROR_OPERATION,
			"convert", "the exact type copy operation failed");
		return false;
	}
	if ( __xrtRangesOverlap(
		pSource, pSourceType->Size, pTarget, pTargetType->Size
	) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"convert", "different-type source and target ranges overlap");
		return false;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)
	if ( __xrtTypeStringCanConvert(pSourceType, pTargetType) ) {
		return __xrtTypeStringConvert(
			pSourceType, pSource, pTargetType, pTarget
		);
	}
#endif
	return __xrtTypeConvertBuiltin(
		pSourceType, pSource, pTargetType, pTarget, Mode
	);
}

#endif
