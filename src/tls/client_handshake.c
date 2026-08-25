#include "../internal/xrt_tls_client.h"



#if defined(XRT_FEATURE_TLS_CLIENT)

#define XTLS_CLIENT_SHARED_MAX_SIZE 56u
#define XTLS_CLIENT_KEY_MAX_SIZE 32u
#define XTLS_CLIENT_IV_MAX_SIZE 12u



/* ServerHello 临时结果只在全部验证和派生成功后提交到会话。 */
typedef struct xtlsclientserverstate {
	xtlstranscript Transcript;
	xtlscipher Cipher;
	xcryptohash Hash;
	bool Resumed;
	size_t HashSize;
	size_t KeySize;
	size_t IvSize;
	uint8 HandshakeSecret[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 ClientTraffic[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 ServerTraffic[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 ReadKey[XTLS_CLIENT_KEY_MAX_SIZE];
	uint8 ReadIv[XTLS_CLIENT_IV_MAX_SIZE];
	uint8 WriteKey[XTLS_CLIENT_KEY_MAX_SIZE];
	uint8 WriteIv[XTLS_CLIENT_IV_MAX_SIZE];
} xtlsclientserverstate;



/* Finished 临时状态在客户端响应成功排队前不修改会话 epoch。 */
typedef struct xtlsclientfinishedstate {
	xtlstranscript Transcript;
	xtlsrecordkey ReadKey;
	xtlsrecordkey WriteKey;
	uint8 ClientTraffic[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 ServerTraffic[XTLS_CLIENT_SECRET_MAX_SIZE];
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		uint8 ResumptionMaster[XTLS_CLIENT_SECRET_MAX_SIZE];
	#endif
	uint8 Message[XTLS_HANDSHAKE_HEADER_SIZE + XTLS_CLIENT_SECRET_MAX_SIZE];
	size_t MessageSize;
} xtlsclientfinishedstate;



/* 把当前客户端会话推进到失败终态，同时保留已经设置的根错误。 */
static xtlsresult __xrtTlsClientFailed(xtlssession* pSession)
{
	return __xrtTlsSessionFail(pSession);
}



/* 设置客户端协议错误并进入失败终态。 */
xtlsresult __xrtTlsClientProtocol(
	xtlssession* pSession,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	(void)__xrtTlsClientError(
		XERR_PROTOCOL, Code, sOperation, sMessage
	);
	return __xrtTlsClientFailed(pSession);
}



/* 检查服务端选择是否确实存在于线路 offer 中。 */
bool __xrtTlsClientOffered(
	const uint16* pValues,
	size_t iCount,
	uint16 iValue
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pValues[i] == iValue ) {
			return true;
		}
	}
	return false;
}



/* 根据发送队列和是否缺少输入发布精确等待方向。 */
bool __xrtTlsClientWait(
	xtlssession* pSession,
	bool bInput
)
{
	uint32 iWait = XTLS_WAIT_NONE;

	if ( bInput && !pSession->CloseReceived && !pSession->TransportEof ) {
		iWait |= XTLS_WAIT_INPUT;
	}
	if ( xrtTlsSessionSendSize(pSession) != 0 ) {
		iWait |= XTLS_WAIT_OUTPUT;
	}
	return __xrtTlsSessionSetWait(pSession, iWait);
}



/* 明文队列达到硬上限时等待应用消费，同时保留待发密文方向。 */
static bool __xrtTlsClientApplicationWait(xtlssession* pSession)
{
	uint32 iWait = XTLS_WAIT_APPLICATION;

	if ( xrtTlsSessionSendSize(pSession) != 0 ) {
		iWait |= XTLS_WAIT_OUTPUT;
	}
	return __xrtTlsSessionSetWait(pSession, iWait);
}



/* 当前实现到达尚未落地的后续握手步骤时暂停并保留线路后缀。 */
static bool __xrtTlsClientPaused(const xtlsclientstate* pState)
{
	#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
		(void)pState;
		return false;
	#else
		return pState->Step == XTLS_CLIENT_WAIT_CERTIFICATE;
	#endif
}



/* 校验普通 TLS 1.3 ServerHello 的选择和扩展集合。 */
static bool __xrtTlsClientServerHello(
	const xtlsclientstate* pState,
	const xtlsserverhello* pHello,
	const xtlscipherinfo** ppCipher,
	xtlskeyshare* pShare,
	bool* pResumed
)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;
	const xtlscipherinfo* pCipher;
	bool bVersion = false;
	bool bShare = false;
	bool bResumed = false;

	*ppCipher = NULL;
	memset(pShare, 0, sizeof(*pShare));
	*pResumed = false;
	if ( pHello->Retry ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"process-server-hello",
			"TLS client expected a normal ServerHello"
		);
	}
	if ( (pHello->SessionId.Size != sizeof(pState->SessionId)) ||
		(memcmp(
			pHello->SessionId.Data, pState->SessionId,
			sizeof(pState->SessionId)
		) != 0) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"process-server-hello",
			"TLS ServerHello did not echo the client session identifier"
		);
	}
	if ( !__xrtTlsClientOffered(
		pState->Ciphers, pState->CipherCount, pHello->CipherSuite
	) || (pState->RetrySeen &&
		(pHello->CipherSuite != (uint16)pState->Cipher)) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_CIPHER,
			"process-server-hello",
			"TLS server selected a cipher that the client did not offer"
		);
	}
	pCipher = xrtTlsCipherInfo((xtlscipher)pHello->CipherSuite);
	if ( (pCipher == NULL) || (pCipher->Version != XTLS_VERSION_13) ||
		(pCipher->HashSize > pState->SecretCapacity) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_CIPHER,
			"process-server-hello",
			"TLS server selected an unusable TLS 1.3 cipher"
		);
	}
	if ( !xrtTlsExtensionsInit(&Cursor, pHello->Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		if ( Extension.Type == XTLS_EXTENSION_SUPPORTED_VERSIONS ) {
			uint16 iVersion;

			if ( !xrtTlsServerVersion(Extension.Data, &iVersion) ) {
				return false;
			}
			if ( iVersion != XTLS_VERSION_13 ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_VERSION,
					"process-server-hello",
					"TLS server selected a version that the client did not offer"
				);
			}
			bVersion = true;
		} else if ( Extension.Type == XTLS_EXTENSION_KEY_SHARE ) {
			if ( !xrtTlsServerKeyShare(Extension.Data, pShare) ) {
				return false;
			}
			if ( pShare->Group != pState->Group ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_KEY_EXCHANGE,
					"process-server-hello",
					"TLS server selected a key share that the client did not send"
				);
			}
			bShare = true;
		#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		} else if ( Extension.Type == XTLS_EXTENSION_PRE_SHARED_KEY ) {
			xtlsresumeinfo Resume;
			const xtlscipherinfo* pResumeCipher;
			uint16 iSelected;

			if ( (pState->OfferResume == NULL) ||
				!xrtTlsServerPsk(Extension.Data, &iSelected) ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
					"process-server-hello",
					"TLS server selected a PSK that the client did not offer"
				);
			}
			if ( iSelected != 0 ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_NEGOTIATION,
					"process-server-hello",
					"TLS server selected an unknown PSK identity"
				);
			}
			if ( !xrtTlsResumeInfo(pState->OfferResume, &Resume) ) {
				return false;
			}
			pResumeCipher = xrtTlsCipherInfo(Resume.Cipher);
			if ( (pResumeCipher == NULL) ||
				(pResumeCipher->Hash != pCipher->Hash) ||
				(Resume.Secret.Size != pCipher->HashSize) ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_CIPHER,
					"process-server-hello",
					"TLS server selected a cipher with the wrong PSK hash"
				);
			}
			bResumed = true;
		#endif
		} else {
			return __xrtTlsClientError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"process-server-hello",
				"TLS ServerHello contains an extension not offered for this message"
			);
		}
	}
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( !bVersion || !bShare ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
			"process-server-hello",
			"TLS ServerHello is missing supported_versions or key_share"
		);
	}
	*ppCipher = pCipher;
	*pResumed = bResumed;
	return true;
}



