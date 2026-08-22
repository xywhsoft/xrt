#include "../test.h"



typedef struct testpop3saslserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	xstrview LongResponse;
	bool Success;
} testpop3saslserver;



static xnetaddr TestPop3SaslAddress;



/* 把测试域名固定解析到本地 POP3 SASL 服务器。 */
static xnetaddrlist* testPop3SaslResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "pop3-sasl.test") != 0 ) {
		return NULL;
	}
	Address = TestPop3SaslAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 发送 POP3 测试服务器的一段完整线路数据。 */
static bool testPop3SaslSend(
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



/* 精确接收并比较一段包含凭据的测试命令。 */
static bool testPop3SaslReceive(
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



/* 服务一个初始响应或 challenge 回退的 AUTH PLAIN 会话。 */
static bool testPop3SaslSession(
	xnetstream* pStream,
	size_t iMode,
	xstrview LongResponse,
	xdeadline iDeadline
)
{
	static const char sCapabilities[] =
		"+OK capabilities\r\n"
		"SASL PLAIN SCRAM-SHA-256 XOAUTH2 OAUTHBEARER\r\n"
		"USER\r\n"
		".\r\n";
	static const char sInitial[] =
		"AUTH PLAIN AHVzZXIAcGFzcw==\r\n";
	static const char sResponse[] = "AHVzZXIAcGFzcw==\r\n";
	bool bSuccess;

	bSuccess = testPop3SaslSend(
		pStream,
		"+OK ready\r\n",
		sizeof("+OK ready\r\n") - 1u,
		iDeadline
	) && testPop3SaslReceive(
		pStream,
		"CAPA\r\n",
		sizeof("CAPA\r\n") - 1u,
		iDeadline
	) && testPop3SaslSend(
		pStream,
		sCapabilities,
		sizeof(sCapabilities) - 1u,
		iDeadline
	);
	if ( iMode == 0 ) {
		bSuccess = bSuccess && testPop3SaslReceive(
			pStream,
			sInitial,
			sizeof(sInitial) - 1u,
			iDeadline
		);
	} else {
		bSuccess = bSuccess && testPop3SaslReceive(
			pStream,
			"AUTH PLAIN\r\n",
			sizeof("AUTH PLAIN\r\n") - 1u,
			iDeadline
		) && testPop3SaslSend(
			pStream,
			"+ \r\n",
			sizeof("+ \r\n") - 1u,
			iDeadline
		);
		if ( iMode == 1 ) {
			bSuccess = bSuccess && testPop3SaslReceive(
				pStream,
				sResponse,
				sizeof(sResponse) - 1u,
				iDeadline
			);
		} else {
			bSuccess = bSuccess && testPop3SaslReceive(
				pStream,
				LongResponse.Data,
				LongResponse.Size,
				iDeadline
			) && testPop3SaslReceive(
				pStream,
				"\r\n",
				2u,
				iDeadline
			);
		}
	}
	return bSuccess && testPop3SaslSend(
		pStream,
		"+OK authenticated\r\n",
		sizeof("+OK authenticated\r\n") - 1u,
		iDeadline
	) && testPop3SaslReceive(
		pStream,
		"QUIT\r\n",
		sizeof("QUIT\r\n") - 1u,
		iDeadline
	) && testPop3SaslSend(
		pStream,
		"+OK signing off\r\n",
		sizeof("+OK signing off\r\n") - 1u,
		iDeadline
	);
}



/* 连续服务初始响应、显式 challenge 和超长响应回退。 */
static int32 testPop3SaslServer(ptr pData)
{
	testpop3saslserver* pServer = (testpop3saslserver*)pData;
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
		bSuccess = testPop3SaslSession(
			pStream,
			i,
			pServer->LongResponse,
			pServer->Deadline
		) && xrtNetStreamClose(pStream) && xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_CLOSE,
			pServer->Deadline,
			NULL
		);
		xrtNetStreamDestroy(pStream);
		if ( !bSuccess ) {
			break;
		}
	}
	pServer->Success = bSuccess;
	return bSuccess ? 0 : 1;
}



