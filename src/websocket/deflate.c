#include "../internal/xrt_websocket.h"



#if defined(XRT_FEATURE_WEBSOCKET_DEFLATE)

#define __XRT_WS_DEFLATE_FLAGS \
	(XWS_DEFLATE_SERVER_NO_CONTEXT | \
	 XWS_DEFLATE_CLIENT_NO_CONTEXT | \
	 XWS_DEFLATE_SERVER_MAX_WINDOW | \
	 XWS_DEFLATE_CLIENT_MAX_WINDOW | \
	 XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY)



/* 判断窗口位数是否属于 RFC 7692 的闭区间。 */
static bool __xrtWsDeflateWindowValid(uint8 iBits)
{
	return (iBits >= XWS_DEFLATE_WINDOW_MIN) &&
		(iBits <= XWS_DEFLATE_WINDOW_MAX);
}



/* 验证已经对齐的配置值满足 offer 或 response 的稳定结构不变量。 */
static bool __xrtWsDeflateValueValid(
	const xwsdeflate* pConfig,
	bool bOffer
)
{
	uint32 iFlags;

	iFlags = pConfig->Flags;
	if ( (iFlags & ~__XRT_WS_DEFLATE_FLAGS) != 0 ) {
		return false;
	}
	if ( ((iFlags & XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY) != 0) &&
		(((iFlags & XWS_DEFLATE_CLIENT_MAX_WINDOW) == 0) ||
		 !bOffer) ) {
		return false;
	}
	if ( (iFlags & XWS_DEFLATE_SERVER_MAX_WINDOW) != 0 ) {
		if ( !__xrtWsDeflateWindowValid(
			pConfig->ServerMaxWindowBits
		) ) {
			return false;
		}
	} else if (
		pConfig->ServerMaxWindowBits !=
		XWS_DEFLATE_WINDOW_MAX
	) {
		return false;
	}
	if ( (iFlags & XWS_DEFLATE_CLIENT_MAX_WINDOW) != 0 ) {
		if ( ((iFlags & XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY) == 0) &&
			!__xrtWsDeflateWindowValid(
				pConfig->ClientMaxWindowBits
			) ) {
			return false;
		}
		if ( ((iFlags & XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY) != 0) &&
			(pConfig->ClientMaxWindowBits !=
			 XWS_DEFLATE_WINDOW_MAX) ) {
			return false;
		}
	} else if (
		pConfig->ClientMaxWindowBits !=
		XWS_DEFLATE_WINDOW_MAX
	) {
		return false;
	}
	return true;
}



/* 从可能未对齐的完整地址范围读取并验证配置快照。 */
static bool __xrtWsDeflateConfigRead(
	const xwsdeflate* pSource,
	bool bOffer,
	xwsdeflate* pConfig
)
{
	if ( !__xrtRangeValid(pSource, sizeof(*pSource)) ) {
		return false;
	}
	memcpy(pConfig, pSource, sizeof(*pConfig));
	return __xrtWsDeflateValueValid(pConfig, bOffer);
}



/* 从保留转义的参数语义值读取规范十进制窗口位数。 */
static bool __xrtWsDeflateWindowRead(
	const xhttpparam* pParam,
	uint8* pBits
)
{
	uint8 Digits[2];
	size_t iDigits = 0;
	size_t i;

	if ( (pParam == NULL) || (pBits == NULL) ||
		!xrtHttpParamTokenValid(pParam) ) {
		return false;
	}
	for ( i = 0; i < pParam->Value.Size; i++ ) {
		uint8 iByte = (uint8)pParam->Value.Data[i];

		if ( ((pParam->Flags & XHTTP_PARAM_QUOTED) != 0) &&
			(iByte == (uint8)'\\') ) {
			iByte = (uint8)pParam->Value.Data[++i];
		}
		if ( (iDigits == sizeof(Digits)) ||
			(iByte < (uint8)'0') ||
			(iByte > (uint8)'9') ) {
			return false;
		}
		Digits[iDigits++] = iByte;
	}
	if ( iDigits == 1u ) {
		if ( (Digits[0] != (uint8)'8') &&
			(Digits[0] != (uint8)'9') ) {
			return false;
		}
		*pBits = Digits[0] - (uint8)'0';
		return true;
	}
	if ( (iDigits != 2u) ||
		(Digits[0] != (uint8)'1') ||
		(Digits[1] < (uint8)'0') ||
		(Digits[1] > (uint8)'5') ) {
		return false;
	}
	*pBits = UINT8_C(10) + Digits[1] - (uint8)'0';
	return true;
}



