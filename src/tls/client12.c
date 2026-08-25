#include "../internal/xrt_tls_client.h"



#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY) && \
	defined(XRT_FEATURE_TLS_AUTH_MESSAGES) && \
	defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE)

#define XTLS_CLIENT12_PUBLIC_MAX_SIZE 97u
#define XTLS_CLIENT12_KEY_EXCHANGE_MAX_SIZE \
	(XTLS_HANDSHAKE_HEADER_SIZE + 1u + XTLS_CLIENT12_PUBLIC_MAX_SIZE)
#define XTLS_CLIENT12_FINISHED_MESSAGE_SIZE \
	(XTLS_HANDSHAKE_HEADER_SIZE + XTLS12_FINISHED_SIZE)

static const uint8 __xrtTls12ClientDowngrade13[] = {
	0x44, 0x4F, 0x57, 0x4E, 0x47, 0x52, 0x44, 0x01
};



/* 保留当前根错误并把客户端会话推进到失败终态。 */
static xtlsresult __xrtTlsClient12Failed(xtlssession* pSession)
{
	return __xrtTlsSessionFail(pSession);
}



/* 返回握手消息包含四字节头部的完整借用视图。 */
static xbytesview __xrtTlsClient12Encoded(const xtlshandshake* pMessage)
{
	xbytesview Encoded;

	Encoded.Data = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pMessage->EncodedSize;
	return Encoded;
}



/* 校验 TLS 1.2 ServerHello 的固定字段、套件和允许扩展。 */
static bool __xrtTlsClient12Hello(
	const xtlsclientstate* pState,
	const xtlsserverhello* pHello,
	const xtlscipherinfo** ppCipher,
	xbytesview* pProtocol
)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;
	const xtlscipherinfo* pCipher;
	bool bExtendedMasterSecret = false;

	*ppCipher = NULL;
	memset(pProtocol, 0, sizeof(*pProtocol));
	if ( pHello->Retry || (pHello->LegacyVersion != XTLS_VERSION_12) ||
		(pHello->CompressionMethod != 0) ||
		(pHello->SessionId.Size > XTLS_SESSION_ID_MAX) ||
		(pHello->Random.Size != XTLS12_RANDOM_SIZE) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"process-tls12-server-hello",
			"TLS 1.2 ServerHello fixed fields are invalid"
		);
	}
	if ( pState->Offer13 && xrtConstTimeEqual(
		pHello->Random.Data + XTLS12_RANDOM_SIZE -
			sizeof(__xrtTls12ClientDowngrade13),
		__xrtTls12ClientDowngrade13,
		sizeof(__xrtTls12ClientDowngrade13)
	) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_VERSION,
			"process-tls12-server-hello",
			"TLS 1.2 ServerHello contains a TLS 1.3 downgrade marker"
		);
	}
	if ( !__xrtTlsClientOffered(
		pState->Ciphers, pState->CipherCount, pHello->CipherSuite
	) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_CIPHER,
			"process-tls12-server-hello",
			"TLS 1.2 server selected a cipher that was not offered"
		);
	}
	pCipher = xrtTlsCipherInfo((xtlscipher)pHello->CipherSuite);
	if ( (pCipher == NULL) || (pCipher->Version != XTLS_VERSION_12) ||
		(pCipher->HashSize > pState->SecretCapacity) ||
		!__xrtTlsRecordCipherSupported(
			XTLS_VERSION_12, (xtlscipher)pHello->CipherSuite
		) ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_CIPHER,
			"process-tls12-server-hello",
			"TLS server selected an unusable TLS 1.2 cipher"
		);
	}
	if ( !xrtTlsExtensionsInit(&Cursor, pHello->Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		if ( Extension.Type == XTLS_EXTENSION_EXTENDED_MASTER_SECRET ) {
			if ( Extension.Data.Size != 0 ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
					"process-tls12-server-hello",
					"TLS extended_master_secret acknowledgement is not empty"
				);
			}
			bExtendedMasterSecret = true;
		} else if ( Extension.Type == XTLS_EXTENSION_SERVER_NAME ) {
			if ( (pState->SniName.Size == 0) ||
				(Extension.Data.Size != 0) ) {
				return __xrtTlsClientError(
					XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
					"process-tls12-server-hello",
					"TLS server_name acknowledgement is invalid"
				);
			}
		} else if ( Extension.Type == XTLS_EXTENSION_ALPN ) {
			xbytesview Selected;

			if ( (pState->ProtocolCount == 0) ||
				!xrtTlsProtocolSelected(Extension.Data, &Selected) ||
				!__xrtTlsClientProtocolSelect(
					pState, Selected, pProtocol,
					"process-tls12-server-hello"
				) ) {
				return false;
			}
		} else {
			return __xrtTlsClientError(
				XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
				"process-tls12-server-hello",
				"TLS 1.2 ServerHello contains an unoffered extension"
			);
		}
	}
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( !bExtendedMasterSecret ) {
		return __xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
			"process-tls12-server-hello",
			"TLS 1.2 server did not negotiate extended master secret"
		);
	}
	*ppCipher = pCipher;
	return true;
}



