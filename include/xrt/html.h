#ifndef XRT_HTML_H
#define XRT_HTML_H

#include <xrt/error.h>
#include <xrt/memory.h>



#if defined(XRT_FEATURE_HTML_ESCAPE) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT_FEATURE_HTML_ESCAPE requires XRT_FEATURE_UNICODE"
#endif



#if defined(XRT_FEATURE_HTML_ESCAPE)

/* HTML 转义上下文；属性模式只适用于由引号包围的属性值。 */
typedef enum xhtmlescapemode {
	XHTML_ESCAPE_TEXT = 0,
	XHTML_ESCAPE_ATTRIBUTE
} xhtmlescapemode;



/* HTML 文本原语的稳定错误代码。 */
typedef enum xhtmlerror {
	XHTML_ERROR_MODE = 1,
	XHTML_ERROR_UTF8
} xhtmlerror;



XRT_EXTERN_C_BEGIN



/* 严格校验 UTF-8 并返回转义后的精确字节数，不包含末尾零。 */
XRT_API bool xrtHtmlEscapeSize(
	xstrview Text,
	xhtmlescapemode Mode,
	size_t* pOutputSize
);



/* 转义到调用方缓冲区；容量必须包含末尾零，空输出可只查询长度。 */
XRT_API bool xrtHtmlEscapeWrite(
	xstrview Text,
	xhtmlescapemode Mode,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的零结尾转义文本，长度输出可以为空。 */
XRT_API str xrtHtmlEscape(
	xstrview Text,
	xhtmlescapemode Mode,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
