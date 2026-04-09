#ifndef XRT_XCODEC_H
#define XRT_XCODEC_H

/*
	XRT mainline codec adapter and framing helpers.

	This header defines:
	  - the backend-neutral parser contract used by xnet-based protocols
	  - common frame metadata over xnetchain
	  - reusable line and length-field framing helpers
	  - shared status codes consumed by HTTP and WebSocket codecs
*/


/* ============================== Common codec status and frame types ============================== */

#if !defined(XRT_BUILD_CORE)

typedef enum {
	XCODEC_STATUS_ERROR = -1,
	XCODEC_STATUS_NEED_MORE = 0,
	XCODEC_STATUS_FRAME = 1
} xcodecstatus;

#define XCODEC_KIND_NONE    0u
#define XCODEC_KIND_LINE    1u
#define XCODEC_KIND_LENGTH  2u
#define XCODEC_KIND_HTTP1   3u
#define XCODEC_KIND_WS      4u

#define XCODEC_FRAME_F_NONE        0x00000000u
#define XCODEC_FRAME_F_TEXT        0x00000001u
#define XCODEC_FRAME_F_BINARY      0x00000002u
#define XCODEC_FRAME_F_REQUEST     0x00000004u
#define XCODEC_FRAME_F_RESPONSE    0x00000008u
#define XCODEC_FRAME_F_FIN         0x00000010u
#define XCODEC_FRAME_F_MASKED      0x00000020u
#define XCODEC_FRAME_F_UPGRADE     0x00000040u
#define XCODEC_FRAME_F_KEEPALIVE   0x00000080u
#define XCODEC_FRAME_F_CHUNKED     0x00000100u
#define XCODEC_FRAME_F_CONTROL     0x00000200u

typedef struct {
	uint32 iKind;
	uint32 iFlags;
	size_t iHeaderBytes;
	size_t iPayloadOffset;
	size_t iPayloadBytes;
	size_t iFrameBytes;
	uint64 iMeta0;
	uint64 iMeta1;
} xcodecframe;



/* ============================== Generic parser adapter ============================== */

typedef xcodecstatus (*xcodec_parse_fn)(ptr pCtx, const xnetchain* pInput, xcodecframe* pFrame);
typedef void (*xcodec_reset_fn)(ptr pCtx);

typedef struct {
	xcodec_parse_fn Parse;
	xcodec_reset_fn Reset;
} xcodecparserops;

typedef struct {
	const xcodecparserops* pOps;
	ptr pCtx;
} xcodecparser;

#endif /* !XRT_BUILD_CORE */

XXAPI void xrtCodecParserInit(xcodecparser* pParser, const xcodecparserops* pOps, ptr pCtx)
{
	if ( !pParser ) return;
	pParser->pOps = pOps;
	pParser->pCtx = pCtx;
}


// xrtCodecParserParse 相关处理
XXAPI xcodecstatus xrtCodecParserParse(const xcodecparser* pParser, const xnetchain* pInput, xcodecframe* pFrame)
{
	if ( !pParser || !pParser->pOps || !pParser->pOps->Parse ) return XCODEC_STATUS_ERROR;
	return pParser->pOps->Parse(pParser->pCtx, pInput, pFrame);
}


// xrtCodecParserReset 相关处理
XXAPI void xrtCodecParserReset(const xcodecparser* pParser)
{
	if ( !pParser || !pParser->pOps || !pParser->pOps->Reset ) return;
	pParser->pOps->Reset(pParser->pCtx);
}



/* ============================== Internal chain slice helpers ============================== */

