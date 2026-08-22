#include "../internal/xrt_tls_server.h"



#if defined(XRT_FEATURE_TLS_SERVER)

#define XTLS_SERVER12_PUBLIC_MAX_SIZE 97u
#define XTLS_SERVER12_PARAMETER_MAX_SIZE \
	(4u + XTLS_SERVER12_PUBLIC_MAX_SIZE)
#define XTLS_SERVER12_SIGN_CONTENT_MAX_SIZE \
	((2u * XTLS12_RANDOM_SIZE) + XTLS_SERVER12_PARAMETER_MAX_SIZE)
#define XTLS_SERVER12_EXTENSION_MAX_SIZE 272u
#define XTLS_SERVER12_FINISHED_MESSAGE_SIZE \
	(XTLS_HANDSHAKE_HEADER_SIZE + XTLS12_FINISHED_SIZE)



/* 保留根错误并把服务端会话推进到失败终态。 */
static xtlsresult __xrtTlsServer12Failed(xtlssession* pSession)
{
	return __xrtTlsSessionFail(pSession);
}



/* 返回握手消息包含四字节头部的完整借用视图。 */
static xbytesview __xrtTlsServer12Encoded(const xtlshandshake* pMessage)
{
	xbytesview Encoded;

	Encoded.Data = pMessage->Body.Data - XTLS_HANDSHAKE_HEADER_SIZE;
	Encoded.Size = pMessage->EncodedSize;
	return Encoded;
}



/* 安全累加首航动态尺寸。 */
static bool __xrtTlsServer12Size(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		return __xrtTlsServerError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "build-tls12-server-flight",
			"TLS 1.2 server flight size overflows"
		);
	}
	*pSize += iAdd;
	return true;
}



/* 检查单条握手消息是否位于上下文正文硬上限内。 */
static bool __xrtTlsServer12HandshakeLimit(
	const xtlssession* pSession,
	size_t iMessage,
	cstr sMessage
)
{
	const xtlslimits* pLimits = xrtTlsContextLimits(pSession->Context);

	if ( (pLimits != NULL) &&
		(iMessage >= XTLS_HANDSHAKE_HEADER_SIZE) &&
		((iMessage - XTLS_HANDSHAKE_HEADER_SIZE) <=
		 pLimits->HandshakeLimit) ) {
		return true;
	}
	return __xrtTlsServerError(
		XERR_RANGE, XTLS_ERROR_LIMIT,
		"build-tls12-server-flight", sMessage
	);
}



/* 把正文已经位于头部之后的消息编码为完整握手消息。 */
static bool __xrtTlsServer12HandshakeEncode(
	xtlshandshaketype Type,
	bytes pMessage,
	size_t iMessage
)
{
	return (pMessage != NULL) &&
		(iMessage >= XTLS_HANDSHAKE_HEADER_SIZE) &&
		xrtTlsHandshakeEncode(
			Type,
			(xbytesview) {
				pMessage + XTLS_HANDSHAKE_HEADER_SIZE,
				iMessage - XTLS_HANDSHAKE_HEADER_SIZE
			}, pMessage, iMessage
		);
}



/* 把完整明文握手流拆分成协议允许的记录并一次分配输出。 */
static bool __xrtTlsServer12Records(
	xbytesview Stream,
	bytes* ppOutput,
	size_t* pOutputSize
)
{
	bytes pOutput;
	size_t iOutput = 0;
	size_t iOffset = 0;
	size_t iWrite = 0;

	*ppOutput = NULL;
	*pOutputSize = 0;
	while ( iOffset < Stream.Size ) {
		size_t iChunk = Stream.Size - iOffset;

		if ( iChunk > XTLS_RECORD_PLAINTEXT_MAX ) {
			iChunk = XTLS_RECORD_PLAINTEXT_MAX;
		}
		if ( !__xrtTlsServer12Size(
			&iOutput, xrtTlsRecordSize(iChunk)
		) ) {
			return false;
		}
		iOffset += iChunk;
	}
	pOutput = (bytes)xrtMalloc(iOutput);
	if ( pOutput == NULL ) {
		return __xrtTlsServerCause(
			"build-tls12-server-flight",
			"TLS 1.2 server record allocation failed"
		);
	}
	iOffset = 0;
	while ( iOffset < Stream.Size ) {
		size_t iChunk = Stream.Size - iOffset;
		size_t iRecord;

		if ( iChunk > XTLS_RECORD_PLAINTEXT_MAX ) {
			iChunk = XTLS_RECORD_PLAINTEXT_MAX;
		}
		iRecord = xrtTlsRecordSize(iChunk);
		if ( !xrtTlsRecordEncode(
			XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
			(xbytesview) { Stream.Data + iOffset, iChunk },
			pOutput + iWrite, iRecord
		) ) {
			xrtSecureZero(pOutput, iOutput);
			xrtFree(pOutput);
			return false;
		}
		iOffset += iChunk;
		iWrite += iRecord;
	}
	*ppOutput = pOutput;
	*pOutputSize = iOutput;
	return true;
}