/* 严格验证一次 HelloRetryRequest 并发布选择的组、套件和 cookie。 */
static bool __xrtTlsClientRetryHello(
	const xtlsclientstate* pState,
	const xtlsserverhello* pHello,
	const xtlscipherinfo** ppCipher,
	uint16* pGroup,
	xbytesview* pCookie
)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;
	const xtlscipherinfo* pCipher;
	bool bVersion = false;
	bool bGroup = false;

	*ppCipher = NULL;
	*pGroup = 0;
	memset(pCookie, 0, sizeof(*pCookie));
	if ( !pHello->Retry || pState->RetrySeen ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"process-hello-retry-request",
			"TLS peer sent an invalid or repeated HelloRetryRequest"
		);
	}
	if ( (pHello->SessionId.Size != sizeof(pState->SessionId)) ||
		(memcmp(
			pHello->SessionId.Data, pState->SessionId,
			sizeof(pState->SessionId)
		) != 0) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"process-hello-retry-request",
			"TLS HelloRetryRequest did not echo the session identifier"
		);
	}
	if ( !__xrtTlsClientOffered(
		pState->Ciphers, pState->CipherCount, pHello->CipherSuite
	) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_CIPHER,
			"process-hello-retry-request",
			"TLS HelloRetryRequest selected an unoffered cipher"
		);
	}
	pCipher = xrtTlsCipherInfo((xtlscipher)pHello->CipherSuite);
	if ( (pCipher == NULL) || (pCipher->Version != XTLS_VERSION_13) ||
		(pCipher->HashSize > pState->SecretCapacity) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_CIPHER,
			"process-hello-retry-request",
			"TLS HelloRetryRequest selected an unusable cipher"
		);
	}
	if ( !xrtTlsExtensionsInit(&Cursor, pHello->Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		if ( Extension.Type == XTLS_EXTENSION_SUPPORTED_VERSIONS ) {
			uint16 iVersion;

			if ( !xrtTlsServerVersion(Extension.Data, &iVersion) ||
				(iVersion != XTLS_VERSION_13) ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_VERSION,
					"process-hello-retry-request",
					"TLS HelloRetryRequest did not select TLS 1.3"
				);
			}
			bVersion = true;
		} else if ( Extension.Type == XTLS_EXTENSION_KEY_SHARE ) {
			uint16 iGroup;

			if ( !xrtTlsRetryGroup(Extension.Data, &iGroup) ) {
				return false;
			}
			if ( !__xrtTlsClientOffered(
				pState->Groups, pState->GroupCount, iGroup
			) || (iGroup == pState->Group) ||
				!xrtTlsGroupAvailable(iGroup) ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_KEY_EXCHANGE,
					"process-hello-retry-request",
					"TLS HelloRetryRequest selected an invalid retry group"
				);
			}
			*pGroup = iGroup;
			bGroup = true;
		} else if ( Extension.Type == XTLS_EXTENSION_COOKIE ) {
			if ( !xrtTlsRetryCookie(Extension.Data, pCookie) ) {
				return false;
			}
		} else {
			return __xrtTlsClientError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"process-hello-retry-request",
				"TLS HelloRetryRequest contains a forbidden extension"
			);
		}
	}
	if ( (Result != XTLS_ITEM_DONE) || !bVersion || !bGroup ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
			"process-hello-retry-request",
			"TLS HelloRetryRequest is missing a required extension"
		);
	}
	*ppCipher = pCipher;
	return true;
}



/* 提交 HRR transcript，生成新密钥共享并排队第二个 ClientHello。 */
static xtlsresult __xrtTlsClientRetryHelloCommit(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage,
	const xtlsserverhello* pHello
)
{
	const xtlscipherinfo* pCipher;
	xtlstranscript Next;
	xbytesview Cookie;
	xbytesview Encoded;
	uint16 iGroup;
	bool bCommitted = false;

	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTlsClientRetryHello(
		pState, pHello, &pCipher, &iGroup, &Cookie
	) ) {
		goto cleanup;
	}
	Encoded.Data = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pMessage->EncodedSize;
	if ( !__xrtTlsTranscriptInit(
		&Next, __xrtTlsHash(pCipher->Hash)
	) || !__xrtTlsTranscriptUpdate(
		&Next,
		(xbytesview) { pState->ClientHello, pState->ClientHelloSize }
	) || !__xrtTlsTranscriptRetry(&Next) ||
		!__xrtTlsTranscriptUpdate(&Next, Encoded) ) {
		goto cleanup;
	}
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	memset(&Next, 0, sizeof(Next));
	pState->Cipher = pCipher->Cipher;
	pState->Group = iGroup;
	pState->RetrySeen = true;
	if ( !__xrtTlsClientHelloQueue(
		pSession, pState, Cookie, true
	) ) {
		goto cleanup;
	}
	bCommitted = __xrtTlsClientWait(pSession, true);

cleanup:
	__xrtTlsTranscriptClear(&Next);
	return bCommitted ? XTLS_OK : __xrtTlsClientFailed(pSession);
}



/* 生成 TLS 1.3 空 transcript 摘要。 */
static bool __xrtTlsClientEmptyHash(
	xcryptohash Hash,
	void* pDigest,
	size_t iHashSize
)
{
	xtlstranscript Empty;
	bool bResult;

	memset(&Empty, 0, sizeof(Empty));
	bResult = __xrtTlsTranscriptInit(&Empty, Hash) &&
		__xrtTlsTranscriptDigest(&Empty, pDigest, iHashSize);
	__xrtTlsTranscriptClear(&Empty);
	return bResult;
}



