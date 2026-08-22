#include "../test.h"



typedef struct testimapcommandserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} testimapcommandserver;



static xnetaddr TestImapCommandAddress;



/* 把测试域名解析到本地命令服务器。 */
static xnetaddrlist* testImapCommandResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap-command.test") != 0 ) {
		return NULL;
	}
	Address = TestImapCommandAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 完整发送一段 IMAP 响应。 */
static bool testImapCommandSend(
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



/* 精确接收并比较客户端命令字节。 */
static bool testImapCommandReceive(
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



/* 完成一条无 continuation 的精确命令交换。 */
static bool testImapCommandExchange(
	xnetstream* pStream,
	cstr sCommand,
	cstr sResponse,
	xdeadline iDeadline
)
{
	return testImapCommandReceive(
		pStream,
		sCommand,
		strlen(sCommand),
		iDeadline
	) && testImapCommandSend(
		pStream,
		sResponse,
		strlen(sResponse),
		iDeadline
	);
}



/* 服务完整命令 helper、literal 和 IDLE 转录。 */
static int32 testImapCommandServer(ptr pData)
{
	testimapcommandserver* pServer = (testimapcommandserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testImapCommandSend(
		pStream,
		"* PREAUTH imap-command.test ready\r\n",
		sizeof("* PREAUTH imap-command.test ready\r\n") - 1u,
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000001 CAPABILITY\r\n",
		"* CAPABILITY IMAP4rev1 IDLE UIDPLUS MOVE UNSELECT\r\n"
		"A00000001 OK capability complete\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000002 SELECT \"INBOX\"\r\n",
		"* 3 EXISTS\r\n"
		"* 1 RECENT\r\n"
		"* OK [UNSEEN 2] unseen\r\n"
		"* OK [UIDVALIDITY 42] valid\r\n"
		"* OK [UIDNEXT 7] next\r\n"
		"* OK [HIGHESTMODSEQ 9] modseq\r\n"
		"A00000002 OK [READ-WRITE] selected\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000003 CHECK\r\n",
		"A00000003 OK checked\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000004 SEARCH UNSEEN\r\n",
		"* SEARCH 2 3\r\nA00000004 OK search complete\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000005 UID SEARCH ALL\r\n",
		"* SEARCH 10 11\r\nA00000005 OK uid search complete\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000006 UID FETCH 1:* BODY.PEEK[]\r\n",
		"* 1 FETCH (BODY[] {5}\r\nhello)\r\n"
		"A00000006 OK fetch complete\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000007 UID STORE 1 +FLAGS.SILENT (\\Seen)\r\n",
		"* 1 FETCH (FLAGS (\\Seen))\r\n"
		"A00000007 OK store complete\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000008 UID MOVE 1 \"Archive\"\r\n",
		"A00000008 OK [COPYUID 42 1 9] move complete\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000009 UID EXPUNGE 1\r\n",
		"* 1 EXPUNGE\r\nA00000009 OK expunge complete\r\n",
		pServer->Deadline
	) && testImapCommandReceive(
		pStream,
		"A0000000A IDLE\r\n",
		sizeof("A0000000A IDLE\r\n") - 1u,
		pServer->Deadline
	) && testImapCommandSend(
		pStream,
		"+ idling\r\n* 2 EXISTS\r\n",
		sizeof("+ idling\r\n* 2 EXISTS\r\n") - 1u,
		pServer->Deadline
	) && testImapCommandReceive(
		pStream,
		"DONE\r\n",
		sizeof("DONE\r\n") - 1u,
		pServer->Deadline
	) && testImapCommandSend(
		pStream,
		"A0000000A OK idle complete\r\n",
		sizeof("A0000000A OK idle complete\r\n") - 1u,
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A0000000B UNSELECT\r\n",
		"A0000000B OK unselected\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A0000000C CREATE \"New \\\"Box\"\r\n",
		"A0000000C OK created\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A0000000D RENAME \"New \\\"Box\" \"Renamed\\\\Box\"\r\n",
		"A0000000D OK renamed\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A0000000E SUBSCRIBE \"Renamed\\\\Box\"\r\n",
		"A0000000E OK subscribed\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A0000000F UNSUBSCRIBE \"Renamed\\\\Box\"\r\n",
		"A0000000F OK unsubscribed\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000010 DELETE \"Renamed\\\\Box\"\r\n",
		"A00000010 OK deleted\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000011 LIST \"\" \"*\"\r\n",
		"* LIST () \"/\" \"INBOX\"\r\nA00000011 OK list complete\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000012 STATUS \"INBOX\" "
		"(MESSAGES UNSEEN UIDNEXT UIDVALIDITY)\r\n",
		"* STATUS \"INBOX\" (MESSAGES 3 UNSEEN 2 UIDNEXT 7 "
		"UIDVALIDITY 42)\r\nA00000012 OK status complete\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000013 NOOP\r\n",
		"A00000013 OK noop complete\r\n",
		pServer->Deadline
	) && testImapCommandExchange(
		pStream,
		"A00000014 LOGOUT\r\n",
		"* BYE signing off\r\nA00000014 OK logout complete\r\n",
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



/* 消费一个流式命令，并可收集第一个 literal。 */
static bool testImapCommandDrain(
	ximapclient* pClient,
	char* sLiteral,
	size_t iCapacity,
	size_t* pLiteralSize,
	xdeadline iDeadline
)
{
	size_t iLiteral = 0;

	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			NULL
		);

		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( pLiteralSize != NULL ) {
				*pLiteralSize = iLiteral;
			}
			return xrtImapClientLastResponse(pClient, &Final) &&
				(Final.Status == XIMAP_STATUS_OK);
		}
		while ( Event.HasLiteral &&
			(xrtImapClientLiteralRemaining(pClient) != 0) ) {
			char arrDiscard[16];
			void* pOutput = (sLiteral != NULL) && (iLiteral < iCapacity) ?
				sLiteral + iLiteral : arrDiscard;
			size_t iOutput = pOutput == arrDiscard ? sizeof(arrDiscard) :
				iCapacity - iLiteral;
			size_t iRead;

			if ( (iOutput == 0) || !xrtImapClientReadLiteral(
				pClient,
				pOutput,
				iOutput,
				&iRead,
				iDeadline,
				NULL
			) ) {
				return false;
			}
			if ( pOutput != arrDiscard ) {
				iLiteral += iRead;
			}
		}
	}
}



/* 验证命令 helper 的真实线路、状态和 literal 流。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	testimapcommandserver Server;
	ximapclientconfig Config;
	ximapmailboxinfo Info;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	ximapclient* pClient;
	xthread* pThread;
	xdeadline Deadline;
	ximapevent Event;
	char sLiteral[8];
	size_t iLiteral;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP command engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP command loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapCommandAddress
	), "IMAP command listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapCommandResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "IMAP command resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testImapCommandServer, &Server, 0);
	testRequire(pThread != NULL, "IMAP command server thread creation failed");
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "imap-command.test";
	Config.Net.Port = TestImapCommandAddress.Port;
	pClient = xrtImapClientOpen(&Config, Deadline, NULL);
	testRequire((pClient != NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
		"IMAP PREAUTH open failed");
	testRequire(xrtImapClientSelect(
		pClient,
		XRT_STR_LITERAL("INBOX"),
		&Info,
		Deadline,
		NULL
	) && (xrtImapClientState(pClient) == XIMAP_CLIENT_SELECTED) &&
		(Info.Exists == UINT64_C(3)) && (Info.Recent == UINT64_C(1)) &&
		(Info.Unseen == UINT64_C(2)) &&
		(Info.UidValidity == UINT64_C(42)) &&
		(Info.UidNext == UINT64_C(7)) &&
		(Info.HighestModSeq == UINT64_C(9)) && !Info.ReadOnly,
		"IMAP SELECT helper failed");
	testRequire(xrtImapClientCheck(pClient, Deadline, NULL),
		"IMAP CHECK helper failed");
	testRequire(xrtImapClientBeginSearch(
		pClient,
		XRT_STR_LITERAL("UNSEEN"),
		false,
		Deadline,
		NULL
	) && testImapCommandDrain(pClient, NULL, 0, NULL, Deadline),
		"IMAP SEARCH helper failed");
	testRequire(xrtImapClientBeginSearch(
		pClient,
		XRT_STR_LITERAL("ALL"),
		true,
		Deadline,
		NULL
	) && testImapCommandDrain(pClient, NULL, 0, NULL, Deadline),
		"IMAP UID SEARCH helper failed");
	testRequire(xrtImapClientBeginFetch(
		pClient,
		XRT_STR_LITERAL("1:*"),
		XRT_STR_LITERAL("BODY.PEEK[]"),
		true,
		Deadline,
		NULL
	) && testImapCommandDrain(
		pClient,
		sLiteral,
		sizeof(sLiteral),
		&iLiteral,
		Deadline
	) && (iLiteral == 5u) && (memcmp(sLiteral, "hello", 5u) == 0),
		"IMAP UID FETCH literal helper failed");
	testRequire(xrtImapClientBeginStore(
		pClient,
		XRT_STR_LITERAL("1"),
		XIMAP_STORE_ADD_SILENT,
		XRT_STR_LITERAL("(\\Seen)"),
		true,
		Deadline,
		NULL
	) && testImapCommandDrain(pClient, NULL, 0, NULL, Deadline),
		"IMAP UID STORE helper failed");
	testRequire(xrtImapClientBeginMove(
		pClient,
		XRT_STR_LITERAL("1"),
		XRT_STR_LITERAL("Archive"),
		true,
		Deadline,
		NULL
	) && testImapCommandDrain(pClient, NULL, 0, NULL, Deadline),
		"IMAP UID MOVE helper failed");
	testRequire(xrtImapClientBeginExpunge(
		pClient,
		XRT_STR_LITERAL("1"),
		Deadline,
		NULL
	) && testImapCommandDrain(pClient, NULL, 0, NULL, Deadline),
		"IMAP UID EXPUNGE helper failed");
	testRequire(xrtImapClientBeginIdle(pClient, Deadline, NULL) &&
		(xrtImapClientNext(pClient, &Event, Deadline, NULL) == XMAIL_NEXT_ITEM) &&
		(Event.Response.Kind == XIMAP_RESPONSE_CONTINUATION) &&
		(xrtImapClientNext(pClient, &Event, Deadline, NULL) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Event.Response.Text, XRT_STR_LITERAL("2 EXISTS")) &&
		xrtImapClientEndIdle(pClient, Deadline, NULL) &&
		(xrtImapClientNext(pClient, &Event, Deadline, NULL) == XMAIL_NEXT_END),
		"IMAP IDLE helper failed");
	testRequire(xrtImapClientUnselect(pClient, Deadline, NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
		"IMAP UNSELECT helper failed");
	testRequire(xrtImapClientCreateMailbox(
		pClient,
		XRT_STR_LITERAL("New \"Box"),
		Deadline,
		NULL
	), "IMAP CREATE helper failed");
	testRequire(xrtImapClientRenameMailbox(
		pClient,
		XRT_STR_LITERAL("New \"Box"),
		XRT_STR_LITERAL("Renamed\\Box"),
		Deadline,
		NULL
	), "IMAP RENAME helper failed");
	testRequire(xrtImapClientSubscribe(
		pClient,
		XRT_STR_LITERAL("Renamed\\Box"),
		Deadline,
		NULL
	) && xrtImapClientUnsubscribe(
		pClient,
		XRT_STR_LITERAL("Renamed\\Box"),
		Deadline,
		NULL
	) && xrtImapClientDeleteMailbox(
		pClient,
		XRT_STR_LITERAL("Renamed\\Box"),
		Deadline,
		NULL
	), "IMAP mailbox management helper failed");
	testRequire(xrtImapClientBeginList(
		pClient,
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("*"),
		Deadline,
		NULL
	) && testImapCommandDrain(pClient, NULL, 0, NULL, Deadline),
		"IMAP LIST helper failed");
	testRequire(xrtImapClientBeginStatus(
		pClient,
		XRT_STR_LITERAL("INBOX"),
		XRT_STR_LITERAL(""),
		Deadline,
		NULL
	) && testImapCommandDrain(pClient, NULL, 0, NULL, Deadline),
		"IMAP STATUS helper failed");
	testRequire(xrtImapClientNoop(pClient, Deadline, NULL),
		"IMAP NOOP helper failed");
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL),
		"IMAP command LOGOUT failed");
	xrtImapClientDestroy(pClient);

	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP command server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP command transcript mismatch");
	xrtThreadDestroy(pThread);
	testRequire(xrtNetListenerClose(pListener),
		"IMAP command listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP command resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP command engine destroy failed");
	return 0;
}
