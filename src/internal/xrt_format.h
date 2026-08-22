#ifndef XRT_INTERNAL_FORMAT_H
#define XRT_INTERNAL_FORMAT_H

#include <stdarg.h>

#include "xrt_internal.h"



/* 验证 printf 格式不会在双遍处理中修改调用方内存。 */
bool __xrtFormatSafe(cstr sFormat);



/* 在不消耗调用方参数列表的情况下计算格式化长度。 */
int __xrtFormatMeasure(cstr sFormat, va_list Args);



/* 在不消耗调用方参数列表的情况下写入格式化文本。 */
int __xrtFormatWrite(
	str sOutput,
	size_t iCapacity,
	cstr sFormat,
	va_list Args
);

#endif
