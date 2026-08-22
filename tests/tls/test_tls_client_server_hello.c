#include "../test.h"
#include "../../src/internal/xrt_tls_client.h"
#include "../fixtures/tls_identity_legacy.h"



#define TEST_TLS_CLIENT_HELLO_CAPACITY 512u
#define TEST_TLS_SERVER_HELLO_CAPACITY 256u
#define TEST_TLS_RECORD_CAPACITY 512u



/* ServerHello 变体用于逐项验证客户端角色的协商约束。 */
typedef enum test_tls_server_hello_mode {
	TEST_TLS_SERVER_HELLO_VALID = 0,
	TEST_TLS_SERVER_HELLO_WRONG_SESSION,
	TEST_TLS_SERVER_HELLO_UNOFFERED_CIPHER,
	TEST_TLS_SERVER_HELLO_MISSING_VERSION,
	TEST_TLS_SERVER_HELLO_MISSING_SHARE,
	TEST_TLS_SERVER_HELLO_WRONG_GROUP,
	TEST_TLS_SERVER_HELLO_RETRY,
	TEST_TLS_SERVER_HELLO_PSK,
	TEST_TLS_SERVER_HELLO_PSK_BAD_INDEX,
	TEST_TLS_SERVER_HELLO_PSK_MISSING_SHARE
} test_tls_server_hello_mode;



/* EncryptedExtensions 变体覆盖协商成功和未请求扩展。 */
typedef enum test_tls_encrypted_extensions_mode {
	TEST_TLS_ENCRYPTED_EXTENSIONS_VALID = 0,
	TEST_TLS_ENCRYPTED_EXTENSIONS_NO_ALPN,
	TEST_TLS_ENCRYPTED_EXTENSIONS_HTTP1,
	TEST_TLS_ENCRYPTED_EXTENSIONS_BAD_ALPN,
	TEST_TLS_ENCRYPTED_EXTENSIONS_EARLY_DATA,
	TEST_TLS_ENCRYPTED_EXTENSIONS_UNKNOWN
} test_tls_encrypted_extensions_mode;



/* Fixture 保留两端临时密钥和完整线路 transcript。 */
typedef struct test_tls_client_server_hello {
	xtlscontext* Context;
	xtlssession* Client;
	xtlsclientstate* State;
	uint8 ClientHello[TEST_TLS_CLIENT_HELLO_CAPACITY];
	size_t ClientHelloSize;
	uint8 ClientPublic[32];
	uint8 ServerPrivate[32];
	uint8 ServerPublic[32];
	uint8 ServerHello[TEST_TLS_SERVER_HELLO_CAPACITY];
	size_t ServerHelloSize;
	uint8 ResumptionMaster[32];
} test_tls_client_server_hello;



/* 客户端握手故障分配器只在指定的一次底层申请中返回失败。 */
typedef struct test_tls_client_alloc {
	size_t Calls;
	size_t FailAt;
	size_t LastSize;
	size_t FailSize;
	bool FailSizeOnce;
	bool Failed;
} test_tls_client_alloc;



static test_tls_client_alloc TestTlsClientAlloc = {
	0, SIZE_MAX, 0, SIZE_MAX, false, false
};



/* 统计底层分配，并在目标序号注入一次 OOM。 */
static ptr testTlsClientAlloc(ptr pContext, size_t iSize)
{
	test_tls_client_alloc* pState = (test_tls_client_alloc*)pContext;

	pState->Calls++;
	pState->LastSize = iSize;
	if ( (pState->Calls == pState->FailAt) ||
		(pState->FailSizeOnce && !pState->Failed &&
		(iSize == pState->FailSize)) ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配与普通分配共享同一条故障序列。 */
static ptr testTlsClientRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_tls_client_alloc* pState = (test_tls_client_alloc*)pContext;

	pState->Calls++;
	pState->LastSize = iSize;
	if ( (pState->Calls == pState->FailAt) ||
		(pState->FailSizeOnce && !pState->Failed &&
		(iSize == pState->FailSize)) ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放故障分配器成功交付的内存。 */
static void testTlsClientFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 创建只启用 AES-128-GCM、SHA-256 和 X25519 的客户端上下文。 */
static xtlscontext* testTlsClientServerContext(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_13 };
	static const xtlscipher Ciphers[] = { XTLS_AES_128_GCM_SHA256 };
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
	Config.Limits.RecordBudget = 1u;
	Config.Limits.HandshakeBudget = 1u;
	return xrtTlsContextCreate(&Config);
}



/* 从客户端首航记录提取完整 ClientHello 和 X25519 公钥。 */
static void testTlsClientServerReadOffer(
	test_tls_client_server_hello* pFixture
)
{
	xnetspan Span;
	xtlsrecord Record;
	xtlshandshake Handshake;
	xtlsclienthello Hello;
	xtlsextension Extension;
	xtlskeysharecursor Cursor;
	xtlskeyshare Share;

	testRequire(xrtTlsSessionSendFront(pFixture->Client, &Span) &&
		(xrtTlsRecordParse(
			(xbytesview) { Span.Data, Span.Size }, &Record, NULL
		) == XTLS_OK) &&
		(xrtTlsHandshakeParse(
			Record.Payload, &Handshake, NULL
		) == XTLS_OK) &&
		(Handshake.Type == XTLS_HANDSHAKE_CLIENT_HELLO) &&
		xrtTlsClientHelloParse(Handshake.Body, &Hello),
		"TLS client fixture ClientHello parse failed");
	testRequire((Handshake.EncodedSize <= sizeof(pFixture->ClientHello)) &&
		(xrtTlsExtensionsFind(
			Hello.Extensions, XTLS_EXTENSION_KEY_SHARE, &Extension
		) == XTLS_ITEM_VALUE) &&
		xrtTlsClientKeyShares(Extension.Data, &Cursor) &&
		(xrtTlsKeySharesRead(&Cursor, &Share) == XTLS_ITEM_VALUE) &&
		(Share.Group == XTLS_GROUP_X25519) &&
		(Share.Key.Size == sizeof(pFixture->ClientPublic)),
		"TLS client fixture key share parse failed");
	pFixture->ClientHelloSize = Handshake.EncodedSize;
	memcpy(
		pFixture->ClientHello,
		Handshake.Body.Data - XTLS_HANDSHAKE_HEADER_SIZE,
		Handshake.EncodedSize
	);
	memcpy(pFixture->ClientPublic, Share.Key.Data, Share.Key.Size);
}



/* 构建指定语义变体的完整 ServerHello 握手消息。 */
static void testTlsClientServerBuildHello(
	test_tls_client_server_hello* pFixture,
	test_tls_server_hello_mode Mode
)
{
	static const uint8 RetryRandom[XTLS_RANDOM_SIZE] = {
		0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11,
		0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
		0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E,
		0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C
	};
	uint8 Random[XTLS_RANDOM_SIZE];
	uint8 SessionId[XTLS_SESSION_ID_MAX];
	uint8 Extensions[96];
	uint8 Body[192];
	xtlswriter Writer;
	xtlskeyshare Share;
	xtlsserverhello Hello;
	size_t iBodySize;

	for ( size_t i = 0; i < sizeof(Random); i++ ) {
		Random[i] = (uint8)(0x80u + i);
	}
	memcpy(SessionId, pFixture->State->SessionId, sizeof(SessionId));
	if ( Mode == TEST_TLS_SERVER_HELLO_WRONG_SESSION ) {
		SessionId[0] ^= 0xFFu;
	}
	testRequire(xrtTlsWriterInit(
		&Writer, Extensions, sizeof(Extensions)
	), "TLS server fixture writer initialization failed");
	if ( Mode != TEST_TLS_SERVER_HELLO_MISSING_VERSION ) {
		testRequire(xrtTlsWriterServerVersion(
			&Writer, XTLS_VERSION_13
		), "TLS server fixture version extension failed");
	}
	if ( Mode == TEST_TLS_SERVER_HELLO_RETRY ) {
		testRequire(xrtTlsWriterRetryGroup(
			&Writer, XTLS_GROUP_X25519
		), "TLS server fixture retry extension failed");
	} else if ( (Mode != TEST_TLS_SERVER_HELLO_MISSING_SHARE) &&
		(Mode != TEST_TLS_SERVER_HELLO_PSK_MISSING_SHARE) ) {
		Share.Group = XTLS_GROUP_X25519;
		Share.Key = (xbytesview) {
			pFixture->ServerPublic, sizeof(pFixture->ServerPublic)
		};
		testRequire(xrtTlsWriterServerKeyShare(
			&Writer, &Share
		), "TLS server fixture key-share extension failed");
	}
	if ( (Mode == TEST_TLS_SERVER_HELLO_PSK) ||
		(Mode == TEST_TLS_SERVER_HELLO_PSK_BAD_INDEX) ||
		(Mode == TEST_TLS_SERVER_HELLO_PSK_MISSING_SHARE) ) {
		testRequire(xrtTlsWriterServerPsk(
			&Writer,
			Mode == TEST_TLS_SERVER_HELLO_PSK_BAD_INDEX ? 1u : 0u
		), "TLS server fixture PSK selection failed");
	}

	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = Mode == TEST_TLS_SERVER_HELLO_RETRY ?
		(xbytesview) { RetryRandom, sizeof(RetryRandom) } :
		(xbytesview) { Random, sizeof(Random) };
	Hello.SessionId = (xbytesview) { SessionId, sizeof(SessionId) };
	Hello.CipherSuite = Mode == TEST_TLS_SERVER_HELLO_UNOFFERED_CIPHER ?
		XTLS_AES_256_GCM_SHA384 : XTLS_AES_128_GCM_SHA256;
	Hello.Extensions = xrtTlsWriterData(&Writer);
	Hello.Retry = Mode == TEST_TLS_SERVER_HELLO_RETRY;
	iBodySize = xrtTlsServerHelloSize(&Hello);
	testRequire((iBodySize != 0) && (iBodySize <= sizeof(Body)) &&
		xrtTlsServerHelloEncode(&Hello, Body, sizeof(Body)),
		"TLS server fixture ServerHello encoding failed");
	pFixture->ServerHelloSize = xrtTlsHandshakeSize(iBodySize);
	testRequire((pFixture->ServerHelloSize != 0) &&
		(pFixture->ServerHelloSize <= sizeof(pFixture->ServerHello)) &&
		xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_SERVER_HELLO,
			(xbytesview) { Body, iBodySize },
			pFixture->ServerHello, sizeof(pFixture->ServerHello)
		), "TLS server fixture handshake encoding failed");

	if ( Mode == TEST_TLS_SERVER_HELLO_WRONG_GROUP ) {
		xtlshandshake Handshake;
		xtlsserverhello Parsed;
		xtlsextension Extension;

		testRequire((xrtTlsHandshakeParse(
			(xbytesview) {
				pFixture->ServerHello, pFixture->ServerHelloSize
			}, &Handshake, NULL
		) == XTLS_OK) && xrtTlsServerHelloParse(
			Handshake.Body, &Parsed
		) && (xrtTlsExtensionsFind(
			Parsed.Extensions, XTLS_EXTENSION_KEY_SHARE, &Extension
		) == XTLS_ITEM_VALUE),
			"TLS wrong-group fixture parse failed");
		((uint8*)Extension.Data.Data)[0] =
			(uint8)(XTLS_GROUP_SECP256R1 >> 8u);
		((uint8*)Extension.Data.Data)[1] =
			(uint8)XTLS_GROUP_SECP256R1;
	}
}



/* 创建客户端、复制首航资产并构建服务端响应。 */
static void testTlsClientServerFixtureInitWithVerifierResume(
	test_tls_client_server_hello* pFixture,
	test_tls_server_hello_mode Mode,
	const xtlsverifier* pVerifier,
	const xtlsresume* pResume
)
{
	static const xstrview Protocols[] = {
		{ "h2", sizeof("h2") - 1u },
		{ "http/1.1", sizeof("http/1.1") - 1u }
	};
	xtlsclientconfig Config;

	memset(pFixture, 0, sizeof(*pFixture));
	pFixture->Context = testTlsClientServerContext();
	testRequire(pFixture->Context != NULL,
		"TLS server fixture context creation failed");
	xrtTlsClientConfigInit(&Config);
	Config.Context = pFixture->Context;
	Config.ServerName = XRT_STR_LITERAL("example.com");
	Config.Protocols = Protocols;
	Config.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	Config.Verifier = pVerifier;
	Config.Resume = pResume;
	pFixture->Client = xrtTlsClientCreate(&Config, NULL);
	testRequire(pFixture->Client != NULL,
		"TLS server fixture client creation failed");
	pFixture->State = (xtlsclientstate*)__xrtTlsSessionRoleData(
		pFixture->Client
	);
	testRequire(pFixture->State != NULL,
		"TLS server fixture role state is missing");
	testTlsClientServerReadOffer(pFixture);
	testRequire(xrtTlsKeyShareGenerate(
		XTLS_GROUP_X25519,
		pFixture->ServerPrivate, sizeof(pFixture->ServerPrivate),
		pFixture->ServerPublic, sizeof(pFixture->ServerPublic)
	), "TLS server fixture key generation failed");
	testTlsClientServerBuildHello(pFixture, Mode);
	testRequire(xrtTlsSessionSendConsume(
		pFixture->Client, xrtTlsSessionSendSize(pFixture->Client)
	), "TLS client fixture initial send consumption failed");
}



/* 创建可选验证器但不提供恢复票据的完整握手 fixture。 */
static void testTlsClientServerFixtureInitWithVerifier(
	test_tls_client_server_hello* pFixture,
	test_tls_server_hello_mode Mode,
	const xtlsverifier* pVerifier
)
{
	testTlsClientServerFixtureInitWithVerifierResume(
		pFixture, Mode, pVerifier, NULL
	);
}



/* 创建与 fixture 路由和 SHA-256 套件绑定的已认证恢复对象。 */
static xtlsresume* testTlsClientServerResume(uint8* pSecret)
{
	xtlsresumeconfig Config;

	for ( size_t i = 0; i < 32u; i++ ) {
		pSecret[i] = (uint8)(0xA0u + i);
	}
	xrtTlsResumeConfigInit(&Config);
	Config.Cipher = XTLS_AES_128_GCM_SHA256;
	Config.Ticket = XRT_BYTES_LITERAL("server-fixture-ticket");
	Config.Secret = (xbytesview) { pSecret, 32u };
	Config.ServerName = XRT_STR_LITERAL("example.com");
	Config.Protocol = XRT_BYTES_LITERAL("h2");
	Config.PeerIdentity = XRT_BYTES_LITERAL("authenticated-peer");
	Config.Lifetime = 3600u;
	Config.AgeAdd = UINT32_C(0x11223344);
	return xrtTlsResumeCreate(&Config);
}



/* 创建提供单张恢复票据的客户端，并构造对应 ServerHello 变体。 */
static xtlsresume* testTlsClientServerFixtureInitResume(
	test_tls_client_server_hello* pFixture,
	test_tls_server_hello_mode Mode,
	uint8* pSecret
)
{
	xtlsresume* pResume = testTlsClientServerResume(pSecret);

	testRequire(pResume != NULL,
		"TLS resumed fixture object creation failed");
	testTlsClientServerFixtureInitWithVerifierResume(
		pFixture, Mode, NULL, pResume
	);
	return pResume;
}



/* 创建不绑定验证器的基础客户端 fixture。 */
static void testTlsClientServerFixtureInit(
	test_tls_client_server_hello* pFixture,
	test_tls_server_hello_mode Mode
)
{
	testTlsClientServerFixtureInitWithVerifier(pFixture, Mode, NULL);
}



/* 释放 fixture 的会话和共享上下文。 */
static void testTlsClientServerFixtureUnit(
	test_tls_client_server_hello* pFixture
)
{
	xrtTlsSessionDestroy(pFixture->Client);
	xrtTlsContextRelease(pFixture->Context);
	memset(pFixture, 0, sizeof(*pFixture));
}



/* 把一个握手分片包装为独立明文记录并驱动客户端。 */
static xtlsresult testTlsClientServerFeed(
	test_tls_client_server_hello* pFixture,
	size_t iOffset,
	size_t iSize
)
{
	uint8 Record[TEST_TLS_RECORD_CAPACITY];
	size_t iRecordSize = xrtTlsRecordSize(iSize);

	testRequire((iOffset <= pFixture->ServerHelloSize) &&
		(iSize <= (pFixture->ServerHelloSize - iOffset)) &&
		(iRecordSize <= sizeof(Record)) && xrtTlsRecordEncode(
			XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
			(xbytesview) { pFixture->ServerHello + iOffset, iSize },
			Record, sizeof(Record)
		) && (xrtTlsSessionFeed(
			pFixture->Client, Record, iRecordSize
		) == XTLS_OK), "TLS ServerHello record feed failed");
	return xrtTlsClientDrive(pFixture->Client);
}



/* 构建完整 EncryptedExtensions 握手消息。 */
static size_t testTlsClientEncryptedExtensions(
	test_tls_encrypted_extensions_mode Mode,
	void* pOutput,
	size_t iCapacity
)
{
	static const xbytesview H2[] = { XRT_BYTES_INIT("h2") };
	static const xbytesview Http1[] = { XRT_BYTES_INIT("http/1.1") };
	static const xbytesview H3[] = { XRT_BYTES_INIT("h3") };
	uint8 Extensions[128];
	uint8 Body[160];
	xtlswriter Writer;
	xbytesview Data;
	size_t iBodySize;
	size_t iMessageSize;
	uint16 Group = XTLS_GROUP_X25519;

	testRequire(xrtTlsWriterInit(
		&Writer, Extensions, sizeof(Extensions)
	), "TLS EncryptedExtensions writer initialization failed");
	if ( Mode == TEST_TLS_ENCRYPTED_EXTENSIONS_EARLY_DATA ) {
		testRequire(xrtTlsWriterExtension(
			&Writer, XTLS_EXTENSION_EARLY_DATA,
			(xbytesview) { NULL, 0 }
		), "TLS EncryptedExtensions early-data encoding failed");
	} else if ( Mode == TEST_TLS_ENCRYPTED_EXTENSIONS_UNKNOWN ) {
		testRequire(xrtTlsWriterExtension(
			&Writer, (xtlsextensiontype)65000,
			XRT_BYTES_LITERAL("x")
		), "TLS EncryptedExtensions unknown encoding failed");
	} else {
		testRequire(xrtTlsWriterExtension(
			&Writer, XTLS_EXTENSION_SERVER_NAME,
			(xbytesview) { NULL, 0 }
		) && xrtTlsWriterIds(
			&Writer, XTLS_EXTENSION_SUPPORTED_GROUPS, &Group, 1u
		), "TLS EncryptedExtensions acknowledgement encoding failed");
		if ( Mode != TEST_TLS_ENCRYPTED_EXTENSIONS_NO_ALPN ) {
			Data = Mode == TEST_TLS_ENCRYPTED_EXTENSIONS_BAD_ALPN ?
				H3[0] : (Mode == TEST_TLS_ENCRYPTED_EXTENSIONS_HTTP1 ?
				 Http1[0] : H2[0]);
			testRequire(xrtTlsWriterProtocols(
				&Writer, &Data, 1u
			), "TLS EncryptedExtensions ALPN encoding failed");
		}
	}
	Data = xrtTlsWriterData(&Writer);
	iBodySize = xrtTlsEncryptedExtensionsSize(Data);
	testRequire((iBodySize != 0) && (iBodySize <= sizeof(Body)) &&
		xrtTlsEncryptedExtensionsEncode(
			Data, Body, sizeof(Body)
		), "TLS EncryptedExtensions body encoding failed");
	iMessageSize = xrtTlsHandshakeSize(iBodySize);
	testRequire((iMessageSize != 0) && (iMessageSize <= iCapacity) &&
		xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_ENCRYPTED_EXTENSIONS,
			(xbytesview) { Body, iBodySize }, pOutput, iCapacity
		), "TLS EncryptedExtensions handshake encoding failed");
	if ( Mode == TEST_TLS_ENCRYPTED_EXTENSIONS_VALID ) {
		xtlshandshake Handshake;
		xbytesview ParsedExtensions;
		xtlsextension Extension;
		xbytesview Selected;

		testRequire((xrtTlsHandshakeParse(
			(xbytesview) { (const uint8*)pOutput, iMessageSize },
			&Handshake, NULL
		) == XTLS_OK) && xrtTlsEncryptedExtensionsParse(
			Handshake.Body, &ParsedExtensions
		) && (xrtTlsExtensionsFind(
			ParsedExtensions, XTLS_EXTENSION_ALPN, &Extension
		) == XTLS_ITEM_VALUE) && xrtTlsProtocolSelected(
			Extension.Data, &Selected
		) && (Selected.Size == 2u) &&
		(memcmp(Selected.Data, "h2", 2u) == 0),
			"TLS EncryptedExtensions ALPN fixture is invalid");
	}
	return iMessageSize;
}



