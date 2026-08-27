#include "../internal/xrt_websocket.h"



#if defined(XRT_FEATURE_WEBSOCKET_MESSAGE)

#define __XRT_WS_MESSAGE_RSV \
	(XWS_FRAME_RSV1 | XWS_FRAME_RSV2 | XWS_FRAME_RSV3)

#define __XRT_WS_MESSAGE_FRAME_FLAGS \
	(XWS_FRAME_FIN | XWS_FRAME_MASKED | __XRT_WS_MESSAGE_RSV)



/* 判断借用负载视图是否具有完整且不回绕的内存范围。 */
static bool __xrtWsMessageViewValid(xbytesview Payload)
{
	return __xrtRangeValid(Payload.Data, Payload.Size);
}



/* 构造默认消息配置，不依赖公开输出地址的对齐方式。 */
static void __xrtWsMessageConfigDefault(xwsmessageconfig* pConfig)
{
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->MaxSize = SIZE_MAX;
	pConfig->ValidateText = true;
}



/* 读取可选且可能未对齐的配置，空配置使用默认值。 */
static bool __xrtWsMessageConfigRead(
	const xwsmessageconfig* pInput,
	xwsmessageconfig* pConfig
)
{
	if ( pInput == NULL ) {
		__xrtWsMessageConfigDefault(pConfig);
		return true;
	}
	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
		return false;
	}
	memcpy(pConfig, pInput, sizeof(*pConfig));
	return true;
}



/* 验证扩展位图和消息上限配置。 */
static bool __xrtWsMessageConfigValid(
	const xwsmessageconfig* pConfig
)
{
	uint16 iRsv;

	iRsv = (uint16)(
		pConfig->FirstRsv |
		pConfig->ContinuationRsv |
		pConfig->ControlRsv
	);
	return (iRsv & (uint16)~__XRT_WS_MESSAGE_RSV) == 0;
}



/* 清空可选消息错误详情。 */
static void __xrtWsMessageErrorClear(
	xwsmessageerrorinfo* pError
)
{
	xwsmessageerrorinfo Error;

	if ( pError != NULL ) {
		memset(&Error, 0, sizeof(Error));
		Error.Offset = XRT_NPOS;
		memcpy(pError, &Error, sizeof(Error));
	}
}



/* 发布协议或调用状态错误，并按需使当前连接状态失效。 */
static bool __xrtWsMessageFail(
	xwsmessagestate* pState,
	xwsmessageerrorinfo* pError,
	xerrkind Kind,
	xwsmessageerror Code,
	uint16 iCloseCode,
	size_t iOffset,
	cstr sOperation,
	cstr sMessage,
	bool bPoison
)
{
	xwsmessagestate State;
	xwsmessageerrorinfo Error;

	if ( pError != NULL ) {
		Error.Code = Code;
		Error.CloseCode = iCloseCode;
		Error.Offset = iOffset;
		memcpy(pError, &Error, sizeof(Error));
	}
	if ( bPoison && (pState != NULL) ) {
		memcpy(&State, pState, sizeof(State));
		State.Failed = true;
		memcpy(pState, &State, sizeof(State));
	}
	__xrtWsMessageError(
		Kind,
		Code,
		sOperation,
		sMessage
	);
	return false;
}



/* 判断当前帧是否携带控制操作码。 */
static bool __xrtWsMessageControl(uint8 iOpcode)
{
	return (iOpcode == (uint8)XWS_OPCODE_CLOSE) ||
		(iOpcode == (uint8)XWS_OPCODE_PING) ||
		(iOpcode == (uint8)XWS_OPCODE_PONG);
}



/* 返回当前数据帧经过扩展解释时需要继承的全部 RSV 位。 */
static uint32 __xrtWsMessageRsv(
	const xwsmessagestate* pState
)
{
	if ( __xrtWsMessageControl(pState->FrameOpcode) ) {
		return pState->FrameRsv;
	}
	return pState->MessageRsv | pState->FrameRsv;
}



/* 清除当前帧的短期状态，不影响仍在进行的分片消息。 */
static void __xrtWsMessageFrameClear(
	xwsmessagestate* pState
)
{
	pState->FrameSize = 0;
	pState->FramePayloadSize = 0;
	pState->FrameRsv = 0;
	pState->FrameOpcode = 0;
	pState->CloseHead[0] = 0;
	pState->CloseHead[1] = 0;
	pState->CloseHeadSize = 0;
	pState->FrameActive = false;
	pState->FrameFinal = false;
	xrtUtf8StateInit(&pState->CloseUtf8);
}



