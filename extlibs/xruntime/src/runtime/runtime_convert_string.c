#include "../internal/xrt_runtime_convert.h"

#include <xrt/number.h>
#include <xrt/runtime_type_string.h>
#include <xrt/string.h>
#include <xrt/time.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)

/* 判断类型是否为转换层唯一支持的拥有型 XRT 字符串槽。 */
static bool __xrtTypeConvertCanonicalString(const xrttype* pType)
{
	return xrtTypeSame(pType, xrtTypeString());
}



/* 判断可选文本扩展是否支持指定转换方向。 */
bool __xrtTypeStringCanConvert(
	const xrttype* pSourceType,
	const xrttype* pTargetType
)
{
	if ( __xrtTypeConvertCanonicalString(pSourceType) ) {
		return (pTargetType->Kind == XRT_TYPE_BOOL) ||
			(pTargetType->Kind == XRT_TYPE_SIGNED_INT) ||
			(pTargetType->Kind == XRT_TYPE_UNSIGNED_INT) ||
			(pTargetType->Kind == XRT_TYPE_FLOAT) ||
			(pTargetType->Kind == XRT_TYPE_TIME) ||
			(pTargetType->Kind == XRT_TYPE_TYPE);
	}
	if ( !__xrtTypeConvertCanonicalString(pTargetType) ) {
		return false;
	}
	if ( (pSourceType->Ops != NULL) &&
		 (pSourceType->Ops->Format != NULL) ) {
		return true;
	}
	return (pSourceType->Kind == XRT_TYPE_NULL) ||
		(pSourceType->Kind == XRT_TYPE_BOOL) ||
		(pSourceType->Kind == XRT_TYPE_SIGNED_INT) ||
		(pSourceType->Kind == XRT_TYPE_UNSIGNED_INT) ||
		(pSourceType->Kind == XRT_TYPE_FLOAT) ||
		(pSourceType->Kind == XRT_TYPE_TIME) ||
		(pSourceType->Kind == XRT_TYPE_POINTER) ||
		(pSourceType->Kind == XRT_TYPE_TYPE);
}



/* 严格解析布尔文本，只接受 true、false、1 和 0。 */
static bool __xrtTypeStringParseBool(xstrview Text, bool* pValue)
{
	if ( xrtStrCaseEqual(Text, XRT_STR_LITERAL("true")) ||
		 xrtStrEqual(Text, XRT_STR_LITERAL("1")) ) {
		*pValue = true;
		return true;
	}
	if ( xrtStrCaseEqual(Text, XRT_STR_LITERAL("false")) ||
		 xrtStrEqual(Text, XRT_STR_LITERAL("0")) ) {
		*pValue = false;
		return true;
	}
	__xrtTypeConvertError(XERR_VALUE, XTYPE_CONVERT_ERROR_PARSE,
		"parse-string", "the string is not a strict boolean value");
	return false;
}



/* 包装文本解析器错误，保留具体的格式或范围原因。 */
static bool __xrtTypeStringParseFailed(cstr sMessage)
{
	__xrtTypeConvertWrap(XERR_VALUE, XTYPE_CONVERT_ERROR_PARSE,
		"parse-string", sMessage);
	return false;
}



