#include "../internal/xrt_websocket.h"



#if defined(XRT_FEATURE_WEBSOCKET_FRAME)

#define XRT_WS_FRAME_FLAGS \
	((uint32)XWS_FRAME_FIN | (uint32)XWS_FRAME_MASKED | \
	(uint32)XWS_FRAME_RSV1 | (uint32)XWS_FRAME_RSV2 | \
	(uint32)XWS_FRAME_RSV3)

#define XRT_WS_FRAME_RSV \
	((uint16)XWS_FRAME_RSV1 | (uint16)XWS_FRAME_RSV2 | \
	(uint16)XWS_FRAME_RSV3)



/* 构造默认帧配置，不依赖公开输出地址的对齐方式。 */
static void __xrtWsFrameConfigDefault(xwsframeconfig* pConfig)
{
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->MaxPayload = XWS_FRAME_PAYLOAD_MAX;
	pConfig->AllowedOpcodes = XWS_OPCODES_STANDARD;
	pConfig->Mask = XWS_MASK_ANY;
}



/* 读取可选且可能未对齐的配置，空配置使用默认值。 */
static bool __xrtWsFrameConfigRead(
	const xwsframeconfig* pInput,
	xwsframeconfig* pConfig
)
{
	if ( pInput == NULL ) {
		__xrtWsFrameConfigDefault(pConfig);
		return true;
	}
	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
		return false;
	}
	memcpy(pConfig, pInput, sizeof(*pConfig));
	return true;
}



/* 清空解析输出，不直接解引用可能未对齐的公开结构。 */
static void __xrtWsFrameOutputsClear(
	xwsframe* pFrame,
	xwsframeerrorinfo* pError
)
{
	xwsframe Frame;
	xwsframeerrorinfo Error;

	memset(&Frame, 0, sizeof(Frame));
	memcpy(pFrame, &Frame, sizeof(Frame));
	if ( pError != NULL ) {
		memset(&Error, 0, sizeof(Error));
		memcpy(pError, &Error, sizeof(Error));
	}
}



/* 发布精确错误，同时保证未对齐输出只发生完整结构复制。 */
static void __xrtWsFrameErrorStore(
	xwsframeerrorinfo* pError,
	xwsframeerror Code,
	size_t iOffset
)
{
	xwsframeerrorinfo Error;

	if ( pError == NULL ) {
		return;
	}
	Error.Code = Code;
	Error.Offset = iOffset;
	memcpy(pError, &Error, sizeof(Error));
}



/* 返回参数错误时仍可安全写入且不会覆盖任何调用方输入的错误输出。 */
static xwsframeerrorinfo* __xrtWsFrameSafeError(
	xbytesview Input,
	const xwsframe* pFrame,
	const xwsframeconfig* pConfig,
	xwsframeerrorinfo* pError
)
{
	if ( (pError == NULL) ||
		!__xrtRangeValid(pError, sizeof(*pError)) ||
		__xrtRangesOverlap(
			pError, sizeof(*pError), Input.Data, Input.Size
		) || __xrtRangesOverlap(
			pError, sizeof(*pError), pFrame, sizeof(*pFrame)
		) || ((pConfig != NULL) && __xrtRangesOverlap(
			pError, sizeof(*pError), pConfig, sizeof(*pConfig)
		)) ) {
		return NULL;
	}
	return pError;
}



/* 验证解析的全部固定范围和可写输出隔离契约。 */
static bool __xrtWsFrameParseRangesValid(
	xbytesview Input,
	xwsframe* pFrame,
	const xwsframeconfig* pConfig,
	xwsframeerrorinfo* pError
)
{
	if ( !__xrtRangeValid(Input.Data, Input.Size) ||
		!__xrtRangeValid(pFrame, sizeof(*pFrame)) ||
		((pConfig != NULL) &&
		 !__xrtRangeValid(pConfig, sizeof(*pConfig))) ||
		((pError != NULL) &&
		 !__xrtRangeValid(pError, sizeof(*pError))) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pFrame, sizeof(*pFrame), Input.Data, Input.Size
	) || ((pConfig != NULL) && __xrtRangesOverlap(
		pFrame, sizeof(*pFrame), pConfig, sizeof(*pConfig)
	)) || ((pError != NULL) && (
		__xrtRangesOverlap(
			pError, sizeof(*pError), Input.Data, Input.Size
		) || __xrtRangesOverlap(
			pError, sizeof(*pError), pFrame, sizeof(*pFrame)
		) || ((pConfig != NULL) && __xrtRangesOverlap(
			pError, sizeof(*pError), pConfig, sizeof(*pConfig)
		))
	)) ) {
		return false;
	}
	return true;
}