/* 从 ClientHello、ServerHello 和 ECDHE 共享秘密派生握手 epoch。 */
static bool __xrtTlsClientServerKeys(
	const xtlsclientstate* pState,
	const xtlscipherinfo* pCipher,
	const xtlskeyshare* pShare,
	bool bResumed,
	xbytesview ServerHello,
	xtlsclientserverstate* pNext
)
{
	const xtlsgroupinfo* pGroup = xrtTlsGroupInfo(pState->Group);
	uint8 Shared[XTLS_CLIENT_SHARED_MAX_SIZE];
	uint8 Zero[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 EmptyHash[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 HandshakeHash[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 Early[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 Derived[XTLS_CLIENT_SECRET_MAX_SIZE];
	xbytesview Empty = { NULL, 0 };
	xbytesview Psk;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		xtlsresumeinfo Resume;
	#endif
	bool bResult = false;

	memset(pNext, 0, sizeof(*pNext));
	memset(Shared, 0, sizeof(Shared));
	memset(Zero, 0, sizeof(Zero));
	memset(EmptyHash, 0, sizeof(EmptyHash));
	memset(HandshakeHash, 0, sizeof(HandshakeHash));
	memset(Early, 0, sizeof(Early));
	memset(Derived, 0, sizeof(Derived));
	if ( (pGroup == NULL) ||
		(pGroup->SharedSize > sizeof(Shared)) ||
		(pCipher->HashSize > sizeof(Zero)) ||
		(pCipher->KeySize > sizeof(pNext->ReadKey)) ||
		(pCipher->IvSize > sizeof(pNext->ReadIv)) ) {
		(void)__xrtTlsClientError(
			XERR_UNSUPPORTED, XTLS_ERROR_KEY_DERIVATION,
			"derive-server-hello-keys",
			"TLS client key material exceeds the current role capacity"
		);
		goto cleanup;
	}
	pNext->Cipher = pCipher->Cipher;
	pNext->Hash = __xrtTlsHash(pCipher->Hash);
	pNext->HashSize = pCipher->HashSize;
	pNext->KeySize = pCipher->KeySize;
	pNext->IvSize = pCipher->IvSize;
	pNext->Resumed = bResumed;
	Psk = (xbytesview) { Zero, pNext->HashSize };
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( bResumed ) {
			if ( (pState->OfferResume == NULL) ||
				!xrtTlsResumeInfo(pState->OfferResume, &Resume) ||
				(Resume.Secret.Size != pNext->HashSize) ) {
				(void)__xrtTlsClientError(
					XERR_STATE, XTLS_ERROR_KEY_DERIVATION,
					"derive-server-hello-keys",
					"TLS client resume secret is unavailable"
				);
				goto cleanup;
			}
			Psk = Resume.Secret;
		}
	#else
		(void)bResumed;
	#endif
	if ( pState->RetrySeen ) {
		if ( !pState->Transcript.Ready ||
			(pState->Transcript.Hash != pNext->Hash) ) {
			(void)__xrtTlsClientError(
				XERR_STATE, XTLS_ERROR_TRANSCRIPT,
				"derive-server-hello-keys",
				"TLS retry transcript is unavailable"
			);
			goto cleanup;
		}
		pNext->Transcript = pState->Transcript;
	} else if ( !__xrtTlsTranscriptInit(
		&pNext->Transcript, pNext->Hash
	) ) {
		goto cleanup;
	}
	if ( !__xrtTlsTranscriptUpdate(
			&pNext->Transcript,
			(xbytesview) { pState->ClientHello, pState->ClientHelloSize }
		) || !__xrtTlsTranscriptUpdate(
			&pNext->Transcript, ServerHello
		) || !__xrtTlsTranscriptDigest(
			&pNext->Transcript, HandshakeHash, pNext->HashSize
		) || !__xrtTlsClientEmptyHash(
			pNext->Hash, EmptyHash, pNext->HashSize
		) || !xrtTlsKeyShareDerive(
			pState->Group,
			(xbytesview) { pState->PrivateKey, pState->PrivateKeySize },
			pShare->Key, Shared, sizeof(Shared)
		) || !__xrtTls13Extract(
			pNext->Hash, Empty, Psk,
			Early, pNext->HashSize
		) || !__xrtTls13DeriveSecret(
			pNext->Hash, (xbytesview) { Early, pNext->HashSize },
			XRT_STR_LITERAL("derived"),
			(xbytesview) { EmptyHash, pNext->HashSize },
			Derived, pNext->HashSize
		) || !__xrtTls13Extract(
			pNext->Hash,
			(xbytesview) { Derived, pNext->HashSize },
			(xbytesview) { Shared, pGroup->SharedSize },
			pNext->HandshakeSecret, pNext->HashSize
		) || !__xrtTls13DeriveSecret(
			pNext->Hash,
			(xbytesview) { pNext->HandshakeSecret, pNext->HashSize },
			XRT_STR_LITERAL("c hs traffic"),
			(xbytesview) { HandshakeHash, pNext->HashSize },
			pNext->ClientTraffic, pNext->HashSize
		) || !__xrtTls13DeriveSecret(
			pNext->Hash,
			(xbytesview) { pNext->HandshakeSecret, pNext->HashSize },
			XRT_STR_LITERAL("s hs traffic"),
			(xbytesview) { HandshakeHash, pNext->HashSize },
			pNext->ServerTraffic, pNext->HashSize
		) || !__xrtTls13ExpandLabel(
			pNext->Hash,
			(xbytesview) { pNext->ServerTraffic, pNext->HashSize },
			XRT_STR_LITERAL("key"), Empty,
			pNext->ReadKey, pNext->KeySize
		) || !__xrtTls13ExpandLabel(
			pNext->Hash,
			(xbytesview) { pNext->ServerTraffic, pNext->HashSize },
			XRT_STR_LITERAL("iv"), Empty,
			pNext->ReadIv, pNext->IvSize
		) || !__xrtTls13ExpandLabel(
			pNext->Hash,
			(xbytesview) { pNext->ClientTraffic, pNext->HashSize },
			XRT_STR_LITERAL("key"), Empty,
			pNext->WriteKey, pNext->KeySize
		) || !__xrtTls13ExpandLabel(
			pNext->Hash,
			(xbytesview) { pNext->ClientTraffic, pNext->HashSize },
			XRT_STR_LITERAL("iv"), Empty,
			pNext->WriteIv, pNext->IvSize
		) ) {
		goto cleanup;
	}
	bResult = true;

cleanup:
	xrtSecureZero(Derived, sizeof(Derived));
	xrtSecureZero(Early, sizeof(Early));
	xrtSecureZero(HandshakeHash, sizeof(HandshakeHash));
	xrtSecureZero(EmptyHash, sizeof(EmptyHash));
	xrtSecureZero(Zero, sizeof(Zero));
	xrtSecureZero(Shared, sizeof(Shared));
	if ( !bResult ) {
		__xrtTlsTranscriptClear(&pNext->Transcript);
		xrtSecureZero(pNext, sizeof(*pNext));
	}
	return bResult;
}



/* 验证并提交一条普通 TLS 1.3 ServerHello。 */
static xtlsresult __xrtTlsClient13ServerHelloCommit(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage,
	const xtlsserverhello* pHello
)
{
	xtlskeyshare Share;
	const xtlscipherinfo* pCipher;
	xtlsclientserverstate Next;
	xbytesview Encoded;
	bool bResumed;
	bool bCommitted = false;

	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTlsClientServerHello(
			pState, pHello, &pCipher, &Share, &bResumed
		) ) {
		goto cleanup;
	}
	if ( pState->ResumeOnly && !bResumed ) {
		(void)__xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_RESUME,
			"process-server-hello",
			"TLS server rejected the required resume object"
		);
		goto cleanup;
	}
	Encoded.Data = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pMessage->EncodedSize;
	if ( !__xrtTlsClientServerKeys(
		pState, pCipher, &Share, bResumed, Encoded, &Next
	) ) {
		goto cleanup;
	}
	if ( !__xrtTlsSessionNegotiated(
		pSession, XTLS_VERSION_13, Next.Cipher
	) || !__xrtTlsSessionKeys(
		pSession, XTLS_VERSION_13, Next.Cipher,
		(xbytesview) { Next.ReadKey, Next.KeySize },
		(xbytesview) { Next.ReadIv, Next.IvSize },
		(xbytesview) { Next.WriteKey, Next.KeySize },
		(xbytesview) { Next.WriteIv, Next.IvSize }
	) ) {
		goto cleanup;
	}
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next.Transcript;
	memset(&Next.Transcript, 0, sizeof(Next.Transcript));
	memcpy(
		pState->HandshakeSecret, Next.HandshakeSecret, Next.HashSize
	);
	memcpy(
		pState->ClientHandshakeTraffic,
		Next.ClientTraffic, Next.HashSize
	);
	memcpy(
		pState->ServerHandshakeTraffic,
		Next.ServerTraffic, Next.HashSize
	);
	pState->HashSize = Next.HashSize;
	pState->Cipher = Next.Cipher;
	pState->Version = XTLS_VERSION_13;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		pState->Resumed = Next.Resumed;
	#endif
	pState->Step = XTLS_CLIENT_WAIT_ENCRYPTED_EXTENSIONS;
	xrtSecureZero(pState->PrivateKey, pState->PrivateKeySize);
	bCommitted = __xrtTlsClientWait(pSession, true);

cleanup:
	__xrtTlsTranscriptClear(&Next.Transcript);
	xrtSecureZero(&Next, sizeof(Next));
	return bCommitted ? XTLS_OK : __xrtTlsClientFailed(pSession);
}



/* 只解析一次 ServerHello，再按线路版本分派到对应角色状态机。 */
static xtlsresult __xrtTlsClientServerHelloCommit(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
)
{
	xtlsserverhello Hello;
	xtlsextension Extension;
	xtlsitemresult Result;
	uint16 iVersion;

	if ( !xrtTlsServerHelloParse(pMessage->Body, &Hello) ) {
		return __xrtTlsClientFailed(pSession);
	}
	Result = xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_SUPPORTED_VERSIONS, &Extension
	);
	if ( Result == XTLS_ITEM_ERROR ) {
		return __xrtTlsClientFailed(pSession);
	}
	if ( Result == XTLS_ITEM_VALUE ) {
		if ( !xrtTlsServerVersion(Extension.Data, &iVersion) ) {
			return __xrtTlsClientFailed(pSession);
		}
	} else {
		iVersion = Hello.LegacyVersion;
	}
	if ( Hello.Retry ) {
		if ( (iVersion != XTLS_VERSION_13) || !pState->Offer13 ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_VERSION,
				"process-hello-retry-request",
				"TLS HelloRetryRequest did not select an offered TLS version"
			);
		}
		return __xrtTlsClientRetryHelloCommit(
			pSession, pState, pMessage, &Hello
		);
	}
	if ( (iVersion == XTLS_VERSION_13) && pState->Offer13 ) {
		return __xrtTlsClient13ServerHelloCommit(
			pSession, pState, pMessage, &Hello
		);
	}
	#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY) && \
		defined(XRT_FEATURE_TLS_AUTH_MESSAGES) && \
		defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE)
		if ( (iVersion == XTLS_VERSION_12) && pState->Offer12 &&
			!pState->ResumeOnly ) {
			return __xrtTlsClient12ServerHello(
				pSession, pState, pMessage, &Hello
			);
		}
	#endif
	return __xrtTlsClientProtocol(
		pSession, XTLS_ERROR_VERSION, "process-server-hello",
		"TLS server selected a version that the client did not offer"
	);
}



/* 在客户端实际发送的 ALPN 列表中查找服务端选择。 */
bool __xrtTlsClientProtocolSelect(
	const xtlsclientstate* pState,
	xbytesview Selected,
	xbytesview* pProtocol,
	cstr sOperation
)
{
	for ( size_t i = 0; i < pState->ProtocolCount; i++ ) {
		const xbytesview* pCurrent = &pState->Protocols[i];

		if ( (pCurrent->Size == Selected.Size) &&
			(memcmp(pCurrent->Data, Selected.Data, Selected.Size) == 0) ) {
			*pProtocol = *pCurrent;
			return true;
		}
	}
	return __xrtTlsClientError(
		XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
		sOperation,
		"TLS server selected an ALPN protocol that the client did not offer"
	);
}



/* 校验 EncryptedExtensions 只包含实际请求且允许位于此消息的扩展。 */
static bool __xrtTlsClientEncryptedExtensions(
	const xtlsclientstate* pState,
	const xtlshandshake* pMessage,
	xbytesview* pProtocol
)
{
	static const uint16 Allowed[] = {
		XTLS_EXTENSION_SERVER_NAME,
		XTLS_EXTENSION_SUPPORTED_GROUPS,
		XTLS_EXTENSION_ALPN
	};
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;
	xbytesview Extensions;

	memset(pProtocol, 0, sizeof(*pProtocol));
	if ( !xrtTlsEncryptedExtensionsParse(
		pMessage->Body, &Extensions
	) || !xrtTlsExtensionsInit(&Cursor, Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		if ( !__xrtTlsClientOffered(
			Allowed, sizeof(Allowed) / sizeof(Allowed[0]),
			Extension.Type
		) ) {
			return __xrtTlsClientError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"process-encrypted-extensions",
				"TLS EncryptedExtensions contains an extension not allowed here"
			);
		}
		if ( Extension.Type == XTLS_EXTENSION_SERVER_NAME ) {
			if ( pState->SniName.Size == 0 ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
					"process-encrypted-extensions",
					"TLS server acknowledged a server name that was not offered"
				);
			}
		} else if ( Extension.Type == XTLS_EXTENSION_ALPN ) {
			xbytesview Selected;

			if ( (pState->ProtocolCount == 0) ||
				!xrtTlsProtocolSelected(Extension.Data, &Selected) ||
				!__xrtTlsClientProtocolSelect(
					pState, Selected, pProtocol,
					"process-encrypted-extensions"
				) ) {
				return false;
			}
		}
	}
	return Result == XTLS_ITEM_DONE;
}



