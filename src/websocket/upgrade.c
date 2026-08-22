#include "../internal/xrt_websocket_upgrade.h"



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE)

/* 发布 Upgrade 层稳定的握手错误。 */
static bool __xrtWsUpgradeError(
	xerrkind Kind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtWsHandshakeError(Kind, Code, sOperation, sMessage);
	return false;
}



/* 验证 Header 借用视图、字段描述符和已解析状态。 */
static bool __xrtWsUpgradeHeadRead(
	const xhttp1head* pInput,
	xhttpkind Kind,
	xhttp1head* pHead,
	cstr sOperation
)
{
	xhttp1head Head;

	if ( !__xrtRangeValid(pInput, sizeof(Head)) ) {
		return __xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			sOperation,
			"WebSocket HTTP/1 Head range is invalid"
		);
	}
	memcpy(&Head, pInput, sizeof(Head));
	if ( (Head.Kind != Kind) ||
		(Head.FieldCount > Head.FieldCapacity) ||
		((Head.Fields == NULL) && (Head.FieldCount != 0)) ||
		(Head.FieldCount > (SIZE_MAX / sizeof(xhttpfield))) ||
		!__xrtRangeValid(
			Head.Fields,
			Head.FieldCount * sizeof(xhttpfield)
		) || !__xrtRangeValid(Head.Method.Data, Head.Method.Size) ||
		!__xrtRangeValid(Head.Target.Data, Head.Target.Size) ||
		!__xrtRangeValid(Head.Reason.Data, Head.Reason.Size) ) {
		return __xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			sOperation,
			"WebSocket HTTP/1 Head is incomplete or has the wrong kind"
		);
	}
	for ( size_t i = 0; i < Head.FieldCount; i++ ) {
		xhttpfield Field;

		__xrtWsUpgradeFieldRead(Head.Fields, i, &Field);
		if ( !__xrtRangeValid(Field.Name.Data, Field.Name.Size) ||
			!__xrtRangeValid(Field.Value.Data, Field.Value.Size) ||
			!xrtHttpTokenValid(Field.Name) ||
			!xrtHttpFieldValueValid(Field.Value) ) {
			return __xrtWsUpgradeError(
				XERR_ARGUMENT,
				XWS_HANDSHAKE_ERROR_FIELD,
				sOperation,
				"WebSocket HTTP/1 Head contains an invalid field"
			);
		}
	}
	*pHead = Head;
	return true;
}



/* 拒绝会让借用握手结果失效的输出覆盖。 */
static bool __xrtWsUpgradeOutputSeparate(
	const xhttp1head* pHead,
	const void* pOutput,
	size_t iOutput,
	cstr sOperation
)
{
	if ( __xrtRangesOverlap(
		pOutput,
		iOutput,
		pHead->Fields,
		pHead->FieldCount * sizeof(xhttpfield)
	) || __xrtRangesOverlap(
		pOutput,
		iOutput,
		pHead->Method.Data,
		pHead->Method.Size
	) || __xrtRangesOverlap(
		pOutput,
		iOutput,
		pHead->Target.Data,
		pHead->Target.Size
	) || __xrtRangesOverlap(
		pOutput,
		iOutput,
		pHead->Reason.Data,
		pHead->Reason.Size
	) ) {
		return __xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			sOperation,
			"WebSocket Upgrade output overlaps HTTP input"
		);
	}
	for ( size_t i = 0; i < pHead->FieldCount; i++ ) {
		xhttpfield Field;

		__xrtWsUpgradeFieldRead(pHead->Fields, i, &Field);
		if ( __xrtRangesOverlap(
			pOutput,
			iOutput,
			Field.Name.Data,
			Field.Name.Size
		) || __xrtRangesOverlap(
			pOutput,
			iOutput,
			Field.Value.Data,
			Field.Value.Size
		) ) {
			return __xrtWsUpgradeError(
				XERR_ARGUMENT,
				XWS_HANDSHAKE_ERROR_ARGUMENT,
				sOperation,
				"WebSocket Upgrade output overlaps a Header field"
			);
		}
	}
	return true;
}