/* 独立派生并安装服务端握手流量密钥，同时验证客户端中间秘密。 */
static xtlssession* testTlsClientServerPeer(
	test_tls_client_server_hello* pFixture
)
{
	uint8 Shared[32];
	uint8 Zero[32] = { 0 };
	uint8 EmptyHash[32];
	uint8 HandshakeHash[32];
	uint8 Early[32];
	uint8 Derived[32];
	uint8 HandshakeSecret[32];
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 ClientKey[16];
	uint8 ClientIv[12];
	uint8 ServerKey[16];
	uint8 ServerIv[12];
	xsha256 Transcript;
	xtlssession* pServer;
	xbytesview Empty = { NULL, 0 };
	xbytesview Psk = { Zero, sizeof(Zero) };
	xtlsresumeinfo Resume;

	if ( pFixture->State->Resumed ) {
		testRequire((pFixture->State->OfferResume != NULL) &&
			xrtTlsResumeInfo(
				pFixture->State->OfferResume, &Resume
			) && (Resume.Secret.Size == sizeof(Zero)),
			"TLS resumed fixture PSK is unavailable");
		Psk = Resume.Secret;
	}

	testRequire(xrtTlsKeyShareDerive(
		XTLS_GROUP_X25519,
		(xbytesview) {
			pFixture->ServerPrivate, sizeof(pFixture->ServerPrivate)
		},
		(xbytesview) {
			pFixture->ClientPublic, sizeof(pFixture->ClientPublic)
		}, Shared, sizeof(Shared)
	), "TLS server fixture shared-secret derivation failed");
	xrtSha256Init(&Transcript);
	testRequire(xrtSha256(NULL, 0, EmptyHash) &&
		xrtSha256Update(
			&Transcript, pFixture->ClientHello,
			pFixture->ClientHelloSize
		) && xrtSha256Update(
			&Transcript, pFixture->ServerHello,
			pFixture->ServerHelloSize
		) && xrtSha256Final(&Transcript, HandshakeHash),
		"TLS server fixture transcript digest failed");
	testRequire(__xrtTls13Extract(
		XCRYPTO_HASH_SHA256, Empty, Psk, Early, sizeof(Early)
	) && __xrtTls13DeriveSecret(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Early, sizeof(Early) },
		XRT_STR_LITERAL("derived"),
		(xbytesview) { EmptyHash, sizeof(EmptyHash) },
		Derived, sizeof(Derived)
	) && __xrtTls13Extract(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { Derived, sizeof(Derived) },
		(xbytesview) { Shared, sizeof(Shared) },
		HandshakeSecret, sizeof(HandshakeSecret)
	) && __xrtTls13DeriveSecret(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { HandshakeSecret, sizeof(HandshakeSecret) },
		XRT_STR_LITERAL("c hs traffic"),
		(xbytesview) { HandshakeHash, sizeof(HandshakeHash) },
		ClientTraffic, sizeof(ClientTraffic)
	) && __xrtTls13DeriveSecret(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { HandshakeSecret, sizeof(HandshakeSecret) },
		XRT_STR_LITERAL("s hs traffic"),
		(xbytesview) { HandshakeHash, sizeof(HandshakeHash) },
		ServerTraffic, sizeof(ServerTraffic)
	), "TLS server fixture traffic-secret derivation failed");
	testRequire((memcmp(
		pFixture->State->HandshakeSecret,
		HandshakeSecret, sizeof(HandshakeSecret)
	) == 0) && (memcmp(
		pFixture->State->ClientHandshakeTraffic,
		ClientTraffic, sizeof(ClientTraffic)
	) == 0) && (memcmp(
		pFixture->State->ServerHandshakeTraffic,
		ServerTraffic, sizeof(ServerTraffic)
	) == 0), "TLS client committed incorrect handshake secrets");
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { ClientTraffic, sizeof(ClientTraffic) },
		XRT_STR_LITERAL("key"), Empty, ClientKey, sizeof(ClientKey)
	) && __xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { ClientTraffic, sizeof(ClientTraffic) },
		XRT_STR_LITERAL("iv"), Empty, ClientIv, sizeof(ClientIv)
	) && __xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { ServerTraffic, sizeof(ServerTraffic) },
		XRT_STR_LITERAL("key"), Empty, ServerKey, sizeof(ServerKey)
	) && __xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { ServerTraffic, sizeof(ServerTraffic) },
		XRT_STR_LITERAL("iv"), Empty, ServerIv, sizeof(ServerIv)
	), "TLS server fixture record-key expansion failed");

	pServer = __xrtTlsSessionCreate(
		pFixture->Context, NULL, XTLS_SERVER
	);
	testRequire((pServer != NULL) && __xrtTlsSessionKeys(
		pServer, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { ClientKey, sizeof(ClientKey) },
		(xbytesview) { ClientIv, sizeof(ClientIv) },
		(xbytesview) { ServerKey, sizeof(ServerKey) },
		(xbytesview) { ServerIv, sizeof(ServerIv) }
	), "TLS server fixture record-key installation failed");
	xrtSecureZero(ServerIv, sizeof(ServerIv));
	xrtSecureZero(ServerKey, sizeof(ServerKey));
	xrtSecureZero(ClientIv, sizeof(ClientIv));
	xrtSecureZero(ClientKey, sizeof(ClientKey));
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtSecureZero(HandshakeSecret, sizeof(HandshakeSecret));
	xrtSecureZero(Derived, sizeof(Derived));
	xrtSecureZero(Early, sizeof(Early));
	xrtSecureZero(HandshakeHash, sizeof(HandshakeHash));
	xrtSecureZero(EmptyHash, sizeof(EmptyHash));
	xrtSecureZero(Shared, sizeof(Shared));
	return pServer;
}



/* 由服务端保护一个握手分片，搬运全部密文后驱动客户端。 */
static xtlsresult testTlsClientServerProtect(
	test_tls_client_server_hello* pFixture,
	xtlssession* pServer,
	const void* pData,
	size_t iSize
)
{
	xnetspan Span;

	testRequire(__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { (const uint8*)pData, iSize }, 0
	) == XTLS_OK, "TLS server fixture handshake protection failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				pFixture->Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS server fixture handshake record move failed");
	}
	return xrtTlsClientDrive(pFixture->Client);
}



/* 测试验证器只接管信任决策，握手签名仍必须由客户端验证。 */
static xtlsverifydecision testTlsClientAcceptPeer(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	if ( (pPeer == NULL) || (pPeer->Role != XTLS_SERVER) ||
		(pPeer->CertificateCount == 0) ) {
		return XTLS_VERIFY_REJECT;
	}
	return XTLS_VERIFY_ACCEPT;
}



/* 创建测试专用信任决策器，避免历史自签名证书绕过握手签名测试。 */
static xtlsverifier* testTlsClientVerifier(void)
{
	xtlsverifierconfig Config;

	xrtTlsVerifierConfigInit(&Config);
	Config.Verify = testTlsClientAcceptPeer;
	return xrtTlsVerifierCreate(&Config);
}



/* 从旧版真实证书和匹配私钥创建 RSA 服务端身份。 */
static xtlsidentity* testTlsClientIdentity(void)
{
	uint8 PrivateKey[2048];
	xbytesview Certificate = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentity* pIdentity;
	size_t iPrivateKeySize = 0;

	testRequire(testTlsIdentityLegacyKey(
		PrivateKey, sizeof(PrivateKey), &iPrivateKeySize
	), "TLS client fixture private key decoding failed");
	pIdentity = xrtTlsIdentityRsa(
		&Certificate, 1u,
		(xbytesview) { PrivateKey, iPrivateKeySize }
	);
	xrtSecureZero(PrivateKey, sizeof(PrivateKey));
	testRequire(pIdentity != NULL,
		"TLS client fixture RSA identity creation failed");
	return pIdentity;
}



/* 构建携带真实 DER 证书和可选条目扩展的握手消息。 */
static size_t testTlsClientCertificateMessage(
	xbytesview Extensions,
	void* pOutput,
	size_t iCapacity
)
{
	uint8 Body[2048];
	xtlscertificateentry Entry;
	size_t iBodySize;
	size_t iMessageSize;

	memset(&Entry, 0, sizeof(Entry));
	Entry.Data = (xbytesview) {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	Entry.Extensions = Extensions;
	iBodySize = xrtTlsCertificateSize(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, &Entry, 1u
	);
	iMessageSize = xrtTlsHandshakeSize(iBodySize);
	testRequire((iBodySize != 0) && (iBodySize <= sizeof(Body)) &&
		(iMessageSize != 0) && (iMessageSize <= iCapacity) &&
		xrtTlsCertificateEncode(
			XTLS_VERSION_13, (xbytesview) { NULL, 0 }, &Entry, 1u,
			Body, sizeof(Body)
		) && xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_CERTIFICATE,
			(xbytesview) { Body, iBodySize }, pOutput, iCapacity
		), "TLS client fixture Certificate encoding failed");
	return iMessageSize;
}



/* 构建 RFC 8446 CertificateVerify 完整待签内容。 */
static size_t testTlsClientCertificateVerifyContent(
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iCapacity
)
{
	static const char Context[] = "TLS 1.3, server CertificateVerify";
	const size_t iRequired = 64u + sizeof(Context) + TranscriptHash.Size;
	bytes pWrite = (bytes)pOutput;

	testRequire(iRequired <= iCapacity,
		"TLS client fixture CertificateVerify content is too large");
	memset(pWrite, 0x20, 64u);
	memcpy(pWrite + 64u, Context, sizeof(Context) - 1u);
	pWrite[64u + sizeof(Context) - 1u] = 0;
	memcpy(
		pWrite + 64u + sizeof(Context),
		TranscriptHash.Data, TranscriptHash.Size
	);
	return iRequired;
}



/* 对当前客户端 transcript 签名并编码 CertificateVerify 握手消息。 */
static size_t testTlsClientCertificateVerifyMessage(
	const test_tls_client_server_hello* pFixture,
	const xtlsidentity* pIdentity,
	xtlssignature Scheme,
	bool bDamage,
	void* pOutput,
	size_t iCapacity
)
{
	uint8 Digest[32];
	uint8 Content[160];
	uint8 Signature[512];
	uint8 Body[520];
	xtlscertificateverify Verify;
	size_t iContentSize;
	size_t iSignatureSize = 0;
	size_t iBodySize;
	size_t iMessageSize;

	testRequire(__xrtTlsTranscriptDigest(
		&pFixture->State->Transcript, Digest, sizeof(Digest)
	), "TLS client fixture transcript digest failed");
	iContentSize = testTlsClientCertificateVerifyContent(
		(xbytesview) { Digest, sizeof(Digest) }, Content, sizeof(Content)
	);
	testRequire(xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13, Scheme,
		(xbytesview) { Content, iContentSize },
		Signature, sizeof(Signature), &iSignatureSize
	) && (iSignatureSize != 0),
		"TLS client fixture CertificateVerify signing failed");
	if ( bDamage ) {
		Signature[iSignatureSize - 1u] ^= 1u;
	}
	Verify.Scheme = Scheme;
	Verify.Signature = (xbytesview) { Signature, iSignatureSize };
	iBodySize = xrtTlsCertificateVerifySize(&Verify);
	iMessageSize = xrtTlsHandshakeSize(iBodySize);
	testRequire((iBodySize != 0) && (iBodySize <= sizeof(Body)) &&
		(iMessageSize != 0) && (iMessageSize <= iCapacity) &&
		xrtTlsCertificateVerifyEncode(
			&Verify, Body, sizeof(Body)
		) && xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_CERTIFICATE_VERIFY,
			(xbytesview) { Body, iBodySize }, pOutput, iCapacity
		), "TLS client fixture CertificateVerify encoding failed");
	xrtSecureZero(Signature, sizeof(Signature));
	xrtSecureZero(Content, sizeof(Content));
	xrtSecureZero(Digest, sizeof(Digest));
	return iMessageSize;
}



