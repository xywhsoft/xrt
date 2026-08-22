#include "../internal/xrt_websocket_http.h"



#if defined(XWS_FEATURE_WEBSOCKET_SERVER)

typedef struct __xrt_ws_upgrade {
	xwsserverconfig Config;
	xwsconnevents Events;
	xwsupgradeproc Proc;
	ptr EventData;
	ptr Data;
	xstrview Protocol;
	char Storage[];
} __xrt_ws_upgrade;



/* 比较大小写敏感的子协议 token。 */
static bool __xrtWsServerProtocolEqual(
	xstrview Left,
	xstrview Right
)
{
	return __xrtWsHttpTextEqual(Left, Right);
}



/* 判断当前 token 是否已在更早字段或当前字段前缀出现。 */
static bool __xrtWsServerProtocolSeen(
	const xhttpserverrequest* pRequest,
	size_t iField,
	size_t iOrdinal,
	xstrview Protocol
)
{
	for ( size_t i = 0; i <= iField; i++ ) {
		const xhttpfield* pField =
			xrtHttpServerRequestHeaderAt(pRequest, i);
		size_t iOffset = 0;
		size_t iLimit = i == iField ?
			iOrdinal : SIZE_MAX;
		size_t iCurrent = 0;

		if ( (pField == NULL) ||
			!xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Protocol"
				)
			) ) {
			continue;
		}
		while ( iCurrent < iLimit ) {
			xstrview Previous;
			xhttpnext Next = xrtWsProtocolNext(
				pField->Value,
				&iOffset,
				&Previous
			);

			if ( Next != XHTTP_NEXT_ITEM ) {
				break;
			}
			if ( __xrtWsServerProtocolEqual(
				Previous,
				Protocol
			) ) {
				return true;
			}
			iCurrent++;
		}
	}
	return false;
}



/* 验证重复子协议字段并按客户端出现顺序选择交集。 */
static bool __xrtWsServerProtocolSelect(
	const xhttpserverrequest* pRequest,
	xstrview Protocols,
	xstrview* pSelected
)
{
	size_t iCount = xrtHttpServerRequestHeaderCount(
		pRequest
	);
	xstrview Selected = { 0 };

	if ( !xrtWsProtocolsValid(Protocols) ) {
		__xwsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"check-websocket-request",
			"WebSocket server protocol list is invalid"
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpServerRequestHeaderAt(pRequest, i);
		size_t iOffset = 0;
		size_t iOrdinal = 0;

		if ( (pField == NULL) ||
			!xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Protocol"
				)
			) ) {
			continue;
		}
		if ( !xrtWsProtocolsValid(pField->Value) ) {
			__xwsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_PROTOCOL,
				"check-websocket-request",
				"WebSocket request protocol field is invalid"
			);
			return false;
		}
		for ( ;; ) {
			xstrview Offered;
			xhttpnext Next = xrtWsProtocolNext(
				pField->Value,
				&iOffset,
				&Offered
			);

			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			if ( Next != XHTTP_NEXT_ITEM ) {
				return false;
			}
			if ( __xrtWsServerProtocolSeen(
				pRequest,
				i,
				iOrdinal,
				Offered
			) ) {
				__xwsHandshakeError(
					XERR_PROTOCOL,
					XWS_HANDSHAKE_ERROR_PROTOCOL,
					"check-websocket-request",
					"WebSocket request repeats a protocol"
				);
				return false;
			}
			if ( (Selected.Size == 0) &&
				xrtWsProtocolsHas(
					Protocols,
					Offered
				) ) {
				Selected = Offered;
			}
			iOrdinal++;
		}
	}
	*pSelected = Selected;
	return true;
}




#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)

typedef struct __xrt_ws_server_deflate {
	xwsdeflate Offer;
	bool Found;
} __xrt_ws_server_deflate;



/* 找出唯一的 permessage-deflate offer；未知扩展由服务端主动忽略。 */
static bool __xrtWsServerDeflateVisit(
	const xwsextension* pExtension,
	ptr pData
)
{
	__xrt_ws_server_deflate* pDeflate =
		(__xrt_ws_server_deflate*)pData;

	if ( !xrtWsDeflateIs(pExtension) ) {
		return true;
	}
	if ( pDeflate->Found ) {
		__xwsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket request repeats permessage-deflate"
		);
		return false;
	}
	if ( !xrtWsDeflateOfferParse(
		pExtension,
		&pDeflate->Offer
	) ) {
		__xwsHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket permessage-deflate offer is invalid"
		);
		return false;
	}
	pDeflate->Found = true;
	return true;
}