/* 读取唯一字段，缺失和重复都返回空指针。 */
static const xhttpfield* __xrtWsUpgradeFieldUnique(
	const xhttp1head* pHead,
	xstrview Name
)
{
	const xhttpfield* pField = NULL;

	if ( xrtHttpFieldGetUnique(
		pHead->Fields,
		pHead->FieldCount,
		Name,
		&pField
	) != XHTTP_NEXT_ITEM ) {
		return NULL;
	}
	return pField;
}



/* 验证唯一字段只包含指定 token。 */
static bool __xrtWsUpgradeTokenExact(
	const xhttp1head* pHead,
	xstrview Name,
	xstrview Token
)
{
	const xhttpfield* pSource =
		__xrtWsUpgradeFieldUnique(pHead, Name);
	xhttpfield Field;
	size_t iCount;

	if ( pSource == NULL ) {
		return false;
	}
	memcpy(&Field, pSource, sizeof(Field));
	return xrtHttpTokenListCount(Field.Value, &iCount) &&
		(iCount == 1u) &&
		xrtHttpTokenListHas(Field.Value, Token);
}



/* 请求允许其它 Upgrade 协议，但全部字段必须合法且至少包含 websocket。 */
static bool __xrtWsUpgradeRequestProtocol(
	const xhttp1head* pHead
)
{
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Item;
	xhttpnext Next;
	bool bFound = false;

	xrtHttpUpgradeFieldCursorInit(&Cursor);
	while ( (Next = xrtHttpUpgradeFieldNext(
		pHead->Fields,
		pHead->FieldCount,
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Item.Version.Size == 0) &&
			xrtHttpTokenEqual(
				Item.Protocol,
				XRT_STR_LITERAL("websocket")
			) ) {
			bFound = true;
		}
	}
	return (Next == XHTTP_NEXT_END) && bFound;
}



/* 101 响应只能选择唯一的无版本 websocket Upgrade。 */
static bool __xrtWsUpgradeResponseProtocol(
	const xhttp1head* pHead
)
{
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Item;
	xhttpnext Next;
	size_t iCount = 0;
	bool bMatch = false;

	xrtHttpUpgradeFieldCursorInit(&Cursor);
	while ( (Next = xrtHttpUpgradeFieldNext(
		pHead->Fields,
		pHead->FieldCount,
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( iCount == SIZE_MAX ) {
			return false;
		}
		iCount++;
		bMatch = (Item.Version.Size == 0) &&
			xrtHttpTokenEqual(
				Item.Protocol,
				XRT_STR_LITERAL("websocket")
			);
	}
	return (Next == XHTTP_NEXT_END) &&
		(iCount == 1u) && bMatch;
}



/* 判断协议是否已经出现在更早的重复字段中。 */
static bool __xrtWsUpgradeProtocolSeen(
	const xhttp1head* pHead,
	size_t iField,
	xstrview Protocol
)
{
	for ( size_t i = 0; i < iField; i++ ) {
		xhttpfield Field;

		__xrtWsUpgradeFieldRead(pHead->Fields, i, &Field);
		if ( xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol")
		) && xrtWsProtocolsHas(Field.Value, Protocol) ) {
			return true;
		}
	}
	return false;
}



/* 验证重复子协议字段并按客户端线路顺序选择首个交集。 */
static bool __xrtWsUpgradeProtocolSelect(
	const xhttp1head* pHead,
	xstrview Supported,
	xstrview* pSelected
)
{
	xstrview Selected = { 0 };

	if ( !xrtWsProtocolsValid(Supported) ) {
		return __xrtWsUpgradeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"check-websocket-request",
			"WebSocket server protocol list is invalid"
		);
	}
	for ( size_t i = 0; i < pHead->FieldCount; i++ ) {
		xhttpfield Field;
		size_t iOffset = 0;

		__xrtWsUpgradeFieldRead(pHead->Fields, i, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol")
		) ) {
			continue;
		}
		if ( !xrtWsProtocolsValid(Field.Value) ) {
			return __xrtWsUpgradeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_PROTOCOL,
				"check-websocket-request",
				"WebSocket request protocol field is invalid"
			);
		}
		for ( ;; ) {
			xstrview Offered;
			xhttpnext Next = xrtWsProtocolNext(
				Field.Value,
				&iOffset,
				&Offered
			);

			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			if ( Next != XHTTP_NEXT_ITEM ) {
				return false;
			}
			if ( __xrtWsUpgradeProtocolSeen(
				pHead,
				i,
				Offered
			) ) {
				return __xrtWsUpgradeError(
					XERR_PROTOCOL,
					XWS_HANDSHAKE_ERROR_PROTOCOL,
					"check-websocket-request",
					"WebSocket request repeats a protocol"
				);
			}
			if ( (Selected.Size == 0) &&
				xrtWsProtocolsHas(Supported, Offered) ) {
				Selected = Offered;
			}
		}
	}
	*pSelected = Selected;
	return true;
}