/* 设置帧错误、精确位置和线程结构化错误。 */
static xwsframestatus __xrtWsFrameFail(
	xwsframe* pFrame,
	xwsframeerrorinfo* pError,
	xwsframeerror Code,
	size_t iOffset,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	if ( pFrame != NULL ) {
		__xrtWsFrameOutputsClear(pFrame, NULL);
	}
	__xrtWsFrameErrorStore(pError, Code, iOffset);
	__xrtWsFrameError(Kind, Code, sOperation, sMessage);
	return XWS_FRAME_ERROR;
}



/* 验证配置位图和掩码策略。 */
static bool __xrtWsFrameConfigValid(const xwsframeconfig* pConfig)
{
	if ( (pConfig->MaxPayload > XWS_FRAME_PAYLOAD_MAX) ||
		(pConfig->AllowedOpcodes == 0) ||
		((pConfig->AllowedRsv & (uint16)~XRT_WS_FRAME_RSV) != 0) ) {
		return false;
	}
	if ( (pConfig->Mask != XWS_MASK_ANY) &&
		(pConfig->Mask != XWS_MASK_REQUIRED) &&
		(pConfig->Mask != XWS_MASK_FORBIDDEN) ) {
		return false;
	}
	return true;
}



/* 把线路 RSV 位转换为公开逻辑标志。 */
static uint32 __xrtWsFrameRsvFlags(uint8 iByte)
{
	uint32 iFlags = 0;

	if ( (iByte & UINT8_C(0x40)) != 0 ) {
		iFlags |= (uint32)XWS_FRAME_RSV1;
	}
	if ( (iByte & UINT8_C(0x20)) != 0 ) {
		iFlags |= (uint32)XWS_FRAME_RSV2;
	}
	if ( (iByte & UINT8_C(0x10)) != 0 ) {
		iFlags |= (uint32)XWS_FRAME_RSV3;
	}
	return iFlags;
}



/* 把公开逻辑 RSV 标志转换为线路位。 */
static uint8 __xrtWsFrameRsvByte(uint32 iFlags)
{
	uint8 iByte = 0;

	if ( (iFlags & (uint32)XWS_FRAME_RSV1) != 0 ) {
		iByte |= UINT8_C(0x40);
	}
	if ( (iFlags & (uint32)XWS_FRAME_RSV2) != 0 ) {
		iByte |= UINT8_C(0x20);
	}
	if ( (iFlags & (uint32)XWS_FRAME_RSV3) != 0 ) {
		iByte |= UINT8_C(0x10);
	}
	return iByte;
}



/* 返回操作码对应的配置位。 */
static uint16 __xrtWsFrameOpcodeBit(uint8 iOpcode)
{
	return (uint16)(UINT16_C(1) << iOpcode);
}



