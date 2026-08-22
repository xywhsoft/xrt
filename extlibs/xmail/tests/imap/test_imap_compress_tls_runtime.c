#include "../test.h"
#include "../test_tls.h"



typedef struct testimapcompresstlsbuffer {
	unsigned char Data[1024];
	size_t Size;
} testimapcompresstlsbuffer;



typedef struct testimapcompresstlssend {
	xtlsstream* Stream;
	xdeadline Deadline;
} testimapcompresstlssend;



typedef struct testimapcompresstlsserver {
	xnetlistener* Listener;
	const xtlsserverconfig* Tls;
	xdeadline Deadline;
	bool Success;
} testimapcompresstlsserver;



static xnetaddr TestImapCompressTlsAddress;



/* 把测试域名解析到本地 IMAP STARTTLS 服务端。 */
static xnetaddrlist* testImapCompressTlsResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap-compress-tls.test") != 0 ) {
		return NULL;
	}
	Address = TestImapCompressTlsAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 把压缩器或解压器输出追加到有界测试缓冲。 */
static bool testImapCompressTlsBufferWrite(xbytesview Data, ptr pData)
{
	testimapcompresstlsbuffer* pBuffer =
		(testimapcompresstlsbuffer*)pData;

	if ( Data.Size > (sizeof(pBuffer->Data) - pBuffer->Size) ) {
		return false;
	}
	if ( Data.Size != 0 ) {
		memcpy(pBuffer->Data + pBuffer->Size, Data.Data, Data.Size);
	}
	pBuffer->Size += Data.Size;
	return true;
}



/* 把压缩输出作为 TLS 应用数据完整发送。 */
static bool testImapCompressTlsStreamWrite(xbytesview Data, ptr pData)
{
	testimapcompresstlssend* pSend =
		(testimapcompresstlssend*)pData;

	return testMailTlsSend(
		pSend->Stream,
		(cstr)Data.Data,
		Data.Size,
		pSend->Deadline
	);
}



/* 从 TLS 应用数据中取得一块仍保持压缩格式的拥有型字节。 */
static xnetbytes* testImapCompressTlsRawReceive(
	xtlsstream* pStream,
	xdeadline iDeadline
)
{
	xfuture* pFuture = xrtTlsStreamRecvAsync(pStream, 256u);
	xnetbytes* pBytes = NULL;

	if ( testMailTlsFuture(pFuture, iDeadline) ) {
		pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pFuture));
	}
	xrtFutureDestroy(pFuture);
	return pBytes;
}



/* 在 TLS 内部的持续 raw-DEFLATE 流中发送一个协议片段。 */
static bool testImapCompressTlsSend(
	xdeflate* pDeflate,
	xtlsstream* pStream,
	cstr sText,
	size_t iSize,
	xdeadline iDeadline
)
{
	testimapcompresstlssend Send;

	Send.Stream = pStream;
	Send.Deadline = iDeadline;
	return xrtDeflateWrite(
		pDeflate,
		(xbytesview) { (cbytes)sText, iSize },
		XDEFLATE_FLUSH_SYNC,
		testImapCompressTlsStreamWrite,
		&Send
	);
}



/* 解压并精确比较 TLS 内收到的下一段 IMAP 命令。 */
static bool testImapCompressTlsReceive(
	xinflate* pInflate,
	testimapcompresstlsbuffer* pPlain,
	xtlsstream* pStream,
	cstr sExpected,
	size_t iExpected,
	xdeadline iDeadline
)
{
	while ( pPlain->Size < iExpected ) {
		xnetbytes* pBytes = testImapCompressTlsRawReceive(
			pStream,
			iDeadline
		);
		xbytesview Bytes;
		bool bSuccess;

		if ( pBytes == NULL ) {
			return false;
		}
		Bytes = xrtNetBytesView(pBytes);
		bSuccess = (Bytes.Size != 0) && xrtInflateWrite(
			pInflate,
			Bytes,
			false,
			testImapCompressTlsBufferWrite,
			pPlain
		);
		xrtNetBytesDestroy(pBytes);
		if ( !bSuccess ) {
			return false;
		}
	}
	if ( memcmp(pPlain->Data, sExpected, iExpected) != 0 ) {
		return false;
	}
	pPlain->Size -= iExpected;
	if ( pPlain->Size != 0 ) {
		memmove(pPlain->Data, pPlain->Data + iExpected, pPlain->Size);
	}
	return true;
}