/* 初始化服务端 Upgrade 配置。 */
XRT_API void xrtWsUpgradeServerConfigInit(
	xwsupgradeserverconfig* pConfig
)
{
	xwsupgradeserverconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		(void)__xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"config-init-websocket-upgrade-server",
			"WebSocket Upgrade server configuration range is invalid"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 验证服务端 Upgrade 配置。 */
XRT_API bool xrtWsUpgradeServerConfigValid(
	const xwsupgradeserverconfig* pConfig
)
{
	xwsupgradeserverconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	if ( !__xrtRangeValid(
		Config.Protocols.Data,
		Config.Protocols.Size
	) || !xrtWsProtocolsValid(Config.Protocols) ) {
		return false;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)
		if ( Config.RequireDeflate && !Config.EnableDeflate ) {
			return false;
		}
	#endif
	return true;
}



/* 初始化客户端 Upgrade 配置。 */
XRT_API void xrtWsUpgradeClientConfigInit(
	xwsupgradeclientconfig* pConfig
)
{
	xwsupgradeclientconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		(void)__xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"config-init-websocket-upgrade-client",
			"WebSocket Upgrade client configuration range is invalid"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)
		xrtWsDeflateInit(&Config.Deflate);
	#endif
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 验证客户端 Upgrade 配置。 */
XRT_API bool xrtWsUpgradeClientConfigValid(
	const xwsupgradeclientconfig* pConfig
)
{
	xwsupgradeclientconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	if ( !__xrtRangeValid(
		Config.Protocols.Data,
		Config.Protocols.Size
	) || !xrtWsProtocolsValid(Config.Protocols) ) {
		return false;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)
		if ( Config.RequireDeflate && !Config.EnableDeflate ) {
			return false;
		}
		if ( Config.EnableDeflate ) {
			size_t iSize = 0;

			if ( !xrtWsDeflateOfferWrite(
				&Config.Deflate,
				NULL,
				0,
				&iSize
			) ) {
				return false;
			}
		}
	#endif
	return true;
}



/* 快照可选服务端配置。 */
static bool __xrtWsUpgradeServerConfigRead(
	xwsupgradeserverconfig* pConfig,
	const xwsupgradeserverconfig* pInput
)
{
	xrtWsUpgradeServerConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	return xrtWsUpgradeServerConfigValid(pConfig);
}



/* 快照可选客户端配置。 */
static bool __xrtWsUpgradeClientConfigRead(
	xwsupgradeclientconfig* pConfig,
	const xwsupgradeclientconfig* pInput
)
{
	xrtWsUpgradeClientConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	return xrtWsUpgradeClientConfigValid(pConfig);
}



/* 严格验证服务端收到的 WebSocket Upgrade 请求。 */
XRT_API bool xrtWsUpgradeRequestCheck(
	const xhttp1head* pRequest,
	const xwsupgradeserverconfig* pInputConfig,
	xwsupgrade* pOutput
)
{
	xwsupgradeserverconfig Config;
	xwsupgrade Upgrade;
	xhttp1head Request;
	const xhttpfield* pSource;
	xhttpfield Field;
	xhttpnext Connection;

	if ( !__xrtWsUpgradeServerConfigRead(
		&Config,
		pInputConfig
	) ) {
		return __xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-request",
			"WebSocket Upgrade server configuration is invalid"
		);
	}
	if ( !__xrtRangeValid(pOutput, sizeof(Upgrade)) ||
		!__xrtWsUpgradeHeadRead(
			pRequest,
			XHTTP_REQUEST,
			&Request,
			"check-websocket-request"
		) || !__xrtWsUpgradeOutputSeparate(
			&Request,
			pOutput,
			sizeof(Upgrade),
			"check-websocket-request"
		) ) {
		return false;
	}
	memset(&Upgrade, 0, sizeof(Upgrade));
	if ( !__xrtWsUpgradeProtocolSelect(
		&Request,
		Config.Protocols,
		&Upgrade.Protocol
	) ) {
		return false;
	}
	if ( !__xrtWsUpgradeTextEqual(
		Request.Method,
		XRT_STR_LITERAL("GET")
	) ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_METHOD,
			"check-websocket-request",
			"WebSocket Upgrade method is not GET"
		);
	}
	if ( Request.Version != XHTTP_VERSION_1_1 ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_VERSION,
			"check-websocket-request",
			"WebSocket Upgrade requires HTTP/1.1"
		);
	}
	pSource = __xrtWsUpgradeFieldUnique(
		&Request,
		XRT_STR_LITERAL("Host")
	);
	if ( pSource == NULL ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_HOST,
			"check-websocket-request",
			"WebSocket Upgrade requires exactly one Host"
		);
	}
	memcpy(&Field, pSource, sizeof(Field));
	if ( (Field.Value.Size == 0) ||
		!xrtHttpHostValid(Field.Value) ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_HOST,
			"check-websocket-request",
			"WebSocket Upgrade Host is invalid"
		);
	}
	if ( !__xrtWsUpgradeRequestProtocol(&Request) ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_UPGRADE,
			"check-websocket-request",
			"WebSocket Upgrade field is missing or invalid"
		);
	}
	Connection = xrtHttpFieldTokenFind(
		Request.Fields,
		Request.FieldCount,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade")
	);
	if ( Connection != XHTTP_NEXT_ITEM ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_CONNECTION,
			"check-websocket-request",
			"WebSocket Connection field does not select Upgrade"
		);
	}
	if ( !__xrtWsUpgradeTokenExact(
		&Request,
		XRT_STR_LITERAL("Sec-WebSocket-Version"),
		XRT_STR_LITERAL("13")
	) ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_VERSION,
			"check-websocket-request",
			"WebSocket version is missing, repeated or unsupported"
		);
	}
	pSource = __xrtWsUpgradeFieldUnique(
		&Request,
		XRT_STR_LITERAL("Sec-WebSocket-Key")
	);
	if ( pSource == NULL ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_KEY,
			"check-websocket-request",
			"WebSocket key is missing or repeated"
		);
	}
	memcpy(&Field, pSource, sizeof(Field));
	if ( !xrtWsKeyValid(Field.Value) ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_KEY,
			"check-websocket-request",
			"WebSocket key is invalid"
		);
	}
	if ( (xrtHttpFieldCount(
		Request.Fields,
		Request.FieldCount,
		XRT_STR_LITERAL("Content-Length")
	) != 0) || (xrtHttpFieldCount(
		Request.Fields,
		Request.FieldCount,
		XRT_STR_LITERAL("Transfer-Encoding")
	) != 0) ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_BODY,
			"check-websocket-request",
			"WebSocket Upgrade request contains HTTP body framing"
		);
	}
	#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)
		if ( !__xrtWsUpgradeServerDeflate(
			Request.Fields,
			Request.FieldCount,
			&Config,
			&Upgrade
		) ) {
			return false;
		}
	#endif
	if ( !xrtWsAccept(
		Field.Value,
		Upgrade.Accept,
		sizeof(Upgrade.Accept)
	) ) {
		return false;
	}
	memcpy(pOutput, &Upgrade, sizeof(Upgrade));
	return true;
}



