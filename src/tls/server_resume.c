#include "../internal/xrt_tls_server.h"



#if defined(XRT_FEATURE_TLS_SERVER_RESUME)

/* 比较两个允许为空的借用视图。 */
static bool __xrtTlsServerResumeViewEqual(
	xbytesview Left,
	xbytesview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 判断客户端是否声明支持当前服务端要求的 PSK+DHE 模式。 */
static bool __xrtTlsServerResumeMode(
	const xtlsclienthello* pHello,
	bool* pEnabled
)
{
	xtlsextension Extension;
	xtlsitemresult Result;
	xbytesview Modes;

	*pEnabled = false;
	Result = xrtTlsExtensionsFind(
		pHello->Extensions,
		XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES, &Extension
	);
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( Result == XTLS_ITEM_DONE ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
			"select-server-resume",
			"TLS ClientHello with PSK is missing psk_key_exchange_modes"
		);
	}
	if ( !xrtTlsPskModes(Extension.Data, &Modes) ) {
		return false;
	}
	for ( size_t i = 0; i < Modes.Size; i++ ) {
		if ( Modes.Data[i] == XTLS_PSK_DHE_KE ) {
			*pEnabled = true;
			break;
		}
	}
	return true;
}



/* 检查票据对象是否仍绑定当前 SNI、ALPN、密码摘要和线路年龄。 */
static bool __xrtTlsServerResumeMatch(
	const xtlsserverstate* pState,
	const xtlsresume* pResume,
	const xtlsresumeinfo* pInfo,
	const xtlspsk* pPsk,
	xbytesview ServerName,
	xtlscipher Cipher,
	size_t iProtocol,
	xtime iNow
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(Cipher);
	const xtlscipherinfo* pResumeCipher = xrtTlsCipherInfo(
		pInfo->Cipher
	);
	xbytesview Protocol = { NULL, 0 };
	xbytesview ResumeName = {
		(const uint8*)pInfo->ServerName.Data,
		pInfo->ServerName.Size
	};
	uint64 iActualAge;
	uint32 iClientAge;
	uint64 iDifference;

	if ( iProtocol != XTLS_SERVER_PROTOCOL_NONE ) {
		Protocol = pState->Protocols[iProtocol];
	}
	if ( (pCipher == NULL) || (pResumeCipher == NULL) ||
		(pInfo->Version != XTLS_VERSION_13) ||
		(pCipher->Hash != pResumeCipher->Hash) ||
		(pInfo->Secret.Size != pCipher->HashSize) ||
		!xrtTlsResumeValidAt(pResume, iNow) ) {
		return false;
	}
	if ( !__xrtTlsServerResumeViewEqual(
		pInfo->Ticket, pPsk->Identity
	) || !__xrtTlsServerResumeViewEqual(
		ResumeName, ServerName
	) || !__xrtTlsServerResumeViewEqual(
		pInfo->Protocol, Protocol
	) ) {
		return false;
	}
	iActualAge = (uint64)(iNow - pInfo->IssuedAt) / 1000u;
	iClientAge = pPsk->ObfuscatedAge - pInfo->AgeAdd;
	iDifference = iActualAge > iClientAge ?
		iActualAge - iClientAge : iClientAge - iActualAge;
	return iDifference <= pState->ResumeAgeTolerance;
}



/* 从完整 ClientHello 计算所有 PSK 共用的截断 transcript 长度。 */
static bool __xrtTlsServerResumePartial(
	const xtlshandshake* pMessage,
	const xtlspskcursor* pPsks,
	xbytesview* pPartial
)
{
	cbytes pStart = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	cbytes pBinders = pPsks->Binders.Data;

	if ( (pBinders < pStart + 2u) ||
		(pBinders > pStart + pMessage->EncodedSize) ) {
		return __xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"select-server-resume",
			"TLS PSK binder vector lies outside ClientHello"
		);
	}
	pPartial->Data = pStart;
	pPartial->Size = (size_t)(pBinders - pStart) - 2u;
	return true;
}