/* 验证并提交 TLS 1.3 EncryptedExtensions 和稳定 ALPN 选择。 */
static xtlsresult __xrtTlsClientEncryptedExtensionsCommit(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
)
{
	xtlstranscript Next = pState->Transcript;
	xbytesview Protocol;
	xbytesview Encoded;

	if ( !__xrtTlsClientEncryptedExtensions(
		pState, pMessage, &Protocol
	) ) {
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( pState->Resumed ) {
			xtlsresumeinfo Resume;

			if ( !xrtTlsResumeInfo(pState->OfferResume, &Resume) ||
				(Protocol.Size != Resume.Protocol.Size) ||
				((Protocol.Size != 0) &&
				 (memcmp(
					Protocol.Data, Resume.Protocol.Data, Protocol.Size
				 ) != 0)) ) {
				(void)__xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_NEGOTIATION,
					"process-encrypted-extensions",
					"TLS resumed connection changed its bound ALPN protocol"
				);
				__xrtTlsTranscriptClear(&Next);
				return __xrtTlsClientFailed(pSession);
			}
		}
	#endif
	Encoded.Data = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pMessage->EncodedSize;
	if ( !__xrtTlsTranscriptUpdate(&Next, Encoded) ||
		((Protocol.Size != 0) &&
		 !__xrtTlsSessionSetProtocol(pSession, Protocol)) ) {
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		pState->Step = pState->Resumed ?
			XTLS_CLIENT_WAIT_FINISHED : XTLS_CLIENT_WAIT_CERTIFICATE;
	#else
		pState->Step = XTLS_CLIENT_WAIT_CERTIFICATE;
	#endif
	return XTLS_OK;
}



#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)

/* 使用当前 X.509 错误作为原因设置 TLS 客户端认证错误。 */
static bool __xrtTlsClientAuthCause(cstr sOperation, cstr sMessage)
{
	const xerror* pCause = xrtGetError();
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_PROTOCOL;

	__xrtTlsErrorCause(
		Kind, XTLS_ERROR_VERIFY, sOperation,
		sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 统计 TLS 1.3 证书链的条目和 DER 总量，并拒绝未请求扩展。 */
static bool __xrtTlsClientCertificateMeasure(
	const xtlscertificatemessage* pCertificate,
	size_t* pCount,
	size_t* pBytes
)
{
	xtlscertificatecursor Cursor;
	xtlscertificateentry Entry;
	xtlsitemresult Result;
	size_t iCount = 0;
	size_t iBytes = 0;

	*pCount = 0;
	*pBytes = 0;
	if ( pCertificate->RequestContext.Size != 0 ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE,
			"process-server-certificate",
			"TLS server Certificate request context is not empty"
		);
	}
	if ( !xrtTlsCertificateEntries(pCertificate, &Cursor) ) {
		return false;
	}
	while ( (Result = xrtTlsCertificatesRead(
		&Cursor, &Entry
	)) == XTLS_ITEM_VALUE ) {
		if ( Entry.Extensions.Size != 0 ) {
			return __xrtTlsClientError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"process-server-certificate",
				"TLS server sent an unrequested Certificate extension"
			);
		}
		if ( (iCount == SIZE_MAX) ||
			(Entry.Data.Size > (SIZE_MAX - iBytes)) ) {
			return __xrtTlsClientError(
				XERR_RANGE, XTLS_ERROR_LIMIT,
				"process-server-certificate",
				"TLS server certificate chain size overflows"
			);
		}
		iCount++;
		iBytes += Entry.Data.Size;
	}
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( iCount == 0 ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE,
			"process-server-certificate",
			"TLS server certificate chain is empty"
		);
	}
	*pCount = iCount;
	*pBytes = iBytes;
	return true;
}



/* 分配并深复制完整对端证书链，所有解析视图都指向稳定 DER。 */
static xtlsclientpeer* __xrtTlsClientCertificateCopy(
	const xtlscertificatemessage* pCertificate,
	size_t iCount,
	size_t iBytes
)
{
	const size_t iCertificateAlignment =
		XRT_INTERNAL_ALIGNOF(xx509cert);
	const size_t iHeader = sizeof(xtlsclientpeer) +
		((iCertificateAlignment -
			(sizeof(xtlsclientpeer) % iCertificateAlignment)) %
			iCertificateAlignment);
	size_t iCertificates;
	size_t iTotal;
	xtlsclientpeer* pPeer;
	xtlscertificatecursor Cursor;
	xtlscertificateentry Entry;
	xtlsitemresult Result;
	bytes pData;
	size_t i = 0;

	if ( iCount > (SIZE_MAX / sizeof(xx509cert)) ) {
		(void)__xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			"process-server-certificate",
			"TLS server certificate view array overflows"
		);
		return NULL;
	}
	iCertificates = iCount * sizeof(xx509cert);
	if ( (iCertificates > (SIZE_MAX - iHeader)) ||
		(iBytes > (SIZE_MAX - iHeader - iCertificates)) ) {
		(void)__xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			"process-server-certificate",
			"TLS server certificate snapshot overflows"
		);
		return NULL;
	}
	iTotal = iHeader + iCertificates + iBytes;
	pPeer = (xtlsclientpeer*)xrtMalloc(iTotal);
	if ( pPeer == NULL ) {
		(void)__xrtTlsClientAuthCause(
			"process-server-certificate",
			"TLS server certificate snapshot allocation failed"
		);
		return NULL;
	}
	memset(pPeer, 0, iHeader + iCertificates);
	pPeer->Certificates = (xx509cert*)((bytes)pPeer + iHeader);
	pPeer->CertificateCount = iCount;
	pData = (bytes)(pPeer->Certificates + iCount);
	if ( !xrtTlsCertificateEntries(pCertificate, &Cursor) ) {
		xrtFree(pPeer);
		return NULL;
	}
	while ( (Result = xrtTlsCertificatesRead(
		&Cursor, &Entry
	)) == XTLS_ITEM_VALUE ) {
		memcpy(pData, Entry.Data.Data, Entry.Data.Size);
		if ( !xrtX509Parse(
			pData, Entry.Data.Size, &pPeer->Certificates[i]
		) ) {
			xrtFree(pPeer);
			(void)__xrtTlsClientAuthCause(
				"process-server-certificate",
				"TLS server certificate DER parsing failed"
			);
			return NULL;
		}
		pData += Entry.Data.Size;
		i++;
	}
	if ( (Result != XTLS_ITEM_DONE) || (i != iCount) ) {
		xrtFree(pPeer);
		(void)__xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE,
			"process-server-certificate",
			"TLS server certificate chain changed during parsing"
		);
		return NULL;
	}
	if ( !xrtX509PublicKey(
		&pPeer->Certificates[0], &pPeer->PublicKey
	) ) {
		xrtFree(pPeer);
		(void)__xrtTlsClientAuthCause(
			"process-server-certificate",
			"TLS server certificate public key parsing failed"
		);
		return NULL;
	}
	return pPeer;
}