/* 运行可选策略，并区分无错误的主动放弃和带原因的策略失败。 */
static bool __xrtWsServerDeflateAccept(
	const xwsserverconfig* pConfig,
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	bool* pAccepted
)
{
	xerror* pPrevious;
	xerror* pCallbackError;
	bool bAccepted;

	if ( !xrtWsDeflateAccept(pOffer, pResponse) ) {
		__xwsHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket permessage-deflate offer cannot be accepted"
		);
		return false;
	}
	if ( pConfig->AcceptDeflate == NULL ) {
		*pAccepted = true;
		return true;
	}

	pPrevious = xrtTakeError();
	bAccepted = pConfig->AcceptDeflate(
		pOffer,
		pResponse,
		pConfig->DeflateData
	);
	pCallbackError = xrtTakeError();
	if ( !bAccepted && (pCallbackError != NULL) ) {
		xrtSetError(pCallbackError);
		__xwsHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket permessage-deflate policy failed"
		);
		xrtErrorFree(pCallbackError);
		xrtErrorFree(pPrevious);
		return false;
	}
	xrtErrorFree(pCallbackError);
	if ( pPrevious != NULL ) {
		xrtSetError(pPrevious);
	}
	xrtErrorFree(pPrevious);
	*pAccepted = bAccepted;
	return true;
}



/* 协商服务端压缩响应，并把规范字段值保存在握手快照中。 */
static bool __xrtWsServerDeflateSelect(
	const __xrt_ws_fields* pFields,
	const xwsserverconfig* pConfig,
	xwsserverhandshake* pHandshake
)
{
	__xrt_ws_server_deflate Deflate;
	xwsdeflate Response;
	size_t iSize = 0;
	bool bAccepted;

	if ( !pConfig->EnableDeflate ) {
		if ( pConfig->RequireDeflate ) {
			__xwsHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-request",
				"Required WebSocket compression is not enabled"
			);
			return false;
		}
		return true;
	}
	memset(&Deflate, 0, sizeof(Deflate));
	if ( !__xrtWsHttpExtensionsVisit(
		pFields,
		__xrtWsServerDeflateVisit,
		&Deflate
	) ) {
		return false;
	}
	if ( !Deflate.Found ) {
		if ( pConfig->RequireDeflate ) {
			__xwsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-request",
				"WebSocket request did not offer required compression"
			);
			return false;
		}
		return true;
	}
	if ( !__xrtWsServerDeflateAccept(
		pConfig,
		&Deflate.Offer,
		&Response,
		&bAccepted
	) ) {
		return false;
	}
	if ( !bAccepted ) {
		if ( pConfig->RequireDeflate ) {
			__xwsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-request",
				"WebSocket compression policy declined a required offer"
			);
			return false;
		}
		return true;
	}
	if ( !xrtWsDeflateResponseCheck(
		&Deflate.Offer,
		&Response
	) || !xrtWsDeflateResponseWrite(
		&Response,
		pHandshake->Extensions,
		XWS_DEFLATE_MAX_SIZE,
		&iSize
	) ) {
		__xwsHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket compression policy produced an invalid response"
		);
		return false;
	}
	pHandshake->Extensions[iSize] = '\0';
	pHandshake->Deflate = Response;
	pHandshake->DeflateEnabled = true;
	return true;
}

#endif