static size_t __xcodecChainPeekAt(const xnetchain* pChain, size_t iOffset, ptr pOut, size_t iLen)
{
	size_t iCopied = 0;
	size_t iSeen = 0;
	uint8* pDst = (uint8*)pOut;
	const __xnet_blk* pBlk;

	if ( !pChain || !pOut || iLen == 0 || iOffset >= xrtNetChainBytes(pChain) ) return 0;

	for ( pBlk = pChain->pHead; pBlk && iCopied < iLen; pBlk = pBlk->pNext ) {
		size_t iReadable = __xnetBlkReadable(pBlk);
		size_t iStart;
		size_t iChunk;
		if ( iReadable == 0 ) continue;
		if ( iSeen + iReadable <= iOffset ) {
			iSeen += iReadable;
			continue;
		}
		iStart = (iOffset > iSeen) ? (iOffset - iSeen) : 0;
		iChunk = iReadable - iStart;
		if ( iChunk > (iLen - iCopied) ) iChunk = iLen - iCopied;
		memcpy(pDst + iCopied, __xnetBlkDataPtr(pBlk) + pBlk->iBegin + iStart, iChunk);
		iCopied += iChunk;
		iSeen += iReadable;
	}

	return iCopied;
}


// 内部函数：__xcodecChainMatchAt
static bool __xcodecChainMatchAt(const xnetchain* pChain, size_t iOffset, const uint8* pNeedle, size_t iNeedleLen)
{
	size_t iSeen = 0;
	const __xnet_blk* pBlk;
	size_t iNeedleOff = 0;

	if ( !pChain || !pNeedle || iNeedleLen == 0 ) return false;
	if ( iOffset + iNeedleLen > xrtNetChainBytes(pChain) ) return false;

	for ( pBlk = pChain->pHead; pBlk && iNeedleOff < iNeedleLen; pBlk = pBlk->pNext ) {
		size_t iReadable = __xnetBlkReadable(pBlk);
		size_t iStart;
		size_t iChunk;
		if ( iReadable == 0 ) continue;
		if ( iSeen + iReadable <= iOffset ) {
			iSeen += iReadable;
			continue;
		}
		iStart = (iOffset > iSeen) ? (iOffset - iSeen) : 0;
		iChunk = iReadable - iStart;
		if ( iChunk > (iNeedleLen - iNeedleOff) ) iChunk = iNeedleLen - iNeedleOff;
		if ( memcmp(__xnetBlkDataPtr(pBlk) + pBlk->iBegin + iStart, pNeedle + iNeedleOff, iChunk) != 0 ) {
			return false;
		}
		iNeedleOff += iChunk;
		iSeen += iReadable;
	}

	return iNeedleOff == iNeedleLen;
}


// 内部函数：__xcodecChainFindPattern
static size_t __xcodecChainFindPattern(const xnetchain* pChain, const uint8* pNeedle, size_t iNeedleLen, size_t iStartOff)
{
	size_t iPos;
	if ( !pChain || !pNeedle || iNeedleLen == 0 ) return (size_t)-1;
	if ( iNeedleLen == 1 ) return xrtNetChainFindByte(pChain, pNeedle[0], iStartOff);

	iPos = xrtNetChainFindByte(pChain, pNeedle[0], iStartOff);
	while ( iPos != (size_t)-1 ) {
		if ( __xcodecChainMatchAt(pChain, iPos, pNeedle, iNeedleLen) ) return iPos;
		iPos = xrtNetChainFindByte(pChain, pNeedle[0], iPos + 1u);
	}
	return (size_t)-1;
}


// xrtCodecFrameInit 相关处理
XXAPI void xrtCodecFrameInit(xcodecframe* pFrame)
{
	if ( !pFrame ) return;
	memset(pFrame, 0, sizeof(xcodecframe));
}


// xrtCodecFramePeek 相关处理
XXAPI size_t xrtCodecFramePeek(const xnetchain* pInput, const xcodecframe* pFrame, ptr pOut, size_t iLen)
{
	size_t iCopyLen;
	if ( !pInput || !pFrame || !pOut || iLen == 0 ) return 0;
	iCopyLen = pFrame->iPayloadBytes;
	if ( iCopyLen > iLen ) iCopyLen = iLen;
	return __xcodecChainPeekAt(pInput, pFrame->iPayloadOffset, pOut, iCopyLen);
}


