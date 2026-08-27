#include "../internal/xrt_text_value.h"
#include "../internal/xrt_json_escape.h"

#include <math.h>



#if defined(XRT_FEATURE_JSON_WRITE) || defined(XRT_FEATURE_XSON_WRITE)

/* 每个容器帧只保存类型、待键状态和已完成值数量。 */
typedef struct xtextvaluewriterframe {
	size_t Count;
	uint8 Type;
	bool NeedKey;
} xtextvaluewriterframe;



/* 共享 writer 保存输出、帧栈、资源预算和回调重入状态。 */
struct xtextvaluewriter {
	xtextvaluewriteconfig Config;
	xtextvaluewriteproc Write;
	ptr WriteData;
	xtextvaluewriteerrorproc Error;
	ptr ErrorData;
	xbuffer Output;
	xbuffer Frames;
	size_t Written;
	bool Memory;
	bool RootWritten;
	bool Finished;
	bool Failed;
	bool Busy;
	bool Taken;
};



/* 建立格式层错误并关闭 writer。 */
bool __xrtTextValueWriterFail(
	xtextvaluewriter* pWriter,
	xerrkind Kind,
	xtextvaluewriteerror Code,
	cstr sMessage
)
{
	if ( pWriter != NULL ) {
		pWriter->Failed = true;
		pWriter->Error(Kind, Code, sMessage, pWriter->ErrorData);
	} else {
		__xrtErrorSetInvalidArgument();
	}
	return false;
}



/* 标记下层已经设置具体错误的 writer 为失败。 */
void __xrtTextValueWriterPoison(xtextvaluewriter* pWriter)
{
	if ( pWriter != NULL ) {
		pWriter->Failed = true;
	}
}



/* 传播下层或输出回调错误，没有新错误时建立格式输出错误。 */
static bool __xrtTextValueWriterPropagate(
	xtextvaluewriter* pWriter,
	const xerror* pPrevious,
	xerror* pHeld,
	xerrkind Kind,
	xtextvaluewriteerror Code,
	cstr sMessage
)
{
	const xerror* pCurrent = xrtGetError();
	bool bChanged = pCurrent != pPrevious;

	pWriter->Failed = true;
	xrtErrorFree(pHeld);
	if ( !bChanged ) {
		pWriter->Error(Kind, Code, sMessage, pWriter->ErrorData);
	}
	return false;
}



/* 进入一次写调用，并拒绝完成态、失败态和 sink 回调重入。 */
static bool __xrtTextValueWriterEnter(xtextvaluewriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pWriter->Busy ) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"text value writer cannot be reentered"
		);
	}
	if ( pWriter->Failed || pWriter->Finished ) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"text value writer is not writable"
		);
	}
	pWriter->Busy = true;
	return true;
}



/* 离开一次写调用，并把任意失败固化到 writer。 */
static bool __xrtTextValueWriterLeave(
	xtextvaluewriter* pWriter,
	bool bResult
)
{
	if ( !bResult ) {
		pWriter->Failed = true;
	}
	pWriter->Busy = false;
	return bResult && !pWriter->Failed;
}



/* 返回当前容器层数。 */
static size_t __xrtTextValueWriterDepth(
	const xtextvaluewriter* pWriter
)
{
	return pWriter->Frames.Size / sizeof(xtextvaluewriterframe);
}



/* 返回当前容器帧，根位置没有帧。 */
static xtextvaluewriterframe* __xrtTextValueWriterTop(
	xtextvaluewriter* pWriter
)
{
	if ( pWriter->Frames.Size == 0 ) {
		return NULL;
	}
	return (xtextvaluewriterframe*)(
		pWriter->Frames.Data +
		pWriter->Frames.Size - sizeof(xtextvaluewriterframe)
	);
}



