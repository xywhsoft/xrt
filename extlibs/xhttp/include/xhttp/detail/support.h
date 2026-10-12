#ifndef XHTTP_DETAIL_SUPPORT_H
#define XHTTP_DETAIL_SUPPORT_H

#include <xrt/error.h>
#include <xrt/memory.h>

#if defined(XRT_FEATURE_HTTP)
	#include <xrt/http.h>
#endif

#include <limits.h>
#include <string.h>

#if defined(_MSC_VER)
	#define XHTTP_ALIGNOF(Type) __alignof(Type)
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
	#define XHTTP_ALIGNOF(Type) __alignof__(Type)
#elif defined(__cplusplus)
	#define XHTTP_ALIGNOF(Type) alignof(Type)
#else
	#define XHTTP_ALIGNOF(Type) _Alignof(Type)
#endif

/* TCC i386 自动对象只保证指针对齐，与核心的公开对象契约一致。 */
#if defined(__TINYC__) && defined(__i386__)
	#define XHTTP_OBJECT_ALIGNOF(Type) \
		((XHTTP_ALIGNOF(Type) > sizeof(void*)) ? sizeof(void*) : XHTTP_ALIGNOF(Type))
#else
	#define XHTTP_OBJECT_ALIGNOF(Type) XHTTP_ALIGNOF(Type)
#endif

/* 扩展只通过公共错误接口发布结构化错误。 */
static inline void __xhttpErrorSetDetail(
	xerrkind Kind, cstr sDomain, int32 iCode,
	cstr sOperation, cstr sMessage, const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}

