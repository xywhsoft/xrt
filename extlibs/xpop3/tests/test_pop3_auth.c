#include <xrt/detail/wait.h>
#include "test.h"



typedef struct testpop3server {
	xnetlistener* Listener;
	double Deadline;
	bool Success;
} testpop3server;

typedef struct testpop3faultserver {
	xnetlistener* Listener;
	double Deadline;
	xcancel* Cancel;
	xatomic32 Release;
	unsigned Mode;
	bool Success;
} testpop3faultserver;



static xnetaddr TestPop3Address;



/* 把测试域名解析到本地 POP3 服务器。 */
static xnetaddrlist* testPop3Resolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "pop3.test") != 0 ) {
		return NULL;
	}
	Address = TestPop3Address;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 发送 POP3 测试服务器的一段完整响应。 */
static bool testPop3Send(
	xnetstream* pStream,
	cstr sText,
	size_t iSize,
	double iDeadline
)
{
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pStream, sText, iSize);

		if ( Result == XNET_RESULT_OK ) {
			return true;
		}
		if ( (Result != XNET_RESULT_AGAIN) || !__xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_WRITE,
			iDeadline,
			NULL
		) ) {
			return false;
		}
	}
}



/* 精确接收并比较一段 POP3 命令。 */
static bool testPop3Receive(
	xnetstream* pStream,
	cstr sExpected,
	size_t iExpected,
	double iDeadline
)
{
	size_t iReceived = 0;

	while ( iReceived < iExpected ) {
		xnetbytes* pBytes = __xrtNetStreamRecv(
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



/* 模拟完整 POP3 认证和事务命令序列。 */
static int32 testPop3Server(ptr pData)
{
	testpop3server* pServer = (testpop3server*)pData;
	xnetstream* pStream = __xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testPop3Send(
		pStream,
		"+OK pop3.test ready\r\n",
		sizeof("+OK pop3.test ready\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"CAPA\r\n",
		sizeof("CAPA\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK capabilities\r\nUSER\r\nUIDL\r\nTOP\r\nPIPELINING\r\n.\r\n",
		sizeof(
			"+OK capabilities\r\nUSER\r\nUIDL\r\nTOP\r\nPIPELINING\r\n.\r\n"
		) - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"USER user\r\n",
		sizeof("USER user\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK user accepted\r\n",
		sizeof("+OK user accepted\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"PASS pass\r\n",
		sizeof("PASS pass\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK mailbox locked\r\n",
		sizeof("+OK mailbox locked\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"STAT\r\n",
		sizeof("STAT\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK 2 300\r\n",
		sizeof("+OK 2 300\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"LIST 1\r\n",
		sizeof("LIST 1\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK 1 120\r\n",
		sizeof("+OK 1 120\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"UIDL 1\r\n",
		sizeof("UIDL 1\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK 1 uid-1\r\n",
		sizeof("+OK 1 uid-1\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"LIST\r\n",
		sizeof("LIST\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK list follows\r\n1 120\r\n2 180\r\n.\r\n",
		sizeof("+OK list follows\r\n1 120\r\n2 180\r\n.\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"UIDL\r\n",
		sizeof("UIDL\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK uidl follows\r\n1 uid-1\r\n2 uid-2\r\n.\r\n",
		sizeof("+OK uidl follows\r\n1 uid-1\r\n2 uid-2\r\n.\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"RETR 1\r\n",
		sizeof("RETR 1\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK message follows\r\nHeader: value\r\n\r\n..dot\r\n.\r\n",
		sizeof(
			"+OK message follows\r\nHeader: value\r\n\r\n..dot\r\n.\r\n"
		) - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"DELE 1\r\n",
		sizeof("DELE 1\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK marked\r\n",
		sizeof("+OK marked\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"RSET\r\n",
		sizeof("RSET\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK reset\r\n",
		sizeof("+OK reset\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"NOOP\r\n",
		sizeof("NOOP\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK\r\n",
		sizeof("+OK\r\n") - 1u,
		pServer->Deadline
	) && testPop3Receive(
		pStream,
		"QUIT\r\n",
		sizeof("QUIT\r\n") - 1u,
		pServer->Deadline
	) && testPop3Send(
		pStream,
		"+OK signing off\r\n",
		sizeof("+OK signing off\r\n") - 1u,
		pServer->Deadline
	) && xrtNetStreamClose(pStream) && __xrtNetStreamWait(
		pStream,
		XNET_STREAM_WAIT_CLOSE,
		pServer->Deadline,
		NULL
	);
	pServer->Success = bSuccess;
	xrtNetStreamDestroy(pStream);
	return bSuccess ? 0 : 2;
}



/* 畸形成功响应与正常 -ERR 拒绝的状态机故障样本。 */
static int32 testPop3FaultServer(ptr pData)
{
	testpop3faultserver* pServer = (testpop3faultserver*)pData;
	xnetstream* pStream = __xrtNetListenerAcceptWait(
		pServer->Listener, pServer->Deadline, NULL);
	cstr sCommand = pServer->Mode == 1u ? "LIST 1\r\n" :
		pServer->Mode == 2u ? "UIDL 1\r\n" : "STAT\r\n";
	cstr sReply = pServer->Mode == 8u ? "+BROKEN reply\r\n" :
		pServer->Mode == 4u ? "+OK 2 " :
		pServer->Mode == 0u ? "+OK invalid\r\n" :
		pServer->Mode == 1u ? "+OK invalid\r\n" :
		pServer->Mode == 2u ? "+OK 1 \r\n" : "-ERR unavailable\r\n";
	bool bOk;
	if ( pStream == NULL ) {
		return 1;
	}
	bOk = testPop3Send(pStream, "+OK ready\r\n",
		strlen("+OK ready\r\n"), pServer->Deadline) &&
		testPop3Receive(pStream, "USER user\r\n",
			strlen("USER user\r\n"), pServer->Deadline);
	if ( bOk && (pServer->Mode != 7u) ) {
		bOk = testPop3Send(pStream, "+OK user\r\n",
			strlen("+OK user\r\n"), pServer->Deadline) &&
			testPop3Receive(pStream, "PASS pass\r\n",
				strlen("PASS pass\r\n"), pServer->Deadline);
		if ( bOk && (pServer->Mode == 9u) ) {
			bOk = testPop3Send(pStream, "+BROKEN auth\r\n",
				strlen("+BROKEN auth\r\n"), pServer->Deadline);
		} else if ( bOk ) {
			bOk = testPop3Send(pStream, "+OK locked\r\n",
				strlen("+OK locked\r\n"), pServer->Deadline);
			if ( bOk && (pServer->Mode < 10u) ) {
				bOk = testPop3Receive(pStream, sCommand,
					strlen(sCommand), pServer->Deadline);
			}
		}
	}
	if ( bOk && ((pServer->Mode < 5u) || (pServer->Mode == 8u)) ) {
		bOk = testPop3Send(pStream, sReply, strlen(sReply),
			pServer->Deadline);
	}
	if ( bOk && (((pServer->Mode >= 5u) &&
		(pServer->Mode <= 7u)) || (pServer->Mode >= 10u)) ) {
		if ( pServer->Mode == 6u ) {
			bOk = xrtCancelRequest(pServer->Cancel);
		}
		while ( bOk && !xrtAtomic32Load(&pServer->Release,
			XMEMORY_ACQUIRE) ) {
			if ( __xrtWaitExpired(pServer->Deadline) ) {
				bOk = false;
				break;
			}
			xrtThreadYield();
		}
	}
	if ( bOk && (pServer->Mode >= 10u) ) {
		xnetbytes* pUnexpected = __xrtNetStreamRecv(
			pStream, 1u, __xrtWaitAfter(UINT64_C(100000)), NULL);

		bOk = (pUnexpected == NULL) &&
			(xrtNetStreamState(pStream) != XNET_STREAM_OPEN);
		xrtNetBytesDestroy(pUnexpected);
	}
	if ( bOk && (pServer->Mode == 6u) ) {
		/* 取消已发送命令后，失败终态必须主动切断连接。 */
		xnetbytes* pUnexpected = __xrtNetStreamRecv(
			pStream, 1u, pServer->Deadline, NULL);

		bOk = pUnexpected == NULL &&
			(xrtErrorKind(xrtGetError()) != XERR_TIMEOUT);
		xrtNetBytesDestroy(pUnexpected);
		xrtClearError();
	}
	if ( bOk && (pServer->Mode == 3u) ) {
		bOk = testPop3Receive(pStream, "NOOP\r\n",
			strlen("NOOP\r\n"), pServer->Deadline) &&
			testPop3Send(pStream, "+OK\r\n",
				strlen("+OK\r\n"), pServer->Deadline) &&
			testPop3Receive(pStream, "QUIT\r\n",
				strlen("QUIT\r\n"), pServer->Deadline) &&
			testPop3Send(pStream, "+OK bye\r\n",
				strlen("+OK bye\r\n"), pServer->Deadline);
	}
	if ( bOk && (xrtNetStreamState(pStream) == XNET_STREAM_OPEN) ) {
		bOk = xrtNetStreamClose(pStream) && __xrtNetStreamWait(
			pStream, XNET_STREAM_WAIT_CLOSE, pServer->Deadline, NULL);
	}
	pServer->Success = bOk;
	xrtNetStreamDestroy(pStream);
	return bOk ? 0 : 2;
}



/* 读取并解析一项 LIST 多行数据。 */
static bool testPop3ListNext(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iBytes,
	double iDeadline
)
{
	xpop3listview Item;
	xstrview Line;

	return (__xrtPop3ClientNext(pClient, &Line, iDeadline, NULL) ==
		XMAIL_NEXT_ITEM) && xrtPop3ListParse(Line, &Item) &&
		(Item.Message == iMessage) && (Item.Bytes == iBytes);
}



/* 读取并解析一项 UIDL 多行数据。 */
static bool testPop3UidlNext(
	xpop3client* pClient,
	uint64 iMessage,
	xstrview Id,
	double iDeadline
)
{
	xpop3uidlview Item;
	xstrview Line;

	return (__xrtPop3ClientNext(pClient, &Line, iDeadline, NULL) ==
		XMAIL_NEXT_ITEM) && xrtPop3UidlParse(Line, &Item) &&
		(Item.Message == iMessage) && testMailViewEqual(Item.Id, Id);
}



/* 验证完整 POP3 状态机、认证安全门和流式多行读取。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xpop3clientconfig Config;
	testpop3server Server;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xpop3client* pClient;
	xpop3stat Stat;
	xpop3listview List;
	xpop3uidlview Uidl;
	xstrview Line;
	xthread* pThread;
	double Deadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"POP3 client engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "POP3 client loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestPop3Address
	), "POP3 client listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testPop3Resolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "POP3 client resolver creation failed");

	Deadline = __xrtWaitAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testPop3Server, &Server, 0);
	testRequire(pThread != NULL, "POP3 server thread creation failed");
	xrtPop3ClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "pop3.test";
	Config.Net.Port = TestPop3Address.Port;
	pClient = __xrtPop3ClientOpen(&Config, Deadline, NULL);
	testRequire((pClient != NULL) &&
		(xrtPop3ClientState(pClient) == XPOP3_CLIENT_AUTHORIZATION) &&
		((xrtPop3ClientCapabilities(pClient) &
			(XPOP3_CAP_USER | XPOP3_CAP_UIDL | XPOP3_CAP_TOP |
			 XPOP3_CAP_PIPELINING)) ==
			(XPOP3_CAP_USER | XPOP3_CAP_UIDL | XPOP3_CAP_TOP |
			 XPOP3_CAP_PIPELINING)),
		"POP3 open or CAPA snapshot failed");
	testRequire(!__xrtPop3ClientLogin(
		pClient,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("pass"),
		false,
		Deadline,
		NULL
	), "POP3 sent credentials without plaintext opt-in");
	xrtClearError();
	testRequire(__xrtPop3ClientLogin(
		pClient,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("pass"),
		true,
		Deadline,
		NULL
	) && (xrtPop3ClientState(pClient) == XPOP3_CLIENT_TRANSACTION),
		"POP3 USER/PASS login failed");

	testRequire(__xrtPop3ClientStat(pClient, &Stat, Deadline, NULL) &&
		(Stat.Messages == 2u) && (Stat.Bytes == 300u),
		"POP3 STAT mismatch");
	testRequire(__xrtPop3ClientList(pClient, 1u, &List, Deadline, NULL) &&
		(List.Message == 1u) && (List.Bytes == 120u),
		"POP3 LIST item mismatch");
	testRequire(__xrtPop3ClientUidl(pClient, 1u, &Uidl, Deadline, NULL) &&
		(Uidl.Message == 1u) &&
		testMailViewEqual(Uidl.Id, XRT_STR_LITERAL("uid-1")),
		"POP3 UIDL item mismatch");

	testRequire(__xrtPop3ClientListAll(pClient, Deadline, NULL) &&
		testPop3ListNext(pClient, 1u, 120u, Deadline) &&
		testPop3ListNext(pClient, 2u, 180u, Deadline) &&
		(__xrtPop3ClientNext(pClient, &Line, Deadline, NULL) ==
			XMAIL_NEXT_END), "POP3 LIST multiline mismatch");
	testRequire(__xrtPop3ClientUidlAll(pClient, Deadline, NULL) &&
		testPop3UidlNext(
			pClient, 1u, XRT_STR_LITERAL("uid-1"), Deadline
		) && testPop3UidlNext(
			pClient, 2u, XRT_STR_LITERAL("uid-2"), Deadline
		) && (__xrtPop3ClientNext(pClient, &Line, Deadline, NULL) ==
			XMAIL_NEXT_END), "POP3 UIDL multiline mismatch");

	testRequire(__xrtPop3ClientRetr(pClient, 1u, Deadline, NULL),
		"POP3 RETR begin failed");
	testRequire((__xrtPop3ClientNext(pClient, &Line, Deadline, NULL) ==
		XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Line, XRT_STR_LITERAL("Header: value")),
		"POP3 RETR header mismatch");
	testRequire((__xrtPop3ClientNext(pClient, &Line, Deadline, NULL) ==
		XMAIL_NEXT_ITEM) && (Line.Size == 0),
		"POP3 RETR blank line mismatch");
	testRequire((__xrtPop3ClientNext(pClient, &Line, Deadline, NULL) ==
		XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Line, XRT_STR_LITERAL(".dot")),
		"POP3 RETR dot transparency mismatch");
	testRequire(__xrtPop3ClientNext(pClient, &Line, Deadline, NULL) ==
		XMAIL_NEXT_END, "POP3 RETR terminator mismatch");

	testRequire(__xrtPop3ClientDelete(pClient, 1u, Deadline, NULL),
		"POP3 DELE failed");
	testRequire(__xrtPop3ClientReset(pClient, Deadline, NULL),
		"POP3 RSET failed");
	testRequire(__xrtPop3ClientNoop(pClient, Deadline, NULL),
		"POP3 NOOP failed");
	testRequire(__xrtPop3ClientQuit(pClient, Deadline, NULL) &&
		(xrtPop3ClientState(pClient) == XPOP3_CLIENT_CLOSED),
		"POP3 QUIT failed");
	testRequire(__xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"POP3 server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"POP3 server transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtPop3ClientDestroy(pClient);
	/* 0-3 协议错误/拒绝，4-7 断流/等待，8-9 畸形状态，10-11 发送前取消/到期。 */
	for ( unsigned iMode = 0u; iMode < 12u; iMode++ ) {
		testpop3faultserver Fault = { 0 };
		xcancel* pCancel = NULL;
		double CommandDeadline;
		bool bCommandOk;
		xpop3reply Last;

		Deadline = __xrtWaitAfter(UINT64_C(10000000));
		Fault.Listener = pListener;
		Fault.Deadline = Deadline;
		Fault.Mode = iMode;
		xrtAtomic32Init(&Fault.Release, 0u);
		if ( (iMode == 6u) || (iMode == 10u) ) {
			pCancel = xrtCancelCreate();
			testRequire(pCancel != NULL,
				"POP3 fault cancellation creation failed");
			Fault.Cancel = pCancel;
		}
		pThread = xrtThreadCreate(testPop3FaultServer, &Fault, 0);
		testRequire(pThread != NULL,
			"POP3 fault server thread creation failed");
		Config.ReadCapabilities = false;
		pClient = __xrtPop3ClientOpen(&Config, Deadline, NULL);
		testRequire(pClient != NULL, "POP3 fault scenario open failed");
		if ( (iMode != 7u) && (iMode != 9u) ) {
			testRequire(__xrtPop3ClientLogin(pClient,
				XRT_STR_LITERAL("user"), XRT_STR_LITERAL("pass"),
				true, Deadline, NULL),
				"POP3 fault scenario login failed");
		}
		xrtClearError();
		CommandDeadline = (iMode == 5u || iMode == 7u) ?
			__xrtWaitAfter(UINT64_C(100000)) : Deadline;
		if ( iMode == 10u ) {
			testRequire(xrtCancelRequest(pCancel),
				"POP3 send cancellation request failed");
		} else if ( iMode == 11u ) {
			CommandDeadline = __xrtWaitAfter(0);
		}
		if ( (iMode == 7u) || (iMode == 9u) ) {
			bCommandOk = __xrtPop3ClientLogin(pClient,
				XRT_STR_LITERAL("user"), XRT_STR_LITERAL("pass"),
				true, CommandDeadline, NULL);
		} else if ( iMode == 1u ) {
			bCommandOk = __xrtPop3ClientList(
				pClient, 1u, &List, Deadline, NULL);
		} else if ( iMode == 2u ) {
			bCommandOk = __xrtPop3ClientUidl(
				pClient, 1u, &Uidl, Deadline, NULL);
		} else if ( iMode >= 10u ) {
			bCommandOk = __xrtPop3ClientSend(pClient,
				XRT_STR_LITERAL("STAT"), CommandDeadline, pCancel);
		} else {
			bCommandOk = __xrtPop3ClientStat(
				pClient, &Stat, CommandDeadline, pCancel);
		}
		testRequire(!bCommandOk &&
			(((iMode == 5u || iMode == 7u) &&
			  xrtErrorKind(xrtGetError()) == XERR_TIMEOUT) ||
			 (iMode == 6u &&
			  xrtErrorKind(xrtGetError()) == XERR_CANCELLED) ||
			 (iMode == 10u &&
			  xrtErrorKind(xrtGetError()) == XERR_CANCELLED) ||
			 (iMode == 11u &&
			  xrtErrorKind(xrtGetError()) == XERR_TIMEOUT) ||
			 ((iMode == 8u || iMode == 9u) &&
			  xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) ||
			 (iMode == 4u &&
			  xrtErrorKind(xrtGetError()) != XERR_NONE) ||
			 (iMode < 4u &&
			  xrtErrorKind(xrtGetError()) == XERR_PROTOCOL)) &&
			xrtPop3ClientLastReply(pClient, &Last),
			"POP3 fault response or LastReply mismatch");
		if ( iMode == 7u ) {
			testRequire(testMailViewEqual(
				Last.Source, XRT_STR_LITERAL("+OK ready")),
				"POP3 auth timeout changed greeting reply");
		} else if ( iMode == 9u ) {
			testRequire(testMailViewEqual(
				Last.Source, XRT_STR_LITERAL("+OK user")),
				"POP3 malformed PASS reply changed prior reply");
		} else if ( iMode >= 4u ) {
			testRequire(testMailViewEqual(
				Last.Source, XRT_STR_LITERAL("+OK locked")),
				"POP3 transport failure changed the prior reply");
		}
		if ( ((iMode >= 5u) && (iMode <= 7u)) ||
			(iMode >= 10u) ) {
			xrtAtomic32Store(&Fault.Release, 1u, XMEMORY_RELEASE);
		}
		if ( iMode == 6u ) {
			testRequire(__xrtThreadWaitUntil(pThread, Deadline) ==
				XWAIT_OK && Fault.Success &&
				(xrtThreadExitCode(pThread) == 0),
				"POP3 cancelled command did not close the connection");
		}
		if ( iMode >= 10u ) {
			testRequire(__xrtThreadWaitUntil(pThread, Deadline) ==
				XWAIT_OK && Fault.Success &&
				(xrtThreadExitCode(pThread) == 0),
				"POP3 stopped send reached the server");
		}
		if ( iMode == 3u ) {
			testRequire(xrtPop3ClientState(pClient) ==
				XPOP3_CLIENT_TRANSACTION &&
				__xrtPop3ClientNoop(pClient, Deadline, NULL) &&
				__xrtPop3ClientQuit(pClient, Deadline, NULL),
				"POP3 -ERR must leave transaction usable");
		} else {
			xrtClearError();
			testRequire(xrtPop3ClientState(pClient) ==
				XPOP3_CLIENT_FAILED &&
				!__xrtPop3ClientNoop(pClient, Deadline, NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
				xrtPop3ClientAbort(pClient),
				"POP3 malformed +OK must fail the session");
		}
		xrtPop3ClientDestroy(pClient);
		if ( iMode < 10u ) {
			testRequire(__xrtThreadWaitUntil(pThread, Deadline) ==
				XWAIT_OK && Fault.Success &&
				(xrtThreadExitCode(pThread) == 0),
				"POP3 fault server transcript mismatch");
		}
		xrtThreadDestroy(pThread);
		xrtCancelDestroy(pCancel);
	}

	testRequire(xrtNetListenerClose(pListener),
		"POP3 listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"POP3 resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"POP3 engine destroy failed");
	return 0;
}