/* 打开一个借用测试引擎和解析器的 POP3 客户端。 */
static xpop3client* testPop3SaslOpen(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	xdeadline iDeadline
)
{
	xpop3clientconfig Config;

	xrtPop3ClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "pop3-sasl.test";
	Config.Net.Port = TestPop3SaslAddress.Port;
	return xrtPop3ClientOpen(&Config, iDeadline, NULL);
}



/* 验证 POP3 SASL 能力、TLS 安全门、初始响应和 challenge 回退。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	testpop3saslserver Server;
	xpop3authconfig Auth;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xpop3client* pClient;
	xthread* pThread;
	xdeadline Deadline;
	str sLongResponse;
	char sPlain[606];
	char sSecret[600];
	uint32 iExpected = XPOP3_SASL_PLAIN | XPOP3_SASL_XOAUTH2 |
		XPOP3_SASL_OAUTHBEARER;

	testRequire((xrtPop3SaslMechanism(XRT_STR_LITERAL("plain")) ==
		XPOP3_SASL_PLAIN) &&
		(xrtPop3SaslMechanism(XRT_STR_LITERAL("SCRAM-SHA-256")) == 0),
		"POP3 SASL mechanism mapping mismatch");
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"POP3 SASL engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "POP3 SASL loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestPop3SaslAddress
	), "POP3 SASL listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testPop3SaslResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "POP3 SASL resolver creation failed");
	memset(sSecret, 'p', sizeof(sSecret));
	sPlain[0] = 0;
	memcpy(sPlain + 1u, "user", 4u);
	sPlain[5] = 0;
	memcpy(sPlain + 6u, sSecret, sizeof(sSecret));
	sLongResponse = xrtBase64EncodeNew(sPlain, sizeof(sPlain), NULL);
	testRequire((sLongResponse != NULL) &&
		(strlen(sLongResponse) > (XPOP3_COMMAND_MAX - 2u)),
		"POP3 long SASL response setup failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.LongResponse = testMailView(sLongResponse);
	Server.Success = false;
	pThread = xrtThreadCreate(testPop3SaslServer, &Server, 0);
	testRequire(pThread != NULL, "POP3 SASL server thread creation failed");
	for ( size_t i = 0; i < 3u; i++ ) {
		pClient = testPop3SaslOpen(pEngine, pResolver, Deadline);
		testRequire((pClient != NULL) &&
			(xrtPop3ClientSaslMechanisms(pClient) == iExpected),
			"POP3 SASL capability snapshot mismatch");
		xrtPop3AuthConfigInit(&Auth);
		Auth.Method = XPOP3_AUTH_XOAUTH2;
		Auth.Username = XRT_STR_LITERAL("user");
		Auth.Secret = XRT_STR_LITERAL("token");
		Auth.AllowPlaintext = true;
		testRequire(!xrtPop3ClientAuth(
			pClient,
			&Auth,
			Deadline,
			NULL
		) && (xrtPop3ClientState(pClient) == XPOP3_CLIENT_AUTHORIZATION),
			"POP3 bearer authentication crossed the TLS gate");
		xrtClearError();
		xrtPop3AuthConfigInit(&Auth);
		Auth.Username = XRT_STR_LITERAL("user");
		Auth.Secret = i == 2 ? testMailViewN(
			sSecret,
			sizeof(sSecret)
		) : XRT_STR_LITERAL("pass");
		Auth.InitialResponse = i != 1;
		Auth.AllowPlaintext = true;
		testRequire(xrtPop3ClientAuth(
			pClient,
			&Auth,
			Deadline,
			NULL
		) && (xrtPop3ClientState(pClient) == XPOP3_CLIENT_TRANSACTION),
			"POP3 AUTH PLAIN exchange failed");
		testRequire(xrtPop3ClientQuit(pClient, Deadline, NULL),
			"POP3 SASL QUIT failed");
		xrtPop3ClientDestroy(pClient);
	}
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"POP3 SASL server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"POP3 SASL server transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtSecureZero(sPlain, sizeof(sPlain));
	xrtSecureZero(sSecret, sizeof(sSecret));
	xrtFree(sLongResponse);

	testRequire(xrtNetListenerClose(pListener),
		"POP3 SASL listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"POP3 SASL resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"POP3 SASL engine destroy failed");
	return 0;
}
