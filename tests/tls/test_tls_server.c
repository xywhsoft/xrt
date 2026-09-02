#include "../fixtures/tls_server.h"



/* SNI/ALPN 选择器状态记录真实回调输入。 */
typedef struct test_tls_server_select {
	bool Called;
	bool Name;
	bool Protocols;
	uint64 Cookie;
} test_tls_server_select;



/* 票据查找状态只持有测试所有者的一份服务端恢复对象。 */
typedef struct test_tls_server_resume {
	const xtlsresume* Resume;
	bool Called;
	bool Name;
} test_tls_server_resume;



/* 动态选择器确认借用请求，并选择配置中的 http/1.1。 */
static bool testTlsServerSelect(
	ptr pContext,
	const xtlsserverrequest* pRequest,
	xtlsserverchoice* pChoice
)
{
	test_tls_server_select* pState =
		(test_tls_server_select*)pContext;
	xbytesview Http11 = XRT_BYTES_LITERAL("http/1.1");
	xtlsitemresult ProtocolResult;

	if ( (pState == NULL) || (pRequest == NULL) ||
		(pChoice == NULL) || (pChoice->Identity == NULL) ) {
		return false;
	}
	pState->Called = true;
	pState->Name = (pRequest->ServerName.Size == 11u) &&
		(memcmp(pRequest->ServerName.Data, "example.com", 11u) == 0);
	ProtocolResult = xrtTlsProtocolFind(pRequest->Protocols, Http11);
	pState->Protocols = ProtocolResult == XTLS_ITEM_VALUE;
	pChoice->Protocol = 1u;
	pChoice->Cookie = pState->Cookie;
	if ( !pState->Name || !pState->Protocols ) {
		fprintf(
			stderr,
			"[TLS] selector name=%llu protocols=%llu match=%llu result=%d\n",
			(unsigned long long)pRequest->ServerName.Size,
			(unsigned long long)pRequest->Protocols.Size,
			(unsigned long long)Http11.Size, (int)ProtocolResult
		);
	}
	return pState->Name && pState->Protocols;
}



/* 按不透明票据精确查找恢复对象，并记录第二次 ClientHello 的路由输入。 */
static const xtlsresume* testTlsServerResume(
	ptr pContext,
	const xtlsserverresumerequest* pRequest
)
{
	test_tls_server_resume* pState =
		(test_tls_server_resume*)pContext;
	xtlsresumeinfo Info;

	if ( (pState == NULL) || (pRequest == NULL) ||
		(pState->Resume == NULL) ||
		!xrtTlsResumeInfo(pState->Resume, &Info) ) {
		return NULL;
	}
	pState->Called = true;
	pState->Name = (pRequest->ServerName.Size == 11u) &&
		(memcmp(
			pRequest->ServerName.Data, "example.com", 11u
		) == 0);
	return testTlsServerViewEqual(
		pRequest->Ticket, Info.Ticket
	) ? pState->Resume : NULL;
}



