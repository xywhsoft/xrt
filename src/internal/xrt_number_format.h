#ifndef XRT_INTERNAL_NUMBER_FORMAT_H
#define XRT_INTERNAL_NUMBER_FORMAT_H

#include "xrt_charset.h"
#include "xrt_number.h"



#if defined(XRT_FEATURE_NUMBER_FORMAT)

#define XRT_NUMBER_FORMAT_PRECISION_MAX 1000u
#define XRT_NUMBER_FORMAT_CORE_CAPACITY 1408u



/* 数字展示格式的解析结果不保留输入指针，允许输出复用格式缓冲区。 */
typedef struct xrt_number_format_options {
	size_t Width;
	size_t Precision;
	char Group;
	char Type;
	bool HasPrecision;
	bool HasSign;
	bool Plus;
	bool Alternate;
	bool Zero;
} xrt_number_format_options;



/*
	构造不带符号的浮点核心文本。
	NaN 不保留 IEEE-754 符号位，以获得跨编译器一致的文本结果。
*/
bool __xrtNumberFormatFloatCore(
	double fValue,
	const xrt_number_format_options* pOptions,
	char* sCore,
	size_t* pCoreSize,
	bool* pNegative,
	bool* pSpecial
);

#endif

#endif