/* 验证操作码、扩展位、掩码方向和控制帧结构。 */
static xwsframeerror __xrtWsFrameShapeError(
	const xwsframe* pFrame,
	const xwsframeconfig* pConfig
)
{
	uint32 iRsv = pFrame->Flags &
		((uint32)XWS_FRAME_RSV1 | (uint32)XWS_FRAME_RSV2 |
		(uint32)XWS_FRAME_RSV3);
	bool bControl;
	bool bMasked;

	if ( (pFrame->Flags & ~XRT_WS_FRAME_FLAGS) != 0 ) {
		return XWS_FRAME_ERROR_ARGUMENT;
	}
	if ( (pFrame->Opcode > UINT8_C(0x0F)) ||
		((pConfig->AllowedOpcodes &
		__xrtWsFrameOpcodeBit(pFrame->Opcode)) == 0) ) {
		return XWS_FRAME_ERROR_OPCODE;
	}
	if ( (iRsv & ~(uint32)pConfig->AllowedRsv) != 0 ) {
		return XWS_FRAME_ERROR_RSV;
	}
	bMasked = (pFrame->Flags & (uint32)XWS_FRAME_MASKED) != 0;
	if ( ((pConfig->Mask == XWS_MASK_REQUIRED) && !bMasked) ||
		((pConfig->Mask == XWS_MASK_FORBIDDEN) && bMasked) ) {
		return XWS_FRAME_ERROR_MASK;
	}
	if ( pFrame->PayloadSize > pConfig->MaxPayload ) {
		return XWS_FRAME_ERROR_LENGTH;
	}
	bControl = (pFrame->Opcode & UINT8_C(0x08)) != 0;
	if ( bControl &&
		(((pFrame->Flags & (uint32)XWS_FRAME_FIN) == 0) ||
		(pFrame->PayloadSize > UINT64_C(125))) ) {
		return XWS_FRAME_ERROR_CONTROL;
	}
	if ( (pFrame->Opcode == (uint8)XWS_OPCODE_CLOSE) &&
		(pFrame->PayloadSize == UINT64_C(1)) ) {
		return XWS_FRAME_ERROR_CLOSE;
	}
	return 0;
}



/* 返回帧形状错误对应的线路字节位置。 */
static size_t __xrtWsFrameShapeOffset(
	const xwsframe* pFrame,
	xwsframeerror Code
)
{
	if ( (Code == XWS_FRAME_ERROR_MASK) ||
		(Code == XWS_FRAME_ERROR_LENGTH) ||
		(Code == XWS_FRAME_ERROR_CLOSE) ) {
		return 1u;
	}
	if ( (Code == XWS_FRAME_ERROR_CONTROL) &&
		((pFrame->Flags & (uint32)XWS_FRAME_FIN) != 0) ) {
		return 1u;
	}
	return 0;
}



/* 将形状错误转换为稳定错误类别和消息。 */
static void __xrtWsFrameShapeErrorSet(
	xwsframeerror Code,
	cstr sOperation,
	bool bParse
)
{
	xerrkind Kind = bParse ? XERR_PROTOCOL : XERR_VALUE;
	cstr sMessage = "invalid WebSocket frame";

	switch ( Code ) {
		case XWS_FRAME_ERROR_ARGUMENT:
			Kind = XERR_ARGUMENT;
			sMessage = "WebSocket frame contains unknown flags";
			break;
		case XWS_FRAME_ERROR_RSV:
			sMessage = "WebSocket frame uses an unnegotiated RSV bit";
			break;
		case XWS_FRAME_ERROR_OPCODE:
			sMessage = "WebSocket frame uses a disabled opcode";
			break;
		case XWS_FRAME_ERROR_MASK:
			sMessage = "WebSocket frame violates the masking policy";
			break;
		case XWS_FRAME_ERROR_LENGTH:
			Kind = XERR_RANGE;
			sMessage = "WebSocket frame payload length is invalid";
			break;
		case XWS_FRAME_ERROR_CONTROL:
			sMessage = "WebSocket control frame is fragmented or too large";
			break;
		case XWS_FRAME_ERROR_CLOSE:
			sMessage = "WebSocket close frame has a one-byte payload";
			break;
		default:
			Kind = XERR_INTERNAL;
			break;
	}
	__xrtWsFrameError(Kind, Code, sOperation, sMessage);
}