/* 查找第一张可接受票据；命中的错误 binder 必须终止而不能降级。 */
bool __xrtTlsServerResumeSelect(
	const xtlshandshake* pMessage,
	const xtlsclienthello* pHello,
	const xtlsserverstate* pState,
	xbytesview ServerName,
	xbytesview Protocols,
	xtlscipher Cipher,
	size_t iProtocol,
	xtlsserverresumeoffer* pOffer
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(Cipher);
	xtlsextension Extension;
	xtlspskcursor Psks;
	xtlspsk Psk;
	xtlsitemresult Result;
	xbytesview Partial;
	bool bMode;
	uint16 iSelected = 0;

	memset(pOffer, 0, sizeof(*pOffer));
	Result = xrtTlsExtensionsFind(
		pHello->Extensions,
		XTLS_EXTENSION_PRE_SHARED_KEY, &Extension
	);
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( Result == XTLS_ITEM_DONE ) {
		return true;
	}
	if ( (Extension.Data.Data + Extension.Data.Size) !=
		(pHello->Extensions.Data + pHello->Extensions.Size) ) {
		return __xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
			"select-server-resume",
			"TLS pre_shared_key extension is not last in ClientHello"
		);
	}
	if ( !xrtTlsClientPsks(Extension.Data, &Psks) ||
		!__xrtTlsServerResumeMode(pHello, &bMode) ||
		!__xrtTlsServerResumePartial(pMessage, &Psks, &Partial) ) {
		return false;
	}
	if ( !bMode || (pState->Resume == NULL) ) {
		return true;
	}
	while ( (Result = xrtTlsPsksRead(
		&Psks, &Psk
	)) == XTLS_ITEM_VALUE ) {
		xtlsserverresumerequest Request;
		const xtlsresume* pBorrowed;
		xtlsresume* pResume;
		xtlsresumeinfo Info;
		uint8 Expected[XTLS_SERVER_SECRET_MAX_SIZE];
		xcryptohash Hash;
		xtime iNow = xrtNow();
		bool bMatch;

		memset(Expected, 0, sizeof(Expected));
		Request.ServerName = ServerName;
		Request.Protocols = Protocols;
		Request.Ticket = Psk.Identity;
		Request.Age = Psk.ObfuscatedAge;
		pBorrowed = pState->Resume(
			pState->ResumeContext, &Request
		);
		if ( pBorrowed == NULL ) {
			if ( iSelected == UINT16_MAX ) {
				break;
			}
			iSelected++;
			continue;
		}
		pResume = xrtTlsResumeRetain(pBorrowed);
		if ( pResume == NULL ) {
			return __xrtTlsServerCause(
				"select-server-resume",
				"TLS server could not retain resume lookup result"
			);
		}
		bMatch = xrtTlsResumeInfo(pResume, &Info) &&
			__xrtTlsServerResumeMatch(
				pState, pResume, &Info, &Psk, ServerName,
				Cipher, iProtocol, iNow
			);
		if ( bMatch && ((pCipher == NULL) ||
			(Psk.Binder.Size != pCipher->HashSize)) ) {
			bMatch = false;
		}
		if ( bMatch ) {
			Hash = __xrtTlsHash(pCipher->Hash);
			bMatch = __xrtTls13ResumptionBinder(
				Hash, Info.Secret, Partial,
				Expected, pCipher->HashSize
			);
			if ( bMatch && !xrtConstTimeEqual(
				Expected, Psk.Binder.Data, pCipher->HashSize
			) ) {
				xrtSecureZero(Expected, sizeof(Expected));
				xrtTlsResumeRelease(pResume);
				return __xrtTlsServerError(
					XERR_PROTOCOL, XTLS_ERROR_VERIFY,
					"select-server-resume",
					"TLS client PSK binder verification failed"
				);
			}
		}
		if ( bMatch ) {
			memcpy(
				pOffer->Secret, Info.Secret.Data, Info.Secret.Size
			);
			pOffer->Selected = iSelected;
			pOffer->Resumed = true;
		}
		xrtSecureZero(Expected, sizeof(Expected));
		xrtTlsResumeRelease(pResume);
		if ( pOffer->Resumed ) {
			return true;
		}
		if ( iSelected == UINT16_MAX ) {
			break;
		}
		iSelected++;
	}
	return Result == XTLS_ITEM_DONE;
}



