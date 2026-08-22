#include "../test.h"



static const char TestPop3MessageRaw[] =
	"Subject: test\r\n"
	"Content-Type: text/plain; charset=UTF-8\r\n"
	"Content-Transfer-Encoding: quoted-printable\r\n"
	"\r\n"
	".dot=20line\r\n"
	"second\r\n";

static const char TestPop3TopRaw[] =
	"Subject: test\r\n"
	"Content-Type: text/plain; charset=UTF-8\r\n"
	"Content-Transfer-Encoding: quoted-printable\r\n"
	"\r\n"
	".dot=20line\r\n";



typedef struct testpop3messageserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} testpop3messageserver;



typedef struct testpop3messagesink {
	char Data[1024];
	size_t Size;
	size_t Calls;
} testpop3messagesink;



static xnetaddr TestPop3MessageAddress;



/* 把测试域名解析到本地 POP3 Listener。 */
static xnetaddrlist* testPop3MessageResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "message.pop3.test") != 0 ) {
		return NULL;
	}
	Address = TestPop3MessageAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 发送一段完整 POP3 响应。 */
static bool testPop3MessageSend(
	xnetstream* pStream,
	cstr sText,
	xdeadline iDeadline
)
{
	size_t iSize = strlen(sText);

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



/* 精确接收一条 POP3 命令。 */
static bool testPop3MessageReceive(
	xnetstream* pStream,
	cstr sExpected,
	xdeadline iDeadline
)
{
	size_t iExpected = strlen(sExpected);
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



/* 通过 FIN 或异常复位终态验证对端已经停止传输。 */
static bool testPop3MessagePeerClosed(
	xnetstream* pStream,
	xdeadline iDeadline
)
{
	xnetbytes* pBytes = xrtNetStreamRecv(
		pStream,
		0,
		iDeadline,
		NULL
	);
	bool bClosed;

	if ( pBytes != NULL ) {
		xrtNetBytesDestroy(pBytes);
		return false;
	}
	xrtClearError();
	if ( (xrtNetStreamState(pStream) == XNET_STREAM_OPEN) &&
		!xrtNetStreamClose(pStream) ) {
		return false;
	}
	bClosed = xrtNetStreamWait(
		pStream,
		XNET_STREAM_WAIT_CLOSE,
		iDeadline,
		NULL
	) || (xrtNetStreamState(pStream) == XNET_STREAM_CLOSED);
	xrtClearError();
	return bClosed;
}



/* 发送包含 dot-stuffed 正文的测试消息。 */
static bool testPop3MessageResponse(
	xnetstream* pStream,
	bool bTop,
	xdeadline iDeadline
)
{
	if ( bTop ) {
		return testPop3MessageSend(
			pStream,
			"+OK top follows\r\n"
			"Subject: test\r\n"
			"Content-Type: text/plain; charset=UTF-8\r\n"
			"Content-Transfer-Encoding: quoted-printable\r\n"
			"\r\n"
			"..dot=20line\r\n"
			".\r\n",
			iDeadline
		);
	}
	return testPop3MessageSend(
		pStream,
		"+OK message follows\r\n"
		"Subject: test\r\n"
		"Content-Type: text/plain; charset=UTF-8\r\n"
		"Content-Transfer-Encoding: quoted-printable\r\n"
		"\r\n"
		"..dot=20line\r\n"
		"second\r\n"
		".\r\n",
		iDeadline
	);
}



/* 模拟认证和四种消息读取路径。 */
static int32 testPop3MessageServer(ptr pData)
{
	testpop3messageserver* pServer = (testpop3messageserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testPop3MessageSend(
		pStream,
		"+OK message.pop3.test ready\r\n",
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"USER user\r\n",
		pServer->Deadline
	) && testPop3MessageSend(
		pStream,
		"+OK user accepted\r\n",
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"PASS pass\r\n",
		pServer->Deadline
	) && testPop3MessageSend(
		pStream,
		"+OK mailbox locked\r\n",
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"RETR 1\r\n",
		pServer->Deadline
	) && testPop3MessageResponse(
		pStream,
		false,
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"RETR 2\r\n",
		pServer->Deadline
	) && testPop3MessageResponse(
		pStream,
		false,
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"TOP 3 1\r\n",
		pServer->Deadline
	) && testPop3MessageResponse(
		pStream,
		true,
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"RETR 4\r\n",
		pServer->Deadline
	) && testPop3MessageResponse(
		pStream,
		false,
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"QUIT\r\n",
		pServer->Deadline
	) && testPop3MessageSend(
		pStream,
		"+OK signing off\r\n",
		pServer->Deadline
	) && xrtNetStreamClose(pStream) && xrtNetStreamWait(
		pStream,
		XNET_STREAM_WAIT_CLOSE,
		pServer->Deadline,
		NULL
	);
	pServer->Success = bSuccess;
	xrtNetStreamDestroy(pStream);
	return bSuccess ? 0 : 2;
}



/* 发送超过客户端预算的一行，并验证客户端关闭未排空的会话。 */
static int32 testPop3MessageLimitServer(ptr pData)
{
	testpop3messageserver* pServer = (testpop3messageserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testPop3MessageSend(
		pStream,
		"+OK message.pop3.test ready\r\n",
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"USER user\r\n",
		pServer->Deadline
	) && testPop3MessageSend(
		pStream,
		"+OK user accepted\r\n",
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"PASS pass\r\n",
		pServer->Deadline
	) && testPop3MessageSend(
		pStream,
		"+OK mailbox locked\r\n",
		pServer->Deadline
	) && testPop3MessageReceive(
		pStream,
		"RETR 5\r\n",
		pServer->Deadline
	) && testPop3MessageSend(
		pStream,
		"+OK message follows\r\n123456\r\n",
		pServer->Deadline
	) && testPop3MessagePeerClosed(
		pStream,
		pServer->Deadline
	);
	pServer->Success = bSuccess;
	xrtNetStreamDestroy(pStream);
	return bSuccess ? 0 : 2;
}



/* 收集流式输出，记录调用次数以验证非整报文交付。 */
static bool testPop3MessageWrite(xbytesview Data, ptr pUserData)
{
	testpop3messagesink* pSink = (testpop3messagesink*)pUserData;

	if ( Data.Size > (sizeof(pSink->Data) - pSink->Size) ) {
		return false;
	}
	memcpy(pSink->Data + pSink->Size, Data.Data, Data.Size);
	pSink->Size += Data.Size;
	pSink->Calls++;
	return true;
}



/* 验证流式、owned、TOP 和 MIME 树路径共享同一客户端状态机。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xpop3clientconfig ClientConfig;
	testpop3messageserver Server;
	testpop3messagesink Sink = { 0 };
	xmailtree Tree;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xpop3client* pClient;
	xthread* pThread;
	xdeadline Deadline;
	bytes pMessage;
	bytes pTop;
	size_t iWritten;
	size_t iMessageSize;
	size_t iTopSize;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"POP3 message engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "POP3 message loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestPop3MessageAddress
	), "POP3 message listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testPop3MessageResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "POP3 message resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testPop3MessageServer, &Server, 0);
	testRequire(pThread != NULL, "POP3 message server thread creation failed");
	xrtPop3ClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "message.pop3.test";
	ClientConfig.Net.Port = TestPop3MessageAddress.Port;
	ClientConfig.ReadCapabilities = false;
	pClient = xrtPop3ClientOpen(&ClientConfig, Deadline, NULL);
	testRequire(pClient != NULL, "POP3 message client open failed");
	testRequire(xrtPop3ClientLogin(
		pClient,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("pass"),
		true,
		Deadline,
		NULL
	), "POP3 message login failed");

	testRequire(xrtPop3ClientRetrWrite(
		pClient,
		1u,
		0,
		testPop3MessageWrite,
		&Sink,
		&iWritten,
		Deadline,
		NULL
	) && (iWritten == sizeof(TestPop3MessageRaw) - 1u) &&
		(Sink.Size == iWritten) && (Sink.Calls > 5u) &&
		(memcmp(Sink.Data, TestPop3MessageRaw, iWritten) == 0),
		"POP3 streamed RETR output mismatch");
	pMessage = xrtPop3ClientRetrBytes(
		pClient,
		2u,
		0,
		&iMessageSize,
		Deadline,
		NULL
	);
	testRequire((pMessage != NULL) &&
		(iMessageSize == sizeof(TestPop3MessageRaw) - 1u) &&
		(pMessage[iMessageSize] == 0) &&
		(memcmp(pMessage, TestPop3MessageRaw, iMessageSize) == 0),
		"POP3 owned RETR output mismatch");
	xrtFree(pMessage);
	pTop = xrtPop3ClientTopBytes(
		pClient,
		3u,
		1u,
		0,
		&iTopSize,
		Deadline,
		NULL
	);
	testRequire((pTop != NULL) &&
		(iTopSize == sizeof(TestPop3TopRaw) - 1u) &&
		(memcmp(pTop, TestPop3TopRaw, iTopSize) == 0),
		"POP3 owned TOP output mismatch");
	xrtFree(pTop);
	testRequire(xrtPop3ClientRetrTree(
		pClient,
		4u,
		NULL,
		&Tree,
		Deadline,
		NULL
	) && (Tree.Root != NULL) && Tree.Root->Decoded &&
		(Tree.Root->Data.Size == sizeof(".dot line\r\nsecond\r\n") - 1u) &&
		(memcmp(
			Tree.Root->Data.Data,
			".dot line\r\nsecond\r\n",
			Tree.Root->Data.Size
		) == 0),
		"POP3 RETR MIME tree mismatch");
	xrtMailTreeFree(&Tree);
	testRequire(xrtPop3ClientQuit(pClient, Deadline, NULL),
		"POP3 message QUIT failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"POP3 message server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"POP3 message server transcript mismatch");

	xrtThreadDestroy(pThread);
	xrtPop3ClientDestroy(pClient);

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testPop3MessageLimitServer, &Server, 0);
	testRequire(pThread != NULL,
		"POP3 message limit server thread creation failed");
	pClient = xrtPop3ClientOpen(&ClientConfig, Deadline, NULL);
	testRequire((pClient != NULL) && xrtPop3ClientLogin(
		pClient,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("pass"),
		true,
		Deadline,
		NULL
	), "POP3 message limit client login failed");
	testRequire(xrtPop3ClientRetrBytes(
		pClient,
		5u,
		5u,
		NULL,
		Deadline,
		NULL
	) == NULL && (xrtPop3ClientState(pClient) == XPOP3_CLIENT_CLOSED) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"POP3 message byte limit did not close the partial response");
	xrtClearError();
	testRequire(xrtPop3ClientAbort(pClient) &&
		(xrtPop3ClientState(pClient) == XPOP3_CLIENT_CLOSED),
		"POP3 client abort was not idempotent");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"POP3 message limit server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"POP3 message limit server transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtPop3ClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"POP3 message listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"POP3 message resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"POP3 message engine destroy failed");
	return 0;
}