/* 验证并提交 TLS 1.3 服务端证书链。 */
xtlsresult __xrtTlsClientCertificateCommit(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
)
{
	xtlscertificatemessage Certificate;
	xtlsclientpeer* pPeer;
	xtlstranscript Next = pState->Transcript;
	xbytesview Encoded;
	size_t iCount;
	size_t iBytes;

	if ( pState->Verifier == NULL ) {
		(void)__xrtTlsClientError(
			XERR_STATE, XTLS_ERROR_VERIFY,
			"process-server-certificate",
			"TLS client has no certificate verifier"
		);
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	if ( !xrtTlsCertificateParse(
		pState->Version, pMessage->Body, &Certificate
	) || !__xrtTlsClientCertificateMeasure(
		&Certificate, &iCount, &iBytes
	) ) {
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	pPeer = __xrtTlsClientCertificateCopy(
		&Certificate, iCount, iBytes
	);
	if ( pPeer == NULL ) {
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	if ( !xrtTlsVerifierVerify(
		pState->Verifier, XTLS_SERVER,
		(xstrview) {
			(const char*)pState->ServerName.Data,
			pState->ServerName.Size
		},
		pPeer->Certificates, pPeer->CertificateCount
	) ) {
		xrtFree(pPeer);
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	Encoded.Data = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pMessage->EncodedSize;
	if ( !__xrtTlsTranscriptUpdate(&Next, Encoded) ) {
		xrtFree(pPeer);
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( !xrtSha256(
			pPeer->Certificates[0].Raw.Data,
			pPeer->Certificates[0].Raw.Size,
			pState->PeerIdentity
		) ) {
			xrtFree(pPeer);
			__xrtTlsTranscriptClear(&Next);
			return __xrtTlsClientFailed(pSession);
		}
	#endif
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	pState->Peer = pPeer;
	pState->Step = pState->Version == XTLS_VERSION_13 ?
		XTLS_CLIENT_WAIT_CERTIFICATE_VERIFY :
		XTLS_CLIENT_WAIT_SERVER_KEY_EXCHANGE;
	return XTLS_OK;
}



/* 验证并提交 TLS 1.3 服务端 CertificateVerify。 */
static xtlsresult __xrtTlsClientCertificateVerifyCommit(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
)
{
	xtlscertificateverify Verify;
	xtlstranscript Next = pState->Transcript;
	uint8 Digest[XTLS_CLIENT_SECRET_MAX_SIZE];
	xbytesview Encoded;
	bool bVerified;

	memset(Digest, 0, sizeof(Digest));
	if ( (pState->Peer == NULL) ||
		!xrtTlsCertificateVerifyParse(pMessage->Body, &Verify) ) {
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	if ( !__xrtTlsClientOffered(
		pState->Signatures, pState->SignatureCount, Verify.Scheme
	) ) {
		(void)__xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_VERIFY,
			"process-server-certificate-verify",
			"TLS server used a signature scheme that was not offered"
		);
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	bVerified = __xrtTlsTranscriptDigest(
		&pState->Transcript, Digest, pState->HashSize
	) && xrtTls13CertificateVerifySignature(
		XTLS_SERVER, (xtlssignature)Verify.Scheme,
		(xbytesview) { Digest, pState->HashSize },
		Verify.Signature, &pState->Peer->PublicKey
	);
	xrtSecureZero(Digest, sizeof(Digest));
	if ( !bVerified ) {
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	Encoded.Data = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pMessage->EncodedSize;
	if ( !__xrtTlsTranscriptUpdate(&Next, Encoded) ) {
		__xrtTlsTranscriptClear(&Next);
		return __xrtTlsClientFailed(pSession);
	}
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	pState->Step = XTLS_CLIENT_WAIT_FINISHED;
	return XTLS_OK;
}

#endif



/* 检查发送硬上限是否足以原子排队客户端 Finished。 */
static xtlsresult __xrtTlsClientFinishedWritable(
	xtlssession* pSession,
	const xtlsclientstate* pState
)
{
	const xtlslimits* pLimits = xrtTlsContextLimits(pSession->Context);
	size_t iMessage = xrtTlsHandshakeSize(pState->HashSize);
	size_t iRecord = __xrtTlsRecordSealSize(
		&pSession->WriteKey, iMessage, 0
	);
	size_t iQueued = xrtTlsSessionSendSize(pSession);

	if ( (pLimits == NULL) || (iMessage == 0) || (iRecord == 0) ) {
		(void)__xrtTlsClientError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"process-server-finished",
			"TLS client Finished output state is invalid"
		);
		return XTLS_ERROR;
	}
	if ( iRecord > pLimits->SendLimit ) {
		(void)__xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			"process-server-finished",
			"TLS send limit cannot hold the mandatory client Finished"
		);
		return XTLS_ERROR;
	}
	if ( (iQueued > pLimits->SendLimit) ||
		(iRecord > (pLimits->SendLimit - iQueued)) ) {
		return XTLS_AGAIN;
	}
	return XTLS_OK;
}



/* 从服务端 Finished 原子派生客户端响应和应用数据 epoch。 */
static bool __xrtTlsClientFinishedPrepare(
	const xtlsclientstate* pState,
	const xtlshandshake* pMessage,
	xtlsclientfinishedstate* pNext
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(pState->Cipher);
	uint8 BeforeHash[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 ServerHash[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 EmptyHash[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 FinishedKey[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 Expected[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 ClientVerify[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 Derived[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 Master[XTLS_CLIENT_SECRET_MAX_SIZE];
	uint8 Zero[XTLS_CLIENT_SECRET_MAX_SIZE];
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		uint8 ClientHash[XTLS_CLIENT_SECRET_MAX_SIZE];
	#endif
	xbytesview VerifyData;
	xbytesview Encoded;
	xbytesview Empty = { NULL, 0 };
	xcryptohash Hash;
	bool bResult = false;

	memset(pNext, 0, sizeof(*pNext));
	memset(BeforeHash, 0, sizeof(BeforeHash));
	memset(ServerHash, 0, sizeof(ServerHash));
	memset(EmptyHash, 0, sizeof(EmptyHash));
	memset(FinishedKey, 0, sizeof(FinishedKey));
	memset(Expected, 0, sizeof(Expected));
	memset(ClientVerify, 0, sizeof(ClientVerify));
	memset(Derived, 0, sizeof(Derived));
	memset(Master, 0, sizeof(Master));
	memset(Zero, 0, sizeof(Zero));
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		memset(ClientHash, 0, sizeof(ClientHash));
	#endif
	if ( (pCipher == NULL) ||
		(pCipher->HashSize != pState->HashSize) ||
		(pCipher->KeySize > XTLS_CLIENT_KEY_MAX_SIZE) ||
		(pCipher->IvSize > XTLS_CLIENT_IV_MAX_SIZE) ) {
		(void)__xrtTlsClientError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"process-server-finished",
			"TLS client negotiated key schedule is inconsistent"
		);
		goto cleanup;
	}
	Hash = __xrtTlsHash(pCipher->Hash);
	if ( !xrtTlsFinishedParse(
		pMessage->Body, pState->HashSize, &VerifyData
	) || !__xrtTlsTranscriptDigest(
		&pState->Transcript, BeforeHash, pState->HashSize
	) || !__xrtTls13ExpandLabel(
		Hash,
		(xbytesview) {
			pState->ServerHandshakeTraffic, pState->HashSize
		}, XRT_STR_LITERAL("finished"), Empty,
		FinishedKey, pState->HashSize
	) || !__xrtTls13Finished(
		Hash, (xbytesview) { FinishedKey, pState->HashSize },
		(xbytesview) { BeforeHash, pState->HashSize },
		Expected, pState->HashSize
	) ) {
		goto cleanup;
	}
	if ( !xrtConstTimeEqual(
		Expected, VerifyData.Data, pState->HashSize
	) ) {
		(void)__xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_VERIFY,
			"process-server-finished",
			"TLS server Finished verification failed"
		);
		goto cleanup;
	}

	/* 应用 traffic secret 绑定包含服务端 Finished 的 transcript。 */
	pNext->Transcript = pState->Transcript;
	Encoded.Data = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pMessage->EncodedSize;
	if ( !__xrtTlsTranscriptUpdate(
		&pNext->Transcript, Encoded
	) || !__xrtTlsTranscriptDigest(
		&pNext->Transcript, ServerHash, pState->HashSize
	) || !__xrtTlsClientEmptyHash(
		Hash, EmptyHash, pState->HashSize
	) || !__xrtTls13DeriveSecret(
		Hash,
		(xbytesview) { pState->HandshakeSecret, pState->HashSize },
		XRT_STR_LITERAL("derived"),
		(xbytesview) { EmptyHash, pState->HashSize },
		Derived, pState->HashSize
	) || !__xrtTls13Extract(
		Hash, (xbytesview) { Derived, pState->HashSize },
		(xbytesview) { Zero, pState->HashSize },
		Master, pState->HashSize
	) || !__xrtTls13DeriveSecret(
		Hash, (xbytesview) { Master, pState->HashSize },
		XRT_STR_LITERAL("c ap traffic"),
		(xbytesview) { ServerHash, pState->HashSize },
		pNext->ClientTraffic, pState->HashSize
	) || !__xrtTls13DeriveSecret(
		Hash, (xbytesview) { Master, pState->HashSize },
		XRT_STR_LITERAL("s ap traffic"),
		(xbytesview) { ServerHash, pState->HashSize },
		pNext->ServerTraffic, pState->HashSize
	) ) {
		goto cleanup;
	}

	/* 客户端 Finished 仍使用握手 traffic secret 和握手写 epoch。 */
	if ( !__xrtTls13ExpandLabel(
		Hash,
		(xbytesview) {
			pState->ClientHandshakeTraffic, pState->HashSize
		}, XRT_STR_LITERAL("finished"), Empty,
		FinishedKey, pState->HashSize
	) || !__xrtTls13Finished(
		Hash, (xbytesview) { FinishedKey, pState->HashSize },
		(xbytesview) { ServerHash, pState->HashSize },
		ClientVerify, pState->HashSize
	) || !xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_FINISHED,
		(xbytesview) { ClientVerify, pState->HashSize },
		pNext->Message, sizeof(pNext->Message)
	) ) {
		goto cleanup;
	}
	pNext->MessageSize = xrtTlsHandshakeSize(pState->HashSize);

	/* 两向应用 record key 全部就绪后才允许排队客户端 Finished。 */
	if ( !__xrtTls13RecordKey(
		pState->Cipher,
		(xbytesview) { pNext->ServerTraffic, pState->HashSize },
		&pNext->ReadKey
	) || !__xrtTls13RecordKey(
		pState->Cipher,
		(xbytesview) { pNext->ClientTraffic, pState->HashSize },
		&pNext->WriteKey
	) || !__xrtTlsTranscriptUpdate(
		&pNext->Transcript,
		(xbytesview) { pNext->Message, pNext->MessageSize }
	) ) {
		goto cleanup;
	}
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( !__xrtTlsTranscriptDigest(
			&pNext->Transcript, ClientHash, pState->HashSize
		) || !__xrtTls13DeriveSecret(
			Hash, (xbytesview) { Master, pState->HashSize },
			XRT_STR_LITERAL("res master"),
			(xbytesview) { ClientHash, pState->HashSize },
			pNext->ResumptionMaster, pState->HashSize
		) ) {
			goto cleanup;
		}
	#endif
	bResult = true;

cleanup:
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		xrtSecureZero(ClientHash, sizeof(ClientHash));
	#endif
	xrtSecureZero(Zero, sizeof(Zero));
	xrtSecureZero(Master, sizeof(Master));
	xrtSecureZero(Derived, sizeof(Derived));
	xrtSecureZero(ClientVerify, sizeof(ClientVerify));
	xrtSecureZero(Expected, sizeof(Expected));
	xrtSecureZero(FinishedKey, sizeof(FinishedKey));
	xrtSecureZero(EmptyHash, sizeof(EmptyHash));
	xrtSecureZero(ServerHash, sizeof(ServerHash));
	xrtSecureZero(BeforeHash, sizeof(BeforeHash));
	if ( !bResult ) {
		__xrtTlsRecordKeyClear(&pNext->WriteKey);
		__xrtTlsRecordKeyClear(&pNext->ReadKey);
		__xrtTlsTranscriptClear(&pNext->Transcript);
		xrtSecureZero(pNext, sizeof(*pNext));
	}
	return bResult;
}



/* 验证服务端 Finished，排队客户端响应，再一次提交应用 epoch。 */
static xtlsresult __xrtTlsClientFinishedCommit(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
)
{
	xtlsclientfinishedstate Next;
	xtlsresult Result;
	uint32 iWait;

	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTlsClientFinishedPrepare(pState, pMessage, &Next) ) {
		return __xrtTlsClientFailed(pSession);
	}
	Result = __xrtTlsSessionRecordProtect(
		pSession, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Next.Message, Next.MessageSize }, 0
	);
	if ( Result != XTLS_OK ) {
		__xrtTlsRecordKeyClear(&Next.WriteKey);
		__xrtTlsRecordKeyClear(&Next.ReadKey);
		__xrtTlsTranscriptClear(&Next.Transcript);
		xrtSecureZero(&Next, sizeof(Next));
		return Result == XTLS_AGAIN ?
			XTLS_AGAIN : __xrtTlsClientFailed(pSession);
	}

	/* 上方成功后不再执行可能失败的密码操作。 */
	__xrtTlsRecordKeyClear(&pSession->ReadKey);
	__xrtTlsRecordKeyClear(&pSession->WriteKey);
	pSession->ReadKey = Next.ReadKey;
	pSession->WriteKey = Next.WriteKey;
	xrtSecureZero(&Next.ReadKey, sizeof(Next.ReadKey));
	xrtSecureZero(&Next.WriteKey, sizeof(Next.WriteKey));
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next.Transcript;
	memset(&Next.Transcript, 0, sizeof(Next.Transcript));
	memcpy(
		pState->ClientApplicationTraffic,
		Next.ClientTraffic, pState->HashSize
	);
	memcpy(
		pState->ServerApplicationTraffic,
		Next.ServerTraffic, pState->HashSize
	);
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		memcpy(
			pState->ResumptionMaster,
			Next.ResumptionMaster, pState->HashSize
		);
		pState->ResumptionReady = true;
	#endif
	xrtSecureZero(pState->HandshakeSecret, pState->SecretCapacity);
	xrtSecureZero(
		pState->ClientHandshakeTraffic, pState->SecretCapacity
	);
	xrtSecureZero(
		pState->ServerHandshakeTraffic, pState->SecretCapacity
	);
	pState->Step = XTLS_CLIENT_READY;
	iWait = XTLS_WAIT_INPUT;
	if ( xrtTlsSessionSendSize(pSession) != 0 ) {
		iWait |= XTLS_WAIT_OUTPUT;
	}
	if ( !__xrtTlsSessionSetState(
		pSession, XTLS_STATE_READY
	) || !__xrtTlsSessionSetWait(pSession, iWait) ) {
		Result = __xrtTlsClientFailed(pSession);
	} else {
		Result = XTLS_OK;
	}
	xrtSecureZero(&Next, sizeof(Next));
	return Result;
}



/* 响应使用旧写 epoch 入队，然后一次提交下一代收发 epoch。 */
static xtlsresult __xrtTlsClientKeyUpdateCommit(
	xtlssession* pSession,
	xtlsclientstate* pState,
	xtlskeyupdate Request
)
{
	xtlssessionupdate Next;
	xtlsresult Result = XTLS_OK;

	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTls13KeyUpdateReceive(
		pState->Cipher,
		(xbytesview) {
			pState->ServerApplicationTraffic, pState->HashSize
		},
		(xbytesview) {
			pState->ClientApplicationTraffic, pState->HashSize
		}, Request, &Next, "process-key-update"
	) ) {
		return __xrtTlsClientFailed(pSession);
	}
	if ( Request == XTLS_KEY_UPDATE_REQUESTED ) {
		Result = __xrtTlsSessionRecordProtect(
			pSession, XTLS_RECORD_HANDSHAKE,
			(xbytesview) { Next.Message, Next.MessageSize }, 0
		);
		if ( Result != XTLS_OK ) {
			__xrtTlsRecordKeyClear(&Next.WriteKey);
			__xrtTlsRecordKeyClear(&Next.ReadKey);
			xrtSecureZero(&Next, sizeof(Next));
			return Result == XTLS_AGAIN ?
				XTLS_AGAIN : __xrtTlsClientFailed(pSession);
		}
	}

	/* 上方成功后只执行无失败提交。 */
	__xrtTlsRecordKeyClear(&pSession->ReadKey);
	pSession->ReadKey = Next.ReadKey;
	xrtSecureZero(&Next.ReadKey, sizeof(Next.ReadKey));
	memcpy(
		pState->ServerApplicationTraffic,
		Next.ReadTraffic, pState->HashSize
	);
	if ( Request == XTLS_KEY_UPDATE_REQUESTED ) {
		__xrtTlsRecordKeyClear(&pSession->WriteKey);
		pSession->WriteKey = Next.WriteKey;
		xrtSecureZero(&Next.WriteKey, sizeof(Next.WriteKey));
		memcpy(
			pState->ClientApplicationTraffic,
			Next.WriteTraffic, pState->HashSize
		);
	}
	xrtSecureZero(&Next, sizeof(Next));
	return XTLS_OK;
}



/* 主动消息使用旧写 epoch 入队，成功后才提交下一代客户端写 epoch。 */
XRT_API xtlsresult xrtTlsClientKeyUpdate(
	xtlssession* pSession,
	xtlskeyupdate Request
)
{
	xtlsclientstate* pState;
	xtlssessionupdate Next;
	xtlsresult Result;

	if ( pSession == NULL ) {
		(void)__xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"key-update-tls-client", "TLS client session is null"
		);
		return XTLS_ERROR;
	}
	if ( xrtTlsSessionRole(pSession) != XTLS_CLIENT ) {
		(void)__xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"key-update-tls-client", "TLS session is not a client"
		);
		return XTLS_ERROR;
	}
	if ( xrtTlsSessionState(pSession) != XTLS_STATE_READY ) {
		(void)__xrtTlsClientError(
			XERR_STATE, XTLS_ERROR_STATE,
			"key-update-tls-client",
			"TLS client KeyUpdate requires a ready session"
		);
		return XTLS_ERROR;
	}
	if ( xrtTlsSessionVersion(pSession) != XTLS_VERSION_13 ) {
		(void)__xrtTlsClientError(
			XERR_UNSUPPORTED, XTLS_ERROR_VERSION,
			"key-update-tls-client",
			"TLS KeyUpdate requires a TLS 1.3 session"
		);
		return XTLS_ERROR;
	}
	if ( (Request != XTLS_KEY_UPDATE_NOT_REQUESTED) &&
		(Request != XTLS_KEY_UPDATE_REQUESTED) ) {
		(void)__xrtTlsClientError(
			XERR_VALUE, XTLS_ERROR_HANDSHAKE,
			"key-update-tls-client",
			"TLS client KeyUpdate request value is invalid"
		);
		return XTLS_ERROR;
	}
	pState = (xtlsclientstate*)__xrtTlsSessionRoleData(pSession);
	if ( (pState == NULL) || (pState->Step != XTLS_CLIENT_READY) ) {
		(void)__xrtTlsClientError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"key-update-tls-client",
			"TLS client KeyUpdate role state is invalid"
		);
		return XTLS_ERROR;
	}
	Result = __xrtTlsSessionKeyUpdateWritable(
		pSession, "key-update-tls-client"
	);
	if ( Result != XTLS_OK ) {
		if ( (Result == XTLS_AGAIN) &&
			!__xrtTlsClientWait(pSession, true) ) {
			return __xrtTlsClientFailed(pSession);
		}
		return Result;
	}
	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTls13KeyUpdateSend(
		pState->Cipher,
		(xbytesview) {
			pState->ClientApplicationTraffic, pState->HashSize
		}, Request, &Next, "key-update-tls-client"
	) ) {
		return XTLS_ERROR;
	}
	Result = __xrtTlsSessionRecordProtect(
		pSession, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Next.Message, Next.MessageSize }, 0
	);
	if ( Result != XTLS_OK ) {
		__xrtTlsRecordKeyClear(&Next.WriteKey);
		xrtSecureZero(&Next, sizeof(Next));
		if ( (Result == XTLS_AGAIN) &&
			!__xrtTlsClientWait(pSession, true) ) {
			return __xrtTlsClientFailed(pSession);
		}
		return Result;
	}
	__xrtTlsRecordKeyClear(&pSession->WriteKey);
	pSession->WriteKey = Next.WriteKey;
	xrtSecureZero(&Next.WriteKey, sizeof(Next.WriteKey));
	memcpy(
		pState->ClientApplicationTraffic,
		Next.WriteTraffic, pState->HashSize
	);
	xrtSecureZero(&Next, sizeof(Next));
	if ( !__xrtTlsClientWait(pSession, true) ) {
		return __xrtTlsClientFailed(pSession);
	}
	return XTLS_OK;
}



/* 严格处理 READY 状态下的票据与 KeyUpdate 握手消息。 */
static xtlsresult __xrtTlsClientPostHandshakeRecord(
	xtlssession* pSession,
	xtlsclientstate* pState,
	xbytesview Input,
	size_t* pConsumed,
	bool* pComplete
)
{
	xtlshandshake Message;
	xtlssessionticket Ticket;
	xtlskeyupdate Request;
	xtlsresult Result;
	bool bContinued = (pState->Reader.Size != 0) ||
		(pState->Reader.HeaderSize != 0);

	*pConsumed = 0;
	*pComplete = false;
	if ( pState->Version != XTLS_VERSION_13 ) {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_VERSION, "drive-tls-client",
			"TLS 1.2 does not permit post-handshake messages"
		);
	}
	Result = xrtTlsHandshakeReaderRead(
		&pState->Reader, Input, pConsumed, &Message
	);
	if ( Result == XTLS_ERROR ) {
		return __xrtTlsClientFailed(pSession);
	}
	if ( Result == XTLS_AGAIN ) {
		if ( *pConsumed != Input.Size ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"drive-tls-client",
				"TLS post-handshake reader left an incomplete record suffix"
			);
		}
		return XTLS_OK;
	}
	*pComplete = true;
	if ( Message.Type == XTLS_HANDSHAKE_NEW_SESSION_TICKET ) {
		if ( !xrtTlsSessionTicketParse(
			XTLS_VERSION_13, Message.Body, &Ticket
		) ) {
			return __xrtTlsClientFailed(pSession);
		}
		#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
			if ( !__xrtTlsClientResumePublish(
				pSession, pState, &Ticket
			) ) {
				return __xrtTlsClientFailed(pSession);
			}
		#endif
	} else if ( Message.Type == XTLS_HANDSHAKE_KEY_UPDATE ) {
		if ( bContinued || (pState->RecordOffset != 0) ||
			(*pConsumed != Input.Size) ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"process-key-update",
				"TLS KeyUpdate is not aligned to a record boundary"
			);
		}
		if ( !xrtTlsKeyUpdateParse(Message.Body, &Request) ) {
			return __xrtTlsClientFailed(pSession);
		}
		if ( Request == XTLS_KEY_UPDATE_REQUESTED ) {
			Result = __xrtTlsSessionKeyUpdateWritable(
				pSession, "process-key-update"
			);
			if ( Result != XTLS_OK ) {
				return Result == XTLS_AGAIN ?
					XTLS_AGAIN : __xrtTlsClientFailed(pSession);
			}
		}
		Result = __xrtTlsClientKeyUpdateCommit(
			pSession, pState, Request
		);
		if ( Result != XTLS_OK ) {
			return Result;
		}
	} else {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_HANDSHAKE,
			"drive-tls-client",
			"TLS client received an unsupported post-handshake message"
		);
	}
	if ( !xrtTlsHandshakeReaderReset(&pState->Reader) ) {
		return __xrtTlsClientFailed(pSession);
	}
	return XTLS_OK;
}



