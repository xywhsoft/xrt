#ifndef XRT_RUNTIME_CONVERT_H
#define XRT_RUNTIME_CONVERT_H

#include <xrt/runtime_type.h>

#if defined(XRUNTIME_FEATURE_VALUE_CONVERT)
	#include <xrt/value.h>
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT) && \
	!defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_RUNTIME_CONVERT requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING) && \
	(!defined(XRUNTIME_FEATURE_RUNTIME_CONVERT) || \
	 !defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING) || \
	 !defined(XRT_FEATURE_NUMBER_INTEGER) || \
	 !defined(XRT_FEATURE_NUMBER_FLOAT) || \
	 !defined(XRT_FEATURE_TIME_TEXT))
	#error "XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING requires RUNTIME_CONVERT, RUNTIME_TYPE_STRING, NUMBER_INTEGER, NUMBER_FLOAT and TIME_TEXT"
#endif

#if defined(XRUNTIME_FEATURE_VALUE_CONVERT) && \
	(!defined(XRUNTIME_FEATURE_RUNTIME_CONVERT) || \
	 !defined(XRT_FEATURE_VALUE))
	#error "XRUNTIME_FEATURE_VALUE_CONVERT requires RUNTIME_CONVERT and VALUE"
#endif

#if defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING) && \
	(!defined(XRUNTIME_FEATURE_VALUE_CONVERT) || \
	 !defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING))
	#error "XRUNTIME_FEATURE_VALUE_CONVERT_STRING requires VALUE_CONVERT and RUNTIME_CONVERT_STRING"
#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT)

/* 转换模式逐级包含：显式转换也允许无损拓宽和同类型复制。 */
typedef enum xtypeconvertmode {
	XTYPE_CONVERT_EXACT = 0,
	XTYPE_CONVERT_WIDEN,
	XTYPE_CONVERT_EXPLICIT
} xtypeconvertmode;



/* 运行时类型转换层稳定错误代码。 */
typedef enum xtypeconverterror {
	XTYPE_CONVERT_ERROR_ARGUMENT = 1,
	XTYPE_CONVERT_ERROR_MODE,
	XTYPE_CONVERT_ERROR_TYPE,
	XTYPE_CONVERT_ERROR_RANGE,
	XTYPE_CONVERT_ERROR_PARSE,
	XTYPE_CONVERT_ERROR_OPERATION
} xtypeconverterror;



XRT_EXTERN_C_BEGIN



/* 判断源类型的全部有效值是否都能被目标类型无损表示。 */
XRT_API bool xrtTypeCanWiden(
	const xrttype* pSourceType,
	const xrttype* pTargetType
);



/* 判断两个类型在指定模式下是否存在稳定的内建转换路径。 */
XRT_API bool xrtTypeCanConvert(
	const xrttype* pSourceType,
	const xrttype* pTargetType,
	xtypeconvertmode Mode
);



/*
	把借用的源值转换后写入已经初始化的目标值。
	失败时目标保持不变；不同类型的源和目标存储不得重叠。
*/
XRT_API bool xrtTypeConvert(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)

XRT_EXTERN_C_BEGIN



/*
	把一个借用类型值同步分块格式化为 UTF-8 文本。
	内建类型不分配中间字符串；writer 接收的分块只在回调期间有效。
*/
XRT_API bool xrtTypeFormat(
	const xrttype* pType,
	const void* pValue,
	xrttypewriter pWrite,
	ptr pContext
);



/* 把借用类型值格式化为由 xrtFree 释放的零结尾 UTF-8 字符串。 */
XRT_API str xrtTypeToString(
	const xrttype* pType,
	const void* pValue
);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_VALUE_CONVERT)

XRT_EXTERN_C_BEGIN



/*
	把动态 Value 标量转换后写入已经初始化的目标值。
	失败时目标保持不变；容器、字节和句柄不属于标量转换。
*/
XRT_API bool xrtValueConvertTo(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	xtypeconvertmode Mode
);



XRT_EXTERN_C_END

#endif

#endif