/* 初始化服务端 Upgrade 默认配置。 */
XRT_API void xrtWsServerConfigInit(
	xwsserverconfig* pConfig
)
{
	xwsserverconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xwsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"config-init-websocket-server",
			"WebSocket server configuration range is invalid"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtWsConnConfigInit(&Config.Connection);
	Config.Connection.Role = XWS_ROLE_SERVER;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 验证服务端固定配置，并以 Upgrade 实际采用的 Connection 参数完成预检。 */
XRT_API bool xrtWsServerConfigValid(
	const xwsserverconfig* pConfig
)
{
	xwsserverconfig Config;
	xwsconnconfig Connection;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	if ( !xrtWsProtocolsValid(Config.Protocols) ) {
		return false;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		if ( Config.RequireDeflate &&
			!Config.EnableDeflate ) {
			return false;
		}
	#endif
	Connection = Config.Connection;
	Connection.Role = XWS_ROLE_SERVER;
	Connection.Protocol = (xstrview) { 0 };
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		Connection.DeflateEnabled = false;
	#endif
	return xrtWsConnConfigValid(&Connection);
}



/* 严格验证服务端 HTTP/1.1 Upgrade 请求。 */
XRT_API bool xrtWsServerCheck(
	const xhttpserverrequest* pRequest,
	const xwsserverconfig* pConfig,
	xwsserverhandshake* pHandshake
)
{
	__xrt_ws_fields Fields;
	xwsserverconfig Config;
	xwsserverhandshake Handshake;
	const xhttpfield* pKey;
	xhttpauthority Authority;
	bool bConnection;

	if ( !__xrtWsServerConfigSnapshot(
		&Config,
		pConfig,
		"check-websocket-request"
	) || !__xrtWsHttpObjectCheck(
		pRequest,
		"check-websocket-request",
		"WebSocket server request is null"
	) || !__xrtWsHttpRangeCheck(
		pHandshake,
		sizeof(Handshake),
		"check-websocket-request",
		"WebSocket server handshake output range is invalid"
	) ) {
		return false;
	}
	if ( ((Config.Protocols.Data == NULL) &&
		 (Config.Protocols.Size != 0)) ||
		(pHandshake == (const void*)pRequest) ||
		((pConfig != NULL) && xrtMemRangesOverlap(
			pHandshake,
			sizeof(Handshake),
			pConfig,
			sizeof(*pConfig)
		)) ) {
		__xwsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-request",
			"WebSocket protocol list or handshake output is invalid"
		);
		return false;
	}
	memset(&Handshake, 0, sizeof(Handshake));
	Fields.Data = xrtHttpServerRequestHeaderData(pRequest);
	Fields.Count = xrtHttpServerRequestHeaderCount(
		pRequest
	);
	if ( !__xrtWsServerProtocolSelect(
		pRequest,
		Config.Protocols,
		&Handshake.Protocol
	) ) {
		return false;
	}
	if ( !__xrtWsHttpTextEqual(
		xrtHttpServerRequestMethod(pRequest),
		XRT_STR_LITERAL("GET")
	) ) {
		__xwsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_METHOD,
			"check-websocket-request",
			"WebSocket Upgrade method is not GET"
		);
		return false;
	}
	if ( xrtHttpServerRequestVersion(pRequest) !=
		XHTTP_VERSION_1_1 ) {
		__xwsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_VERSION,
			"check-websocket-request",
			"WebSocket Upgrade requires HTTP/1.1"
		);
		return false;
	}
	if ( __xrtWsHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Host")
	) != 1 ) {
		__xwsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_HOST,
			"check-websocket-request",
			"WebSocket Upgrade requires exactly one Host"
		);
		return false;
	}
	if ( !xrtHttpServerRequestAuthority(
		pRequest,
		&Authority
	) ) {
		__xwsHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_HOST,
			"check-websocket-request",
			"WebSocket Upgrade requires one valid Host"
		);
		return false;
	}
	if ( !__xrtWsHttpUpgradeHas(&Fields) ) {
		__xwsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_UPGRADE,
			"check-websocket-request",
			"WebSocket Upgrade field is missing or ambiguous"
		);
		return false;
	}
	if ( !__xrtWsHttpTokenHas(
		&Fields,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade"),
		&bConnection
	) || !bConnection ) {
		__xwsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_CONNECTION,
			"check-websocket-request",
			"WebSocket Connection field does not select Upgrade"
		);
		return false;
	}
	if ( !__xrtWsHttpTokenExact(
		&Fields,
		XRT_STR_LITERAL("Sec-WebSocket-Version"),
		XRT_STR_LITERAL("13")
	) ) {
		__xwsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_VERSION,
			"check-websocket-request",
			"WebSocket version is missing, repeated or unsupported"
		);
		return false;
	}
	pKey = __xrtWsHttpFieldUnique(
		&Fields,
		XRT_STR_LITERAL("Sec-WebSocket-Key")
	);
	if ( (pKey == NULL) ||
		!xrtWsKeyValid(pKey->Value) ) {
		__xwsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_KEY,
			"check-websocket-request",
			"WebSocket key is missing, repeated or invalid"
		);
		return false;
	}
	if ( (__xrtWsHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Content-Length")
	) != 0) || (__xrtWsHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Transfer-Encoding")
	) != 0) || (xrtHttpServerRequestBodyMode(
		pRequest
	) != XHTTP1_BODY_NONE) ||
		(xrtHttpServerRequestBodyBytes(pRequest) != 0) ) {
		__xwsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_BODY,
			"check-websocket-request",
			"WebSocket Upgrade request contains an HTTP body"
		);
		return false;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		if ( !__xrtWsServerDeflateSelect(
			&Fields,
			&Config,
			&Handshake
		) ) {
			return false;
		}
	#endif
	if ( !xrtWsAccept(
		pKey->Value,
		Handshake.Accept,
		sizeof(Handshake.Accept)
	) ) {
		return false;
	}
	memcpy(pHandshake, &Handshake, sizeof(Handshake));
	return true;
}