/* 构造 ServerHello 的 EMS、SNI 和可选 ALPN 扩展。 */
static bool __xrtTlsServer12Extensions(
	const xtlsserverstate* pState,
	const xtlsserverselection* pSelection,
	uint8* pOutput,
	size_t iCapacity,
	xbytesview* pExtensions
)
{
	xtlswriter Writer;
	xbytesview Empty = { NULL, 0 };

	if ( !xrtTlsWriterInit(&Writer, pOutput, iCapacity) ||
		!xrtTlsWriterExtension(
			&Writer, XTLS_EXTENSION_EXTENDED_MASTER_SECRET, Empty
		) ) {
		return false;
	}
	if ( (pSelection->ServerName.Size != 0) &&
		!xrtTlsWriterExtension(
			&Writer, XTLS_EXTENSION_SERVER_NAME, Empty
		) ) {
		return false;
	}
	if ( (pSelection->Protocol != XTLS_SERVER_PROTOCOL_NONE) &&
		!xrtTlsWriterProtocols(
			&Writer, &pState->Protocols[pSelection->Protocol], 1u
		) ) {
		return false;
	}
	*pExtensions = xrtTlsWriterData(&Writer);
	return true;
}



/* 深复制最终 SNI，供首航输入释放后继续公开查询。 */
static bytes __xrtTlsServer12NameCopy(xbytesview ServerName)
{
	bytes pName;

	if ( ServerName.Size == 0 ) {
		return NULL;
	}
	pName = (bytes)xrtMalloc(ServerName.Size + 1u);
	if ( pName == NULL ) {
		(void)__xrtTlsServerCause(
			"build-tls12-server-flight",
			"TLS 1.2 server name allocation failed"
		);
		return NULL;
	}
	memcpy(pName, ServerName.Data, ServerName.Size);
	pName[ServerName.Size] = 0;
	return pName;
}



