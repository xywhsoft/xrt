#include "../test.h"



typedef struct testsmtpauthserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} testsmtpauthserver;



static xnetaddr TestSmtpAuthAddress;



/* 把测试域名解析到本地认证服务器。 */
static xnetaddrlist* testSmtpAuthResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "smtp.test") != 0 ) {
		return NULL;
	}
	Address = TestSmtpAuthAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 发送认证测试服务器的一段完整响应。 */
static bool testSmtpAuthSend(
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



/* 精确接收并比较一段认证命令。 */
static bool testSmtpAuthReceive(
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
			iExpected - iReceived,
			iDeadline,
			NULL
		);
		xbytesview Bytes;

		if ( pBytes == NULL ) {
			return false;
		}
		Bytes = xrtNetBytesView(pBytes);
		if ( (Bytes.Size == 0) ||
			(memcmp(Bytes.Data, sExpected + iReceived, Bytes.Size) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iReceived += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}



/* 接收未知内容但长度精确的 SASL 响应，并验证 Base64 行边界。 */
static bool testSmtpAuthReceiveLine(
	xnetstream* pStream,
	size_t iExpected,
	xdeadline iDeadline
)
{
	size_t iReceived = 0;

	while ( iReceived < iExpected ) {
		xnetbytes* pBytes = xrtNetStreamRecv(
			pStream,
			iExpected - iReceived,
			iDeadline,
			NULL
		);
		xbytesview Bytes;

		if ( pBytes == NULL ) {
			return false;
		}
		Bytes = xrtNetBytesView(pBytes);
		if ( Bytes.Size == 0 ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		for ( size_t i = 0; i < Bytes.Size; i++ ) {
			unsigned char iByte = (unsigned char)Bytes.Data[i];

			if ( ((iByte < (unsigned char)'A') ||
				  (iByte > (unsigned char)'Z')) &&
				 ((iByte < (unsigned char)'a') ||
				  (iByte > (unsigned char)'z')) &&
				 ((iByte < (unsigned char)'0') ||
				  (iByte > (unsigned char)'9')) &&
				 (iByte != (unsigned char)'+') &&
				 (iByte != (unsigned char)'/') &&
				 (iByte != (unsigned char)'=') ) {
				xrtNetBytesDestroy(pBytes);
				return false;
			}
		}
		iReceived += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return testSmtpAuthReceive(pStream, "\r\n", 2u, iDeadline);
}



/* 完成每条认证连接共有的 banner 和 EHLO 交换。 */
static bool testSmtpAuthHello(
	xnetstream* pStream,
	cstr sCapabilities,
	size_t iCapabilities,
	xdeadline iDeadline
)
{
	return testSmtpAuthSend(
		pStream,
		"220 smtp.test ready\r\n",
		sizeof("220 smtp.test ready\r\n") - 1u,
		iDeadline
	) && testSmtpAuthReceive(
		pStream,
		"EHLO client.test\r\n",
		sizeof("EHLO client.test\r\n") - 1u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		sCapabilities,
		iCapabilities,
		iDeadline
	);
}



/* 完成一条认证测试连接的 QUIT 与有序关闭。 */
static bool testSmtpAuthQuit(xnetstream* pStream, xdeadline iDeadline)
{
	return testSmtpAuthReceive(
		pStream,
		"QUIT\r\n",
		sizeof("QUIT\r\n") - 1u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		"221 closing\r\n",
		sizeof("221 closing\r\n") - 1u,
		iDeadline
	) && xrtNetStreamClose(pStream) && xrtNetStreamWait(
		pStream,
		XNET_STREAM_WAIT_CLOSE,
		iDeadline,
		NULL
	);
}



/* 验证 SMTP 原生 initial response，不依赖非标准 SASL-IR 能力。 */
static bool testSmtpAuthPlainSession(
	xnetstream* pStream,
	xdeadline iDeadline
)
{
	static const char sCapabilities[] =
		"250-smtp.test\r\n250 AUTH PLAIN\r\n";

	return testSmtpAuthHello(
		pStream,
		sCapabilities,
		sizeof(sCapabilities) - 1u,
		iDeadline
	) && testSmtpAuthReceive(
		pStream,
		"AUTH PLAIN AHVzZXIAcGFzcw==\r\n",
		sizeof("AUTH PLAIN AHVzZXIAcGFzcw==\r\n") - 1u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		"235 PLAIN accepted\r\n",
		sizeof("235 PLAIN accepted\r\n") - 1u,
		iDeadline
	) && testSmtpAuthQuit(pStream, iDeadline);
}



/* 验证 LOGIN 的 username 和 password 两阶段 challenge。 */
static bool testSmtpAuthLoginSession(
	xnetstream* pStream,
	xdeadline iDeadline
)
{
	static const char sCapabilities[] =
		"250-smtp.test\r\n250 AUTH LOGIN\r\n";

	return testSmtpAuthHello(
		pStream,
		sCapabilities,
		sizeof(sCapabilities) - 1u,
		iDeadline
	) && testSmtpAuthReceive(
		pStream,
		"AUTH LOGIN\r\n",
		sizeof("AUTH LOGIN\r\n") - 1u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		"334 VXNlcm5hbWU6\r\n",
		sizeof("334 VXNlcm5hbWU6\r\n") - 1u,
		iDeadline
	) && testSmtpAuthReceive(
		pStream,
		"dXNlcg==\r\n",
		sizeof("dXNlcg==\r\n") - 1u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		"334 UGFzc3dvcmQ6\r\n",
		sizeof("334 UGFzc3dvcmQ6\r\n") - 1u,
		iDeadline
	) && testSmtpAuthReceive(
		pStream,
		"cGFzcw==\r\n",
		sizeof("cGFzcw==\r\n") - 1u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		"235 LOGIN accepted\r\n",
		sizeof("235 LOGIN accepted\r\n") - 1u,
		iDeadline
	) && testSmtpAuthQuit(pStream, iDeadline);
}



/* 验证 XOAUTH2 失败 challenge 以空响应正常收尾。 */
static bool testSmtpAuthXoauth2Session(
	xnetstream* pStream,
	xdeadline iDeadline
)
{
	static const char sCapabilities[] =
		"250-smtp.test\r\n250 AUTH XOAUTH2\r\n";

	return testSmtpAuthHello(
		pStream,
		sCapabilities,
		sizeof(sCapabilities) - 1u,
		iDeadline
	) && testSmtpAuthReceive(
		pStream,
		"AUTH XOAUTH2 dXNlcj11c2VyAWF1dGg9QmVhcmVyIHRva2VuAQE=\r\n",
		sizeof(
			"AUTH XOAUTH2 dXNlcj11c2VyAWF1dGg9QmVhcmVyIHRva2VuAQE=\r\n"
		) - 1u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		"334 eyJzdGF0dXMiOiI0MDEifQ==\r\n",
		sizeof("334 eyJzdGF0dXMiOiI0MDEifQ==\r\n") - 1u,
		iDeadline
	) && testSmtpAuthReceive(
		pStream,
		"\r\n",
		2u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		"535 XOAUTH2 rejected\r\n",
		sizeof("535 XOAUTH2 rejected\r\n") - 1u,
		iDeadline
	) && testSmtpAuthQuit(pStream, iDeadline);
}



/* 验证超出普通命令上限的响应改走 SASL continuation 长行。 */
static bool testSmtpAuthLongSession(
	xnetstream* pStream,
	xdeadline iDeadline
)
{
	static const char sCapabilities[] =
		"250-smtp.test\r\n250 AUTH PLAIN\r\n";

	return testSmtpAuthHello(
		pStream,
		sCapabilities,
		sizeof(sCapabilities) - 1u,
		iDeadline
	) && testSmtpAuthReceive(
		pStream,
		"AUTH PLAIN\r\n",
		sizeof("AUTH PLAIN\r\n") - 1u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		"334 \r\n",
		sizeof("334 \r\n") - 1u,
		iDeadline
	) && testSmtpAuthReceiveLine(
		pStream,
		672u,
		iDeadline
	) && testSmtpAuthSend(
		pStream,
		"235 long PLAIN accepted\r\n",
		sizeof("235 long PLAIN accepted\r\n") - 1u,
		iDeadline
	) && testSmtpAuthQuit(pStream, iDeadline);
}



/* 分别服务成功和失败认证，避免成功后重复 AUTH 的非法会话。 */
static int32 testSmtpAuthServer(ptr pData)
{
	testsmtpauthserver* pServer = (testsmtpauthserver*)pData;
	bool bSuccess = true;

	for ( size_t i = 0; i < 4u; i++ ) {
		xnetstream* pStream = xrtNetListenerAcceptWait(
			pServer->Listener,
			pServer->Deadline,
			NULL
		);

		if ( pStream == NULL ) {
			bSuccess = false;
			break;
		}
		bSuccess = i == 0 ? testSmtpAuthPlainSession(
			pStream,
			pServer->Deadline
		) : (i == 1 ? testSmtpAuthLoginSession(
			pStream,
			pServer->Deadline
		) : (i == 2 ? testSmtpAuthXoauth2Session(
			pStream,
			pServer->Deadline
		) : testSmtpAuthLongSession(
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



/* 验证认证配置、安全门和三种机制的会话行为。 */
int main(void)
{
	static const char sInvalidBearer[] = { 'u', '\x01', 'x' };
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xsmtpclientconfig ClientConfig;
	xsmtpauthconfig AuthConfig;
	testsmtpauthserver Server;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xsmtpclient* pClient;
	xsmtpreply Reply;
	xthread* pThread;
	xdeadline Deadline;
	char sLongSecret[500];

	xrtSmtpAuthConfigInit(&AuthConfig);
	testRequire((AuthConfig.Method == XSMTP_AUTH_PLAIN) &&
		AuthConfig.InitialResponse && !AuthConfig.AllowPlaintext,
		"SMTP auth default configuration mismatch");
	AuthConfig.Username = XRT_STR_LITERAL("user");
	AuthConfig.Secret = XRT_STR_LITERAL("pass");
	testRequire(xrtSmtpAuthConfigValid(&AuthConfig),
		"SMTP PLAIN configuration validation failed");
	AuthConfig.Method = XSMTP_AUTH_OAUTHBEARER;
	AuthConfig.AuthorizationId = XRT_STR_LITERAL("u,s=e");
	AuthConfig.Secret = XRT_STR_LITERAL("token");
	testRequire(xrtSmtpAuthConfigValid(&AuthConfig),
		"SMTP OAUTHBEARER configuration validation failed");
	AuthConfig.AuthorizationId = (xstrview) {
		sInvalidBearer,
		sizeof(sInvalidBearer)
	};
	testRequire(!xrtSmtpAuthConfigValid(&AuthConfig),
		"SMTP OAUTHBEARER accepted a SASL field separator");
	xrtClearError();
	xrtSmtpAuthConfigInit(&AuthConfig);
	AuthConfig.Username = XRT_STR_LITERAL("user");
	AuthConfig.Secret = XRT_STR_LITERAL("pass");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"SMTP auth engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "SMTP auth loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestSmtpAuthAddress
	), "SMTP auth listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testSmtpAuthResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "SMTP auth resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testSmtpAuthServer, &Server, 0);
	testRequire(pThread != NULL, "SMTP auth server thread creation failed");
	xrtSmtpClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "smtp.test";
	ClientConfig.Net.Port = TestSmtpAuthAddress.Port;
	ClientConfig.Hello = (xstrview)XRT_STR_LITERAL("client.test");
	pClient = xrtSmtpClientOpen(&ClientConfig, Deadline, NULL);
	testRequire((pClient != NULL) &&
		(xrtSmtpClientSecurity(pClient) == XMAIL_SECURITY_PLAIN),
		"SMTP auth client open failed");
	testRequire(!xrtSmtpClientAuth(
		pClient,
		&AuthConfig,
		Deadline,
		NULL
	), "SMTP auth sent credentials without plaintext opt-in");
	xrtClearError();
	AuthConfig.AllowPlaintext = true;
	testRequire(xrtSmtpClientAuth(
		pClient,
		&AuthConfig,
		Deadline,
		NULL
	) && xrtSmtpClientAuthenticated(pClient),
		"SMTP PLAIN authentication state mismatch");
	testRequire(!xrtSmtpClientAuth(
		pClient,
		&AuthConfig,
		Deadline,
		NULL
	), "SMTP repeated authentication was accepted");
	xrtClearError();
	testRequire(xrtSmtpClientQuit(pClient, Deadline, NULL),
		"SMTP PLAIN QUIT failed");
	xrtSmtpClientDestroy(pClient);

	pClient = xrtSmtpClientOpen(&ClientConfig, Deadline, NULL);
	testRequire(pClient != NULL, "SMTP LOGIN client open failed");
	xrtSmtpAuthConfigInit(&AuthConfig);
	AuthConfig.Method = XSMTP_AUTH_LOGIN;
	AuthConfig.Username = XRT_STR_LITERAL("user");
	AuthConfig.Secret = XRT_STR_LITERAL("pass");
	AuthConfig.AllowPlaintext = true;
	testRequire(xrtSmtpClientAuth(
		pClient,
		&AuthConfig,
		Deadline,
		NULL
	) && xrtSmtpClientAuthenticated(pClient),
		"SMTP LOGIN authentication failed");
	testRequire(xrtSmtpClientQuit(pClient, Deadline, NULL),
		"SMTP LOGIN QUIT failed");
	xrtSmtpClientDestroy(pClient);

	pClient = xrtSmtpClientOpen(&ClientConfig, Deadline, NULL);
	testRequire(pClient != NULL, "SMTP XOAUTH2 client open failed");
	xrtSmtpAuthConfigInit(&AuthConfig);
	AuthConfig.Method = XSMTP_AUTH_OAUTHBEARER;
	AuthConfig.Username = XRT_STR_LITERAL("user");
	AuthConfig.Secret = XRT_STR_LITERAL("token");
	AuthConfig.AllowPlaintext = true;
	testRequire(!xrtSmtpClientAuth(
		pClient,
		&AuthConfig,
		Deadline,
		NULL
	), "SMTP OAUTHBEARER was allowed on a plaintext connection");
	xrtClearError();
	AuthConfig.Method = XSMTP_AUTH_XOAUTH2;
	testRequire(!xrtSmtpClientAuth(
		pClient,
		&AuthConfig,
		Deadline,
		NULL
	), "SMTP XOAUTH2 rejection was accepted");
	testRequire(!xrtSmtpClientAuthenticated(pClient) &&
		xrtSmtpClientLastReply(pClient, &Reply) &&
		(Reply.Code == 535), "SMTP XOAUTH2 final rejection mismatch");
	xrtClearError();
	testRequire(xrtSmtpClientQuit(pClient, Deadline, NULL),
		"SMTP XOAUTH2 QUIT failed");
	xrtSmtpClientDestroy(pClient);

	pClient = xrtSmtpClientOpen(&ClientConfig, Deadline, NULL);
	testRequire(pClient != NULL, "SMTP long PLAIN client open failed");
	memset(sLongSecret, 'p', sizeof(sLongSecret));
	xrtSmtpAuthConfigInit(&AuthConfig);
	AuthConfig.Username = XRT_STR_LITERAL("u");
	AuthConfig.Secret = (xstrview) { sLongSecret, sizeof(sLongSecret) };
	AuthConfig.AllowPlaintext = true;
	testRequire(xrtSmtpClientAuth(
		pClient,
		&AuthConfig,
		Deadline,
		NULL
	) && xrtSmtpClientAuthenticated(pClient),
		"SMTP long SASL response fallback failed");
	testRequire(xrtSmtpClientQuit(pClient, Deadline, NULL),
		"SMTP long PLAIN QUIT failed");
	xrtSmtpClientDestroy(pClient);

	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"SMTP auth server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"SMTP auth transcript mismatch");
	xrtThreadDestroy(pThread);

	testRequire(xrtNetListenerClose(pListener),
		"SMTP auth listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"SMTP auth resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"SMTP auth engine destroy failed");
	return 0;
}