/* 验证并提交 TLS 1.2 ServerHello 和初始 transcript。 */
xtlsresult __xrtTlsClient12ServerHello(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage,
	const xtlsserverhello* pHello
)
{
	const xtlscipherinfo* pCipher;
	xtlstranscript Next;
	xbytesview Protocol;
	xbytesview Encoded = __xrtTlsClient12Encoded(pMessage);
	xcryptohash Hash;
	bool bCommitted = false;

	memset(&Next, 0, sizeof(Next));
	if ( !__xrtTlsClient12Hello(
		pState, pHello, &pCipher, &Protocol
	) ) {
		goto cleanup;
	}
	Hash = __xrtTlsHash(pCipher->Hash);
	if ( !__xrtTlsTranscriptInit(&Next, Hash) ||
		!__xrtTlsTranscriptUpdate(
			&Next,
			(xbytesview) { pState->ClientHello, pState->ClientHelloSize }
		) || !__xrtTlsTranscriptUpdate(&Next, Encoded) ) {
		goto cleanup;
	}
	if ( ((Protocol.Size != 0) &&
		 !__xrtTlsSessionSetProtocol(pSession, Protocol)) ||
		!__xrtTlsSessionNegotiated(
			pSession, XTLS_VERSION_12, pCipher->Cipher
		) ) {
		goto cleanup;
	}

	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	memset(&Next, 0, sizeof(Next));
	memcpy(pState->ServerRandom, pHello->Random.Data, XTLS12_RANDOM_SIZE);
	pState->HashSize = pCipher->HashSize;
	pState->Cipher = pCipher->Cipher;
	pState->Version = XTLS_VERSION_12;
	pState->ExtendedMasterSecret = true;
	pState->Step = XTLS_CLIENT_WAIT_CERTIFICATE;
	bCommitted = __xrtTlsClientWait(pSession, true);

cleanup:
	__xrtTlsTranscriptClear(&Next);
	return bCommitted ? XTLS_OK : __xrtTlsClient12Failed(pSession);
}



/* 验证服务端 ECDHE 参数和签名，再生成客户端临时密钥与共享秘密。 */
static xtlsresult __xrtTlsClient12ServerKeyExchange(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
)
{
	xtls12serverkeyexchange Exchange;
	const xtlsgroupinfo* pGroup;
	const xtlssignatureinfo* pSignature;
	xtlstranscript Next = pState->Transcript;
	xbytesview Encoded = __xrtTlsClient12Encoded(pMessage);

	if ( (pState->Peer == NULL) ||
		!xrtTls12ServerKeyExchangeParse(pMessage->Body, &Exchange) ) {
		goto failed;
	}
	pGroup = xrtTlsGroupInfo(Exchange.Group);
	pSignature = xrtTlsSignatureInfo(
		(xtlssignature)Exchange.Verify.Scheme
	);
	if ( !__xrtTlsClientOffered(
		pState->Groups, pState->GroupCount, Exchange.Group
	) || !__xrtTlsClientOffered(
		pState->Signatures, pState->SignatureCount, Exchange.Verify.Scheme
	) || (pGroup == NULL) || (pSignature == NULL) ||
		!xrtTlsCipherCompatible(
			XTLS_VERSION_12, pState->Cipher, pSignature->Identity
		) ||
		(pGroup->PrivateSize > pState->PrivateKeyCapacity) ||
		(pGroup->PublicSize > pState->PublicKeyCapacity) ||
		(pGroup->SharedSize > sizeof(pState->Shared)) ) {
		(void)__xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_KEY_EXCHANGE,
			"process-tls12-server-key-exchange",
			"TLS 1.2 server selected an unoffered or oversized key exchange"
		);
		goto failed;
	}
	if ( !xrtTls12ServerKeyExchangeVerify(
		(xtlssignature)Exchange.Verify.Scheme,
		(xbytesview) { pState->Random, XTLS12_RANDOM_SIZE },
		(xbytesview) { pState->ServerRandom, XTLS12_RANDOM_SIZE },
		Exchange.Parameters, Exchange.Verify.Signature,
		&pState->Peer->PublicKey
	) ) {
		goto failed;
	}

	/* TLS 1.2 使用独立临时密钥，不复用 ClientHello 中的 TLS 1.3 key_share。 */
	xrtSecureZero(pState->PrivateKey, pState->PrivateKeyCapacity);
	xrtSecureZero(pState->PublicKey, pState->PublicKeyCapacity);
	xrtSecureZero(pState->Shared, sizeof(pState->Shared));
	if ( !xrtTlsKeyShareGenerate(
		Exchange.Group,
		pState->PrivateKey, pState->PrivateKeyCapacity,
		pState->PublicKey, pState->PublicKeyCapacity
	) || !xrtTlsKeyShareDerive(
		Exchange.Group,
		(xbytesview) { pState->PrivateKey, pGroup->PrivateSize },
		Exchange.PublicKey, pState->Shared, sizeof(pState->Shared)
	) || !__xrtTlsTranscriptUpdate(&Next, Encoded) ) {
		goto failed;
	}

	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	pState->Group = Exchange.Group;
	pState->PrivateKeySize = pGroup->PrivateSize;
	pState->PublicKeySize = pGroup->PublicSize;
	pState->SharedSize = pGroup->SharedSize;
	pState->Step = XTLS_CLIENT_WAIT_SERVER_HELLO_DONE;
	return XTLS_OK;

