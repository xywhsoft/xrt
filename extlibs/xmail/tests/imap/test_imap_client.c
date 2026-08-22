#include "../test.h"



typedef struct testimapserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} testimapserver;



static xnetaddr TestImapAddress;



/* 把测试域名解析到本地 IMAP 服务器。 */
static xnetaddrlist* testImapResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap.test") != 0 ) {
		return NULL;
	}
	Address = TestImapAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 发送 IMAP 测试服务器的一段完整字节。 */
static bool testImapSend(
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



/* 精确接收并比较一段 IMAP 客户端字节。 */
static bool testImapReceive(
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



/* 模拟能力查询、流水线、双 literal FETCH、IDLE 和 LOGOUT。 */
static int32 testImapServer(ptr pData)
{
	testimapserver* pServer = (testimapserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testImapSend(
		pStream,
		"* OK imap.test ready\r\n",
		sizeof("* OK imap.test ready\r\n") - 1u,
		pServer->Deadline
	) && testImapReceive(
		pStream,
		"A00000001 CAPABILITY\r\n",
		sizeof("A00000001 CAPABILITY\r\n") - 1u,
		pServer->Deadline
	) && testImapSend(
		pStream,
		"* CAPABILITY IMAP4rev2 IDLE SASL-IR AUTH=PLAIN "
		"LOGINDISABLED LIST-EXTENDED COMPRESS=DEFLATE "
		"LITERAL- APPENDLIMIT=1048576\r\n"
		"A00000001 OK capability complete\r\n",
		sizeof(
			"* CAPABILITY IMAP4rev2 IDLE SASL-IR AUTH=PLAIN "
			"LOGINDISABLED LIST-EXTENDED COMPRESS=DEFLATE "
			"LITERAL- APPENDLIMIT=1048576\r\n"
			"A00000001 OK capability complete\r\n"
		) - 1u,
		pServer->Deadline
	) && testImapReceive(
		pStream,
		"P1 NOOP\r\nP2 NOOP\r\n",
		sizeof("P1 NOOP\r\nP2 NOOP\r\n") - 1u,
		pServer->Deadline
	) && testImapSend(
		pStream,
		"* 1 EXISTS\r\nP2 OK second\r\nP1 OK first\r\n",
		sizeof("* 1 EXISTS\r\nP2 OK second\r\nP1 OK first\r\n") - 1u,
		pServer->Deadline
	) && testImapReceive(
		pStream,
		"A00000002 FETCH 1 BODY[]\r\n",
		sizeof("A00000002 FETCH 1 BODY[]\r\n") - 1u,
		pServer->Deadline
	) && testImapSend(
		pStream,
		"* 1 FETCH (BODY[1] {5}\r\nab",
		sizeof("* 1 FETCH (BODY[1] {5}\r\nab") - 1u,
		pServer->Deadline
	) && testImapSend(
		pStream,
		"cde BODY[2] {4}\r\nwx",
		sizeof("cde BODY[2] {4}\r\nwx") - 1u,
		pServer->Deadline
	) && testImapSend(
		pStream,
		"yz)\r\nA00000002 OK fetch complete\r\n",
		sizeof("yz)\r\nA00000002 OK fetch complete\r\n") - 1u,
		pServer->Deadline
	) && testImapReceive(
		pStream,
		"A00000003 IDLE\r\n",
		sizeof("A00000003 IDLE\r\n") - 1u,
		pServer->Deadline
	) && testImapSend(
		pStream,
		"+ idling\r\n* 2 EXISTS\r\n",
		sizeof("+ idling\r\n* 2 EXISTS\r\n") - 1u,
		pServer->Deadline
	) && testImapReceive(
		pStream,
		"DONE\r\n",
		sizeof("DONE\r\n") - 1u,
		pServer->Deadline
	) && testImapSend(
		pStream,
		"A00000003 OK idle complete\r\n",
		sizeof("A00000003 OK idle complete\r\n") - 1u,
		pServer->Deadline
	) && testImapReceive(
		pStream,
		"A00000004 LOGOUT\r\n",
		sizeof("A00000004 LOGOUT\r\n") - 1u,
		pServer->Deadline
	) && testImapSend(
		pStream,
		"* BYE signing off\r\nA00000004 OK logout complete\r\n",
		sizeof(
			"* BYE signing off\r\nA00000004 OK logout complete\r\n"
		) - 1u,
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



/* 读取一个完整 literal，并验证按可用块推进。 */
static bool testImapLiteral(
	ximapclient* pClient,
	cstr sExpected,
	size_t iExpected,
	xdeadline iDeadline
)
{
	char sBuffer[8];
	size_t iOffset = 0;

	while ( xrtImapClientLiteralRemaining(pClient) != 0 ) {
		size_t iRead;

		if ( !xrtImapClientReadLiteral(
			pClient,
			sBuffer,
			3u,
			&iRead,
			iDeadline,
			NULL
		) || (iRead > (iExpected - iOffset)) ||
			(memcmp(sBuffer, sExpected + iOffset, iRead) != 0) ) {
			return false;
		}
		iOffset += iRead;
	}
	return iOffset == iExpected;
}



/* 验证低层流水线和顺序命令共享同一无损事件状态机。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	ximapclientconfig Config;
	testimapserver Server;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	ximapclient* pClient;
	ximapevent Event;
	xthread* pThread;
	xdeadline Deadline;
	uint64 iExpectedCapabilities;
	xstrview Parts[2];
	xmailnext Next;

	testRequire(!xrtImapClientAbort(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"IMAP null client abort mismatch");
	xrtClearError();

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP client engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP client loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapAddress
	), "IMAP client listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "IMAP client resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testImapServer, &Server, 0);
	testRequire(pThread != NULL, "IMAP server thread creation failed");
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "imap.test";
	Config.Net.Port = TestImapAddress.Port;
	pClient = xrtImapClientOpen(&Config, Deadline, NULL);
	iExpectedCapabilities = XIMAP_CAP_IMAP4REV2 | XIMAP_CAP_IDLE |
		XIMAP_CAP_SASL_IR | XIMAP_CAP_AUTH_PLAIN |
		XIMAP_CAP_LOGIN_DISABLED | XIMAP_CAP_LIST_EXTENDED |
		XIMAP_CAP_COMPRESS_DEFLATE | XIMAP_CAP_LITERAL_MINUS |
		XIMAP_CAP_APPENDLIMIT;
	testRequire((pClient != NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_NOT_AUTHENTICATED) &&
		((xrtImapClientCapabilities(pClient) & iExpectedCapabilities) ==
		 iExpectedCapabilities) &&
		(xrtImapClientAppendLimit(pClient) == UINT64_C(1048576)),
		"IMAP open or CAPABILITY snapshot failed");

	Parts[0] = XRT_STR_LITERAL("invalid\r\nargument");
	xrtClearError();
	testRequire(!xrtImapClientSendParts(
		pClient,
		XRT_STR_LITERAL("P0"),
		XRT_STR_LITERAL("NOOP"),
		Parts,
		1u,
		Deadline,
		NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"IMAP command parts accepted a line separator");
	xrtClearError();

	testRequire(xrtImapClientSendParts(
		pClient,
		XRT_STR_LITERAL("P1"),
		XRT_STR_LITERAL("NOOP"),
		NULL,
		0,
		Deadline,
		NULL
	) && xrtImapClientSendParts(
		pClient,
		XRT_STR_LITERAL("P2"),
		XRT_STR_LITERAL("NOOP"),
		NULL,
		0,
		Deadline,
		NULL
	), "IMAP low-level pipelined send failed");
	testRequire(xrtImapClientReceive(pClient, &Event, Deadline, NULL) &&
		(Event.Response.Kind == XIMAP_RESPONSE_UNTAGGED),
		"IMAP unsolicited response mismatch");
	testRequire(xrtImapClientReceive(pClient, &Event, Deadline, NULL) &&
		testMailViewEqual(Event.Response.Tag, XRT_STR_LITERAL("P2")),
		"IMAP second pipelined completion mismatch");
	testRequire(xrtImapClientReceive(pClient, &Event, Deadline, NULL) &&
		testMailViewEqual(Event.Response.Tag, XRT_STR_LITERAL("P1")),
		"IMAP first pipelined completion mismatch");

	Parts[0] = XRT_STR_LITERAL("1");
	Parts[1] = XRT_STR_LITERAL("BODY[]");
	testRequire(xrtImapClientBeginParts(
		pClient,
		XRT_STR_LITERAL("FETCH"),
		Parts,
		2u,
		Deadline,
		NULL
	), "IMAP FETCH begin failed");
	Next = xrtImapClientNext(pClient, &Event, Deadline, NULL);
	testRequire((Next == XMAIL_NEXT_ITEM) && Event.HasLiteral &&
		(Event.Kind == XIMAP_EVENT_RESPONSE) &&
		(Event.Literal.Size == 5u), "IMAP first literal marker mismatch");
	testRequire(xrtImapClientNext(pClient, &Event, Deadline, NULL) ==
		XMAIL_NEXT_ERROR, "IMAP allowed event read before literal completion");
	xrtClearError();
	testRequire(testImapLiteral(pClient, "abcde", 5u, Deadline),
		"IMAP first literal bytes mismatch");
	Next = xrtImapClientNext(pClient, &Event, Deadline, NULL);
	testRequire((Next == XMAIL_NEXT_ITEM) && Event.HasLiteral &&
		(Event.Kind == XIMAP_EVENT_FRAGMENT) &&
		(Event.Literal.Size == 4u), "IMAP second literal marker mismatch");
	testRequire(testImapLiteral(pClient, "wxyz", 4u, Deadline),
		"IMAP second literal bytes mismatch");
	Next = xrtImapClientNext(pClient, &Event, Deadline, NULL);
	testRequire((Next == XMAIL_NEXT_ITEM) && !Event.HasLiteral &&
		(Event.Kind == XIMAP_EVENT_FRAGMENT) &&
		testMailViewEqual(Event.Source, XRT_STR_LITERAL(")")),
		"IMAP literal response closing fragment mismatch");
	testRequire(xrtImapClientNext(pClient, &Event, Deadline, NULL) ==
		XMAIL_NEXT_END, "IMAP FETCH completion mismatch");

	testRequire(xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("IDLE"),
		XRT_STR_LITERAL(""),
		Deadline,
		NULL
	), "IMAP IDLE begin failed");
	testRequire((xrtImapClientNext(pClient, &Event, Deadline, NULL) ==
		XMAIL_NEXT_ITEM) &&
		(Event.Response.Kind == XIMAP_RESPONSE_CONTINUATION),
		"IMAP IDLE continuation mismatch");
	testRequire((xrtImapClientNext(pClient, &Event, Deadline, NULL) ==
		XMAIL_NEXT_ITEM) &&
		(Event.Response.Kind == XIMAP_RESPONSE_UNTAGGED),
		"IMAP IDLE unsolicited event mismatch");
	testRequire(xrtImapClientContinue(
		pClient,
		XRT_STR_LITERAL("DONE"),
		Deadline,
		NULL
	) && (xrtImapClientNext(pClient, &Event, Deadline, NULL) ==
		XMAIL_NEXT_END), "IMAP IDLE termination mismatch");

	testRequire(xrtImapClientLogout(pClient, Deadline, NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_CLOSED),
		"IMAP LOGOUT failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP server transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtImapClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"IMAP listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP engine destroy failed");
	return 0;
}