/* 从共享选择结果构造并提交完整 TLS 1.2 服务端首航。 */
xtlsresult __xrtTlsServer12FirstFlight(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage,
	xtlsserverselection* pSelection
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(pSelection->Cipher);
	const xtlsgroupinfo* pGroup = xrtTlsGroupInfo(pSelection->Group);
	xtlscertificateentry* pEntries = NULL;
	xtlscertificateverify Verify;
	xtlsserverhello Hello;
	xtlstranscript Transcript;
	xbytesview Empty = { NULL, 0 };
	xbytesview Extensions;
	uint8 ExtensionStorage[XTLS_SERVER12_EXTENSION_MAX_SIZE];
	uint8 PrivateKey[56u];
	uint8 PublicKey[XTLS_SERVER12_PUBLIC_MAX_SIZE];
	uint8 Parameters[XTLS_SERVER12_PARAMETER_MAX_SIZE];
	uint8 SignContent[XTLS_SERVER12_SIGN_CONTENT_MAX_SIZE];
	uint8 ServerRandom[XTLS12_RANDOM_SIZE];
	bytes pSignature = NULL;
	bytes pStream = NULL;
	bytes pOutput = NULL;
	bytes pServerName = NULL;
	size_t iCertificateCount;
	size_t iServerHelloBody;
	size_t iServerHello;
	size_t iCertificateBody;
	size_t iCertificate;
	size_t iParameters;
	size_t iSignContent;
	size_t iSignature = 0;
	size_t iKeyExchangeBody;
	size_t iKeyExchange;
	size_t iHelloDone;
	size_t iStream = 0;
	size_t iOffset = 0;
	size_t iOutput = 0;
	uint8 Compression = 0;
	xtlsresult Result = XTLS_ERROR;
	bool bOutputMoved = false;

	memset(&Verify, 0, sizeof(Verify));
	memset(&Hello, 0, sizeof(Hello));
	memset(&Transcript, 0, sizeof(Transcript));
	memset(ExtensionStorage, 0, sizeof(ExtensionStorage));
	memset(PrivateKey, 0, sizeof(PrivateKey));
	memset(PublicKey, 0, sizeof(PublicKey));
	memset(Parameters, 0, sizeof(Parameters));
	memset(SignContent, 0, sizeof(SignContent));
	memset(ServerRandom, 0, sizeof(ServerRandom));
	if ( (pCipher == NULL) || (pCipher->Version != XTLS_VERSION_12) ||
		(pGroup == NULL) ||
		(pGroup->PrivateSize > sizeof(PrivateKey)) ||
		(pGroup->PublicSize > sizeof(PublicKey)) ||
		(pSelection->Hello.Random.Size != XTLS12_RANDOM_SIZE) ) {
		(void)__xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"build-tls12-server-flight",
			"TLS 1.2 server selection state is invalid"
		);
		goto cleanup;
	}
	if ( !xrtSecureRandom(ServerRandom, sizeof(ServerRandom)) ||
		!xrtTlsKeyShareGenerate(
			pSelection->Group,
			PrivateKey, sizeof(PrivateKey),
			PublicKey, sizeof(PublicKey)
		) || !__xrtTlsServer12Extensions(
			pState, pSelection, ExtensionStorage,
			sizeof(ExtensionStorage), &Extensions
		) ) {
		goto cleanup;
	}

	/* ServerHello 不发布 TLS 1.2 会话恢复标识，只承诺完整 EMS 握手。 */
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = (xbytesview) { ServerRandom, sizeof(ServerRandom) };
	Hello.SessionId = Empty;
	Hello.CipherSuite = (uint16)pSelection->Cipher;
	Hello.CompressionMethod = Compression;
	Hello.Extensions = Extensions;
	iServerHelloBody = xrtTlsServerHelloSize(&Hello);
	iServerHello = xrtTlsHandshakeSize(iServerHelloBody);

	iCertificateCount = xrtTlsIdentityCertificateCount(
		pSelection->Identity
	);
	if ( (iCertificateCount == 0) ||
		(iCertificateCount > (SIZE_MAX / sizeof(*pEntries))) ) {
		goto cleanup;
	}
	pEntries = (xtlscertificateentry*)xrtTempAlloc(
		&pState->HandshakeArena,
		iCertificateCount * sizeof(*pEntries)
	);
	if ( pEntries == NULL ) {
		(void)__xrtTlsServerCause(
			"build-tls12-server-flight",
			"TLS 1.2 Certificate entry allocation failed"
		);
		goto cleanup;
	}
	memset(pEntries, 0, iCertificateCount * sizeof(*pEntries));
	for ( size_t i = 0; i < iCertificateCount; i++ ) {
		if ( !xrtTlsIdentityCertificate(
			pSelection->Identity, i, &pEntries[i].Data
		) ) {
			goto cleanup;
		}
	}
	iCertificateBody = xrtTlsCertificateSize(
		XTLS_VERSION_12, Empty, pEntries, iCertificateCount
	);
	iCertificate = xrtTlsHandshakeSize(iCertificateBody);

	/* ServerKeyExchange 签名绑定双方随机数与原始 ECDHE 参数。 */
	iParameters = 4u + pGroup->PublicSize;
	Parameters[0] = 3u;
	__xrtTlsWrite16(Parameters + 1u, pSelection->Group);
	Parameters[3] = (uint8)pGroup->PublicSize;
	memcpy(Parameters + 4u, PublicKey, pGroup->PublicSize);
	iSignContent = (2u * XTLS12_RANDOM_SIZE) + iParameters;
	memcpy(
		SignContent, pSelection->Hello.Random.Data, XTLS12_RANDOM_SIZE
	);
	memcpy(
		SignContent + XTLS12_RANDOM_SIZE,
		ServerRandom, XTLS12_RANDOM_SIZE
	);
	memcpy(
		SignContent + (2u * XTLS12_RANDOM_SIZE),
		Parameters, iParameters
	);
	if ( !xrtTlsIdentitySign(
		pSelection->Identity, XTLS_VERSION_12,
		pSelection->Signature,
		(xbytesview) { SignContent, iSignContent },
		NULL, 0, &iSignature
	) ) {
		goto cleanup;
	}
	pSignature = (bytes)xrtTempAlloc(
		&pState->HandshakeArena,
		iSignature
	);
	if ( pSignature == NULL ) {
		(void)__xrtTlsServerCause(
			"build-tls12-server-flight",
			"TLS 1.2 ServerKeyExchange signature allocation failed"
		);
		goto cleanup;
	}
	if ( !xrtTlsIdentitySign(
		pSelection->Identity, XTLS_VERSION_12,
		pSelection->Signature,
		(xbytesview) { SignContent, iSignContent },
		pSignature, iSignature, &iSignature
	) ) {
		goto cleanup;
	}
	Verify.Scheme = (uint16)pSelection->Signature;
	Verify.Signature = (xbytesview) { pSignature, iSignature };
	iKeyExchangeBody = xrtTls12ServerKeyExchangeSize(
		pSelection->Group,
		(xbytesview) { PublicKey, pGroup->PublicSize }, &Verify
	);
	iKeyExchange = xrtTlsHandshakeSize(iKeyExchangeBody);
	iHelloDone = xrtTlsHandshakeSize(0);
	if ( (iServerHelloBody == 0) || (iServerHello == 0) ||
		(iCertificateBody == 0) || (iCertificate == 0) ||
		(iKeyExchangeBody == 0) || (iKeyExchange == 0) ||
		(iHelloDone == 0) ||
		!__xrtTlsServer12HandshakeLimit(
			pSession, iServerHello,
			"TLS 1.2 ServerHello exceeds the handshake limit"
		) || !__xrtTlsServer12HandshakeLimit(
			pSession, iCertificate,
			"TLS 1.2 Certificate exceeds the handshake limit"
		) || !__xrtTlsServer12HandshakeLimit(
			pSession, iKeyExchange,
			"TLS 1.2 ServerKeyExchange exceeds the handshake limit"
		) || !__xrtTlsServer12Size(&iStream, iServerHello) ||
		!__xrtTlsServer12Size(&iStream, iCertificate) ||
		!__xrtTlsServer12Size(&iStream, iKeyExchange) ||
		!__xrtTlsServer12Size(&iStream, iHelloDone) ) {
		goto cleanup;
	}
	pStream = (bytes)xrtTempAlloc(&pState->HandshakeArena, iStream);
	if ( pStream == NULL ) {
		(void)__xrtTlsServerCause(
			"build-tls12-server-flight",
			"TLS 1.2 server handshake stream allocation failed"
		);
		goto cleanup;
	}
	if ( !xrtTlsServerHelloEncode(
		&Hello, pStream + iOffset + XTLS_HANDSHAKE_HEADER_SIZE,
		iServerHelloBody
	) || !__xrtTlsServer12HandshakeEncode(
		XTLS_HANDSHAKE_SERVER_HELLO, pStream + iOffset, iServerHello
	) ) {
		goto cleanup;
	}
	iOffset += iServerHello;
	if ( !xrtTlsCertificateEncode(
		XTLS_VERSION_12, Empty, pEntries, iCertificateCount,
		pStream + iOffset + XTLS_HANDSHAKE_HEADER_SIZE,
		iCertificateBody
	) || !__xrtTlsServer12HandshakeEncode(
		XTLS_HANDSHAKE_CERTIFICATE,
		pStream + iOffset, iCertificate
	) ) {
		goto cleanup;
	}
	iOffset += iCertificate;
	if ( !xrtTls12ServerKeyExchangeEncode(
		pSelection->Group,
		(xbytesview) { PublicKey, pGroup->PublicSize }, &Verify,
		pStream + iOffset + XTLS_HANDSHAKE_HEADER_SIZE,
		iKeyExchangeBody
	) || !__xrtTlsServer12HandshakeEncode(
		XTLS_HANDSHAKE_SERVER_KEY_EXCHANGE,
		pStream + iOffset, iKeyExchange
	) ) {
		goto cleanup;
	}
	iOffset += iKeyExchange;
	if ( !xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_SERVER_HELLO_DONE, Empty,
		pStream + iOffset, iHelloDone
	) ) {
		goto cleanup;
	}
	iOffset += iHelloDone;
	if ( iOffset != iStream ) {
		goto cleanup;
	}

	if ( !__xrtTlsTranscriptInit(
		&Transcript, __xrtTlsHash(pCipher->Hash)
	) || !__xrtTlsTranscriptUpdate(
		&Transcript, __xrtTlsServer12Encoded(pMessage)
	) || !__xrtTlsTranscriptUpdate(
		&Transcript, (xbytesview) { pStream, iStream }
	) || !__xrtTlsServer12Records(
		(xbytesview) { pStream, iStream }, &pOutput, &iOutput
	) ) {
		goto cleanup;
	}
	if ( pSelection->ServerName.Size != 0 ) {
		pServerName = __xrtTlsServer12NameCopy(pSelection->ServerName);
		if ( pServerName == NULL ) {
			goto cleanup;
		}
	}
	{
		const xtlslimits* pLimits = xrtTlsContextLimits(pSession->Context);
		size_t iQueued = xrtTlsSessionSendSize(pSession);

		if ( (pLimits == NULL) || (iOutput > pLimits->SendLimit) ||
			(iQueued > pLimits->SendLimit) ||
			(iOutput > (pLimits->SendLimit - iQueued)) ) {
			(void)__xrtTlsServerError(
				XERR_RANGE, XTLS_ERROR_LIMIT,
				"build-tls12-server-flight",
				"TLS send limit cannot hold the mandatory TLS 1.2 server flight"
			);
			goto cleanup;
		}
	}
	Result = __xrtTlsSessionSendTake(pSession, pOutput, iOutput);
	if ( Result != XTLS_OK ) {
		goto cleanup;
	}
	bOutputMoved = true;

	/* 航班已接管后只提交不会分配的稳定状态。 */
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Transcript;
	memset(&Transcript, 0, sizeof(Transcript));
	memcpy(pState->ClientRandom, pSelection->Hello.Random.Data, XTLS12_RANDOM_SIZE);
	memcpy(pState->ServerRandom, ServerRandom, XTLS12_RANDOM_SIZE);
	memcpy(pState->PrivateKey, PrivateKey, pGroup->PrivateSize);
	pState->PrivateKeySize = pGroup->PrivateSize;
	pState->Version = XTLS_VERSION_12;
	pState->Cipher = pSelection->Cipher;
	pState->Signature = pSelection->Signature;
	pState->Group = pSelection->Group;
	pState->HashSize = pCipher->HashSize;
	pState->Step = XTLS_SERVER_WAIT_CLIENT_KEY_EXCHANGE;
	pSession->Version = XTLS_VERSION_12;
	pSession->Cipher = pSelection->Cipher;
	if ( pSelection->Protocol != XTLS_SERVER_PROTOCOL_NONE ) {
		pSession->Protocol = pState->Protocols[pSelection->Protocol];
	}
	xrtTlsIdentityRelease((xtlsidentity*)pState->Identity);
	pState->Identity = pSelection->Identity;
	pSelection->Identity = NULL;
	pState->ServerNameStorage = pServerName;
	pState->ServerName.Data = pServerName;
	pState->ServerName.Size = pSelection->ServerName.Size;
	pServerName = NULL;
	pSession->Wait = XTLS_WAIT_INPUT | XTLS_WAIT_OUTPUT;
	Result = XTLS_OK;