/* 在固定输出上限内写入内存或调用方 sink。 */
static bool __xrtTextValueWriterEmit(
	xtextvaluewriter* pWriter,
	const void* pData,
	size_t iSize
)
{
	const xerror* pPrevious;
	xerror* pHeld;
	bool bResult;

	if ( iSize == 0 ) {
		return true;
	}
	if (
		(iSize > pWriter->Config.MaxOutputBytes) ||
		(pWriter->Written > (pWriter->Config.MaxOutputBytes - iSize))
	) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_RANGE,
			XTEXT_VALUE_WRITE_ERROR_LIMIT,
			"output exceeds configured limit"
		);
	}
	pPrevious = xrtGetError();
	pHeld = xrtErrorRef(pPrevious);
	if ( pWriter->Memory ) {
		bResult = xrtBufferAppend(
			&pWriter->Output,
			(xbytesview){ (cbytes)pData, iSize }
		);
	} else {
		bResult = pWriter->Write(
			(xbytesview){ (cbytes)pData, iSize },
			pWriter->WriteData
		);
	}
	if ( !bResult || pWriter->Failed ) {
		return __xrtTextValueWriterPropagate(
			pWriter,
			pPrevious,
			pHeld,
			XERR_IO,
			XTEXT_VALUE_WRITE_ERROR_OUTPUT,
			"output callback failed"
		);
	}
	xrtErrorFree(pHeld);
	pWriter->Written += iSize;
	return true;
}



/* 写出静态 ASCII 文本。 */
static bool __xrtTextValueWriterAscii(
	xtextvaluewriter* pWriter,
	cstr sText,
	size_t iSize
)
{
	return __xrtTextValueWriterEmit(pWriter, sText, iSize);
}



/* 写出指定层数的换行与空格缩进。 */
static bool __xrtTextValueWriterIndent(
	xtextvaluewriter* pWriter,
	size_t iDepth
)
{
	static const char sSpaces[] = "                ";
	size_t iCount;

	if ( (pWriter->Config.Flags & XTEXT_VALUE_WRITE_PRETTY) == 0 ) {
		return true;
	}
	if ( !__xrtTextValueWriterAscii(pWriter, "\n", 1u) ) {
		return false;
	}
	if (
		(pWriter->Config.Indent != 0) &&
		(iDepth > (SIZE_MAX / pWriter->Config.Indent))
	) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_RANGE,
			XTEXT_VALUE_WRITE_ERROR_LIMIT,
			"indentation size overflow"
		);
	}
	iCount = iDepth * pWriter->Config.Indent;
	while ( iCount != 0 ) {
		size_t iChunk = iCount < (sizeof(sSpaces) - 1u)
			? iCount
			: (sizeof(sSpaces) - 1u);

		if ( !__xrtTextValueWriterAscii(pWriter, sSpaces, iChunk) ) {
			return false;
		}
		iCount -= iChunk;
	}
	return true;
}



/* 在根、序列或已经写键的映射位置开始一个值。 */
static bool __xrtTextValueWriterBeforeValue(
	xtextvaluewriter* pWriter
)
{
	xtextvaluewriterframe* pFrame = __xrtTextValueWriterTop(pWriter);

	if ( pFrame == NULL ) {
		if ( pWriter->RootWritten ) {
			return __xrtTextValueWriterFail(
				pWriter,
				XERR_STATE,
				XTEXT_VALUE_WRITE_ERROR_STATE,
				"writer accepts exactly one root value"
			);
		}
		pWriter->RootWritten = true;
		return true;
	}
	if (
		(pFrame->Type == XTEXT_VALUE_CONTAINER_OBJECT) ||
		(pFrame->Type == XTEXT_VALUE_CONTAINER_INT_MAP)
	) {
		if ( pFrame->NeedKey ) {
			return __xrtTextValueWriterFail(
				pWriter,
				XERR_STATE,
				XTEXT_VALUE_WRITE_ERROR_STATE,
				"mapping value requires a key"
			);
		}
		pFrame->NeedKey = true;
		pFrame->Count++;
		return true;
	}
	if ( pFrame->Count != 0 ) {
		if ( !__xrtTextValueWriterAscii(pWriter, ",", 1u) ) {
			return false;
		}
	}
	if ( !__xrtTextValueWriterIndent(
		pWriter,
		__xrtTextValueWriterDepth(pWriter)
	) ) {
		return false;
	}
	pFrame->Count++;
	return true;
}