/* 使用给定 transcript 和 traffic secret 构造真实 TLS 1.3 Finished。 */
static size_t testTlsClientFinishedMessage(
	const xtlstranscript* pTranscript,
	xbytesview Traffic,
	void* pOutput,
	size_t iCapacity
)
{
	uint8 Digest[32];
	uint8 FinishedKey[32];
	uint8 VerifyData[32];
	size_t iMessageSize = xrtTlsHandshakeSize(sizeof(VerifyData));

	testRequire((Traffic.Size == sizeof(FinishedKey)) &&
		(iMessageSize != 0) && (iMessageSize <= iCapacity) &&
		__xrtTlsTranscriptDigest(
			pTranscript, Digest, sizeof(Digest)
		) && __xrtTls13ExpandLabel(
			XCRYPTO_HASH_SHA256, Traffic,
			XRT_STR_LITERAL("finished"),
			(xbytesview) { NULL, 0 },
			FinishedKey, sizeof(FinishedKey)
		) && __xrtTls13Finished(
			XCRYPTO_HASH_SHA256,
			(xbytesview) { FinishedKey, sizeof(FinishedKey) },
			(xbytesview) { Digest, sizeof(Digest) },
			VerifyData, sizeof(VerifyData)
		) && xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_FINISHED,
			(xbytesview) { VerifyData, sizeof(VerifyData) },
			pOutput, iCapacity
		), "TLS client fixture Finished encoding failed");
	xrtSecureZero(VerifyData, sizeof(VerifyData));
	xrtSecureZero(FinishedKey, sizeof(FinishedKey));
	xrtSecureZero(Digest, sizeof(Digest));
	return iMessageSize;
}



/* 从握手 secret 和服务端 Finished transcript 派生双方应用 traffic secret。 */
static void testTlsClientApplicationTraffic(
	xbytesview HandshakeSecret,
	const xtlstranscript* pTranscript,
	void* pClientTraffic,
	void* pServerTraffic,
	void* pMasterSecret
)
{
	uint8 EmptyHash[32];
	uint8 Derived[32];
	uint8 Master[32];
	uint8 Digest[32];
	uint8 Zero[32] = { 0 };
	xtlstranscript Empty;

	memset(&Empty, 0, sizeof(Empty));
	testRequire((HandshakeSecret.Size == sizeof(Master)) &&
		__xrtTlsTranscriptInit(&Empty, XCRYPTO_HASH_SHA256) &&
		__xrtTlsTranscriptDigest(
			&Empty, EmptyHash, sizeof(EmptyHash)
		) && __xrtTls13DeriveSecret(
			XCRYPTO_HASH_SHA256, HandshakeSecret,
			XRT_STR_LITERAL("derived"),
			(xbytesview) { EmptyHash, sizeof(EmptyHash) },
			Derived, sizeof(Derived)
		) && __xrtTls13Extract(
			XCRYPTO_HASH_SHA256,
			(xbytesview) { Derived, sizeof(Derived) },
			(xbytesview) { Zero, sizeof(Zero) },
			Master, sizeof(Master)
		) && __xrtTlsTranscriptDigest(
			pTranscript, Digest, sizeof(Digest)
		) && __xrtTls13DeriveSecret(
			XCRYPTO_HASH_SHA256,
			(xbytesview) { Master, sizeof(Master) },
			XRT_STR_LITERAL("c ap traffic"),
			(xbytesview) { Digest, sizeof(Digest) },
			pClientTraffic, sizeof(Master)
		) && __xrtTls13DeriveSecret(
			XCRYPTO_HASH_SHA256,
			(xbytesview) { Master, sizeof(Master) },
			XRT_STR_LITERAL("s ap traffic"),
			(xbytesview) { Digest, sizeof(Digest) },
			pServerTraffic, sizeof(Master)
		), "TLS client fixture application traffic derivation failed");
	if ( pMasterSecret != NULL ) {
		memcpy(pMasterSecret, Master, sizeof(Master));
	}
	__xrtTlsTranscriptClear(&Empty);
	xrtSecureZero(Zero, sizeof(Zero));
	xrtSecureZero(Digest, sizeof(Digest));
	xrtSecureZero(Master, sizeof(Master));
	xrtSecureZero(Derived, sizeof(Derived));
	xrtSecureZero(EmptyHash, sizeof(EmptyHash));
}



/* 从 SHA-256 应用 traffic secret 派生 AES-128-GCM 记录密钥材料。 */
static void testTlsClientRecordMaterial(
	xbytesview Traffic,
	void* pKey,
	void* pIv
)
{
	testRequire((Traffic.Size == 32u) && (pKey != NULL) && (pIv != NULL) &&
		__xrtTls13ExpandLabel(
			XCRYPTO_HASH_SHA256, Traffic,
			XRT_STR_LITERAL("key"), (xbytesview) { NULL, 0 },
			pKey, 16u
		) && __xrtTls13ExpandLabel(
			XCRYPTO_HASH_SHA256, Traffic,
			XRT_STR_LITERAL("iv"), (xbytesview) { NULL, 0 },
			pIv, 12u
		), "TLS client fixture record material derivation failed");
}



/* 按服务端视角一次安装双方 TLS 1.3 应用记录密钥。 */
static void testTlsClientServerApplicationKeys(
	xtlssession* pServer,
	xbytesview ClientTraffic,
	xbytesview ServerTraffic
)
{
	uint8 ClientKey[16];
	uint8 ClientIv[12];
	uint8 ServerKey[16];
	uint8 ServerIv[12];

	testTlsClientRecordMaterial(ClientTraffic, ClientKey, ClientIv);
	testTlsClientRecordMaterial(ServerTraffic, ServerKey, ServerIv);
	testRequire(__xrtTlsSessionKeys(
		pServer, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { ClientKey, sizeof(ClientKey) },
		(xbytesview) { ClientIv, sizeof(ClientIv) },
		(xbytesview) { ServerKey, sizeof(ServerKey) },
		(xbytesview) { ServerIv, sizeof(ServerIv) }
	), "TLS server application epoch installation failed");
	xrtSecureZero(ServerIv, sizeof(ServerIv));
	xrtSecureZero(ServerKey, sizeof(ServerKey));
	xrtSecureZero(ClientIv, sizeof(ClientIv));
	xrtSecureZero(ClientKey, sizeof(ClientKey));
}



/* 按 RFC 8446 traffic upd 标签派生下一代应用 traffic secret。 */
static void testTlsClientTrafficUpdate(
	xbytesview Current,
	void* pNext
)
{
	testRequire((Current.Size == 32u) && (pNext != NULL) &&
		__xrtTls13ExpandLabel(
			XCRYPTO_HASH_SHA256, Current,
			XRT_STR_LITERAL("traffic upd"),
			(xbytesview) { NULL, 0 }, pNext, 32u
		), "TLS client fixture traffic update failed");
}



/* 编码一条完整 TLS 1.3 KeyUpdate 握手消息。 */
static size_t testTlsClientKeyUpdateMessage(
	xtlskeyupdate Request,
	void* pOutput,
	size_t iCapacity
)
{
	uint8 Body[1];
	size_t iSize = xrtTlsHandshakeSize(sizeof(Body));

	testRequire((iSize != 0) && (iSize <= iCapacity) &&
		xrtTlsKeyUpdateEncode(Request, Body, sizeof(Body)) &&
		xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_KEY_UPDATE,
			(xbytesview) { Body, sizeof(Body) },
			pOutput, iCapacity
		), "TLS client fixture KeyUpdate encoding failed");
	return iSize;
}



/* 使用调用方票据数据编码一条 TLS 1.3 NewSessionTicket。 */
static size_t testTlsClientSessionTicketMessageData(
	void* pOutput,
	size_t iCapacity,
	xbytesview TicketData
)
{
	static const uint8 Nonce[] = { 1, 2 };
	static const uint8 Extensions[] = {
		0x00, XTLS_EXTENSION_EARLY_DATA,
		0x00, 0x04,
		0x00, 0x00, 0x10, 0x00
	};
	xtlssessionticket Ticket;
	bytes pBody;
	size_t iBody;
	size_t iMessage;

	memset(&Ticket, 0, sizeof(Ticket));
	Ticket.Version = XTLS_VERSION_13;
	Ticket.Lifetime = 3600u;
	Ticket.AgeAdd = UINT32_C(0x01020304);
	Ticket.Nonce = (xbytesview) { Nonce, sizeof(Nonce) };
	Ticket.Ticket = TicketData;
	Ticket.Extensions = (xbytesview) { Extensions, sizeof(Extensions) };
	iBody = xrtTlsSessionTicketSize(&Ticket);
	iMessage = xrtTlsHandshakeSize(iBody);
	testRequire((iBody != 0) && (iMessage != 0) &&
		(iMessage <= iCapacity),
		"TLS client fixture NewSessionTicket size is invalid");
	pBody = (bytes)pOutput + (iMessage - iBody);
	testRequire(xrtTlsSessionTicketEncode(&Ticket, pBody, iBody) &&
		xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_NEW_SESSION_TICKET,
			(xbytesview) { pBody, iBody }, pOutput, iCapacity
		), "TLS client fixture NewSessionTicket encoding failed");
	return iMessage;
}



/* 编码常用的小型 TLS 1.3 票据。 */
static size_t testTlsClientSessionTicketMessage(
	void* pOutput,
	size_t iCapacity
)
{
	static const uint8 TicketData[] = { 3, 4, 5, 6 };

	return testTlsClientSessionTicketMessageData(
		pOutput, iCapacity,
		(xbytesview) { TicketData, sizeof(TicketData) }
	);
}



/* 推进到等待服务端 Certificate 的受保护握手状态。 */
static xtlssession* testTlsClientCertificateBeginMode(
	test_tls_client_server_hello* pFixture,
	bool bVerifier,
	test_tls_encrypted_extensions_mode Mode,
	void* pEncryptedExtensions,
	size_t iCapacity,
	size_t* pEncryptedExtensionsSize
)
{
	xtlsverifier* pVerifier = bVerifier ? testTlsClientVerifier() : NULL;
	xtlssession* pServer;

	if ( bVerifier ) {
		testRequire(pVerifier != NULL,
			"TLS client fixture verifier creation failed");
	}
	testTlsClientServerFixtureInitWithVerifier(
		pFixture, TEST_TLS_SERVER_HELLO_VALID, pVerifier
	);
	xrtTlsVerifierRelease(pVerifier);
	testRequire(testTlsClientServerFeed(
		pFixture, 0, pFixture->ServerHelloSize
	) == XTLS_OK, "TLS certificate fixture ServerHello failed");
	pServer = testTlsClientServerPeer(pFixture);
	*pEncryptedExtensionsSize = testTlsClientEncryptedExtensions(
		Mode, pEncryptedExtensions, iCapacity
	);
	testRequire(testTlsClientServerProtect(
		pFixture, pServer, pEncryptedExtensions,
		*pEncryptedExtensionsSize
	) == XTLS_OK &&
		(pFixture->State->Step == XTLS_CLIENT_WAIT_CERTIFICATE),
		"TLS client did not reach Certificate state");
	return pServer;
}



/* 使用真实证书和签名推进到等待服务端 Finished。 */
/* 以常用的无 ALPN 路径推进到等待服务端 Certificate。 */
static xtlssession* testTlsClientCertificateBegin(
	test_tls_client_server_hello* pFixture,
	bool bVerifier,
	void* pEncryptedExtensions,
	size_t iCapacity,
	size_t* pEncryptedExtensionsSize
)
{
	return testTlsClientCertificateBeginMode(
		pFixture, bVerifier, TEST_TLS_ENCRYPTED_EXTENSIONS_NO_ALPN,
		pEncryptedExtensions, iCapacity, pEncryptedExtensionsSize
	);
}



/* 使用指定扩展协商和真实证书签名推进到等待服务端 Finished。 */
static xtlssession* testTlsClientFinishedBeginMode(
	test_tls_client_server_hello* pFixture,
	test_tls_encrypted_extensions_mode Mode
)
{
	uint8 EncryptedExtensions[256];
	uint8 Certificate[2048];
	uint8 Verify[768];
	xtlsidentity* pIdentity = testTlsClientIdentity();
	xtlssession* pServer;
	size_t iEncryptedExtensions;
	size_t iCertificate;
	size_t iVerify;

	pServer = testTlsClientCertificateBeginMode(
		pFixture, true, Mode, EncryptedExtensions,
		sizeof(EncryptedExtensions), &iEncryptedExtensions
	);
	(void)iEncryptedExtensions;
	iCertificate = testTlsClientCertificateMessage(
		(xbytesview) { NULL, 0 }, Certificate, sizeof(Certificate)
	);
	testRequire(testTlsClientServerProtect(
		pFixture, pServer, Certificate, iCertificate
	) == XTLS_OK, "TLS Finished fixture Certificate failed");
	iVerify = testTlsClientCertificateVerifyMessage(
		pFixture, pIdentity, XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		false, Verify, sizeof(Verify)
	);
	testRequire(testTlsClientServerProtect(
		pFixture, pServer, Verify, iVerify
	) == XTLS_OK &&
		(pFixture->State->Step == XTLS_CLIENT_WAIT_FINISHED),
		"TLS client did not reach Finished state");
	xrtTlsIdentityRelease(pIdentity);
	return pServer;
}



/* 复用真实证书握手推进到 READY，并安装服务端视角的应用 epoch。 */
/* 以常用的无 ALPN 路径推进到等待服务端 Finished。 */
static xtlssession* testTlsClientFinishedBegin(
	test_tls_client_server_hello* pFixture
)
{
	return testTlsClientFinishedBeginMode(
		pFixture, TEST_TLS_ENCRYPTED_EXTENSIONS_NO_ALPN
	);
}



/* 从等待 Finished 状态推进到 READY，并同步服务端应用 epoch。 */
static xtlssession* testTlsClientReadyFinish(
	test_tls_client_server_hello* pFixture,
	xtlssession* pServer,
	void* pClientTraffic,
	void* pServerTraffic
)
{
	uint8 Finished[64];
	uint8 HandshakeSecret[32];
	uint8 MasterSecret[32];
	uint8 Digest[32];
	xtlstranscript Transcript;
	xtlssessionrecord Record;
	xnetspan Span;
	size_t iFinished;

	Transcript = pFixture->State->Transcript;
	memcpy(
		HandshakeSecret, pFixture->State->HandshakeSecret,
		sizeof(HandshakeSecret)
	);
	iFinished = testTlsClientFinishedMessage(
		&Transcript,
		(xbytesview) {
			pFixture->State->ServerHandshakeTraffic,
			pFixture->State->HashSize
		}, Finished, sizeof(Finished)
	);
	testRequire(__xrtTlsTranscriptUpdate(
		&Transcript, (xbytesview) { Finished, iFinished }
	), "TLS READY fixture server transcript update failed");
	testTlsClientApplicationTraffic(
		(xbytesview) { HandshakeSecret, sizeof(HandshakeSecret) },
		&Transcript, pClientTraffic, pServerTraffic, MasterSecret
	);
	testRequire((testTlsClientServerProtect(
		pFixture, pServer, Finished, iFinished
	) == XTLS_OK) &&
		(xrtTlsSessionState(pFixture->Client) == XTLS_STATE_READY) &&
		(pFixture->State->Step == XTLS_CLIENT_READY),
		"TLS READY fixture did not complete the client handshake");
	while ( xrtTlsSessionSendSize(pFixture->Client) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pFixture->Client, &Span) &&
			(xrtTlsSessionFeed(
				pServer, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pFixture->Client, Span.Size
			), "TLS READY fixture client Finished move failed");
	}
	testRequire((__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK) && Record.Protected &&
		(Record.Type == XTLS_RECORD_HANDSHAKE) &&
		(Record.Data.Size == xrtTlsHandshakeSize(32u)),
		"TLS READY fixture client Finished mismatch");
	testRequire(__xrtTlsTranscriptUpdate(
		&Transcript, Record.Data
	) && __xrtTlsTranscriptDigest(
		&Transcript, Digest, sizeof(Digest)
	) && __xrtTls13DeriveSecret(
		XCRYPTO_HASH_SHA256,
		(xbytesview) { MasterSecret, sizeof(MasterSecret) },
		XRT_STR_LITERAL("res master"),
		(xbytesview) { Digest, sizeof(Digest) },
		pFixture->ResumptionMaster,
		sizeof(pFixture->ResumptionMaster)
	) && (__xrtTlsSessionRecordFinish(
		pServer, false
	) == XTLS_OK), "TLS READY fixture resumption master derivation failed");
	testTlsClientServerApplicationKeys(
		pServer,
		(xbytesview) { (const uint8*)pClientTraffic, 32u },
		(xbytesview) { (const uint8*)pServerTraffic, 32u }
	);
	__xrtTlsTranscriptClear(&Transcript);
	xrtSecureZero(Digest, sizeof(Digest));
	xrtSecureZero(MasterSecret, sizeof(MasterSecret));
	xrtSecureZero(HandshakeSecret, sizeof(HandshakeSecret));
	xrtSecureZero(Finished, sizeof(Finished));
	return pServer;
}