/* 为已经识别的参数设置唯一标志。 */
static bool __xrtWsDeflateFlagSet(
	uint32* pFlags,
	uint32 iFlag
)
{
	if ( (*pFlags & iFlag) != 0 ) {
		__xrtWsDeflateError(
			XERR_PROTOCOL,
			XWS_DEFLATE_ERROR_DUPLICATE,
			"parse",
			"duplicate permessage-deflate parameter"
		);
		return false;
	}
	*pFlags |= iFlag;
	return true;
}



/* 解析 offer 与 response 共用的四个 RFC 7692 参数。 */
static bool __xrtWsDeflateParse(
	const xwsextension* pExtension,
	xwsdeflate* pConfig,
	bool bOffer
)
{
	xwsextension Extension;
	xwsdeflate Config;
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset = 0;

	if ( !__xrtRangeValid(pExtension, sizeof(*pExtension)) ||
		!__xrtRangeValid(pConfig, sizeof(*pConfig)) ||
		__xrtRangesOverlap(
			pConfig, sizeof(*pConfig),
			pExtension, sizeof(*pExtension)
		) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			bOffer ? "offer-parse" : "response-parse",
			"invalid permessage-deflate parse arguments"
		);
		return false;
	}
	memcpy(&Extension, pExtension, sizeof(Extension));
	if ( !__xrtRangeValid(
		Extension.Name.Data,
		Extension.Name.Size
	) || !__xrtRangeValid(
		Extension.Parameters.Data,
		Extension.Parameters.Size
	) || __xrtRangesOverlap(
		pConfig, sizeof(*pConfig),
		Extension.Name.Data, Extension.Name.Size
	) || __xrtRangesOverlap(
		pConfig, sizeof(*pConfig),
		Extension.Parameters.Data, Extension.Parameters.Size
	) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			bOffer ? "offer-parse" : "response-parse",
			"invalid permessage-deflate borrowed ranges"
		);
		return false;
	}
	if ( !xrtWsDeflateIs(&Extension) ) {
		__xrtWsDeflateError(
			XERR_PROTOCOL,
			XWS_DEFLATE_ERROR_EXTENSION,
			bOffer ? "offer-parse" : "response-parse",
			"extension is not permessage-deflate"
		);
		return false;
	}
	xrtWsDeflateInit(&Config);
	for ( ;; ) {
		Next = xrtWsExtensionParamNext(
			&Extension,
			&iOffset,
			&Param
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			__xrtWsDeflateWrap(
				XERR_PROTOCOL,
				XWS_DEFLATE_ERROR_PARAMETER,
				bOffer ? "offer-parse" : "response-parse",
				"invalid permessage-deflate parameter"
			);
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pConfig, &Config, sizeof(Config));
			return true;
		}

		if ( xrtHttpTokenEqual(
			Param.Name,
			XRT_STR_LITERAL("server_no_context_takeover")
		) ) {
			if ( (Param.Flags != XHTTP_PARAM_NONE) ||
				!__xrtWsDeflateFlagSet(
					&Config.Flags,
					XWS_DEFLATE_SERVER_NO_CONTEXT
				) ) {
				if ( Param.Flags != XHTTP_PARAM_NONE ) {
					__xrtWsDeflateError(
						XERR_PROTOCOL,
						XWS_DEFLATE_ERROR_PARAMETER,
						bOffer ? "offer-parse" : "response-parse",
						"server_no_context_takeover must not have a value"
					);
				}
				return false;
			}
		} else if ( xrtHttpTokenEqual(
			Param.Name,
			XRT_STR_LITERAL("client_no_context_takeover")
		) ) {
			if ( (Param.Flags != XHTTP_PARAM_NONE) ||
				!__xrtWsDeflateFlagSet(
					&Config.Flags,
					XWS_DEFLATE_CLIENT_NO_CONTEXT
				) ) {
				if ( Param.Flags != XHTTP_PARAM_NONE ) {
					__xrtWsDeflateError(
						XERR_PROTOCOL,
						XWS_DEFLATE_ERROR_PARAMETER,
						bOffer ? "offer-parse" : "response-parse",
						"client_no_context_takeover must not have a value"
					);
				}
				return false;
			}
		} else if ( xrtHttpTokenEqual(
			Param.Name,
			XRT_STR_LITERAL("server_max_window_bits")
		) ) {
			if ( !__xrtWsDeflateFlagSet(
				&Config.Flags,
				XWS_DEFLATE_SERVER_MAX_WINDOW
			) ) {
				return false;
			}
			if ( !__xrtWsDeflateWindowRead(
				&Param,
				&Config.ServerMaxWindowBits
			) ) {
				__xrtWsDeflateError(
					XERR_PROTOCOL,
					XWS_DEFLATE_ERROR_WINDOW,
					bOffer ? "offer-parse" : "response-parse",
					"invalid server_max_window_bits value"
				);
				return false;
			}
		} else if ( xrtHttpTokenEqual(
			Param.Name,
			XRT_STR_LITERAL("client_max_window_bits")
		) ) {
			if ( !__xrtWsDeflateFlagSet(
				&Config.Flags,
				XWS_DEFLATE_CLIENT_MAX_WINDOW
			) ) {
				return false;
			}
			if ( Param.Flags == XHTTP_PARAM_NONE ) {
				if ( !bOffer ) {
					__xrtWsDeflateError(
						XERR_PROTOCOL,
						XWS_DEFLATE_ERROR_WINDOW,
						"response-parse",
						"client_max_window_bits response requires a value"
					);
					return false;
				}
				Config.Flags |=
					XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY;
			} else if ( !__xrtWsDeflateWindowRead(
				&Param,
				&Config.ClientMaxWindowBits
			) ) {
				__xrtWsDeflateError(
					XERR_PROTOCOL,
					XWS_DEFLATE_ERROR_WINDOW,
					bOffer ? "offer-parse" : "response-parse",
					"invalid client_max_window_bits value"
				);
				return false;
			}
		} else {
			__xrtWsDeflateError(
				XERR_PROTOCOL,
				XWS_DEFLATE_ERROR_PARAMETER,
				bOffer ? "offer-parse" : "response-parse",
				"unknown permessage-deflate parameter"
			);
			return false;
		}
	}
}



