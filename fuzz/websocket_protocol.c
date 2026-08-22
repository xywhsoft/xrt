#include <stdlib.h>
#include <string.h>

#include <xrt/websocket.h>



#define XRT_WS_FUZZ_INPUT_MAX ((size_t)1048576u)
#define XRT_WS_FUZZ_PAYLOAD_MAX ((size_t)4096u)
#define XRT_WS_FUZZ_TRANSFORM_MAX UINT64_C(65536)



/* 返回输入中的一个选择字节，短输入按零补齐。 */
static uint8 __xrtWsFuzzByte(
	const uint8* pData,
	size_t iSize,
	size_t iOffset
)
{
	return iOffset < iSize ? pData[iOffset] : 0;
}



#if defined(XRT_FEATURE_WEBSOCKET_INFLATER) || \
	defined(XRT_FEATURE_WEBSOCKET_DEFLATER)

/* 变换输出只累计长度，用于覆盖任意分块而不保留扩展结果。 */
static bool __xrtWsFuzzOutput(xbytesview Data, ptr pData)
{
	uint64* pSize = (uint64*)pData;

	if ( UINT64_MAX - *pSize < (uint64)Data.Size ) {
		abort();
	}
	*pSize += (uint64)Data.Size;
	if ( *pSize > XRT_WS_FUZZ_TRANSFORM_MAX ) {
		abort();
	}
	return true;
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_FRAME)

/* 让严格帧解析器处理任意线路前缀和扩展策略组合。 */
static void __xrtWsFuzzFrame(
	const uint8* pData,
	size_t iSize
)
{
	xwsframeconfig Config;
	xwsframeerrorinfo Error;
	xwsframe Frame;
	xwsframestatus Status;

	xrtWsFrameConfigInit(&Config);
	Config.MaxPayload = XRT_WS_FUZZ_PAYLOAD_MAX;
	Config.AllowedOpcodes = UINT16_MAX;
	Config.AllowedRsv =
		(uint16)XWS_FRAME_RSV1 |
		(uint16)XWS_FRAME_RSV2 |
		(uint16)XWS_FRAME_RSV3;
	Config.Mask = (xwsmaskpolicy)(
		__xrtWsFuzzByte(pData, iSize, 0) % 3u
	);
	Status = xrtWsFrameParse(
		(xbytesview) { pData, iSize },
		&Frame,
		&Config,
		&Error
	);

	#if defined(XRT_FEATURE_WEBSOCKET_MESSAGE)
		if ( (Status == XWS_FRAME_READY) &&
			(Frame.HeadSize <= iSize) &&
			(Frame.PayloadSize <= XRT_WS_FUZZ_PAYLOAD_MAX) &&
			(Frame.PayloadSize <=
			 (uint64)(iSize - Frame.HeadSize)) ) {
			xwsmessageconfig MessageConfig;
			xwsmessagestate State;
			xwsmessageerrorinfo MessageError;
			xwsmessageinfo Info;
			uint8 Payload[XRT_WS_FUZZ_PAYLOAD_MAX];
			size_t iPayload = (size_t)Frame.PayloadSize;

			if ( iPayload != 0 ) {
				memcpy(
					Payload,
					pData + Frame.HeadSize,
					iPayload
				);
			}
			if ( (Frame.Flags & XWS_FRAME_MASKED) != 0 ) {
				(void)xrtWsMask(
					Payload,
					iPayload,
					Frame.Mask,
					0
				);
			}
			xrtWsMessageConfigInit(&MessageConfig);
			MessageConfig.MaxSize = XRT_WS_FUZZ_PAYLOAD_MAX;
			MessageConfig.FirstRsv =
				(uint16)XWS_FRAME_RSV1 |
				(uint16)XWS_FRAME_RSV2 |
				(uint16)XWS_FRAME_RSV3;
			MessageConfig.ContinuationRsv =
				MessageConfig.FirstRsv;
			MessageConfig.ControlRsv =
				MessageConfig.FirstRsv;
			if ( xrtWsMessageInit(&State, &MessageConfig) &&
				xrtWsMessageFrameBegin(
					&State,
					&Frame,
					&Info,
					&MessageError
				) ) {
				if ( xrtWsMessagePayload(
					&State,
					(xbytesview) { Payload, iPayload },
					&MessageError
				) ) {
					(void)xrtWsMessageFrameEnd(
						&State,
						&MessageError
					);
				}
			}
		}
	#else
		(void)Status;
	#endif
	xrtClearError();
}