/* 复用指定扩展协商的真实证书握手推进到 READY。 */
static xtlssession* testTlsClientReadyBeginMode(
	test_tls_client_server_hello* pFixture,
	void* pClientTraffic,
	void* pServerTraffic,
	test_tls_encrypted_extensions_mode Mode
)
{
	xtlssession* pServer = testTlsClientFinishedBeginMode(pFixture, Mode);

	return testTlsClientReadyFinish(
		pFixture, pServer, pClientTraffic, pServerTraffic
	);
}



/* 以常用的无 ALPN 路径推进到 READY。 */
static xtlssession* testTlsClientReadyBegin(
	test_tls_client_server_hello* pFixture,
	void* pClientTraffic,
	void* pServerTraffic
)
{
	return testTlsClientReadyBeginMode(
		pFixture, pClientTraffic, pServerTraffic,
		TEST_TLS_ENCRYPTED_EXTENSIONS_NO_ALPN
	);
}



/* 用真实反向密钥保护记录，验证客户端安装的服务端读取方向。 */
static void testTlsClientServerKeys(
	test_tls_client_server_hello* pFixture
)
{
	xtlssession* pServer = testTlsClientServerPeer(pFixture);
	xtlssessionrecord Record;
	xnetspan Span;

	testRequire(__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_HANDSHAKE,
		XRT_BYTES_LITERAL("server-flight"), 3u
	) == XTLS_OK, "TLS server fixture protected record failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				pFixture->Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS server fixture protected record move failed");
	}
	testRequire(__xrtTlsSessionRecordNext(
		pFixture->Client, &Record
	) == XTLS_OK && Record.Protected &&
		(Record.Type == XTLS_RECORD_HANDSHAKE) &&
		(Record.Data.Size == 13u) &&
		(memcmp(Record.Data.Data, "server-flight", 13u) == 0),
		"TLS client installed the wrong server read key");
	testRequire(__xrtTlsSessionRecordFinish(
		pFixture->Client, false
	) == XTLS_OK, "TLS client protected fixture finish failed");
	xrtTlsSessionDestroy(pServer);
}



/* 有效 EE 支持加密记录分片，并稳定发布 ALPN 与 transcript。 */
static void testTlsClientEncryptedExtensionsValid(void)
{
	test_tls_client_server_hello Fixture;
	uint8 Message[256];
	uint8 Expected[32];
	uint8 Actual[32];
	xsha256 Transcript;
	xbytesview Protocol;
	const xerror* pError;
	xtlssession* pServer;
	size_t iMessageSize;
	xtlsresult Result;

	testTlsClientServerFixtureInit(
		&Fixture, TEST_TLS_SERVER_HELLO_VALID
	);
	testRequire((Fixture.State->ProtocolCount == 2u) &&
		(Fixture.State->Protocols[0].Size == 2u) &&
		(memcmp(Fixture.State->Protocols[0].Data, "h2", 2u) == 0),
		"TLS client fixture did not retain its ALPN offer");
	testRequire(testTlsClientServerFeed(
		&Fixture, 0, Fixture.ServerHelloSize
	) == XTLS_OK, "TLS EncryptedExtensions ServerHello failed");
	xrtClearError();
	testRequire(!xrtTlsSessionProtocol(Fixture.Client, &Protocol) &&
		(xrtGetError() == NULL),
		"TLS client published ALPN before EncryptedExtensions");
	pServer = testTlsClientServerPeer(&Fixture);
	iMessageSize = testTlsClientEncryptedExtensions(
		TEST_TLS_ENCRYPTED_EXTENSIONS_VALID,
		Message, sizeof(Message)
	);
	testRequire(testTlsClientServerProtect(
		&Fixture, pServer, Message, 3u
	) == XTLS_OK, "TLS EncryptedExtensions first fragment failed");
	Result = testTlsClientServerProtect(
		&Fixture, pServer, Message + 3u, iMessageSize - 3u
	);
	testRequire(Result == XTLS_OK,
		"TLS EncryptedExtensions final fragment failed");
	testRequire((Fixture.State->Step == XTLS_CLIENT_WAIT_CERTIFICATE) &&
		(Fixture.State->RecordOffset == 0) &&
		(xrtTlsSessionFeedSize(Fixture.Client) == 0) &&
		xrtTlsSessionProtocol(Fixture.Client, &Protocol) &&
		(Protocol.Size == 2u) &&
		(memcmp(Protocol.Data, "h2", 2u) == 0) &&
		(Fixture.Client->ReadKey.Sequence == 2u) &&
		(pServer->WriteKey.Sequence == 2u),
		"TLS EncryptedExtensions commit state mismatch");
	xrtSha256Init(&Transcript);
	testRequire(xrtSha256Update(
		&Transcript, Fixture.ClientHello, Fixture.ClientHelloSize
	) && xrtSha256Update(
		&Transcript, Fixture.ServerHello, Fixture.ServerHelloSize
	) && xrtSha256Update(
		&Transcript, Message, iMessageSize
	) && xrtSha256Final(&Transcript, Expected) &&
		__xrtTlsTranscriptDigest(
			&Fixture.State->Transcript, Actual, sizeof(Actual)
		) && (memcmp(Expected, Actual, sizeof(Expected)) == 0),
		"TLS EncryptedExtensions transcript mismatch");
	xrtClearError();
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_AGAIN) &&
		((xrtTlsSessionWait(Fixture.Client) & XTLS_WAIT_INPUT) != 0),
		"TLS client did not wait for Certificate after EncryptedExtensions");
	pError = xrtGetError();
	testRequire(pError == NULL,
		"TLS client set an error while waiting for Certificate");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* EE 后同记录的下一握手消息必须保留到后续状态处理。 */
static void testTlsClientEncryptedExtensionsCoalesced(void)
{
	test_tls_client_server_hello Fixture;
	uint8 Flight[320];
	uint8 CertificateBody[4] = { 0, 0, 0, 0 };
	xtlssession* pServer;
	size_t iEncryptedExtensions;
	size_t iCertificate;

	testTlsClientServerFixtureInit(
		&Fixture, TEST_TLS_SERVER_HELLO_VALID
	);
	testRequire(testTlsClientServerFeed(
		&Fixture, 0, Fixture.ServerHelloSize
	) == XTLS_OK, "TLS coalesced flight ServerHello failed");
	pServer = testTlsClientServerPeer(&Fixture);
	iEncryptedExtensions = testTlsClientEncryptedExtensions(
		TEST_TLS_ENCRYPTED_EXTENSIONS_NO_ALPN,
		Flight, sizeof(Flight)
	);
	iCertificate = xrtTlsHandshakeSize(sizeof(CertificateBody));
	testRequire((iCertificate != 0) &&
		xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_CERTIFICATE,
			(xbytesview) { CertificateBody, sizeof(CertificateBody) },
			Flight + iEncryptedExtensions,
			sizeof(Flight) - iEncryptedExtensions
		), "TLS coalesced Certificate encoding failed");
	testRequire(testTlsClientServerProtect(
		&Fixture, pServer, Flight,
		iEncryptedExtensions + iCertificate
	) == XTLS_OK, "TLS coalesced server flight failed");
	testRequire((Fixture.State->Step == XTLS_CLIENT_WAIT_CERTIFICATE) &&
		(Fixture.State->RecordOffset == iEncryptedExtensions) &&
		(xrtTlsSessionFeedSize(Fixture.Client) != 0),
		"TLS client lost the coalesced Certificate suffix");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 未请求 ALPN 和未请求扩展必须在 transcript 提交前失败。 */
static void testTlsClientEncryptedExtensionsRejectOne(
	test_tls_encrypted_extensions_mode Mode
)
{
	test_tls_client_server_hello Fixture;
	uint8 Message[256];
	xtlssession* pServer;
	size_t iMessageSize;

	testTlsClientServerFixtureInit(
		&Fixture, TEST_TLS_SERVER_HELLO_VALID
	);
	testRequire(testTlsClientServerFeed(
		&Fixture, 0, Fixture.ServerHelloSize
	) == XTLS_OK, "TLS rejected EE ServerHello failed");
	pServer = testTlsClientServerPeer(&Fixture);
	iMessageSize = testTlsClientEncryptedExtensions(
		Mode, Message, sizeof(Message)
	);
	xrtClearError();
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Message, iMessageSize
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_EXTENSION),
		"TLS client accepted invalid EncryptedExtensions");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* EncryptedExtensions 必须使用握手密钥保护。 */
