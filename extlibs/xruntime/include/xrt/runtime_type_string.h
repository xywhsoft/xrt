#ifndef XRT_RUNTIME_TYPE_STRING_H
#define XRT_RUNTIME_TYPE_STRING_H

#include <xrt/hash.h>
#include <xrt/runtime_type.h>
#include <xrt/string.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) && \
	!defined(XRT_FEATURE_STRING)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_STRING requires XRT_FEATURE_STRING"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) && \
	!defined(XRT_FEATURE_HASH64)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_STRING requires XRT_FEATURE_HASH64"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_STRING requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING)

XRT_EXTERN_C_BEGIN



/* 返回拥有一个零结尾 XRT 字符串的 C ABI 槽类型描述。 */
XRT_API const xrttype* xrtTypeString(void);



XRT_EXTERN_C_END

#endif

#endif
