#include "../fixtures/tls_server.h"



/* TLS 1.2 与 TLS 1.3 使用同一个选择器 Cookie 契约。 */
static bool testTlsServer12Select(
	ptr pContext,
	const xtlsserverrequest* pRequest,
	xtlsserverchoice* pChoice
)
{
	(void)pContext;
	if ( (pRequest == NULL) || (pChoice == NULL) ||
		(pChoice->Identity == NULL) ) {
		return false;
	}
	pChoice->Cookie = UINT64_C(0x12C0011E12C0011E);
	return true;
}



/* 创建只开放 TLS 1.2 ECDHE-RSA-AES-GCM 的测试上下文。 */
static xtlscontext* testTlsServer12Context(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_12 };
	static const xtlscipher Ciphers[] = {
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256
	};
	static const uint16 Groups[] = { XTLS_GROUP_X25519 };
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		XTLS_SIGNATURE_RSA_PKCS1_SHA256
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
	Config.Limits.RecordBudget = 4u;
	Config.Limits.HandshakeBudget = 4u;
	return xrtTlsContextCreate(&Config);
}



/* 修改首条 Hello 的扩展，但保持外层长度正确，验证角色层的初始绑定检查。 */
static void testTlsServer12Renegotiation(
	const xtlsclientconfig* pClientConfig,
	const xtlsserverconfig* pServerConfig
)
{
	for ( size_t i = 0; i < 3u; i++ ) {
		xtlssession* pClient = xrtTlsClientCreate(pClientConfig, NULL);
		xtlssession* pServer = xrtTlsServerCreate(pServerConfig, NULL);
		xtlssession* pSource = i == 0 ? pClient : pServer;
		xtlssession* pTarget = i == 0 ? pServer : pClient;
		xnetspan Span;
		xtlsrecord Record;
		xtlshandshake Message;
		xtlsclienthello ClientHello;
		xtlsserverhello ServerHello;
		xtlsextensioncursor Cursor;
		xtlsextension Extension;
		xtlsitemresult Result;
		xtlswriter Writer;
		xbytesview Extensions;
		uint8 Storage[1024];
		uint8 Body[2048];
		uint8 Wire[2057];
		static const uint8 Binding[] = { 1u, 0xA5u };
		size_t iRequired;
		size_t iSize;
		bool bFound = false;

		testRequire((pClient != NULL) && (pServer != NULL),
			"TLS 1.2 binding roles creation failed");
		if ( i != 0 ) {
			testRequire(xrtTlsSessionSendFront(pClient, &Span) &&
				(xrtTlsSessionFeed(pServer, Span.Data, Span.Size) == XTLS_OK) &&
				xrtTlsSessionSendConsume(pClient, Span.Size) &&
				(xrtTlsServerDrive(pServer) == XTLS_OK),
				"TLS 1.2 binding ServerHello setup failed");
		}
		testRequire(xrtTlsSessionSendFront(pSource, &Span) &&
			(xrtTlsRecordParse((xbytesview) { Span.Data, Span.Size },
				&Record, &iRequired) == XTLS_OK) &&
			(xrtTlsHandshakeParse(Record.Payload, &Message, &iRequired) == XTLS_OK),
			"TLS 1.2 binding Hello parsing failed");
		if ( i == 0 ) {
			testRequire(xrtTlsClientHelloParse(Message.Body, &ClientHello),
				"TLS 1.2 binding ClientHello parsing failed");
			Extensions = ClientHello.Extensions;
		} else {
			testRequire(xrtTlsServerHelloParse(Message.Body, &ServerHello),
				"TLS 1.2 binding ServerHello parsing failed");
			Extensions = ServerHello.Extensions;
		}
		testRequire(xrtTlsExtensionsInit(&Cursor, Extensions) &&
			xrtTlsWriterInit(&Writer, Storage, sizeof(Storage)),
			"TLS 1.2 binding extension setup failed");
		while ( (Result = xrtTlsExtensionsRead(&Cursor, &Extension)) == XTLS_ITEM_VALUE ) {
			if ( Extension.Type == XTLS_EXTENSION_RENEGOTIATION_INFO ) {
				bFound = true;
				testRequire((Extension.Data.Size == 1u) && (Extension.Data.Data[0] == 0),
					"TLS 1.2 did not send an empty initial binding");
				if ( i == 2u ) {
					continue;
				}
				Extension.Data = (xbytesview) { Binding, sizeof(Binding) };
			}
			testRequire(xrtTlsWriterExtension(&Writer, Extension.Type, Extension.Data),
				"TLS 1.2 binding extension encoding failed");
		}
		testRequire(bFound && (Result == XTLS_ITEM_DONE),
			"TLS 1.2 initial binding extension is missing");
		Extensions = (xbytesview) { Storage, Writer.Size };
		if ( i == 0 ) {
			ClientHello.Extensions = Extensions;
			iSize = xrtTlsClientHelloSize(&ClientHello);
			testRequire(xrtTlsClientHelloEncode(&ClientHello, Body, sizeof(Body)),
				"TLS 1.2 binding ClientHello encoding failed");
		} else {
			ServerHello.Extensions = Extensions;
			iSize = xrtTlsServerHelloSize(&ServerHello);
			testRequire(xrtTlsServerHelloEncode(&ServerHello, Body, sizeof(Body)),
				"TLS 1.2 binding ServerHello encoding failed");
		}
		testRequire(xrtTlsHandshakeEncode(Message.Type,
			(xbytesview) { Body, iSize }, Wire + XTLS_RECORD_HEADER_SIZE,
			sizeof(Wire) - XTLS_RECORD_HEADER_SIZE) &&
			xrtTlsRecordEncode(XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
				(xbytesview) { Wire + XTLS_RECORD_HEADER_SIZE, xrtTlsHandshakeSize(iSize) },
				Wire, sizeof(Wire)) &&
			(xrtTlsSessionFeed(pTarget, Wire,
				xrtTlsRecordSize(xrtTlsHandshakeSize(iSize))) == XTLS_OK) &&
			((i == 0 ? xrtTlsServerDrive(pTarget) : xrtTlsClientDrive(pTarget)) == XTLS_ERROR) &&
			(xrtTlsSessionState(pTarget) == XTLS_STATE_FAILED) &&
			(xrtErrorCode(xrtGetError()) == XTLS_ERROR_EXTENSION),
			"TLS 1.2 accepted a nonempty or missing initial binding");
		xrtTlsSessionDestroy(pServer);
		xrtTlsSessionDestroy(pClient);
		xrtClearError();
	}
}