static void testTlsClientEncryptedExtensionsReject(void)
{
	test_tls_client_server_hello Fixture;
	uint8 Message[256];
	uint8 Record[320];
	size_t iMessageSize;
	size_t iRecordSize;

	testTlsClientEncryptedExtensionsRejectOne(
		TEST_TLS_ENCRYPTED_EXTENSIONS_BAD_ALPN
	);
	testTlsClientEncryptedExtensionsRejectOne(
		TEST_TLS_ENCRYPTED_EXTENSIONS_EARLY_DATA
	);
	testTlsClientEncryptedExtensionsRejectOne(
		TEST_TLS_ENCRYPTED_EXTENSIONS_UNKNOWN
	);
	testTlsClientServerFixtureInit(
		&Fixture, TEST_TLS_SERVER_HELLO_VALID
	);
	testRequire(testTlsClientServerFeed(
		&Fixture, 0, Fixture.ServerHelloSize
	) == XTLS_OK, "TLS plaintext EE ServerHello failed");
	iMessageSize = testTlsClientEncryptedExtensions(
		TEST_TLS_ENCRYPTED_EXTENSIONS_VALID,
		Message, sizeof(Message)
	);
	iRecordSize = xrtTlsRecordSize(iMessageSize);
	xrtClearError();
	testRequire(xrtTlsRecordEncode(
		XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
		(xbytesview) { Message, iMessageSize }, Record, sizeof(Record)
	) && (xrtTlsSessionFeed(
		Fixture.Client, Record, iRecordSize
	) == XTLS_OK) && (xrtTlsClientDrive(
		Fixture.Client
	) == XTLS_ERROR) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_RECORD_TYPE),
		"TLS client accepted plaintext EncryptedExtensions");
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 真实 RSA 证书与 PSS 签名必须推进到 Finished，并保留同记录后缀。 */
static void testTlsClientCertificateVerifyValid(void)
{
	test_tls_client_server_hello Fixture;
	uint8 EncryptedExtensions[256];
	uint8 Certificate[2048];
	uint8 Verify[768];
	uint8 Flight[1024];
	uint8 Finished[64];
	uint8 ClientFinished[64];
	uint8 HandshakeSecret[32];
	uint8 ClientHandshakeTraffic[32];
	uint8 ClientApplicationTraffic[32];
	uint8 ServerApplicationTraffic[32];
	uint8 NextClientApplicationTraffic[32];
	uint8 NextServerApplicationTraffic[32];
	uint8 ClientApplicationKey[16];
	uint8 ClientApplicationIv[12];
	uint8 ServerApplicationKey[16];
	uint8 ServerApplicationIv[12];
	uint8 KeyUpdateRequest[XTLS_HANDSHAKE_HEADER_SIZE + 1u];
	uint8 KeyUpdateResponse[XTLS_HANDSHAKE_HEADER_SIZE + 1u];
	uint8 Application[] = "application-data";
	uint8 Received[sizeof(Application)];
	uint8 Alert[2];
	uint8 Expected[32];
	uint8 Actual[32];
	xtlstranscript ServerTranscript;
	xtlsidentity* pIdentity = testTlsClientIdentity();
	xtlssession* pServer;
	xtlssessionrecord Record;
	xnetspan Span;
	size_t iEncryptedExtensions;
	size_t iCertificate;
	size_t iVerify;
	size_t iFinished;
	size_t iClientFinished;
	size_t iKeyUpdate;
	size_t iWritten;
	size_t iRead;

	memset(&ServerTranscript, 0, sizeof(ServerTranscript));
	pServer = testTlsClientCertificateBegin(
		&Fixture, true, EncryptedExtensions,
		sizeof(EncryptedExtensions), &iEncryptedExtensions
	);
	iCertificate = testTlsClientCertificateMessage(
		(xbytesview) { NULL, 0 }, Certificate, sizeof(Certificate)
	);
	testRequire(testTlsClientServerProtect(
		&Fixture, pServer, Certificate, iCertificate
	) == XTLS_OK &&
		(Fixture.State->Step == XTLS_CLIENT_WAIT_CERTIFICATE_VERIFY) &&
		(Fixture.State->Peer != NULL) &&
		(Fixture.State->Peer->CertificateCount == 1u) &&
		(Fixture.State->Peer->Certificates[0].Raw.Data !=
			X509_LEGACY_RSA_CERT) &&
		(Fixture.State->Peer->Certificates[0].Raw.Size ==
			sizeof(X509_LEGACY_RSA_CERT)) &&
		(memcmp(
			Fixture.State->Peer->Certificates[0].Raw.Data,
			X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
		) == 0), "TLS client certificate snapshot mismatch");
	iVerify = testTlsClientCertificateVerifyMessage(
		&Fixture, pIdentity, XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		false, Verify, sizeof(Verify)
	);
	ServerTranscript = Fixture.State->Transcript;
	testRequire(__xrtTlsTranscriptUpdate(
		&ServerTranscript, (xbytesview) { Verify, iVerify }
	), "TLS client fixture CertificateVerify transcript update failed");
	iFinished = testTlsClientFinishedMessage(
		&ServerTranscript,
		(xbytesview) {
			Fixture.State->ServerHandshakeTraffic,
			Fixture.State->HashSize
		}, Finished, sizeof(Finished)
	);
	testRequire(__xrtTlsTranscriptUpdate(
		&ServerTranscript, (xbytesview) { Finished, iFinished }
	), "TLS client fixture server Finished transcript update failed");
	memcpy(
		HandshakeSecret, Fixture.State->HandshakeSecret,
		sizeof(HandshakeSecret)
	);
	memcpy(
		ClientHandshakeTraffic,
		Fixture.State->ClientHandshakeTraffic,
		sizeof(ClientHandshakeTraffic)
	);
	testTlsClientApplicationTraffic(
		(xbytesview) { HandshakeSecret, sizeof(HandshakeSecret) },
		&ServerTranscript,
		ClientApplicationTraffic, ServerApplicationTraffic, NULL
	);
	iClientFinished = testTlsClientFinishedMessage(
		&ServerTranscript,
		(xbytesview) {
			ClientHandshakeTraffic, sizeof(ClientHandshakeTraffic)
		}, ClientFinished, sizeof(ClientFinished)
	);
	testRequire((iVerify + iFinished <= sizeof(Flight)) &&
		(iFinished <= (sizeof(Flight) - iVerify)),
		"TLS client fixture Finished flight is too large");
	memcpy(Flight, Verify, iVerify);
	memcpy(Flight + iVerify, Finished, iFinished);
	testRequire(testTlsClientServerProtect(
		&Fixture, pServer, Flight, iVerify + iFinished
	) == XTLS_OK &&
		(Fixture.State->Step == XTLS_CLIENT_WAIT_FINISHED) &&
		(Fixture.State->RecordOffset == iVerify) &&
		(xrtTlsSessionFeedSize(Fixture.Client) != 0),
		"TLS client lost the coalesced Finished suffix");
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY) &&
		(Fixture.State->Step == XTLS_CLIENT_READY) &&
		(Fixture.State->RecordOffset == 0) &&
		(xrtTlsSessionFeedSize(Fixture.Client) == 0) &&
		(xrtTlsSessionSendSize(Fixture.Client) != 0) &&
		(Fixture.Client->ReadKey.Sequence == 0) &&
		(Fixture.Client->WriteKey.Sequence == 0),
		"TLS client did not atomically complete Finished");
	testRequire((memcmp(
		Fixture.State->ClientApplicationTraffic,
		ClientApplicationTraffic, sizeof(ClientApplicationTraffic)
	) == 0) && (memcmp(
		Fixture.State->ServerApplicationTraffic,
		ServerApplicationTraffic, sizeof(ServerApplicationTraffic)
	) == 0), "TLS client application traffic secret mismatch");
	for ( size_t i = 0; i < Fixture.State->SecretCapacity; i++ ) {
		testRequire((Fixture.State->HandshakeSecret[i] == 0) &&
			(Fixture.State->ClientHandshakeTraffic[i] == 0) &&
			(Fixture.State->ServerHandshakeTraffic[i] == 0),
			"TLS client retained obsolete handshake secrets");
	}

	/* 服务端必须能用原握手读 epoch 解开客户端 Finished。 */
	while ( xrtTlsSessionSendSize(Fixture.Client) != 0 ) {
		testRequire(xrtTlsSessionSendFront(Fixture.Client, &Span) &&
			(xrtTlsSessionFeed(
				pServer, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				Fixture.Client, Span.Size
			), "TLS client Finished record move failed");
	}
	testRequire((__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK) && Record.Protected &&
		(Record.Type == XTLS_RECORD_HANDSHAKE) &&
		(Record.Data.Size == iClientFinished) &&
		(memcmp(
			Record.Data.Data, ClientFinished, iClientFinished
		) == 0) && (__xrtTlsSessionRecordFinish(
			pServer, false
		) == XTLS_OK), "TLS client Finished record mismatch");

	/* 双方切换应用 epoch 后，各方向都必须能够立即传输。 */
	testTlsClientServerApplicationKeys(
		pServer,
		(xbytesview) {
			ClientApplicationTraffic, sizeof(ClientApplicationTraffic)
		},
		(xbytesview) {
			ServerApplicationTraffic, sizeof(ServerApplicationTraffic)
		}
	);
	testRequire((xrtTlsSessionWrite(
		Fixture.Client, Application, sizeof(Application), &iWritten
	) == XTLS_OK) && (iWritten == sizeof(Application)),
		"TLS client application write failed");
	while ( xrtTlsSessionSendSize(Fixture.Client) != 0 ) {
		testRequire(xrtTlsSessionSendFront(Fixture.Client, &Span) &&
			(xrtTlsSessionFeed(
				pServer, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				Fixture.Client, Span.Size
			), "TLS client application record move failed");
	}
	testRequire((__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK) && Record.Protected &&
		(Record.Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Record.Data.Size == sizeof(Application)) &&
		(memcmp(
			Record.Data.Data, Application, sizeof(Application)
		) == 0) && (__xrtTlsSessionRecordFinish(
			pServer, false
		) == XTLS_OK), "TLS client application epoch mismatch");
	testRequire(__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Application, sizeof(Application) }, 0
	) == XTLS_OK, "TLS server application write failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				Fixture.Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS server application record move failed");
	}
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(xrtTlsSessionPlainSize(Fixture.Client) == sizeof(Application)) &&
		(xrtTlsSessionRead(
			Fixture.Client, Received, sizeof(Received), &iRead
		) == XTLS_OK) && (iRead == sizeof(Application)) &&
		(memcmp(Received, Application, sizeof(Application)) == 0),
		"TLS client public application receive path mismatch");

	testRequire(__xrtTlsTranscriptUpdate(
		&ServerTranscript,
		(xbytesview) { ClientFinished, iClientFinished }
	) && __xrtTlsTranscriptDigest(
		&ServerTranscript, Expected, sizeof(Expected)
	) &&
		__xrtTlsTranscriptDigest(
			&Fixture.State->Transcript, Actual, sizeof(Actual)
		) && (memcmp(Expected, Actual, sizeof(Expected)) == 0),
		"TLS client Finished transcript mismatch");

	/* 请求型 KeyUpdate 必须用旧写 epoch 应答，再同时提交新收发 epoch。 */
	testTlsClientTrafficUpdate(
		(xbytesview) {
			ClientApplicationTraffic, sizeof(ClientApplicationTraffic)
		}, NextClientApplicationTraffic
	);
	testTlsClientTrafficUpdate(
		(xbytesview) {
			ServerApplicationTraffic, sizeof(ServerApplicationTraffic)
		}, NextServerApplicationTraffic
	);
	iKeyUpdate = testTlsClientKeyUpdateMessage(
		XTLS_KEY_UPDATE_REQUESTED,
		KeyUpdateRequest, sizeof(KeyUpdateRequest)
	);
	testRequire((iKeyUpdate == sizeof(KeyUpdateRequest)) &&
		(testTlsClientServerProtect(
			&Fixture, pServer, KeyUpdateRequest, iKeyUpdate
		) == XTLS_OK) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY) &&
		(Fixture.Client->ReadKey.Sequence == 0) &&
		(Fixture.Client->WriteKey.Sequence == 0) &&
		(xrtTlsSessionSendSize(Fixture.Client) != 0) &&
		(memcmp(
			Fixture.State->ClientApplicationTraffic,
			NextClientApplicationTraffic,
			sizeof(NextClientApplicationTraffic)
		) == 0) && (memcmp(
			Fixture.State->ServerApplicationTraffic,
			NextServerApplicationTraffic,
			sizeof(NextServerApplicationTraffic)
		) == 0), "TLS client KeyUpdate commit mismatch");

	/* 服务端先保留旧读 epoch 解开应答，再分别切换到下一代收发密钥。 */
	testTlsClientRecordMaterial(
		(xbytesview) {
			NextServerApplicationTraffic,
			sizeof(NextServerApplicationTraffic)
		}, ServerApplicationKey, ServerApplicationIv
	);
	testRequire(__xrtTlsSessionWriteKey(
		pServer, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) {
			ServerApplicationKey, sizeof(ServerApplicationKey)
		}, (xbytesview) {
			ServerApplicationIv, sizeof(ServerApplicationIv)
		}
	), "TLS server KeyUpdate write epoch installation failed");
	while ( xrtTlsSessionSendSize(Fixture.Client) != 0 ) {
		testRequire(xrtTlsSessionSendFront(Fixture.Client, &Span) &&
			(xrtTlsSessionFeed(
				pServer, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				Fixture.Client, Span.Size
			), "TLS client KeyUpdate response move failed");
	}
	testRequire((testTlsClientKeyUpdateMessage(
		XTLS_KEY_UPDATE_NOT_REQUESTED,
		KeyUpdateResponse, sizeof(KeyUpdateResponse)
	) == sizeof(KeyUpdateResponse)) && (__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK) && Record.Protected &&
		(Record.Type == XTLS_RECORD_HANDSHAKE) &&
		(Record.Data.Size == sizeof(KeyUpdateResponse)) &&
		(memcmp(
			Record.Data.Data, KeyUpdateResponse,
			sizeof(KeyUpdateResponse)
		) == 0) && (__xrtTlsSessionRecordFinish(
		pServer, false
	) == XTLS_OK), "TLS client KeyUpdate response epoch mismatch");
	testTlsClientRecordMaterial(
		(xbytesview) {
			NextClientApplicationTraffic,
			sizeof(NextClientApplicationTraffic)
		}, ClientApplicationKey, ClientApplicationIv
	);
	testRequire(__xrtTlsSessionReadKey(
		pServer, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) {
			ClientApplicationKey, sizeof(ClientApplicationKey)
		}, (xbytesview) {
			ClientApplicationIv, sizeof(ClientApplicationIv)
		}
	), "TLS server KeyUpdate read epoch installation failed");

	/* 更新后的两个方向必须从序列号零开始继续传输应用记录。 */
	testRequire((xrtTlsSessionWrite(
		Fixture.Client, Application, sizeof(Application), &iWritten
	) == XTLS_OK) && (iWritten == sizeof(Application)),
		"TLS client post-KeyUpdate application write failed");
	while ( xrtTlsSessionSendSize(Fixture.Client) != 0 ) {
		testRequire(xrtTlsSessionSendFront(Fixture.Client, &Span) &&
			(xrtTlsSessionFeed(
				pServer, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				Fixture.Client, Span.Size
			), "TLS post-KeyUpdate client record move failed");
	}
	testRequire((__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK) && Record.Protected &&
		(Record.Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Record.Data.Size == sizeof(Application)) &&
		(memcmp(
			Record.Data.Data, Application, sizeof(Application)
		) == 0) && (__xrtTlsSessionRecordFinish(
		pServer, false
	) == XTLS_OK), "TLS client post-KeyUpdate epoch mismatch");
	testRequire(__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Application, sizeof(Application) }, 0
	) == XTLS_OK, "TLS server post-KeyUpdate application write failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				Fixture.Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS post-KeyUpdate server record move failed");
	}
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(xrtTlsSessionRead(
			Fixture.Client, Received, sizeof(Received), &iRead
		) == XTLS_OK) && (iRead == sizeof(Application)) &&
		(memcmp(Received, Application, sizeof(Application)) == 0),
		"TLS client post-KeyUpdate public receive mismatch");

	/* 受保护关闭必须自动应答，并在响应排空后进入认证关闭终态。 */
	testRequire(xrtTlsAlertEncode(
		XTLS_ALERT_WARNING, XTLS_ALERT_CLOSE_NOTIFY,
		Alert, sizeof(Alert)
	) && (__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_ALERT,
		(xbytesview) { Alert, sizeof(Alert) }, 0
	) == XTLS_OK), "TLS server close_notify write failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				Fixture.Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS server close_notify record move failed");
	}
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_CLOSING) &&
		(xrtTlsSessionSendSize(Fixture.Client) != 0),
		"TLS client did not enter authenticated closing state");
	while ( xrtTlsSessionSendSize(Fixture.Client) != 0 ) {
		testRequire(xrtTlsSessionSendFront(Fixture.Client, &Span) &&
			(xrtTlsSessionFeed(
				pServer, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				Fixture.Client, Span.Size
			), "TLS client close_notify response move failed");
	}
	testRequire((__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK) && Record.Protected &&
		(Record.Type == XTLS_RECORD_ALERT) &&
		(Record.Data.Size == sizeof(Alert)) &&
		(memcmp(Record.Data.Data, Alert, sizeof(Alert)) == 0) &&
		(__xrtTlsSessionRecordFinish(
			pServer, false
		) == XTLS_OK) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_CLOSED) &&
		(xrtTlsClientDrive(Fixture.Client) == XTLS_CLOSED) &&
		(xrtTlsSessionRead(
			Fixture.Client, Received, sizeof(Received), &iRead
		) == XTLS_CLOSED) && (iRead == 0),
		"TLS client close_notify response or terminal read mismatch");
	__xrtTlsTranscriptClear(&ServerTranscript);
	xrtSecureZero(ServerApplicationIv, sizeof(ServerApplicationIv));
	xrtSecureZero(ServerApplicationKey, sizeof(ServerApplicationKey));
	xrtSecureZero(ClientApplicationIv, sizeof(ClientApplicationIv));
	xrtSecureZero(ClientApplicationKey, sizeof(ClientApplicationKey));
	xrtSecureZero(
		NextServerApplicationTraffic,
		sizeof(NextServerApplicationTraffic)
	);
	xrtSecureZero(
		NextClientApplicationTraffic,
		sizeof(NextClientApplicationTraffic)
	);
	xrtSecureZero(ServerApplicationTraffic, sizeof(ServerApplicationTraffic));
	xrtSecureZero(ClientApplicationTraffic, sizeof(ClientApplicationTraffic));
	xrtSecureZero(ClientHandshakeTraffic, sizeof(ClientHandshakeTraffic));
	xrtSecureZero(HandshakeSecret, sizeof(HandshakeSecret));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
	xrtTlsIdentityRelease(pIdentity);
}



/* 不请求应答的 KeyUpdate 只推进服务端到客户端方向。 */
static void testTlsClientKeyUpdateNotRequested(void)
{
	test_tls_client_server_hello Fixture;
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 NextServerTraffic[32];
	uint8 ServerKey[16];
	uint8 ServerIv[12];
	uint8 Message[XTLS_HANDSHAKE_HEADER_SIZE + 1u];
	uint8 Application[] = "key-update-read";
	uint8 Received[sizeof(Application)];
	xtlssession* pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	xnetspan Span;
	size_t iMessage;
	size_t iRead;

	testTlsClientTrafficUpdate(
		(xbytesview) { ServerTraffic, sizeof(ServerTraffic) },
		NextServerTraffic
	);
	iMessage = testTlsClientKeyUpdateMessage(
		XTLS_KEY_UPDATE_NOT_REQUESTED, Message, sizeof(Message)
	);
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Message, iMessage
	) == XTLS_OK) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY) &&
		(xrtTlsSessionSendSize(Fixture.Client) == 0) &&
		(Fixture.Client->ReadKey.Sequence == 0) &&
		(Fixture.Client->WriteKey.Sequence == 0) &&
		(memcmp(
			Fixture.State->ClientApplicationTraffic,
			ClientTraffic, sizeof(ClientTraffic)
		) == 0) && (memcmp(
			Fixture.State->ServerApplicationTraffic,
			NextServerTraffic, sizeof(NextServerTraffic)
		) == 0), "TLS client not-requested KeyUpdate commit mismatch");
	testTlsClientRecordMaterial(
		(xbytesview) {
			NextServerTraffic, sizeof(NextServerTraffic)
		}, ServerKey, ServerIv
	);
	testRequire(__xrtTlsSessionWriteKey(
		pServer, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { ServerKey, sizeof(ServerKey) },
		(xbytesview) { ServerIv, sizeof(ServerIv) }
	) && (__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Application, sizeof(Application) }, 0
	) == XTLS_OK), "TLS server post-update read test setup failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				Fixture.Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS server post-update read record move failed");
	}
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(xrtTlsSessionRead(
			Fixture.Client, Received, sizeof(Received), &iRead
		) == XTLS_OK) && (iRead == sizeof(Application)) &&
		(memcmp(Received, Application, sizeof(Application)) == 0),
		"TLS client not-requested KeyUpdate read epoch mismatch");
	xrtSecureZero(ServerIv, sizeof(ServerIv));
	xrtSecureZero(ServerKey, sizeof(ServerKey));
	xrtSecureZero(NextServerTraffic, sizeof(NextServerTraffic));
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 主动 KeyUpdate 必须先用旧写 epoch 发消息，再使用新 epoch 发应用数据。 */
static void testTlsClientKeyUpdateSend(void)
{
	test_tls_client_server_hello Fixture;
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 NextClientTraffic[32];
	uint8 NextServerTraffic[32];
	uint8 ClientKey[16];
	uint8 ClientIv[12];
	uint8 Message[XTLS_HANDSHAKE_HEADER_SIZE + 1u];
	uint8 Application[] = "key-update-write";
	xtlssession* pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	xtlssessionrecord Record;
	xnetspan Span;
	size_t iMessage;
	size_t iWritten;

	testTlsClientTrafficUpdate(
		(xbytesview) { ClientTraffic, sizeof(ClientTraffic) },
		NextClientTraffic
	);
	testRequire((xrtTlsClientKeyUpdate(
		Fixture.Client, XTLS_KEY_UPDATE_REQUESTED
	) == XTLS_OK) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY) &&
		(Fixture.Client->ReadKey.Sequence == 0) &&
		(Fixture.Client->WriteKey.Sequence == 0) &&
		(xrtTlsSessionSendSize(Fixture.Client) != 0) &&
		(memcmp(
			Fixture.State->ClientApplicationTraffic,
			NextClientTraffic, sizeof(NextClientTraffic)
		) == 0) && (memcmp(
			Fixture.State->ServerApplicationTraffic,
			ServerTraffic, sizeof(ServerTraffic)
		) == 0), "TLS client active KeyUpdate commit mismatch");
	while ( xrtTlsSessionSendSize(Fixture.Client) != 0 ) {
		testRequire(xrtTlsSessionSendFront(Fixture.Client, &Span) &&
			(xrtTlsSessionFeed(
				pServer, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				Fixture.Client, Span.Size
			), "TLS client active KeyUpdate record move failed");
	}
	iMessage = testTlsClientKeyUpdateMessage(
		XTLS_KEY_UPDATE_REQUESTED, Message, sizeof(Message)
	);
	testRequire((__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK) && Record.Protected &&
		(Record.Type == XTLS_RECORD_HANDSHAKE) &&
		(Record.Data.Size == iMessage) &&
		(memcmp(Record.Data.Data, Message, iMessage) == 0) &&
		(__xrtTlsSessionRecordFinish(
			pServer, false
		) == XTLS_OK), "TLS client active KeyUpdate old epoch mismatch");
	testTlsClientRecordMaterial(
		(xbytesview) {
			NextClientTraffic, sizeof(NextClientTraffic)
		}, ClientKey, ClientIv
	);
	testRequire(__xrtTlsSessionReadKey(
		pServer, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { ClientKey, sizeof(ClientKey) },
		(xbytesview) { ClientIv, sizeof(ClientIv) }
	) && (xrtTlsSessionWrite(
		Fixture.Client, Application, sizeof(Application), &iWritten
	) == XTLS_OK) && (iWritten == sizeof(Application)),
		"TLS client active KeyUpdate application setup failed");
	while ( xrtTlsSessionSendSize(Fixture.Client) != 0 ) {
		testRequire(xrtTlsSessionSendFront(Fixture.Client, &Span) &&
			(xrtTlsSessionFeed(
				pServer, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				Fixture.Client, Span.Size
			), "TLS client active KeyUpdate application move failed");
	}
	testRequire((__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK) && Record.Protected &&
		(Record.Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Record.Data.Size == sizeof(Application)) &&
		(memcmp(
			Record.Data.Data, Application, sizeof(Application)
		) == 0) && (__xrtTlsSessionRecordFinish(
		pServer, false
	) == XTLS_OK), "TLS client active KeyUpdate new epoch mismatch");

	/* 服务端应答仍使用原服务端写 epoch，客户端随后只更新读取方向。 */
	testTlsClientTrafficUpdate(
		(xbytesview) { ServerTraffic, sizeof(ServerTraffic) },
		NextServerTraffic
	);
	iMessage = testTlsClientKeyUpdateMessage(
		XTLS_KEY_UPDATE_NOT_REQUESTED, Message, sizeof(Message)
	);
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Message, iMessage
	) == XTLS_OK) &&
		(xrtTlsSessionSendSize(Fixture.Client) == 0) &&
		(memcmp(
			Fixture.State->ServerApplicationTraffic,
			NextServerTraffic, sizeof(NextServerTraffic)
		) == 0), "TLS client active KeyUpdate response mismatch");
	xrtSecureZero(ClientIv, sizeof(ClientIv));
	xrtSecureZero(ClientKey, sizeof(ClientKey));
	xrtSecureZero(NextServerTraffic, sizeof(NextServerTraffic));
	xrtSecureZero(NextClientTraffic, sizeof(NextClientTraffic));
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 主动 KeyUpdate 的参数错误与输出背压不得修改客户端写 epoch。 */
static void testTlsClientKeyUpdateSendBackpressure(void)
{
	test_tls_client_server_hello Fixture;
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 NextClientTraffic[32];
	xtlssession* pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	const xtlslimits* pLimits = xrtTlsContextLimits(Fixture.Context);
	bytes pFill;
	size_t iMessage = xrtTlsHandshakeSize(1u);
	size_t iRecord = __xrtTlsRecordSealSize(
		&Fixture.Client->WriteKey, iMessage, 0
	);
	size_t iFill;

	testTlsClientTrafficUpdate(
		(xbytesview) { ClientTraffic, sizeof(ClientTraffic) },
		NextClientTraffic
	);
	xrtClearError();
	testRequire((xrtTlsClientKeyUpdate(
		Fixture.Client, (xtlskeyupdate)2
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY) &&
		(xrtTlsSessionSendSize(Fixture.Client) == 0) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_HANDSHAKE) &&
		(memcmp(
			Fixture.State->ClientApplicationTraffic,
			ClientTraffic, sizeof(ClientTraffic)
		) == 0), "TLS client active KeyUpdate accepted an invalid request");
	testRequire((pLimits != NULL) && (iMessage != 0) &&
		(iRecord != 0) && (iRecord <= pLimits->SendLimit),
		"TLS active KeyUpdate backpressure limit is invalid");
	iFill = pLimits->SendLimit - iRecord + 1u;
	pFill = (bytes)xrtMalloc(iFill);
	testRequire(pFill != NULL,
		"TLS active KeyUpdate filler allocation failed");
	memset(pFill, 0xC3, iFill);
	testRequire(__xrtTlsSessionSend(
		Fixture.Client, pFill, iFill
	) == XTLS_OK, "TLS active KeyUpdate backpressure setup failed");
	xrtFree(pFill);
	testRequire((xrtTlsClientKeyUpdate(
		Fixture.Client, XTLS_KEY_UPDATE_REQUESTED
	) == XTLS_AGAIN) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY) &&
		(xrtTlsSessionSendSize(Fixture.Client) == iFill) &&
		(Fixture.Client->WriteKey.Sequence == 0) &&
		((xrtTlsSessionWait(Fixture.Client) & XTLS_WAIT_OUTPUT) != 0) &&
		(memcmp(
			Fixture.State->ClientApplicationTraffic,
			ClientTraffic, sizeof(ClientTraffic)
		) == 0), "TLS active KeyUpdate backpressure changed the write epoch");
	testRequire(xrtTlsSessionSendConsume(Fixture.Client, iFill) &&
		(xrtTlsClientKeyUpdate(
			Fixture.Client, XTLS_KEY_UPDATE_REQUESTED
		) == XTLS_OK) &&
		(xrtTlsSessionSendSize(Fixture.Client) == iRecord) &&
		(Fixture.Client->WriteKey.Sequence == 0) &&
		(memcmp(
			Fixture.State->ClientApplicationTraffic,
			NextClientTraffic, sizeof(NextClientTraffic)
		) == 0), "TLS active KeyUpdate did not recover after output drain");
	xrtSecureZero(NextClientTraffic, sizeof(NextClientTraffic));
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
	xrtClearError();
}



