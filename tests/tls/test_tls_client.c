#include "../test.h"



/* 创建只启用一条可预测 TLS 1.3 路径的共享上下文。 */
static xtlscontext* testTlsClientContext(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_13 };
	static const xtlscipher Ciphers[] = {
		XTLS_AES_128_GCM_SHA256
	};
	static const uint16 Groups[] = { XTLS_GROUP_X25519 };
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_ED25519,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	};
	xtlspolicy Policy;
	xtlscontextconfig Config;

	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions;
	Policy.VersionCount = sizeof(Versions) / sizeof(Versions[0]);
	Policy.Ciphers = Ciphers;
	Policy.CipherCount = sizeof(Ciphers) / sizeof(Ciphers[0]);
	Policy.Groups = Groups;
	Policy.GroupCount = sizeof(Groups) / sizeof(Groups[0]);
	Policy.Signatures = Signatures;
	Policy.SignatureCount = sizeof(Signatures) / sizeof(Signatures[0]);
	xrtTlsContextConfigInit(&Config);
	Config.Policy = &Policy;
	return xrtTlsContextCreate(&Config);
}



/* 解析客户端创建时排队的唯一明文 ClientHello。 */
static bool testTlsClientHello(
	const xtlssession* pSession,
	xtlsclienthello* pHello,
	xbytesview* pEncoded
)
{
	xnetspan Span;
	xtlsrecord Record;
	xtlshandshake Handshake;

	if ( !xrtTlsSessionSendFront(pSession, &Span) ||
		(xrtTlsRecordParse(
			(xbytesview) { Span.Data, Span.Size }, &Record, NULL
		) != XTLS_OK) ) {
		return false;
	}
	if ( (Record.Type != XTLS_RECORD_HANDSHAKE) ||
		(Record.LegacyVersion != XTLS_VERSION_12) ||
		(Record.EncodedSize != Span.Size) ||
		(xrtTlsHandshakeParse(
			Record.Payload, &Handshake, NULL
		) != XTLS_OK) ||
		(Handshake.Type != XTLS_HANDSHAKE_CLIENT_HELLO) ||
		(Handshake.EncodedSize != Record.Payload.Size) ||
		!xrtTlsClientHelloParse(Handshake.Body, pHello) ) {
		return false;
	}
	if ( pEncoded != NULL ) {
		*pEncoded = Record.Payload;
	}
	return true;
}



/* 逐项核对 ALPN，避免把多次游标更新折叠进同一条逻辑表达式。 */
static bool testTlsClientProtocols(xbytesview Data)
{
	static const cstr sExpected[] = { "http/1.1", "xrt-test" };
	xtlsprotocolcursor Cursor;
	xbytesview Protocol;

	if ( !xrtTlsProtocols(Data, &Cursor) ) {
		return false;
	}
	for ( size_t i = 0; i < (sizeof(sExpected) / sizeof(sExpected[0])); i++ ) {
		xtlsitemresult Result = xrtTlsProtocolsRead(&Cursor, &Protocol);

		if ( (Result != XTLS_ITEM_VALUE) ||
			(Protocol.Size != 8u) ||
			(memcmp(Protocol.Data, sExpected[i], 8u) != 0) ) {
			return false;
		}
	}
	if ( xrtTlsProtocolsRead(&Cursor, &Protocol) != XTLS_ITEM_DONE ) {
		return false;
	}
	return true;
}



/* 用公开 HKDF 后端独立编码 TLS 1.3 HkdfLabel。 */
static bool testTlsClientExpandLabel(
	const uint8* pSecret,
	xstrview Label,
	xbytesview Context,
	uint8* pOutput
)
{
	static const char sPrefix[] = "tls13 ";
	uint8 Info[96];
	size_t iLabel = sizeof(sPrefix) - 1u + Label.Size;
	size_t iSize = 2u + 1u + iLabel + 1u + Context.Size;
	size_t iOffset = 0;

	if ( (iLabel > UINT8_MAX) || (Context.Size > UINT8_MAX) ||
		(iSize > sizeof(Info)) ) {
		return false;
	}
	Info[iOffset++] = 0;
	Info[iOffset++] = XRT_SHA256_SIZE;
	Info[iOffset++] = (uint8)iLabel;
	memcpy(Info + iOffset, sPrefix, sizeof(sPrefix) - 1u);
	iOffset += sizeof(sPrefix) - 1u;
	memcpy(Info + iOffset, Label.Data, Label.Size);
	iOffset += Label.Size;
	Info[iOffset++] = (uint8)Context.Size;
	if ( Context.Size != 0 ) {
		memcpy(Info + iOffset, Context.Data, Context.Size);
	}
	return xrtHkdfSha256Expand(
		pSecret, XRT_SHA256_SIZE, Info, iSize,
		pOutput, XRT_SHA256_SIZE
	);
}



