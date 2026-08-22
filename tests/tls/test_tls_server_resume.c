#include "../fixtures/tls_server.h"



/* 所有恢复边界共享一套真实上下文、身份和证书验证器。 */
typedef struct test_tls_server_resume_env {
	xtlscontext* Context;
	xtlsidentity* Identity;
	xtlsverifier* Verifier;
} test_tls_server_resume_env;



/* 查找状态可返回命中对象，也可显式模拟缓存未命中。 */
typedef struct test_tls_server_resume_lookup {
	const xtlsresume* Resume;
	size_t Calls;
} test_tls_server_resume_lookup;



/* ClientHello 变异只改变一处 PSK 契约，其余线路结构保持真实。 */
typedef enum test_tls_server_resume_mutation {
	TEST_TLS_SERVER_RESUME_MISSING_MODE = 1,
	TEST_TLS_SERVER_RESUME_MALFORMED_PSK,
	TEST_TLS_SERVER_RESUME_NON_FINAL_PSK
} test_tls_server_resume_mutation;



static const xstrview TEST_TLS_SERVER_PROTOCOLS[] = {
	XRT_STR_INIT("http/1.1")
};



/* 测试缓存返回借用对象，服务端必须立即持有自己的引用。 */
static const xtlsresume* testTlsServerResumeLookup(
	ptr pContext,
	const xtlsserverresumerequest* pRequest
)
{
	test_tls_server_resume_lookup* pLookup =
		(test_tls_server_resume_lookup*)pContext;

	if ( (pLookup == NULL) || (pRequest == NULL) ) {
		return NULL;
	}
	pLookup->Calls++;
	return pLookup->Resume;
}



/* 创建恢复边界测试共享资源。 */
static bool testTlsServerResumeEnvInit(
	test_tls_server_resume_env* pEnv
)
{
	xtlsverifierconfig Config;

	memset(pEnv, 0, sizeof(*pEnv));
	pEnv->Context = testTlsServerContext();
	pEnv->Identity = testTlsServerIdentity();
	xrtTlsVerifierConfigInit(&Config);
	Config.Verify = testTlsServerAccept;
	pEnv->Verifier = xrtTlsVerifierCreate(&Config);
	return (pEnv->Context != NULL) && (pEnv->Identity != NULL) &&
		(pEnv->Verifier != NULL);
}



/* 释放恢复边界测试共享资源。 */
static void testTlsServerResumeEnvUnit(
	test_tls_server_resume_env* pEnv
)
{
	xrtTlsVerifierRelease(pEnv->Verifier);
	xrtTlsIdentityRelease(pEnv->Identity);
	xrtTlsContextRelease(pEnv->Context);
	memset(pEnv, 0, sizeof(*pEnv));
}



/* 构造线路身份相同但可独立改变绑定字段的恢复对象。 */
static xtlsresume* testTlsServerResumeCreate(
	uint8 iTicket,
	uint8 iSecret,
	xstrview ServerName,
	xbytesview Protocol,
	xtime iIssuedAt,
	uint32 iLifetime
)
{
	uint8 Ticket[32];
	uint8 Secret[32];
	xtlsresumeconfig Config;
	xtlsresume* pResume;

	memset(Ticket, iTicket, sizeof(Ticket));
	memset(Secret, iSecret, sizeof(Secret));
	xrtTlsResumeConfigInit(&Config);
	Config.Cipher = XTLS_AES_128_GCM_SHA256;
	Config.Ticket = (xbytesview) { Ticket, sizeof(Ticket) };
	Config.Secret = (xbytesview) { Secret, sizeof(Secret) };
	Config.ServerName = ServerName;
	Config.Protocol = Protocol;
	Config.Lifetime = iLifetime;
	Config.AgeAdd = UINT32_C(0x31415926);
	Config.IssuedAt = iIssuedAt;
	pResume = xrtTlsResumeCreate(&Config);
	xrtSecureZero(Secret, sizeof(Secret));
	xrtSecureZero(Ticket, sizeof(Ticket));
	return pResume;
}



