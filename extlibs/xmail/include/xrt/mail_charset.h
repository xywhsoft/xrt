#ifndef XRT_MAIL_CHARSET_H
#define XRT_MAIL_CHARSET_H

#include <xrt/charset.h>
#include <xrt/mail.h>



#if defined(XMAIL_FEATURE_MAIL_CHARSET) && !defined(XRT_FEATURE_UNICODE)
	#error "XMAIL_FEATURE_MAIL_CHARSET requires XRT Unicode features"
#endif



#if defined(XMAIL_FEATURE_MAIL_CHARSET)

XRT_EXTERN_C_BEGIN



/* 判断字符集名称是否属于内置的小型转换集合。 */
XRT_API bool xrtMailCharsetSupported(xstrview Charset);



/*
	把 UTF-8、ASCII、Latin-1 或 Windows-1252 字节转换成 UTF-8。
	查询模式使用空输出和零容量，实际容量必须包含末尾零字节。
*/
XRT_API bool xrtMailCharsetToUtf8Write(
	xstrview Charset,
	xbytesview Source,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 UTF-8 文本。 */
XRT_API str xrtMailCharsetToUtf8(
	xstrview Charset,
	xbytesview Source,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