/* 严格验证客户端收到的 WebSocket Upgrade 响应。 */
XRT_API bool xrtWsUpgradeResponseCheck(
	const xhttp1head* pResponse,
	xstrview Key,
	const xwsupgradeclientconfig* pInputConfig,
	xwsupgrade* pOutput
)
{
	xwsupgradeclientconfig Config;
	xwsupgrade Upgrade;
	xhttp1head Response;
	const xhttpfield* pSource;
	xhttpfield Field;
	xhttpnext Connection;
	size_t iProtocols;

	if ( !__xrtWsUpgradeClientConfigRead(
		&Config,
		pInputConfig
	) || !__xrtRangeValid(Key.Data, Key.Size) ||
		!xrtWsKeyValid(Key) ) {
		return __xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-response",
			"WebSocket Upgrade client configuration or key is invalid"
		);
	}
	if ( !__xrtRangeValid(pOutput, sizeof(Upgrade)) ||
		!__xrtWsUpgradeHeadRead(
			pResponse,
			XHTTP_RESPONSE,
			&Response,
			"check-websocket-response"
		) || !__xrtWsUpgradeOutputSeparate(
			&Response,
			pOutput,
			sizeof(Upgrade),
			"check-websocket-response"
		) || __xrtRangesOverlap(
			pOutput,
			sizeof(Upgrade),
			Key.Data,
			Key.Size
		) ) {
		return false;
	}
	memset(&Upgrade, 0, sizeof(Upgrade));
	if ( Response.Version != XHTTP_VERSION_1_1 ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_VERSION,
			"check-websocket-response",
			"WebSocket response requires HTTP/1.1"
		);
	}
	if ( Response.Status != XHTTP_STATUS_SWITCHING_PROTOCOLS ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_STATUS,
			"check-websocket-response",
			"WebSocket response status is not 101"
		);
	}
	if ( !__xrtWsUpgradeResponseProtocol(&Response) ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_UPGRADE,
			"check-websocket-response",
			"WebSocket response Upgrade field is not exactly websocket"
		);
	}
	Connection = xrtHttpFieldTokenFind(
		Response.Fields,
		Response.FieldCount,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade")
	);
	if ( Connection != XHTTP_NEXT_ITEM ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_CONNECTION,
			"check-websocket-response",
			"WebSocket response Connection field does not select Upgrade"
		);
	}
	pSource = __xrtWsUpgradeFieldUnique(
		&Response,
		XRT_STR_LITERAL("Sec-WebSocket-Accept")
	);
	if ( pSource == NULL ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_ACCEPT,
			"check-websocket-response",
			"WebSocket response Accept is missing or repeated"
		);
	}
	memcpy(&Field, pSource, sizeof(Field));
	if ( !xrtWsAcceptValid(Key, Field.Value) ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_ACCEPT,
			"check-websocket-response",
			"WebSocket response Accept is invalid"
		);
	}
	if ( (xrtHttpFieldCount(
		Response.Fields,
		Response.FieldCount,
		XRT_STR_LITERAL("Content-Length")
	) != 0) || (xrtHttpFieldCount(
		Response.Fields,
		Response.FieldCount,
		XRT_STR_LITERAL("Transfer-Encoding")
	) != 0) ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_BODY,
			"check-websocket-response",
			"WebSocket 101 response contains HTTP body framing"
		);
	}
	#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)
		if ( !__xrtWsUpgradeClientDeflate(
			Response.Fields,
			Response.FieldCount,
			&Config,
			&Upgrade
		) ) {
			return false;
		}
	#else
		if ( xrtHttpFieldCount(
			Response.Fields,
			Response.FieldCount,
			XRT_STR_LITERAL("Sec-WebSocket-Extensions")
		) != 0 ) {
			return __xrtWsUpgradeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"WebSocket response selected an unoffered extension"
			);
		}
	#endif
	iProtocols = xrtHttpFieldCount(
		Response.Fields,
		Response.FieldCount,
		XRT_STR_LITERAL("Sec-WebSocket-Protocol")
	);
	if ( iProtocols > 1u ) {
		return __xrtWsUpgradeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"check-websocket-response",
			"WebSocket response repeats the selected protocol"
		);
	}
	if ( iProtocols == 1u ) {
		pSource = __xrtWsUpgradeFieldUnique(
			&Response,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol")
		);
		if ( pSource == NULL ) {
			return false;
		}
		memcpy(&Field, pSource, sizeof(Field));
		if ( !xrtHttpTokenValid(Field.Value) ||
			(Config.Protocols.Size == 0) ||
			!xrtWsProtocolsHas(
				Config.Protocols,
				Field.Value
			) ) {
			return __xrtWsUpgradeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_PROTOCOL,
				"check-websocket-response",
				"WebSocket response selected an unoffered protocol"
			);
		}
		Upgrade.Protocol = Field.Value;
	}
	if ( !xrtWsAccept(
		Key,
		Upgrade.Accept,
		sizeof(Upgrade.Accept)
	) ) {
		return false;
	}
	memcpy(pOutput, &Upgrade, sizeof(Upgrade));
	return true;
}



