#ifndef XRT_NUMBER_H
#define XRT_NUMBER_H

#include <xrt/error.h>
#include <xrt/memory.h>



#if defined(XRT_FEATURE_NUMBER_FORMAT) && \
	(!defined(XRT_FEATURE_NUMBER_INTEGER) || \
	 !defined(XRT_FEATURE_NUMBER_FLOAT) || \
	 !defined(XRT_FEATURE_UNICODE))
	#error "XRT number format requires integer, floating-point and Unicode features"
#endif



#if defined(XRT_FEATURE_NUMBER_INTEGER) || \
	defined(XRT_FEATURE_NUMBER_FLOAT) || defined(XRT_FEATURE_NUMBER_FORMAT)

/*
	数值文本解析标志。
	默认严格解析完整文本；空白、进制前缀、数字分隔符和特殊浮点值均需显式开启。
*/
typedef enum xnumberparseflag {
	XNUMBER_PARSE_SPACE = UINT32_C(0x00000001),
	XNUMBER_PARSE_PREFIX = UINT32_C(0x00000002),
	XNUMBER_PARSE_SEPARATOR = UINT32_C(0x00000004),
	XNUMBER_PARSE_SPECIAL = UINT32_C(0x00000008)
} xnumberparseflag;



/* 数值模块稳定错误码。 */
typedef enum xnumbererror {
	XNUMBER_ERROR_CONFIG = 1201,
	XNUMBER_ERROR_FORMAT = 1202,
	XNUMBER_ERROR_RANGE = 1203
} xnumbererror;



XRT_EXTERN_C_BEGIN

#endif



#if defined(XRT_FEATURE_NUMBER_INTEGER)

/* 整数文本输出标志；默认使用小写数字且不添加前缀或正号。 */
typedef enum xnumberwriteflag {
	XNUMBER_UPPER = UINT32_C(0x00000001),
	XNUMBER_PREFIX = UINT32_C(0x00000002),
	XNUMBER_PLUS = UINT32_C(0x00000004)
} xnumberwriteflag;



/*
	按 2 到 36 进制写出无符号整数。
	输出为空且容量为零时只查询长度；实际写入要求容量额外包含末尾零字节。
*/
XRT_API bool xrtUIntWrite(
	uint64 iValue,
	uint32 iBase,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	uint32 iFlags
);



/*
	按 2 到 36 进制写出有符号整数。
	负号位于进制前缀之前；XNUMBER_PLUS 只影响非负值。
*/
XRT_API bool xrtIntWrite(
	int64 iValue,
	uint32 iBase,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	uint32 iFlags
);



/* 写出无符号整数并返回由 xrtFree 释放的末尾补零文本。 */
XRT_API str xrtUIntString(
	uint64 iValue,
	uint32 iBase,
	uint32 iFlags
);



/* 写出有符号整数并返回由 xrtFree 释放的末尾补零文本。 */
XRT_API str xrtIntString(
	int64 iValue,
	uint32 iBase,
	uint32 iFlags
);



/*
	严格解析无符号整数。
	iBase 为零时默认十进制，并在允许前缀时自动识别 0b、0o、0x。
*/
XRT_API bool xrtUIntParse(
	xstrview Text,
	uint32 iBase,
	uint32 iFlags,
	uint64* pValue
);



/*
	严格解析有符号整数。
	正负号必须位于可选进制前缀之前，溢出时保持输出不变。
*/
XRT_API bool xrtIntParse(
	xstrview Text,
	uint32 iBase,
	uint32 iFlags,
	int64* pValue
);

#endif



#if defined(XRT_FEATURE_NUMBER_FLOAT)

/*
	浮点文本输出标志。
	默认保留整数型 double 的 .0；紧凑模式只保留数值往返所需字符。
*/
typedef enum xnumberfloatflag {
	XNUMBER_FLOAT_COMPACT = UINT32_C(0x00000001)
} xnumberfloatflag;



/*
	写出 IEEE-754 double 的最短往返文本。
	输出为空且容量为零时只查询长度；负零、无穷和 NaN 均有稳定表示。
*/
XRT_API bool xrtNumWrite(
	double fValue,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	uint32 iFlags
);



/* 写出 double 并返回由 xrtFree 释放的末尾补零文本。 */
XRT_API str xrtNumString(
	double fValue,
	uint32 iFlags
);



/*
	严格解析完整十进制浮点文本并执行 IEEE-754 正确舍入。
	默认接受前导正负号、前置或后置小数点；特殊值和分隔符需显式开启。
*/
XRT_API bool xrtNumParse(
	xstrview Text,
	uint32 iFlags,
	double* pValue
);

#endif



#if defined(XRT_FEATURE_NUMBER_FORMAT)

/*
	格式语法固定为：
	[+|-][#][0][width][,|_][.precision][type]
	整数类型为 d/x/X/o/b/B/c，浮点类型为 f/F/e/E/g/G/%。
	c 把整数解释为 Unicode 标量并写出 UTF-8，只接受可选宽度。
*/

/*
	按照展示格式写出有符号整数。
	输出为空且容量为零时只查询长度；实际容量必须额外包含末尾零字节。
*/
XRT_API bool xrtIntFormatTo(int64 iValue, xstrview Format,
	char* sOutput, size_t iCapacity, size_t* pOutputSize);



/* 按照展示格式写出无符号整数。 */
XRT_API bool xrtUIntFormatTo(uint64 iValue, xstrview Format,
	char* sOutput, size_t iCapacity, size_t* pOutputSize);



/* 按照展示格式写出 double，固定支持正确舍入、负零和特殊值。 */
XRT_API bool xrtNumFormatTo(double fValue, xstrview Format,
	char* sOutput, size_t iCapacity, size_t* pOutputSize);



/* 格式化有符号整数并返回由 xrtFree 释放的零结尾字符串。 */
XRT_API str xrtIntFormat(int64 iValue, xstrview Format);



/* 格式化无符号整数并返回由 xrtFree 释放的零结尾字符串。 */
XRT_API str xrtUIntFormat(uint64 iValue, xstrview Format);



/* 格式化 double 并返回由 xrtFree 释放的零结尾字符串。 */
XRT_API str xrtNumFormat(double fValue, xstrview Format);

#endif



#if defined(XRT_FEATURE_NUMBER_INTEGER) || \
	defined(XRT_FEATURE_NUMBER_FLOAT) || defined(XRT_FEATURE_NUMBER_FORMAT)

XRT_EXTERN_C_END

#endif

#endif
