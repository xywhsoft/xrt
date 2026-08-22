#include "../internal/xrt_websocket.h"



#if defined(XRT_FEATURE_WEBSOCKET_CLOSE)

/* 判断借用视图是否具有完整且不回绕的内存范围。 */
static bool __xrtWsCloseViewValid(const void* pData, size_t iSize)
{
	return __xrtRangeValid(pData, iSize);
}



/* 纯判断状态码当前是否允许出现在 Close 控制帧中。 */
XRT_API bool xrtWsCloseCodeValid(uint16 iCode)
{
	if ( (iCode >= UINT16_C(3000)) &&
		(iCode <= UINT16_C(4999)) ) {
		return true;
	}
	return ((iCode >= UINT16_C(1000)) &&
			(iCode <= UINT16_C(1003))) ||
		((iCode >= UINT16_C(1007)) &&
		 (iCode <= UINT16_C(1014)));
}



/* 解析完整 Close 控制帧负载并借用其中的原因文本。 */
XRT_API bool xrtWsCloseParse(
	xbytesview Payload,
	xwsclose* pClose
)
{
	xwsclose Close;
	size_t iError;

	if ( !__xrtRangeValid(pClose, sizeof(Close)) ||
		!__xrtWsCloseViewValid(Payload.Data, Payload.Size) ||
		__xrtRangesOverlap(
			pClose,
			sizeof(Close),
			Payload.Data,
			Payload.Size
		) ) {
		__xrtWsCloseError(
			XERR_ARGUMENT,
			XWS_CLOSE_ERROR_ARGUMENT,
			"parse",
			"invalid WebSocket Close parse arguments"
		);
		return false;
	}
	if ( Payload.Size > XWS_CLOSE_PAYLOAD_MAX ) {
		__xrtWsCloseError(
			XERR_RANGE,
			XWS_CLOSE_ERROR_SIZE,
			"parse",
			"WebSocket Close payload exceeds 125 bytes"
		);
		return false;
	}
	if ( Payload.Size == 1u ) {
		__xrtWsCloseError(
			XERR_PROTOCOL,
			XWS_CLOSE_ERROR_SIZE,
			"parse",
			"WebSocket Close payload cannot contain one byte"
		);
		return false;
	}

	memset(&Close, 0, sizeof(Close));
	if ( Payload.Size == 0 ) {
		memcpy(pClose, &Close, sizeof(Close));
		return true;
	}

	Close.Code = (uint16)(
		((uint16)Payload.Data[0] << 8u) |
		(uint16)Payload.Data[1]
	);
	if ( !xrtWsCloseCodeValid(Close.Code) ) {
		__xrtWsCloseError(
			XERR_PROTOCOL,
			XWS_CLOSE_ERROR_CODE,
			"parse",
			"WebSocket Close payload contains a forbidden status code"
		);
		return false;
	}
	Close.Reason.Data = (const char*)Payload.Data + 2u;
	Close.Reason.Size = Payload.Size - 2u;
	if ( !xrtUtf8Valid(Close.Reason, &iError) ) {
		__xrtWsCloseError(
			XERR_PROTOCOL,
			XWS_CLOSE_ERROR_UTF8,
			"parse",
			"WebSocket Close reason is not valid UTF-8"
		);
		return false;
	}

	memcpy(pClose, &Close, sizeof(Close));
	return true;
}



/* 原子写出完整 Close 控制帧负载。 */
XRT_API bool xrtWsCloseWrite(
	uint16 iCode,
	xstrview Reason,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	size_t iRequired;
	size_t iError;

	if ( !__xrtRangeValid(pSize, sizeof(*pSize)) ||
		!__xrtWsCloseViewValid(Reason.Data, Reason.Size) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		__xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			Reason.Data,
			Reason.Size
		) ||
		((pOutput != NULL) && __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			pOutput,
			iCapacity
		)) ) {
		__xrtWsCloseError(
			XERR_ARGUMENT,
			XWS_CLOSE_ERROR_ARGUMENT,
			"write",
			"invalid WebSocket Close write arguments"
		);
		return false;
	}
	if ( iCode == 0 ) {
		if ( Reason.Size != 0 ) {
			__xrtWsCloseError(
				XERR_VALUE,
				XWS_CLOSE_ERROR_CODE,
				"write",
				"a WebSocket Close reason requires a status code"
			);
			return false;
		}
		iRequired = 0;
	} else {
		if ( !xrtWsCloseCodeValid(iCode) ) {
			__xrtWsCloseError(
				XERR_VALUE,
				XWS_CLOSE_ERROR_CODE,
				"write",
				"WebSocket Close status code is forbidden on the wire"
			);
			return false;
		}
		if ( Reason.Size > XWS_CLOSE_REASON_MAX ) {
			__xrtWsCloseError(
				XERR_RANGE,
				XWS_CLOSE_ERROR_SIZE,
				"write",
				"WebSocket Close reason exceeds 123 bytes"
			);
			return false;
		}
		if ( !xrtUtf8Valid(Reason, &iError) ) {
			__xrtWsCloseError(
				XERR_VALUE,
				XWS_CLOSE_ERROR_UTF8,
				"write",
				"WebSocket Close reason is not valid UTF-8"
			);
			return false;
		}

		iRequired = Reason.Size + 2u;
		Payload[0] = (uint8)(iCode >> 8u);
		Payload[1] = (uint8)iCode;
		if ( Reason.Size != 0 ) {
			memcpy(Payload + 2u, Reason.Data, Reason.Size);
		}
	}

	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		__xrtWsCloseError(
			XERR_RANGE,
			XWS_CLOSE_ERROR_OUTPUT,
			"write",
			"WebSocket Close output capacity is too small"
		);
		return false;
	}
	if ( (pOutput != NULL) && (iRequired != 0) ) {
		memcpy(pOutput, Payload, iRequired);
	}
	return true;
}

#endif