/* 独立重算外部 PSK 的 res binder，不复用 TLS 调度内部函数。 */
static bool testTlsClientBinder(
	xbytesview Encoded,
	const xtlspsk* pPsk,
	const uint8* pSecret
)
{
	uint8 Early[XRT_SHA256_SIZE];
	uint8 EmptyHash[XRT_SHA256_SIZE];
	uint8 BinderKey[XRT_SHA256_SIZE];
	uint8 FinishedKey[XRT_SHA256_SIZE];
	uint8 TranscriptHash[XRT_SHA256_SIZE];
	uint8 Expected[XRT_SHA256_SIZE];
	size_t iPartial;
	bool bResult;

	if ( (pPsk->Binder.Size != XRT_SHA256_SIZE) ||
		(pPsk->Binder.Data < Encoded.Data + 3u) ||
		(pPsk->Binder.Data + pPsk->Binder.Size !=
		 Encoded.Data + Encoded.Size) ) {
		return false;
	}
	iPartial = (size_t)(pPsk->Binder.Data - Encoded.Data) - 3u;
	bResult = xrtSha256(NULL, 0, EmptyHash) &&
		xrtHkdfSha256Extract(
			NULL, 0, pSecret, XRT_SHA256_SIZE, Early
		) && testTlsClientExpandLabel(
			Early, XRT_STR_LITERAL("res binder"),
			(xbytesview) { EmptyHash, sizeof(EmptyHash) }, BinderKey
		) && testTlsClientExpandLabel(
			BinderKey, XRT_STR_LITERAL("finished"),
			(xbytesview) { NULL, 0 }, FinishedKey
		) && xrtSha256(Encoded.Data, iPartial, TranscriptHash) &&
		xrtHmacSha256(
			FinishedKey, sizeof(FinishedKey),
			TranscriptHash, sizeof(TranscriptHash), Expected
		) && xrtConstTimeEqual(
			Expected, pPsk->Binder.Data, sizeof(Expected)
		);
	xrtSecureZero(Expected, sizeof(Expected));
	xrtSecureZero(TranscriptHash, sizeof(TranscriptHash));
	xrtSecureZero(FinishedKey, sizeof(FinishedKey));
	xrtSecureZero(BinderKey, sizeof(BinderKey));
	xrtSecureZero(EmptyHash, sizeof(EmptyHash));
	xrtSecureZero(Early, sizeof(Early));
	return bResult;
}



/* 创建绑定 example.com、h2 和固定 SHA-256 PSK 的恢复对象。 */
static xtlsresume* testTlsClientResume(uint8* pSecret)
{
	xtlsresumeconfig Config;

	for ( size_t i = 0; i < XRT_SHA256_SIZE; i++ ) {
		pSecret[i] = (uint8)(0x60u + i);
	}
	xrtTlsResumeConfigInit(&Config);
	Config.Cipher = XTLS_AES_128_GCM_SHA256;
	Config.Ticket = XRT_BYTES_LITERAL("client-resume-ticket");
	Config.Secret = (xbytesview) { pSecret, XRT_SHA256_SIZE };
	Config.ServerName = XRT_STR_LITERAL("example.com");
	Config.Protocol = XRT_BYTES_LITERAL("h2");
	Config.PeerIdentity = XRT_BYTES_LITERAL("authenticated-peer");
	Config.Lifetime = 60u;
	Config.AgeAdd = UINT32_C(0x10203040);
	return xrtTlsResumeCreate(&Config);
}