/* 把字符串严格解析为目标标量，解析失败和范围失败均保持目标不变。 */
static bool __xrtTypeStringParse(
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget
)
{
	str sSource;
	xstrview Text;
	bool bValue;
	int64 iSigned;
	uint64 iUnsigned;
	double fValue;
	xtime Time;

	memcpy(&sSource, pSource, sizeof(sSource));
	Text = xrtStrView(sSource);
	switch ( pTargetType->Kind ) {
		case XRT_TYPE_BOOL:
			if ( !__xrtTypeStringParseBool(Text, &bValue) ) {
				return false;
			}
			return xrtTypeConvert(xrtTypeBool(), &bValue,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		case XRT_TYPE_SIGNED_INT:
			if ( !xrtIntParse(Text, 10u, 0u, &iSigned) ) {
				return __xrtTypeStringParseFailed(
					"the string is not a strict signed integer"
				);
			}
			return xrtTypeConvert(xrtTypeInt64(), &iSigned,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( !xrtUIntParse(Text, 10u, 0u, &iUnsigned) ) {
				return __xrtTypeStringParseFailed(
					"the string is not a strict unsigned integer"
				);
			}
			return xrtTypeConvert(xrtTypeUInt64(), &iUnsigned,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		case XRT_TYPE_FLOAT:
			if ( !xrtNumParse(Text,
				(uint32)XNUMBER_PARSE_SPECIAL, &fValue) ) {
				return __xrtTypeStringParseFailed(
					"the string is not a strict floating-point value"
				);
			}
			return xrtTypeConvert(xrtTypeFloat64(), &fValue,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		case XRT_TYPE_TIME:
			if ( !xrtTimeParseAny(Text, &Time) ) {
				return __xrtTypeStringParseFailed(
					"the string is not a supported time value"
				);
			}
			return xrtTypeConvert(xrtTypeTime(), &Time,
				pTargetType, pTarget, XTYPE_CONVERT_EXPLICIT);
		default:
			__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
				"parse-string", "the target type cannot be parsed from a string");
			return false;
	}
}



/*
	把内建标量格式化到调用方栈缓冲。
	pSupported 区分不支持的类型与已经产生下层错误的格式化失败。
*/
static bool __xrtTypeStringFormatBuiltin(
	const xrttype* pSourceType,
	const void* pSource,
	char* sBuffer,
	size_t iCapacity,
	xstrview* pText,
	bool* pSupported
)
{
	bool bValue;
	int64 iSigned;
	uint64 iUnsigned;
	double fValue;
	xtime Time;
	ptr pPointer;
	size_t iSize;

	*pSupported = true;
	switch ( pSourceType->Kind ) {
		case XRT_TYPE_NULL:
			*pText = XRT_STR_LITERAL("null");
			return true;
		case XRT_TYPE_BOOL:
			if ( !__xrtTypeReadBool(
				pSource, pSourceType->Size, &bValue
			) ) {
				return false;
			}
			*pText = bValue ? XRT_STR_LITERAL("true") :
				XRT_STR_LITERAL("false");
			return true;
		case XRT_TYPE_SIGNED_INT:
			if ( !__xrtTypeReadSigned(
				pSource, pSourceType->Size, &iSigned
			) || !xrtIntWrite(
				iSigned, 10u, sBuffer, iCapacity, &iSize, 0u
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( !__xrtTypeReadUnsigned(
				pSource, pSourceType->Size, &iUnsigned
			) || !xrtUIntWrite(
				iUnsigned, 10u, sBuffer, iCapacity, &iSize, 0u
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_FLOAT:
			if ( !__xrtTypeReadFloat(
				pSource, pSourceType->Size, &fValue
			) || !xrtNumWrite(
				fValue, sBuffer, iCapacity, &iSize, 0u
			) ) {
				return false;
			}
			break;
		case XRT_TYPE_TIME:
			memcpy(&Time, pSource, sizeof(Time));
			iSize = xrtTimeWriteRFC3339(
				sBuffer, iCapacity, Time, 0
			);
			if ( iSize == XRT_NPOS ) {
				return false;
			}
			break;
		case XRT_TYPE_POINTER:
			memcpy(&pPointer, pSource, sizeof(pPointer));
			if ( pPointer == NULL ) {
				*pText = XRT_STR_LITERAL("null");
				return true;
			}
			if ( !xrtUIntWrite(
				(uint64)(uintptr_t)pPointer, 16u,
				sBuffer, iCapacity, &iSize, (uint32)XNUMBER_PREFIX
			) ) {
				return false;
			}
			break;
		default:
			*pSupported = false;
			return false;
	}
	pText->Data = sBuffer;
	pText->Size = iSize;
	return true;
}



/* 把格式化分块追加到临时字符串构建器。 */
static bool __xrtTypeStringBufferWrite(xstrview Text, ptr pContext)
{
	return xrtStrBufAppend((xstrbuf*)pContext, Text);
}



/* 把一个借用类型值分块格式化为 UTF-8 文本。 */
XRT_API bool xrtTypeFormat(
	const xrttype* pType,
	const void* pValue,
	xrttypewriter pWrite,
	ptr pContext
)
{
	const xerror* pPrevious;
	char sBuffer[128];
	xstrview Text;
	bool bSupported;
	bool bSuccess;

	if ( (pType == NULL) || (pWrite == NULL) ||
		 ((pValue == NULL) && (pType->Size != 0u)) ) {
		__xrtTypeConvertError(XERR_ARGUMENT, XTYPE_CONVERT_ERROR_ARGUMENT,
			"format", "the runtime type, value, or writer is invalid");
		return false;
	}
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypeConvertWrap(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
			"format", "the runtime type descriptor is invalid");
		return false;
	}
	pPrevious = xrtGetError();
	if ( (pType->Ops != NULL) && (pType->Ops->Format != NULL) ) {
		bSuccess = pType->Ops->Format(
			pValue, pType, pWrite, pContext
		);
		if ( !bSuccess ) {
			if ( xrtGetError() == pPrevious ) {
				__xrtTypeConvertError(XERR_STATE,
					XTYPE_CONVERT_ERROR_OPERATION, "format",
					"the custom type formatter failed without an error");
			} else {
				__xrtTypeConvertWrap(XERR_STATE,
					XTYPE_CONVERT_ERROR_OPERATION, "format",
					"the custom type formatter failed");
			}
		}
		return bSuccess;
	}
	bSupported = false;
	if ( !__xrtTypeStringFormatBuiltin(
		pType, pValue, sBuffer, sizeof(sBuffer), &Text, &bSupported
	) ) {
		if ( !bSupported ) {
			__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
				"format", "the runtime type has no text representation");
		} else if ( xrtGetError() == pPrevious ) {
			__xrtTypeConvertError(XERR_STATE,
				XTYPE_CONVERT_ERROR_OPERATION, "format",
				"the built-in type formatter failed without an error");
		} else {
			__xrtTypeConvertWrap(XERR_STATE,
				XTYPE_CONVERT_ERROR_OPERATION, "format",
				"the source value could not be formatted");
		}
		return false;
	}
	pPrevious = xrtGetError();
	bSuccess = pWrite(Text, pContext);
	if ( !bSuccess ) {
		if ( xrtGetError() == pPrevious ) {
			__xrtTypeConvertError(XERR_STATE,
				XTYPE_CONVERT_ERROR_OPERATION, "format",
				"the type format writer failed without an error");
		} else {
			__xrtTypeConvertWrap(XERR_STATE,
				XTYPE_CONVERT_ERROR_OPERATION, "format",
				"the type format writer failed");
		}
	}
	return bSuccess;
}



/* 把一个借用类型值格式化为新分配的零结尾 UTF-8 字符串。 */
XRT_API str xrtTypeToString(
	const xrttype* pType,
	const void* pValue
)
{
	xstrbuf Buffer;
	str sResult;

	xrtStrBufInit(&Buffer);
	if ( !xrtTypeFormat(
		pType, pValue, __xrtTypeStringBufferWrite, &Buffer
	) ) {
		xrtStrBufFree(&Buffer);
		return NULL;
	}
	sResult = xrtStrBufTake(&Buffer);
	if ( sResult == NULL ) {
		__xrtTypeConvertWrap(XERR_MEMORY, XTYPE_CONVERT_ERROR_OPERATION,
			"to-string", "the formatted string could not be allocated");
	}
	xrtStrBufFree(&Buffer);
	return sResult;
}



/* 格式化成功后原子替换目标拥有的旧字符串。 */
static bool __xrtTypeStringFormatReplace(
	const xrttype* pSourceType,
	const void* pSource,
	ptr pTarget
)
{
	str sResult;
	str sPrevious;

	sResult = xrtTypeToString(pSourceType, pSource);
	if ( sResult == NULL ) {
		__xrtTypeConvertWrap(XERR_STATE, XTYPE_CONVERT_ERROR_OPERATION,
			"format-string", "the source value could not be formatted");
		return false;
	}
	memcpy(&sPrevious, pTarget, sizeof(sPrevious));
	memcpy(pTarget, &sResult, sizeof(sResult));
	xrtFree(sPrevious);
	return true;
}



/* 执行可选文本扩展转换，失败时保持已初始化目标不变。 */
bool __xrtTypeStringConvert(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget
)
{
	if ( __xrtTypeConvertCanonicalString(pSourceType) ) {
		return __xrtTypeStringParse(pSource, pTargetType, pTarget);
	}
	if ( __xrtTypeConvertCanonicalString(pTargetType) ) {
		return __xrtTypeStringFormatReplace(
			pSourceType, pSource, pTarget
		);
	}
	__xrtTypeConvertError(XERR_TYPE, XTYPE_CONVERT_ERROR_TYPE,
		"convert-string", "the conversion does not use the canonical string type");
	return false;
}

#endif