/* 强制 KeyUpdate 应答遇到发送硬上限时必须无损暂停并可重试。 */
static void testTlsClientKeyUpdateBackpressure(void)
{
	test_tls_client_server_hello Fixture;
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 NextClientTraffic[32];
	uint8 NextServerTraffic[32];
	uint8 Message[XTLS_HANDSHAKE_HEADER_SIZE + 1u];
	xtlssession* pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	const xtlslimits* pLimits = xrtTlsContextLimits(Fixture.Context);
	bytes pFill;
	size_t iMessage;
	size_t iRecord;
	size_t iFill;

	testTlsClientTrafficUpdate(
		(xbytesview) { ClientTraffic, sizeof(ClientTraffic) },
		NextClientTraffic
	);
	testTlsClientTrafficUpdate(
		(xbytesview) { ServerTraffic, sizeof(ServerTraffic) },
		NextServerTraffic
	);
	iMessage = testTlsClientKeyUpdateMessage(
		XTLS_KEY_UPDATE_REQUESTED, Message, sizeof(Message)
	);
	iRecord = __xrtTlsRecordSealSize(
		&Fixture.Client->WriteKey, iMessage, 0
	);
	testRequire((pLimits != NULL) && (iRecord != 0) &&
		(iRecord <= pLimits->SendLimit),
		"TLS KeyUpdate backpressure limit is invalid");
	iFill = pLimits->SendLimit - iRecord + 1u;
	pFill = (bytes)xrtMalloc(iFill);
	testRequire(pFill != NULL,
		"TLS KeyUpdate backpressure filler allocation failed");
	memset(pFill, 0x5A, iFill);
	testRequire(__xrtTlsSessionSend(
		Fixture.Client, pFill, iFill
	) == XTLS_OK, "TLS KeyUpdate backpressure setup failed");
	xrtFree(pFill);
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Message, iMessage
	) == XTLS_AGAIN) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY) &&
		(Fixture.State->RecordOffset == 0) &&
		(Fixture.State->Reader.Size == 0) &&
		(Fixture.State->Reader.HeaderSize == 0) &&
		(xrtTlsSessionSendSize(Fixture.Client) == iFill) &&
		((xrtTlsSessionWait(Fixture.Client) & XTLS_WAIT_OUTPUT) != 0) &&
		(Fixture.Client->ReadKey.Sequence == 1u) &&
		(Fixture.Client->WriteKey.Sequence == 0) &&
		(memcmp(
			Fixture.State->ClientApplicationTraffic,
			ClientTraffic, sizeof(ClientTraffic)
		) == 0) && (memcmp(
			Fixture.State->ServerApplicationTraffic,
			ServerTraffic, sizeof(ServerTraffic)
		) == 0), "TLS KeyUpdate backpressure changed the active epoch");
	testRequire(xrtTlsSessionSendConsume(Fixture.Client, iFill) &&
		(xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(Fixture.Client->ReadKey.Sequence == 0) &&
		(Fixture.Client->WriteKey.Sequence == 0) &&
		(xrtTlsSessionSendSize(Fixture.Client) == iRecord) &&
		(memcmp(
			Fixture.State->ClientApplicationTraffic,
			NextClientTraffic, sizeof(NextClientTraffic)
		) == 0) && (memcmp(
			Fixture.State->ServerApplicationTraffic,
			NextServerTraffic, sizeof(NextServerTraffic)
		) == 0), "TLS KeyUpdate did not recover after output drain");
	xrtSecureZero(NextServerTraffic, sizeof(NextServerTraffic));
	xrtSecureZero(NextClientTraffic, sizeof(NextClientTraffic));
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 非法、跨记录或同记录拼接的 KeyUpdate 必须进入协议失败终态。 */
static void testTlsClientKeyUpdateReject(void)
{
	test_tls_client_server_hello Fixture;
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 Message[XTLS_HANDSHAKE_HEADER_SIZE + 1u];
	uint8 Flight[(XTLS_HANDSHAKE_HEADER_SIZE + 1u) * 2u];
	xtlssession* pServer;
	size_t iMessage;

	/* 枚举值只能是 update_not_requested 或 update_requested。 */
	pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	iMessage = testTlsClientKeyUpdateMessage(
		XTLS_KEY_UPDATE_NOT_REQUESTED, Message, sizeof(Message)
	);
	Message[iMessage - 1u] = 2u;
	xrtClearError();
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Message, iMessage
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_HANDSHAKE),
		"TLS client accepted an invalid KeyUpdate request value");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);

	/* KeyUpdate 不允许跨越记录边界。 */
	pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	iMessage = testTlsClientKeyUpdateMessage(
		XTLS_KEY_UPDATE_NOT_REQUESTED, Message, sizeof(Message)
	);
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Message, 3u
	) == XTLS_OK) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY),
		"TLS client rejected an incomplete KeyUpdate before its type was known");
	xrtClearError();
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Message + 3u, iMessage - 3u
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_HANDSHAKE),
		"TLS client accepted a fragmented KeyUpdate");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);

	/* KeyUpdate 必须独占其记录，不能与另一条握手消息拼接。 */
	pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	iMessage = testTlsClientKeyUpdateMessage(
		XTLS_KEY_UPDATE_NOT_REQUESTED, Message, sizeof(Message)
	);
	memcpy(Flight, Message, iMessage);
	memcpy(Flight + iMessage, Message, iMessage);
	xrtClearError();
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Flight, iMessage * 2u
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_HANDSHAKE),
		"TLS client accepted a coalesced KeyUpdate");
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
	xrtClearError();
}



/* NewSessionTicket 支持分片和聚合，畸形正文必须由客户端状态机拒绝。 */
static void testTlsClientSessionTicketDrive(void)
{
	test_tls_client_server_hello Fixture;
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 Message[128];
	uint8 Flight[256];
	uint8 BadBody[1] = { 0 };
	uint8 Application[] = "after-ticket";
	uint8 Received[sizeof(Application)];
	uint8 ExpectedSecret[32];
	uint8 ExpectedPeer[32];
	uint8 Nonce[2] = { 1, 2 };
	xtlssession* pServer = testTlsClientReadyBeginMode(
		&Fixture, ClientTraffic, ServerTraffic,
		TEST_TLS_ENCRYPTED_EXTENSIONS_VALID
	);
	xtlsresumeinfo Info;
	xtlsresume* pResume;
	xnetspan Span;
	size_t iMessage = testTlsClientSessionTicketMessage(
		Message, sizeof(Message)
	);
	size_t iBad;
	size_t iRead;

	/* 普通后握手消息继续使用共享 Reader，允许跨记录重组。 */
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Message, 3u
	) == XTLS_OK) &&
		(Fixture.State->Reader.HeaderSize == 3u) &&
		(Fixture.State->RecordOffset == 0) &&
		(testTlsClientServerProtect(
			&Fixture, pServer, Message + 3u, iMessage - 3u
		) == XTLS_OK) &&
		(Fixture.State->Reader.HeaderSize == 0) &&
		(Fixture.State->Reader.Size == 0) &&
		(Fixture.State->RecordOffset == 0) &&
		(xrtTlsClientResumeCount(Fixture.Client) == 1u),
		"TLS client did not reassemble a fragmented NewSessionTicket");

	/* 公平性预算只处理一条消息，下一次驱动继续消费同记录后缀。 */
	testRequire((iMessage * 2u) <= sizeof(Flight),
		"TLS ticket coalesced flight is too large");
	memcpy(Flight, Message, iMessage);
	memcpy(Flight + iMessage, Message, iMessage);
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Flight, iMessage * 2u
	) == XTLS_OK) &&
		(Fixture.State->RecordOffset == iMessage) &&
		(xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(Fixture.State->RecordOffset == 0) &&
		(xrtTlsSessionFeedSize(Fixture.Client) == 0) &&
		(xrtTlsClientResumeCount(Fixture.Client) == 3u),
		"TLS client lost a coalesced NewSessionTicket suffix");

	/* 第二组聚合票据超过默认容量，必须淘汰最旧对象而不是无界增长。 */
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Flight, iMessage * 2u
	) == XTLS_OK) &&
		(Fixture.State->RecordOffset == iMessage) &&
		(xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(xrtTlsClientResumeCount(Fixture.Client) ==
			XTLS_CLIENT_RESUME_LIMIT_DEFAULT) &&
		(xrtTlsClientResumeDropped(Fixture.Client) == 1u),
		"TLS client resume queue did not enforce its hard bound");

	/* 独立复算票据 PSK 和证书身份，再逐张验证深拷贝对象。 */
	testRequire(__xrtTls13ExpandLabel(
		XCRYPTO_HASH_SHA256,
		(xbytesview) {
			Fixture.ResumptionMaster,
			sizeof(Fixture.ResumptionMaster)
		}, XRT_STR_LITERAL("resumption"),
		(xbytesview) { Nonce, sizeof(Nonce) },
		ExpectedSecret, sizeof(ExpectedSecret)
	) && xrtSha256(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), ExpectedPeer
	), "TLS client resume expected material derivation failed");
	for ( size_t i = 0; i < XTLS_CLIENT_RESUME_LIMIT_DEFAULT; i++ ) {
		pResume = xrtTlsClientTakeResume(Fixture.Client);
		testRequire((pResume != NULL) && xrtTlsResumeInfo(pResume, &Info),
			"TLS client did not transfer an owned resume object");
		testRequire((Info.Version == XTLS_VERSION_13) &&
			(Info.Cipher == XTLS_AES_128_GCM_SHA256) &&
			(Info.Lifetime == 3600u) &&
			(Info.AgeAdd == UINT32_C(0x01020304)) &&
			(Info.MaxEarlyData == 4096u) &&
			(Info.ExpiresAt - Info.IssuedAt == INT64_C(3600000000)) &&
			xrtTlsResumeValidAt(pResume, Info.IssuedAt),
			"TLS client exported incorrect resume metadata");
		testRequire(
			(Info.Ticket.Size == 4u) &&
			(memcmp(Info.Ticket.Data, "\x03\x04\x05\x06", 4u) == 0) &&
			(Info.ServerName.Size == 11u) &&
			(memcmp(Info.ServerName.Data, "example.com", 11u) == 0) &&
			(Info.Protocol.Size == 2u) &&
			(memcmp(Info.Protocol.Data, "h2", 2u) == 0),
			"TLS client exported incorrect resume routing scope");
		testRequire((Info.Secret.Size == sizeof(ExpectedSecret)) &&
			(memcmp(
				Info.Secret.Data, ExpectedSecret, sizeof(ExpectedSecret)
			) == 0), "TLS client exported an incorrect ticket PSK");
		testRequire(
			(Info.PeerIdentity.Size == sizeof(ExpectedPeer)) &&
			(memcmp(
				Info.PeerIdentity.Data, ExpectedPeer, sizeof(ExpectedPeer)
			) == 0), "TLS client exported an incorrect peer identity");
		xrtTlsResumeRelease(pResume);
	}
	xrtClearError();
	testRequire((xrtTlsClientResumeCount(Fixture.Client) == 0) &&
		(xrtTlsClientTakeResume(Fixture.Client) == NULL) &&
		(xrtGetError() == NULL),
		"TLS client empty resume queue changed error state");

	/* 票据处理后 Reader 必须不影响后续应用记录。 */
	testRequire(__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Application, sizeof(Application) }, 0
	) == XTLS_OK, "TLS post-ticket application setup failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				Fixture.Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS post-ticket application move failed");
	}
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(xrtTlsSessionRead(
			Fixture.Client, Received, sizeof(Received), &iRead
		) == XTLS_OK) && (iRead == sizeof(Application)) &&
		(memcmp(Received, Application, sizeof(Application)) == 0),
		"TLS post-ticket application path mismatch");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);

	/* 握手封包完整不代表票据正文合法，解析失败必须进入终态。 */
	pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	iBad = xrtTlsHandshakeSize(sizeof(BadBody));
	testRequire((iBad <= sizeof(Message)) && xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_NEW_SESSION_TICKET,
		(xbytesview) { BadBody, sizeof(BadBody) },
		Message, sizeof(Message)
	), "TLS malformed ticket fixture encoding failed");
	xrtClearError();
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Message, iBad
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_RESUME),
		"TLS client accepted a malformed NewSessionTicket");
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtSecureZero(ExpectedPeer, sizeof(ExpectedPeer));
	xrtSecureZero(ExpectedSecret, sizeof(ExpectedSecret));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
	xrtClearError();
}