cleanup:
	if ( !bOutputMoved && (pOutput != NULL) ) {
		xrtSecureZero(pOutput, iOutput);
		xrtFree(pOutput);
	}
	if ( pServerName != NULL ) {
		xrtSecureZero(pServerName, pSelection->ServerName.Size);
		xrtFree(pServerName);
	}
	__xrtTlsTranscriptClear(&Transcript);
	xrtSecureZero(ServerRandom, sizeof(ServerRandom));
	xrtSecureZero(SignContent, sizeof(SignContent));
	xrtSecureZero(Parameters, sizeof(Parameters));
	xrtSecureZero(PublicKey, sizeof(PublicKey));
	xrtSecureZero(PrivateKey, sizeof(PrivateKey));
	return Result;
}



/* 从 ClientKeyExchange 派生双方 TLS 1.2 记录 epoch。 */
static xtlsresult __xrtTlsServer12ClientKeyExchange(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage
)
{
	const xtlsgroupinfo* pGroup = xrtTlsGroupInfo(pState->Group);
	xtls12keymaterial Material;
	xtlsrecordkey ReadKey;
	xtlsrecordkey WriteKey;
	xtlstranscript Next = pState->Transcript;
	xbytesview PublicKey;
	uint8 Shared[56u];
	uint8 SessionHash[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	bool bResult = false;

	memset(&Material, 0, sizeof(Material));
	memset(&ReadKey, 0, sizeof(ReadKey));
	memset(&WriteKey, 0, sizeof(WriteKey));
	memset(Shared, 0, sizeof(Shared));
	memset(SessionHash, 0, sizeof(SessionHash));
	if ( (pMessage->Type != XTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE) ||
		(pGroup == NULL) ||
		(pState->PrivateKeySize != pGroup->PrivateSize) ||
		!xrtTls12ClientKeyExchangeParse(pMessage->Body, &PublicKey) ||
		!xrtTlsKeyShareDerive(
			pState->Group,
			(xbytesview) { pState->PrivateKey, pState->PrivateKeySize },
			PublicKey, Shared, sizeof(Shared)
		) || !__xrtTlsTranscriptUpdate(
			&Next, __xrtTlsServer12Encoded(pMessage)
		) || !__xrtTlsTranscriptDigest(
			&Next, SessionHash, pState->HashSize
		) || !__xrtTls12KeyMaterial(
			pState->Cipher,
			(xbytesview) { Shared, pGroup->SharedSize },
			(xbytesview) { SessionHash, pState->HashSize },
			(xbytesview) { pState->ClientRandom, XTLS12_RANDOM_SIZE },
			(xbytesview) { pState->ServerRandom, XTLS12_RANDOM_SIZE },
			&Material
		) || !__xrtTlsRecordKeyInit(
			&ReadKey, XTLS_VERSION_12, pState->Cipher,
			(xbytesview) { Material.ClientKey, Material.KeySize },
			(xbytesview) { Material.ClientIv, Material.IvSize }
		) || !__xrtTlsRecordKeyInit(
			&WriteKey, XTLS_VERSION_12, pState->Cipher,
			(xbytesview) { Material.ServerKey, Material.KeySize },
			(xbytesview) { Material.ServerIv, Material.IvSize }
		) ) {
		goto cleanup;
	}

	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	memset(&Next, 0, sizeof(Next));
	__xrtTlsRecordKeyClear(&pState->PendingReadKey);
	pState->PendingReadKey = ReadKey;
	memset(&ReadKey, 0, sizeof(ReadKey));
	__xrtTlsRecordKeyClear(&pState->PendingWriteKey);
	pState->PendingWriteKey = WriteKey;
	memset(&WriteKey, 0, sizeof(WriteKey));
	memcpy(pState->Master, Material.Master, sizeof(pState->Master));
	xrtSecureZero(pState->PrivateKey, sizeof(pState->PrivateKey));
	pState->PrivateKeySize = 0;
	pState->Step = XTLS_SERVER_WAIT_CHANGE_CIPHER_SPEC;
	bResult = __xrtTlsServerWait(pSession, true);

cleanup:
	__xrtTlsRecordKeyClear(&WriteKey);
	__xrtTlsRecordKeyClear(&ReadKey);
	__xrtTlsTranscriptClear(&Next);
	xrtSecureZero(SessionHash, sizeof(SessionHash));
	xrtSecureZero(Shared, sizeof(Shared));
	xrtSecureZero(&Material, sizeof(Material));
	return bResult ? XTLS_OK : __xrtTlsServer12Failed(pSession);
}



/* 返回服务端 CCS 与受保护 Finished 的完整线路长度。 */
static bool __xrtTlsServer12FinishedSize(
	const xtlsserverstate* pState,
	size_t* pSize
)
{
	size_t iFinished;

	*pSize = 0;
	if ( !pState->PendingWriteKey.Ready ) {
		return __xrtTlsServerError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL,
			"size-tls12-server-finished",
			"TLS 1.2 pending server write epoch is unavailable"
		);
	}
	iFinished = __xrtTlsRecordSealSize(
		&pState->PendingWriteKey,
		XTLS_SERVER12_FINISHED_MESSAGE_SIZE, 0
	);
	if ( iFinished == 0 ) {
		return false;
	}
	*pSize = xrtTlsRecordSize(1u) + iFinished;
	return true;
}