/* 验证字段填充入口的借用文本。 */
static bool __xrtWsUpgradeFieldsInput(
	xstrview First,
	xstrview Protocol,
	xstrview Extensions,
	cstr sOperation
)
{
	if ( !__xrtRangeValid(First.Data, First.Size) ||
		!__xrtRangeValid(Protocol.Data, Protocol.Size) ||
		!__xrtRangeValid(Extensions.Data, Extensions.Size) ||
		!xrtWsProtocolsValid(Protocol) ||
		!xrtHttpFieldValueValid(Extensions) ) {
		return __xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			sOperation,
			"WebSocket Upgrade field input is invalid"
		);
	}
	#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)
		if ( (Extensions.Size != 0) &&
			!__xrtWsUpgradeExtensionsValid(Extensions) ) {
			return __xrtWsUpgradeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				sOperation,
				"WebSocket extension field is invalid"
			);
		}
	#endif
	return true;
}



/* 原子发布调用方拥有的字段描述符数组。 */
static bool __xrtWsUpgradeFieldsPublish(
	const xhttpfield* pSource,
	size_t iRequired,
	const xstrview* pInputs,
	size_t iInputCount,
	xhttpfield* pFields,
	size_t iCapacity,
	size_t* pCount,
	cstr sOperation
)
{
	if ( (iCapacity > (SIZE_MAX / sizeof(*pFields))) ||
		!__xrtRangeValid(pCount, sizeof(iRequired)) ||
		((pFields == NULL) && (iCapacity != 0)) ||
		((pFields != NULL) &&
		 !__xrtRangeValid(
			pFields,
			iCapacity * sizeof(*pFields)
		 )) || ((pFields != NULL) && __xrtRangesOverlap(
			pFields,
			iCapacity * sizeof(*pFields),
			pCount,
			sizeof(iRequired)
		 )) ) {
		return __xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			sOperation,
			"WebSocket Upgrade field output is invalid"
		);
	}
	for ( size_t i = 0; i < iInputCount; i++ ) {
		xstrview Input;

		memcpy(&Input, pInputs + i, sizeof(Input));
		if ( __xrtRangesOverlap(
			pCount,
			sizeof(iRequired),
			Input.Data,
			Input.Size
		) || ((pFields != NULL) && __xrtRangesOverlap(
			pFields,
			iCapacity * sizeof(*pFields),
			Input.Data,
			Input.Size
		)) ) {
			return __xrtWsUpgradeError(
				XERR_ARGUMENT,
				XWS_HANDSHAKE_ERROR_ARGUMENT,
				sOperation,
				"WebSocket Upgrade field output overlaps input text"
			);
		}
	}
	memcpy(pCount, &iRequired, sizeof(iRequired));
	if ( (pFields == NULL) && (iCapacity == 0) ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		return __xrtWsUpgradeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			sOperation,
			"WebSocket Upgrade field capacity is insufficient"
		);
	}
	if ( iRequired != 0 ) {
		memcpy(
			pFields,
			pSource,
			iRequired * sizeof(*pFields)
		);
	}
	return true;
}