/* 从共享资源和指定票据创建一对尚未驱动的真实会话。 */
static bool testTlsServerResumeSessions(
	const test_tls_server_resume_env* pEnv,
	const xtlsresume* pClientResume,
	test_tls_server_resume_lookup* pLookup,
	xtlssession** ppClient,
	xtlssession** ppServer
)
{
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;

	*ppClient = NULL;
	*ppServer = NULL;
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pEnv->Context;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = TEST_TLS_SERVER_PROTOCOLS;
	ClientConfig.ProtocolCount = sizeof(TEST_TLS_SERVER_PROTOCOLS) /
		sizeof(TEST_TLS_SERVER_PROTOCOLS[0]);
	ClientConfig.Verifier = pEnv->Verifier;
	ClientConfig.Resume = pClientResume;
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Context = pEnv->Context;
	ServerConfig.Identity = pEnv->Identity;
	ServerConfig.Protocols = TEST_TLS_SERVER_PROTOCOLS;
	ServerConfig.ProtocolCount = sizeof(TEST_TLS_SERVER_PROTOCOLS) /
		sizeof(TEST_TLS_SERVER_PROTOCOLS[0]);
	ServerConfig.RequireProtocol = true;
	ServerConfig.Resume = testTlsServerResumeLookup;
	ServerConfig.ResumeContext = pLookup;
	*ppClient = xrtTlsClientCreate(&ClientConfig, NULL);
	*ppServer = xrtTlsServerCreate(&ServerConfig, NULL);
	if ( (*ppClient == NULL) || (*ppServer == NULL) ) {
		xrtTlsSessionDestroy(*ppServer);
		xrtTlsSessionDestroy(*ppClient);
		*ppClient = NULL;
		*ppServer = NULL;
		return false;
	}
	return true;
}



/* 不匹配或过期的缓存对象必须安全回退完整证书握手。 */
static void testTlsServerResumeFallback(
	const test_tls_server_resume_env* pEnv,
	const xtlsresume* pClientResume,
	const xtlsresume* pServerResume,
	cstr sMessage,
	uint32 iSeed
)
{
	test_tls_server_resume_lookup Lookup = { pServerResume, 0 };
	test_tls_server_rng Rng = { iSeed };
	xtlssession* pClient;
	xtlssession* pServer;

	testRequire(testTlsServerResumeSessions(
		pEnv, pClientResume, &Lookup, &pClient, &pServer
	), "TLS resume fallback session creation failed");
	testRequire(testTlsServerHandshake(pClient, pServer, &Rng) &&
		(Lookup.Calls == 1u) &&
		!xrtTlsClientResumed(pClient) &&
		!xrtTlsServerResumed(pServer), sMessage);
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
}