failed:
	__xrtTlsTranscriptClear(&Next);
	return __xrtTlsClient12Failed(pSession);
}



/* 计算完整客户端 TLS 1.2 航班在线路和发送队列中的长度。 */
static bool __xrtTlsClient12FlightSize(
	const xtlsclientstate* pState,
	size_t* pSize
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(pState->Cipher);
	size_t iBody;
	size_t iKeyExchange;
	size_t iKeyExchangeRecord;
	size_t iChangeCipherSpecRecord;
	size_t iFinishedRecord;

	*pSize = 0;
	if ( (pCipher == NULL) || (pCipher->Version != XTLS_VERSION_12) ||
		(pState->PublicKeySize == 0) ||
		(pState->PublicKeySize > XTLS_CLIENT12_PUBLIC_MAX_SIZE) ) {
		return __xrtTlsClientError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"size-tls12-client-flight",
			"TLS 1.2 client flight state is invalid"
		);
	}
	iBody = xrtTls12ClientKeyExchangeSize(
		(xbytesview) { pState->PublicKey, pState->PublicKeySize }
	);
	iKeyExchange = xrtTlsHandshakeSize(iBody);
	iKeyExchangeRecord = xrtTlsRecordSize(iKeyExchange);
	iChangeCipherSpecRecord = xrtTlsRecordSize(1u);
	iFinishedRecord = XTLS_RECORD_HEADER_SIZE + pCipher->ExplicitNonceSize +
		XTLS_CLIENT12_FINISHED_MESSAGE_SIZE + pCipher->TagSize;
	if ( (iBody == 0) || (iKeyExchange == 0) ||
		(iKeyExchangeRecord == 0) || (iChangeCipherSpecRecord == 0) ) {
		return false;
	}
	*pSize = iKeyExchangeRecord + iChangeCipherSpecRecord + iFinishedRecord;
	return true;
}



/* 在消费 ServerHelloDone 前检查完整客户端航班能否原子进入发送队列。 */
xtlsresult __xrtTlsClient12FlightWritable(
	xtlssession* pSession,
	const xtlsclientstate* pState
)
{
	const xtlslimits* pLimits = xrtTlsContextLimits(pSession->Context);
	size_t iFlight;
	size_t iQueued = xrtTlsSessionSendSize(pSession);

	if ( (pLimits == NULL) ||
		!__xrtTlsClient12FlightSize(pState, &iFlight) ) {
		return XTLS_ERROR;
	}
	if ( iFlight > pLimits->SendLimit ) {
		(void)__xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			"queue-tls12-client-flight",
			"TLS send limit cannot hold the mandatory TLS 1.2 client flight"
		);
		return XTLS_ERROR;
	}
	if ( (iQueued > pLimits->SendLimit) ||
		(iFlight > (pLimits->SendLimit - iQueued)) ) {
		return XTLS_AGAIN;
	}
	return XTLS_OK;
}



