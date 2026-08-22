#include "../test.h"



typedef struct testimapappendserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} testimapappendserver;



static xnetaddr TestImapAppendAddress;



/* 把测试域名解析到本地 APPEND 服务器。 */
static xnetaddrlist* testImapAppendResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap-append.test") != 0 ) {
		return NULL;
	}
	Address = TestImapAppendAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 发送完整服务器字节。 */
static bool testImapAppendSend(
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



/* 精确接收并比较客户端字节。 */
static bool testImapAppendReceive(
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



/* 服务同步、非同步和上限拒绝后的完整 APPEND 转录。 */
static int32 testImapAppendServer(ptr pData)
{
	testimapappendserver* pServer = (testimapappendserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testImapAppendSend(
		pStream,
		"* PREAUTH imap-append.test ready\r\n",
		sizeof("* PREAUTH imap-append.test ready\r\n") - 1u,
		pServer->Deadline
	) && testImapAppendReceive(
		pStream,
		"A00000001 CAPABILITY\r\n",
		sizeof("A00000001 CAPABILITY\r\n") - 1u,
		pServer->Deadline
	) && testImapAppendSend(
		pStream,
		"* CAPABILITY IMAP4rev1 UIDPLUS LITERAL+ APPENDLIMIT=64\r\n"
		"A00000001 OK capability complete\r\n",
		sizeof(
			"* CAPABILITY IMAP4rev1 UIDPLUS LITERAL+ APPENDLIMIT=64\r\n"
			"A00000001 OK capability complete\r\n"
		) - 1u,
		pServer->Deadline
	) && testImapAppendReceive(
		pStream,
		"A00000002 APPEND \"INBOX\" (\\Seen) "
		"\"16-Aug-2026 12:00:00 +0800\" {11}\r\n",
		sizeof(
			"A00000002 APPEND \"INBOX\" (\\Seen) "
			"\"16-Aug-2026 12:00:00 +0800\" {11}\r\n"
		) - 1u,
		pServer->Deadline
	) && testImapAppendSend(
		pStream,
		"+ ready for literal\r\n",
		sizeof("+ ready for literal\r\n") - 1u,
		pServer->Deadline
	) && testImapAppendReceive(
		pStream,
		"hello world\r\n",
		sizeof("hello world\r\n") - 1u,
		pServer->Deadline
	) && testImapAppendSend(
		pStream,
		"A00000002 OK [APPENDUID 42 9] appended\r\n",
		sizeof("A00000002 OK [APPENDUID 42 9] appended\r\n") - 1u,
		pServer->Deadline
	) && testImapAppendReceive(
		pStream,
		"A00000003 APPEND \"Drafts\" {5+}\r\nworld\r\n",
		sizeof("A00000003 APPEND \"Drafts\" {5+}\r\nworld\r\n") - 1u,
		pServer->Deadline
	) && testImapAppendSend(
		pStream,
		"A00000003 OK appended\r\n",
		sizeof("A00000003 OK appended\r\n") - 1u,
		pServer->Deadline
	) && testImapAppendReceive(
		pStream,
		"A00000004 LOGOUT\r\n",
		sizeof("A00000004 LOGOUT\r\n") - 1u,
		pServer->Deadline
	) && testImapAppendSend(
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



/* 验证 APPEND 的流式字节约束、能力选择与完成结果。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	testimapappendserver Server;
	ximapclientconfig ClientConfig;
	ximapappendconfig AppendConfig;
	ximapappendresult Result;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	ximapclient* pClient;
	xthread* pThread;
	xdeadline Deadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP APPEND engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP APPEND loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapAppendAddress
	), "IMAP APPEND listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapAppendResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "IMAP APPEND resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testImapAppendServer, &Server, 0);
	testRequire(pThread != NULL, "IMAP APPEND server thread creation failed");
	xrtImapClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "imap-append.test";
	ClientConfig.Net.Port = TestImapAppendAddress.Port;
	pClient = xrtImapClientOpen(&ClientConfig, Deadline, NULL);
	testRequire((pClient != NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED) &&
		(xrtImapClientAppendLimit(pClient) == UINT64_C(64)),
		"IMAP APPEND client open failed");

	xrtImapAppendConfigInit(&AppendConfig);
	AppendConfig.Mailbox = XRT_STR_LITERAL("INBOX");
	AppendConfig.Flags = XRT_STR_LITERAL("(\\Seen)");
	AppendConfig.InternalDate = XRT_STR_LITERAL("16-Aug-2026 12:00:00 +0800");
	AppendConfig.Size = 11u;
	AppendConfig.Literal = XIMAP_LITERAL_SYNC;
	testRequire(xrtImapClientAppendBegin(
		pClient,
		&AppendConfig,
		Deadline,
		NULL
	), "synchronizing IMAP APPEND begin failed");
	xrtClearError();
	testRequire(!xrtImapClientAppendWrite(
		pClient,
		"hello world!",
		12u,
		Deadline,
		NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtImapClientAppendRemaining(pClient) == 11u),
		"IMAP APPEND accepted bytes beyond its literal size");
	xrtClearError();
	testRequire(xrtImapClientAppendWrite(
		pClient,
		"hello ",
		6u,
		Deadline,
		NULL
	) && xrtImapClientAppendWrite(
		pClient,
		"world",
		5u,
		Deadline,
		NULL
	) && (xrtImapClientAppendRemaining(pClient) == 0) &&
		xrtImapClientAppendEnd(pClient, &Result, Deadline, NULL) &&
		Result.Present && (Result.UidValidity == UINT64_C(42)) &&
		(Result.Uid == UINT64_C(9)),
		"streaming IMAP APPEND failed");

	xrtImapAppendConfigInit(&AppendConfig);
	AppendConfig.Mailbox = XRT_STR_LITERAL("Drafts");
	AppendConfig.Size = 5u;
	testRequire(xrtImapClientAppend(
		pClient,
		&AppendConfig,
		"world",
		&Result,
		Deadline,
		NULL
	) && !Result.Present, "non-synchronizing IMAP APPEND failed");

	AppendConfig.Size = 65u;
	xrtClearError();
	testRequire(!xrtImapClientAppendBegin(
		pClient,
		&AppendConfig,
		Deadline,
		NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
		"IMAP APPENDLIMIT did not reject before upload");
	xrtClearError();
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL),
		"IMAP APPEND logout failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP APPEND server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP APPEND transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtImapClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"IMAP APPEND listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP APPEND resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP APPEND engine destroy failed");
	return 0;
}