/* 命中同一身份但 binder 错误时必须终止，禁止降级到证书握手。 */
static void testTlsServerResumeBadBinder(
	const test_tls_server_resume_env* pEnv,
	const xtlsresume* pClientResume,
	const xtlsresume* pServerResume
)
{
	test_tls_server_resume_lookup Lookup = { pServerResume, 0 };
	test_tls_server_rng Rng = { UINT32_C(0xBADB1ADE) };
	xtlssession* pClient;
	xtlssession* pServer;
	bool bFailed = false;

	testRequire(testTlsServerResumeSessions(
		pEnv, pClientResume, &Lookup, &pClient, &pServer
	), "TLS bad-binder session creation failed");
	for ( size_t i = 0; i < 128u; i++ ) {
		xtlsresult ClientResult = xrtTlsClientDrive(pClient);
		xtlsresult ServerResult = xrtTlsServerDrive(pServer);
		bool bProgress = false;

		testRequire((ClientResult == XTLS_OK) ||
			(ClientResult == XTLS_AGAIN),
			"TLS client failed before bad binder reached the server");
		if ( ServerResult == XTLS_ERROR ) {
			bFailed = (xrtTlsSessionState(pServer) == XTLS_STATE_FAILED) &&
				(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
				(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_VERIFY);
			break;
		}
		testRequire(ServerResult == XTLS_AGAIN || ServerResult == XTLS_OK,
			"TLS server returned an unexpected bad-binder result");
		testRequire(testTlsServerMove(
			pClient, pServer, &Rng, 23u, &bProgress
		), "TLS bad-binder ClientHello transfer failed");
	}
	testRequire(bFailed && (Lookup.Calls == 1u),
		"TLS bad binder was downgraded instead of rejected");
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtClearError();
}



/* 复制并消费客户端首个完整 ClientHello 记录。 */
static bytes testTlsServerResumeHelloCopy(
	xtlssession* pClient,
	size_t* pSize
)
{
	bytes pCopy;
	size_t iSize = xrtTlsSessionSendSize(pClient);
	size_t iOffset = 0;

	*pSize = 0;
	if ( iSize == 0 ) {
		return NULL;
	}
	pCopy = (bytes)xrtMalloc(iSize);
	if ( pCopy == NULL ) {
		return NULL;
	}
	while ( iOffset < iSize ) {
		xnetspan Span;

		if ( !xrtTlsSessionSendFront(pClient, &Span) ||
			(Span.Size == 0) || (Span.Size > (iSize - iOffset)) ) {
			xrtFree(pCopy);
			return NULL;
		}
		memcpy(pCopy + iOffset, Span.Data, Span.Size);
		iOffset += Span.Size;
		if ( !xrtTlsSessionSendConsume(pClient, Span.Size) ) {
			xrtFree(pCopy);
			return NULL;
		}
	}
	*pSize = iSize;
	return pCopy;
}



/* 定位真实 ClientHello 的扩展向量并对单一 PSK 契约做变异。 */
static bool testTlsServerResumeMutate(
	bytes pData,
	size_t iSize,
	test_tls_server_resume_mutation Mutation
)
{
	xtlsrecord Record;
	xtlshandshake Handshake;
	xtlsclienthello Hello;
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsextension Previous;
	xtlsextension Psk;
	xtlsextension Modes;
	size_t iRequired = 0;
	bool bPrevious = false;
	bool bPsk = false;
	bool bModes = false;

	memset(&Previous, 0, sizeof(Previous));
	memset(&Psk, 0, sizeof(Psk));
	memset(&Modes, 0, sizeof(Modes));
	if ( (xrtTlsRecordParse(
		(xbytesview) { pData, iSize }, &Record, &iRequired
	) != XTLS_OK) || (Record.EncodedSize != iSize) ||
		(xrtTlsHandshakeParse(
			Record.Payload, &Handshake, &iRequired
		) != XTLS_OK) ||
		(Handshake.Type != XTLS_HANDSHAKE_CLIENT_HELLO) ||
		!xrtTlsClientHelloParse(Handshake.Body, &Hello) ||
		!xrtTlsExtensionsInit(&Cursor, Hello.Extensions) ) {
		return false;
	}
	while ( xrtTlsExtensionsRead(
		&Cursor, &Extension
	) == XTLS_ITEM_VALUE ) {
		if ( Extension.Type == XTLS_EXTENSION_PRE_SHARED_KEY ) {
			Psk = Extension;
			bPsk = true;
			break;
		}
		if ( Extension.Type ==
			XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES ) {
			Modes = Extension;
			bModes = true;
		}
		Previous = Extension;
		bPrevious = true;
	}
	if ( !bPsk ) {
		return false;
	}
	if ( Mutation == TEST_TLS_SERVER_RESUME_MISSING_MODE ) {
		bytes pHeader;

		if ( !bModes ) {
			return false;
		}
		pHeader = (bytes)Modes.Data.Data - 4u;
		pHeader[0] = 0xFAu;
		pHeader[1] = 0xFAu;
		return true;
	}
	if ( Mutation == TEST_TLS_SERVER_RESUME_MALFORMED_PSK ) {
		bytes pPsk = (bytes)Psk.Data.Data;

		if ( Psk.Data.Size < 2u ) {
			return false;
		}
		pPsk[0] = 0;
		pPsk[1] = 0;
		return true;
	}
	if ( Mutation == TEST_TLS_SERVER_RESUME_NON_FINAL_PSK ) {
		bytes pPrevious;
		bytes pPsk;
		bytes pTemporary;

		if ( !bPrevious ) {
			return false;
		}
		pPrevious = (bytes)Previous.Data.Data - 4u;
		pPsk = (bytes)Psk.Data.Data - 4u;
		if ( (pPrevious + Previous.EncodedSize) != pPsk ) {
			return false;
		}
		pTemporary = (bytes)xrtMalloc(Psk.EncodedSize);
		if ( pTemporary == NULL ) {
			return false;
		}
		memcpy(pTemporary, pPsk, Psk.EncodedSize);
		memmove(
			pPrevious + Psk.EncodedSize,
			pPrevious, Previous.EncodedSize
		);
		memcpy(pPrevious, pTemporary, Psk.EncodedSize);
		xrtFree(pTemporary);
		return true;
	}
	return false;
}



/* 非法 PSK 扩展结构必须在首航选择前进入协议失败终态。 */
static void testTlsServerResumeMalformed(
	const test_tls_server_resume_env* pEnv,
	const xtlsresume* pResume,
	test_tls_server_resume_mutation Mutation,
	cstr sMessage
)
{
	test_tls_server_resume_lookup Lookup = { pResume, 0 };
	xtlssession* pClient;
	xtlssession* pServer;
	bytes pHello;
	size_t iHello;

	testRequire(testTlsServerResumeSessions(
		pEnv, pResume, &Lookup, &pClient, &pServer
	), "TLS malformed-resume session creation failed");
	pHello = testTlsServerResumeHelloCopy(pClient, &iHello);
	testRequire((pHello != NULL) &&
		testTlsServerResumeMutate(pHello, iHello, Mutation),
		"TLS ClientHello resume mutation failed");
	xrtClearError();
	testRequire((xrtTlsSessionFeed(
		pServer, pHello, iHello
	) == XTLS_OK) && (xrtTlsServerDrive(pServer) == XTLS_ERROR) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_FAILED) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL), sMessage);
	xrtFree(pHello);
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtClearError();
}