/* 初始化默认消息配置。 */
XRT_API void xrtWsMessageConfigInit(xwsmessageconfig* pConfig)
{
	xwsmessageconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtWsMessageError(
			XERR_ARGUMENT,
			XWS_MESSAGE_ERROR_ARGUMENT,
			"config-init",
			"WebSocket message configuration range is invalid"
		);
		return;
	}
	__xrtWsMessageConfigDefault(&Config);
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化有明确消息预算的安全默认配置。 */
XRT_API void xrtWsMessageConfigInitSafe(xwsmessageconfig* pConfig)
{
	xrtWsMessageConfigInit(pConfig);
	if ( pConfig != NULL ) {
		pConfig->MaxSize = XWS_MESSAGE_SIZE_SAFE_DEFAULT;
	}
}



/* 绑定配置并初始化无资源消息状态。 */
XRT_API bool xrtWsMessageInit(
	xwsmessagestate* pState,
	const xwsmessageconfig* pConfig
)
{
	xwsmessageconfig Config;
	xwsmessagestate State;

	if ( !__xrtRangeValid(pState, sizeof(State)) ||
		((pConfig != NULL) &&
		 !__xrtRangeValid(pConfig, sizeof(Config))) ||
		((pConfig != NULL) && __xrtRangesOverlap(
			pState, sizeof(State), pConfig, sizeof(Config)
		)) ) {
		__xrtWsMessageError(
			XERR_ARGUMENT,
			XWS_MESSAGE_ERROR_ARGUMENT,
			"init",
			"WebSocket message init ranges are invalid or overlap"
		);
		return false;
	}
	if ( !__xrtWsMessageConfigRead(pConfig, &Config) ) {
		__xrtWsMessageError(
			XERR_ARGUMENT,
			XWS_MESSAGE_ERROR_ARGUMENT,
			"init",
			"WebSocket message configuration range is invalid"
		);
		return false;
	}
	if ( !__xrtWsMessageConfigValid(&Config) ) {
		__xrtWsMessageError(
			XERR_VALUE,
			XWS_MESSAGE_ERROR_CONFIG,
			"init",
			"invalid WebSocket message configuration"
		);
		return false;
	}

	memset(&State, 0, sizeof(State));
	State.Config = Config;
	State.Initialized = true;
	xrtUtf8StateInit(&State.Utf8);
	xrtUtf8StateInit(&State.CloseUtf8);
	memcpy(pState, &State, sizeof(State));
	return true;
}



/* 保留配置并把状态恢复到新连接起点。 */
XRT_API void xrtWsMessageReset(xwsmessagestate* pState)
{
	xwsmessageconfig Config;
	xwsmessagestate State;
	xwsmessagestate Next;

	if ( !__xrtRangeValid(pState, sizeof(State)) ) {
		__xrtWsMessageError(
			XERR_ARGUMENT,
			XWS_MESSAGE_ERROR_ARGUMENT,
			"reset",
			"WebSocket message state range is invalid"
		);
		return;
	}
	memcpy(&State, pState, sizeof(State));
	if ( !State.Initialized ||
		!__xrtWsMessageConfigValid(&State.Config) ) {
		__xrtWsMessageError(
			XERR_STATE,
			XWS_MESSAGE_ERROR_STATE,
			"reset",
			"WebSocket message state is not initialized"
		);
		return;
	}
	Config = State.Config;
	memset(&Next, 0, sizeof(Next));
	Next.Config = Config;
	Next.Initialized = true;
	xrtUtf8StateInit(&Next.Utf8);
	xrtUtf8StateInit(&Next.CloseUtf8);
	memcpy(pState, &Next, sizeof(Next));
}