/* 在固定缓冲区末尾追加字面量。 */
static void __xrtWsDeflateTextAppend(
	char* sOutput,
	size_t* pOffset,
	cstr sText
)
{
	size_t iSize = strlen(sText);

	memcpy(sOutput + *pOffset, sText, iSize);
	*pOffset += iSize;
}



/* 在固定缓冲区末尾追加 8 到 15 的窗口位数。 */
static void __xrtWsDeflateWindowAppend(
	char* sOutput,
	size_t* pOffset,
	uint8 iBits
)
{
	if ( iBits >= 10u ) {
		sOutput[(*pOffset)++] = '1';
		sOutput[(*pOffset)++] =
			(char)('0' + (iBits - 10u));
	} else {
		sOutput[(*pOffset)++] = (char)('0' + iBits);
	}
}



/* 规范写出 offer 或 response。 */
static bool __xrtWsDeflateWrite(
	const xwsdeflate* pConfig,
	bool bOffer,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xwsdeflate Config;
	char Text[XWS_DEFLATE_MAX_SIZE];
	size_t iCheckSize;
	size_t iSize = 0;

	if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ||
		!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pConfig, sizeof(*pConfig)
		) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			bOffer ? "offer-write" : "response-write",
			"invalid permessage-deflate write arguments"
		);
		return false;
	}
	if ( !__xrtWsDeflateConfigRead(
		pConfig,
		bOffer,
		&Config
	) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			bOffer ? "offer-write" : "response-write",
			"invalid permessage-deflate configuration"
		);
		return false;
	}
	__xrtWsDeflateTextAppend(
		Text,
		&iSize,
		XWS_DEFLATE_NAME
	);
	if ( (Config.Flags &
		XWS_DEFLATE_SERVER_NO_CONTEXT) != 0 ) {
		__xrtWsDeflateTextAppend(
			Text,
			&iSize,
			"; server_no_context_takeover"
		);
	}
	if ( (Config.Flags &
		XWS_DEFLATE_CLIENT_NO_CONTEXT) != 0 ) {
		__xrtWsDeflateTextAppend(
			Text,
			&iSize,
			"; client_no_context_takeover"
		);
	}
	if ( (Config.Flags &
		XWS_DEFLATE_SERVER_MAX_WINDOW) != 0 ) {
		__xrtWsDeflateTextAppend(
			Text,
			&iSize,
			"; server_max_window_bits="
		);
		__xrtWsDeflateWindowAppend(
			Text,
			&iSize,
			Config.ServerMaxWindowBits
		);
	}
	if ( (Config.Flags &
		XWS_DEFLATE_CLIENT_MAX_WINDOW) != 0 ) {
		__xrtWsDeflateTextAppend(
			Text,
			&iSize,
			"; client_max_window_bits"
		);
		if ( (Config.Flags &
			XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY) == 0 ) {
			Text[iSize++] = '=';
			__xrtWsDeflateWindowAppend(
				Text,
				&iSize,
				Config.ClientMaxWindowBits
			);
		}
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iSize, sizeof(iSize));
		return true;
	}
	iCheckSize = __xrtOutputCheckSize(iSize, iCapacity);
	if ( !__xrtRangeValid(pOutput, iCheckSize) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pOutput, iCheckSize
		) || __xrtRangesOverlap(
			pConfig, sizeof(*pConfig),
			pOutput, iCheckSize
		) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			bOffer ? "offer-write" : "response-write",
			"permessage-deflate output overlaps its inputs"
		);
		return false;
	}
	if ( iCapacity < iSize ) {
		memcpy(pSize, &iSize, sizeof(iSize));
		__xrtWsDeflateError(
			XERR_RANGE,
			XWS_DEFLATE_ERROR_OUTPUT,
			bOffer ? "offer-write" : "response-write",
			"permessage-deflate output is too small"
		);
		return false;
	}
	memcpy(pOutput, Text, iSize);
	memcpy(pSize, &iSize, sizeof(iSize));
	return true;
}



