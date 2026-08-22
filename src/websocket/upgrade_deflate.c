#include "../internal/xrt_websocket_upgrade.h"



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)

typedef bool (*__xrt_ws_upgrade_extension_proc)(
	const xwsextension* pExtension,
	ptr pData
);



/* 包装扩展和压缩解析错误为稳定握手错误。 */
static bool __xrtWsUpgradeDeflateError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtWsHandshakeWrap(
		Kind,
		XWS_HANDSHAKE_ERROR_EXTENSION,
		sOperation,
		sMessage
	);
	return false;
}



/* 按线路顺序遍历全部重复扩展字段。 */
static bool __xrtWsUpgradeExtensionsVisit(
	const xhttpfield* pFields,
	size_t iCount,
	__xrt_ws_upgrade_extension_proc pProc,
	ptr pData
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		xhttpfield Field;
		size_t iOffset = 0;

		__xrtWsUpgradeFieldRead(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Sec-WebSocket-Extensions")
		) ) {
			continue;
		}
		for ( ;; ) {
			xwsextension Extension;
			xhttpnext Next = xrtWsExtensionNext(
				Field.Value,
				&iOffset,
				&Extension
			);

			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			if ( Next != XHTTP_NEXT_ITEM ) {
				return false;
			}
			if ( !pProc(&Extension, pData) ) {
				return false;
			}
		}
	}
	return true;
}



/* 仅用于完整语法验证的扩展访问器。 */
static bool __xrtWsUpgradeExtensionValid(
	const xwsextension* pExtension,
	ptr pData
)
{
	(void)pExtension;
	(void)pData;
	return true;
}



/* 验证一个完整扩展字段值。 */
bool __xrtWsUpgradeExtensionsValid(xstrview Extensions)
{
	xwsextension Extension;
	size_t iOffset = 0;
	xhttpnext Next;
	size_t iCount = 0;

	for ( ;; ) {
		Next = xrtWsExtensionNext(
			Extensions,
			&iOffset,
			&Extension
		);
		if ( Next == XHTTP_NEXT_END ) {
			return iCount != 0;
		}
		if ( Next != XHTTP_NEXT_ITEM ) {
			return false;
		}
		if ( !__xrtWsUpgradeExtensionValid(
			&Extension,
			NULL
		) ) {
			return false;
		}
		if ( iCount == SIZE_MAX ) {
			return false;
		}
		iCount++;
	}
}



typedef struct __xrt_ws_upgrade_server_deflate {
	xwsdeflate Offer;
	bool Found;
} __xrt_ws_upgrade_server_deflate;