/* 验证服务端请求第二密钥组时，双方通过一次 HRR 完成真实握手。 */
static void testTlsServerHelloRetryRequest(
	const xtlsidentity* pIdentity,
	const xtlsverifier* pVerifier
)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_13 };
	static const xtlscipher Ciphers[] = {
		XTLS_AES_128_GCM_SHA256
	};
	static const uint16 ClientGroups[] = {
		XTLS_GROUP_X25519,
		XTLS_GROUP_SECP256R1
	};
	static const uint16 ServerGroups[] = {
		XTLS_GROUP_SECP256R1,
		XTLS_GROUP_X25519
	};
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	};
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	static const char Payload[] = "hello-retry-request application data";
	xtlspolicy ClientPolicy;
	xtlspolicy ServerPolicy;
	xtlscontextconfig ContextConfig;
	xtlscontext* pClientContext;
	xtlscontext* pServerContext;
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;
	xtlssession* pClient;
	xtlssession* pServer;
	test_tls_server_rng Rng = { UINT32_C(0x71D30A5B) };
	uint64 iCookie = UINT64_MAX;

	xrtTlsPolicyInit(&ClientPolicy);
	ClientPolicy.Versions = Versions;
	ClientPolicy.VersionCount = sizeof(Versions) / sizeof(Versions[0]);
	ClientPolicy.Ciphers = Ciphers;
	ClientPolicy.CipherCount = sizeof(Ciphers) / sizeof(Ciphers[0]);
	ClientPolicy.Groups = ClientGroups;
	ClientPolicy.GroupCount = sizeof(ClientGroups) / sizeof(ClientGroups[0]);
	ClientPolicy.Signatures = Signatures;
	ClientPolicy.SignatureCount =
		sizeof(Signatures) / sizeof(Signatures[0]);
	xrtTlsPolicyInit(&ServerPolicy);
	ServerPolicy.Versions = Versions;
	ServerPolicy.VersionCount = sizeof(Versions) / sizeof(Versions[0]);
	ServerPolicy.Ciphers = Ciphers;
	ServerPolicy.CipherCount = sizeof(Ciphers) / sizeof(Ciphers[0]);
	ServerPolicy.Groups = ServerGroups;
	ServerPolicy.GroupCount = sizeof(ServerGroups) / sizeof(ServerGroups[0]);
	ServerPolicy.Signatures = Signatures;
	ServerPolicy.SignatureCount =
		sizeof(Signatures) / sizeof(Signatures[0]);
	ServerPolicy.KeySharePolicy = XTLS_KEY_SHARE_PREFER_GROUP;

	xrtTlsContextConfigInit(&ContextConfig);
	ContextConfig.Policy = &ClientPolicy;
	ContextConfig.Limits.RecordBudget = 4u;
	ContextConfig.Limits.HandshakeBudget = 4u;
	pClientContext = xrtTlsContextCreate(&ContextConfig);
	ContextConfig.Policy = &ServerPolicy;
	pServerContext = xrtTlsContextCreate(&ContextConfig);
	testRequire((pClientContext != NULL) && (pServerContext != NULL),
		"TLS HRR contexts creation failed");

	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pClientContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ClientConfig.Verifier = pVerifier;
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Context = pServerContext;
	ServerConfig.Identity = pIdentity;
	ServerConfig.Protocols = Protocols;
	ServerConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ServerConfig.RequireProtocol = true;
	pClient = xrtTlsClientCreate(&ClientConfig, NULL);
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	testRequire((pClient != NULL) && (pServer != NULL) &&
		testTlsServerHandshake(pClient, pServer, &Rng),
		"TLS HelloRetryRequest handshake failed");
	testRequire(xrtTlsServerCookie(pServer, &iCookie) && (iCookie == 0),
		"TLS server default selector Cookie is not zero");
	testRequire((xrtTlsSessionVersion(pClient) == XTLS_VERSION_13) &&
		(xrtTlsSessionVersion(pServer) == XTLS_VERSION_13) &&
		testTlsServerTransfer(
			pClient, pServer, true,
			Payload, sizeof(Payload) - 1u, &Rng
		), "TLS HelloRetryRequest application epoch failed");
	testRequire(testTlsServerClose(pClient, pServer, &Rng),
		"TLS HelloRetryRequest close_notify exchange failed");

	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtTlsContextRelease(pServerContext);
	xrtTlsContextRelease(pClientContext);
}



