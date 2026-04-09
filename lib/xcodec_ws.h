#ifndef XRT_XCODEC_WS_H
#define XRT_XCODEC_WS_H

/*
	XRT mainline WebSocket frame codec.

	This header provides:
	  - frame header parsing for FIN/opcode/mask/payload length
	  - payload boundary metadata over xnetchain
	  - masking helpers used by the upper WebSocket layer

	Message reassembly remains the responsibility of xws.
*/


/* ============================== WebSocket public model ============================== */

#if !defined(XRT_BUILD_CORE)

#define XCODEC_WS_OPCODE_CONT   0x0u
#define XCODEC_WS_OPCODE_TEXT   0x1u
#define XCODEC_WS_OPCODE_BINARY 0x2u
#define XCODEC_WS_OPCODE_CLOSE  0x8u
#define XCODEC_WS_OPCODE_PING   0x9u
#define XCODEC_WS_OPCODE_PONG   0xAu

#define XCODEC_WS_F_NONE     0x00000000u
#define XCODEC_WS_F_FIN      0x00000001u
#define XCODEC_WS_F_MASKED   0x00000002u
#define XCODEC_WS_F_CONTROL  0x00000004u

typedef struct {
	uint32 iFlags;
	uint8 iOpcode;
	uint8 aMask[4];
	uint64 iPayloadLen;
	size_t iHeaderBytes;
} xcodecwsframeinfo;

#endif /* !XRT_BUILD_CORE */



/* ============================== WebSocket frame parser ============================== */

XXAPI void xrtCodecWsFrameInit(xcodecwsframeinfo* pInfo)
{
	if ( !pInfo ) { return; }
	memset(pInfo, 0, sizeof(xcodecwsframeinfo));
}


// 解析编解码器 WebSocket frame
XXAPI xcodecstatus xrtCodecWsParseFrame(const xnetchain* pInput, xcodecframe* pFrame, xcodecwsframeinfo* pInfo)
{
	uint8 aHead[14];
	uint8 iB0;
	uint8 iB1;
	uint64 iPayloadLen;
	size_t iHeaderBytes = 2;
	size_t iNeedBytes;
	bool bMasked;

	if ( !pInput || !pFrame || !pInfo ) { return XCODEC_STATUS_ERROR; }
	xrtCodecFrameInit(pFrame);
	xrtCodecWsFrameInit(pInfo);

	// 至少需要 2 字节才能读取基本帧头
	if ( xrtNetChainBytes(pInput) < 2 ) { return XCODEC_STATUS_NEED_MORE; }
	if ( __xcodecChainPeekAt(pInput, 0, aHead, 2) != 2 ) { return XCODEC_STATUS_ERROR; }

	// 解析前两个字节：FIN/opcode 和 MASK/负载长度标志
	iB0 = aHead[0];
	iB1 = aHead[1];
	bMasked = (iB1 & 0x80u) != 0;
	iPayloadLen = (uint64)(iB1 & 0x7Fu);

	// 根据负载长度标志读取扩展长度字段
	if ( iPayloadLen == 126u ) {
		// 16 位扩展长度
		iHeaderBytes += 2u;
		if ( xrtNetChainBytes(pInput) < iHeaderBytes ) { return XCODEC_STATUS_NEED_MORE; }
		if ( __xcodecChainPeekAt(pInput, 2, aHead + 2, 2) != 2 ) { return XCODEC_STATUS_ERROR; }
		iPayloadLen = ((uint64)aHead[2] << 8u) | (uint64)aHead[3];
	} else if ( iPayloadLen == 127u ) {
		// 64 位扩展长度
		iHeaderBytes += 8u;
		if ( xrtNetChainBytes(pInput) < iHeaderBytes ) { return XCODEC_STATUS_NEED_MORE; }
		if ( __xcodecChainPeekAt(pInput, 2, aHead + 2, 8) != 8 ) { return XCODEC_STATUS_ERROR; }
		iPayloadLen = 0;
		for ( uint32 i = 0; i < 8; ++i ) {
			iPayloadLen = (iPayloadLen << 8u) | (uint64)aHead[2 + i];
		}
	}

	// 若有掩码标志则读取 4 字节掩码
	if ( bMasked ) {
		iNeedBytes = iHeaderBytes + 4u;
		if ( xrtNetChainBytes(pInput) < iNeedBytes ) { return XCODEC_STATUS_NEED_MORE; }
		if ( __xcodecChainPeekAt(pInput, iHeaderBytes, pInfo->aMask, 4) != 4 ) { return XCODEC_STATUS_ERROR; }
		iHeaderBytes += 4u;
	}

	// 检查完整帧是否已到达
	if ( iPayloadLen > (uint64)(SIZE_MAX - iHeaderBytes) ) { return XCODEC_STATUS_ERROR; }
	if ( xrtNetChainBytes(pInput) < iHeaderBytes + (size_t)iPayloadLen ) { return XCODEC_STATUS_NEED_MORE; }

	// 填充帧信息结构
	pInfo->iOpcode = (uint8)(iB0 & 0x0Fu);
	pInfo->iPayloadLen = iPayloadLen;
	pInfo->iHeaderBytes = iHeaderBytes;
	if ( iB0 & 0x80u ) { pInfo->iFlags |= XCODEC_WS_F_FIN; }
	if ( bMasked ) { pInfo->iFlags |= XCODEC_WS_F_MASKED; }
	// 操作码 >= 0x8 为控制帧
	if ( pInfo->iOpcode >= 0x8u ) { pInfo->iFlags |= XCODEC_WS_F_CONTROL; }

	// 填充通用帧描述
	pFrame->iKind = XCODEC_KIND_WS;
	pFrame->iHeaderBytes = iHeaderBytes;
	pFrame->iPayloadOffset = iHeaderBytes;
	pFrame->iPayloadBytes = (size_t)iPayloadLen;
	pFrame->iFrameBytes = iHeaderBytes + (size_t)iPayloadLen;
	pFrame->iMeta0 = pInfo->iOpcode;
	pFrame->iMeta1 = pInfo->iFlags;

	// 映射 WebSocket 特有标志到通用帧标志
	if ( pInfo->iFlags & XCODEC_WS_F_FIN ) { pFrame->iFlags |= XCODEC_FRAME_F_FIN; }
	if ( pInfo->iFlags & XCODEC_WS_F_MASKED ) { pFrame->iFlags |= XCODEC_FRAME_F_MASKED; }
	if ( pInfo->iFlags & XCODEC_WS_F_CONTROL ) { pFrame->iFlags |= XCODEC_FRAME_F_CONTROL; }
	if ( pInfo->iOpcode == XCODEC_WS_OPCODE_TEXT ) { pFrame->iFlags |= XCODEC_FRAME_F_TEXT; }
	if ( pInfo->iOpcode == XCODEC_WS_OPCODE_BINARY ) { pFrame->iFlags |= XCODEC_FRAME_F_BINARY; }
	return XCODEC_STATUS_FRAME;
}


// xrtCodecWsUnmask 相关处理
XXAPI void xrtCodecWsUnmask(ptr pData, size_t iLen, const uint8 aMask[4], size_t iStartOffset)
{
	uint8* pBytes = (uint8*)pData;
	if ( !pBytes || !aMask ) { return; }
	for ( size_t i = 0; i < iLen; ++i ) {
		pBytes[i] ^= aMask[(iStartOffset + i) & 3u];
	}
}


#endif