/* 处理一段握手记录，并支持跨记录重组及记录内多消息。 */
static xtlsresult __xrtTlsClientHandshakeRecord(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlssessionrecord* pRecord,
	xbytesview Input,
	size_t* pConsumed,
	bool* pComplete
)
{
	xtlshandshake Message;
	xtlsresult Result;

	*pConsumed = 0;
	*pComplete = false;
	if ( (pState->Step == XTLS_CLIENT_WAIT_SERVER_HELLO) &&
		pRecord->Protected ) {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_RECORD_TYPE,
			"drive-tls-client",
			"TLS ServerHello arrived in a protected record"
		);
	}
	if ( (pState->Version == XTLS_VERSION_13) &&
		(pState->Step != XTLS_CLIENT_WAIT_SERVER_HELLO) &&
		!pRecord->Protected ) {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_RECORD_TYPE,
			"drive-tls-client",
			"TLS protected handshake message arrived in a plaintext record"
		);
	}
	if ( (pState->Version == XTLS_VERSION_12) &&
		(pState->Step != XTLS_CLIENT_WAIT_FINISHED) &&
		pRecord->Protected ) {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_RECORD_TYPE,
			"drive-tls-client",
			"TLS 1.2 plaintext handshake arrived in a protected record"
		);
	}
	if ( (pState->Version == XTLS_VERSION_12) &&
		(pState->Step == XTLS_CLIENT_WAIT_FINISHED) &&
		!pRecord->Protected ) {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_RECORD_TYPE,
			"drive-tls-client",
			"TLS 1.2 Finished arrived in a plaintext record"
		);
	}
	Result = xrtTlsHandshakeReaderRead(
		&pState->Reader, Input, pConsumed, &Message
	);
	if ( Result == XTLS_ERROR ) {
		return __xrtTlsClientFailed(pSession);
	}
	if ( Result == XTLS_AGAIN ) {
		if ( *pConsumed != Input.Size ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"drive-tls-client",
				"TLS handshake reader left an incomplete record suffix"
			);
		}
		return XTLS_OK;
	}
	*pComplete = true;
	if ( pState->Step == XTLS_CLIENT_WAIT_SERVER_HELLO ) {
		if ( Message.Type != XTLS_HANDSHAKE_SERVER_HELLO ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"drive-tls-client",
				"TLS client expected ServerHello as the first server message"
			);
		}
		Result = __xrtTlsClientServerHelloCommit(
			pSession, pState, &Message
		);
	} else if ( pState->Step == XTLS_CLIENT_WAIT_ENCRYPTED_EXTENSIONS ) {
		if ( Message.Type != XTLS_HANDSHAKE_ENCRYPTED_EXTENSIONS ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"drive-tls-client",
				"TLS client expected EncryptedExtensions after ServerHello"
			);
		}
		Result = __xrtTlsClientEncryptedExtensionsCommit(
			pSession, pState, &Message
		);
	#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
	} else if ( pState->Step == XTLS_CLIENT_WAIT_CERTIFICATE ) {
		if ( Message.Type != XTLS_HANDSHAKE_CERTIFICATE ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"drive-tls-client",
				"TLS client expected Certificate after EncryptedExtensions"
			);
		}
		Result = __xrtTlsClientCertificateCommit(
			pSession, pState, &Message
		);
	} else if ( pState->Step == XTLS_CLIENT_WAIT_CERTIFICATE_VERIFY ) {
		if ( Message.Type != XTLS_HANDSHAKE_CERTIFICATE_VERIFY ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"drive-tls-client",
				"TLS client expected CertificateVerify after Certificate"
			);
		}
		Result = __xrtTlsClientCertificateVerifyCommit(
			pSession, pState, &Message
		);
	#endif
	} else if ( pState->Step == XTLS_CLIENT_WAIT_FINISHED ) {
		if ( pState->Version == XTLS_VERSION_12 ) {
			#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY) && \
				defined(XRT_FEATURE_TLS_AUTH_MESSAGES) && \
				defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE)
				Result = __xrtTlsClient12Handshake(
					pSession, pState, &Message
				);
			#else
				Result = __xrtTlsClientProtocol(
					pSession, XTLS_ERROR_INTERNAL, "drive-tls-client",
					"TLS 1.2 client state is unavailable"
				);
			#endif
		} else if ( Message.Type != XTLS_HANDSHAKE_FINISHED ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"drive-tls-client",
				"TLS client expected Finished after server authentication"
			);
		} else {
			Result = __xrtTlsClientFinishedCommit(
				pSession, pState, &Message
			);
		}
	#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY) && \
		defined(XRT_FEATURE_TLS_AUTH_MESSAGES) && \
		defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE)
	} else if ( (pState->Version == XTLS_VERSION_12) &&
		((pState->Step == XTLS_CLIENT_WAIT_SERVER_KEY_EXCHANGE) ||
		 (pState->Step == XTLS_CLIENT_WAIT_SERVER_HELLO_DONE)) ) {
		Result = __xrtTlsClient12Handshake(
			pSession, pState, &Message
		);
	#endif
	} else {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_INTERNAL, "drive-tls-client",
			"TLS client handshake step is not implemented"
		);
	}
	if ( (Result == XTLS_OK) &&
		!xrtTlsHandshakeReaderReset(&pState->Reader) ) {
		return __xrtTlsClientFailed(pSession);
	}
	return Result;
}