/* 初始化默认帧配置。 */
XRT_API void xrtWsFrameConfigInit(xwsframeconfig* pConfig)
{
	xwsframeconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtWsFrameError(
			XERR_ARGUMENT, XWS_FRAME_ERROR_ARGUMENT,
			"init-websocket-frame-config",
			"WebSocket frame config range is invalid"
		);
		return;
	}
	__xrtWsFrameConfigDefault(&Config);
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化空帧描述。 */
XRT_API void xrtWsFrameInit(xwsframe* pFrame)
{
	xwsframe Frame;

	if ( !__xrtRangeValid(pFrame, sizeof(Frame)) ) {
		__xrtWsFrameError(
			XERR_ARGUMENT, XWS_FRAME_ERROR_ARGUMENT,
			"init-websocket-frame",
			"WebSocket frame range is invalid"
		);
		return;
	}
	memset(&Frame, 0, sizeof(Frame));
	memcpy(pFrame, &Frame, sizeof(Frame));
}



/* 增量解析一个完整帧头，不等待或借用负载。 */
XRT_API xwsframestatus xrtWsFrameParse(
	xbytesview Input,
	xwsframe* pFrame,
	const xwsframeconfig* pInputConfig,
	xwsframeerrorinfo* pError
)
{
	xwsframeconfig Config;
	xwsframe Frame;
	xwsframeerror ShapeError;
	uint8 iFirst;
	uint8 iSecond;
	uint8 iLengthCode;
	size_t iHeadSize;
	size_t i;

	if ( !__xrtWsFrameParseRangesValid(
		Input, pFrame, pInputConfig, pError
	) ) {
		return __xrtWsFrameFail(
			NULL,
			__xrtWsFrameSafeError(
				Input, pFrame, pInputConfig, pError
			),
			XWS_FRAME_ERROR_ARGUMENT, 0,
			XERR_ARGUMENT, "parse-websocket-frame",
			"WebSocket frame ranges are invalid or overlap"
		);
	}
	__xrtWsFrameOutputsClear(pFrame, pError);
	if ( !__xrtWsFrameConfigRead(pInputConfig, &Config) ) {
		return __xrtWsFrameFail(
			pFrame, pError, XWS_FRAME_ERROR_CONFIG, 0,
			XERR_ARGUMENT, "parse-websocket-frame",
			"WebSocket frame configuration range is invalid"
		);
	}
	if ( !__xrtWsFrameConfigValid(&Config) ) {
		return __xrtWsFrameFail(
			pFrame, pError, XWS_FRAME_ERROR_CONFIG, 0,
			XERR_ARGUMENT, "parse-websocket-frame",
			"WebSocket frame configuration is invalid"
		);
	}
	if ( Input.Size < 2u ) {
		return XWS_FRAME_MORE;
	}

	memset(&Frame, 0, sizeof(Frame));
	iFirst = Input.Data[0];
	iSecond = Input.Data[1];
	Frame.Opcode = (uint8)(iFirst & UINT8_C(0x0F));
	if ( (iFirst & UINT8_C(0x80)) != 0 ) {
		Frame.Flags |= (uint32)XWS_FRAME_FIN;
	}
	Frame.Flags |= __xrtWsFrameRsvFlags(iFirst);
	if ( (iSecond & UINT8_C(0x80)) != 0 ) {
		Frame.Flags |= (uint32)XWS_FRAME_MASKED;
	}

	iLengthCode = (uint8)(iSecond & UINT8_C(0x7F));
	iHeadSize = 2u;
	if ( iLengthCode < UINT8_C(126) ) {
		Frame.PayloadSize = (uint64)iLengthCode;
	} else if ( iLengthCode == UINT8_C(126) ) {
		if ( Input.Size < 4u ) {
			return XWS_FRAME_MORE;
		}
		Frame.PayloadSize =
			((uint64)Input.Data[2] << 8u) |
			(uint64)Input.Data[3];
		if ( Frame.PayloadSize < UINT64_C(126) ) {
			return __xrtWsFrameFail(
				pFrame, pError, XWS_FRAME_ERROR_LENGTH, 2,
				XERR_PROTOCOL, "parse-websocket-frame",
				"WebSocket frame uses a noncanonical 16-bit length"
			);
		}
		iHeadSize = 4u;
	} else {
		if ( Input.Size < 10u ) {
			return XWS_FRAME_MORE;
		}
		if ( (Input.Data[2] & UINT8_C(0x80)) != 0 ) {
			return __xrtWsFrameFail(
				pFrame, pError, XWS_FRAME_ERROR_LENGTH, 2,
				XERR_PROTOCOL, "parse-websocket-frame",
				"WebSocket frame payload length exceeds 63 bits"
			);
		}
		Frame.PayloadSize = 0;
		for ( i = 2u; i < 10u; i++ ) {
			Frame.PayloadSize =
				(Frame.PayloadSize << 8u) |
				(uint64)Input.Data[i];
		}
		if ( Frame.PayloadSize <= UINT64_C(65535) ) {
			return __xrtWsFrameFail(
				pFrame, pError, XWS_FRAME_ERROR_LENGTH, 2,
				XERR_PROTOCOL, "parse-websocket-frame",
				"WebSocket frame uses a noncanonical 64-bit length"
			);
		}
		iHeadSize = 10u;
	}

	ShapeError = __xrtWsFrameShapeError(&Frame, &Config);
	if ( ShapeError != 0 ) {
		__xrtWsFrameShapeErrorSet(
			ShapeError, "parse-websocket-frame", true
		);
		__xrtWsFrameOutputsClear(pFrame, NULL);
		__xrtWsFrameErrorStore(
			pError,
			ShapeError,
			__xrtWsFrameShapeOffset(&Frame, ShapeError)
		);
		return XWS_FRAME_ERROR;
	}

	if ( (Frame.Flags & (uint32)XWS_FRAME_MASKED) != 0 ) {
		if ( Input.Size < (iHeadSize + 4u) ) {
			return XWS_FRAME_MORE;
		}
		memcpy(
			Frame.Mask, Input.Data + iHeadSize, XWS_MASK_SIZE
		);
		iHeadSize += XWS_MASK_SIZE;
	}
	Frame.HeadSize = iHeadSize;
	memcpy(pFrame, &Frame, sizeof(Frame));
	return XWS_FRAME_READY;
}



