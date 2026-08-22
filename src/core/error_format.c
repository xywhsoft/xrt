#include "../internal/xrt_format.h"



#if defined(XRT_FEATURE_ERROR_FORMAT)

#define XRT_ERROR_FORMAT_STACK_SIZE 512u



/* 使用 printf 规则创建常用错误并直接设置到当前执行上下文。 */
XRT_API void xrtSetErrorFormat(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sFormat,
	...
)
{
	char arrLocal[XRT_ERROR_FORMAT_STACK_SIZE];
	str sMessage = arrLocal;
	va_list Args;
	int iLength;

	if ( sFormat == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	if ( !__xrtFormatSafe(sFormat) ) {
		xrtSetErrorInfo(
			XERR_VALUE,
			"xrt.error",
			1,
			"%n is not allowed by the safe error formatter"
		);
		return;
	}
	va_start(Args, sFormat);
	iLength = __xrtFormatMeasure(sFormat, Args);
	if ( iLength < 0 ) {
		va_end(Args);
		xrtSetErrorInfo(XERR_VALUE, "xrt.error", 2, "invalid error format");
		return;
	}
	if ( ((size_t)iLength + 1u) > sizeof(arrLocal) ) {
		sMessage = (str)xrtMalloc((size_t)iLength + 1u);
		if ( sMessage == NULL ) {
			va_end(Args);
			return;
		}
	}
	if (
		__xrtFormatWrite(
			sMessage,
			(size_t)iLength + 1u,
			sFormat,
			Args
		) != iLength
	) {
		if ( sMessage != arrLocal ) {
			xrtFree(sMessage);
		}
		va_end(Args);
		xrtSetErrorInfo(
			XERR_VALUE,
			"xrt.error",
			3,
			"error format result changed while writing"
		);
		return;
	}
	va_end(Args);
	xrtSetErrorInfo(Kind, sDomain, iCode, sMessage);
	if ( sMessage != arrLocal ) {
		xrtFree(sMessage);
	}
}

#undef XRT_ERROR_FORMAT_STACK_SIZE

#endif