/* 恢复首航必须自动继承路由绑定并生成可独立验证的末尾 PSK。 */
static void testTlsClientResumeStart(void)
{
	uint8 Secret[XRT_SHA256_SIZE];
	xtlsresume* pResume = testTlsClientResume(Secret);
	xtlscontext* pContext = testTlsClientContext();
	xtlsclientconfig Config;
	xtlssession* pSession;
	xtlsclienthello Hello;
	xtlsextensioncursor Extensions;
	xtlsextension Extension;
	xtlspskcursor Psks;
	xtlspsk Psk;
	xbytesview Encoded;
	xbytesview Name;
	xbytesview Modes;
	xbytesview Protocol;
	xtlsprotocolcursor Protocols;
	xtlsitemresult Result;
	bool bPsk = false;

	testRequire((pResume != NULL) && (pContext != NULL),
		"TLS client resume fixture failed");
	xrtTlsClientConfigInit(&Config);
	Config.Context = pContext;
	Config.Resume = pResume;
	pSession = xrtTlsClientCreate(&Config, NULL);
	testRequire(pSession != NULL,
		"TLS client resume creation failed");
	testRequire(!xrtTlsClientResumed(pSession) &&
		testTlsClientHello(pSession, &Hello, &Encoded),
		"TLS client reported resume before ServerHello or wrote invalid hello");
	testRequire((xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_SERVER_NAME, &Extension
	) == XTLS_ITEM_VALUE) &&
		(xrtTlsHostName(Extension.Data, &Name) == XTLS_ITEM_VALUE) &&
		(Name.Size == sizeof("example.com") - 1u) &&
		(memcmp(Name.Data, "example.com", Name.Size) == 0),
		"TLS client did not inherit resume SNI");
	testRequire((xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_ALPN, &Extension
	) == XTLS_ITEM_VALUE) && xrtTlsProtocols(
		Extension.Data, &Protocols
	) && (xrtTlsProtocolsRead(
		&Protocols, &Protocol
	) == XTLS_ITEM_VALUE) && (Protocol.Size == 2u) &&
		(memcmp(Protocol.Data, "h2", 2u) == 0) &&
		(xrtTlsProtocolsRead(
			&Protocols, &Protocol
		) == XTLS_ITEM_DONE),
		"TLS client did not inherit resume ALPN");
	testRequire((xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES, &Extension
	) == XTLS_ITEM_VALUE) && xrtTlsPskModes(
		Extension.Data, &Modes
	) && (Modes.Size == 1u) && (Modes.Data[0] == XTLS_PSK_DHE_KE),
		"TLS client did not restrict resume to psk_dhe_ke");
	testRequire(xrtTlsExtensionsInit(&Extensions, Hello.Extensions),
		"TLS client resume extension cursor failed");
	while ( (Result = xrtTlsExtensionsRead(
		&Extensions, &Extension
	)) == XTLS_ITEM_VALUE ) {
		if ( Extension.Type == XTLS_EXTENSION_PRE_SHARED_KEY ) {
			bPsk = (Extensions.Offset == Extensions.Data.Size) &&
				xrtTlsClientPsks(Extension.Data, &Psks) &&
				(xrtTlsPsksRead(&Psks, &Psk) == XTLS_ITEM_VALUE) &&
				(xrtTlsPsksRead(&Psks, &Psk) == XTLS_ITEM_DONE);
		}
	}
	testRequire((Result == XTLS_ITEM_DONE) && bPsk &&
		(Psk.Identity.Size == sizeof("client-resume-ticket") - 1u) &&
		(memcmp(
			Psk.Identity.Data, "client-resume-ticket", Psk.Identity.Size
		) == 0) &&
		((uint32)(Psk.ObfuscatedAge - UINT32_C(0x10203040)) < 1000u) &&
		testTlsClientBinder(Encoded, &Psk, Secret),
		"TLS client resume PSK or binder mismatch");
	xrtTlsSessionDestroy(pSession);
	xrtTlsContextRelease(pContext);
	xrtTlsResumeRelease(pResume);
	xrtSecureZero(Secret, sizeof(Secret));
}