/* 覆盖票据失配的安全回退、binder 防降级和 PSK 线路结构边界。 */
int main(void)
{
	test_tls_server_resume_env Env;
	const xbytesview Http11 = XRT_BYTES_LITERAL("http/1.1");
	const xbytesview H2 = XRT_BYTES_LITERAL("h2");
	xtime iNow = xrtNow();
	xtlsresume* pClient;
	xtlsresume* pTicket;
	xtlsresume* pName;
	xtlsresume* pProtocol;
	xtlsresume* pExpired;
	xtlsresume* pAge;
	xtlsresume* pSecret;

	testRequire(testTlsServerResumeEnvInit(&Env),
		"TLS resume boundary environment creation failed");
	pClient = testTlsServerResumeCreate(
		0x11u, 0x22u, XRT_STR_LITERAL("example.com"),
		Http11, iNow, 120u
	);
	pTicket = testTlsServerResumeCreate(
		0x12u, 0x22u, XRT_STR_LITERAL("example.com"),
		Http11, iNow, 120u
	);
	pName = testTlsServerResumeCreate(
		0x11u, 0x22u, XRT_STR_LITERAL("other.example"),
		Http11, iNow, 120u
	);
	pProtocol = testTlsServerResumeCreate(
		0x11u, 0x22u, XRT_STR_LITERAL("example.com"),
		H2, iNow, 120u
	);
	pExpired = testTlsServerResumeCreate(
		0x11u, 0x22u, XRT_STR_LITERAL("example.com"),
		Http11, iNow - INT64_C(2000000), 1u
	);
	pAge = testTlsServerResumeCreate(
		0x11u, 0x22u, XRT_STR_LITERAL("example.com"),
		Http11, iNow - INT64_C(30000000), 120u
	);
	pSecret = testTlsServerResumeCreate(
		0x11u, 0x23u, XRT_STR_LITERAL("example.com"),
		Http11, iNow, 120u
	);
	testRequire((pClient != NULL) && (pTicket != NULL) &&
		(pName != NULL) && (pProtocol != NULL) &&
		(pExpired != NULL) && (pAge != NULL) && (pSecret != NULL),
		"TLS resume boundary object creation failed");

	testTlsServerResumeFallback(
		&Env, pClient, NULL,
		"TLS resume cache miss did not fall back", UINT32_C(0x101)
	);
	testTlsServerResumeFallback(
		&Env, pClient, pTicket,
		"TLS resume ticket mismatch did not fall back", UINT32_C(0x102)
	);
	testTlsServerResumeFallback(
		&Env, pClient, pName,
		"TLS resume SNI mismatch did not fall back", UINT32_C(0x103)
	);
	testTlsServerResumeFallback(
		&Env, pClient, pProtocol,
		"TLS resume ALPN mismatch did not fall back", UINT32_C(0x104)
	);
	testTlsServerResumeFallback(
		&Env, pClient, pExpired,
		"TLS expired resume object did not fall back", UINT32_C(0x105)
	);
	testTlsServerResumeFallback(
		&Env, pClient, pAge,
		"TLS resume age mismatch did not fall back", UINT32_C(0x106)
	);
	testTlsServerResumeBadBinder(&Env, pClient, pSecret);
	testTlsServerResumeMalformed(
		&Env, pClient, TEST_TLS_SERVER_RESUME_MISSING_MODE,
		"TLS PSK without key-exchange modes was accepted"
	);
	testTlsServerResumeMalformed(
		&Env, pClient, TEST_TLS_SERVER_RESUME_MALFORMED_PSK,
		"TLS malformed PSK vectors were accepted"
	);
	testTlsServerResumeMalformed(
		&Env, pClient, TEST_TLS_SERVER_RESUME_NON_FINAL_PSK,
		"TLS non-final PSK extension was accepted"
	);

	xrtTlsResumeRelease(pSecret);
	xrtTlsResumeRelease(pAge);
	xrtTlsResumeRelease(pExpired);
	xrtTlsResumeRelease(pProtocol);
	xrtTlsResumeRelease(pName);
	xrtTlsResumeRelease(pTicket);
	xrtTlsResumeRelease(pClient);
	testTlsServerResumeEnvUnit(&Env);
	return 0;
}