/* 验证服务端会话可签发票据，并返回内部角色状态。 */
static xtlsserverstate* __xrtTlsServerTicketState(
	xtlssession* pSession,
	xtlsresume** ppResume,
	cstr sOperation
)
{
	xtlsserverstate* pState;

	if ( ppResume != NULL ) {
		*ppResume = NULL;
	}
	if ( (pSession == NULL) || (ppResume == NULL) ) {
		(void)__xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, sOperation,
			"TLS server ticket session or output is null"
		);
		return NULL;
	}
	if ( xrtTlsSessionRole(pSession) != XTLS_SERVER ) {
		(void)__xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, sOperation,
			"TLS session is not a server"
		);
		return NULL;
	}
	pState = (xtlsserverstate*)__xrtTlsSessionRoleData(pSession);
	if ( (xrtTlsSessionState(pSession) != XTLS_STATE_READY) ||
		(pState == NULL) || (pState->Step != XTLS_SERVER_READY) ) {
		(void)__xrtTlsServerError(
			XERR_STATE, XTLS_ERROR_STATE, sOperation,
			"TLS server session is not ready to issue a ticket"
		);
		return NULL;
	}
	if ( xrtTlsSessionVersion(pSession) != XTLS_VERSION_13 ) {
		(void)__xrtTlsServerError(
			XERR_UNSUPPORTED, XTLS_ERROR_VERSION, sOperation,
			"TLS session tickets require a TLS 1.3 session"
		);
		return NULL;
	}
	if ( !pState->ResumptionReady ) {
		(void)__xrtTlsServerError(
			XERR_STATE, XTLS_ERROR_STATE, sOperation,
			"TLS server session is not ready to issue a ticket"
		);
		return NULL;
	}
	return pState;
}



/* 派生票据 PSK、排队 NewSessionTicket，并把匹配对象交给缓存所有者。 */
XRT_API xtlsresult xrtTlsServerTicket(
	xtlssession* pSession,
	xbytesview Ticket,
	uint32 iLifetime,
	xtlsresume** ppResume
)
{
	xtlsserverstate* pState = __xrtTlsServerTicketState(
		pSession, ppResume, "issue-tls-server-ticket"
	);
	const xtlscipherinfo* pCipher;
	xtlssessionticket Message;
	xtlsresumeconfig Config;
	xtlsresume* pResume = NULL;
	bytes pEncoded = NULL;
	uint8 Nonce[8];
	uint8 AgeBytes[4];
	uint8 Secret[XTLS_SERVER_SECRET_MAX_SIZE];
	size_t iBody;
	size_t iMessage;
	xtlsresult Writable;
	xtlsresult Result = XTLS_ERROR;

	memset(&Message, 0, sizeof(Message));
	memset(Nonce, 0, sizeof(Nonce));
	memset(AgeBytes, 0, sizeof(AgeBytes));
	memset(Secret, 0, sizeof(Secret));
	if ( pState == NULL ) {
		goto cleanup;
	}
	if ( (Ticket.Data == NULL) || (Ticket.Size == 0) ||
		(Ticket.Size > UINT16_MAX) || (iLifetime == 0) ||
		(iLifetime > XTLS13_TICKET_LIFETIME_MAX) ) {
		(void)__xrtTlsServerError(
			XERR_VALUE, XTLS_ERROR_RESUME,
			"issue-tls-server-ticket",
			"TLS server ticket or lifetime is invalid"
		);
		goto cleanup;
	}
	pCipher = xrtTlsCipherInfo(pState->Cipher);
	if ( (pCipher == NULL) ||
		(pCipher->HashSize != pState->HashSize) ) {
		(void)__xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"issue-tls-server-ticket",
			"TLS server ticket cipher state is inconsistent"
		);
		goto cleanup;
	}
	Message.Version = XTLS_VERSION_13;
	Message.Lifetime = iLifetime;
	Message.Nonce = (xbytesview) { Nonce, sizeof(Nonce) };
	Message.Ticket = Ticket;
	Message.Extensions = (xbytesview) { NULL, 0 };
	iBody = xrtTlsSessionTicketSize(&Message);
	iMessage = xrtTlsHandshakeSize(iBody);
	if ( (iBody == 0) || (iMessage == 0) ) {
		goto cleanup;
	}
	Writable = __xrtTlsSessionRecordWritable(
		pSession, iMessage, 0, "issue-tls-server-ticket"
	);
	if ( Writable != XTLS_OK ) {
		Result = Writable;
		if ( (Writable == XTLS_AGAIN) &&
			!__xrtTlsServerWait(pSession, true) ) {
			Result = XTLS_ERROR;
		}
		goto cleanup;
	}
	if ( !xrtSecureRandom(Nonce, sizeof(Nonce)) ||
		!xrtSecureRandom(AgeBytes, sizeof(AgeBytes)) ||
		!__xrtTls13ExpandLabel(
			__xrtTlsHash(pCipher->Hash),
			(xbytesview) {
				pState->ResumptionMaster, pState->HashSize
			}, XRT_STR_LITERAL("resumption"),
			(xbytesview) { Nonce, sizeof(Nonce) },
			Secret, pState->HashSize
		) ) {
		(void)__xrtTlsServerCause(
			"issue-tls-server-ticket",
			"TLS server ticket secret derivation failed"
		);
		goto cleanup;
	}
	xrtTlsResumeConfigInit(&Config);
	Config.Cipher = pState->Cipher;
	Config.Ticket = Ticket;
	Config.Secret = (xbytesview) { Secret, pState->HashSize };
	Config.ServerName = (xstrview) {
		(const char*)pState->ServerName.Data,
		pState->ServerName.Size
	};
	Config.Protocol = pSession->Protocol;
	Config.Lifetime = iLifetime;
	Config.AgeAdd = __xrtTlsRead32(AgeBytes);
	pResume = xrtTlsResumeCreate(&Config);
	if ( pResume == NULL ) {
		goto cleanup;
	}
	Message.AgeAdd = Config.AgeAdd;
	pEncoded = (bytes)xrtMalloc(iMessage);
	if ( pEncoded == NULL ) {
		(void)__xrtTlsServerCause(
			"issue-tls-server-ticket",
			"TLS server ticket message allocation failed"
		);
		goto cleanup;
	}
	if ( !xrtTlsSessionTicketEncode(
		&Message, pEncoded + XTLS_HANDSHAKE_HEADER_SIZE, iBody
	) ) {
		goto cleanup;
	}
	pEncoded[0] = (uint8)XTLS_HANDSHAKE_NEW_SESSION_TICKET;
	__xrtTlsWrite24(pEncoded + 1u, (uint32)iBody);
	Result = __xrtTlsSessionRecordProtect(
		pSession, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { pEncoded, iMessage }, 0
	);
	if ( Result != XTLS_OK ) {
		goto cleanup;
	}
	if ( !__xrtTlsServerWait(pSession, true) ) {
		Result = XTLS_ERROR;
		goto cleanup;
	}
	*ppResume = pResume;
	pResume = NULL;

