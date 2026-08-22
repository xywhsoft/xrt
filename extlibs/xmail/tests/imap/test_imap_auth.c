#include "../test.h"



typedef struct testimapauthserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} testimapauthserver;



static xnetaddr TestImapAuthAddress;



/* 把测试域名解析到本地 IMAP 认证服务器。 */
static xnetaddrlist* testImapAuthResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap-auth.test") != 0 ) {
		return NULL;
	}
	Address = TestImapAuthAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 发送认证测试服务器的一段完整响应。 */
static bool testImapAuthSend(
	xnetstream* pStream,
	cstr sText,
	size_t iSize,
	xdeadline iDeadline
)
{
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pStream, sText, iSize);

		if ( Result == XNET_RESULT_OK ) {
			return true;
		}
		if ( (Result != XNET_RESULT_AGAIN) || !xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_WRITE,
			iDeadline,
			NULL
		) ) {
			return false;
		}
	}
}



/* 精确接收并比较认证测试客户端字节。 */
static bool testImapAuthReceive(
	xnetstream* pStream,
	cstr sExpected,
	size_t iExpected,
	xdeadline iDeadline
)
{
	size_t iReceived = 0;

	while ( iReceived < iExpected ) {
		xnetbytes* pBytes = xrtNetStreamRecv(
			pStream,
			1u,
			iDeadline,
			NULL
		);
		xbytesview Bytes;

		if ( pBytes == NULL ) {
			fprintf(
				stderr,
				"[TRANSCRIPT] receive stopped at %zu/%zu\n",
				iReceived,
				iExpected
			);
			return false;
		}
		Bytes = xrtNetBytesView(pBytes);
		if ( (Bytes.Size == 0) ||
			(memcmp(Bytes.Data, sExpected + iReceived, Bytes.Size) != 0) ) {
			fprintf(
				stderr,
				"[TRANSCRIPT] mismatch at %zu: expected 0x%02X, got 0x%02X\n",
				iReceived,
				(unsigned int)(unsigned char)sExpected[iReceived],
				Bytes.Size != 0 ?
					(unsigned int)(unsigned char)Bytes.Data[0] : 0u
			);
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iReceived += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}



/* 完成固定 tag 的 CAPABILITY 交换。 */
static bool testImapAuthCapability(
	xnetstream* pStream,
	cstr sCapabilities,
	size_t iCapabilities,
	xdeadline iDeadline
)
{
	return testImapAuthReceive(
		pStream,
		"A00000001 CAPABILITY\r\n",
		sizeof("A00000001 CAPABILITY\r\n") - 1u,
		iDeadline
	) && testImapAuthSend(
		pStream,
		sCapabilities,
		iCapabilities,
		iDeadline
	);
}



/* 完成固定 tag 的 LOGOUT 交换并关闭一条连接。 */
static bool testImapAuthLogout(
	xnetstream* pStream,
	xdeadline iDeadline
)
{
	return testImapAuthReceive(
		pStream,
		"A00000003 LOGOUT\r\n",
		sizeof("A00000003 LOGOUT\r\n") - 1u,
		iDeadline
	) && testImapAuthSend(
		pStream,
		"* BYE signing off\r\nA00000003 OK logout complete\r\n",
		sizeof(
			"* BYE signing off\r\nA00000003 OK logout complete\r\n"
		) - 1u,
		iDeadline
	) && xrtNetStreamClose(pStream) && xrtNetStreamWait(
		pStream,
		XNET_STREAM_WAIT_CLOSE,
		iDeadline,
		NULL
	);
}



/* 验证引号转义后的传统 LOGIN。 */
static bool testImapAuthLoginSession(
	xnetstream* pStream,
	xdeadline iDeadline
)
{
	static const char sCapabilities[] =
		"* CAPABILITY IMAP4rev1\r\n"
		"A00000001 OK capability complete\r\n";

	return testImapAuthCapability(
		pStream,
		sCapabilities,
		sizeof(sCapabilities) - 1u,
		iDeadline
	) && testImapAuthReceive(
		pStream,
		"A00000002 LOGIN \"u\\\"ser\" \"p\\\\ass\"\r\n",
		sizeof("A00000002 LOGIN \"u\\\"ser\" \"p\\\\ass\"\r\n") - 1u,
		iDeadline
	) && testImapAuthSend(
		pStream,
		"A00000002 OK login complete\r\n",
		sizeof("A00000002 OK login complete\r\n") - 1u,
		iDeadline
	) && testImapAuthLogout(pStream, iDeadline);
}



/* 验证没有 SASL-IR 时的 PLAIN continuation。 */
static bool testImapAuthPlainSession(
	xnetstream* pStream,
	xdeadline iDeadline
)
{
	static const char sCapabilities[] =
		"* CAPABILITY IMAP4rev1 AUTH=PLAIN\r\n"
		"A00000001 OK capability complete\r\n";

	return testImapAuthCapability(
		pStream,
		sCapabilities,
		sizeof(sCapabilities) - 1u,
		iDeadline
	) && testImapAuthReceive(
		pStream,
		"A00000002 AUTHENTICATE PLAIN\r\n",
		sizeof("A00000002 AUTHENTICATE PLAIN\r\n") - 1u,
		iDeadline
	) && testImapAuthSend(
		pStream,
		"+ \r\n",
		sizeof("+ \r\n") - 1u,
		iDeadline
	) && testImapAuthReceive(
		pStream,
		"AHVzZXIAcGFzcw==\r\n",
		sizeof("AHVzZXIAcGFzcw==\r\n") - 1u,
		iDeadline
	) && testImapAuthSend(
		pStream,
		"A00000002 OK PLAIN complete\r\n",
		sizeof("A00000002 OK PLAIN complete\r\n") - 1u,
		iDeadline
	) && testImapAuthLogout(pStream, iDeadline);
}



/* 验证 XOAUTH2 SASL-IR 失败 challenge 必须以空行收尾。 */
static bool testImapAuthXoauth2Session(
	xnetstream* pStream,
	xdeadline iDeadline
)
{
	static const char sCapabilities[] =
		"* CAPABILITY IMAP4rev1 AUTH=XOAUTH2 SASL-IR\r\n"
		"A00000001 OK capability complete\r\n";

	return testImapAuthCapability(
		pStream,
		sCapabilities,
		sizeof(sCapabilities) - 1u,
		iDeadline
	) && testImapAuthReceive(
		pStream,
		"A00000002 AUTHENTICATE XOAUTH2 "
		"dXNlcj11c2VyAWF1dGg9QmVhcmVyIHRva2VuAQE=\r\n",
		sizeof(
			"A00000002 AUTHENTICATE XOAUTH2 "
			"dXNlcj11c2VyAWF1dGg9QmVhcmVyIHRva2VuAQE=\r\n"
		) - 1u,
		iDeadline
	) && testImapAuthSend(
		pStream,
		"+ eyJzdGF0dXMiOiI0MDEifQ==\r\n",
		sizeof("+ eyJzdGF0dXMiOiI0MDEifQ==\r\n") - 1u,
		iDeadline
	) && testImapAuthReceive(
		pStream,
		"\r\n",
		2u,
		iDeadline
	) && testImapAuthSend(
		pStream,
		"A00000002 NO invalid token\r\n",
		sizeof("A00000002 NO invalid token\r\n") - 1u,
		iDeadline
	) && testImapAuthLogout(pStream, iDeadline);
}



/* 依次服务 LOGIN、PLAIN 和 XOAUTH2 三条真实连接。 */
static int32 testImapAuthServer(ptr pData)
{
	testimapauthserver* pServer = (testimapauthserver*)pData;
	bool bSuccess = true;

	for ( size_t i = 0; i < 3u; i++ ) {
		xnetstream* pStream = xrtNetListenerAcceptWait(
			pServer->Listener,
			pServer->Deadline,
			NULL
		);

		if ( pStream == NULL ) {
			bSuccess = false;
			break;
		}
		bSuccess = testImapAuthSend(
			pStream,
			"* OK imap-auth.test ready\r\n",
			sizeof("* OK imap-auth.test ready\r\n") - 1u,
			pServer->Deadline
		) && (i == 0 ? testImapAuthLoginSession(
			pStream,
			pServer->Deadline
		) : (i == 1 ? testImapAuthPlainSession(
			pStream,
			pServer->Deadline
		) : testImapAuthXoauth2Session(
			pStream,
			pServer->Deadline
		)));
		xrtNetStreamDestroy(pStream);
		if ( !bSuccess ) {
			break;
		}
	}
	pServer->Success = bSuccess;
	return bSuccess ? 0 : 1;
}



/* 打开一条共享本地测试连接。 */
static ximapclient* testImapAuthOpen(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	xdeadline iDeadline
)
{
	ximapclientconfig Config;

	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "imap-auth.test";
	Config.Net.Port = TestImapAuthAddress.Port;
	return xrtImapClientOpen(&Config, iDeadline, NULL);
}



/* 验证三种认证机制的线路、安全门和状态转换。 */
int main(void)
{
	static const char sInvalidBearer[] = { 'u', '\x01', 'x' };
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	testimapauthserver Server;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	ximapclient* pClient;
	ximapauthconfig Auth;
	xthread* pThread;
	xdeadline Deadline;

	xrtImapAuthConfigInit(&Auth);
	Auth.Method = XIMAP_AUTH_OAUTHBEARER;
	Auth.Username = XRT_STR_LITERAL("user");
	Auth.AuthorizationId = XRT_STR_LITERAL("u,s=e");
	Auth.Secret = XRT_STR_LITERAL("token");
	testRequire(xrtImapAuthConfigValid(&Auth),
		"IMAP OAUTHBEARER configuration validation failed");
	Auth.AuthorizationId = (xstrview) {
		sInvalidBearer,
		sizeof(sInvalidBearer)
	};
	testRequire(!xrtImapAuthConfigValid(&Auth),
		"IMAP OAUTHBEARER accepted a SASL field separator");
	xrtClearError();

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP auth engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP auth loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapAuthAddress
	), "IMAP auth listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapAuthResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "IMAP auth resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testImapAuthServer, &Server, 0);
	testRequire(pThread != NULL, "IMAP auth server thread creation failed");

	pClient = testImapAuthOpen(pEngine, pResolver, Deadline);
	testRequire(pClient != NULL, "IMAP LOGIN client open failed");
	xrtImapAuthConfigInit(&Auth);
	Auth.Method = XIMAP_AUTH_LOGIN;
	Auth.Username = XRT_STR_LITERAL("u\"ser");
	Auth.Secret = XRT_STR_LITERAL("p\\ass");
	testRequire(!xrtImapClientAuth(
		pClient,
		&Auth,
		Deadline,
		NULL
	), "IMAP LOGIN sent plaintext credentials without opt-in");
	xrtClearError();
	Auth.AllowPlaintext = true;
	testRequire(xrtImapClientAuth(pClient, &Auth, Deadline, NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
		"IMAP LOGIN authentication failed");
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL),
		"IMAP LOGIN logout failed");
	xrtImapClientDestroy(pClient);

	pClient = testImapAuthOpen(pEngine, pResolver, Deadline);
	testRequire(pClient != NULL, "IMAP PLAIN client open failed");
	xrtImapAuthConfigInit(&Auth);
	Auth.Username = XRT_STR_LITERAL("user");
	Auth.Secret = XRT_STR_LITERAL("pass");
	Auth.AllowPlaintext = true;
	testRequire(xrtImapClientAuth(pClient, &Auth, Deadline, NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
		"IMAP PLAIN authentication failed");
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL),
		"IMAP PLAIN logout failed");
	xrtImapClientDestroy(pClient);

	pClient = testImapAuthOpen(pEngine, pResolver, Deadline);
	testRequire(pClient != NULL, "IMAP XOAUTH2 client open failed");
	xrtImapAuthConfigInit(&Auth);
	Auth.Method = XIMAP_AUTH_OAUTHBEARER;
	Auth.Username = XRT_STR_LITERAL("user");
	Auth.Secret = XRT_STR_LITERAL("token");
	Auth.AllowPlaintext = true;
	testRequire(!xrtImapClientAuth(pClient, &Auth, Deadline, NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_NOT_AUTHENTICATED),
		"IMAP OAUTHBEARER was allowed on a plaintext connection");
	xrtClearError();
	Auth.Method = XIMAP_AUTH_XOAUTH2;
	testRequire(!xrtImapClientAuth(pClient, &Auth, Deadline, NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_NOT_AUTHENTICATED),
		"IMAP XOAUTH2 rejection did not preserve reusable state");
	xrtClearError();
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL),
		"IMAP XOAUTH2 rejection logout failed");
	xrtImapClientDestroy(pClient);

	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP auth server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP auth server transcript mismatch");
	xrtThreadDestroy(pThread);
	testRequire(xrtNetListenerClose(pListener),
		"IMAP auth listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP auth resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP auth engine destroy failed");
	return 0;
}
