#ifndef XRT_RUNTIME_VALUE_INTERNAL_H
#define XRT_RUNTIME_VALUE_INTERNAL_H

#include "xrt_value.h"
#include <xrt/runtime_value.h>

#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
	#include <xrt/runtime_type_future.h>
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)

/* 内建 Value 槽描述供动态字段等复合运行时类型建立静态泛型实参。 */
extern const xrttype __xrtTypeValueDescriptor;

#endif

#endif