/* ClientHello 必须完整表达配置和当前构建的真实能力。 */
static void testTlsClientStart(void)
{
	static const xstrview Protocols[] = {
		{ "http/1.1", sizeof("http/1.1") - 1u },
		{ "xrt-test", sizeof("xrt-test") - 1u }
	};
	xtlscontext* pContext = testTlsClientContext();
	xtlsclientconfig Config;
	xtlssession* pSession;
	xtlsclienthello Hello;
	xtlsextension Extension;
	xtlsids Ids;
	xtlskeysharecursor ShareCursor;
	xtlskeyshare Share;
	xbytesview Name;
	uint16 iCipher = 0;

	testRequire(pContext != NULL, "TLS client context failed");
	xrtTlsClientConfigInit(&Config);
	Config.Context = pContext;
	Config.ServerName = XRT_STR_LITERAL("example.com");
	Config.Protocols = Protocols;
	Config.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	pSession = xrtTlsClientCreate(&Config, NULL);
	xrtTlsContextRelease(pContext);
	testRequire(pSession != NULL, "TLS client creation failed");
	testRequire((xrtTlsSessionRole(pSession) == XTLS_CLIENT) &&
		(xrtTlsSessionState(pSession) == XTLS_STATE_HANDSHAKE) &&
		(xrtTlsSessionWait(pSession) ==
		 (XTLS_WAIT_INPUT | XTLS_WAIT_OUTPUT)) &&
		(xrtTlsSessionSendSize(pSession) != 0),
		"TLS client initial state mismatch");
	testRequire(testTlsClientHello(pSession, &Hello, NULL),
		"TLS client queued an invalid ClientHello");
	testRequire((Hello.Random.Size == XTLS_RANDOM_SIZE) &&
		(Hello.SessionId.Size == XTLS_SESSION_ID_MAX) &&
		(Hello.CipherSuites.Data.Size == 2u) &&
		xrtTlsIdsGet(&Hello.CipherSuites, 0, &iCipher) &&
		(iCipher == XTLS_AES_128_GCM_SHA256),
		"TLS client core ClientHello fields mismatch");

	testRequire((xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_SERVER_NAME, &Extension
	) == XTLS_ITEM_VALUE) && (xrtTlsHostName(
		Extension.Data, &Name
	) == XTLS_ITEM_VALUE) && (Name.Size == 11u) &&
		(memcmp(Name.Data, "example.com", 11u) == 0),
		"TLS client SNI extension mismatch");
	testRequire((xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_SUPPORTED_VERSIONS, &Extension
	) == XTLS_ITEM_VALUE) && xrtTlsClientVersions(
		Extension.Data, &Ids
	) && (xrtTlsIdsCount(&Ids) == 1u) &&
		xrtTlsIdsContain(&Ids, XTLS_VERSION_13),
		"TLS client supported_versions mismatch");
	testRequire((xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_SUPPORTED_GROUPS, &Extension
	) == XTLS_ITEM_VALUE) && xrtTlsGroups(Extension.Data, &Ids) &&
		(xrtTlsIdsCount(&Ids) == 1u) &&
		xrtTlsIdsContain(&Ids, XTLS_GROUP_X25519),
		"TLS client supported_groups mismatch");
	testRequire((xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_SIGNATURE_ALGORITHMS, &Extension
	) == XTLS_ITEM_VALUE) && xrtTlsSignatures(Extension.Data, &Ids) &&
		(xrtTlsIdsCount(&Ids) == 2u) &&
		xrtTlsIdsContain(&Ids, XTLS_SIGNATURE_ED25519) &&
		xrtTlsIdsContain(&Ids, XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256),
		"TLS client signature_algorithms mismatch");

	testRequire((xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_ALPN, &Extension
	) == XTLS_ITEM_VALUE) && testTlsClientProtocols(Extension.Data),
		"TLS client ALPN extension mismatch");
	testRequire((xrtTlsExtensionsFind(
		Hello.Extensions, XTLS_EXTENSION_KEY_SHARE, &Extension
	) == XTLS_ITEM_VALUE) && xrtTlsClientKeyShares(
		Extension.Data, &ShareCursor
	) && (xrtTlsKeySharesRead(
		&ShareCursor, &Share
	) == XTLS_ITEM_VALUE) && (Share.Group == XTLS_GROUP_X25519) &&
		(Share.Key.Size == 32u) && (xrtTlsKeySharesRead(
			&ShareCursor, &Share
		) == XTLS_ITEM_DONE),
		"TLS client key_share extension mismatch");
	xrtTlsSessionDestroy(pSession);
}



/* 显式验证身份不能被错误编码成线路 SNI。 */
static void testTlsClientVerifyName(void)
{
	xtlscontext* pContext = testTlsClientContext();
	xtlsclientconfig Config;
	xtlssession* pSession;
	xtlsclienthello Hello;
	xtlsextension Extension;

	testRequire(
		pContext != NULL,
		"TLS verification-name context failed"
	);
	xrtTlsClientConfigInit(&Config);
	Config.Context = pContext;
	Config.VerifyName = XRT_STR_LITERAL("127.0.0.1");
	pSession = xrtTlsClientCreate(&Config, NULL);
	xrtTlsContextRelease(pContext);
	testRequire(
		(pSession != NULL) &&
		testTlsClientHello(pSession, &Hello, NULL),
		"TLS verification-name client creation failed"
	);
	testRequire(
		xrtTlsExtensionsFind(
			Hello.Extensions,
			XTLS_EXTENSION_SERVER_NAME,
			&Extension
		) == XTLS_ITEM_DONE,
		"TLS verification name leaked into the SNI extension"
	);
	xrtTlsSessionDestroy(pSession);
}