/* 模拟 STARTTLS、认证、COMPRESS 和压缩命令的完整服务端链路。 */
static int32 testImapCompressTlsServer(ptr pData)
{
	testimapcompresstlsserver* pServer =
		(testimapcompresstlsserver*)pData;
	xnetstream* pTcp = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	xdeflateconfig DeflateConfig;
	xinflateconfig InflateConfig;
	testimapcompresstlsbuffer Plain = { { 0 }, 0 };
	xtlsstream* pTls;
	xdeflate* pDeflate;
	xinflate* pInflate;
	xfuture* pClose;
	bool bSuccess;

	if ( pTcp == NULL ) {
		return 1;
	}
	bSuccess = testMailTcpSend(
		pTcp,
		"* OK imap-compress-tls.test ready\r\n",
		sizeof("* OK imap-compress-tls.test ready\r\n") - 1u,
		pServer->Deadline
	) && testMailTcpReceive(
		pTcp,
		"A00000001 CAPABILITY\r\n",
		sizeof("A00000001 CAPABILITY\r\n") - 1u,
		pServer->Deadline
	) && testMailTcpSend(
		pTcp,
		"* CAPABILITY IMAP4rev1 STARTTLS\r\n"
		"A00000001 OK capability complete\r\n",
		sizeof(
			"* CAPABILITY IMAP4rev1 STARTTLS\r\n"
			"A00000001 OK capability complete\r\n"
		) - 1u,
		pServer->Deadline
	) && testMailTcpReceive(
		pTcp,
		"A00000002 STARTTLS\r\n",
		sizeof("A00000002 STARTTLS\r\n") - 1u,
		pServer->Deadline
	) && testMailTcpSend(
		pTcp,
		"A00000002 OK begin TLS\r\n",
		sizeof("A00000002 OK begin TLS\r\n") - 1u,
		pServer->Deadline
	);
	if ( !bSuccess ) {
		xrtNetStreamDestroy(pTcp);
		return 2;
	}
	pTls = testMailTlsUpgrade(&pTcp, pServer->Tls, pServer->Deadline);
	if ( pTls == NULL ) {
		xrtNetStreamDestroy(pTcp);
		return 3;
	}
	bSuccess = testMailTlsReceive(
		pTls,
		"A00000003 CAPABILITY\r\n",
		sizeof("A00000003 CAPABILITY\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsSend(
		pTls,
		"* CAPABILITY IMAP4rev1 AUTH=PLAIN COMPRESS=DEFLATE\r\n"
		"A00000003 OK capability complete\r\n",
		sizeof(
			"* CAPABILITY IMAP4rev1 AUTH=PLAIN COMPRESS=DEFLATE\r\n"
			"A00000003 OK capability complete\r\n"
		) - 1u,
		pServer->Deadline
	) && testMailTlsReceive(
		pTls,
		"A00000004 AUTHENTICATE PLAIN\r\n",
		sizeof("A00000004 AUTHENTICATE PLAIN\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsSend(
		pTls,
		"+ \r\n",
		sizeof("+ \r\n") - 1u,
		pServer->Deadline
	) && testMailTlsReceive(
		pTls,
		"AHVzZXIAcGFzcw==\r\n",
		sizeof("AHVzZXIAcGFzcw==\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsSend(
		pTls,
		"A00000004 OK authenticated\r\n",
		sizeof("A00000004 OK authenticated\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsReceive(
		pTls,
		"A00000005 COMPRESS DEFLATE\r\n",
		sizeof("A00000005 COMPRESS DEFLATE\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsSend(
		pTls,
		"A00000005 OK compression active\r\n",
		sizeof("A00000005 OK compression active\r\n") - 1u,
		pServer->Deadline
	);
	xrtDeflateConfigInit(&DeflateConfig);
	DeflateConfig.Format = XDEFLATE_RAW;
	xrtInflateConfigInit(&InflateConfig);
	InflateConfig.Format = XINFLATE_RAW;
	pDeflate = bSuccess ? xrtDeflateCreate(&DeflateConfig) : NULL;
	pInflate = bSuccess ? xrtInflateCreate(&InflateConfig) : NULL;
	bSuccess = bSuccess && (pDeflate != NULL) && (pInflate != NULL) &&
		testImapCompressTlsReceive(
			pInflate,
			&Plain,
			pTls,
			"A00000006 NOOP\r\n",
			sizeof("A00000006 NOOP\r\n") - 1u,
			pServer->Deadline
		) && testImapCompressTlsSend(
			pDeflate,
			pTls,
			"A00000006 OK noop complete\r\n",
			sizeof("A00000006 OK noop complete\r\n") - 1u,
			pServer->Deadline
		) && testImapCompressTlsReceive(
			pInflate,
			&Plain,
			pTls,
			"A00000007 LOGOUT\r\n",
			sizeof("A00000007 LOGOUT\r\n") - 1u,
			pServer->Deadline
		) && testImapCompressTlsSend(
			pDeflate,
			pTls,
			"* BYE signing off\r\nA00000007 OK logout complete\r\n",
			sizeof(
				"* BYE signing off\r\n"
				"A00000007 OK logout complete\r\n"
			) - 1u,
			pServer->Deadline
		);
	xrtDeflateDestroy(pDeflate);
	xrtInflateDestroy(pInflate);
	if ( bSuccess ) {
		pClose = xrtTlsStreamWaitAsync(pTls, XTLS_STREAM_WAIT_CLOSE);
		bSuccess = (pClose != NULL) && xrtTlsStreamClose(pTls) &&
			testMailTlsFuture(pClose, pServer->Deadline);
		xrtFutureDestroy(pClose);
	}
	pServer->Success = bSuccess;
	if ( !bSuccess ) {
		(void)xrtTlsStreamAbort(pTls);
	}
	xrtTlsStreamDestroy(pTls);
	return bSuccess ? 0 : 4;
}



/* 验证 raw-DEFLATE 位于 IMAP 与真实 TLS 1.3 记录层之间。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xtlsverifierconfig VerifierConfig;
	xtlsserverconfig ServerConfig;
	testimapcompresstlsserver Server;
	ximapclientconfig ClientConfig;
	ximapauthconfig AuthConfig;
	ximapcompressconfig CompressConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	ximapclient* pClient;
	xthread* pThread;
	xdeadline Deadline;
	ximapevent Event;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"IMAP COMPRESS TLS fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"IMAP COMPRESS TLS verifier creation failed");
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Context = pContext;
	ServerConfig.Identity = pIdentity;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP COMPRESS TLS engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP COMPRESS TLS loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapCompressTlsAddress
	), "IMAP COMPRESS TLS listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapCompressTlsResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"IMAP COMPRESS TLS resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Tls = &ServerConfig;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testImapCompressTlsServer, &Server, 0);
	testRequire(pThread != NULL,
		"IMAP COMPRESS TLS server thread creation failed");
	xrtImapClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "imap-compress-tls.test";
	ClientConfig.Net.Port = TestImapCompressTlsAddress.Port;
	ClientConfig.Net.Security = XMAIL_SECURITY_STARTTLS;
	ClientConfig.Net.Tls.Context = pContext;
	ClientConfig.Net.Tls.Verifier = pVerifier;
	pClient = xrtImapClientOpen(&ClientConfig, Deadline, NULL);
	testRequire((pClient != NULL) &&
		(xrtImapClientSecurity(pClient) == XMAIL_SECURITY_TLS) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_NOT_AUTHENTICATED) &&
		((xrtImapClientCapabilities(pClient) &
		 (XIMAP_CAP_AUTH_PLAIN | XIMAP_CAP_COMPRESS_DEFLATE)) ==
		 (XIMAP_CAP_AUTH_PLAIN | XIMAP_CAP_COMPRESS_DEFLATE)),
		"IMAP COMPRESS STARTTLS open failed");

	xrtImapAuthConfigInit(&AuthConfig);
	AuthConfig.Method = XIMAP_AUTH_PLAIN;
	AuthConfig.Username = XRT_STR_LITERAL("user");
	AuthConfig.Secret = XRT_STR_LITERAL("pass");
	AuthConfig.InitialResponse = false;
	testRequire(xrtImapClientAuth(
		pClient,
		&AuthConfig,
		Deadline,
		NULL
	) && (xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
		"IMAP COMPRESS TLS authentication failed");
	xrtImapCompressConfigInit(&CompressConfig);
	testRequire(xrtImapClientCompress(
		pClient,
		&CompressConfig,
		Deadline,
		NULL
	) && xrtImapClientCompressed(pClient) &&
		(xrtImapClientSecurity(pClient) == XMAIL_SECURITY_TLS),
		"IMAP COMPRESS over TLS negotiation failed");
	testRequire(xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("NOOP"),
		XRT_STR_LITERAL(""),
		Deadline,
		NULL
	) && (xrtImapClientNext(
		pClient,
		&Event,
		Deadline,
		NULL
	) == XMAIL_NEXT_END),
		"compressed IMAP command over TLS failed");
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_CLOSED),
		"compressed IMAP LOGOUT over TLS failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP COMPRESS TLS server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP COMPRESS TLS transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtImapClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"IMAP COMPRESS TLS listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP COMPRESS TLS resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP COMPRESS TLS engine destroy failed");
	return 0;
}