/* 开始一个数据帧或可穿插的控制帧。 */
XRT_API bool xrtWsMessageFrameBegin(
	xwsmessagestate* pState,
	const xwsframe* pFrame,
	xwsmessageinfo* pInfo,
	xwsmessageerrorinfo* pError
)
{
	xwsmessagestate State;
	xwsmessagestate Next;
	xwsframe Frame;
	xwsmessageinfo Info;
	uint32 iRsv;
	uint16 iAllowedRsv;
	uint8 iOpcode;
	bool bControl;
	bool bNewData;

	if ( !__xrtRangeValid(pState, sizeof(State)) ||
		!__xrtRangeValid(pFrame, sizeof(Frame)) ||
		!__xrtRangeValid(pInfo, sizeof(Info)) ||
		((pError != NULL) &&
		 !__xrtRangeValid(pError, sizeof(*pError))) ||
		__xrtRangesOverlap(
			pState, sizeof(State), pFrame, sizeof(Frame)
		) ||
		__xrtRangesOverlap(
			pInfo,
			sizeof(Info),
			pState,
			sizeof(State)
		) ||
		__xrtRangesOverlap(
			pInfo,
			sizeof(Info),
			pFrame,
			sizeof(Frame)
		) ||
		((pError != NULL) && (
			__xrtRangesOverlap(
				pError,
				sizeof(*pError),
				pState,
				sizeof(State)
			) ||
			__xrtRangesOverlap(
				pError,
				sizeof(*pError),
				pFrame,
				sizeof(Frame)
			) ||
			__xrtRangesOverlap(
				pError,
				sizeof(*pError),
				pInfo,
				sizeof(Info)
			)
		)) ) {
		return __xrtWsMessageFail(
			pState,
			NULL,
			XERR_ARGUMENT,
			XWS_MESSAGE_ERROR_ARGUMENT,
			0,
			XRT_NPOS,
			"frame-begin",
			"invalid WebSocket message frame arguments",
			false
		);
	}
	memcpy(&State, pState, sizeof(State));
	memcpy(&Frame, pFrame, sizeof(Frame));
	if ( !State.Initialized ||
		!__xrtWsMessageConfigValid(&State.Config) ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_STATE,
			XWS_MESSAGE_ERROR_STATE,
			0,
			XRT_NPOS,
			"frame-begin",
			"WebSocket message state is not initialized",
			false
		);
	}
	if ( State.Failed || State.Closed ||
		State.FrameActive ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_STATE,
			XWS_MESSAGE_ERROR_STATE,
			0,
			State.Size,
			"frame-begin",
			"WebSocket message state cannot begin another frame",
			false
		);
	}
	if ( (Frame.Flags & ~__XRT_WS_MESSAGE_FRAME_FLAGS) != 0 ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_PROTOCOL,
			XWS_MESSAGE_ERROR_RSV,
			XWS_CLOSE_PROTOCOL,
			State.Size,
			"frame-begin",
			"WebSocket frame contains unknown flags",
			true
		);
	}

	iOpcode = Frame.Opcode;
	bControl = __xrtWsMessageControl(iOpcode);
	bNewData =
		(iOpcode == (uint8)XWS_OPCODE_TEXT) ||
		(iOpcode == (uint8)XWS_OPCODE_BINARY);
	if ( !bControl && !bNewData &&
		(iOpcode != (uint8)XWS_OPCODE_CONTINUATION) ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_PROTOCOL,
			XWS_MESSAGE_ERROR_OPCODE,
			XWS_CLOSE_PROTOCOL,
			State.Size,
			"frame-begin",
			"WebSocket frame contains an unsupported opcode",
			true
		);
	}
	if ( Frame.PayloadSize > XWS_FRAME_PAYLOAD_MAX ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_PROTOCOL,
			XWS_MESSAGE_ERROR_PAYLOAD,
			XWS_CLOSE_PROTOCOL,
			State.Size,
			"frame-begin",
			"WebSocket frame payload length exceeds the wire limit",
			true
		);
	}

	iRsv = Frame.Flags & __XRT_WS_MESSAGE_RSV;
	if ( bControl ) {
		iAllowedRsv = State.Config.ControlRsv;
		if ( ((Frame.Flags & XWS_FRAME_FIN) == 0) ||
			(Frame.PayloadSize > XWS_CLOSE_PAYLOAD_MAX) ||
			((iOpcode == (uint8)XWS_OPCODE_CLOSE) &&
			 (Frame.PayloadSize == 1u)) ) {
			return __xrtWsMessageFail(
				pState,
				pError,
				XERR_PROTOCOL,
				XWS_MESSAGE_ERROR_PAYLOAD,
				XWS_CLOSE_PROTOCOL,
				0,
				"frame-begin",
				"invalid WebSocket control frame",
				true
			);
		}
	} else if ( bNewData ) {
		iAllowedRsv = State.Config.FirstRsv;
		if ( State.Fragmented ) {
			return __xrtWsMessageFail(
				pState,
				pError,
				XERR_PROTOCOL,
				XWS_MESSAGE_ERROR_FRAGMENT,
				XWS_CLOSE_PROTOCOL,
				State.Size,
				"frame-begin",
				"new WebSocket data frame interrupts a fragmented message",
				true
			);
		}
	} else {
		iAllowedRsv = State.Config.ContinuationRsv;
		if ( !State.Fragmented ) {
			return __xrtWsMessageFail(
				pState,
				pError,
				XERR_PROTOCOL,
				XWS_MESSAGE_ERROR_FRAGMENT,
				XWS_CLOSE_PROTOCOL,
				0,
				"frame-begin",
				"WebSocket continuation frame has no fragmented message",
				true
			);
		}
	}
	if ( (iRsv & ~(uint32)iAllowedRsv) != 0 ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_PROTOCOL,
			XWS_MESSAGE_ERROR_RSV,
			XWS_CLOSE_PROTOCOL,
			State.Size,
			"frame-begin",
			"WebSocket frame uses an RSV bit outside its negotiated position",
			true
		);
	}

	Next = State;
	if ( bNewData ) {
		Next.Size = 0;
		Next.MessageRsv = iRsv;
		Next.Opcode = iOpcode;
		xrtUtf8StateInit(&Next.Utf8);
	}
	Next.FrameSize = 0;
	Next.FramePayloadSize = Frame.PayloadSize;
	Next.FrameRsv = iRsv;
	Next.FrameOpcode = iOpcode;
	Next.FrameActive = true;
	Next.FrameFinal =
		(Frame.Flags & XWS_FRAME_FIN) != 0;
	if ( iOpcode == (uint8)XWS_OPCODE_CLOSE ) {
		Next.CloseHead[0] = 0;
		Next.CloseHead[1] = 0;
		Next.CloseHeadSize = 0;
		xrtUtf8StateInit(&Next.CloseUtf8);
	}

	if ( !bControl &&
		(__xrtWsMessageRsv(&Next) == 0) &&
		(
			(Next.Size > Next.Config.MaxSize) ||
			(Frame.PayloadSize >
			 (uint64)(Next.Config.MaxSize - Next.Size))
		)
	) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_RANGE,
			XWS_MESSAGE_ERROR_SIZE,
			XWS_CLOSE_TOO_BIG,
			Next.Size,
			"frame-begin",
			"WebSocket message exceeds its configured size limit",
			true
		);
	}

	memset(&Info, 0, sizeof(Info));
	Info.Opcode = bControl ? iOpcode : Next.Opcode;
	Info.FrameOpcode = iOpcode;
	Info.PayloadSize = Frame.PayloadSize;
	Info.Offset = bControl ? 0 : Next.Size;
	Info.Rsv = (uint16)__xrtWsMessageRsv(&Next);
	if ( bControl || bNewData ) {
		Info.Flags |= XWS_MESSAGE_BEGIN;
	}
	if ( bControl || Next.FrameFinal ) {
		Info.Flags |= XWS_MESSAGE_END;
	}
	if ( bControl ) {
		Info.Flags |= XWS_MESSAGE_CONTROL;
	}
	if ( Info.Rsv != 0 ) {
		Info.Flags |= XWS_MESSAGE_EXTENDED;
	}

	memcpy(pState, &Next, sizeof(Next));
	memcpy(pInfo, &Info, sizeof(Info));
	__xrtWsMessageErrorClear(pError);
	return true;
}