/* 把共享 JSON quote 输出桥接到当前文本 Writer。 */
static bool __xrtTextValueWriterEscapeEmit(
	const void* pData,
	size_t iSize,
	ptr pUserData
)
{
	return __xrtTextValueWriterEmit(
		(xtextvaluewriter*)pUserData,
		pData,
		iSize
	);
}



/* 严格校验并写出 JSON 兼容字符串 token。 */
static bool __xrtTextValueWriterStringToken(
	xtextvaluewriter* pWriter,
	xstrview Text
)
{
	xjsonescaperesult Result;
	size_t iErrorOffset;

	Result = __xrtJsonEscapeWrite(
		Text,
		pWriter->Config.Flags & (
			XTEXT_VALUE_WRITE_ESCAPE_SLASH |
			XTEXT_VALUE_WRITE_ESCAPE_HTML |
			XTEXT_VALUE_WRITE_ESCAPE_NON_ASCII
		),
		__xrtTextValueWriterEscapeEmit,
		pWriter,
		NULL,
		&iErrorOffset
	);
	if ( Result == XJSON_ESCAPE_OK ) {
		return true;
	}
	if ( Result == XJSON_ESCAPE_INVALID ) {
		(void)iErrorOffset;
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_VALUE,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"string is not valid UTF-8"
		);
	}
	if ( Result == XJSON_ESCAPE_OVERFLOW ) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_RANGE,
			XTEXT_VALUE_WRITE_ERROR_LIMIT,
			"escaped string size overflow"
		);
	}
	return false;
}



/* 写出 int64 token。 */
static bool __xrtTextValueWriterIntToken(
	xtextvaluewriter* pWriter,
	int64 iValue
)
{
	char Output[32];
	size_t iSize;

	if ( !xrtIntWrite(iValue, 10u, Output, sizeof(Output), &iSize, 0) ) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_INTERNAL,
			XTEXT_VALUE_WRITE_ERROR_OUTPUT,
			"failed to format integer"
		);
	}
	return __xrtTextValueWriterEmit(pWriter, Output, iSize);
}



/* 写出 uint64 token。 */
static bool __xrtTextValueWriterUIntToken(
	xtextvaluewriter* pWriter,
	uint64 iValue
)
{
	char Output[32];
	size_t iSize;

	if ( !xrtUIntWrite(iValue, 10u, Output, sizeof(Output), &iSize, 0) ) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_INTERNAL,
			XTEXT_VALUE_WRITE_ERROR_OUTPUT,
			"failed to format unsigned integer"
		);
	}
	return __xrtTextValueWriterEmit(pWriter, Output, iSize);
}



/* 写出有限 double token。 */
static bool __xrtTextValueWriterFloatToken(
	xtextvaluewriter* pWriter,
	double fValue
)
{
	char Output[64];
	size_t iSize;

	if ( !isfinite(fValue) ) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_VALUE,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"non-finite floating-point value requires a format policy"
		);
	}
	if ( !xrtNumWrite(fValue, Output, sizeof(Output), &iSize, 0) ) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_INTERNAL,
			XTEXT_VALUE_WRITE_ERROR_OUTPUT,
			"failed to format floating-point value"
		);
	}
	return __xrtTextValueWriterEmit(pWriter, Output, iSize);
}



/* 返回容器固定前缀。 */
static cstr __xrtTextValueWriterOpen(
	xtextvaluecontainertype Type,
	size_t* pSize
)
{
	if ( Type == XTEXT_VALUE_CONTAINER_ARRAY ) {
		*pSize = 1u;
		return "[";
	}
	if ( Type == XTEXT_VALUE_CONTAINER_OBJECT ) {
		*pSize = 1u;
		return "{";
	}
	if ( Type == XTEXT_VALUE_CONTAINER_INT_MAP ) {
		*pSize = 7u;
		return "intmap{";
	}
	if ( Type == XTEXT_VALUE_CONTAINER_SET ) {
		*pSize = 4u;
		return "set[";
	}
	*pSize = 0;
	return NULL;
}