/* 初始化默认 permessage-deflate 配置。 */
XRT_API void xrtWsDeflateInit(xwsdeflate* pConfig)
{
	xwsdeflate Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"config-init",
			"invalid permessage-deflate configuration range"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.ServerMaxWindowBits = XWS_DEFLATE_WINDOW_MAX;
	Config.ClientMaxWindowBits = XWS_DEFLATE_WINDOW_MAX;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 判断扩展是否是 permessage-deflate。 */
XRT_API bool xrtWsDeflateIs(const xwsextension* pExtension)
{
	xwsextension Extension;

	if ( !__xrtRangeValid(pExtension, sizeof(Extension)) ) {
		return false;
	}
	memcpy(&Extension, pExtension, sizeof(Extension));
	if ( !__xrtRangeValid(
		Extension.Name.Data,
		Extension.Name.Size
	) ) {
		return false;
	}
	return xrtHttpTokenEqual(
		Extension.Name,
		XRT_STR_LITERAL(XWS_DEFLATE_NAME)
	);
}



/* 解析 permessage-deflate offer。 */
XRT_API bool xrtWsDeflateOfferParse(
	const xwsextension* pExtension,
	xwsdeflate* pOffer
)
{
	return __xrtWsDeflateParse(
		pExtension,
		pOffer,
		true
	);
}



/* 解析 permessage-deflate response。 */
XRT_API bool xrtWsDeflateResponseParse(
	const xwsextension* pExtension,
	xwsdeflate* pResponse
)
{
	return __xrtWsDeflateParse(
		pExtension,
		pResponse,
		false
	);
}



/* 构造确认 offer 强制约束的最小响应。 */
XRT_API bool xrtWsDeflateAccept(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse
)
{
	xwsdeflate Offer;
	xwsdeflate Response;

	if ( !__xrtRangeValid(pOffer, sizeof(*pOffer)) ||
		!__xrtRangeValid(pResponse, sizeof(*pResponse)) ||
		((pOffer != pResponse) &&
		 __xrtRangesOverlap(
			pOffer, sizeof(*pOffer),
			pResponse, sizeof(*pResponse)
		 )) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"accept",
			"invalid permessage-deflate offer or response output"
		);
		return false;
	}
	if ( !__xrtWsDeflateConfigRead(pOffer, true, &Offer) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"accept",
			"invalid permessage-deflate offer"
		);
		return false;
	}
	xrtWsDeflateInit(&Response);
	Response.Flags = Offer.Flags & (
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_SERVER_MAX_WINDOW
	);
	if ( (Response.Flags &
		XWS_DEFLATE_SERVER_MAX_WINDOW) != 0 ) {
		Response.ServerMaxWindowBits =
			Offer.ServerMaxWindowBits;
	}
	memcpy(pResponse, &Response, sizeof(Response));
	return true;
}