/* 服务端只提取唯一的 permessage-deflate，未知扩展由上层策略忽略。 */
static bool __xrtWsUpgradeServerDeflateVisit(
	const xwsextension* pExtension,
	ptr pData
)
{
	__xrt_ws_upgrade_server_deflate* pDeflate =
		(__xrt_ws_upgrade_server_deflate*)pData;

	if ( !xrtWsDeflateIs(pExtension) ) {
		return true;
	}
	if ( pDeflate->Found ) {
		__xrtWsHandshakeError(
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
		return __xrtWsUpgradeDeflateError(
			XERR_PROTOCOL,
			"check-websocket-request",
			"WebSocket permessage-deflate offer is invalid"
		);
	}
	pDeflate->Found = true;
	return true;
}



/* 执行默认协商和可选服务端策略。 */
static bool __xrtWsUpgradeServerDeflateAccept(
	const xwsupgradeserverconfig* pConfig,
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	bool* pAccepted
)
{
	xerror* pPrevious;
	xerror* pCallbackError;
	bool bAccepted;

	if ( !xrtWsDeflateAccept(pOffer, pResponse) ) {
		return __xrtWsUpgradeDeflateError(
			XERR_PROTOCOL,
			"check-websocket-request",
			"WebSocket permessage-deflate offer cannot be accepted"
		);
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
		(void)__xrtWsUpgradeDeflateError(
			XERR_PROTOCOL,
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



/* 服务端协商唯一的 permessage-deflate 响应。 */
bool __xrtWsUpgradeServerDeflate(
	const xhttpfield* pFields,
	size_t iCount,
	const xwsupgradeserverconfig* pConfig,
	xwsupgrade* pUpgrade
)
{
	__xrt_ws_upgrade_server_deflate Deflate;
	xwsdeflate Response;
	size_t iSize = 0;
	bool bAccepted;

	if ( !pConfig->EnableDeflate ) {
		return true;
	}
	memset(&Deflate, 0, sizeof(Deflate));
	if ( !__xrtWsUpgradeExtensionsVisit(
		pFields,
		iCount,
		__xrtWsUpgradeServerDeflateVisit,
		&Deflate
	) ) {
		return __xrtWsUpgradeDeflateError(
			XERR_PROTOCOL,
			"check-websocket-request",
			"WebSocket extension offer is invalid"
		);
	}
	if ( !Deflate.Found ) {
		if ( pConfig->RequireDeflate ) {
			__xrtWsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-request",
				"WebSocket request omitted required compression"
			);
			return false;
		}
		return true;
	}
	if ( !__xrtWsUpgradeServerDeflateAccept(
		pConfig,
		&Deflate.Offer,
		&Response,
		&bAccepted
	) ) {
		return false;
	}
	if ( !bAccepted ) {
		if ( pConfig->RequireDeflate ) {
			__xrtWsHandshakeError(
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
		pUpgrade->Extensions,
		XWS_DEFLATE_MAX_SIZE,
		&iSize
	) ) {
		return __xrtWsUpgradeDeflateError(
			XERR_PROTOCOL,
			"check-websocket-request",
			"WebSocket compression policy produced an invalid response"
		);
	}
	pUpgrade->Extensions[iSize] = '\0';
	pUpgrade->ExtensionSize = iSize;
	pUpgrade->Deflate = Response;
	pUpgrade->DeflateEnabled = true;
	return true;
}



typedef struct __xrt_ws_upgrade_client_deflate {
	const xwsdeflate* Offer;
	xwsdeflate Response;
	bool Found;
} __xrt_ws_upgrade_client_deflate;



/* 客户端只接受唯一且由本次 offer 允许的 permessage-deflate。 */
static bool __xrtWsUpgradeClientDeflateVisit(
	const xwsextension* pExtension,
	ptr pData
)
{
	__xrt_ws_upgrade_client_deflate* pDeflate =
		(__xrt_ws_upgrade_client_deflate*)pData;

	if ( !xrtWsDeflateIs(pExtension) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-response",
			"WebSocket response selected an unsupported extension"
		);
		return false;
	}
	if ( pDeflate->Found ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-response",
			"WebSocket response repeats permessage-deflate"
		);
		return false;
	}
	if ( !xrtWsDeflateResponseParse(
		pExtension,
		&pDeflate->Response
	) || !xrtWsDeflateResponseCheck(
		pDeflate->Offer,
		&pDeflate->Response
	) ) {
		return __xrtWsUpgradeDeflateError(
			XERR_PROTOCOL,
			"check-websocket-response",
			"WebSocket permessage-deflate response violates the offer"
		);
	}
	pDeflate->Found = true;
	return true;
}



/* 客户端验证服务器选择的全部扩展。 */
bool __xrtWsUpgradeClientDeflate(
	const xhttpfield* pFields,
	size_t iCount,
	const xwsupgradeclientconfig* pConfig,
	xwsupgrade* pUpgrade
)
{
	__xrt_ws_upgrade_client_deflate Deflate;
	size_t iSize = 0;
	size_t iFields = xrtHttpFieldCount(
		pFields,
		iCount,
		XRT_STR_LITERAL("Sec-WebSocket-Extensions")
	);

	if ( !pConfig->EnableDeflate ) {
		if ( iFields != 0 ) {
			__xrtWsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"WebSocket response selected an unoffered extension"
			);
			return false;
		}
		return true;
	}
	memset(&Deflate, 0, sizeof(Deflate));
	Deflate.Offer = &pConfig->Deflate;
	if ( !__xrtWsUpgradeExtensionsVisit(
		pFields,
		iCount,
		__xrtWsUpgradeClientDeflateVisit,
		&Deflate
	) ) {
		return __xrtWsUpgradeDeflateError(
			XERR_PROTOCOL,
			"check-websocket-response",
			"WebSocket extension response is invalid"
		);
	}
	if ( !Deflate.Found ) {
		if ( pConfig->RequireDeflate ) {
			__xrtWsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"WebSocket response omitted required compression"
			);
			return false;
		}
		return true;
	}
	if ( !xrtWsDeflateResponseWrite(
		&Deflate.Response,
		pUpgrade->Extensions,
		XWS_DEFLATE_MAX_SIZE,
		&iSize
	) ) {
		return __xrtWsUpgradeDeflateError(
			XERR_PROTOCOL,
			"check-websocket-response",
			"WebSocket permessage-deflate response cannot be normalized"
		);
	}
	pUpgrade->Extensions[iSize] = '\0';
	pUpgrade->ExtensionSize = iSize;
	pUpgrade->Deflate = Deflate.Response;
	pUpgrade->DeflateEnabled = true;
	return true;
}

#endif