/* 从握手快照构造标准 101 Reply。 */
XRT_API xhttpreply* xrtWsServerReply(
	const xwsserverhandshake* pHandshake
)
{
	xwsserverhandshake Handshake;
	xhttpreply* pReply;
	#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		char sExtensions[XWS_DEFLATE_MAX_SIZE];
		size_t iExtensions = 0;
		bool bDeflateValid;
	#endif

	if ( !__xrtWsHttpRangeCheck(
		pHandshake,
		sizeof(Handshake),
		"reply-websocket-upgrade",
		"WebSocket server handshake snapshot range is invalid"
	) ) {
		return NULL;
	}
	memcpy(&Handshake, pHandshake, sizeof(Handshake));
	#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		bDeflateValid = !Handshake.DeflateEnabled ||
			 (xrtWsDeflateResponseWrite(
				&Handshake.Deflate,
				sExtensions,
				sizeof(sExtensions),
				&iExtensions
			  ) &&
			  (Handshake.Extensions[iExtensions] == '\0') &&
			  (memcmp(
				Handshake.Extensions,
				sExtensions,
				iExtensions
			   ) == 0));
	#endif
	if ( (Handshake.Accept[XWS_ACCEPT_SIZE] != '\0') ||
		!xrtHttpFieldValueValid((xstrview) {
			Handshake.Accept,
			XWS_ACCEPT_SIZE
		}) ||
		((Handshake.Protocol.Data == NULL) &&
		 (Handshake.Protocol.Size != 0)) ||
		((Handshake.Protocol.Size != 0) &&
		 !xrtHttpTokenValid(Handshake.Protocol))
		#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)
			|| !bDeflateValid
		#endif
		) {
		__xwsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"reply-websocket-upgrade",
			"WebSocket server handshake snapshot is invalid"
		);
		return NULL;
	}
	pReply = xrtHttpReplyCreate(101);
	if ( pReply == NULL ) {
		__xwsHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"reply-websocket-upgrade",
			"WebSocket 101 Reply allocation failed"
		);
		return NULL;
	}
	if ( !xrtHttpReplySetHeader(
		pReply,
		XRT_STR_LITERAL("Upgrade"),
		XRT_STR_LITERAL("websocket")
	) || !xrtHttpReplySetHeader(
		pReply,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade")
	) || !xrtHttpReplySetHeader(
		pReply,
		XRT_STR_LITERAL("Sec-WebSocket-Accept"),
		(xstrview) {
			Handshake.Accept,
			XWS_ACCEPT_SIZE
		}
	) || ((Handshake.Protocol.Size != 0) &&
		!xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol"),
			Handshake.Protocol
		))
	#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		|| (Handshake.DeflateEnabled &&
			!xrtHttpReplySetHeader(
				pReply,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Extensions"
				),
				(xstrview) {
					Handshake.Extensions,
					iExtensions
				}
			))
	#endif
		) {
		xrtHttpReplyDestroy(pReply);
		__xwsHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"reply-websocket-upgrade",
			"WebSocket 101 Reply fields could not be stored"
		);
		return NULL;
	}
	return pReply;
}



