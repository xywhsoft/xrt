#include "../test.h"



typedef struct testsmtpsubmitserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	cstr Message;
	size_t MessageSize;
	bool Success;
} testsmtpsubmitserver;



static xnetaddr TestSmtpSubmitAddress;



/* 把测试域名解析到本地 SMTP Listener。 */
static xnetaddrlist* testSmtpSubmitResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "submit.test") != 0 ) {
		return NULL;
	}
	Address = TestSmtpSubmitAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 完整发送一段测试服务器响应。 */
static bool testSmtpSubmitServerSend(
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



/* 精确接收并比较客户端字节，不依赖 TCP 分包边界。 */
static bool testSmtpSubmitServerReceiveBytes(
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



/* 精确接收一条常量命令。 */
static bool testSmtpSubmitServerReceive(
	xnetstream* pStream,
	cstr sExpected,
	xdeadline iDeadline
)
{
	return testSmtpSubmitServerReceiveBytes(
		pStream,
		sExpected,
		strlen(sExpected),
		iDeadline
	);
}



/* 验证恢复事务、自动 envelope、独立 envelope 和两次流式 DATA。 */
static int32 testSmtpSubmitServer(ptr pData)
{
	testsmtpsubmitserver* pServer = (testsmtpsubmitserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testSmtpSubmitServerSend(
		pStream,
		"220 submit.test ready\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"EHLO client.test\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"250-submit.test\r\n250 PIPELINING\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"MAIL FROM:<sender@example.com>\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"250 sender accepted\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"RCPT TO:<reject@example.net>\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"550 recipient rejected\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"RSET\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"250 reset\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"MAIL FROM:<sender@example.com>\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"250 sender accepted\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"RCPT TO:<to@example.net>\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"250 recipient accepted\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"RCPT TO:<cc@example.org>\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"251 recipient forwarded\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"RCPT TO:<hidden@example.io>\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"252 recipient accepted\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"DATA\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"354 send message\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceiveBytes(
		pStream,
		pServer->Message,
		pServer->MessageSize,
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"250 queued\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"MAIL FROM:<> RET=FULL\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"250 sender accepted\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"RCPT TO:<advanced@example.net> NOTIFY=SUCCESS\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"250 recipient accepted\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"DATA\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"354 send message\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceiveBytes(
		pStream,
		pServer->Message,
		pServer->MessageSize,
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"250 queued\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerReceive(
		pStream,
		"QUIT\r\n",
		pServer->Deadline
	) && testSmtpSubmitServerSend(
		pStream,
		"221 closing\r\n",
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



/* 初始化含 To、Cc 和 Bcc 的确定性消息。 */
static void testSmtpSubmitMessage(
	xmailmessage* pMessage,
	xmailaddress* pTo,
	xmailaddress* pCc,
	xmailaddress* pBcc
)
{
	xrtMailMessageInit(pMessage);
	pMessage->From = (xmailaddress){
		XRT_STR_LITERAL("Sender"),
		XRT_STR_LITERAL("sender@example.com")
	};
	*pTo = (xmailaddress){
		XRT_STR_LITERAL("To"),
		XRT_STR_LITERAL("to@example.net")
	};
	*pCc = (xmailaddress){
		XRT_STR_LITERAL("Cc"),
		XRT_STR_LITERAL("cc@example.org")
	};
	*pBcc = (xmailaddress){
		XRT_STR_LITERAL("Hidden"),
		XRT_STR_LITERAL("hidden@example.io")
	};
	pMessage->To = pTo;
	pMessage->ToCount = 1u;
	pMessage->Cc = pCc;
	pMessage->CcCount = 1u;
	pMessage->Bcc = pBcc;
	pMessage->BccCount = 1u;
	pMessage->Subject = XRT_STR_LITERAL("submit test");
	pMessage->Text = XRT_STR_LITERAL(".first line\r\nsecond line");
	pMessage->Date = XRT_STR_LITERAL("Tue, 12 May 2026 10:00:00 +0000");
	pMessage->MessageId = XRT_STR_LITERAL("<submit@example.com>");
}



/* 验证提交层不复制整封消息，并保持 envelope 与报文字段独立。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xsmtpclientconfig ClientConfig;
	testsmtpsubmitserver Server;
	xmailmessage Message;
	xmailmessage InvalidMessage;
	xmailaddress To;
	xmailaddress Cc;
	xmailaddress Bcc;
	xsmtprecipient RejectRecipient;
	xsmtprecipient AdvancedRecipient;
	xsmtpenvelope Envelope;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xsmtpclient* pClient;
	xthread* pThread;
	xdeadline Deadline;
	str sMessage;
	bytes pWireMessage;
	size_t iMessageSize;
	size_t iWireSize;

	testSmtpSubmitMessage(&Message, &To, &Cc, &Bcc);
	sMessage = xrtMailCompose(&Message, &iMessageSize);
	testRequire(sMessage != NULL, "SMTP submit expected message compose failed");
	testRequire(strstr(sMessage, "Bcc:") == NULL &&
		strstr(sMessage, "hidden@example.io") == NULL,
		"SMTP submit compose leaked Bcc into message content");
	pWireMessage = xrtMailDot(
		(xstrview){ sMessage, iMessageSize },
		true,
		&iWireSize
	);
	testRequire(pWireMessage != NULL,
		"SMTP submit expected dot transparency failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"SMTP submit engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "SMTP submit loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestSmtpSubmitAddress
	), "SMTP submit listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testSmtpSubmitResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "SMTP submit resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Message = (cstr)pWireMessage;
	Server.MessageSize = iWireSize;
	Server.Success = false;
	pThread = xrtThreadCreate(testSmtpSubmitServer, &Server, 0);
	testRequire(pThread != NULL, "SMTP submit server thread creation failed");
	xrtSmtpClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "submit.test";
	ClientConfig.Net.Port = TestSmtpSubmitAddress.Port;
	ClientConfig.Hello = (xstrview)XRT_STR_LITERAL("client.test");
	pClient = xrtSmtpClientOpen(&ClientConfig, Deadline, NULL);
	testRequire(pClient != NULL, "SMTP submit client open failed");

	InvalidMessage = Message;
	InvalidMessage.ToCount = 0;
	InvalidMessage.CcCount = 0;
	InvalidMessage.BccCount = 0;
	testRequire(!xrtSmtpSubmit(
		pClient,
		&InvalidMessage,
		Deadline,
		NULL
	) && (xrtSmtpClientState(pClient) == XSMTP_CLIENT_READY),
		"SMTP submit accepted invalid message or changed client state");
	xrtClearError();

	RejectRecipient = (xsmtprecipient){
		XRT_STR_LITERAL("reject@example.net"),
		XRT_STR_LITERAL("")
	};
	Envelope = (xsmtpenvelope){
		XRT_STR_LITERAL("sender@example.com"),
		XRT_STR_LITERAL(""),
		&RejectRecipient,
		1u
	};
	testRequire(!xrtSmtpSubmitEnvelope(
		pClient,
		&Envelope,
		&Message,
		Deadline,
		NULL
	) && (xrtSmtpClientState(pClient) == XSMTP_CLIENT_READY) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
		"SMTP submit did not preserve RCPT failure after RSET");
	xrtClearError();

	testRequire(xrtSmtpSubmit(pClient, &Message, Deadline, NULL) &&
		(xrtSmtpClientState(pClient) == XSMTP_CLIENT_READY),
		"SMTP submit automatic envelope failed");
	AdvancedRecipient = (xsmtprecipient){
		XRT_STR_LITERAL("advanced@example.net"),
		XRT_STR_LITERAL("NOTIFY=SUCCESS")
	};
	Envelope = (xsmtpenvelope){
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("RET=FULL"),
		&AdvancedRecipient,
		1u
	};
	testRequire(xrtSmtpSubmitEnvelope(
		pClient,
		&Envelope,
		&Message,
		Deadline,
		NULL
	), "SMTP submit independent envelope failed");
	testRequire(xrtSmtpClientQuit(pClient, Deadline, NULL),
		"SMTP submit QUIT failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"SMTP submit server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"SMTP submit server transcript mismatch");

	xrtThreadDestroy(pThread);
	xrtSmtpClientDestroy(pClient);
	testRequire(xrtNetListenerClose(pListener),
		"SMTP submit listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"SMTP submit resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"SMTP submit engine destroy failed");
	xrtFree(pWireMessage);
	xrtFree(sMessage);
	return 0;
}