// xrtCodecFrameConsume 相关处理
XXAPI void xrtCodecFrameConsume(xnetchain* pInput, const xcodecframe* pFrame)
{
	if ( !pInput || !pFrame || pFrame->iFrameBytes == 0 ) return;
	xrtNetChainConsume(pInput, pFrame->iFrameBytes);
}



/* ============================== Line codec ============================== */

#if !defined(XRT_BUILD_CORE)
typedef struct {
	uint8 aDelimiter[4];
	uint32 iDelimiterLen;
	uint32 iMaxLineBytes;
	bool bStripDelimiter;
} xcodeclinecodec;
#endif

// 初始化编解码器行配置
XXAPI void xrtCodecLineConfigInit(xcodeclinecodec* pCodec)
{
	if ( !pCodec ) return;
	memset(pCodec, 0, sizeof(xcodeclinecodec));
	pCodec->aDelimiter[0] = '\n';
	pCodec->iDelimiterLen = 1;
	pCodec->iMaxLineBytes = 8192;
	pCodec->bStripDelimiter = true;
}


// 设置编解码器行 delimiter
XXAPI bool xrtCodecLineSetDelimiter(xcodeclinecodec* pCodec, const void* pDelimiter, uint32 iDelimiterLen)
{
	if ( !pCodec || !pDelimiter || iDelimiterLen == 0 || iDelimiterLen > sizeof(pCodec->aDelimiter) ) return false;
	memset(pCodec->aDelimiter, 0, sizeof(pCodec->aDelimiter));
	memcpy(pCodec->aDelimiter, pDelimiter, iDelimiterLen);
	pCodec->iDelimiterLen = iDelimiterLen;
	return true;
}


// 解析编解码器行
XXAPI xcodecstatus xrtCodecLineParse(ptr pCtx, const xnetchain* pInput, xcodecframe* pFrame)
{
	xcodeclinecodec* pCodec = (xcodeclinecodec*)pCtx;
	size_t iPos;
	size_t iBytes;

	if ( !pCodec || !pInput || !pFrame || pCodec->iDelimiterLen == 0 ) return XCODEC_STATUS_ERROR;
	xrtCodecFrameInit(pFrame);
	iBytes = xrtNetChainBytes(pInput);
	if ( iBytes == 0 ) return XCODEC_STATUS_NEED_MORE;
	if ( pCodec->iMaxLineBytes > 0 && iBytes > pCodec->iMaxLineBytes && __xcodecChainFindPattern(pInput, pCodec->aDelimiter, pCodec->iDelimiterLen, 0) == (size_t)-1 ) {
		return XCODEC_STATUS_ERROR;
	}

	iPos = __xcodecChainFindPattern(pInput, pCodec->aDelimiter, pCodec->iDelimiterLen, 0);
	if ( iPos == (size_t)-1 ) return XCODEC_STATUS_NEED_MORE;

	pFrame->iKind = XCODEC_KIND_LINE;
	pFrame->iHeaderBytes = 0;
	pFrame->iPayloadOffset = 0;
	pFrame->iPayloadBytes = pCodec->bStripDelimiter ? iPos : (iPos + pCodec->iDelimiterLen);
	pFrame->iFrameBytes = iPos + pCodec->iDelimiterLen;
	pFrame->iFlags = XCODEC_FRAME_F_TEXT;
	return XCODEC_STATUS_FRAME;
}


// 重置编解码器行
XXAPI void xrtCodecLineReset(ptr pCtx)
{
	(void)pCtx;
}


// xrtCodecLineOps 相关处理
XXAPI const xcodecparserops* xrtCodecLineOps(void)
{
	static const xcodecparserops tOps = {
		xrtCodecLineParse,
		xrtCodecLineReset
	};
	return &tOps;
}



/* ============================== Length-field codec ============================== */