/* 根据稳定握手域和错误类型选择不会泄露内部细节的 HTTP 状态。 */
static uint16 __xrtWsServerRejectStatus(
	const xerror* pError
)
{
	cstr sDomain;
	int32 iCode;

	if ( (pError == NULL) ||
		(xrtErrorKind(pError) != XERR_PROTOCOL) ) {
		return XHTTP_STATUS_INTERNAL_SERVER_ERROR;
	}
	sDomain = xrtErrorDomain(pError);
	if ( (sDomain == NULL) ||
		(strcmp(sDomain, "xrt.websocket.handshake") != 0) ) {
		return XHTTP_STATUS_INTERNAL_SERVER_ERROR;
	}
	iCode = xrtErrorCode(pError);
	if ( iCode == (int32)XWS_HANDSHAKE_ERROR_METHOD ) {
		return XHTTP_STATUS_METHOD_NOT_ALLOWED;
	}
	if ( iCode == (int32)XWS_HANDSHAKE_ERROR_VERSION ) {
		return XHTTP_STATUS_UPGRADE_REQUIRED;
	}
	return XHTTP_STATUS_BAD_REQUEST;
}



/* 构造并提交标准 WebSocket 握手拒绝响应。 */
XRT_API xnetresult xrtWsServerReject(
	xhttpconn* pHttp,
	const xerror* pError
)
{
	uint16 iStatus;
	xhttpreply* pReply;
	xnetresult Result;
	bool bReady;

	if ( !__xrtWsHttpObjectCheck(
		pHttp,
		"reject-websocket-upgrade",
		"HTTP server connection is null"
	) ) {
		return XNET_RESULT_ERROR;
	}
	iStatus = __xrtWsServerRejectStatus(pError);
	pReply = xrtHttpReplyCreate(iStatus);
	if ( pReply == NULL ) {
		return XNET_RESULT_ERROR;
	}
	bReady = true;
	if ( iStatus == XHTTP_STATUS_METHOD_NOT_ALLOWED ) {
		bReady = xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Allow"),
			XRT_STR_LITERAL("GET")
		);
	} else if ( iStatus == XHTTP_STATUS_UPGRADE_REQUIRED ) {
		bReady = xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("websocket")
		) && xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Sec-WebSocket-Version"),
			XRT_STR_LITERAL("13")
		);
	}
	if ( bReady ) {
		Result = xrtHttpConnRespond(pHttp, pReply);
	} else {
		Result = XNET_RESULT_ERROR;
	}
	xrtHttpReplyDestroy(pReply);
	return Result;
}



/* 把 HTTP Upgrade 终态接管为共享 WebSocket Connection。 */
static void __xrtWsServerUpgradeDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
)
{
	__xrt_ws_upgrade* pUpgrade =
		(__xrt_ws_upgrade*)pData;
	xwsconn* pConnection = NULL;
	xerror* pOwnedError = NULL;

	if ( Result == XNET_RESULT_OK ) {
		pUpgrade->Config.Connection.Protocol =
			pUpgrade->Protocol;
		if ( Upgrade.Tcp != NULL ) {
			pConnection = xrtWsConnAttach(
				Upgrade.Tcp,
				&pUpgrade->Config.Connection,
				&pUpgrade->Events,
				pUpgrade->EventData
			);
			if ( pConnection != NULL ) {
				Upgrade.Tcp = NULL;
			}
		}
		#if defined(XWS_FEATURE_WEBSOCKET_SERVER_TLS)
			else if ( Upgrade.Tls != NULL ) {
				pConnection = xrtWsConnAttachTls(
					Upgrade.Tls,
					&pUpgrade->Config.Connection,
					&pUpgrade->Events,
					pUpgrade->EventData
				);
				if ( pConnection != NULL ) {
					Upgrade.Tls = NULL;
				}
			}
		#endif
		else {
			__xwsHandshakeError(
				XERR_STATE,
				XWS_HANDSHAKE_ERROR_UPGRADE,
				"attach-websocket-server",
				"HTTP Upgrade returned an unsupported transport"
			);
		}
		if ( pConnection == NULL ) {
			pOwnedError = xrtTakeError();
			xrtHttpUpgradeAbort(&Upgrade);
			Result = XNET_RESULT_ERROR;
			pError = pOwnedError;
		}
	}
	pUpgrade->Proc(
		pHttp,
		Result,
		pConnection,
		pError,
		pUpgrade->Data
	);
	xrtErrorFree(pOwnedError);
	xrtFree(pUpgrade);
}



