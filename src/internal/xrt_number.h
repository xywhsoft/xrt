#ifndef XRT_INTERNAL_NUMBER_H
#define XRT_INTERNAL_NUMBER_H

#include "xrt_internal.h"

#include <xrt/number.h>



#if defined(XRT_FEATURE_NUMBER_INTEGER) || defined(XRT_FEATURE_NUMBER_FLOAT)

/* 设置数值模块共享的结构化错误。 */
static inline void __xrtNumberError(
	xerrkind Kind,
	xnumbererror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.number";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 判断一个字节是否为数值解析可选忽略的 ASCII 空白。 */
static inline bool __xrtNumberAsciiSpace(uint8 iByte)
{
	return (iByte == (uint8)' ') || (iByte == (uint8)'\t') ||
		(iByte == UINT8_C(0x0B)) || (iByte == UINT8_C(0x0C)) ||
		(iByte == (uint8)'\r') || (iByte == (uint8)'\n');
}



/* 完成数值文本查询或写入，并保证容量失败前不修改输出。 */
static inline bool __xrtNumberWriteResult(
	const char* sText,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	if ( sOutput == NULL ) {
		*pOutputSize = iSize;
		return true;
	}
	if ( iCapacity <= iSize ) {
		*pOutputSize = iSize;
		__xrtErrorSetRange();
		return false;
	}
	memcpy(sOutput, sText, iSize);
	sOutput[iSize] = 0;
	*pOutputSize = iSize;
	return true;
}

#endif



#if defined(XRT_FEATURE_NUMBER_FLOAT)

/*
	把已经完成语法检查和归一化的十进制有效数字转换为 double 位模式。
	该内部入口只接收正数；符号由公开解析层在成功后附加。
*/
bool __xrtNumberFloatConvert(
	const uint8* pDigits,
	uint32 iDigitCount,
	int32 iDigitExponent,
	uint64 iSignificand,
	int32 iSignificandExponent,
	uint64* pBits
);



/* 把 double 写成最短往返文本；调用方必须提供至少 40 字节。 */
size_t __xrtNumberFloatFormat(
	double fValue,
	char* sOutput,
	bool bCompact
);

#endif

#endif
