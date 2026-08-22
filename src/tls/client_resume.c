#include "../internal/xrt_tls_client.h"



#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)

/* 验证公共查询确实指向带恢复能力的客户端会话。 */
static xtlsclientstate* __xrtTlsClientResumeState(
	const xtlssession* pSession,
	cstr sOperation
)
{
	if ( pSession == NULL ) {
		(void)__xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation, "TLS client session is null"
		);
		return NULL;
	}
	if ( (pSession->Role != XTLS_CLIENT) ||
		(pSession->AllocationSize <= sizeof(*pSession)) ) {
		(void)__xrtTlsClientError(
			XERR_STATE, XTLS_ERROR_STATE,
			sOperation, "TLS session is not a resumable client"
		);
		return NULL;
	}
	return (xtlsclientstate*)(pSession + 1);
}



/* 比较恢复对象与线路字段，不对零长度空指针执行库调用。 */
static bool __xrtTlsClientResumeViewEqual(
	xbytesview Left,
	xbytesview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 为最终 ClientHello 的唯一外部 PSK 计算并原位写入 res binder。 */
bool __xrtTlsClientResumeBinder(xtlsclientstate* pState)
{
	uint8 Binder[XTLS_CLIENT_SECRET_MAX_SIZE];
	const xtlscipherinfo* pCipher;
	xtlsresumeinfo Resume;
	xtlshandshake Handshake;
	xtlsclienthello Hello;
	xtlsextensioncursor Extensions;
	xtlsextension Extension;
	xtlspskcursor Psks;
	xtlspsk Psk;
	xcryptohash Hash;
	size_t iPartial;
	bool bPsk = false;
	bool bResult = false;

	memset(Binder, 0, sizeof(Binder));
	if ( (pState == NULL) || (pState->OfferResume == NULL) ||
		!xrtTlsResumeInfo(pState->OfferResume, &Resume) ) {
		(void)__xrtTlsClientError(
			XERR_STATE, XTLS_ERROR_STATE, "write-tls-client-binder",
			"TLS client resume state is not initialized"
		);
		goto cleanup;
	}
	pCipher = xrtTlsCipherInfo(Resume.Cipher);
	if ( (pCipher == NULL) ||
		(pCipher->HashSize != Resume.Secret.Size) ||
		(Resume.Secret.Size > sizeof(Binder)) ) {
		(void)__xrtTlsClientError(
			XERR_VALUE, XTLS_ERROR_CIPHER, "write-tls-client-binder",
			"TLS client resume cipher and PSK are inconsistent"
		);
		goto cleanup;
	}
	if ( (xrtTlsHandshakeParse(
		(xbytesview) { pState->ClientHello, pState->ClientHelloSize },
		&Handshake, NULL
	) != XTLS_OK) ||
		(Handshake.Type != XTLS_HANDSHAKE_CLIENT_HELLO) ||
		(Handshake.EncodedSize != pState->ClientHelloSize) ||
		!xrtTlsClientHelloParse(Handshake.Body, &Hello) ||
		!xrtTlsExtensionsInit(&Extensions, Hello.Extensions) ) {
		goto cleanup;
	}
	while ( xrtTlsExtensionsRead(
		&Extensions, &Extension
	) == XTLS_ITEM_VALUE ) {
		if ( Extension.Type == XTLS_EXTENSION_PRE_SHARED_KEY ) {
			if ( (Extensions.Offset != Extensions.Data.Size) ||
				!xrtTlsClientPsks(Extension.Data, &Psks) ||
				(xrtTlsPsksRead(&Psks, &Psk) != XTLS_ITEM_VALUE) ||
				(xrtTlsPsksRead(&Psks, &Psk) != XTLS_ITEM_DONE) ) {
				(void)__xrtTlsClientError(
					XERR_INTERNAL, XTLS_ERROR_EXTENSION,
					"write-tls-client-binder",
					"TLS client PSK extension is not the final single offer"
				);
				goto cleanup;
			}
			bPsk = true;
			break;
		}
	}
	if ( !bPsk ||
		!__xrtTlsClientResumeViewEqual(Psk.Identity, Resume.Ticket) ||
		(Psk.ObfuscatedAge != pState->ResumeAge) ||
		(Psk.Binder.Size != Resume.Secret.Size) ||
		(Psk.Binder.Data < (const uint8*)pState->ClientHello + 3u) ||
		(Psk.Binder.Data + Psk.Binder.Size !=
		 (const uint8*)pState->ClientHello + pState->ClientHelloSize) ) {
		(void)__xrtTlsClientError(
			XERR_INTERNAL, XTLS_ERROR_EXTENSION, "write-tls-client-binder",
			"TLS client PSK binder layout does not match its resume object"
		);
		goto cleanup;
	}
	iPartial = (size_t)(Psk.Binder.Data -
		(const uint8*)pState->ClientHello) - 3u;
	Hash = __xrtTlsHash(pCipher->Hash);
	if ( !__xrtTls13ResumptionBinder(
		Hash, Resume.Secret,
		(xbytesview) { pState->ClientHello, iPartial },
		Binder, Resume.Secret.Size
	) ) {
		goto cleanup;
	}
	memcpy((bytes)Psk.Binder.Data, Binder, Resume.Secret.Size);
	bResult = true;

cleanup:
	xrtSecureZero(Binder, sizeof(Binder));
	return bResult;
}



/* 从已验证的 ticket 扩展中读取可选 0-RTT 字节上限。 */
static bool __xrtTlsClientResumeEarlyData(
	xbytesview Extensions,
	uint32* pMaxEarlyData
)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;
	uint32 iMaximum = 0;

	if ( !xrtTlsExtensionsInit(&Cursor, Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		if ( Extension.Type == XTLS_EXTENSION_EARLY_DATA ) {
			iMaximum = __xrtTlsRead32(Extension.Data.Data);
		}
	}
	if ( Result != XTLS_ITEM_DONE ) {
		return false;
	}
	*pMaxEarlyData = iMaximum;
	return true;
}



/* 把新对象加入有界环；满队列总是保留更新的票据。 */
static void __xrtTlsClientResumeQueue(
	xtlsclientstate* pState,
	xtlsresume* pResume
)
{
	size_t iTail;

	if ( pState->ResumeLimit == 0 ) {
		pState->ResumeDropped++;
		xrtTlsResumeRelease(pResume);
		return;
	}
	if ( pState->ResumeCount == pState->ResumeLimit ) {
		xrtTlsResumeRelease(pState->Resumes[pState->ResumeHead]);
		pState->Resumes[pState->ResumeHead] = NULL;
		pState->ResumeHead = (pState->ResumeHead + 1u) %
			pState->ResumeLimit;
		pState->ResumeCount--;
		pState->ResumeDropped++;
	}
	iTail = (pState->ResumeHead + pState->ResumeCount) %
		pState->ResumeLimit;
	pState->Resumes[iTail] = pResume;
	pState->ResumeCount++;
	pState->ResumePublished++;
}



/* 组合层用发布序号观察满队列替换，不把内部环暴露为公共 API。 */
uint64 __xrtTlsClientResumePublished(
	const xtlssession* pSession
)
{
	xtlsclientstate* pState;

	if ( (pSession == NULL) ||
		(pSession->Role != XTLS_CLIENT) ||
		(pSession->AllocationSize <= sizeof(*pSession)) ) {
		return 0;
	}
	pState = (xtlsclientstate*)(pSession + 1);
	return pState->ResumePublished;
}



/* 从恢复主密钥派生票据 PSK，并发布不可变恢复对象。 */
bool __xrtTlsClientResumePublish(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlssessionticket* pTicket
)
{
	xtlsresumeconfig Config;
	xtlsresumeinfo Previous;
	xtlsresume* pResume;
	const xtlscipherinfo* pCipher;
	xcryptohash Hash;
	uint8 Secret[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint32 iMaxEarlyData;
	bool bDerived;

	memset(Secret, 0, sizeof(Secret));
	if ( (pSession == NULL) || (pState == NULL) || (pTicket == NULL) ||
		(pState->Step != XTLS_CLIENT_READY) || !pState->ResumptionReady ||
		(pState->HashSize == 0) ||
		(pState->HashSize > sizeof(Secret)) ) {
		return __xrtTlsClientError(
			XERR_STATE, XTLS_ERROR_STATE,
			"publish-tls-client-resume",
			"TLS client resumption key schedule is not ready"
		);
	}
	if ( pTicket->Lifetime == 0 ) {
		return true;
	}
	if ( !__xrtTlsClientResumeEarlyData(
		pTicket->Extensions, &iMaxEarlyData
	) ) {
		return false;
	}
	if ( pState->ResumeLimit == 0 ) {
		pState->ResumeDropped++;
		return true;
	}
	pCipher = xrtTlsCipherInfo(pState->Cipher);
	if ( pCipher == NULL ) {
		return __xrtTlsClientError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"publish-tls-client-resume",
			"TLS client resume cipher metadata is missing"
		);
	}
	Hash = __xrtTlsHash(pCipher->Hash);
	bDerived = __xrtTls13ExpandLabel(
		Hash,
		(xbytesview) {
			pState->ResumptionMaster, pState->HashSize
		}, XRT_STR_LITERAL("resumption"), pTicket->Nonce,
		Secret, pState->HashSize
	);
	if ( !bDerived ) {
		xrtSecureZero(Secret, sizeof(Secret));
		return false;
	}

	xrtTlsResumeConfigInit(&Config);
	Config.Cipher = pState->Cipher;
	Config.Ticket = pTicket->Ticket;
	Config.Secret = (xbytesview) { Secret, pState->HashSize };
	Config.ServerName = (xstrview) {
		(const char*)pState->ServerName.Data,
		pState->ServerName.Size
	};
	Config.Protocol = pSession->Protocol;
	if ( pState->Resumed ) {
		if ( !xrtTlsResumeInfo(pState->OfferResume, &Previous) ) {
			xrtSecureZero(Secret, sizeof(Secret));
			return false;
		}
		Config.PeerIdentity = Previous.PeerIdentity;
	} else {
		Config.PeerIdentity = (xbytesview) {
			pState->PeerIdentity, sizeof(pState->PeerIdentity)
		};
	}
	Config.Lifetime = pTicket->Lifetime;
	Config.AgeAdd = pTicket->AgeAdd;
	Config.MaxEarlyData = iMaxEarlyData;
	pResume = xrtTlsResumeCreate(&Config);
	xrtSecureZero(Secret, sizeof(Secret));
	if ( pResume == NULL ) {
		if ( xrtErrorIs(xrtGetError(), XERR_MEMORY) != NULL ) {
			pState->ResumeDropped++;
			xrtClearError();
			return true;
		}
		return false;
	}
	__xrtTlsClientResumeQueue(pState, pResume);
	return true;
}



/* 只有服务端在 ServerHello 中接受 identity 0 后才报告恢复成功。 */
XRT_API bool xrtTlsClientResumed(const xtlssession* pSession)
{
	xtlsclientstate* pState = __xrtTlsClientResumeState(
		pSession, "get-tls-client-resumed"
	);

	return (pState != NULL) && pState->Resumed;
}



/* 释放环中仍由会话拥有的全部恢复对象。 */
void __xrtTlsClientResumeClear(xtlsclientstate* pState)
{
	if ( pState == NULL ) {
		return;
	}
	while ( pState->ResumeCount != 0 ) {
		xtlsresume* pResume = pState->Resumes[pState->ResumeHead];

		pState->Resumes[pState->ResumeHead] = NULL;
		pState->ResumeHead = (pState->ResumeHead + 1u) %
			pState->ResumeLimit;
		pState->ResumeCount--;
		xrtTlsResumeRelease(pResume);
	}
	pState->ResumeHead = 0;
}



/* 返回当前有界环中的票据数量。 */
XRT_API size_t xrtTlsClientResumeCount(const xtlssession* pSession)
{
	xtlsclientstate* pState = __xrtTlsClientResumeState(
		pSession, "count-tls-client-resume"
	);

	return pState != NULL ? pState->ResumeCount : 0;
}



/* 返回显式禁用、队列淘汰或可选缓存 OOM 的累计票据数量。 */
XRT_API uint64 xrtTlsClientResumeDropped(const xtlssession* pSession)
{
	xtlsclientstate* pState = __xrtTlsClientResumeState(
		pSession, "get-tls-client-resume-dropped"
	);

	return pState != NULL ? pState->ResumeDropped : 0;
}



/* 从队首移除对象并把会话持有的引用转移给调用方。 */
XRT_API xtlsresume* xrtTlsClientTakeResume(xtlssession* pSession)
{
	xtlsclientstate* pState = __xrtTlsClientResumeState(
		pSession, "take-tls-client-resume"
	);
	xtlsresume* pResume;

	if ( (pState == NULL) || (pState->ResumeCount == 0) ) {
		return NULL;
	}
	pResume = pState->Resumes[pState->ResumeHead];
	pState->Resumes[pState->ResumeHead] = NULL;
	pState->ResumeHead = (pState->ResumeHead + 1u) %
		pState->ResumeLimit;
	pState->ResumeCount--;
	if ( pState->ResumeCount == 0 ) {
		pState->ResumeHead = 0;
	}
	return pResume;
}

#endif