/* 开始指定容器并压入固定大小帧。 */
bool __xrtTextValueWriterBegin(
	xtextvaluewriter* pWriter,
	xtextvaluecontainertype Type
)
{
	xtextvaluewriterframe Frame;
	cstr sOpen;
	size_t iOpenSize;
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	sOpen = __xrtTextValueWriterOpen(Type, &iOpenSize);
	if ( sOpen == NULL ) {
		bResult = __xrtTextValueWriterFail(
			pWriter,
			XERR_ARGUMENT,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"invalid writer container type"
		);
		return __xrtTextValueWriterLeave(pWriter, bResult);
	}
	if ( __xrtTextValueWriterDepth(pWriter) >= pWriter->Config.MaxDepth ) {
		bResult = __xrtTextValueWriterFail(
			pWriter,
			XERR_RANGE,
			XTEXT_VALUE_WRITE_ERROR_LIMIT,
			"output nesting exceeds configured depth"
		);
		return __xrtTextValueWriterLeave(pWriter, bResult);
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.Type = (uint8)Type;
	Frame.NeedKey =
		(Type == XTEXT_VALUE_CONTAINER_OBJECT) ||
		(Type == XTEXT_VALUE_CONTAINER_INT_MAP);
	bResult =
		__xrtTextValueWriterBeforeValue(pWriter) &&
		__xrtTextValueWriterEmit(pWriter, sOpen, iOpenSize) &&
		xrtBufferAppend(
			&pWriter->Frames,
			(xbytesview){ (cbytes)&Frame, sizeof(Frame) }
		);
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 结束当前容器并弹出帧。 */
bool __xrtTextValueWriterEnd(xtextvaluewriter* pWriter)
{
	xtextvaluewriterframe* pFrame;
	char cClose;
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	pFrame = __xrtTextValueWriterTop(pWriter);
	if ( pFrame == NULL ) {
		bResult = __xrtTextValueWriterFail(
			pWriter,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"no container is open"
		);
		return __xrtTextValueWriterLeave(pWriter, bResult);
	}
	if (
		((pFrame->Type == XTEXT_VALUE_CONTAINER_OBJECT) ||
		 (pFrame->Type == XTEXT_VALUE_CONTAINER_INT_MAP)) &&
		!pFrame->NeedKey
	) {
		bResult = __xrtTextValueWriterFail(
			pWriter,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"mapping key has no value"
		);
		return __xrtTextValueWriterLeave(pWriter, bResult);
	}
	cClose =
		(pFrame->Type == XTEXT_VALUE_CONTAINER_OBJECT) ||
		(pFrame->Type == XTEXT_VALUE_CONTAINER_INT_MAP)
		? '}'
		: ']';
	bResult = true;
	if ( pFrame->Count != 0 ) {
		bResult = __xrtTextValueWriterIndent(
			pWriter,
			__xrtTextValueWriterDepth(pWriter) - 1u
		);
	}
	if ( bResult ) {
		bResult = __xrtTextValueWriterEmit(pWriter, &cClose, 1u);
	}
	if ( bResult ) {
		bResult = xrtBufferResize(
			&pWriter->Frames,
			pWriter->Frames.Size - sizeof(xtextvaluewriterframe)
		);
	}
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 在当前映射中开始一个键。 */
static bool __xrtTextValueWriterBeforeKey(
	xtextvaluewriter* pWriter,
	xtextvaluecontainertype Type
)
{
	xtextvaluewriterframe* pFrame = __xrtTextValueWriterTop(pWriter);

	if (
		(pFrame == NULL) || (pFrame->Type != (uint8)Type) ||
		!pFrame->NeedKey
	) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"key is not valid at the current writer position"
		);
	}
	if ( pFrame->Count != 0 ) {
		if ( !__xrtTextValueWriterAscii(pWriter, ",", 1u) ) {
			return false;
		}
	}
	if ( !__xrtTextValueWriterIndent(
		pWriter,
		__xrtTextValueWriterDepth(pWriter)
	) ) {
		return false;
	}
	pFrame->NeedKey = false;
	return true;
}



/* 完成映射键后的冒号和可选空格。 */
static bool __xrtTextValueWriterAfterKey(xtextvaluewriter* pWriter)
{
	return
		__xrtTextValueWriterAscii(pWriter, ":", 1u) &&
		(((pWriter->Config.Flags & XTEXT_VALUE_WRITE_PRETTY) == 0) ||
		 __xrtTextValueWriterAscii(pWriter, " ", 1u));
}



/* 写入对象名称。 */
bool __xrtTextValueWriterName(
	xtextvaluewriter* pWriter,
	xstrview Name
)
{
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	bResult =
		__xrtTextValueWriterBeforeKey(
			pWriter,
			XTEXT_VALUE_CONTAINER_OBJECT
		) &&
		__xrtTextValueWriterStringToken(pWriter, Name) &&
		__xrtTextValueWriterAfterKey(pWriter);
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 写入整数映射键。 */
bool __xrtTextValueWriterKey(
	xtextvaluewriter* pWriter,
	int64 iKey
)
{
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	bResult =
		__xrtTextValueWriterBeforeKey(
			pWriter,
			XTEXT_VALUE_CONTAINER_INT_MAP
		) &&
		__xrtTextValueWriterIntToken(pWriter, iKey) &&
		__xrtTextValueWriterAfterKey(pWriter);
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 写入 null。 */
bool __xrtTextValueWriterNull(xtextvaluewriter* pWriter)
{
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	bResult =
		__xrtTextValueWriterBeforeValue(pWriter) &&
		__xrtTextValueWriterAscii(pWriter, "null", 4u);
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 写入布尔值。 */
bool __xrtTextValueWriterBool(
	xtextvaluewriter* pWriter,
	bool bValue
)
{
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	bResult =
		__xrtTextValueWriterBeforeValue(pWriter) &&
		__xrtTextValueWriterAscii(
			pWriter,
			bValue ? "true" : "false",
			bValue ? 4u : 5u
		);
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 写入 int64。 */
bool __xrtTextValueWriterInt(
	xtextvaluewriter* pWriter,
	int64 iValue
)
{
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	bResult =
		__xrtTextValueWriterBeforeValue(pWriter) &&
		__xrtTextValueWriterIntToken(pWriter, iValue);
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 写入 uint64。 */
bool __xrtTextValueWriterUInt(
	xtextvaluewriter* pWriter,
	uint64 iValue
)
{
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	bResult =
		__xrtTextValueWriterBeforeValue(pWriter) &&
		__xrtTextValueWriterUIntToken(pWriter, iValue);
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 写入有限 double。 */
bool __xrtTextValueWriterFloat(
	xtextvaluewriter* pWriter,
	double fValue
)
{
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	bResult =
		__xrtTextValueWriterBeforeValue(pWriter) &&
		__xrtTextValueWriterFloatToken(pWriter, fValue);
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 写入严格 UTF-8 字符串。 */
bool __xrtTextValueWriterString(
	xtextvaluewriter* pWriter,
	xstrview Text
)
{
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	bResult =
		__xrtTextValueWriterBeforeValue(pWriter) &&
		__xrtTextValueWriterStringToken(pWriter, Text);
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



/* 判断标签名称字节是否合法。 */
static bool __xrtTextValueWriterTagByte(uint8 iByte, bool bFirst)
{
	bool bStart =
		((iByte >= (uint8)'a') && (iByte <= (uint8)'z')) ||
		((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
		(iByte == (uint8)'_');

	return
		bStart ||
		(!bFirst && (
			((iByte >= (uint8)'0') && (iByte <= (uint8)'9')) ||
			(iByte == (uint8)'.') ||
			(iByte == (uint8)'-')
		));
}



/* 写入单字符串载荷标签。 */
bool __xrtTextValueWriterTag(
	xtextvaluewriter* pWriter,
	xstrview Tag,
	xstrview Payload
)
{
	bool bResult = true;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	if (
		(Tag.Data == NULL) || (Tag.Size == 0)
	) {
		bResult = __xrtTextValueWriterFail(
			pWriter,
			XERR_ARGUMENT,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"tag name is invalid"
		);
	}
	for ( size_t i = 0; bResult && (i < Tag.Size); i++ ) {
		if ( !__xrtTextValueWriterTagByte((uint8)Tag.Data[i], i == 0) ) {
			bResult = __xrtTextValueWriterFail(
				pWriter,
				XERR_ARGUMENT,
				XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
				"tag name contains an invalid byte"
			);
		}
	}
	if ( bResult ) {
		bResult =
			__xrtTextValueWriterBeforeValue(pWriter) &&
			__xrtTextValueWriterEmit(pWriter, Tag.Data, Tag.Size) &&
			__xrtTextValueWriterAscii(pWriter, "(", 1u) &&
			__xrtTextValueWriterStringToken(pWriter, Payload) &&
			__xrtTextValueWriterAscii(pWriter, ")", 1u);
	}
	return __xrtTextValueWriterLeave(pWriter, bResult);
}



#if defined(XRT_FEATURE_XSON_WRITE)

/* 以 3 KiB 输入块增量写出规范 Base64，避免为二进制建立等大临时文本。 */
bool __xrtTextValueWriterBase64Tag(
	xtextvaluewriter* pWriter,
	xstrview Tag,
	xbytesview Data
)
{
	char Output[4097];
	size_t iOffset = 0;
	bool bResult;

	if ( !__xrtTextValueWriterEnter(pWriter) ) {
		return false;
	}
	if (
		(Tag.Data == NULL) || (Tag.Size == 0) ||
		((Data.Data == NULL) && (Data.Size != 0))
	) {
		bResult = __xrtTextValueWriterFail(
			pWriter,
			XERR_ARGUMENT,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"Base64 tag input is invalid"
		);
		return __xrtTextValueWriterLeave(pWriter, bResult);
	}
	for ( size_t i = 0; i < Tag.Size; i++ ) {
		if ( !__xrtTextValueWriterTagByte((uint8)Tag.Data[i], i == 0) ) {
			bResult = __xrtTextValueWriterFail(
				pWriter,
				XERR_ARGUMENT,
				XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
				"Base64 tag name contains an invalid byte"
			);
			return __xrtTextValueWriterLeave(pWriter, bResult);
		}
	}
	bResult =
		__xrtTextValueWriterBeforeValue(pWriter) &&
		__xrtTextValueWriterEmit(pWriter, Tag.Data, Tag.Size) &&
		__xrtTextValueWriterAscii(pWriter, "(\"", 2u);
	while ( bResult && (iOffset < Data.Size) ) {
		size_t iChunk = Data.Size - iOffset;
		size_t iOutputSize;

		if ( iChunk > 3072u ) {
			iChunk = 3072u;
		}
		bResult = xrtBase64Encode(
			Data.Data + iOffset,
			iChunk,
			Output,
			sizeof(Output),
			&iOutputSize,
			NULL
		);
		if ( bResult ) {
			bResult = __xrtTextValueWriterEmit(
				pWriter,
				Output,
				iOutputSize
			);
		}
		iOffset += iChunk;
	}
	if ( bResult ) {
		bResult = __xrtTextValueWriterAscii(pWriter, "\")", 2u);
	}
	return __xrtTextValueWriterLeave(pWriter, bResult);
}

#endif



/* 进入格式适配器的用户回调保护区。 */
bool __xrtTextValueWriterCallbackEnter(xtextvaluewriter* pWriter)
{
	return __xrtTextValueWriterEnter(pWriter);
}



/* 离开用户回调保护区，并固化重入造成的失败。 */
bool __xrtTextValueWriterCallbackLeave(xtextvaluewriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterLeave(pWriter, true);
}



/* 创建内存或同步 sink 写入器。 */
xtextvaluewriter* __xrtTextValueWriterCreate(
	const xtextvaluewriteconfig* pConfig,
	xtextvaluewriteproc pWrite,
	ptr pWriteData,
	bool bMemory,
	xtextvaluewriteerrorproc pError,
	ptr pErrorData
)
{
	xtextvaluewriter* pWriter;
	uint32 iKnownFlags =
		XTEXT_VALUE_WRITE_PRETTY |
		XTEXT_VALUE_WRITE_ESCAPE_SLASH |
		XTEXT_VALUE_WRITE_ESCAPE_HTML |
		XTEXT_VALUE_WRITE_ESCAPE_NON_ASCII;

	if (
		(pConfig == NULL) || (pError == NULL) ||
		(!bMemory && (pWrite == NULL)) ||
		((pConfig->Flags & ~iKnownFlags) != 0) ||
		(pConfig->MaxDepth == 0) ||
		(pConfig->MaxDepth > XRT_VALUE_DEPTH_MAX) ||
		(pConfig->Indent > 16u) ||
		(pConfig->MaxOutputBytes == 0)
	) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pWriter = (xtextvaluewriter*)xrtCalloc(1, sizeof(xtextvaluewriter));
	if ( pWriter == NULL ) {
		return NULL;
	}
	pWriter->Config = *pConfig;
	pWriter->Write = pWrite;
	pWriter->WriteData = pWriteData;
	pWriter->Error = pError;
	pWriter->ErrorData = pErrorData;
	pWriter->Memory = bMemory;
	if (
		!xrtBufferInit(&pWriter->Output) ||
		!xrtBufferInit(&pWriter->Frames)
	) {
		xrtBufferUnit(&pWriter->Frames);
		xrtBufferUnit(&pWriter->Output);
		xrtFree(pWriter);
		return NULL;
	}
	return pWriter;
}



/* 验证根值和全部容器已经完整结束。 */
bool __xrtTextValueWriterFinish(xtextvaluewriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pWriter->Busy ) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"writer cannot finish during a callback"
		);
	}
	if ( pWriter->Finished ) {
		return !pWriter->Failed;
	}
	if (
		pWriter->Failed || !pWriter->RootWritten ||
		(pWriter->Frames.Size != 0)
	) {
		return __xrtTextValueWriterFail(
			pWriter,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"writer root or containers are incomplete"
		);
	}
	pWriter->Finished = true;
	return true;
}



/* 从已完成的内存 writer 移交零结尾文本。 */
str __xrtTextValueWriterTake(
	xtextvaluewriter* pWriter,
	size_t* pSize
)
{
	bytes pText;
	size_t iSize;

	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if (
		!pWriter->Memory || !pWriter->Finished || pWriter->Failed ||
		pWriter->Taken || pWriter->Busy
	) {
		(void)__xrtTextValueWriterFail(
			pWriter,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"writer result is not available"
		);
		return NULL;
	}
	iSize = pWriter->Output.Size;
	if ( xrtBufferAdd(&pWriter->Output, 1u) == NULL ) {
		pWriter->Failed = true;
		return NULL;
	}
	pWriter->Output.Data[iSize] = 0;
	pText = xrtBufferTake(&pWriter->Output, NULL, NULL);
	if ( pText == NULL ) {
		pWriter->Failed = true;
		return NULL;
	}
	pWriter->Taken = true;
	if ( pSize != NULL ) {
		*pSize = iSize;
	}
	return (str)pText;
}



/* 销毁未处于回调中的 writer。 */
bool __xrtTextValueWriterFree(xtextvaluewriter* pWriter)
{
	if ( pWriter == NULL ) {
		return true;
	}
	if ( pWriter->Busy ) {
		(void)__xrtTextValueWriterFail(
			pWriter,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"writer cannot be freed during a callback"
		);
		return false;
	}
	xrtBufferUnit(&pWriter->Frames);
	xrtBufferUnit(&pWriter->Output);
	xrtFree(pWriter);
	return true;
}

#endif