/* 在消费客户端 Finished 前检查完整服务端响应能否原子排队。 */
xtlsresult __xrtTlsServer12FinishedWritable(
	xtlssession* pSession,
	const xtlsserverstate* pState
)
{
	const xtlslimits* pLimits = xrtTlsContextLimits(pSession->Context);
	size_t iOutput;
	size_t iQueued = xrtTlsSessionSendSize(pSession);

	if ( (pLimits == NULL) ||
		!__xrtTlsServer12FinishedSize(pState, &iOutput) ) {
		return XTLS_ERROR;
	}
	if ( iOutput > pLimits->SendLimit ) {
		(void)__xrtTlsServerError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			"queue-tls12-server-finished",
			"TLS send limit cannot hold the mandatory TLS 1.2 server Finished"
		);
		return XTLS_ERROR;
	}
	if ( (iQueued > pLimits->SendLimit) ||
		(iOutput > (pLimits->SendLimit - iQueued)) ) {
		return XTLS_AGAIN;
	}
	return XTLS_OK;
}



/* 验证客户端 Finished，并原子排队服务端 CCS 与 Finished。 */
static xtlsresult __xrtTlsServer12Finished(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage
)
{
	xtlstranscript Next = pState->Transcript;
	xtlsrecordkey WriteKey = pState->PendingWriteKey;
	xbytesview VerifyData;
	uint8 HandshakeHash[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 Expected[XTLS12_FINISHED_SIZE];
	uint8 ServerVerify[XTLS12_FINISHED_SIZE];
	uint8 Finished[XTLS_SERVER12_FINISHED_MESSAGE_SIZE];
	uint8 ChangeCipherSpec = 1u;
	bytes pOutput = NULL;
	size_t iChangeCipherSpec;
	size_t iFinished;
	size_t iOutput;
	size_t iWritten;
	uint32 iWait = XTLS_WAIT_INPUT;
	xtlsresult Result = XTLS_ERROR;
	bool bOutputMoved = false;

	memset(HandshakeHash, 0, sizeof(HandshakeHash));
	memset(Expected, 0, sizeof(Expected));
	memset(ServerVerify, 0, sizeof(ServerVerify));
	memset(Finished, 0, sizeof(Finished));
	if ( (pMessage->Type != XTLS_HANDSHAKE_FINISHED) ||
		!xrtTlsFinishedParse(
			pMessage->Body, XTLS12_FINISHED_SIZE, &VerifyData
		) || !__xrtTlsTranscriptDigest(
			&pState->Transcript, HandshakeHash, pState->HashSize
		) || !__xrtTls12Finished(
			pState->Transcript.Hash,
			(xbytesview) { pState->Master, sizeof(pState->Master) },
			false,
			(xbytesview) { HandshakeHash, pState->HashSize },
			Expected, sizeof(Expected)
		) || !xrtConstTimeEqual(
			Expected, VerifyData.Data, sizeof(Expected)
		) ) {
		(void)__xrtTlsServerError(
			XERR_PROTOCOL, XTLS_ERROR_VERIFY,
			"process-tls12-client-finished",
			"TLS 1.2 client Finished verification failed"
		);
		goto cleanup;
	}
	if ( !__xrtTlsTranscriptUpdate(
		&Next, __xrtTlsServer12Encoded(pMessage)
	) || !__xrtTlsTranscriptDigest(
		&Next, HandshakeHash, pState->HashSize
	) || !__xrtTls12Finished(
		pState->Transcript.Hash,
		(xbytesview) { pState->Master, sizeof(pState->Master) },
		true,
		(xbytesview) { HandshakeHash, pState->HashSize },
		ServerVerify, sizeof(ServerVerify)
	) || !xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_FINISHED,
		(xbytesview) { ServerVerify, sizeof(ServerVerify) },
		Finished, sizeof(Finished)
	) || !__xrtTlsTranscriptUpdate(
		&Next, (xbytesview) { Finished, sizeof(Finished) }
	) ) {
		goto cleanup;
	}
	iChangeCipherSpec = xrtTlsRecordSize(1u);
	iFinished = __xrtTlsRecordSealSize(
		&WriteKey, sizeof(Finished), 0
	);
	iOutput = iChangeCipherSpec + iFinished;
	pOutput = (bytes)xrtMalloc(iOutput);
	if ( (pOutput == NULL) || !xrtTlsRecordEncode(
		XTLS_RECORD_CHANGE_CIPHER_SPEC, XTLS_VERSION_12,
		(xbytesview) { &ChangeCipherSpec, 1u },
		pOutput, iChangeCipherSpec
	) || !__xrtTlsRecordSeal(
		&WriteKey, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Finished, sizeof(Finished) }, 0,
		pOutput + iChangeCipherSpec, iFinished, &iWritten
	) || (iWritten != iFinished) ) {
		goto cleanup;
	}
	Result = __xrtTlsSessionSendTake(pSession, pOutput, iOutput);
	if ( Result != XTLS_OK ) {
		goto cleanup;
	}
	bOutputMoved = true;

	__xrtTlsRecordKeyClear(&pSession->WriteKey);
	pSession->WriteKey = WriteKey;
	memset(&WriteKey, 0, sizeof(WriteKey));
	memset(&pState->PendingWriteKey, 0, sizeof(pState->PendingWriteKey));
	__xrtTlsTranscriptClear(&pState->Transcript);
	pState->Transcript = Next;
	memset(&Next, 0, sizeof(Next));
	xrtSecureZero(pState->Master, sizeof(pState->Master));
	pState->Step = XTLS_SERVER_READY;
	if ( xrtTlsSessionSendSize(pSession) != 0 ) {
		iWait |= XTLS_WAIT_OUTPUT;
	}
	if ( !__xrtTlsSessionSetState(
		pSession, XTLS_STATE_READY
	) || !__xrtTlsSessionSetWait(pSession, iWait) ) {
		Result = XTLS_ERROR;
	} else {
		Result = XTLS_OK;
	}