/* 填充客户端 WebSocket Upgrade 字段。 */
XRT_API bool xrtWsUpgradeRequestFields(
	xstrview Host,
	xstrview Key,
	xstrview Protocols,
	xstrview Extensions,
	xhttpfield* pFields,
	size_t iCapacity,
	size_t* pCount
)
{
	xhttpfield Fields[XWS_UPGRADE_REQUEST_FIELDS_MAX];
	xstrview Inputs[4];
	size_t iCount = 0;

	if ( !__xrtWsUpgradeFieldsInput(
		Host,
		Protocols,
		Extensions,
		"write-websocket-upgrade-request-fields"
	) ) {
		return false;
	}
	if ( (Host.Size == 0) || !xrtHttpHostValid(Host) ||
		!__xrtRangeValid(Key.Data, Key.Size) ||
		!xrtWsKeyValid(Key) ) {
		return __xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"write-websocket-upgrade-request-fields",
			"WebSocket Host or Key is invalid"
		);
	}
	Fields[iCount++] = (xhttpfield) {
		XRT_STR_LITERAL("Host"),
		Host
	};
	Fields[iCount++] = (xhttpfield) {
		XRT_STR_LITERAL("Upgrade"),
		XRT_STR_LITERAL("websocket")
	};
	Fields[iCount++] = (xhttpfield) {
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade")
	};
	Fields[iCount++] = (xhttpfield) {
		XRT_STR_LITERAL("Sec-WebSocket-Version"),
		XRT_STR_LITERAL("13")
	};
	Fields[iCount++] = (xhttpfield) {
		XRT_STR_LITERAL("Sec-WebSocket-Key"),
		Key
	};
	if ( Protocols.Size != 0 ) {
		Fields[iCount++] = (xhttpfield) {
			XRT_STR_LITERAL("Sec-WebSocket-Protocol"),
			Protocols
		};
	}
	if ( Extensions.Size != 0 ) {
		Fields[iCount++] = (xhttpfield) {
			XRT_STR_LITERAL("Sec-WebSocket-Extensions"),
			Extensions
		};
	}
	Inputs[0] = Host;
	Inputs[1] = Key;
	Inputs[2] = Protocols;
	Inputs[3] = Extensions;
	return __xrtWsUpgradeFieldsPublish(
		Fields,
		iCount,
		Inputs,
		sizeof(Inputs) / sizeof(Inputs[0]),
		pFields,
		iCapacity,
		pCount,
		"write-websocket-upgrade-request-fields"
	);
}