/* 构造并原子排队 ClientKeyExchange、CCS 和受保护 Finished。 */
static xtlsresult __xrtTlsClient12ServerHelloDone(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
)
{
	xtls12keymaterial Material;
	xtlsrecordkey WriteKey;
	xtlsrecordkey ReadKey;
	xtlstranscript Next = pState->Transcript;
	uint8 KeyExchange[XTLS_CLIENT12_KEY_EXCHANGE_MAX_SIZE];
	uint8 Finished[XTLS_CLIENT12_FINISHED_MESSAGE_SIZE];
	uint8 SessionHash[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 HandshakeHash[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 VerifyData[XTLS12_FINISHED_SIZE];
	uint8 ChangeCipherSpec = 1u;
	uint8* pOutput = NULL;
	size_t iBody;
	size_t iKeyExchange;
	size_t iKeyExchangeRecord;
	size_t iChangeCipherSpecRecord;
	size_t iFinishedRecord;
	size_t iFlight;
	size_t iWritten;
	xtlsresult Result = XTLS_ERROR;
	bool bOutputMoved = false;

	memset(&Material, 0, sizeof(Material));
	memset(&WriteKey, 0, sizeof(WriteKey));
	memset(&ReadKey, 0, sizeof(ReadKey));
	memset(KeyExchange, 0, sizeof(KeyExchange));
	memset(Finished, 0, sizeof(Finished));
	memset(SessionHash, 0, sizeof(SessionHash));
	memset(HandshakeHash, 0, sizeof(HandshakeHash));
	memset(VerifyData, 0, sizeof(VerifyData));
	if ( (pMessage->Type != XTLS_HANDSHAKE_SERVER_HELLO_DONE) ||
		(pMessage->Body.Size != 0) ||
		(__xrtTlsClient12FlightWritable(pSession, pState) != XTLS_OK) ) {
		if ( (pMessage->Type != XTLS_HANDSHAKE_SERVER_HELLO_DONE) ||
			(pMessage->Body.Size != 0) ) {
			(void)__xrtTlsClientError(
				XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
				"process-tls12-server-hello-done",
				"TLS 1.2 client expected an empty ServerHelloDone"
			);
		}
		goto cleanup;
	}
	if ( !__xrtTlsTranscriptUpdate(
		&Next, __xrtTlsClient12Encoded(pMessage)
	) ) {
		goto cleanup;
	}
	iBody = xrtTls12ClientKeyExchangeSize(
		(xbytesview) { pState->PublicKey, pState->PublicKeySize }
	);
	iKeyExchange = xrtTlsHandshakeSize(iBody);
	if ( (iKeyExchange > sizeof(KeyExchange)) ||
		!xrtTls12ClientKeyExchangeEncode(
			(xbytesview) { pState->PublicKey, pState->PublicKeySize },
			KeyExchange + XTLS_HANDSHAKE_HEADER_SIZE, iBody
		) || !xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE,
			(xbytesview) {
				KeyExchange + XTLS_HANDSHAKE_HEADER_SIZE, iBody
			}, KeyExchange, sizeof(KeyExchange)
		) || !__xrtTlsTranscriptUpdate(
			&Next, (xbytesview) { KeyExchange, iKeyExchange }
		) || !__xrtTlsTranscriptDigest(
			&Next, SessionHash, pState->HashSize
		) || !__xrtTls12KeyMaterial(
			pState->Cipher,
			(xbytesview) { pState->Shared, pState->SharedSize },
			(xbytesview) { SessionHash, pState->HashSize },
			(xbytesview) { pState->Random, XTLS12_RANDOM_SIZE },
			(xbytesview) { pState->ServerRandom, XTLS12_RANDOM_SIZE },
			&Material
		) ) {
		goto cleanup;
	}
	if ( !__xrtTlsRecordKeyInit(
		&WriteKey, XTLS_VERSION_12, pState->Cipher,
		(xbytesview) { Material.ClientKey, Material.KeySize },
		(xbytesview) { Material.ClientIv, Material.IvSize }
	) || !__xrtTlsRecordKeyInit(
		&ReadKey, XTLS_VERSION_12, pState->Cipher,
		(xbytesview) { Material.ServerKey, Material.KeySize },
		(xbytesview) { Material.ServerIv, Material.IvSize }
	) || !__xrtTlsTranscriptDigest(
		&Next, HandshakeHash, pState->HashSize
	) || !__xrtTls12Finished(
		Material.Hash,
		(xbytesview) { Material.Master, sizeof(Material.Master) },
		false,
		(xbytesview) { HandshakeHash, pState->HashSize },
		VerifyData, sizeof(VerifyData)
	) || !xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_FINISHED,
		(xbytesview) { VerifyData, sizeof(VerifyData) },
		Finished, sizeof(Finished)
	) || !__xrtTlsTranscriptUpdate(
		&Next, (xbytesview) { Finished, sizeof(Finished) }
	) ) {
		goto cleanup;
	}

	iKeyExchangeRecord = xrtTlsRecordSize(iKeyExchange);
	iChangeCipherSpecRecord = xrtTlsRecordSize(1u);
	iFinishedRecord = __xrtTlsRecordSealSize(
		&WriteKey, sizeof(Finished), 0
	);
	iFlight = iKeyExchangeRecord + iChangeCipherSpecRecord + iFinishedRecord;
	pOutput = (uint8*)xrtMalloc(iFlight);
	if ( (pOutput == NULL) || !xrtTlsRecordEncode(
		XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
		(xbytesview) { KeyExchange, iKeyExchange },
		pOutput, iKeyExchangeRecord
	) || !xrtTlsRecordEncode(
		XTLS_RECORD_CHANGE_CIPHER_SPEC, XTLS_VERSION_12,
		(xbytesview) { &ChangeCipherSpec, 1u },
		pOutput + iKeyExchangeRecord, iChangeCipherSpecRecord
	) || !__xrtTlsRecordSeal(
		&WriteKey, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Finished, sizeof(Finished) }, 0,
		pOutput + iKeyExchangeRecord + iChangeCipherSpecRecord,
		iFinishedRecord, &iWritten
	) || (iWritten != iFinishedRecord) ) {
		goto cleanup;
	}
	Result = __xrtTlsSessionSendTake(pSession, pOutput, iFlight);
	if ( Result != XTLS_OK ) {
		goto cleanup;
	}
	bOutputMoved = true;

	/* 输出已完整转交后，提交双方记录 epoch 和握手 transcript。 */
	__xrtTlsRecordKeyClear(&pSession->WriteKey);
	pSession->WriteKey = WriteKey;
	memset(&WriteKey, 0, sizeof(WriteKey));
	__xrtTlsRecordKeyClear(&pState->PendingReadKey);
	pState->PendingReadKey = ReadKey;
	memset(&ReadKey, 0, sizeof(ReadKey));
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	memset(&Next, 0, sizeof(Next));
	memcpy(pState->Master, Material.Master, sizeof(pState->Master));
	pState->Step = XTLS_CLIENT_WAIT_CHANGE_CIPHER_SPEC;
	xrtSecureZero(pState->PrivateKey, pState->PrivateKeyCapacity);
	xrtSecureZero(pState->Shared, sizeof(pState->Shared));
	Result = __xrtTlsClientWait(pSession, true) ? XTLS_OK : XTLS_ERROR;