/* 严格忽略 RFC 8446 兼容 CCS，不让任意明文穿过握手状态机。 */
static xtlsresult __xrtTlsClientChangeCipherSpec(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlssessionrecord* pRecord
)
{
	#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY) && \
		defined(XRT_FEATURE_TLS_AUTH_MESSAGES) && \
		defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE)
		if ( pState->Version == XTLS_VERSION_12 ) {
			return __xrtTlsClient12ChangeCipherSpec(
				pSession, pState, pRecord
			);
		}
	#else
		(void)pState;
	#endif
	if ( (pState->Version != XTLS_VERSION_13) ||
		(pState->Step < XTLS_CLIENT_WAIT_ENCRYPTED_EXTENSIONS) ||
		(pState->Step > XTLS_CLIENT_WAIT_FINISHED) ||
		pState->CompatibilityCcsSeen || pRecord->Protected ||
		(pRecord->Data.Size != 1u) ||
		(pRecord->Data.Data[0] != 1u) ) {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_HANDSHAKE,
			"drive-tls-client",
			"TLS compatibility ChangeCipherSpec is malformed"
		);
	}
	if ( __xrtTlsSessionRecordFinish(pSession, false) != XTLS_OK ) {
		return __xrtTlsClientFailed(pSession);
	}
	pState->CompatibilityCcsSeen = true;
	return XTLS_OK;
}