/* 检查 response 是否满足 offer 的强制约束和参数方向规则。 */
XRT_API bool xrtWsDeflateResponseCheck(
	const xwsdeflate* pOffer,
	const xwsdeflate* pResponse
)
{
	xwsdeflate Offer;
	xwsdeflate Response;
	uint32 iOffer;
	uint32 iResponse;

	if ( !__xrtWsDeflateConfigRead(pOffer, true, &Offer) ||
		!__xrtWsDeflateConfigRead(pResponse, false, &Response) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"response-check",
			"invalid permessage-deflate offer or response"
		);
		return false;
	}
	iOffer = Offer.Flags;
	iResponse = Response.Flags;

	if ( ((iOffer & XWS_DEFLATE_SERVER_NO_CONTEXT) != 0) &&
		((iResponse & XWS_DEFLATE_SERVER_NO_CONTEXT) == 0) ) {
		__xrtWsDeflateError(
			XERR_PROTOCOL,
			XWS_DEFLATE_ERROR_RESPONSE,
			"response-check",
			"response omitted required server_no_context_takeover"
		);
		return false;
	}
	if ( (iOffer & XWS_DEFLATE_SERVER_MAX_WINDOW) != 0 ) {
		if ( ((iResponse &
			XWS_DEFLATE_SERVER_MAX_WINDOW) == 0) ||
			(Response.ServerMaxWindowBits >
			 Offer.ServerMaxWindowBits) ) {
			__xrtWsDeflateError(
				XERR_PROTOCOL,
				XWS_DEFLATE_ERROR_RESPONSE,
				"response-check",
				"response violates server_max_window_bits"
			);
			return false;
		}
	}
	if ( ((iResponse & XWS_DEFLATE_CLIENT_MAX_WINDOW) != 0) &&
		((iOffer & XWS_DEFLATE_CLIENT_MAX_WINDOW) == 0) ) {
		__xrtWsDeflateError(
			XERR_PROTOCOL,
			XWS_DEFLATE_ERROR_RESPONSE,
			"response-check",
			"response used unoffered client_max_window_bits"
		);
		return false;
	}
	return true;
}



/* 规范写出 permessage-deflate offer。 */
XRT_API bool xrtWsDeflateOfferWrite(
	const xwsdeflate* pOffer,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtWsDeflateWrite(
		pOffer,
		true,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 规范写出 permessage-deflate response。 */
XRT_API bool xrtWsDeflateResponseWrite(
	const xwsdeflate* pResponse,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtWsDeflateWrite(
		pResponse,
		false,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 把协商响应中的客户端/服务端参数映射到本地收发方向。 */
XRT_API bool xrtWsDeflateDirection(
	const xwsdeflate* pResponse,
	xwsrole Role,
	bool bSend,
	xwsdeflatedirection* pDirection
)
{
	xwsdeflate Response;
	xwsdeflatedirection Direction;
	bool bServer;

	if ( !__xrtRangeValid(pResponse, sizeof(*pResponse)) ||
		!__xrtRangeValid(pDirection, sizeof(*pDirection)) ||
		((Role != XWS_ROLE_CLIENT) &&
		 (Role != XWS_ROLE_SERVER)) ||
		__xrtRangesOverlap(
			pResponse,
			sizeof(*pResponse),
			pDirection,
			sizeof(*pDirection)
		) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"direction",
			"invalid permessage-deflate direction arguments"
		);
		return false;
	}
	if ( !__xrtWsDeflateConfigRead(
		pResponse,
		false,
		&Response
	) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"direction",
			"invalid permessage-deflate response"
		);
		return false;
	}

	bServer = ((Role == XWS_ROLE_SERVER) == bSend);
	if ( bServer ) {
		Direction.WindowBits =
			Response.ServerMaxWindowBits;
		Direction.NoContextTakeover =
			(Response.Flags &
			 XWS_DEFLATE_SERVER_NO_CONTEXT) != 0;
	} else {
		Direction.WindowBits =
			Response.ClientMaxWindowBits;
		Direction.NoContextTakeover =
			(Response.Flags &
			 XWS_DEFLATE_CLIENT_NO_CONTEXT) != 0;
	}
	memcpy(pDirection, &Direction, sizeof(Direction));
	return true;
}

#endif