/* 从输入构造合法帧头，并要求写出和解析结果严格往返。 */
static void __xrtWsFuzzFrameRoundTrip(
	const uint8* pData,
	size_t iSize
)
{
	static const uint8 Opcodes[] = {
		XWS_OPCODE_CONTINUATION,
		XWS_OPCODE_TEXT,
		XWS_OPCODE_BINARY,
		XWS_OPCODE_CLOSE,
		XWS_OPCODE_PING,
		XWS_OPCODE_PONG
	};
	xwsframe Parsed;
	xwsframe Frame;
	xbytesview Input;
	uint8 Head[XWS_FRAME_HEAD_MAX];
	size_t iHeadSize = 0;
	bool bControl;

	xrtWsFrameInit(&Frame);
	Frame.Opcode = Opcodes[
		__xrtWsFuzzByte(pData, iSize, 1) %
		(sizeof(Opcodes) / sizeof(Opcodes[0]))
	];
	bControl = (Frame.Opcode & UINT8_C(0x08)) != 0;
	Frame.Flags = bControl ||
		((__xrtWsFuzzByte(pData, iSize, 2) & 1u) != 0) ?
			XWS_FRAME_FIN : 0;
	if ( (__xrtWsFuzzByte(pData, iSize, 3) & 1u) != 0 ) {
		Frame.Flags |= XWS_FRAME_MASKED;
	}
	Frame.PayloadSize = (uint64)(iSize > XRT_WS_FUZZ_PAYLOAD_MAX ?
		XRT_WS_FUZZ_PAYLOAD_MAX : iSize);
	if ( bControl && (Frame.PayloadSize > XWS_CLOSE_PAYLOAD_MAX) ) {
		Frame.PayloadSize = XWS_CLOSE_PAYLOAD_MAX;
	}
	for ( size_t i = 0; i < XWS_MASK_SIZE; i++ ) {
		Frame.Mask[i] = __xrtWsFuzzByte(
			pData,
			iSize,
			i + 4u
		);
	}

	if ( !xrtWsFrameWrite(
		&Frame,
		NULL,
		Head,
		sizeof(Head),
		&iHeadSize
	) ) {
		abort();
	}
	Input.Data = Head;
	Input.Size = iHeadSize;
	if ( (xrtWsFrameParse(
		Input,
		&Parsed,
		NULL,
		NULL
	) != XWS_FRAME_READY) ||
		(Parsed.Flags != Frame.Flags) ||
		(Parsed.Opcode != Frame.Opcode) ||
		(Parsed.PayloadSize != Frame.PayloadSize) ||
		(Parsed.HeadSize != iHeadSize) ||
		(((Frame.Flags & XWS_FRAME_MASKED) != 0) &&
		 (memcmp(Parsed.Mask, Frame.Mask, XWS_MASK_SIZE) != 0)) ) {
		abort();
	}
}



/* 验证任意负载的掩码与解除掩码保持对称。 */
static void __xrtWsFuzzMask(
	const uint8* pData,
	size_t iSize
)
{
	uint8 Buffer[XRT_WS_FUZZ_PAYLOAD_MAX];
	uint8 Mask[XWS_MASK_SIZE];
	size_t iCopy = iSize > sizeof(Buffer) ?
		sizeof(Buffer) : iSize;
	uint64 iOffset = (uint64)__xrtWsFuzzByte(
		pData,
		iSize,
		8
	);

	if ( iCopy != 0 ) {
		memcpy(Buffer, pData, iCopy);
	}
	for ( size_t i = 0; i < XWS_MASK_SIZE; i++ ) {
		Mask[i] = __xrtWsFuzzByte(
			pData,
			iSize,
			i + 9u
		);
	}
	if ( !xrtWsMask(Buffer, iCopy, Mask, iOffset) ||
		!xrtWsMask(Buffer, iCopy, Mask, iOffset) ||
		((iCopy != 0) &&
		 (memcmp(Buffer, pData, iCopy) != 0)) ) {
		abort();
	}
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_CLOSE)

/* 让 Close 解析器覆盖任意协议允许长度。 */
static void __xrtWsFuzzClose(
	const uint8* pData,
	size_t iSize
)
{
	xwsclose Close;
	size_t iPayload = iSize > XWS_CLOSE_PAYLOAD_MAX ?
		XWS_CLOSE_PAYLOAD_MAX : iSize;

	(void)xrtWsCloseParse(
		(xbytesview) { pData, iPayload },
		&Close
	);
	xrtClearError();
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)

/* 覆盖任意子协议列表、Key 和 Accept 文本。 */
static void __xrtWsFuzzHandshake(
	const uint8* pData,
	size_t iSize
)
{
	xstrview Text = { (cstr)pData, iSize };
	xstrview Protocol;
	char Accept[XWS_ACCEPT_CAPACITY];
	size_t iOffset = 0;

	(void)xrtWsKeyValid(Text);
	(void)xrtWsAccept(Text, Accept, sizeof(Accept));
	(void)xrtWsAcceptValid(Text, Text);
	(void)xrtWsProtocolsValid(Text);
	for ( size_t i = 0; i <= iSize; i++ ) {
		xhttpnext Next = xrtWsProtocolNext(
			Text,
			&iOffset,
			&Protocol
		);

		if ( Next != XHTTP_NEXT_ITEM ) {
			break;
		}
	}
	xrtClearError();
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION)