/* 提交一个扩展解码后的语义负载分块。 */
XRT_API bool xrtWsMessagePayload(
	xwsmessagestate* pState,
	xbytesview Payload,
	xwsmessageerrorinfo* pError
)
{
	xwsmessagestate State;
	xwsmessagestate Next;
	xutfstatus Status;
	size_t iPosition = 0;
	size_t iError;
	bool bControl;

	if ( !__xrtRangeValid(pState, sizeof(State)) ||
		!__xrtWsMessageViewValid(Payload) ||
		__xrtRangesOverlap(
			pState, sizeof(State), Payload.Data, Payload.Size
		) ||
		((pError != NULL) && (
			!__xrtRangeValid(pError, sizeof(*pError)) ||
			__xrtRangesOverlap(
				pError,
				sizeof(*pError),
				pState,
				sizeof(State)
			) ||
			__xrtRangesOverlap(
				pError,
				sizeof(*pError),
				Payload.Data,
				Payload.Size
			)
		)) ) {
		return __xrtWsMessageFail(
			pState,
			NULL,
			XERR_ARGUMENT,
			XWS_MESSAGE_ERROR_ARGUMENT,
			0,
			XRT_NPOS,
			"payload",
			"invalid WebSocket message payload arguments",
			false
		);
	}
	memcpy(&State, pState, sizeof(State));
	if ( !State.Initialized || State.Failed ||
		State.Closed || !State.FrameActive ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_STATE,
			XWS_MESSAGE_ERROR_STATE,
			0,
			State.Size,
			"payload",
			"WebSocket message state has no writable frame",
			false
		);
	}
	if ( Payload.Size > (SIZE_MAX - State.FrameSize) ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_RANGE,
			XWS_MESSAGE_ERROR_PAYLOAD,
			0,
			State.FrameSize,
			"payload",
			"WebSocket frame payload accounting overflowed",
			true
		);
	}
	if ( (__xrtWsMessageRsv(&State) == 0) &&
		((uint64)(State.FrameSize + Payload.Size) >
		 State.FramePayloadSize) ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_STATE,
			XWS_MESSAGE_ERROR_PAYLOAD,
			0,
			State.FrameSize,
			"payload",
			"more semantic payload was supplied than the frame declares",
			true
		);
	}

	Next = State;
	bControl = __xrtWsMessageControl(Next.FrameOpcode);
	if ( bControl ) {
		if ( Next.FrameOpcode == (uint8)XWS_OPCODE_CLOSE ) {
			while ( (Next.CloseHeadSize < 2u) &&
				(iPosition < Payload.Size) ) {
				Next.CloseHead[Next.CloseHeadSize++] =
					Payload.Data[iPosition++];
			}
			if ( (Next.CloseHeadSize == 2u) &&
				(State.CloseHeadSize < 2u) ) {
				uint16 iCode = (uint16)(
					((uint16)Next.CloseHead[0] << 8u) |
					(uint16)Next.CloseHead[1]
				);

				if ( !xrtWsCloseCodeValid(iCode) ) {
					return __xrtWsMessageFail(
						pState,
						pError,
						XERR_PROTOCOL,
						XWS_MESSAGE_ERROR_CLOSE,
						XWS_CLOSE_PROTOCOL,
						0,
						"payload",
						"WebSocket Close payload contains a forbidden status code",
						true
					);
				}
			}
			if ( iPosition < Payload.Size ) {
				xstrview Reason;

				Reason.Data = (const char*)Payload.Data +
					iPosition;
				Reason.Size = Payload.Size - iPosition;
				Status = xrtUtf8StateFeed(
					&Next.CloseUtf8,
					Reason,
					false
				);
				if ( (Status != XUTF_OK) &&
					(Status != XUTF_MORE) ) {
					iError = xrtUtf8StateError(
						&Next.CloseUtf8
					);
					return __xrtWsMessageFail(
						pState,
						pError,
						XERR_PROTOCOL,
						XWS_MESSAGE_ERROR_UTF8,
						XWS_CLOSE_INVALID_DATA,
						iError == XRT_NPOS ?
							Next.FrameSize + iPosition :
							iError + 2u,
						"payload",
						"WebSocket Close reason is not valid UTF-8",
						true
					);
				}
			}
		}
	} else {
		if ( (Next.Size > Next.Config.MaxSize) ||
			(Payload.Size >
			 (Next.Config.MaxSize - Next.Size)) ) {
			return __xrtWsMessageFail(
				pState,
				pError,
				XERR_RANGE,
				XWS_MESSAGE_ERROR_SIZE,
				XWS_CLOSE_TOO_BIG,
				Next.Size,
				"payload",
				"WebSocket message exceeds its configured size limit",
				true
			);
		}
		if ( Next.Config.ValidateText &&
			(Next.Opcode == (uint8)XWS_OPCODE_TEXT) ) {
			xstrview Text;

			Text.Data = (const char*)Payload.Data;
			Text.Size = Payload.Size;
			Status = xrtUtf8StateFeed(
				&Next.Utf8,
				Text,
				false
			);
			if ( (Status != XUTF_OK) &&
				(Status != XUTF_MORE) ) {
				iError = xrtUtf8StateError(&Next.Utf8);
				return __xrtWsMessageFail(
					pState,
					pError,
					XERR_PROTOCOL,
					XWS_MESSAGE_ERROR_UTF8,
					XWS_CLOSE_INVALID_DATA,
					iError == XRT_NPOS ?
						Next.Size : iError,
					"payload",
					"WebSocket text message is not valid UTF-8",
					true
				);
			}
		}
		Next.Size += Payload.Size;
	}
	Next.FrameSize += Payload.Size;

	memcpy(pState, &Next, sizeof(Next));
	__xrtWsMessageErrorClear(pError);
	return true;
}



