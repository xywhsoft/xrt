#include "../internal/xrt_internal.h"

#include <xrt/charset.h>
#include <xrt/html.h>



#if defined(XRT_FEATURE_HTML_ESCAPE)

/* 设置 HTML 文本原语的结构化错误。 */
static void __xrtHtmlError(
	xerrkind Kind,
	xhtmlerror Code,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.html";
	Desc.Code = (int32)Code;
	Desc.Operation = "html-escape";
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 返回当前上下文中一个字节对应的替换文本。 */
static cstr __xrtHtmlReplacement(
	uint8 iByte,
	xhtmlescapemode Mode,
	size_t* pSize
)
{
	cstr sReplacement = NULL;

	if ( iByte == (uint8)'&' ) {
		sReplacement = "&amp;";
		*pSize = 5u;
	} else if ( iByte == (uint8)'<' ) {
		sReplacement = "&lt;";
		*pSize = 4u;
	} else if ( iByte == (uint8)'>' ) {
		sReplacement = "&gt;";
		*pSize = 4u;
	} else if (
		(Mode == XHTML_ESCAPE_ATTRIBUTE) &&
		(iByte == (uint8)'\"')
	) {
		sReplacement = "&quot;";
		*pSize = 6u;
	} else if (
		(Mode == XHTML_ESCAPE_ATTRIBUTE) &&
		(iByte == (uint8)'\'')
	) {
		sReplacement = "&#39;";
		*pSize = 5u;
	} else {
		*pSize = 1u;
	}
	return sReplacement;
}



/* 校验模式与 UTF-8，并计算不会包含末尾零的精确长度。 */
static bool __xrtHtmlEncodedSize(
	xstrview Text,
	xhtmlescapemode Mode,
	size_t* pOutputSize
)
{
	size_t iRequired = 0;
	size_t iError;
	size_t i;

	if ( !__xrtRangeValid(Text.Data, Text.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(Mode != XHTML_ESCAPE_TEXT) &&
		(Mode != XHTML_ESCAPE_ATTRIBUTE)
	) {
		__xrtHtmlError(
			XERR_VALUE,
			XHTML_ERROR_MODE,
			"invalid HTML escape mode"
		);
		return false;
	}
	if ( !xrtUtf8Valid(Text, &iError) ) {
		__xrtHtmlError(
			XERR_VALUE,
			XHTML_ERROR_UTF8,
			"HTML text is not valid UTF-8"
		);
		return false;
	}
	for ( i = 0; i < Text.Size; i++ ) {
		size_t iAdd;

		(void)__xrtHtmlReplacement((uint8)Text.Data[i], Mode, &iAdd);
		if ( iRequired > (SIZE_MAX - iAdd) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iRequired += iAdd;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 在不重叠的目标上从前向后写出转义文本。 */
static void __xrtHtmlWriteForward(
	xstrview Text,
	xhtmlescapemode Mode,
	char* sOutput,
	size_t iOutputSize
)
{
	size_t iOutput = 0;
	size_t i;

	for ( i = 0; i < Text.Size; i++ ) {
		size_t iSize;
		cstr sReplacement = __xrtHtmlReplacement(
			(uint8)Text.Data[i], Mode, &iSize
		);

		if ( sReplacement != NULL ) {
			memcpy(sOutput + iOutput, sReplacement, iSize);
		} else {
			sOutput[iOutput] = Text.Data[i];
		}
		iOutput += iSize;
	}
	sOutput[iOutputSize] = '\0';
}



/* 从后向前写出转义文本，使输入和输出可以从同一地址开始。 */
static void __xrtHtmlWriteBackward(
	xstrview Text,
	xhtmlescapemode Mode,
	char* sOutput,
	size_t iOutputSize
)
{
	size_t iInput = Text.Size;
	size_t iOutput = iOutputSize;

	sOutput[iOutputSize] = '\0';
	while ( iInput != 0 ) {
		size_t iSize;
		uint8 iByte = (uint8)Text.Data[--iInput];
		cstr sReplacement = __xrtHtmlReplacement(iByte, Mode, &iSize);

		iOutput -= iSize;
		if ( sReplacement != NULL ) {
			memcpy(sOutput + iOutput, sReplacement, iSize);
		} else {
			sOutput[iOutput] = (char)iByte;
		}
	}
}



/* 严格校验 UTF-8 并返回 HTML 转义后的精确长度。 */
XRT_API bool xrtHtmlEscapeSize(
	xstrview Text,
	xhtmlescapemode Mode,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if (
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		__xrtRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size
		)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHtmlEncodedSize(Text, Mode, &iRequired) ) {
		return false;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 转义到调用方缓冲区，并保证所有失败路径不发布部分文本。 */
XRT_API bool xrtHtmlEscapeWrite(
	xstrview Text,
	xhtmlescapemode Mode,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if (
		!__xrtRangeValid(Text.Data, Text.Size) ||
		!__xrtRangeValid(sOutput, iCapacity) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHtmlEncodedSize(Text, Mode, &iRequired) ) {
		return false;
	}
	if (
		__xrtRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size
		) || ((sOutput != NULL) && __xrtRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity
		))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}
	if (
		__xrtRangesOverlap(
			sOutput, iRequired + 1u, Text.Data, Text.Size
		) && ((const void*)sOutput != (const void*)Text.Data)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (const void*)sOutput == (const void*)Text.Data ) {
		__xrtHtmlWriteBackward(Text, Mode, sOutput, iRequired);
	} else {
		__xrtHtmlWriteForward(Text, Mode, sOutput, iRequired);
	}
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的零结尾 HTML 转义文本。 */
XRT_API str xrtHtmlEscape(
	xstrview Text,
	xhtmlescapemode Mode,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if (
		!__xrtRangeValid(
			pOutputSize,
			pOutputSize != NULL ? sizeof(*pOutputSize) : 0
		) || ((pOutputSize != NULL) && __xrtRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size
		))
	) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtHtmlEncodedSize(Text, Mode, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	__xrtHtmlWriteForward(Text, Mode, sOutput, iRequired);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}

#endif
