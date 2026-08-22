#include "../test.h"



static const char TestImapMessageRaw[] =
	"Subject: test\r\n"
	"Content-Type: text/plain; charset=UTF-8\r\n"
	"Content-Transfer-Encoding: quoted-printable\r\n"
	"\r\n"
	"hello=20imap\r\n";

static const char TestImapHeaderRaw[] =
	"Subject: test\r\n"
	"From: sender@example.test\r\n"
	"\r\n";



typedef struct testimapmessageserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} testimapmessageserver;



typedef struct testimapmessagesink {
	char Data[1024];
	size_t Size;
	size_t Calls;
} testimapmessagesink;



static xnetaddr TestImapMessageAddress;



/* 把测试域名解析到本地 IMAP Listener。 */
static xnetaddrlist* testImapMessageResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "message.imap.test") != 0 ) {
		return NULL;
	}
	Address = TestImapMessageAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 发送一段完整 IMAP 响应。 */
static bool testImapMessageSend(
	xnetstream* pStream,
	const void* pData,
	size_t iSize,
	xdeadline iDeadline
)
{
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pStream, pData, iSize);

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



/* 精确接收一条 IMAP 命令。 */
static bool testImapMessageReceive(
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



/* 完成一条没有 literal 的精确命令交换。 */
static bool testImapMessageExchange(
	xnetstream* pStream,
	cstr sCommand,
	cstr sResponse,
	xdeadline iDeadline
)
{
	return testImapMessageReceive(pStream, sCommand, iDeadline) &&
		testImapMessageSend(
			pStream,
			sResponse,
			strlen(sResponse),
			iDeadline
		);
}



/* 发送包含一个 FETCH literal 的完整响应。 */
static bool testImapMessageLiteral(
	xnetstream* pStream,
	uint32 iSequence,
	cstr sPrefixItems,
	cstr sAttribute,
	cstr sData,
	size_t iSize,
	cstr sTag,
	xdeadline iDeadline
)
{
	char sPrefix[256];
	char sSuffix[96];
	int iPrefix;
	int iSuffix;

	iPrefix = snprintf(
		sPrefix,
		sizeof(sPrefix),
		"* %u FETCH (%s%s {%zu}\r\n",
		(unsigned int)iSequence,
		sPrefixItems,
		sAttribute,
		iSize
	);
	iSuffix = snprintf(
		sSuffix,
		sizeof(sSuffix),
		")\r\n%s OK fetch complete\r\n",
		sTag
	);
	return (iPrefix > 0) && ((size_t)iPrefix < sizeof(sPrefix)) &&
		(iSuffix > 0) && ((size_t)iSuffix < sizeof(sSuffix)) &&
		testImapMessageSend(
			pStream,
			sPrefix,
			(size_t)iPrefix,
			iDeadline
		) && testImapMessageSend(
			pStream,
			sData,
			iSize,
			iDeadline
		) && testImapMessageSend(
			pStream,
			sSuffix,
			(size_t)iSuffix,
			iDeadline
		);
}



/* 通过 FIN 或异常复位终态验证对端已经停止传输。 */
static bool testImapMessagePeerClosed(
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



/* 服务成功、空结果和连接复用转录。 */
static int32 testImapMessageServer(ptr pData)
{
	testimapmessageserver* pServer = (testimapmessageserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testImapMessageSend(
		pStream,
		"* PREAUTH message.imap.test ready\r\n",
		sizeof("* PREAUTH message.imap.test ready\r\n") - 1u,
		pServer->Deadline
	) && testImapMessageExchange(
		pStream,
		"A00000001 CAPABILITY\r\n",
		"* CAPABILITY IMAP4rev1\r\n"
		"A00000001 OK capability complete\r\n",
		pServer->Deadline
	) && testImapMessageExchange(
		pStream,
		"A00000002 SELECT \"INBOX\"\r\n",
		"* 3 EXISTS\r\nA00000002 OK [READ-WRITE] selected\r\n",
		pServer->Deadline
	) && testImapMessageReceive(
		pStream,
		"A00000003 FETCH 1 BODY.PEEK[]\r\n",
		pServer->Deadline
	) && testImapMessageSend(
		pStream,
		"* 4 EXISTS\r\n",
		sizeof("* 4 EXISTS\r\n") - 1u,
		pServer->Deadline
	) && testImapMessageLiteral(
		pStream,
		1u,
		"UID 41 ",
		"BODY[]",
		TestImapMessageRaw,
		sizeof(TestImapMessageRaw) - 1u,
		"A00000003",
		pServer->Deadline
	) && testImapMessageReceive(
		pStream,
		"A00000004 UID FETCH 42 BODY[HEADER]\r\n",
		pServer->Deadline
	) && testImapMessageLiteral(
		pStream,
		2u,
		"UID 42 ",
		"body[header]",
		TestImapHeaderRaw,
		sizeof(TestImapHeaderRaw) - 1u,
		"A00000004",
		pServer->Deadline
	) && testImapMessageReceive(
		pStream,
		"A00000005 FETCH 3 BODY.PEEK[]\r\n",
		pServer->Deadline
	) && testImapMessageLiteral(
		pStream,
		3u,
		"",
		"BODY[]",
		TestImapMessageRaw,
		sizeof(TestImapMessageRaw) - 1u,
		"A00000005",
		pServer->Deadline
	) && testImapMessageExchange(
		pStream,
		"A00000006 UID FETCH 99 BODY.PEEK[]\r\n",
		"A00000006 OK no such message\r\n",
		pServer->Deadline
	) && testImapMessageExchange(
		pStream,
		"A00000007 NOOP\r\n",
		"A00000007 OK noop complete\r\n",
		pServer->Deadline
	) && testImapMessageExchange(
		pStream,
		"A00000008 LOGOUT\r\n",
		"* BYE signing off\r\nA00000008 OK logout complete\r\n",
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



/* 声明超限 literal，并验证客户端不会把残余字节留给下一条响应。 */
static int32 testImapMessageLimitServer(ptr pData)
{
	testimapmessageserver* pServer = (testimapmessageserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testImapMessageSend(
		pStream,
		"* PREAUTH message.imap.test ready\r\n",
		sizeof("* PREAUTH message.imap.test ready\r\n") - 1u,
		pServer->Deadline
	) && testImapMessageExchange(
		pStream,
		"A00000001 CAPABILITY\r\n",
		"* CAPABILITY IMAP4rev1\r\n"
		"A00000001 OK capability complete\r\n",
		pServer->Deadline
	) && testImapMessageExchange(
		pStream,
		"A00000002 SELECT \"INBOX\"\r\n",
		"* 1 EXISTS\r\nA00000002 OK [READ-WRITE] selected\r\n",
		pServer->Deadline
	) && testImapMessageReceive(
		pStream,
		"A00000003 FETCH 1 BODY[]\r\n",
		pServer->Deadline
	) && testImapMessageSend(
		pStream,
		"* 1 FETCH (BODY[] {6}\r\n123456",
		sizeof("* 1 FETCH (BODY[] {6}\r\n123456") - 1u,
		pServer->Deadline
	) && testImapMessagePeerClosed(
		pStream,
		pServer->Deadline
	);
	pServer->Success = bSuccess;
	xrtNetStreamDestroy(pStream);
	return bSuccess ? 0 : 2;
}



/* 收集流式输出。 */
static bool testImapMessageWrite(xbytesview Data, ptr pUserData)
{
	testimapmessagesink* pSink = (testimapmessagesink*)pUserData;

	if ( Data.Size > (sizeof(pSink->Data) - pSink->Size) ) {
		return false;
	}
	memcpy(pSink->Data + pSink->Size, Data.Data, Data.Size);
	pSink->Size += Data.Size;
	pSink->Calls++;
	return true;
}



/* 验证 BODY 流、owned 字节、MIME 树、空结果和超限恢复。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	testimapmessageserver Server;
	testimapmessagesink Sink = { 0 };
	ximapclientconfig Config;
	ximapmailboxinfo Info;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	ximapclient* pClient;
	xthread* pThread;
	xdeadline Deadline;
	xmailtree Tree;
	bytes pHeader;
	size_t iWritten;
	size_t iHeaderSize;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP message engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP message loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapMessageAddress
	), "IMAP message listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapMessageResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "IMAP message resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testImapMessageServer, &Server, 0);
	testRequire(pThread != NULL, "IMAP message server thread creation failed");
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "message.imap.test";
	Config.Net.Port = TestImapMessageAddress.Port;
	pClient = xrtImapClientOpen(&Config, Deadline, NULL);
	testRequire(pClient != NULL, "IMAP message client open failed");
	testRequire(xrtImapClientSelect(
		pClient,
		XRT_STR_LITERAL("INBOX"),
		&Info,
		Deadline,
		NULL
	) && (Info.Exists == 3u), "IMAP message SELECT failed");
	testRequire(!xrtImapClientBodyWrite(
		pClient,
		1u,
		XRT_STR_LITERAL("HEADER] UID FETCH 2 BODY["),
		false,
		true,
		0,
		testImapMessageWrite,
		&Sink,
		NULL,
		Deadline,
		NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_SELECTED),
		"IMAP BODY section injection reached the active session");
	xrtClearError();
	testRequire(xrtImapClientBodyWrite(
		pClient,
		1u,
		XRT_STR_LITERAL(""),
		false,
		true,
		0,
		testImapMessageWrite,
		&Sink,
		&iWritten,
		Deadline,
		NULL
	) && (iWritten == sizeof(TestImapMessageRaw) - 1u) &&
		(Sink.Size == iWritten) && (Sink.Calls != 0) &&
		(memcmp(Sink.Data, TestImapMessageRaw, iWritten) == 0),
		"IMAP streamed BODY output mismatch");
	pHeader = xrtImapClientBodyBytes(
		pClient,
		42u,
		XRT_STR_LITERAL("HEADER"),
		true,
		false,
		0,
		&iHeaderSize,
		Deadline,
		NULL
	);
	testRequire((pHeader != NULL) &&
		(iHeaderSize == sizeof(TestImapHeaderRaw) - 1u) &&
		(pHeader[iHeaderSize] == 0) &&
		(memcmp(pHeader, TestImapHeaderRaw, iHeaderSize) == 0),
		"IMAP owned BODY section mismatch");
	xrtFree(pHeader);
	testRequire(xrtImapClientMessageTree(
		pClient,
		3u,
		false,
		true,
		NULL,
		&Tree,
		Deadline,
		NULL
	) && (Tree.Root != NULL) && Tree.Root->Decoded &&
		(Tree.Root->Data.Size == sizeof("hello imap\r\n") - 1u) &&
		(memcmp(
			Tree.Root->Data.Data,
			"hello imap\r\n",
			Tree.Root->Data.Size
		) == 0), "IMAP BODY MIME tree mismatch");
	xrtMailTreeFree(&Tree);
	testRequire(xrtImapClientBodyBytes(
		pClient,
		99u,
		XRT_STR_LITERAL(""),
		true,
		true,
		0,
		NULL,
		Deadline,
		NULL
	) == NULL && (xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_SELECTED),
		"IMAP missing BODY result state mismatch");
	xrtClearError();
	testRequire(xrtImapClientNoop(pClient, Deadline, NULL),
		"IMAP client was not reusable after an empty BODY result");
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL),
		"IMAP message LOGOUT failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP message server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP message server transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtImapClientDestroy(pClient);

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testImapMessageLimitServer, &Server, 0);
	testRequire(pThread != NULL,
		"IMAP message limit server thread creation failed");
	pClient = xrtImapClientOpen(&Config, Deadline, NULL);
	testRequire((pClient != NULL) && xrtImapClientSelect(
		pClient,
		XRT_STR_LITERAL("INBOX"),
		NULL,
		Deadline,
		NULL
	), "IMAP message limit client setup failed");
	testRequire(xrtImapClientBodyBytes(
		pClient,
		1u,
		XRT_STR_LITERAL(""),
		false,
		false,
		5u,
		NULL,
		Deadline,
		NULL
	) == NULL && (xrtImapClientState(pClient) == XIMAP_CLIENT_CLOSED) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"IMAP BODY byte limit did not close the partial response");
	xrtClearError();
	testRequire(xrtImapClientAbort(pClient) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_CLOSED),
		"IMAP client abort was not idempotent");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP message limit server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP message limit server transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtImapClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"IMAP message listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP message resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP message engine destroy failed");
	return 0;
}
