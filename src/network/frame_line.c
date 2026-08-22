#include "../internal/xrt_net_buffer.h"



#if defined(XRT_FEATURE_NET_FRAME_LINE)

#define XRT_NET_LINE_GUARD UINT32_C(0x4C494E45)



/* 从当前块内候选位置直接比较任意长度分隔符。 */
static bool __xrtNetLineMatch(
	const xnetblock* pBlock,
	size_t iOffset,
	xbytesview Delimiter
)
{
	size_t iCompared = 0;

	while ( iCompared < Delimiter.Size ) {
		size_t iReadable;
		size_t iChunk;

		if ( pBlock == NULL ) {
			return false;
		}
		iReadable = __xrtNetBlockReadable(pBlock);
		if ( iOffset >= iReadable ) {
			pBlock = pBlock->Next;
			iOffset = 0;
			continue;
		}
		iChunk = iReadable - iOffset;
		if ( iChunk > (Delimiter.Size - iCompared) ) {
			iChunk = Delimiter.Size - iCompared;
		}
		if ( memcmp(
				__xrtNetBlockData(pBlock) + pBlock->Begin + iOffset,
				Delimiter.Data + iCompared, iChunk
			) != 0 ) {
			return false;
		}
		iCompared += iChunk;
		iOffset += iChunk;
	}
	return true;
}



/* 报告行 payload 超过配置硬上限，并重置搜索进度。 */
static xnetframestatus __xrtNetLineLimit(xnetlineframer* pFramer)
{
	pFramer->Input = NULL;
	pFramer->Cursor = NULL;
	pFramer->CursorOffset = 0;
	pFramer->Search = 0;
	pFramer->PreviousSize = 0;
	__xrtNetSetError(XERR_RANGE, XNET_ERROR_FRAME_LIMIT,
		"line frame", "line payload exceeds configured limit", 0);
	return XNET_FRAME_ERROR;
}



/* 初始化适合文本协议的安全默认配置。 */
XRT_API void xrtNetLineConfigInit(xnetlineconfig* pConfig)
{
	static const uint8 Delimiter[] = { '\n' };

	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Delimiter.Data = Delimiter;
	pConfig->Delimiter.Size = sizeof(Delimiter);
	pConfig->MaxPayload = 8192;
}



/* 验证借用分隔符并一次提交可复用状态。 */
XRT_API bool xrtNetLineInit(
	xnetlineframer* pFramer,
	const xnetlineconfig* pConfig
)
{
	xnetlineframer Next;

	if ( (pFramer == NULL) || (pConfig == NULL) ||
		 !__xrtRangeValid(
			pConfig->Delimiter.Data, pConfig->Delimiter.Size
		) || (pConfig->Delimiter.Size == 0) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_FRAME_CONFIG,
			"line frame", "invalid line frame configuration", 0);
		return false;
	}
	memset(&Next, 0, sizeof(Next));
	Next.Config = *pConfig;
	Next.Guard = XRT_NET_LINE_GUARD;
	*pFramer = Next;
	return true;
}



/* 保留经过验证的配置，清空只与当前输入前缀有关的状态。 */
XRT_API bool xrtNetLineReset(xnetlineframer* pFramer)
{
	if ( (pFramer == NULL) ||
		 (pFramer->Guard != XRT_NET_LINE_GUARD) ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_FRAME_STATE,
			"line frame", "invalid line framer state", 0);
		return false;
	}
	pFramer->Input = NULL;
	pFramer->Cursor = NULL;
	pFramer->CursorOffset = 0;
	pFramer->Search = 0;
	pFramer->PreviousSize = 0;
	return true;
}