/* 规范写出一个帧头，所有验证和构建都先在栈上完成。 */
XRT_API bool xrtWsFrameWrite(
	const xwsframe* pFrame,
	const xwsframeconfig* pInputConfig,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xwsframeconfig Config;
	xwsframe Frame;
	xwsframeerror ShapeError;
	uint8 pHead[XWS_FRAME_HEAD_MAX];
	size_t iHeadSize;
	size_t i;

	if ( !__xrtRangeValid(pFrame, sizeof(Frame)) ||
		!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		((pInputConfig != NULL) &&
		 !__xrtRangeValid(pInputConfig, sizeof(Config))) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize), pFrame, sizeof(Frame)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pOutput, iCapacity
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pFrame, sizeof(Frame)
		) || ((pInputConfig != NULL) && (
		__xrtRangesOverlap(
			pSize, sizeof(*pSize), pInputConfig, sizeof(Config)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pInputConfig, sizeof(Config)
		)
	)) ) {
		__xrtWsFrameError(
			XERR_ARGUMENT, XWS_FRAME_ERROR_ARGUMENT,
			"write-websocket-frame",
			"WebSocket frame ranges are invalid or overlap"
		);
		return false;
	}
	memcpy(&Frame, pFrame, sizeof(Frame));
	if ( !__xrtWsFrameConfigRead(pInputConfig, &Config) ) {
		__xrtWsFrameError(
			XERR_ARGUMENT, XWS_FRAME_ERROR_CONFIG,
			"write-websocket-frame",
			"WebSocket frame configuration range is invalid"
		);
		return false;
	}
	if ( !__xrtWsFrameConfigValid(&Config) ) {
		__xrtWsFrameError(
			XERR_ARGUMENT, XWS_FRAME_ERROR_CONFIG,
			"write-websocket-frame",
			"WebSocket frame configuration is invalid"
		);
		return false;
	}
	ShapeError = __xrtWsFrameShapeError(&Frame, &Config);
	if ( ShapeError != 0 ) {
		__xrtWsFrameShapeErrorSet(
			ShapeError, "write-websocket-frame", false
		);
		return false;
	}

	pHead[0] = (uint8)(Frame.Opcode |
		__xrtWsFrameRsvByte(Frame.Flags));
	if ( (Frame.Flags & (uint32)XWS_FRAME_FIN) != 0 ) {
		pHead[0] |= UINT8_C(0x80);
	}
	pHead[1] =
		((Frame.Flags & (uint32)XWS_FRAME_MASKED) != 0) ?
		UINT8_C(0x80) : UINT8_C(0);
	iHeadSize = 2u;
	if ( Frame.PayloadSize <= UINT64_C(125) ) {
		pHead[1] |= (uint8)Frame.PayloadSize;
	} else if ( Frame.PayloadSize <= UINT64_C(65535) ) {
		pHead[1] |= UINT8_C(126);
		pHead[2] = (uint8)(Frame.PayloadSize >> 8u);
		pHead[3] = (uint8)Frame.PayloadSize;
		iHeadSize = 4u;
	} else {
		pHead[1] |= UINT8_C(127);
		for ( i = 0; i < 8u; i++ ) {
			pHead[2u + i] = (uint8)(
				Frame.PayloadSize >> ((7u - i) * 8u)
			);
		}
		iHeadSize = 10u;
	}
	if ( (Frame.Flags & (uint32)XWS_FRAME_MASKED) != 0 ) {
		memcpy(
			pHead + iHeadSize, Frame.Mask, XWS_MASK_SIZE
		);
		iHeadSize += XWS_MASK_SIZE;
	}

	if ( pOutput == NULL ) {
		memcpy(pSize, &iHeadSize, sizeof(iHeadSize));
		return true;
	}
	if ( iCapacity < iHeadSize ) {
		memcpy(pSize, &iHeadSize, sizeof(iHeadSize));
		__xrtWsFrameError(
			XERR_RANGE, XWS_FRAME_ERROR_OUTPUT,
			"write-websocket-frame",
			"WebSocket frame output capacity is insufficient"
		);
		return false;
	}
	memcpy(pOutput, pHead, iHeadSize);
	memcpy(pSize, &iHeadSize, sizeof(iHeadSize));
	return true;
}