/* 填充服务端 WebSocket Upgrade 响应字段。 */
XRT_API bool xrtWsUpgradeResponseFields(
	xstrview Accept,
	xstrview Protocol,
	xstrview Extensions,
	xhttpfield* pFields,
	size_t iCapacity,
	size_t* pCount
)
{
	xhttpfield Fields[XWS_UPGRADE_RESPONSE_FIELDS_MAX];
	xstrview Inputs[3];
	size_t iCount = 0;

	if ( !__xrtWsUpgradeFieldsInput(
		Accept,
		Protocol,
		Extensions,
		"write-websocket-upgrade-response-fields"
	) ) {
		return false;
	}
	if ( (Accept.Size != XWS_ACCEPT_SIZE) ||
		!xrtHttpFieldValueValid(Accept) ||
		((Protocol.Size != 0) &&
		 !xrtHttpTokenValid(Protocol)) ) {
		return __xrtWsUpgradeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"write-websocket-upgrade-response-fields",
			"WebSocket Accept or selected protocol is invalid"
		);
	}
	Fields[iCount++] = (xhttpfield) {
		XRT_STR_LITERAL("Upgrade"),
		XRT_STR_LITERAL("websocket")
	};
	Fields[iCount++] = (xhttpfield) {
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade")
	};
	Fields[iCount++] = (xhttpfield) {
		XRT_STR_LITERAL("Sec-WebSocket-Accept"),
		Accept
	};
	if ( Protocol.Size != 0 ) {
		Fields[iCount++] = (xhttpfield) {
			XRT_STR_LITERAL("Sec-WebSocket-Protocol"),
			Protocol
		};
	}
	if ( Extensions.Size != 0 ) {
		Fields[iCount++] = (xhttpfield) {
			XRT_STR_LITERAL("Sec-WebSocket-Extensions"),
			Extensions
		};
	}
	Inputs[0] = Accept;
	Inputs[1] = Protocol;
	Inputs[2] = Extensions;
	return __xrtWsUpgradeFieldsPublish(
		Fields,
		iCount,
		Inputs,
		sizeof(Inputs) / sizeof(Inputs[0]),
		pFields,
		iCapacity,
		pCount,
		"write-websocket-upgrade-response-fields"
	);
}

#endif