cleanup:
	if ( !bOutputMoved ) {
		xrtFree(pOutput);
	}
	__xrtTlsRecordKeyClear(&WriteKey);
	__xrtTlsTranscriptClear(&Next);
	xrtSecureZero(Finished, sizeof(Finished));
	xrtSecureZero(ServerVerify, sizeof(ServerVerify));
	xrtSecureZero(Expected, sizeof(Expected));
	xrtSecureZero(HandshakeHash, sizeof(HandshakeHash));
	return Result == XTLS_OK ? XTLS_OK : __xrtTlsServer12Failed(pSession);
}



/* 按服务端 TLS 1.2 当前阶段分派客户端密钥交换和 Finished。 */
xtlsresult __xrtTlsServer12Handshake(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage
)
{
	if ( pState->Step == XTLS_SERVER_WAIT_CLIENT_KEY_EXCHANGE ) {
		return __xrtTlsServer12ClientKeyExchange(
			pSession, pState, pMessage
		);
	}
	if ( pState->Step == XTLS_SERVER_WAIT_CLIENT_FINISHED ) {
		return __xrtTlsServer12Finished(pSession, pState, pMessage);
	}
	return __xrtTlsServerProtocol(
		pSession, XTLS_ERROR_INTERNAL, "drive-tls12-server",
		"TLS 1.2 server handshake step is invalid"
	);
}