/* 使用重复八字节掩码字优化大块原地处理。 */
XRT_API bool xrtWsMask(
	void* pData,
	size_t iSize,
	const uint8 pMask[XWS_MASK_SIZE],
	uint64 iOffset
)
{
	uint8* pBytes = (uint8*)pData;
	uint8 pMaskCopy[XWS_MASK_SIZE];
	uint8 pPattern[8];
	uint64 iMask;
	size_t iRotation;
	size_t i = 0;

	if ( !__xrtRangeValid(pData, iSize) ||
		!__xrtRangeValid(pMask, XWS_MASK_SIZE) ) {
		__xrtWsFrameError(
			XERR_ARGUMENT, XWS_FRAME_ERROR_ARGUMENT,
			"mask-websocket-payload",
			"WebSocket payload or mask pointer is invalid"
		);
		return false;
	}
	memcpy(pMaskCopy, pMask, sizeof(pMaskCopy));
	iRotation = (size_t)(iOffset & UINT64_C(3));
	for ( i = 0; i < sizeof(pPattern); i++ ) {
		pPattern[i] = pMaskCopy[(iRotation + i) & 3u];
	}
	memcpy(&iMask, pPattern, sizeof(iMask));

	i = 0;
	while ( (iSize - i) >= sizeof(uint64) ) {
		uint64 iValue;

		memcpy(&iValue, pBytes + i, sizeof(iValue));
		iValue ^= iMask;
		memcpy(pBytes + i, &iValue, sizeof(iValue));
		i += sizeof(uint64);
	}
	while ( i < iSize ) {
		pBytes[i] ^= pPattern[i & 7u];
		i++;
	}
	return true;
}

#endif