/* 验证 TLS 1.2 完整握手、协商快照、数据、版本边界和认证关闭。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("h2"),
		XRT_STR_INIT("http/1.1")
	};
	static const char ClientData[] =
		"TLS 1.2 client fragmented application payload";
	static const char ServerData[] =
		"TLS 1.2 server fragmented application payload";
	static const uint8 TicketData[] = { 0x12u, 0x34u, 0x56u, 0x78u };
	xtlscontext* pContext = testTlsServer12Context();
	xtlsidentity* pIdentity = testTlsServerIdentity();
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier;
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;
	test_tls_server_rng Rng = { UINT32_C(0xD4C3B2A1) };
	xtlssession* pClient;
	xtlssession* pServer;
	xtlsresume* pResume = NULL;
	xbytesview Name;
	xbytesview ClientProtocol;
	xbytesview ServerProtocol;
	uint64 iCookie = 0;

	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS 1.2 fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL, "TLS 1.2 verifier creation failed");

	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ClientConfig.Verifier = pVerifier;
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Context = pContext;
	ServerConfig.Identity = pIdentity;
	ServerConfig.Protocols = Protocols;
	ServerConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ServerConfig.Select = testTlsServer12Select;
	ServerConfig.RequireProtocol = true;
	testTlsServer12Renegotiation(&ClientConfig, &ServerConfig);
	pClient = xrtTlsClientCreate(&ClientConfig, NULL);
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	testRequire((pClient != NULL) && (pServer != NULL),
		"TLS 1.2 client or server creation failed");
	testRequire(testTlsServerHandshake(pClient, pServer, &Rng),
		"TLS 1.2 fragmented handshake failed");

	testRequire((xrtTlsSessionVersion(pClient) == XTLS_VERSION_12) &&
		(xrtTlsSessionVersion(pServer) == XTLS_VERSION_12) &&
		(xrtTlsSessionCipher(pClient) ==
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256) &&
		(xrtTlsSessionCipher(pServer) ==
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256),
		"TLS 1.2 negotiated snapshot differs");
	testRequire((xrtTlsClientCertificateCount(pClient) == 1u) &&
		xrtTlsServerName(pServer, &Name) && (Name.Size == 11u) &&
		(memcmp(Name.Data, "example.com", 11u) == 0),
		"TLS 1.2 certificate or SNI state differs");
	testRequire(xrtTlsServerCookie(pServer, &iCookie) &&
		(iCookie == UINT64_C(0x12C0011E12C0011E)),
		"TLS 1.2 server did not retain the selector Cookie");
	testRequire(xrtTlsSessionProtocol(pClient, &ClientProtocol) &&
		xrtTlsSessionProtocol(pServer, &ServerProtocol) &&
		testTlsServerViewEqual(ClientProtocol, ServerProtocol) &&
		(ClientProtocol.Size == 2u) &&
		(memcmp(ClientProtocol.Data, "h2", 2u) == 0),
		"TLS 1.2 ALPN selection differs");

	testRequire(testTlsServerTransfer(
		pClient, pServer, true,
		ClientData, sizeof(ClientData) - 1u, &Rng
	) && testTlsServerTransfer(
		pServer, pClient, false,
		ServerData, sizeof(ServerData) - 1u, &Rng
	), "TLS 1.2 bidirectional transfer failed");
	testRequire((xrtTlsClientKeyUpdate(
		pClient, XTLS_KEY_UPDATE_NOT_REQUESTED
	) == XTLS_ERROR) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtTlsSessionState(pClient) == XTLS_STATE_READY),
		"TLS 1.2 client accepted KeyUpdate or damaged the session");
	testRequire((xrtTlsServerKeyUpdate(
		pServer, XTLS_KEY_UPDATE_NOT_REQUESTED
	) == XTLS_ERROR) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_READY),
		"TLS 1.2 server accepted KeyUpdate or damaged the session");
	testRequire((xrtTlsServerTicket(
		pServer,
		(xbytesview) { TicketData, sizeof(TicketData) }, 60u, &pResume
	) == XTLS_ERROR) && (pResume == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_READY),
		"TLS 1.2 server accepted a TLS 1.3 ticket");
	testRequire(testTlsServerTransfer(
		pClient, pServer, true,
		ClientData, sizeof(ClientData) - 1u, &Rng
	) && testTlsServerTransfer(
		pServer, pClient, false,
		ServerData, sizeof(ServerData) - 1u, &Rng
	), "TLS 1.2 transfer failed after rejected TLS 1.3 operations");
	testRequire(testTlsServerClose(pClient, pServer, &Rng),
		"TLS 1.2 close_notify exchange failed");

	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	/* 新增 RI 应答不能挤掉最大 255 字节 ALPN 的 ServerHello 容量。 */
	{
		char LongName[255];
		xstrview LongProtocol = { LongName, sizeof(LongName) };

		memset(LongName, 'x', sizeof(LongName));
		ClientConfig.Protocols = &LongProtocol;
		ClientConfig.ProtocolCount = 1;
		ServerConfig.Protocols = &LongProtocol;
		ServerConfig.ProtocolCount = 1;
		pClient = xrtTlsClientCreate(&ClientConfig, NULL);
		pServer = xrtTlsServerCreate(&ServerConfig, NULL);
		testRequire((pClient != NULL) && (pServer != NULL) &&
			testTlsServerHandshake(pClient, pServer, &Rng) &&
			xrtTlsSessionProtocol(pClient, &ClientProtocol) &&
			(ClientProtocol.Size == sizeof(LongName)) &&
			(memcmp(ClientProtocol.Data, LongName, sizeof(LongName)) == 0),
			"TLS 1.2 initial binding broke maximum ALPN size");
		xrtTlsSessionDestroy(pServer);
		xrtTlsSessionDestroy(pClient);
	}
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	return 0;
}