/* 在上下文公平性预算内推进客户端握手。 */
XRT_API xtlsresult xrtTlsClientDrive(xtlssession* pSession)
{
	xtlsclientstate* pState;
	const xtlslimits* pLimits;
	xtlsstate State;
	uint32 iRecords = 0;
	uint32 iHandshakes = 0;
	bool bProgress = false;

	if ( pSession == NULL ) {
		(void)__xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"drive-tls-client", "TLS client session is null"
		);
		return XTLS_ERROR;
	}
	if ( xrtTlsSessionRole(pSession) != XTLS_CLIENT ) {
		(void)__xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"drive-tls-client", "TLS session is not a client"
		);
		return XTLS_ERROR;
	}
	State = xrtTlsSessionState(pSession);
	if ( State == XTLS_STATE_CLOSED ) {
		return XTLS_CLOSED;
	}
	if ( State == XTLS_STATE_FAILED ) {
		(void)__xrtTlsClientError(
			XERR_STATE, XTLS_ERROR_STATE,
			"drive-tls-client", "TLS client has failed"
		);
		return XTLS_ERROR;
	}
	if ( (State != XTLS_STATE_HANDSHAKE) &&
		(State != XTLS_STATE_READY) &&
		(State != XTLS_STATE_CLOSING) ) {
		(void)__xrtTlsClientError(
			XERR_STATE, XTLS_ERROR_STATE,
			"drive-tls-client", "TLS client cannot be driven in this state"
		);
		return XTLS_ERROR;
	}
	pState = (xtlsclientstate*)__xrtTlsSessionRoleData(pSession);
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( (pState == NULL) || (pLimits == NULL) ||
		(pLimits->RecordBudget == 0) ||
		(pLimits->HandshakeBudget == 0) ) {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_INTERNAL,
			"drive-tls-client", "TLS client role state is invalid"
		);
	}
	while ( iRecords < pLimits->RecordBudget ) {
		xtlssessionrecord Record;
		bool bHandshake = xrtTlsSessionState(pSession) ==
			XTLS_STATE_HANDSHAKE;
		xtlsresult Result = __xrtTlsSessionRecordNext(
			pSession, &Record
		);

		if ( Result == XTLS_AGAIN ) {
			if ( !__xrtTlsClientWait(pSession, true) ) {
				return __xrtTlsClientFailed(pSession);
			}
			return bProgress ? XTLS_OK : XTLS_AGAIN;
		}
		if ( Result != XTLS_OK ) {
			return __xrtTlsClientFailed(pSession);
		}
		if ( pState->RecordOffset > Record.Data.Size ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_INTERNAL,
				"drive-tls-client",
				"TLS client record offset exceeds the pending record"
			);
		}
		if ( Record.Type == XTLS_RECORD_ALERT ) {
			if ( pState->RecordOffset != 0 ) {
				return __xrtTlsClientProtocol(
					pSession, XTLS_ERROR_INTERNAL,
					"drive-tls-client",
					"TLS client changed record type while a record was pending"
				);
			}
			Result = __xrtTlsSessionRecordAlert(pSession, &Record);
			if ( Result == XTLS_ERROR ) {
				return Result;
			}
			iRecords++;
			bProgress = true;
			if ( bHandshake ) {
				return __xrtTlsClientProtocol(
					pSession, XTLS_ERROR_CLOSED,
					"drive-tls-client",
					"TLS peer closed during the client handshake"
				);
			}
			if ( (Result == XTLS_CLOSED) ||
				(xrtTlsSessionState(pSession) == XTLS_STATE_CLOSED) ) {
				return XTLS_CLOSED;
			}
			continue;
		}
		if ( !bHandshake &&
			(Record.Type == XTLS_RECORD_APPLICATION_DATA) ) {
			if ( pState->RecordOffset != 0 ) {
				return __xrtTlsClientProtocol(
					pSession, XTLS_ERROR_INTERNAL,
					"drive-tls-client",
					"TLS client has a partial application record"
				);
			}
			Result = __xrtTlsSessionRecordFinish(
				pSession, !pSession->CloseReceived
			);
			if ( Result == XTLS_AGAIN ) {
				if ( !__xrtTlsClientApplicationWait(pSession) ) {
					return __xrtTlsClientFailed(pSession);
				}
				return bProgress ? XTLS_OK : XTLS_AGAIN;
			}
			if ( Result != XTLS_OK ) {
				return __xrtTlsClientFailed(pSession);
			}
			iRecords++;
			bProgress = true;
			continue;
		}
		if ( !bHandshake && (Record.Type != XTLS_RECORD_HANDSHAKE) ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_RECORD_TYPE,
				"drive-tls-client",
				"TLS client received an unexpected record after the handshake"
			);
		}
		if ( bHandshake &&
			(Record.Type == XTLS_RECORD_CHANGE_CIPHER_SPEC) ) {
			if ( pState->RecordOffset != 0 ) {
				return __xrtTlsClientProtocol(
					pSession, XTLS_ERROR_INTERNAL,
					"drive-tls-client",
					"TLS client has a partial ChangeCipherSpec record"
				);
			}
			Result = __xrtTlsClientChangeCipherSpec(
				pSession, pState, &Record
			);
			if ( Result == XTLS_OK ) {
				iRecords++;
			}
		} else if ( Record.Type == XTLS_RECORD_HANDSHAKE ) {
			xbytesview Remaining;
			size_t iConsumed;
			bool bComplete;

			if ( iHandshakes >= pLimits->HandshakeBudget ) {
				break;
			}
			if ( bHandshake && (pState->Version == XTLS_VERSION_13) &&
				(pState->Step == XTLS_CLIENT_WAIT_FINISHED) ) {
				Result = __xrtTlsClientFinishedWritable(
					pSession, pState
				);
				if ( Result == XTLS_AGAIN ) {
					if ( !__xrtTlsClientWait(pSession, false) ) {
						return __xrtTlsClientFailed(pSession);
					}
					return bProgress ? XTLS_OK : XTLS_AGAIN;
				}
				if ( Result != XTLS_OK ) {
					return __xrtTlsClientFailed(pSession);
				}
			}
			#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY) && \
				defined(XRT_FEATURE_TLS_AUTH_MESSAGES) && \
				defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE)
				if ( bHandshake &&
					(pState->Version == XTLS_VERSION_12) &&
					(pState->Step == XTLS_CLIENT_WAIT_SERVER_HELLO_DONE) ) {
					Result = __xrtTlsClient12FlightWritable(
						pSession, pState
					);
					if ( Result == XTLS_AGAIN ) {
						if ( !__xrtTlsClientWait(pSession, false) ) {
							return __xrtTlsClientFailed(pSession);
						}
						return bProgress ? XTLS_OK : XTLS_AGAIN;
					}
					if ( Result != XTLS_OK ) {
						return __xrtTlsClientFailed(pSession);
					}
				}
			#endif
			if ( bHandshake && __xrtTlsClientPaused(pState) ) {
				if ( !__xrtTlsClientWait(pSession, false) ) {
					return __xrtTlsClientFailed(pSession);
				}
				return bProgress ? XTLS_OK : XTLS_AGAIN;
			}
			Remaining.Data = Record.Data.Data + pState->RecordOffset;
			Remaining.Size = Record.Data.Size - pState->RecordOffset;
			if ( bHandshake ) {
				Result = __xrtTlsClientHandshakeRecord(
					pSession, pState, &Record, Remaining,
					&iConsumed, &bComplete
				);
			} else {
				Result = __xrtTlsClientPostHandshakeRecord(
					pSession, pState, Remaining,
					&iConsumed, &bComplete
				);
			}
			if ( !bHandshake && (Result == XTLS_AGAIN) ) {
				if ( !__xrtTlsClientWait(pSession, false) ) {
					return __xrtTlsClientFailed(pSession);
				}
				return bProgress ? XTLS_OK : XTLS_AGAIN;
			}
			if ( Result != XTLS_OK ) {
				return Result;
			}
			pState->RecordOffset += iConsumed;
			bProgress = bProgress || (iConsumed != 0);
			if ( bComplete ) {
				iHandshakes++;
			}
			if ( bHandshake &&
				(xrtTlsSessionState(pSession) == XTLS_STATE_READY) &&
				(pState->RecordOffset != Record.Data.Size) ) {
				return __xrtTlsClientProtocol(
					pSession, XTLS_ERROR_HANDSHAKE,
					"drive-tls-client",
					"TLS server Finished record contains trailing handshake data"
				);
			}
			if ( pState->RecordOffset == Record.Data.Size ) {
				if ( __xrtTlsSessionRecordFinish(
					pSession, false
				) != XTLS_OK ) {
					return __xrtTlsClientFailed(pSession);
				}
				pState->RecordOffset = 0;
				iRecords++;
			} else if ( !bComplete ) {
				return __xrtTlsClientProtocol(
					pSession, XTLS_ERROR_INTERNAL,
					"drive-tls-client",
					"TLS client did not consume an incomplete record"
				);
			}
			if ( bHandshake &&
				(xrtTlsSessionState(pSession) == XTLS_STATE_READY) ) {
				return XTLS_OK;
			}
			if ( bHandshake && __xrtTlsClientPaused(pState) ) {
				if ( !__xrtTlsClientWait(
					pSession, pState->RecordOffset == 0
				) ) {
					return __xrtTlsClientFailed(pSession);
				}
				return XTLS_OK;
			}
			continue;
		} else {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_RECORD_TYPE,
				"drive-tls-client",
				bHandshake ?
					"TLS client received an unexpected record during the handshake" :
					"TLS client received an unexpected record after the handshake"
			);
		}
		if ( Result != XTLS_OK ) {
			return Result;
		}
		bProgress = true;
	}
	if ( !__xrtTlsClientWait(pSession, false) ) {
		return __xrtTlsClientFailed(pSession);
	}
	return XTLS_OK;
}

#endif