/* 配置视图错误必须拒绝，纯 TLS 1.2 策略必须进入已实现客户端路径。 */
static void testTlsClientConfig(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_12 };
	static const xtlscipher Ciphers[] = {
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256
	};
	static const uint16 Groups[] = { XTLS_GROUP_X25519 };
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	};
	xtlsclientconfig Client;
	xtlspolicy Policy;
	xtlscontextconfig ContextConfig;
	xtlscontext* pContext;

	xrtTlsClientConfigInit(&Client);
	Client.ServerName = (xstrview) { NULL, 1u };
	testRequire(xrtTlsClientCreate(&Client, NULL) == NULL,
		"TLS client accepted an invalid server name view");
	xrtTlsClientConfigInit(&Client);
	Client.VerifyName = (xstrview) { NULL, 1u };
	testRequire(xrtTlsClientCreate(&Client, NULL) == NULL,
		"TLS client accepted an invalid verification name view");
	xrtTlsClientConfigInit(&Client);
	Client.ProtocolCount = 1u;
	testRequire(xrtTlsClientCreate(&Client, NULL) == NULL,
		"TLS client accepted a null ALPN array");

	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions;
	Policy.VersionCount = 1u;
	Policy.Ciphers = Ciphers;
	Policy.CipherCount = 1u;
	Policy.Groups = Groups;
	Policy.GroupCount = 1u;
	Policy.Signatures = Signatures;
	Policy.SignatureCount = 1u;
	xrtTlsContextConfigInit(&ContextConfig);
	ContextConfig.Policy = &Policy;
	pContext = xrtTlsContextCreate(&ContextConfig);
	testRequire(pContext != NULL, "TLS 1.2-only client context failed");
	xrtTlsClientConfigInit(&Client);
	Client.Context = pContext;
	{
		xtlssession* pSession = xrtTlsClientCreate(&Client, NULL);

		testRequire(pSession != NULL,
			"TLS client rejected the implemented TLS 1.2 role path");
		xrtTlsSessionDestroy(pSession);
	}
	xrtTlsContextRelease(pContext);
}



/* 过期票据和显式冲突的 SNI/ALPN 不能静默降级为错误目标的连接。 */
static void testTlsClientResumeReject(void)
{
	static const xstrview WrongProtocol[] = {
		{ "http/1.1", sizeof("http/1.1") - 1u }
	};
	uint8 Secret[XRT_SHA256_SIZE];
	xtlsresume* pResume = testTlsClientResume(Secret);
	xtlsresumeconfig ResumeConfig;
	xtlsresume* pExpired;
	xtlsclientconfig Config;

	testRequire(pResume != NULL, "TLS client resume reject fixture failed");
	xrtTlsClientConfigInit(&Config);
	Config.Resume = pResume;
	Config.ServerName = XRT_STR_LITERAL("other.example");
	testRequire(xrtTlsClientCreate(&Config, NULL) == NULL,
		"TLS client accepted a resume object for another server name");
	xrtTlsClientConfigInit(&Config);
	Config.Resume = pResume;
	Config.ServerName = XRT_STR_LITERAL("example.com");
	Config.VerifyName = XRT_STR_LITERAL("other.example");
	testRequire(xrtTlsClientCreate(&Config, NULL) == NULL,
		"TLS client accepted a resume object for another verification name");
	xrtTlsClientConfigInit(&Config);
	Config.Resume = pResume;
	Config.Protocols = WrongProtocol;
	Config.ProtocolCount = 1u;
	testRequire(xrtTlsClientCreate(&Config, NULL) == NULL,
		"TLS client accepted a resume object for another ALPN");

	xrtTlsResumeConfigInit(&ResumeConfig);
	ResumeConfig.Cipher = XTLS_AES_128_GCM_SHA256;
	ResumeConfig.Ticket = XRT_BYTES_LITERAL("expired-ticket");
	ResumeConfig.Secret = (xbytesview) { Secret, sizeof(Secret) };
	ResumeConfig.Lifetime = 1u;
	ResumeConfig.IssuedAt = xrtNow() - INT64_C(2000000);
	pExpired = xrtTlsResumeCreate(&ResumeConfig);
	testRequire(pExpired != NULL, "TLS expired resume fixture failed");
	xrtTlsClientConfigInit(&Config);
	Config.Resume = pExpired;
	testRequire(xrtTlsClientCreate(&Config, NULL) == NULL,
		"TLS client accepted an expired resume object");
	xrtTlsResumeRelease(pExpired);
	xrtTlsResumeRelease(pResume);
	xrtSecureZero(Secret, sizeof(Secret));
}



/* 执行 TLS 客户端首航回归。 */
int main(void)
{
	testTlsClientStart();
	testTlsClientVerifyName();
	testTlsClientResumeStart();
	testTlsClientConfig();
	testTlsClientResumeReject();
	return 0;
}