#if !defined(XRT_BUILD_CORE)
typedef struct {
	uint8 iFieldBytes;
	bool bBigEndian;
	int32_t iLengthAdjust;
	uint32 iMaxPayloadBytes;
} xcodeclengthcodec;
#endif

// 初始化编解码器 length 配置
XXAPI void xrtCodecLengthConfigInit(xcodeclengthcodec* pCodec)
{
	if ( !pCodec ) return;
	memset(pCodec, 0, sizeof(xcodeclengthcodec));
	pCodec->iFieldBytes = 4;
	pCodec->bBigEndian = true;
	pCodec->iLengthAdjust = 0;
	pCodec->iMaxPayloadBytes = 1024u * 1024u;
}


// 内部函数：__xcodecReadUint
static uint64 __xcodecReadUint(const uint8* pBytes, uint32 iByteCount, bool bBigEndian)
{
	uint64 iValue = 0;
	if ( !pBytes || iByteCount == 0 || iByteCount > 8 ) return 0;
	if ( bBigEndian ) {
		for ( uint32 i = 0; i < iByteCount; ++i ) {
			iValue = (iValue << 8u) | (uint64)pBytes[i];
		}
	} else {
		for ( uint32 i = 0; i < iByteCount; ++i ) {
			iValue |= ((uint64)pBytes[i]) << (8u * i);
		}
	}
	return iValue;
}


// xrtCodecLengthParse 相关处理
XXAPI xcodecstatus xrtCodecLengthParse(ptr pCtx, const xnetchain* pInput, xcodecframe* pFrame)
{
	xcodeclengthcodec* pCodec = (xcodeclengthcodec*)pCtx;
	uint8 aHeader[8];
	uint64 iDeclaredLen;
	int64_t iPayloadLen;
	size_t iFrameBytes;

	if ( !pCodec || !pInput || !pFrame ) return XCODEC_STATUS_ERROR;
	if ( pCodec->iFieldBytes == 0 || pCodec->iFieldBytes > sizeof(aHeader) ) return XCODEC_STATUS_ERROR;

	xrtCodecFrameInit(pFrame);
	if ( xrtNetChainBytes(pInput) < pCodec->iFieldBytes ) return XCODEC_STATUS_NEED_MORE;
	if ( __xcodecChainPeekAt(pInput, 0, aHeader, pCodec->iFieldBytes) != pCodec->iFieldBytes ) return XCODEC_STATUS_ERROR;

	iDeclaredLen = __xcodecReadUint(aHeader, pCodec->iFieldBytes, pCodec->bBigEndian);
	iPayloadLen = (int64_t)iDeclaredLen + (int64_t)pCodec->iLengthAdjust;
	if ( iPayloadLen < 0 ) return XCODEC_STATUS_ERROR;
	if ( pCodec->iMaxPayloadBytes > 0 && (uint64)iPayloadLen > (uint64)pCodec->iMaxPayloadBytes ) return XCODEC_STATUS_ERROR;

	iFrameBytes = (size_t)pCodec->iFieldBytes + (size_t)iPayloadLen;
	if ( xrtNetChainBytes(pInput) < iFrameBytes ) return XCODEC_STATUS_NEED_MORE;

	pFrame->iKind = XCODEC_KIND_LENGTH;
	pFrame->iHeaderBytes = pCodec->iFieldBytes;
	pFrame->iPayloadOffset = pCodec->iFieldBytes;
	pFrame->iPayloadBytes = (size_t)iPayloadLen;
	pFrame->iFrameBytes = iFrameBytes;
	pFrame->iMeta0 = iDeclaredLen;
	return XCODEC_STATUS_FRAME;
}


// xrtCodecLengthReset 相关处理
XXAPI void xrtCodecLengthReset(ptr pCtx)
{
	(void)pCtx;
}


// xrtCodecLengthOps 相关处理
XXAPI const xcodecparserops* xrtCodecLengthOps(void)
{
	static const xcodecparserops tOps = {
		xrtCodecLengthParse,
		xrtCodecLengthReset
	};
	return &tOps;
}


#endif