/* 验证真实客户端与服务端的证书握手、更新 epoch、数据和认证关闭。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("h2"),
		XRT_STR_INIT("http/1.1")
	};
	static const char ClientData[] =
		"client-to-server fragmented application payload";
	static const char ServerData[] =
		"server-to-client fragmented application payload";
	xtlscontext* pContext = testTlsServerContext();
	xtlsidentity* pIdentity = testTlsServerIdentity();
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier;
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;
	test_tls_server_select Select = {
		false, false, false, UINT64_C(0xD15EA5E5C001C0DE)
	};
	test_tls_server_resume Resume = { NULL, false, false };
	test_tls_server_rng Rng = { UINT32_C(0x12345678) };
	xtlssession* pClient;
	xtlssession* pServer;
	xtlsresume* pServerResume = NULL;
	xtlsresume* pClientResume = NULL;
	xtlsresumeinfo ServerResumeInfo;
	xtlsresumeinfo ClientResumeInfo;
	xbytesview Name;
	xbytesview ClientProtocol;
	xbytesview ServerProtocol;
	uint64 iCookie = UINT64_MAX;

	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS server fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL, "TLS client verifier creation failed");
	testTlsServerHelloRetryRequest(pIdentity, pVerifier);
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
	ServerConfig.Select = testTlsServerSelect;
	ServerConfig.SelectContext = &Select;
	ServerConfig.RequireProtocol = true;
	pClient = xrtTlsClientCreate(&ClientConfig, NULL);
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	testRequire((pClient != NULL) && (pServer != NULL),
		"TLS client or server session creation failed");
	testRequire(xrtTlsServerCookie(pServer, &iCookie) && (iCookie == 0),
		"TLS server Cookie is not zero before selection");
	testRequire(testTlsServerHandshake(pClient, pServer, &Rng),
		"TLS client/server fragmented handshake failed");
	testRequire((xrtTlsSessionVersion(pClient) == XTLS_VERSION_13) &&
		(xrtTlsSessionVersion(pServer) == XTLS_VERSION_13) &&
		(xrtTlsSessionCipher(pClient) == XTLS_AES_128_GCM_SHA256) &&
		(xrtTlsSessionCipher(pServer) == XTLS_AES_128_GCM_SHA256),
		"TLS 1.3 negotiated snapshot differs");
	testRequire(Select.Called && Select.Name && Select.Protocols,
		"TLS server selector did not receive the expected request");
	testRequire(xrtTlsServerCookie(pServer, &iCookie) &&
		(iCookie == Select.Cookie),
		"TLS server did not retain the selector Cookie");
	testRequire(xrtTlsServerName(pServer, &Name) &&
		(Name.Size == 11u) &&
		(memcmp(Name.Data, "example.com", 11u) == 0),
		"TLS server did not retain the negotiated SNI");
	testRequire(xrtTlsSessionProtocol(pClient, &ClientProtocol) &&
		xrtTlsSessionProtocol(pServer, &ServerProtocol) &&
		(ClientProtocol.Size == 8u) &&
		testTlsServerViewEqual(ClientProtocol, ServerProtocol) &&
		(memcmp(ClientProtocol.Data, "http/1.1", 8u) == 0),
		"TLS client/server ALPN selection differs");
	testRequire(testTlsServerTransfer(
		pClient, pServer, true,
		ClientData, sizeof(ClientData) - 1u, &Rng
	), "TLS client-to-server data transfer failed");
	testRequire(testTlsServerTransfer(
		pServer, pClient, false,
		ServerData, sizeof(ServerData) - 1u, &Rng
	), "TLS server-to-client data transfer failed");
	testRequire((xrtTlsClientKeyUpdate(
		pClient, XTLS_KEY_UPDATE_REQUESTED
	) == XTLS_OK) && testTlsServerPostHandshake(
		pClient, pServer, &Rng
	), "TLS client-requested KeyUpdate exchange failed");
	testRequire(testTlsServerTransfer(
		pClient, pServer, true,
		ClientData, sizeof(ClientData) - 1u, &Rng
	) && testTlsServerTransfer(
		pServer, pClient, false,
		ServerData, sizeof(ServerData) - 1u, &Rng
	), "TLS data transfer after client KeyUpdate failed");
	testRequire((xrtTlsServerKeyUpdate(
		pServer, XTLS_KEY_UPDATE_REQUESTED
	) == XTLS_OK) && testTlsServerPostHandshake(
		pClient, pServer, &Rng
	), "TLS server-requested KeyUpdate exchange failed");
	testRequire(testTlsServerTransfer(
		pClient, pServer, true,
		ClientData, sizeof(ClientData) - 1u, &Rng
	) && testTlsServerTransfer(
		pServer, pClient, false,
		ServerData, sizeof(ServerData) - 1u, &Rng
	), "TLS data transfer after server KeyUpdate failed");
	testRequire((xrtTlsServerTicketNew(
		pServer, &pServerResume
	) == XTLS_OK) && (pServerResume != NULL) &&
		testTlsServerPostHandshake(pClient, pServer, &Rng) &&
		(xrtTlsClientResumeCount(pClient) == 1u),
		"TLS server ticket issuance or client publication failed");
	pClientResume = xrtTlsClientTakeResume(pClient);
	testRequire((pClientResume != NULL) &&
		xrtTlsResumeInfo(pServerResume, &ServerResumeInfo) &&
		xrtTlsResumeInfo(pClientResume, &ClientResumeInfo) &&
		testTlsServerViewEqual(
			ServerResumeInfo.Ticket, ClientResumeInfo.Ticket
		) && testTlsServerViewEqual(
			ServerResumeInfo.Secret, ClientResumeInfo.Secret
		), "TLS client and server stored different ticket material");
	testRequire(testTlsServerClose(pClient, pServer, &Rng),
		"TLS client/server close_notify exchange failed");
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);

	/* 第二条连接只借用缓存对象，服务端核心负责全部绑定与 binder 验证。 */
	Resume.Resume = pServerResume;
	Select = (test_tls_server_select) {
		false, false, false, UINT64_C(0xD15EA5E5C001C0DE)
	};
	ClientConfig.Resume = pClientResume;
	ServerConfig.Resume = testTlsServerResume;
	ServerConfig.ResumeContext = &Resume;
	pClient = xrtTlsClientCreate(&ClientConfig, NULL);
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	xrtTlsResumeRelease(pClientResume);
	pClientResume = NULL;
	testRequire((pClient != NULL) && (pServer != NULL) &&
		testTlsServerHandshake(pClient, pServer, &Rng),
		"TLS resumed client/server handshake failed");
	testRequire(xrtTlsServerCookie(pServer, &iCookie) &&
		(iCookie == Select.Cookie),
		"TLS resumed server did not retain the selector Cookie");
	testRequire(Resume.Called && Resume.Name &&
		xrtTlsClientResumed(pClient) && xrtTlsServerResumed(pServer),
		"TLS server did not accept the stored ticket");
	testRequire(testTlsServerTransfer(
		pClient, pServer, true,
		ClientData, sizeof(ClientData) - 1u, &Rng
	) && testTlsServerTransfer(
		pServer, pClient, false,
		ServerData, sizeof(ServerData) - 1u, &Rng
	), "TLS resumed application epochs failed");
	testRequire(testTlsServerClose(pClient, pServer, &Rng),
		"TLS resumed close_notify exchange failed");

	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtTlsResumeRelease(pServerResume);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	return 0;
}