/* 迭代任意扩展和参数文本，并把 Deflate 项交给严格协商解析器。 */
static void __xrtWsFuzzExtension(
	const uint8* pData,
	size_t iSize
)
{
	xstrview Text = { (cstr)pData, iSize };
	xwsextension Extension;
	size_t iOffset = 0;
	size_t iCount;

	(void)xrtWsExtensionCount(Text, &iCount);
	for ( size_t i = 0; i <= iSize; i++ ) {
		xhttpnext Next = xrtWsExtensionNext(
			Text,
			&iOffset,
			&Extension
		);

		if ( Next != XHTTP_NEXT_ITEM ) {
			break;
		}
		{
			xhttpparam Param;
			size_t iParam = 0;

			for ( size_t j = 0;
				j <= Extension.Parameters.Size;
				j++ ) {
				if ( xrtWsExtensionParamNext(
					&Extension,
					&iParam,
					&Param
				) != XHTTP_NEXT_ITEM ) {
					break;
				}
			}
		}
		#if defined(XRT_FEATURE_WEBSOCKET_DEFLATE)
			if ( xrtWsDeflateIs(&Extension) ) {
				xwsdeflate Deflate;

				(void)xrtWsDeflateOfferParse(
					&Extension,
					&Deflate
				);
				(void)xrtWsDeflateResponseParse(
					&Extension,
					&Deflate
				);
			}
		#endif
	}
	xrtClearError();
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_INFLATER)

/* 使用硬输出上限让 Inflate 状态机处理任意压缩消息。 */
static void __xrtWsFuzzInflater(
	const uint8* pData,
	size_t iSize
)
{
	xwsinflaterconfig Config;
	xwsinflater* pInflater;
	size_t iInput = iSize > XRT_WS_FUZZ_PAYLOAD_MAX ?
		XRT_WS_FUZZ_PAYLOAD_MAX : iSize;
	uint64 iOutput = 0;

	xrtWsInflaterConfigInit(&Config);
	Config.OutputLimit = XRT_WS_FUZZ_TRANSFORM_MAX;
	pInflater = xrtWsInflaterCreate(&Config);
	if ( pInflater == NULL ) {
		xrtClearError();
		return;
	}
	if ( xrtWsInflaterBegin(pInflater, true) &&
		xrtWsInflaterWrite(
			pInflater,
			(xbytesview) { pData, iInput },
			__xrtWsFuzzOutput,
			&iOutput
		) ) {
		(void)xrtWsInflaterEnd(
			pInflater,
			__xrtWsFuzzOutput,
			&iOutput
		);
	}
	xrtWsInflaterDestroy(pInflater);
	xrtClearError();
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_DEFLATER)

/* 压缩任意语义负载，并约束累计线路输出。 */
static void __xrtWsFuzzDeflater(
	const uint8* pData,
	size_t iSize
)
{
	xwsdeflaterconfig Config;
	xwsdeflater* pDeflater;
	size_t iInput = iSize > XRT_WS_FUZZ_PAYLOAD_MAX ?
		XRT_WS_FUZZ_PAYLOAD_MAX : iSize;
	uint64 iOutput = 0;

	xrtWsDeflaterConfigInit(&Config);
	Config.OutputLimit = XRT_WS_FUZZ_TRANSFORM_MAX;
	Config.Level = (int32)(
		__xrtWsFuzzByte(pData, iSize, 14) % 10u
	);
	pDeflater = xrtWsDeflaterCreate(&Config);
	if ( pDeflater == NULL ) {
		xrtClearError();
		return;
	}
	if ( xrtWsDeflaterBegin(pDeflater, true) &&
		xrtWsDeflaterWrite(
			pDeflater,
			(xbytesview) { pData, iInput },
			__xrtWsFuzzOutput,
			&iOutput
		) ) {
		(void)xrtWsDeflaterEnd(
			pDeflater,
			__xrtWsFuzzOutput,
			&iOutput
		);
	}
	xrtWsDeflaterDestroy(pDeflater);
	xrtClearError();
}

#endif



/* 统一公开给 libFuzzer 和确定性回归驱动的协议入口。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	if ( ((pData == NULL) && (iSize != 0)) ||
		(iSize > XRT_WS_FUZZ_INPUT_MAX) ) {
		return 0;
	}

	#if defined(XRT_FEATURE_WEBSOCKET_FRAME)
		__xrtWsFuzzFrame(pData, iSize);
		__xrtWsFuzzFrameRoundTrip(pData, iSize);
		__xrtWsFuzzMask(pData, iSize);
	#endif
	#if defined(XRT_FEATURE_WEBSOCKET_CLOSE)
		__xrtWsFuzzClose(pData, iSize);
	#endif
	#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)
		__xrtWsFuzzHandshake(pData, iSize);
	#endif
	#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION)
		__xrtWsFuzzExtension(pData, iSize);
	#endif
	#if defined(XRT_FEATURE_WEBSOCKET_INFLATER)
		__xrtWsFuzzInflater(pData, iSize);
	#endif
	#if defined(XRT_FEATURE_WEBSOCKET_DEFLATER)
		__xrtWsFuzzDeflater(pData, iSize);
	#endif
	return 0;
}