/* 验证客户端 CCS 并启用客户端写入方向对应的接收 epoch。 */
xtlsresult __xrtTlsServer12ChangeCipherSpec(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlssessionrecord* pRecord
)
{
	if ( (pState->Step != XTLS_SERVER_WAIT_CHANGE_CIPHER_SPEC) ||
		pRecord->Protected || (pRecord->Data.Size != 1u) ||
		(pRecord->Data.Data[0] != 1u) ||
		!pState->PendingReadKey.Ready ) {
		return __xrtTlsServerProtocol(
			pSession, XTLS_ERROR_HANDSHAKE,
			"process-tls12-change-cipher-spec",
			"TLS 1.2 ChangeCipherSpec is malformed or out of order"
		);
	}
	if ( __xrtTlsSessionRecordFinish(pSession, false) != XTLS_OK ) {
		return __xrtTlsServer12Failed(pSession);
	}
	__xrtTlsRecordKeyClear(&pSession->ReadKey);
	pSession->ReadKey = pState->PendingReadKey;
	memset(&pState->PendingReadKey, 0, sizeof(pState->PendingReadKey));
	pState->Step = XTLS_SERVER_WAIT_CLIENT_FINISHED;
	if ( !__xrtTlsServerWait(pSession, true) ) {
		return __xrtTlsServer12Failed(pSession);
	}
	return XTLS_OK;
}

#endif