/* 可选票据对象 OOM 只计入丢弃统计，不能破坏已就绪连接。 */
static void testTlsClientSessionTicketOom(void)
{
	test_tls_client_server_hello Fixture;
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 Message[1400];
	uint8 Ticket[1200];
	uint8 Secret[32] = { 0 };
	uint8 PeerIdentity[32] = { 0 };
	xtlssession* pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	xtlsresumeconfig ProbeConfig;
	xtlsresume* pProbe;
	xnetspan Span;
	size_t iMessage;
	size_t iResumeSize;
	xtlsresult Result;

	memset(Ticket, 0xA5, sizeof(Ticket));
	iMessage = testTlsClientSessionTicketMessageData(
		Message, sizeof(Message),
		(xbytesview) { Ticket, sizeof(Ticket) }
	);

	/* 先用同形对象取得连续恢复对象的精确申请尺寸。 */
	xrtTlsResumeConfigInit(&ProbeConfig);
	ProbeConfig.Cipher = XTLS_AES_128_GCM_SHA256;
	ProbeConfig.Ticket = (xbytesview) { Ticket, sizeof(Ticket) };
	ProbeConfig.Secret = (xbytesview) { Secret, sizeof(Secret) };
	ProbeConfig.ServerName = XRT_STR_LITERAL("example.com");
	ProbeConfig.PeerIdentity = (xbytesview) {
		PeerIdentity, sizeof(PeerIdentity)
	};
	ProbeConfig.Lifetime = 3600u;
	pProbe = xrtTlsResumeCreate(&ProbeConfig);
	testRequire(pProbe != NULL,
		"TLS ticket OOM probe resume creation failed");
	iResumeSize = TestTlsClientAlloc.LastSize;
	xrtTlsResumeRelease(pProbe);

	testRequire(__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Message, iMessage }, 0
	) == XTLS_OK, "TLS ticket OOM record protection failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				Fixture.Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS ticket OOM record move failed");
	}
	xrtClearError();
	TestTlsClientAlloc.FailSize = iResumeSize;
	TestTlsClientAlloc.FailSizeOnce = true;
	TestTlsClientAlloc.Failed = false;
	Result = xrtTlsClientDrive(Fixture.Client);
	testRequire(TestTlsClientAlloc.Failed,
		"TLS ticket cache OOM did not reach the resume allocation");
	testRequire(Result == XTLS_OK,
		"TLS ticket cache OOM failed the client drive");
	testRequire(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY,
		"TLS ticket cache OOM changed the ready session state");
	testRequire(xrtTlsClientResumeCount(Fixture.Client) == 0,
		"TLS ticket cache OOM retained a resume object");
	testRequire(xrtTlsClientResumeDropped(Fixture.Client) == 1u,
		"TLS ticket cache OOM did not increment the drop counter");
	testRequire(xrtGetError() == NULL,
		"TLS ticket cache OOM leaked a benign thread error");
	TestTlsClientAlloc.FailSize = SIZE_MAX;
	TestTlsClientAlloc.FailSizeOnce = false;
	TestTlsClientAlloc.Failed = false;
	xrtSecureZero(PeerIdentity, sizeof(PeerIdentity));
	xrtSecureZero(Secret, sizeof(Secret));
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* READY 应用输入遇到 PlainLimit 时必须保留挂起记录并等待应用消费。 */
static void testTlsClientApplicationBackpressure(void)
{
	test_tls_client_server_hello Fixture;
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 Application[] = "plain-backpressure";
	uint8 Received[sizeof(Application)];
	xtlssession* pServer = testTlsClientReadyBegin(
		&Fixture, ClientTraffic, ServerTraffic
	);
	const xtlslimits* pLimits = xrtTlsContextLimits(Fixture.Context);
	xnetspan Span;
	bytes pFill;
	size_t iFeed;
	size_t iRead;

	testRequire(pLimits != NULL,
		"TLS application backpressure limits are missing");
	pFill = (bytes)xrtMalloc(pLimits->PlainLimit);
	testRequire(pFill != NULL,
		"TLS application backpressure filler allocation failed");
	memset(pFill, 0x69, pLimits->PlainLimit);
	testRequire(__xrtTlsSessionPlain(
		Fixture.Client, pFill, pLimits->PlainLimit
	) == XTLS_OK, "TLS application backpressure queue fill failed");
	xrtFree(pFill);
	testRequire(__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Application, sizeof(Application) }, 0
	) == XTLS_OK, "TLS blocked application record setup failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				Fixture.Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS blocked application record move failed");
	}
	iFeed = xrtTlsSessionFeedSize(Fixture.Client);
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_AGAIN) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY) &&
		((xrtTlsSessionWait(Fixture.Client) & XTLS_WAIT_APPLICATION) != 0) &&
		(xrtTlsSessionPlainSize(Fixture.Client) == pLimits->PlainLimit) &&
		(xrtTlsSessionFeedSize(Fixture.Client) == iFeed) &&
		Fixture.Client->RecordPending &&
		(Fixture.Client->ReadKey.Sequence == 1u),
		"TLS application backpressure did not preserve the pending record");
	testRequire(xrtTlsSessionPlainConsume(
		Fixture.Client, pLimits->PlainLimit
	) && (xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		!Fixture.Client->RecordPending &&
		(Fixture.Client->ReadKey.Sequence == 1u) &&
		(xrtTlsSessionRead(
			Fixture.Client, Received, sizeof(Received), &iRead
		) == XTLS_OK) && (iRead == sizeof(Application)) &&
		(memcmp(Received, Application, sizeof(Application)) == 0),
		"TLS application record did not resume after PlainLimit release");
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 篡改的服务端 Finished 不得提交应用 traffic secret，并且必须返回致命告警。 */
static void testTlsClientFinishedReject(void)
{
	test_tls_client_server_hello Fixture;
	uint8 Finished[64];
	xtlssession* pServer = testTlsClientFinishedBegin(&Fixture);
	const xerror* pError;
	xtlssessionrecord Record;
	xnetspan Span;
	xtlsalertlevel Level;
	xtlsalert Alert;
	size_t iFinished;
	xtlsresult Result;

	iFinished = testTlsClientFinishedMessage(
		&Fixture.State->Transcript,
		(xbytesview) {
			Fixture.State->ServerHandshakeTraffic,
			Fixture.State->HashSize
		}, Finished, sizeof(Finished)
	);
	Finished[iFinished - 1u] ^= 1u;
	xrtClearError();
	Result = testTlsClientServerProtect(
		&Fixture, pServer, Finished, iFinished
	);
	testRequire(Result == XTLS_ERROR,
		"TLS client accepted a damaged server Finished");
	testRequire(
		xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED,
		"TLS client did not enter FAILED after damaged Finished"
	);
	for ( size_t i = 0; i < Fixture.State->SecretCapacity; i++ ) {
		testRequire((Fixture.State->ClientApplicationTraffic[i] == 0) &&
			(Fixture.State->ServerApplicationTraffic[i] == 0),
			"TLS client committed application secrets after bad Finished");
	}
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(xrtErrorCode(pError) == XTLS_ERROR_VERIFY) &&
		(xrtErrorOperation(pError) != NULL) &&
		(strcmp(
			xrtErrorOperation(pError), "process-server-finished"
		) == 0), "TLS damaged Finished error mismatch");

	/* 验证失败只能产生 RFC 8446 要求的 fatal decrypt_error 告警。 */
	testRequire(xrtTlsSessionSendSize(Fixture.Client) != 0,
		"TLS client omitted the damaged Finished fatal alert");
	while ( xrtTlsSessionSendSize(Fixture.Client) != 0 ) {
		testRequire(xrtTlsSessionSendFront(Fixture.Client, &Span) &&
			(xrtTlsSessionFeed(
				pServer, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				Fixture.Client, Span.Size
			), "TLS damaged Finished alert move failed");
	}
	testRequire((__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK) && Record.Protected &&
		(Record.Type == XTLS_RECORD_ALERT) &&
		xrtTlsAlertParse(Record.Data, &Level, &Alert) &&
		(Level == XTLS_ALERT_FATAL) &&
		(Alert == XTLS_ALERT_DECRYPT_ERROR) &&
		(__xrtTlsSessionRecordFinish(
			pServer, false
		) == XTLS_OK) &&
		(xrtTlsSessionSendSize(Fixture.Client) == 0),
		"TLS damaged Finished fatal alert mismatch");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
	xrtClearError();
}



/* Finished 输出背压必须在解析前暂停，并可在排空后无损重试。 */
static void testTlsClientFinishedBackpressure(void)
{
	test_tls_client_server_hello Fixture;
	uint8 Finished[64];
	uint8 Before[32];
	uint8 After[32];
	xtlssession* pServer = testTlsClientFinishedBegin(&Fixture);
	const xtlslimits* pLimits = xrtTlsContextLimits(Fixture.Context);
	bytes pFill;
	size_t iFinished;
	size_t iRecord;
	size_t iFill;

	iFinished = testTlsClientFinishedMessage(
		&Fixture.State->Transcript,
		(xbytesview) {
			Fixture.State->ServerHandshakeTraffic,
			Fixture.State->HashSize
		}, Finished, sizeof(Finished)
	);
	iRecord = __xrtTlsRecordSealSize(
		&Fixture.Client->WriteKey,
		xrtTlsHandshakeSize(Fixture.State->HashSize), 0
	);
	testRequire((pLimits != NULL) && (iRecord != 0) &&
		(iRecord <= pLimits->SendLimit),
		"TLS Finished backpressure limit is invalid");
	iFill = pLimits->SendLimit - iRecord + 1u;
	pFill = (bytes)xrtMalloc(iFill);
	testRequire(pFill != NULL,
		"TLS Finished backpressure filler allocation failed");
	memset(pFill, 0xA5, iFill);
	testRequire((__xrtTlsSessionSend(
		Fixture.Client, pFill, iFill
	) == XTLS_OK) && __xrtTlsTranscriptDigest(
		&Fixture.State->Transcript, Before, sizeof(Before)
	), "TLS Finished backpressure setup failed");
	xrtFree(pFill);
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Finished, iFinished
	) == XTLS_AGAIN) &&
		(Fixture.State->Step == XTLS_CLIENT_WAIT_FINISHED) &&
		(Fixture.State->RecordOffset == 0) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_HANDSHAKE) &&
		((xrtTlsSessionWait(Fixture.Client) & XTLS_WAIT_OUTPUT) != 0) &&
		__xrtTlsTranscriptDigest(
			&Fixture.State->Transcript, After, sizeof(After)
		) && (memcmp(Before, After, sizeof(Before)) == 0),
		"TLS Finished backpressure changed handshake state");
	testRequire(xrtTlsSessionSendConsume(Fixture.Client, iFill) &&
		(xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_READY) &&
		(Fixture.State->Step == XTLS_CLIENT_READY) &&
		(Fixture.State->RecordOffset == 0),
		"TLS Finished did not recover after output drain");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 证书快照 OOM 必须保持未提交状态，并留下可定位的原因链。 */
static void testTlsClientCertificateSnapshotOom(void)
{
	test_tls_client_server_hello Fixture;
	uint8 EncryptedExtensions[256];
	uint8 Certificate[2048];
	xtlssession* pServer;
	xnetspan Span;
	const xerror* pError;
	size_t iEncryptedExtensions;
	size_t iCertificate;

	pServer = testTlsClientCertificateBegin(
		&Fixture, true, EncryptedExtensions,
		sizeof(EncryptedExtensions), &iEncryptedExtensions
	);
	(void)iEncryptedExtensions;
	iCertificate = testTlsClientCertificateMessage(
		(xbytesview) { NULL, 0 }, Certificate, sizeof(Certificate)
	);
	testRequire(__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Certificate, iCertificate }, 0
	) == XTLS_OK, "TLS certificate OOM record protection failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				Fixture.Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS certificate OOM record move failed");
	}

	xrtClearError();
	TestTlsClientAlloc.FailAt = TestTlsClientAlloc.Calls + 1u;
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(Fixture.State->Peer == NULL),
		"TLS client committed a partial certificate snapshot after OOM");
	TestTlsClientAlloc.FailAt = SIZE_MAX;
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(xrtErrorIs(pError, XERR_MEMORY) != NULL) &&
		(xrtErrorOperation(pError) != NULL) &&
		(strcmp(
			xrtErrorOperation(pError), "process-server-certificate"
		) == 0), "TLS certificate snapshot OOM error mismatch");

	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
	xrtClearError();
}



/* 缺少验证器时，即使 DER 合法也必须在提交证书前失败。 */
static void testTlsClientCertificateRejectNoVerifier(void)
{
	test_tls_client_server_hello Fixture;
	uint8 EncryptedExtensions[256];
	uint8 Certificate[2048];
	xtlssession* pServer;
	size_t iEncryptedExtensions;
	size_t iCertificate;

	pServer = testTlsClientCertificateBegin(
		&Fixture, false, EncryptedExtensions,
		sizeof(EncryptedExtensions), &iEncryptedExtensions
	);
	(void)iEncryptedExtensions;
	iCertificate = testTlsClientCertificateMessage(
		(xbytesview) { NULL, 0 }, Certificate, sizeof(Certificate)
	);
	xrtClearError();
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Certificate, iCertificate
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_VERIFY),
		"TLS client accepted a certificate without a verifier");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 未请求的 CertificateEntry 扩展不可进入信任回调。 */
static void testTlsClientCertificateRejectExtension(void)
{
	static const uint8 Extension[] = { 0, 0, 0, 0 };
	test_tls_client_server_hello Fixture;
	uint8 EncryptedExtensions[256];
	uint8 Certificate[2048];
	xtlssession* pServer;
	size_t iEncryptedExtensions;
	size_t iCertificate;

	pServer = testTlsClientCertificateBegin(
		&Fixture, true, EncryptedExtensions,
		sizeof(EncryptedExtensions), &iEncryptedExtensions
	);
	(void)iEncryptedExtensions;
	iCertificate = testTlsClientCertificateMessage(
		(xbytesview) { Extension, sizeof(Extension) },
		Certificate, sizeof(Certificate)
	);
	xrtClearError();
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Certificate, iCertificate
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_EXTENSION),
		"TLS client accepted an unrequested certificate extension");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 坏签名和线路未提供的签名方案都必须稳定进入失败终态。 */
static void testTlsClientCertificateVerifyRejectOne(
	xtlssignature Scheme,
	bool bDamage
)
{
	test_tls_client_server_hello Fixture;
	uint8 EncryptedExtensions[256];
	uint8 Certificate[2048];
	uint8 Verify[768];
	xtlsidentity* pIdentity = testTlsClientIdentity();
	xtlssession* pServer;
	size_t iEncryptedExtensions;
	size_t iCertificate;
	size_t iVerify;

	pServer = testTlsClientCertificateBegin(
		&Fixture, true, EncryptedExtensions,
		sizeof(EncryptedExtensions), &iEncryptedExtensions
	);
	(void)iEncryptedExtensions;
	iCertificate = testTlsClientCertificateMessage(
		(xbytesview) { NULL, 0 }, Certificate, sizeof(Certificate)
	);
	testRequire(testTlsClientServerProtect(
		&Fixture, pServer, Certificate, iCertificate
	) == XTLS_OK, "TLS rejected-signature Certificate failed");
	iVerify = testTlsClientCertificateVerifyMessage(
		&Fixture, pIdentity, Scheme, bDamage, Verify, sizeof(Verify)
	);
	xrtClearError();
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Verify, iVerify
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_VERIFY),
		"TLS client accepted an invalid CertificateVerify");
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
	xrtTlsIdentityRelease(pIdentity);
}



