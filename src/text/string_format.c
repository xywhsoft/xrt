#include "../internal/xrt_format.h"
#include "../internal/xrt_string.h"



#if defined(XRT_FEATURE_STRING_FORMAT)

#define XRT_FORMAT_STACK_SIZE 512u

/* 使用 printf 规则和已有参数列表创建字符串。 */
XRT_API str xrtFormatV(cstr sFormat, va_list Args)
{
	int iSize;
	str sResult;

	if ( sFormat == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtFormatSafe(sFormat) ) {
		__xrtStrSetValueError(XSTR_ERROR_FORMAT, "format",
			"%n is not allowed by the safe string formatter");
		return NULL;
	}
	iSize = __xrtFormatMeasure(sFormat, Args);
	if ( iSize < 0 ) {
		__xrtStrSetValueError(XSTR_ERROR_FORMAT, "format", "invalid string format");
		return NULL;
	}
	sResult = (str)xrtMalloc((size_t)iSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	if ( __xrtFormatWrite(sResult, (size_t)iSize + 1u, sFormat, Args) != iSize ) {
		xrtFree(sResult);
		__xrtStrSetValueError(XSTR_ERROR_FORMAT, "format", "string format result changed while writing");
		return NULL;
	}
	return sResult;
}



/* 使用 printf 规则创建字符串。 */
XRT_API str xrtFormat(cstr sFormat, ...)
{
	va_list Args;
	str sResult;

	va_start(Args, sFormat);
	sResult = xrtFormatV(sFormat, Args);
	va_end(Args);
	return sResult;
}



/* 使用 printf 规则和已有参数列表直接追加到构建器。 */
XRT_API bool xrtStrBufAppendFormatV(xstrbuf* pBuffer, cstr sFormat, va_list Args)
{
	char arrStack[XRT_FORMAT_STACK_SIZE];
	str sOutput = arrStack;
	int iSize;
	bool bResult;

	if ( pBuffer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtStrBufReserve(pBuffer, pBuffer->Size) ) {
		return false;
	}
	if ( sFormat == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFormatSafe(sFormat) ) {
		__xrtStrSetValueError(XSTR_ERROR_FORMAT, "format",
			"%n is not allowed by the safe string formatter");
		return false;
	}
	iSize = __xrtFormatMeasure(sFormat, Args);
	if ( iSize < 0 ) {
		__xrtStrSetValueError(XSTR_ERROR_FORMAT, "format", "invalid string format");
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	if ( ((size_t)iSize + 1u) > sizeof(arrStack) ) {
		sOutput = (str)xrtMalloc((size_t)iSize + 1u);
		if ( sOutput == NULL ) {
			return false;
		}
	}
	if ( __xrtFormatWrite(sOutput, (size_t)iSize + 1u, sFormat, Args) != iSize ) {
		if ( sOutput != arrStack ) {
			xrtFree(sOutput);
		}
		__xrtStrSetValueError(XSTR_ERROR_FORMAT, "format", "string format result changed while writing");
		return false;
	}
	bResult = xrtStrBufAppend(pBuffer, xrtStrViewN(sOutput, (size_t)iSize));
	if ( sOutput != arrStack ) {
		xrtFree(sOutput);
	}
	return bResult;
}



/* 使用 printf 规则直接追加到构建器。 */
XRT_API bool xrtStrBufAppendFormat(xstrbuf* pBuffer, cstr sFormat, ...)
{
	va_list Args;
	bool bResult;

	va_start(Args, sFormat);
	bResult = xrtStrBufAppendFormatV(pBuffer, sFormat, Args);
	va_end(Args);
	return bResult;
}

#undef XRT_FORMAT_STACK_SIZE

#endif