cleanup:
	if ( pEncoded != NULL ) {
		xrtSecureZero(pEncoded, iMessage);
		xrtFree(pEncoded);
	}
	xrtTlsResumeRelease(pResume);
	xrtSecureZero(Secret, sizeof(Secret));
	xrtSecureZero(AgeBytes, sizeof(AgeBytes));
	xrtSecureZero(Nonce, sizeof(Nonce));
	return Result;
}



/* 为常见内存缓存生成 256 位随机票据并使用一天有效期。 */
XRT_API xtlsresult xrtTlsServerTicketNew(
	xtlssession* pSession,
	xtlsresume** ppResume
)
{
	uint8 Ticket[XTLS_SERVER_TICKET_SIZE_DEFAULT];
	xtlsresult Result;

	if ( ppResume != NULL ) {
		*ppResume = NULL;
	}
	if ( (ppResume == NULL) ||
		!xrtSecureRandom(Ticket, sizeof(Ticket)) ) {
		if ( ppResume == NULL ) {
			(void)__xrtTlsServerError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"issue-new-tls-server-ticket",
				"TLS server ticket output is null"
			);
		}
		xrtSecureZero(Ticket, sizeof(Ticket));
		return XTLS_ERROR;
	}
	Result = xrtTlsServerTicket(
		pSession, (xbytesview) { Ticket, sizeof(Ticket) },
		XTLS_SERVER_TICKET_LIFETIME_DEFAULT, ppResume
	);
	xrtSecureZero(Ticket, sizeof(Ticket));
	return Result;
}



/* 查询服务端握手是否接受了本次 ClientHello 中的票据。 */
XRT_API bool xrtTlsServerResumed(const xtlssession* pSession)
{
	const xtlsserverstate* pState;

	if ( pSession == NULL ) {
		return __xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"get-tls-server-resumed", "TLS server session is null"
		);
	}
	if ( xrtTlsSessionRole(pSession) != XTLS_SERVER ) {
		return __xrtTlsServerError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"get-tls-server-resumed", "TLS session is not a server"
		);
	}
	pState = (const xtlsserverstate*)__xrtTlsSessionRoleData(
		(xtlssession*)pSession
	);
	return (pState != NULL) && pState->Resumed;
}

#endif