/* 完成当前帧并提交分片、UTF-8 或关闭状态。 */
XRT_API bool xrtWsMessageFrameEnd(
	xwsmessagestate* pState,
	xwsmessageerrorinfo* pError
)
{
	xwsmessagestate State;
	xwsmessagestate Next;
	xutfstatus Status;
	size_t iError;
	bool bControl;

	if ( !__xrtRangeValid(pState, sizeof(State)) ||
		((pError != NULL) && (
			!__xrtRangeValid(pError, sizeof(*pError)) ||
			__xrtRangesOverlap(
				pError,
				sizeof(*pError),
				pState,
				sizeof(State)
			)
		)) ) {
		return __xrtWsMessageFail(
			pState,
			NULL,
			XERR_ARGUMENT,
			XWS_MESSAGE_ERROR_ARGUMENT,
			0,
			XRT_NPOS,
			"frame-end",
			"invalid WebSocket message frame-end arguments",
			false
		);
	}
	memcpy(&State, pState, sizeof(State));
	if ( !State.Initialized || State.Failed ||
		State.Closed || !State.FrameActive ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_STATE,
			XWS_MESSAGE_ERROR_STATE,
			0,
			State.Size,
			"frame-end",
			"WebSocket message state has no frame to finish",
			false
		);
	}
	if ( (__xrtWsMessageRsv(&State) == 0) &&
		((uint64)State.FrameSize !=
		 State.FramePayloadSize) ) {
		return __xrtWsMessageFail(
			pState,
			pError,
			XERR_STATE,
			XWS_MESSAGE_ERROR_PAYLOAD,
			0,
			State.FrameSize,
			"frame-end",
			"semantic payload size does not match the frame",
			true
		);
	}

	Next = State;
	bControl = __xrtWsMessageControl(Next.FrameOpcode);
	if ( bControl ) {
		if ( Next.FrameOpcode == (uint8)XWS_OPCODE_CLOSE ) {
			if ( Next.FrameSize == 1u ) {
				return __xrtWsMessageFail(
					pState,
					pError,
					XERR_PROTOCOL,
					XWS_MESSAGE_ERROR_CLOSE,
					XWS_CLOSE_PROTOCOL,
					0,
					"frame-end",
					"WebSocket Close payload cannot contain one byte",
					true
				);
			}
			if ( Next.FrameSize >= 2u ) {
				xstrview Empty = { NULL, 0 };

				Status = xrtUtf8StateFeed(
					&Next.CloseUtf8,
					Empty,
					true
				);
				if ( Status != XUTF_OK ) {
					iError = xrtUtf8StateError(
						&Next.CloseUtf8
					);
					return __xrtWsMessageFail(
						pState,
						pError,
						XERR_PROTOCOL,
						XWS_MESSAGE_ERROR_UTF8,
						XWS_CLOSE_INVALID_DATA,
						iError == XRT_NPOS ?
							Next.FrameSize :
							iError + 2u,
						"frame-end",
						"WebSocket Close reason ends with incomplete UTF-8",
						true
					);
				}
			}
			Next.Closed = true;
		}
		__xrtWsMessageFrameClear(&Next);
	} else {
		if ( Next.FrameFinal &&
			Next.Config.ValidateText &&
			(Next.Opcode == (uint8)XWS_OPCODE_TEXT) ) {
			xstrview Empty = { NULL, 0 };

			Status = xrtUtf8StateFeed(
				&Next.Utf8,
				Empty,
				true
			);
			if ( Status != XUTF_OK ) {
				iError = xrtUtf8StateError(&Next.Utf8);
				return __xrtWsMessageFail(
					pState,
					pError,
					XERR_PROTOCOL,
					XWS_MESSAGE_ERROR_UTF8,
					XWS_CLOSE_INVALID_DATA,
					iError == XRT_NPOS ?
						Next.Size : iError,
					"frame-end",
					"WebSocket text message ends with incomplete UTF-8",
					true
				);
			}
		}

		if ( Next.FrameFinal ) {
			Next.Size = 0;
			Next.MessageRsv = 0;
			Next.Opcode = 0;
			Next.Fragmented = false;
			xrtUtf8StateInit(&Next.Utf8);
		} else {
			Next.Fragmented = true;
		}
		__xrtWsMessageFrameClear(&Next);
	}

	memcpy(pState, &Next, sizeof(Next));
	__xrtWsMessageErrorClear(pError);
	return true;
}

#endif
