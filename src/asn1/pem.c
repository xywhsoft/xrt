#include "../internal/xrt_pem.h"

#include <stdio.h>



#if defined(XRT_FEATURE_PEM)

static const char __xrtPemBegin[] = "-----BEGIN ";
static const char __xrtPemEnd[] = "-----END ";
static const char __xrtPemClose[] = "-----";



/* 设置带文本偏移和可选原因链的 PEM 结构化错误。 */
void __xrtPemError(
	xerrkind Kind,
	xpemerror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset,
	const xerror* pCause
)
{
	char Data[64];
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.pem";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	if ( iOffset != SIZE_MAX ) {
		(void)snprintf(
			Data, sizeof(Data), "offset=%llu", (unsigned long long)iOffset
		);
		Desc.Data = Data;
	}
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 判断偏移是否位于文本起点或任一种换行之后。 */
static bool __xrtPemLineStart(cstr sText, size_t iOffset)
{
	return (iOffset == 0) || (sText[iOffset - 1u] == '\n') ||
		(sText[iOffset - 1u] == '\r');
}



/* 在严格长度范围内寻找下一条行首边界。 */
static size_t __xrtPemFindBoundary(
	cstr sText,
	size_t iSize,
	size_t iOffset,
	cstr sPrefix,
	size_t iPrefixSize
)
{
	if ( iPrefixSize > iSize ) {
		return XRT_NPOS;
	}
	for ( size_t i = iOffset; i <= iSize - iPrefixSize; i++ ) {
		if ( __xrtPemLineStart(sText, i) &&
			(memcmp(sText + i, sPrefix, iPrefixSize) == 0) ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 验证 RFC 7468 标签，不允许首尾或连续的空格与连字符。 */
static bool __xrtPemLabelValid(cstr sLabel, size_t iSize)
{
	if ( (sLabel == NULL) || (iSize == 0) ||
		(sLabel[0] == ' ') || (sLabel[0] == '-') ||
		(sLabel[iSize - 1u] == ' ') || (sLabel[iSize - 1u] == '-') ) {
		return false;
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		uint8 iCharacter = (uint8)sLabel[i];

		if ( (iCharacter < 0x20u) || (iCharacter > 0x7Eu) ) {
			return false;
		}
		if ( (i != 0) &&
			((sLabel[i] == ' ') || (sLabel[i] == '-')) &&
			((sLabel[i - 1u] == ' ') || (sLabel[i - 1u] == '-')) ) {
			return false;
		}
	}
	return true;
}



/* 解析边界标签、尾部空白和换行，并返回下一行偏移。 */
static bool __xrtPemParseBoundary(
	cstr sText,
	size_t iSize,
	size_t iOffset,
	size_t iPrefixSize,
	xstrview* pLabel,
	size_t* pNextLine,
	bool bRequireNewline,
	cstr sOperation
)
{
	size_t iLabelStart = iOffset + iPrefixSize;
	size_t iClose = XRT_NPOS;
	size_t iPosition;

	if ( (iLabelStart > iSize) ||
		((sizeof(__xrtPemClose) - 1u) > iSize - iLabelStart) ) {
		__xrtPemError(
			XERR_PROTOCOL, XPEM_ERROR_LABEL, sOperation,
			"PEM boundary has an invalid or missing label", iOffset, NULL
		);
		return false;
	}
	for ( size_t i = iLabelStart;
		i <= iSize - (sizeof(__xrtPemClose) - 1u); i++ ) {
		if ( memcmp(
			sText + i, __xrtPemClose, sizeof(__xrtPemClose) - 1u
		) == 0 ) {
			iClose = i;
			break;
		}
		if ( (sText[i] == '\r') || (sText[i] == '\n') ) {
			break;
		}
	}
	if ( (iClose == XRT_NPOS) ||
		!__xrtPemLabelValid(sText + iLabelStart, iClose - iLabelStart) ) {
		__xrtPemError(
			XERR_PROTOCOL, XPEM_ERROR_LABEL, sOperation,
			"PEM boundary has an invalid or missing label", iOffset, NULL
		);
		return false;
	}
	iPosition = iClose + sizeof(__xrtPemClose) - 1u;
	while ( (iPosition < iSize) &&
		((sText[iPosition] == ' ') || (sText[iPosition] == '\t')) ) {
		iPosition++;
	}
	if ( iPosition == iSize ) {
		if ( bRequireNewline ) {
			__xrtPemError(
				XERR_PROTOCOL, XPEM_ERROR_BOUNDARY, sOperation,
				"PEM begin boundary is missing its line ending", iPosition, NULL
			);
			return false;
		}
		*pNextLine = iPosition;
	} else if ( sText[iPosition] == '\r' ) {
		iPosition++;
		if ( (iPosition < iSize) && (sText[iPosition] == '\n') ) {
			iPosition++;
		}
		*pNextLine = iPosition;
	} else if ( sText[iPosition] == '\n' ) {
		*pNextLine = iPosition + 1u;
	} else {
		__xrtPemError(
			XERR_PROTOCOL, XPEM_ERROR_BOUNDARY, sOperation,
			"PEM boundary contains text after its closing hyphens",
			iPosition, NULL
		);
		return false;
	}
	pLabel->Data = sText + iLabelStart;
	pLabel->Size = iClose - iLabelStart;
	return true;
}



/* 初始化严格有界的 PEM 游标。 */
XRT_API bool xrtPemInit(xpemcursor* pCursor, cstr sText, size_t iSize)
{
	xpemcursor Cursor;

	if ( (pCursor == NULL) || ((sText == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Cursor.Text.Data = sText;
	Cursor.Text.Size = iSize;
	Cursor.Offset = 0;
	*pCursor = Cursor;
	return true;
}



/* 解析下一个完整 PEM 块，并严格匹配结束标签。 */
XRT_API xpemresult xrtPemRead(xpemcursor* pCursor, xpemblock* pBlock)
{
	cstr sText;
	size_t iSize;
	size_t iBegin;
	size_t iBody;
	size_t iEnd;
	size_t iNext;
	xstrview BeginLabel;
	xstrview EndLabel;
	xpemblock Block;

	if ( (pCursor == NULL) || (pBlock == NULL) ||
		((pCursor->Text.Data == NULL) && (pCursor->Text.Size != 0)) ||
		(pCursor->Offset > pCursor->Text.Size) ) {
		__xrtErrorSetInvalidArgument();
		return XPEM_ERROR;
	}
	sText = pCursor->Text.Data;
	iSize = pCursor->Text.Size;
	iBegin = __xrtPemFindBoundary(
		sText, iSize, pCursor->Offset,
		__xrtPemBegin, sizeof(__xrtPemBegin) - 1u
	);
	if ( iBegin == XRT_NPOS ) {
		return XPEM_DONE;
	}
	if ( !__xrtPemParseBoundary(
		sText, iSize, iBegin, sizeof(__xrtPemBegin) - 1u,
		&BeginLabel, &iBody, true, "pem-read"
	) ) {
		return XPEM_ERROR;
	}

	iEnd = iBody;
	while ( true ) {
		size_t iNested = __xrtPemFindBoundary(
			sText, iSize, iEnd,
			__xrtPemBegin, sizeof(__xrtPemBegin) - 1u
		);
		size_t iCandidate = __xrtPemFindBoundary(
			sText, iSize, iEnd,
			__xrtPemEnd, sizeof(__xrtPemEnd) - 1u
		);

		if ( (iNested != XRT_NPOS) &&
			((iCandidate == XRT_NPOS) || (iNested < iCandidate)) ) {
			__xrtPemError(
				XERR_PROTOCOL, XPEM_ERROR_BOUNDARY, "pem-read",
				"nested PEM begin boundary appears before the matching end",
				iNested, NULL
			);
			return XPEM_ERROR;
		}
		if ( iCandidate == XRT_NPOS ) {
			__xrtPemError(
				XERR_PROTOCOL, XPEM_ERROR_BOUNDARY, "pem-read",
				"PEM block is missing its end boundary", iBegin, NULL
			);
			return XPEM_ERROR;
		}
		iEnd = iCandidate;
		break;
	}
	if ( !__xrtPemParseBoundary(
		sText, iSize, iEnd, sizeof(__xrtPemEnd) - 1u,
		&EndLabel, &iNext, false, "pem-read"
	) ) {
		return XPEM_ERROR;
	}
	if ( (BeginLabel.Size != EndLabel.Size) ||
		(memcmp(BeginLabel.Data, EndLabel.Data, BeginLabel.Size) != 0) ) {
		__xrtPemError(
			XERR_PROTOCOL, XPEM_ERROR_LABEL, "pem-read",
			"PEM begin and end labels do not match", iEnd, NULL
		);
		return XPEM_ERROR;
	}
	Block.Label = BeginLabel;
	Block.Body.Data = sText + iBody;
	Block.Body.Size = iEnd - iBody;
	Block.Raw.Data = sText + iBegin;
	Block.Raw.Size = iNext - iBegin;
	pCursor->Offset = iNext;
	*pBlock = Block;
	return XPEM_BLOCK;
}



/* 查找第一个标签完全匹配的 PEM 块。 */
XRT_API bool xrtPemFind(
	cstr sText,
	size_t iSize,
	cstr sLabel,
	xpemblock* pBlock
)
{
	xpemcursor Cursor;
	xpemblock Block;
	size_t iLabelSize;

	if ( (sLabel == NULL) || (pBlock == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iLabelSize = strlen(sLabel);
	if ( !__xrtPemLabelValid(sLabel, iLabelSize) ) {
		__xrtPemError(
			XERR_VALUE, XPEM_ERROR_LABEL, "pem-find",
			"requested PEM label is invalid", SIZE_MAX, NULL
		);
		return false;
	}
	if ( !xrtPemInit(&Cursor, sText, iSize) ) {
		return false;
	}
	while ( true ) {
		xpemresult Result = xrtPemRead(&Cursor, &Block);

		if ( Result == XPEM_ERROR ) {
			return false;
		}
		if ( Result == XPEM_DONE ) {
			__xrtPemError(
				XERR_NOT_FOUND, XPEM_ERROR_NOT_FOUND, "pem-find",
				"requested PEM block was not found", SIZE_MAX, NULL
			);
			return false;
		}
		if ( (Block.Label.Size == iLabelSize) &&
			(memcmp(Block.Label.Data, sLabel, iLabelSize) == 0) ) {
			*pBlock = Block;
			return true;
		}
	}
}



/* 判断当前错误是否是可包装的 Base64 正文格式错误。 */
static bool __xrtPemBase64FormatError(const xerror* pError)
{
	cstr sDomain = pError != NULL ? xrtErrorDomain(pError) : NULL;

	return (sDomain != NULL) &&
		(strcmp(sDomain, "xrt.codec") == 0) &&
		(xrtErrorCode(pError) == XCODEC_ERROR_BASE64_FORMAT);
}



/* 解码 PEM 正文，并把 Base64 格式失败包装为 PEM 原因链。 */
XRT_API bool xrtPemDecode(
	const xpemblock* pBlock,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xbase64config Config = { NULL, (uint32)XBASE64_IGNORE_SPACE };
	const xerror* pCause;

	if ( (pBlock == NULL) || (pOutputSize == NULL) ||
		((pBlock->Body.Data == NULL) && (pBlock->Body.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtBase64Decode(
		pBlock->Body.Data, pBlock->Body.Size,
		pOutput, iCapacity, pOutputSize, &Config
	) ) {
		return true;
	}
	pCause = xrtGetError();
	if ( __xrtPemBase64FormatError(pCause) ) {
		__xrtPemError(
			XERR_PROTOCOL, XPEM_ERROR_BODY, "pem-decode",
			"PEM body is not canonical Base64", SIZE_MAX, pCause
		);
	}
	return false;
}



/* 分配并解码 PEM 正文。 */
XRT_API bytes xrtPemDecodeNew(
	const xpemblock* pBlock,
	size_t* pOutputSize
)
{
	xbase64config Config = { NULL, (uint32)XBASE64_IGNORE_SPACE };
	const xerror* pCause;
	bytes pOutput;

	if ( (pBlock == NULL) || (pOutputSize == NULL) ||
		((pBlock->Body.Data == NULL) && (pBlock->Body.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pOutput = xrtBase64DecodeNew(
		pBlock->Body.Data, pBlock->Body.Size, pOutputSize, &Config
	);
	if ( pOutput != NULL ) {
		return pOutput;
	}
	pCause = xrtGetError();
	if ( __xrtPemBase64FormatError(pCause) ) {
		__xrtPemError(
			XERR_PROTOCOL, XPEM_ERROR_BODY, "pem-decode",
			"PEM body is not canonical Base64", SIZE_MAX, pCause
		);
	}
	return NULL;
}



/* 计算无溢出的规范 PEM 文本长度。 */
static bool __xrtPemEncodedSize(
	size_t iLabelSize,
	size_t iBase64Size,
	size_t* pSize
)
{
	size_t iLines = iBase64Size == 0 ? 0 :
		(iBase64Size / 64u) + ((iBase64Size % 64u) != 0 ? 1u : 0u);
	size_t iFixed = (sizeof(__xrtPemBegin) - 1u) +
		(sizeof(__xrtPemEnd) - 1u) +
		((sizeof(__xrtPemClose) - 1u) * 2u) + 2u;

	if ( (iLabelSize > (SIZE_MAX - iFixed) / 2u) ||
		(iBase64Size > SIZE_MAX - iFixed - (iLabelSize * 2u)) ||
		(iLines > SIZE_MAX - iFixed - (iLabelSize * 2u) - iBase64Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize = iFixed + (iLabelSize * 2u) + iBase64Size + iLines;
	if ( *pSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return true;
}



/* 插入 64 字符换行，并从后向前扩张以避免额外临时分配。 */
static size_t __xrtPemWrapBase64(
	char* sBody,
	size_t iBase64Size
)
{
	size_t iSource = iBase64Size;
	size_t iLines = iBase64Size == 0 ? 0 :
		(iBase64Size / 64u) + ((iBase64Size % 64u) != 0 ? 1u : 0u);
	size_t iTarget = iBase64Size + iLines;

	while ( iSource != 0 ) {
		size_t iChunk = iSource % 64u;

		if ( iChunk == 0 ) {
			iChunk = 64u;
		}
		iTarget--;
		sBody[iTarget] = '\n';
		iTarget -= iChunk;
		iSource -= iChunk;
		memmove(sBody + iTarget, sBody + iSource, iChunk);
	}
	return iBase64Size + iLines;
}



/* 生成规范 RFC 7468 文本。 */
XRT_API bool xrtPemEncode(
	cstr sLabel,
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iLabelSize;
	size_t iBase64Size;
	size_t iRequired;
	size_t iPosition = 0;
	size_t iBody;
	size_t iWrapped;

	if ( (sLabel == NULL) || ((pData == NULL) && (iSize != 0)) ||
		(pOutputSize == NULL) || ((sOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iLabelSize = strlen(sLabel);
	if ( !__xrtPemLabelValid(sLabel, iLabelSize) ) {
		__xrtPemError(
			XERR_VALUE, XPEM_ERROR_LABEL, "pem-encode",
			"PEM label is invalid", SIZE_MAX, NULL
		);
		return false;
	}
	if ( !xrtBase64Encode(
		pData, iSize, NULL, 0, &iBase64Size, NULL
	) || !__xrtPemEncodedSize(iLabelSize, iBase64Size, &iRequired) ) {
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(
		sOutput, iRequired + 1u, pData, iSize
	) || __xrtRangesOverlap(
		sOutput, iRequired + 1u, sLabel, iLabelSize + 1u
	) || __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), sOutput, iRequired + 1u
	) || __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), pData, iSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	iBody = (sizeof(__xrtPemBegin) - 1u) + iLabelSize +
		(sizeof(__xrtPemClose) - 1u) + 1u;
	if ( !xrtBase64Encode(
		pData, iSize, sOutput + iBody, iCapacity - iBody,
		&iBase64Size, NULL
	) ) {
		return false;
	}

	memcpy(sOutput + iPosition, __xrtPemBegin, sizeof(__xrtPemBegin) - 1u);
	iPosition += sizeof(__xrtPemBegin) - 1u;
	memcpy(sOutput + iPosition, sLabel, iLabelSize);
	iPosition += iLabelSize;
	memcpy(sOutput + iPosition, __xrtPemClose, sizeof(__xrtPemClose) - 1u);
	iPosition += sizeof(__xrtPemClose) - 1u;
	sOutput[iPosition++] = '\n';
	iWrapped = __xrtPemWrapBase64(sOutput + iBody, iBase64Size);
	iPosition = iBody + iWrapped;
	memcpy(sOutput + iPosition, __xrtPemEnd, sizeof(__xrtPemEnd) - 1u);
	iPosition += sizeof(__xrtPemEnd) - 1u;
	memcpy(sOutput + iPosition, sLabel, iLabelSize);
	iPosition += iLabelSize;
	memcpy(sOutput + iPosition, __xrtPemClose, sizeof(__xrtPemClose) - 1u);
	iPosition += sizeof(__xrtPemClose) - 1u;
	sOutput[iPosition++] = '\n';
	sOutput[iPosition] = '\0';
	*pOutputSize = iPosition;
	return true;
}



/* 生成独立的规范 PEM 文本。 */
XRT_API str xrtPemEncodeNew(cstr sLabel, const void* pData, size_t iSize)
{
	str sOutput;
	size_t iOutputSize;

	if ( !xrtPemEncode(
		sLabel, pData, iSize, NULL, 0, &iOutputSize
	) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iOutputSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtPemEncode(
		sLabel, pData, iSize, sOutput, iOutputSize + 1u, &iOutputSize
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	return sOutput;
}

#endif