cleanup:
	if ( !bOutputMoved ) {
		xrtFree(pOutput);
	}
	__xrtTlsRecordKeyClear(&ReadKey);
	__xrtTlsRecordKeyClear(&WriteKey);
	__xrtTlsTranscriptClear(&Next);
	xrtSecureZero(VerifyData, sizeof(VerifyData));
	xrtSecureZero(HandshakeHash, sizeof(HandshakeHash));
	xrtSecureZero(SessionHash, sizeof(SessionHash));
	xrtSecureZero(Finished, sizeof(Finished));
	xrtSecureZero(KeyExchange, sizeof(KeyExchange));
	xrtSecureZero(&Material, sizeof(Material));
	return Result == XTLS_OK ? XTLS_OK : __xrtTlsClient12Failed(pSession);
}



/* 验证服务端 Finished，并把 TLS 1.2 会话推进到应用数据状态。 */
static xtlsresult __xrtTlsClient12Finished(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
)
{
	xtlstranscript Next = pState->Transcript;
	xbytesview VerifyData;
	uint8 HandshakeHash[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 Expected[XTLS12_FINISHED_SIZE];
	uint32 iWait = XTLS_WAIT_INPUT;
	bool bVerified;

	memset(HandshakeHash, 0, sizeof(HandshakeHash));
	memset(Expected, 0, sizeof(Expected));
	bVerified = (pMessage->Type == XTLS_HANDSHAKE_FINISHED) &&
		xrtTlsFinishedParse(
			pMessage->Body, XTLS12_FINISHED_SIZE, &VerifyData
		) && __xrtTlsTranscriptDigest(
			&pState->Transcript, HandshakeHash, pState->HashSize
		) && __xrtTls12Finished(
			pState->Transcript.Hash,
			(xbytesview) { pState->Master, sizeof(pState->Master) },
			true,
			(xbytesview) { HandshakeHash, pState->HashSize },
			Expected, sizeof(Expected)
		) && xrtConstTimeEqual(
			Expected, VerifyData.Data, sizeof(Expected)
		);
	if ( !bVerified ) {
		(void)__xrtTlsClientError(
			XERR_PROTOCOL, XTLS_ERROR_VERIFY,
			"process-tls12-server-finished",
			"TLS 1.2 server Finished verification failed"
		);
		goto failed;
	}
	if ( !__xrtTlsTranscriptUpdate(
		&Next, __xrtTlsClient12Encoded(pMessage)
	) ) {
		goto failed;
	}

	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	memset(&Next, 0, sizeof(Next));
	xrtSecureZero(pState->Master, sizeof(pState->Master));
	pState->Step = XTLS_CLIENT_READY;
	if ( xrtTlsSessionSendSize(pSession) != 0 ) {
		iWait |= XTLS_WAIT_OUTPUT;
	}
	if ( !__xrtTlsSessionSetState(
		pSession, XTLS_STATE_READY
	) || !__xrtTlsSessionSetWait(pSession, iWait) ) {
		goto failed;
	}
	xrtSecureZero(Expected, sizeof(Expected));
	xrtSecureZero(HandshakeHash, sizeof(HandshakeHash));
	return XTLS_OK;

failed:
	__xrtTlsTranscriptClear(&Next);
	xrtSecureZero(Expected, sizeof(Expected));
	xrtSecureZero(HandshakeHash, sizeof(HandshakeHash));
	return __xrtTlsClient12Failed(pSession);
}



/* 按客户端 TLS 1.2 当前阶段分派服务端认证和 Finished 消息。 */
xtlsresult __xrtTlsClient12Handshake(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
)
{
	if ( pState->Step == XTLS_CLIENT_WAIT_SERVER_KEY_EXCHANGE ) {
		if ( pMessage->Type != XTLS_HANDSHAKE_SERVER_KEY_EXCHANGE ) {
			return __xrtTlsClientProtocol(
				pSession, XTLS_ERROR_HANDSHAKE,
				"drive-tls12-client",
				"TLS 1.2 client expected ServerKeyExchange"
			);
		}
		return __xrtTlsClient12ServerKeyExchange(
			pSession, pState, pMessage
		);
	}
	if ( pState->Step == XTLS_CLIENT_WAIT_SERVER_HELLO_DONE ) {
		return __xrtTlsClient12ServerHelloDone(
			pSession, pState, pMessage
		);
	}
	if ( pState->Step == XTLS_CLIENT_WAIT_FINISHED ) {
		return __xrtTlsClient12Finished(pSession, pState, pMessage);
	}
	return __xrtTlsClientProtocol(
		pSession, XTLS_ERROR_INTERNAL, "drive-tls12-client",
		"TLS 1.2 client handshake step is invalid"
	);
}



/* 验证 CCS 并原子启用服务端写入方向对应的接收 epoch。 */
xtlsresult __xrtTlsClient12ChangeCipherSpec(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlssessionrecord* pRecord
)
{
	if ( (pState->Step != XTLS_CLIENT_WAIT_CHANGE_CIPHER_SPEC) ||
		pRecord->Protected || (pRecord->Data.Size != 1u) ||
		(pRecord->Data.Data[0] != 1u) ||
		!pState->PendingReadKey.Ready ) {
		return __xrtTlsClientProtocol(
			pSession, XTLS_ERROR_HANDSHAKE,
			"process-tls12-change-cipher-spec",
			"TLS 1.2 ChangeCipherSpec is malformed or out of order"
		);
	}
	if ( __xrtTlsSessionRecordFinish(pSession, false) != XTLS_OK ) {
		return __xrtTlsClient12Failed(pSession);
	}
	__xrtTlsRecordKeyClear(&pSession->ReadKey);
	pSession->ReadKey = pState->PendingReadKey;
	memset(&pState->PendingReadKey, 0, sizeof(pState->PendingReadKey));
	pState->Step = XTLS_CLIENT_WAIT_FINISHED;
	if ( !__xrtTlsClientWait(pSession, true) ) {
		return __xrtTlsClient12Failed(pSession);
	}
	return XTLS_OK;
}

#endif