/* 判断附加 Header 是否会覆盖 WebSocket 握手或 HTTP 分帧语义。 */
static bool __xrtWsServerHeaderReserved(xstrview Name)
{
	return xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Connection")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Upgrade")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Sec-WebSocket-Accept")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Sec-WebSocket-Protocol")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Sec-WebSocket-Extensions")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Content-Length")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Transfer-Encoding")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Trailer")
	);
}



/* 把已经快照的握手、事件和 Reply 提交给 HTTP Upgrade 层。 */
static xnetresult __xrtWsServerUpgradeSubmit(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsserverhandshake* pHandshake,
	const xhttpreply* pReply,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
)
{
	__xrt_ws_upgrade* pUpgrade;
	xnetresult Result;
	size_t iAllocation;

	if ( pHandshake->Protocol.Size >
		(SIZE_MAX - sizeof(*pUpgrade) - 1u) ) {
		__xwsHandshakeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"accept-websocket-upgrade",
			"WebSocket selected protocol is too large"
		);
		return XNET_RESULT_ERROR;
	}
	iAllocation = sizeof(*pUpgrade) +
		pHandshake->Protocol.Size + 1u;
	pUpgrade = (__xrt_ws_upgrade*)xrtCalloc(
		1,
		iAllocation
	);
	if ( pUpgrade == NULL ) {
		__xwsHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"accept-websocket-upgrade",
			"WebSocket Upgrade context allocation failed"
		);
		return XNET_RESULT_ERROR;
	}
	pUpgrade->Config = *pConfig;
	pUpgrade->Config.Connection.Role = XWS_ROLE_SERVER;
	pUpgrade->Config.Connection.Protocol =
		(xstrview) { 0 };
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		pUpgrade->Config.Connection.DeflateEnabled = false;
		#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)
			pUpgrade->Config.Connection.DeflateEnabled =
				pHandshake->DeflateEnabled;
			if ( pHandshake->DeflateEnabled ) {
				pUpgrade->Config.Connection.Deflate =
					pHandshake->Deflate;
			}
		#endif
	#endif
	pUpgrade->Events = *pEvents;
	pUpgrade->Proc = pProc;
	pUpgrade->EventData = pEventData;
	pUpgrade->Data = pData;
	if ( pHandshake->Protocol.Size != 0 ) {
		memcpy(
			pUpgrade->Storage,
			pHandshake->Protocol.Data,
			pHandshake->Protocol.Size
		);
		pUpgrade->Protocol.Data =
			pUpgrade->Storage;
		pUpgrade->Protocol.Size =
			pHandshake->Protocol.Size;
	}
	Result = xrtHttpConnUpgrade(
		pHttp,
		pReply,
		__xrtWsServerUpgradeDone,
		pUpgrade
	);
	if ( Result != XNET_RESULT_OK ) {
		xrtFree(pUpgrade);
	}
	return Result;
}



