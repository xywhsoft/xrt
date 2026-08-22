#ifndef XRT_ERROR_FORMAT_H
#define XRT_ERROR_FORMAT_H

#include <xrt/error.h>



#if defined(XRT_FEATURE_ERROR_FORMAT)

XRT_EXTERN_C_BEGIN



/* 使用 printf 规则创建常用错误并直接设置到当前执行上下文。 */
XRT_API void xrtSetErrorFormat(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sFormat,
	...
);



XRT_EXTERN_C_END

#endif

#endif