/* 覆盖 CertificateVerify 的密码失败和 offer 约束。 */
static void testTlsClientCertificateVerifyReject(void)
{
	testTlsClientCertificateVerifyRejectOne(
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256, true
	);
	testTlsClientCertificateVerifyRejectOne(
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384, false
	);
}



/* PSK+DHE 恢复必须跳过证书航班并完成真实双向应用 epoch。 */
static void testTlsClientResumeValid(void)
{
	test_tls_client_server_hello Fixture;
	uint8 Secret[32];
	uint8 EncryptedExtensions[256];
	uint8 ClientTraffic[32];
	uint8 ServerTraffic[32];
	uint8 Ticket[128];
	uint8 Application[] = "resumed-application";
	uint8 Received[sizeof(Application)];
	xtlsresume* pOffered;
	xtlsresume* pNext;
	xtlsresumeinfo Info;
	xtlssession* pServer;
	xnetspan Span;
	xbytesview Protocol;
	size_t iEncryptedExtensions;
	size_t iTicket;
	size_t iRead;

	pOffered = testTlsClientServerFixtureInitResume(
		&Fixture, TEST_TLS_SERVER_HELLO_PSK, Secret
	);
	xrtTlsResumeRelease(pOffered);
	testRequire((testTlsClientServerFeed(
		&Fixture, 0, Fixture.ServerHelloSize
	) == XTLS_OK) && xrtTlsClientResumed(Fixture.Client) &&
		Fixture.State->Resumed &&
		(Fixture.State->Step == XTLS_CLIENT_WAIT_ENCRYPTED_EXTENSIONS),
		"TLS client did not accept the selected PSK identity");
	pServer = testTlsClientServerPeer(&Fixture);
	iEncryptedExtensions = testTlsClientEncryptedExtensions(
		TEST_TLS_ENCRYPTED_EXTENSIONS_VALID,
		EncryptedExtensions, sizeof(EncryptedExtensions)
	);
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, EncryptedExtensions, iEncryptedExtensions
	) == XTLS_OK) &&
		(Fixture.State->Step == XTLS_CLIENT_WAIT_FINISHED),
		"TLS resumed client did not skip certificate authentication");
	testTlsClientReadyFinish(
		&Fixture, pServer, ClientTraffic, ServerTraffic
	);
	testRequire(xrtTlsClientResumed(Fixture.Client) &&
		xrtTlsSessionProtocol(Fixture.Client, &Protocol) &&
		(Protocol.Size == 2u) &&
		(memcmp(
			Protocol.Data, "h2", 2u
		) == 0), "TLS resumed client lost its ALPN or resume state");

	/* 应用记录证明恢复握手安装的是可互操作密钥，而不只是状态标记。 */
	testRequire(__xrtTlsSessionRecordProtect(
		pServer, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Application, sizeof(Application) }, 0
	) == XTLS_OK, "TLS resumed application protection failed");
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(xrtTlsSessionSendFront(pServer, &Span) &&
			(xrtTlsSessionFeed(
				Fixture.Client, Span.Data, Span.Size
			) == XTLS_OK) && xrtTlsSessionSendConsume(
				pServer, Span.Size
			), "TLS resumed application record move failed");
	}
	testRequire((xrtTlsClientDrive(Fixture.Client) == XTLS_OK) &&
		(xrtTlsSessionRead(
			Fixture.Client, Received, sizeof(Received), &iRead
		) == XTLS_OK) && (iRead == sizeof(Application)) &&
		(memcmp(Received, Application, sizeof(Application)) == 0),
		"TLS resumed application data mismatch");

	/* 恢复连接签发的新票据必须继承原已认证身份，而不是空证书状态。 */
	iTicket = testTlsClientSessionTicketMessage(
		Ticket, sizeof(Ticket)
	);
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, Ticket, iTicket
	) == XTLS_OK) &&
		(xrtTlsClientResumeCount(Fixture.Client) == 1u),
		"TLS resumed connection did not publish a new ticket");
	pNext = xrtTlsClientTakeResume(Fixture.Client);
	testRequire((pNext != NULL) && xrtTlsResumeInfo(pNext, &Info) &&
		(Info.Protocol.Size == 2u) &&
		(memcmp(Info.Protocol.Data, "h2", 2u) == 0) &&
		(Info.PeerIdentity.Size == sizeof("authenticated-peer") - 1u) &&
		(memcmp(
			Info.PeerIdentity.Data, "authenticated-peer",
			Info.PeerIdentity.Size
		) == 0), "TLS resumed ticket lost its authenticated binding");
	xrtTlsResumeRelease(pNext);
	xrtSecureZero(ServerTraffic, sizeof(ServerTraffic));
	xrtSecureZero(ClientTraffic, sizeof(ClientTraffic));
	xrtSecureZero(Secret, sizeof(Secret));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 服务端拒绝 PSK 时客户端必须安全回到完整证书握手。 */
static void testTlsClientResumeFallback(void)
{
	test_tls_client_server_hello Fixture;
	uint8 Secret[32];
	uint8 EncryptedExtensions[256];
	xtlsresume* pResume = testTlsClientServerFixtureInitResume(
		&Fixture, TEST_TLS_SERVER_HELLO_VALID, Secret
	);
	xtlssession* pServer;
	size_t iEncryptedExtensions;

	xrtTlsResumeRelease(pResume);
	testRequire((testTlsClientServerFeed(
		&Fixture, 0, Fixture.ServerHelloSize
	) == XTLS_OK) && !xrtTlsClientResumed(Fixture.Client) &&
		!Fixture.State->Resumed,
		"TLS client did not fall back after PSK rejection");
	pServer = testTlsClientServerPeer(&Fixture);
	iEncryptedExtensions = testTlsClientEncryptedExtensions(
		TEST_TLS_ENCRYPTED_EXTENSIONS_VALID,
		EncryptedExtensions, sizeof(EncryptedExtensions)
	);
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, EncryptedExtensions, iEncryptedExtensions
	) == XTLS_OK) &&
		(Fixture.State->Step == XTLS_CLIENT_WAIT_CERTIFICATE),
		"TLS client PSK fallback did not enter certificate authentication");
	xrtSecureZero(Secret, sizeof(Secret));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 非法 PSK 索引、缺少 DHE share 和改变票据 ALPN 都必须失败。 */
static void testTlsClientResumeReject(void)
{
	static const test_tls_server_hello_mode Modes[] = {
		TEST_TLS_SERVER_HELLO_PSK_BAD_INDEX,
		TEST_TLS_SERVER_HELLO_PSK_MISSING_SHARE
	};
	static const xtlserror Errors[] = {
		XTLS_ERROR_NEGOTIATION,
		XTLS_ERROR_EXTENSION
	};
	test_tls_client_server_hello Fixture;
	uint8 Secret[32];
	uint8 EncryptedExtensions[256];
	xtlsresume* pResume;
	xtlssession* pServer;
	size_t iEncryptedExtensions;

	for ( size_t i = 0; i < sizeof(Modes) / sizeof(Modes[0]); i++ ) {
		pResume = testTlsClientServerFixtureInitResume(
			&Fixture, Modes[i], Secret
		);
		xrtTlsResumeRelease(pResume);
		xrtClearError();
		testRequire((testTlsClientServerFeed(
			&Fixture, 0, Fixture.ServerHelloSize
		) == XTLS_ERROR) &&
			(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
			(xrtErrorCode(xrtGetError()) == (int32)Errors[i]) &&
			!Fixture.Client->ReadKey.Ready &&
			!Fixture.Client->WriteKey.Ready,
			"TLS client accepted an invalid resumed ServerHello");
		testTlsClientServerFixtureUnit(&Fixture);
	}

	pResume = testTlsClientServerFixtureInitResume(
		&Fixture, TEST_TLS_SERVER_HELLO_PSK, Secret
	);
	xrtTlsResumeRelease(pResume);
	testRequire(testTlsClientServerFeed(
		&Fixture, 0, Fixture.ServerHelloSize
	) == XTLS_OK, "TLS resumed ALPN fixture ServerHello failed");
	pServer = testTlsClientServerPeer(&Fixture);
	iEncryptedExtensions = testTlsClientEncryptedExtensions(
		TEST_TLS_ENCRYPTED_EXTENSIONS_HTTP1,
		EncryptedExtensions, sizeof(EncryptedExtensions)
	);
	xrtClearError();
	testRequire((testTlsClientServerProtect(
		&Fixture, pServer, EncryptedExtensions, iEncryptedExtensions
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_NEGOTIATION),
		"TLS resumed client accepted a changed bound ALPN");
	xrtSecureZero(Secret, sizeof(Secret));
	xrtTlsSessionDestroy(pServer);
	testTlsClientServerFixtureUnit(&Fixture);
	xrtClearError();
}



/* 有效 ServerHello 必须支持跨记录重组并原子切换握手 epoch。 */
static void testTlsClientServerHelloValid(void)
{
	test_tls_client_server_hello Fixture;
	size_t iTail;

	testTlsClientServerFixtureInit(
		&Fixture, TEST_TLS_SERVER_HELLO_VALID
	);
	testRequire(testTlsClientServerFeed(
		&Fixture, 0, 2u
	) == XTLS_OK, "TLS ServerHello first fragment failed");
	testRequire(testTlsClientServerFeed(
		&Fixture, 2u, 7u
	) == XTLS_OK, "TLS ServerHello second fragment failed");
	iTail = Fixture.ServerHelloSize - 9u;
	testRequire(testTlsClientServerFeed(
		&Fixture, 9u, iTail
	) == XTLS_OK, "TLS ServerHello final fragment failed");
	testRequire((xrtTlsSessionFeedSize(Fixture.Client) == 0) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_HANDSHAKE) &&
		(Fixture.State->Step == XTLS_CLIENT_WAIT_ENCRYPTED_EXTENSIONS) &&
		(Fixture.State->Cipher == XTLS_AES_128_GCM_SHA256) &&
		(Fixture.State->HashSize == 32u) &&
		Fixture.Client->ReadKey.Ready && Fixture.Client->WriteKey.Ready &&
		(Fixture.Client->ReadKey.Sequence == 0) &&
		(Fixture.Client->WriteKey.Sequence == 0),
		"TLS client ServerHello commit state mismatch");
	for ( size_t i = 0; i < Fixture.State->PrivateKeySize; i++ ) {
		testRequire(Fixture.State->PrivateKey[i] == 0,
			"TLS client retained its ephemeral private key");
	}
	testTlsClientServerKeys(&Fixture);
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 单个非法 ServerHello 必须让客户端进入稳定失败终态。 */
static void testTlsClientServerHelloRejectOne(
	test_tls_server_hello_mode Mode,
	xtlserror Code
)
{
	test_tls_client_server_hello Fixture;

	testTlsClientServerFixtureInit(&Fixture, Mode);
	xrtClearError();
	testRequire((testTlsClientServerFeed(
		&Fixture, 0, Fixture.ServerHelloSize
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)Code) &&
		!Fixture.Client->ReadKey.Ready &&
		!Fixture.Client->WriteKey.Ready,
		"TLS client accepted an invalid ServerHello");
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 所有服务端选择都必须来自真实线路 offer，且关键扩展不可缺失。 */
static void testTlsClientServerHelloReject(void)
{
	testTlsClientServerHelloRejectOne(
		TEST_TLS_SERVER_HELLO_WRONG_SESSION, XTLS_ERROR_HANDSHAKE
	);
	testTlsClientServerHelloRejectOne(
		TEST_TLS_SERVER_HELLO_UNOFFERED_CIPHER, XTLS_ERROR_CIPHER
	);
	testTlsClientServerHelloRejectOne(
		TEST_TLS_SERVER_HELLO_MISSING_VERSION, XTLS_ERROR_VERSION
	);
	testTlsClientServerHelloRejectOne(
		TEST_TLS_SERVER_HELLO_MISSING_SHARE, XTLS_ERROR_EXTENSION
	);
	testTlsClientServerHelloRejectOne(
		TEST_TLS_SERVER_HELLO_WRONG_GROUP, XTLS_ERROR_KEY_EXCHANGE
	);
	testTlsClientServerHelloRejectOne(
		TEST_TLS_SERVER_HELLO_RETRY, XTLS_ERROR_HANDSHAKE
	);
	testTlsClientServerHelloRejectOne(
		TEST_TLS_SERVER_HELLO_PSK, XTLS_ERROR_EXTENSION
	);
}



/* 兼容 CCS 只接受单字节 1，其他明文记录不可穿过首航状态。 */
static void testTlsClientServerRecordReject(void)
{
	test_tls_client_server_hello Fixture;
	uint8 Record[32];
	uint8 Alert[2];
	uint8 Ccs = 2u;
	size_t iSize;

	testTlsClientServerFixtureInit(
		&Fixture, TEST_TLS_SERVER_HELLO_VALID
	);
	iSize = xrtTlsRecordSize(1u);
	testRequire(xrtTlsRecordEncode(
		XTLS_RECORD_CHANGE_CIPHER_SPEC, XTLS_VERSION_12,
		(xbytesview) { &Ccs, 1u }, Record, sizeof(Record)
	) && (xrtTlsSessionFeed(
		Fixture.Client, Record, iSize
	) == XTLS_OK) && (xrtTlsClientDrive(
		Fixture.Client
	) == XTLS_ERROR) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_HANDSHAKE),
		"TLS client accepted malformed compatibility CCS");
	testTlsClientServerFixtureUnit(&Fixture);

	testTlsClientServerFixtureInit(
		&Fixture, TEST_TLS_SERVER_HELLO_VALID
	);
	iSize = xrtTlsRecordSize(sizeof(Alert));
	testRequire(xrtTlsAlertEncode(
		XTLS_ALERT_FATAL, XTLS_ALERT_HANDSHAKE_FAILURE,
		Alert, sizeof(Alert)
	) && xrtTlsRecordEncode(
		XTLS_RECORD_ALERT, XTLS_VERSION_12,
		(xbytesview) { Alert, sizeof(Alert) }, Record, sizeof(Record)
	) && (xrtTlsSessionFeed(
		Fixture.Client, Record, iSize
	) == XTLS_OK) && (xrtTlsClientDrive(
		Fixture.Client
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(Fixture.Client) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ALERT),
		"TLS client did not propagate a fatal peer alert");
	testTlsClientServerFixtureUnit(&Fixture);

	testTlsClientServerFixtureInit(
		&Fixture, TEST_TLS_SERVER_HELLO_VALID
	);
	iSize = xrtTlsRecordSize(1u);
	testRequire(xrtTlsRecordEncode(
		XTLS_RECORD_APPLICATION_DATA, XTLS_VERSION_12,
		XRT_BYTES_LITERAL("x"), Record, sizeof(Record)
	) && (xrtTlsSessionFeed(
		Fixture.Client, Record, iSize
	) == XTLS_OK) && (xrtTlsClientDrive(
		Fixture.Client
	) == XTLS_ERROR) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_RECORD_TYPE),
		"TLS client accepted plaintext application data before ServerHello");
	testTlsClientServerFixtureUnit(&Fixture);
}



/* 执行 TLS 客户端 ServerHello、密钥切换和失败边界回归。 */
int main(void)
{
	xallocator Allocator;

	Allocator.Context = &TestTlsClientAlloc;
	Allocator.Alloc = testTlsClientAlloc;
	Allocator.Realloc = testTlsClientRealloc;
	Allocator.Free = testTlsClientFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS client test allocator install failed");
	testTlsClientServerHelloValid();
	testTlsClientServerHelloReject();
	testTlsClientServerRecordReject();
	testTlsClientResumeValid();
	testTlsClientResumeFallback();
	testTlsClientResumeReject();
	testTlsClientEncryptedExtensionsValid();
	testTlsClientEncryptedExtensionsCoalesced();
	testTlsClientEncryptedExtensionsReject();
	testTlsClientCertificateVerifyValid();
	testTlsClientKeyUpdateNotRequested();
	testTlsClientKeyUpdateSend();
	testTlsClientKeyUpdateSendBackpressure();
	testTlsClientKeyUpdateBackpressure();
	testTlsClientKeyUpdateReject();
	testTlsClientSessionTicketDrive();
	testTlsClientSessionTicketOom();
	testTlsClientApplicationBackpressure();
	testTlsClientFinishedReject();
	testTlsClientFinishedBackpressure();
	testTlsClientCertificateSnapshotOom();
	testTlsClientCertificateRejectNoVerifier();
	testTlsClientCertificateRejectExtension();
	testTlsClientCertificateVerifyReject();
	return 0;
}