/* 包装当前错误并释放临时引用。 */
static inline void __xhttpErrorWrapDetail(
	xerrkind DefaultKind, cstr sDomain, int32 iCode,
	cstr sOperation, cstr sMessage
)
{
	xerror* pCause = xrtTakeError();

	__xhttpErrorSetDetail(
		pCause != NULL ? xrtErrorKind(pCause) : DefaultKind,
		sDomain, iCode, sOperation, sMessage, pCause
	);
	if ( (xrtGetError() == NULL) && (pCause != NULL) ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 设置 xhttp 参数错误。 */
static inline void __xhttpErrorSetInvalidArgument(void)
{
	xrtSetErrorInfo(
		XERR_ARGUMENT, "xhttp", 1, "invalid argument"
	);
}



/* 设置 xhttp 非法值错误。 */
static inline void __xhttpErrorSetValue(void)
{
	xrtSetErrorInfo(
		XERR_VALUE, "xhttp", 2, "invalid value"
	);
}



/* 设置 xhttp 状态错误。 */
static inline void __xhttpErrorSetInvalidState(void)
{
	xrtSetErrorInfo(
		XERR_STATE, "xhttp", 3, "invalid state"
	);
}



/* 设置 xhttp 大小溢出错误。 */
static inline void __xhttpErrorSetSizeOverflow(void)
{
	xrtSetErrorInfo(
		XERR_RANGE, "xhttp", 4, "size overflow"
	);
}



/* 设置 xhttp 内存分配失败。 */
static inline void __xhttpErrorSetOutOfMemory(void)
{
	xrtSetErrorInfo(
		XERR_MEMORY, "xhttp", 7, "out of memory"
	);
}



/* 设置 xhttp 范围错误。 */
static inline void __xhttpErrorSetRange(void)
{
	xrtSetErrorInfo(
		XERR_RANGE, "xhttp", 5, "range is out of bounds"
	);
}



/* 设置 xhttp 内部契约错误。 */
static inline void __xhttpErrorSetInternal(void)
{
	xrtSetErrorInfo(
		XERR_INTERNAL, "xhttp", 6, "internal contract violation"
	);
}



/* 将错误所有权转移到当前执行上下文。 */
static inline void __xhttpErrorSetOwned(xerror* pError)
{
	xrtSetErrorTake(pError);
}



/* 静默交换当前错误所有权。 */
static inline xerror* __xhttpErrorSwapOwned(xerror* pError)
{
	xerror* pPrevious = xrtTakeError();

	xrtSetErrorTake(pError);
	return pPrevious;
}



/* 验证借用文本是一段不会发生地址回绕的连续内存。 */
static inline bool __xhttpViewValid(xstrview Text)
{
	if ( !xrtMemRangeValid(Text.Data, Text.Size) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 按 ASCII 规则把大写字母转换为小写。 */
static inline unsigned char __xhttpAsciiLower(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(iByte +
			((unsigned char)'a' - (unsigned char)'A'));
	}
	return iByte;
}



#if defined(XRT_FEATURE_HTTP)

/* 从已经验证的字段数组复制一个描述符，兼容未对齐存储。 */
static inline void __xhttpFieldLoad(
	const xhttpfield* pFields,
	size_t iIndex,
	xhttpfield* pField
)
{
	memcpy(
		pField,
		((const uint8*)pFields) + (iIndex * sizeof(*pFields)),
		sizeof(*pField)
	);
}



/* 验证字段描述符数组及其借用文本的基础内存边界。 */
static inline bool __xhttpFieldArrayValid(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpfield Field;
	size_t iBytes;
	size_t i;

	if ( ((pFields == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pFields))) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	iBytes = iCount * sizeof(*pFields);
	if ( !xrtMemRangeValid(pFields, iBytes) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xhttpFieldLoad(pFields, i, &Field);
		if ( !__xhttpViewValid(Field.Name) ||
			!__xhttpViewValid(Field.Value) ) {
			return false;
		}
	}
	return true;
}



/* 判断输出内存是否覆盖字段数组或任意借用文本。 */
static inline bool __xhttpFieldArrayOverlap(
	const xhttpfield* pFields,
	size_t iCount,
	const void* pMemory,
	size_t iSize
)
{
	xhttpfield Field;
	size_t i;

	if ( xrtMemRangesOverlap(
		pFields,
		iCount * sizeof(*pFields),
		pMemory,
		iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xhttpFieldLoad(pFields, i, &Field);
		if ( xrtMemRangesOverlap(
			Field.Name.Data,
			Field.Name.Size,
			pMemory,
			iSize
		) || xrtMemRangesOverlap(
			Field.Value.Data,
			Field.Value.Size,
			pMemory,
			iSize
		) ) {
			return true;
		}
	}
	return false;
}


/* 验证 Header 查询使用的非空字段名。 */
static inline bool __xhttpLookupNameValid(xstrview Name)
{
	if ( !__xhttpViewValid(Name) ) {
		return false;
	}
	if ( !xrtHttpTokenValid(Name) ) {
		__xhttpErrorSetValue();
		return false;
	}
	return true;
}

#endif




#if defined(XRT_FEATURE_HTTP)

/* 消息写出只保存调用方缓冲和当前长度，不持有传输对象。 */
static inline bool __xhttpSizeAdd(size_t* pSize, size_t iAdd)
{
 if (*pSize > SIZE_MAX - iAdd) { xrtSetErrorKind(XERR_RANGE); return false; }
 *pSize += iAdd;
 return true;
}

static inline bool __xhttpViewEqual(xstrview Left, xstrview Right)
{
 return Left.Size == Right.Size && (Left.Size == 0 ||
  memcmp(Left.Data, Right.Data, Left.Size) == 0);
}

static inline bool __xhttpTokenByte(unsigned char iByte)
{
 char iValue = (char)iByte;
 return xrtHttpTokenValid((xstrview){ &iValue, 1u });
}

static inline bool __xhttpQuotedTextByte(unsigned char iByte)
{
 return iByte == 9u || iByte == 32u || iByte == 33u ||
  (iByte >= 35u && iByte <= 91u) || (iByte >= 93u && iByte <= 126u) || iByte >= 128u;
}

static inline bool __xhttpQuotedPairByte(unsigned char iByte)
{
 return iByte == 9u || (iByte >= 32u && iByte != 127u);
}

static inline size_t __xhttpUInt64Size(uint64 iValue)
{
 size_t iSize = 1u;
 while (iValue >= UINT64_C(10)) { iValue /= UINT64_C(10); iSize++; }
 return iSize;
}

static inline size_t __xhttpUInt64Write(char* sOutput, uint64 iValue)
{
 size_t iSize = __xhttpUInt64Size(iValue);
 size_t i = iSize;
 do { sOutput[--i] = (char)('0' + (char)(iValue % UINT64_C(10)));
  iValue /= UINT64_C(10); } while (iValue != 0);
 return iSize;
}
#endif

#if defined(XRT_FEATURE_HTTP_PARAM)

typedef struct __xhttp_param_writer {
 bytes Output;
 size_t Size;
} __xhttp_param_writer;

static inline size_t __xhttpQuotedWriteUnchecked(xstrview Value, bytes pOutput)
{
 size_t iSize = 0;
 if (!xrtHttpQuotedWrite(Value, NULL, 0, &iSize)) return 0;
 if (!xrtHttpQuotedWrite(Value, pOutput, iSize, &iSize)) return 0;
 return iSize;
}

static inline size_t __xhttpParamValueWriteUnchecked(const xhttpparam* pParam, bytes pOutput)
{
 size_t iSize = 0;
 if (!xrtHttpParamValueWrite(pParam, NULL, 0, &iSize)) return 0;
 if (!xrtHttpParamValueWrite(pParam, pOutput, iSize, &iSize)) return 0;
 return iSize;
}

/* 仅在核心参数解析器已验证值之后遍历转义字节。 */
static inline bool __xhttpParamSemanticNext(const xhttpparam* pParam, size_t* pOffset, uint8* pByte)
{
 size_t i = *pOffset;
 if (i >= pParam->Value.Size) return false;
 *pByte = (uint8)pParam->Value.Data[i++];
 if ((pParam->Flags & XHTTP_PARAM_QUOTED) && *pByte == (uint8)'\\')
  *pByte = (uint8)pParam->Value.Data[i++];
 *pOffset = i;
 return true;
}

static inline bool __xhttpParamWriterBytes(
	__xhttp_param_writer* pWriter,
	const void* pData,
	size_t iSize
)
{
	if ( pWriter->Size > (SIZE_MAX - iSize) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( pWriter->Output != NULL ) {
		memcpy(pWriter->Output + pWriter->Size, pData, iSize);
	}
	pWriter->Size += iSize;
	return true;
}

static inline bool __xhttpParamWriterName(
	__xhttp_param_writer* pWriter,
	xstrview Name,
	bool bFirst
)
{
	return (bFirst || __xhttpParamWriterBytes(
		pWriter, ", ", 2u
	)) && __xhttpParamWriterBytes(
		pWriter, Name.Data, Name.Size
	) && __xhttpParamWriterBytes(pWriter, "=", 1u);
}

static inline bool __xhttpParamWriterQuoted(
	__xhttp_param_writer* pWriter,
	xstrview Name,
	xstrview Value,
	bool bFirst
)
{
	size_t iQuoted;

	if ( !xrtHttpQuotedWrite(Value, NULL, 0, &iQuoted) ) {
		return false;
	}
	if ( !__xhttpParamWriterName(pWriter, Name, bFirst) ) {
		return false;
	}
	if ( pWriter->Size > (SIZE_MAX - iQuoted) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( pWriter->Output != NULL ) {
		(void)__xhttpQuotedWriteUnchecked(
			Value, pWriter->Output + pWriter->Size
		);
	}
	pWriter->Size += iQuoted;
	return true;
}

static inline bool __xhttpParamWriterToken(
	__xhttp_param_writer* pWriter,
	xstrview Name,
	xstrview Value,
	bool bFirst
)
{
	return __xhttpParamWriterName(pWriter, Name, bFirst) &&
		__xhttpParamWriterBytes(
			pWriter, Value.Data, Value.Size
		);
}

#endif

#endif
