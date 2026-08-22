#include "../fixtures/tls_server.h"



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
	ServerConfig.RequireProtocol = true;
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
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	return 0;
}
