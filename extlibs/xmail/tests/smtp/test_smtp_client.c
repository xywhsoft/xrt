#include "../test.h"



typedef struct testsmtpserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} testsmtpserver;



static xnetaddr TestSmtpAddress;

static const unsigned char TestSmtpChunkFirst[] = {
	0x00u, (unsigned char)'.', (unsigned char)'\r',
	(unsigned char)'\n', 0xffu
};

static const unsigned char TestSmtpChunkLast[] = {
	(unsigned char)'x', 0x00u, (unsigned char)'.'
};



/* 把测试域名解析到本地 SMTP Listener。 */
static xnetaddrlist* testSmtpResolve(
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
	Address = TestSmtpAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 发送测试服务器的完整短响应。 */
static bool testSmtpServerSend(
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



/* 精确接收并比较一段客户端字节，不依赖 TCP 分包边界。 */
static bool testSmtpServerReceive(
	xnetstream* pStream,
	const void* pExpected,
	size_t iExpected,
	xdeadline iDeadline
)
{
	const unsigned char* pBytesExpected =
		(const unsigned char*)pExpected;
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
			(memcmp(
				Bytes.Data,
				pBytesExpected + iReceived,
				Bytes.Size
			) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iReceived += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}



/* 等待客户端以 FIN 或 RST 终止连接，不接受额外线路字节。 */
static bool testSmtpServerPeerClosed(
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



/* 模拟完整明文 SMTP 事务并验证线路上的原始字节。 */
static int32 testSmtpServer(ptr pData)
{
	testsmtpserver* pServer = (testsmtpserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testSmtpServerSend(
		pStream,
		"220 smtp.test ready\r\n",
		21u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"EHLO client.test\r\n",
		18u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250-smtp.test\r\n"
		"250-SIZE 4096\r\n"
		"250-PIPELINING\r\n"
		"250-BINARYMIME\r\n"
		"250 CHUNKING\r\n",
		sizeof(
			"250-smtp.test\r\n"
			"250-SIZE 4096\r\n"
			"250-PIPELINING\r\n"
			"250-BINARYMIME\r\n"
			"250 CHUNKING\r\n"
		) - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"MAIL FROM:<sender@test> SIZE=36\r\n",
		33u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 sender accepted\r\n",
		21u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"RCPT TO:<target@test> NOTIFY=SUCCESS\r\n",
		38u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"251 recipient forwarded\r\n",
		25u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"DATA\r\n",
		6u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"354 send data\r\n",
		15u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"Subject: test\r\n\r\n..first\r\nsecond\r\n.\r\n",
		37u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 queued\r\n",
		12u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"NOOP\r\n",
		6u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 still here\r\n",
		16u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"MAIL FROM:<sender@test> BODY=BINARYMIME\r\n",
		sizeof("MAIL FROM:<sender@test> BODY=BINARYMIME\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 binary sender accepted\r\n",
		sizeof("250 binary sender accepted\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"RCPT TO:<binary@test>\r\n",
		sizeof("RCPT TO:<binary@test>\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 binary recipient accepted\r\n",
		sizeof("250 binary recipient accepted\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"BDAT 5\r\n",
		sizeof("BDAT 5\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		TestSmtpChunkFirst,
		sizeof(TestSmtpChunkFirst),
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 first chunk\r\n",
		sizeof("250 first chunk\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"BDAT 0\r\n",
		sizeof("BDAT 0\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 empty chunk\r\n",
		sizeof("250 empty chunk\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"BDAT 3 LAST\r\n",
		sizeof("BDAT 3 LAST\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		TestSmtpChunkLast,
		sizeof(TestSmtpChunkLast),
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 binary queued\r\n",
		sizeof("250 binary queued\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"MAIL FROM:<sender@test>\r\n",
		sizeof("MAIL FROM:<sender@test>\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 sender accepted\r\n",
		sizeof("250 sender accepted\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"RCPT TO:<reject@test>\r\n",
		sizeof("RCPT TO:<reject@test>\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 recipient accepted\r\n",
		sizeof("250 recipient accepted\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"BDAT 1 LAST\r\n!",
		sizeof("BDAT 1 LAST\r\n!") - 1u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"554 chunk rejected\r\n",
		sizeof("554 chunk rejected\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"RSET\r\n",
		6u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 reset\r\n",
		11u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"QUIT\r\n",
		6u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"221 closing\r\n",
		13u,
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



/* 完成握手后只等待客户端主动异常中止。 */
static int32 testSmtpAbortServer(ptr pData)
{
	testsmtpserver* pServer = (testsmtpserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testSmtpServerSend(
		pStream,
		"220 smtp.test ready\r\n",
		sizeof("220 smtp.test ready\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerReceive(
		pStream,
		"EHLO client.test\r\n",
		sizeof("EHLO client.test\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerSend(
		pStream,
		"250 smtp.test\r\n",
		sizeof("250 smtp.test\r\n") - 1u,
		pServer->Deadline
	) && testSmtpServerPeerClosed(
		pStream,
		pServer->Deadline
	);
	pServer->Success = bSuccess;
	xrtNetStreamDestroy(pStream);
	return bSuccess ? 0 : 2;
}



/* 验证默认配置、完整会话状态机和流式 DATA 点转义。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xsmtpclientconfig Config;
	testsmtpserver Server;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xsmtpclient* pClient;
	xsmtpreply Reply;
	xbytesview Chunk;
	xthread* pThread;
	xdeadline Deadline;

	xrtSmtpClientConfigInit(&Config);
	testRequire((Config.Net.Port == 25u) &&
		(Config.Net.Security == XMAIL_SECURITY_PLAIN) &&
		testMailViewEqual(Config.Hello, XRT_STR_LITERAL("localhost")) &&
		(Config.ReplyLines == XSMTP_REPLY_LINES_DEFAULT) &&
		Config.HeloFallback,
		"SMTP client default configuration mismatch");
	testRequire(!xrtSmtpClientConfigValid(&Config),
		"SMTP client accepted missing engine and resolver");
	testRequire(!xrtSmtpClientAbort(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SMTP null client abort mismatch");
	xrtClearError();

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"SMTP client engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "SMTP client loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestSmtpAddress
	), "SMTP client listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testSmtpResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"SMTP client resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testSmtpServer, &Server, 0);
	testRequire(pThread != NULL, "SMTP test server thread creation failed");
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "smtp.test";
	Config.Net.Port = TestSmtpAddress.Port;
	Config.Hello = (xstrview)XRT_STR_LITERAL("client.test");
	pClient = xrtSmtpClientOpen(&Config, Deadline, NULL);
	testRequire(pClient != NULL, "SMTP client open failed");
	testRequire((xrtSmtpClientCapabilities(pClient) &
		(XSMTP_CAP_SIZE | XSMTP_CAP_PIPELINING |
		 XSMTP_CAP_BINARYMIME | XSMTP_CAP_CHUNKING)) ==
		(XSMTP_CAP_SIZE | XSMTP_CAP_PIPELINING |
		 XSMTP_CAP_BINARYMIME | XSMTP_CAP_CHUNKING),
		"SMTP EHLO capability snapshot mismatch");
	testRequire(xrtSmtpClientSizeLimit(pClient) == UINT64_C(4096),
		"SMTP SIZE limit mismatch");
	testRequire(xrtSmtpClientLastReply(pClient, &Reply) &&
		(Reply.Code == 250) && (Reply.Lines == 5u) &&
		testMailViewEqual(Reply.Text, XRT_STR_LITERAL("CHUNKING")),
		"SMTP EHLO final reply mismatch");

	testRequire(xrtSmtpClientMail(
		pClient,
		XRT_STR_LITERAL("sender@test"),
		XRT_STR_LITERAL("SIZE=36"),
		Deadline,
		NULL
	) && (xrtSmtpClientState(pClient) == XSMTP_CLIENT_MAIL),
		"SMTP MAIL transaction failed");
	testRequire(xrtSmtpClientRcpt(
		pClient,
		XRT_STR_LITERAL("target@test"),
		XRT_STR_LITERAL("NOTIFY=SUCCESS"),
		Deadline,
		NULL
	) && (xrtSmtpClientState(pClient) == XSMTP_CLIENT_RECIPIENT),
		"SMTP RCPT transaction failed");
	testRequire(xrtSmtpClientData(
		pClient,
		XRT_STR_LITERAL("Subject: test\r\n\r\n.first\r\nsecond\r\n"),
		Deadline,
		NULL
	) && (xrtSmtpClientState(pClient) == XSMTP_CLIENT_READY),
		"SMTP DATA transaction failed");
	testRequire(xrtSmtpClientLastReply(pClient, &Reply) &&
		(Reply.Code == 250) &&
		testMailViewEqual(Reply.Text, XRT_STR_LITERAL("queued")),
		"SMTP DATA final reply mismatch");
	testRequire(xrtSmtpClientNoop(pClient, Deadline, NULL),
		"SMTP NOOP failed");
	testRequire(xrtSmtpClientMail(
		pClient,
		XRT_STR_LITERAL("sender@test"),
		XRT_STR_LITERAL("BODY=BINARYMIME"),
		Deadline,
		NULL
	) && xrtSmtpClientRcpt(
		pClient,
		XRT_STR_LITERAL("binary@test"),
		XRT_STR_LITERAL(""),
		Deadline,
		NULL
	), "SMTP binary envelope failed");
	testRequire(xrtSmtpClientBdatBegin(
		pClient,
		sizeof(TestSmtpChunkFirst),
		false,
		Deadline,
		NULL
	) && (xrtSmtpClientState(pClient) == XSMTP_CLIENT_CHUNK),
		"SMTP fragmented BDAT begin failed");
	testRequire(!xrtSmtpClientNoop(pClient, Deadline, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SMTP command was accepted inside an active BDAT block");
	xrtClearError();
	Chunk.Data = TestSmtpChunkFirst;
	Chunk.Size = 2u;
	testRequire(xrtSmtpClientBdatWrite(
		pClient,
		Chunk,
		Deadline,
		NULL
	), "SMTP first BDAT fragment failed");
	Chunk.Data = TestSmtpChunkFirst + 2u;
	Chunk.Size = sizeof(TestSmtpChunkFirst) - 2u;
	testRequire(xrtSmtpClientBdatWrite(
		pClient,
		Chunk,
		Deadline,
		NULL
	) && xrtSmtpClientBdatEnd(pClient, Deadline, NULL),
		"SMTP fragmented BDAT completion failed");
	Chunk.Data = TestSmtpChunkFirst;
	Chunk.Size = 0;
	testRequire(xrtSmtpClientBdat(
		pClient,
		Chunk,
		false,
		Deadline,
		NULL
	), "SMTP zero-length BDAT failed");
	Chunk.Data = TestSmtpChunkLast;
	Chunk.Size = sizeof(TestSmtpChunkLast);
	testRequire(xrtSmtpClientBdat(
		pClient,
		Chunk,
		true,
		Deadline,
		NULL
	) && (xrtSmtpClientState(pClient) == XSMTP_CLIENT_READY),
		"SMTP final binary BDAT failed");
	testRequire(xrtSmtpClientMail(
		pClient,
		XRT_STR_LITERAL("sender@test"),
		XRT_STR_LITERAL(""),
		Deadline,
		NULL
	) && xrtSmtpClientRcpt(
		pClient,
		XRT_STR_LITERAL("reject@test"),
		XRT_STR_LITERAL(""),
		Deadline,
		NULL
	), "SMTP rejected chunk envelope failed");
	Chunk.Data = (const unsigned char*)"!";
	Chunk.Size = 1u;
	testRequire(!xrtSmtpClientBdat(
		pClient,
		Chunk,
		true,
		Deadline,
		NULL
	) && (xrtSmtpClientState(pClient) == XSMTP_CLIENT_CHUNK) &&
		xrtSmtpClientLastReply(pClient, &Reply) && (Reply.Code == 554),
		"SMTP rejected BDAT state mismatch");
	xrtClearError();
	testRequire(xrtSmtpClientReset(pClient, Deadline, NULL),
		"SMTP RSET after rejected BDAT failed");
	testRequire(xrtSmtpClientQuit(pClient, Deadline, NULL) &&
		(xrtSmtpClientState(pClient) == XSMTP_CLIENT_CLOSED),
		"SMTP QUIT failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"SMTP test server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"SMTP test server transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtSmtpClientDestroy(pClient);

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testSmtpAbortServer, &Server, 0);
	testRequire(pThread != NULL,
		"SMTP abort server thread creation failed");
	pClient = xrtSmtpClientOpen(&Config, Deadline, NULL);
	testRequire((pClient != NULL) && xrtSmtpClientAbort(pClient) &&
		(xrtSmtpClientState(pClient) == XSMTP_CLIENT_CLOSED) &&
		xrtSmtpClientAbort(pClient),
		"SMTP active abort or idempotence failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"SMTP abort server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"SMTP abort server transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtSmtpClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"SMTP client listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"SMTP client resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"SMTP client engine destroy failed");
	return 0;
}
