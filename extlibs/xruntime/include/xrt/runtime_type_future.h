#ifndef XRT_RUNTIME_TYPE_FUTURE_H
#define XRT_RUNTIME_TYPE_FUTURE_H

#include <xrt/future.h>
#include <xrt/runtime_type.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE) && \
	!defined(XRT_FEATURE_FUTURE)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE requires XRT_FEATURE_FUTURE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE)

XRT_EXTERN_C_BEGIN



/* 返回拥有一个 Future 消费端引用的 C ABI 槽类型描述。 */
XRT_API const xrttype* xrtTypeFuture(void);



XRT_EXTERN_C_END

#endif

#endif