/* 从上次未决候选继续搜索，避免每次追加后重新扫描完整前缀。 */
XRT_API xnetframestatus xrtNetLineNext(
	xnetlineframer* pFramer,
	const xnetbuf* pInput,
	xnetframe* pFrame
)
{
	size_t iSize;
	xbytesview Delimiter;

	if ( (pFramer == NULL) || (pInput == NULL) || (pFrame == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_FRAME_ERROR;
	}
	if ( (pFramer->Guard != XRT_NET_LINE_GUARD) ||
		 !__xrtRangeValid(
			pFramer->Config.Delimiter.Data,
			pFramer->Config.Delimiter.Size
		) || (pFramer->Config.Delimiter.Size == 0) ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_FRAME_STATE,
			"line frame", "invalid line framer state", 0);
		return XNET_FRAME_ERROR;
	}
	memset(pFrame, 0, sizeof(*pFrame));
	iSize = xrtNetBufSize(pInput);
	Delimiter = pFramer->Config.Delimiter;
	if ( (pFramer->Input != NULL) &&
		(pFramer->Input != pInput) &&
		(pFramer->PreviousSize != 0) ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_FRAME_STATE,
			"line frame", "line input changed before reset", 0);
		return XNET_FRAME_ERROR;
	}
	if ( iSize < pFramer->PreviousSize ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_FRAME_STATE,
			"line frame", "line input prefix was consumed before reset", 0);
		return XNET_FRAME_ERROR;
	}
	if ( pFramer->Input != pInput ) {
		pFramer->Input = pInput;
		pFramer->Cursor = pInput->Head;
		pFramer->CursorOffset = 0;
		pFramer->Search = 0;
	}
	if ( (pFramer->Cursor == NULL) && (pFramer->Search == 0) ) {
		pFramer->Cursor = pInput->Head;
	}
	while ( pFramer->Cursor != NULL ) {
		xnetblock* pBlock = pFramer->Cursor;
		size_t iReadable = __xrtNetBlockReadable(pBlock);
		cbytes pBegin;
		cbytes pFound;
		size_t iAdvance;

		if ( pFramer->CursorOffset >= iReadable ) {
			if ( pBlock->Next == NULL ) {
				break;
			}
			pFramer->Cursor = pBlock->Next;
			pFramer->CursorOffset = 0;
			continue;
		}
		pBegin = __xrtNetBlockData(pBlock) + pBlock->Begin;
		pFound = (cbytes)memchr(
			pBegin + pFramer->CursorOffset,
			Delimiter.Data[0], iReadable - pFramer->CursorOffset
		);
		if ( pFound == NULL ) {
			iAdvance = iReadable - pFramer->CursorOffset;
			pFramer->CursorOffset = iReadable;
			pFramer->Search += iAdvance;
			continue;
		}
		iAdvance = (size_t)(pFound -
			(pBegin + pFramer->CursorOffset));
		pFramer->CursorOffset += iAdvance;
		pFramer->Search += iAdvance;
		if ( pFramer->Search > pFramer->Config.MaxPayload ) {
			return __xrtNetLineLimit(pFramer);
		}
		if ( (iSize - pFramer->Search) < Delimiter.Size ) {
			pFramer->PreviousSize = iSize;
			return XNET_FRAME_MORE;
		}
		if ( __xrtNetLineMatch(
				pBlock, pFramer->CursorOffset, Delimiter
			) ) {
			size_t iFrameSize = pFramer->Search + Delimiter.Size;

			pFrame->PayloadSize = pFramer->Config.IncludeDelimiter ?
				iFrameSize : pFramer->Search;
			pFrame->FrameSize = iFrameSize;
			pFrame->Declared = (uint64)pFramer->Search;
			xrtNetLineReset(pFramer);
			return XNET_FRAME_READY;
		}
		pFramer->CursorOffset++;
		pFramer->Search++;
	}
	pFramer->PreviousSize = iSize;
	if ( iSize > pFramer->Config.MaxPayload ) {
		return __xrtNetLineLimit(pFramer);
	}
	return XNET_FRAME_MORE;
}



#undef XRT_NET_LINE_GUARD

#endif
