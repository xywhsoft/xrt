#ifndef XRT_INTERNAL_RUNTIME_CONVERT_H
#define XRT_INTERNAL_RUNTIME_CONVERT_H

#include "xrt_runtime_type.h"

#include <xrt/runtime_convert.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT)

/* 判断转换模式枚举是否有效。 */
bool __xrtTypeConvertModeValid(xtypeconvertmode Mode);




/* 设置转换层结构化错误。 */
void __xrtTypeConvertError(
	xerrkind Kind,
	xtypeconverterror Code,
	cstr sOperation,
	cstr sMessage
);



/* 包装下层转换失败并保留原始错误链。 */
void __xrtTypeConvertWrap(
	xerrkind DefaultKind,
	xtypeconverterror Code,
	cstr sOperation,
	cstr sMessage
);

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING)

/* 判断可选文本扩展是否支持指定转换方向。 */
bool __xrtTypeStringCanConvert(
	const xrttype* pSourceType,
	const xrttype* pTargetType
);



/* 执行可选文本扩展转换，失败时保持已初始化目标不变。 */
bool __xrtTypeStringConvert(
	const xrttype* pSourceType,
	const void* pSource,
	const xrttype* pTargetType,
	ptr pTarget
);

#endif

#endif