/* 提交已经校验的握手，并在受保护字段之外追加应用 Header。 */
XRT_API xnetresult xrtWsUpgradeAccept(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsserverhandshake* pHandshake,
	const xhttpheaders* pHeaders,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
)
{
	xwsserverconfig Config;
	xwsconnevents Events;
	xwsserverhandshake Handshake;
	xhttpheaders* pHeaderSnapshot = NULL;
	xhttpreply* pReply;
	xnetresult Result;
	size_t iHeaderCount;

	if ( !__xrtWsServerConfigSnapshot(
		&Config,
		pConfig,
		"accept-websocket-upgrade"
	) || !__xrtWsConnEventsSnapshot(
		&Events,
		pEvents,
		"accept-websocket-upgrade"
	) || !__xrtWsHttpObjectCheck(
		pHttp,
		"accept-websocket-upgrade",
		"HTTP server connection is null"
	) || !__xrtWsHttpRangeCheck(
		pHandshake,
		sizeof(Handshake),
		"accept-websocket-upgrade",
		"WebSocket server handshake snapshot range is invalid"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pProc == NULL ) {
		__xwsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"accept-websocket-upgrade",
			"WebSocket Upgrade callback is null"
		);
		return XNET_RESULT_ERROR;
	}
	if ( !xrtWsServerConfigValid(&Config) ) {
		__xwsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"accept-websocket-upgrade",
			"WebSocket server configuration is invalid"
		);
		return XNET_RESULT_ERROR;
	}
	memcpy(&Handshake, pHandshake, sizeof(Handshake));
	if ( (Handshake.Protocol.Size != 0) &&
		!xrtWsProtocolsHas(
			Config.Protocols,
			Handshake.Protocol
		) ) {
		__xwsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"accept-websocket-upgrade",
			"WebSocket selected protocol is not enabled by the server"
		);
		return XNET_RESULT_ERROR;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		if ( Handshake.DeflateEnabled &&
			!Config.EnableDeflate ) {
			__xwsHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"accept-websocket-upgrade",
				"WebSocket compression was not enabled by the server"
			);
			return XNET_RESULT_ERROR;
		}
	#endif
	pReply = xrtWsServerReply(&Handshake);
	if ( pReply == NULL ) {
		return XNET_RESULT_ERROR;
	}
	if ( pHeaders != NULL ) {
		pHeaderSnapshot = xrtHttpHeadersClone(pHeaders);
		if ( pHeaderSnapshot == NULL ) {
			xrtHttpReplyDestroy(pReply);
			__xwsHandshakeWrap(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_OUTPUT,
				"accept-websocket-upgrade",
				"WebSocket application Header snapshot failed"
			);
			return XNET_RESULT_ERROR;
		}
	}
	iHeaderCount = xrtHttpHeadersCount(pHeaderSnapshot);
	for ( size_t i = 0; i < iHeaderCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpHeadersAt(pHeaderSnapshot, i);

		if ( (pField == NULL) ||
			__xrtWsServerHeaderReserved(pField->Name) ) {
			__xwsHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_OUTPUT,
				"accept-websocket-upgrade",
				"WebSocket application Header is invalid or reserved"
			);
			Result = XNET_RESULT_ERROR;
			goto Cleanup;
		}
		if ( !xrtHttpReplyAddHeader(
			pReply,
			pField->Name,
			pField->Value
		) ) {
			__xwsHandshakeWrap(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_OUTPUT,
				"accept-websocket-upgrade",
				"WebSocket application Header could not be appended"
			);
			Result = XNET_RESULT_ERROR;
			goto Cleanup;
		}
	}
	Result = __xrtWsServerUpgradeSubmit(
		pHttp,
		&Config,
		&Handshake,
		pReply,
		&Events,
		pEventData,
		pProc,
		pData
	);

Cleanup:
	xrtHttpHeadersDestroy(pHeaderSnapshot);
	xrtHttpReplyDestroy(pReply);
	return Result;
}



/* 验证并提交常用服务端 WebSocket Upgrade。 */
XRT_API xnetresult xrtWsUpgrade(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
)
{
	xwsserverconfig Config;
	xwsconnevents Events;
	xwsserverhandshake Handshake;
	const xhttpserverrequest* pRequest;

	if ( !__xrtWsServerConfigSnapshot(
		&Config,
		pConfig,
		"upgrade-websocket-server"
	) || !__xrtWsConnEventsSnapshot(
		&Events,
		pEvents,
		"upgrade-websocket-server"
	) || !__xrtWsHttpObjectCheck(
		pHttp,
		"upgrade-websocket-server",
		"HTTP server connection is null"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pProc == NULL ) {
		__xwsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"upgrade-websocket-server",
			"WebSocket Upgrade callback is null"
		);
		return XNET_RESULT_ERROR;
	}
	if ( !xrtWsServerConfigValid(&Config) ) {
		__xwsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"upgrade-websocket-server",
			"WebSocket server configuration is invalid"
		);
		return XNET_RESULT_ERROR;
	}
	pRequest = xrtHttpConnRequest(pHttp);
	if ( !xrtWsServerCheck(
		pRequest,
		&Config,
		&Handshake
	) ) {
		return XNET_RESULT_ERROR;
	}
	return xrtWsUpgradeAccept(
		pHttp,
		&Config,
		&Handshake,
		NULL,
		&Events,
		pEventData,
		pProc,
		pData
	);
}

#endif
