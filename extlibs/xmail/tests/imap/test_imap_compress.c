#include "../test.h"



typedef struct testimapcompressbuffer {
	unsigned char Data[1024];
	size_t Size;
} testimapcompressbuffer;



typedef struct testimapcompresssend {
	xnetstream* Stream;
	xdeadline Deadline;
} testimapcompresssend;



typedef struct testimapcompressserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} testimapcompressserver;



static xnetaddr TestImapCompressAddress;



/* 把测试域名解析到本地 IMAP COMPRESS 服务端。 */
static xnetaddrlist* testImapCompressResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap-compress.test") != 0 ) {
		return NULL;
	}
	Address = TestImapCompressAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 同步发送完整的原始 TCP 字节。 */
static bool testImapCompressRawSend(
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



/* 精确接收并比较一段未压缩的 IMAP 字节。 */
static bool testImapCompressRawReceive(
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



/* 把压缩器或解压器的一块输出追加到有界测试缓冲。 */
static bool testImapCompressBufferWrite(xbytesview Data, ptr pData)
{
	testimapcompressbuffer* pBuffer = (testimapcompressbuffer*)pData;

	if ( Data.Size > (sizeof(pBuffer->Data) - pBuffer->Size) ) {
		return false;
	}
	if ( Data.Size != 0 ) {
		memcpy(pBuffer->Data + pBuffer->Size, Data.Data, Data.Size);
	}
	pBuffer->Size += Data.Size;
	return true;
}



/* 把压缩器输出直接提交到测试 TCP 流。 */
static bool testImapCompressStreamWrite(xbytesview Data, ptr pData)
{
	testimapcompresssend* pSend = (testimapcompresssend*)pData;

	return testImapCompressRawSend(
		pSend->Stream,
		Data.Data,
		Data.Size,
		pSend->Deadline
	);
}



/* 在持续 raw-DEFLATE 流中发送一个可立即消费的协议片段。 */
static bool testImapCompressSend(
	xdeflate* pDeflate,
	xnetstream* pStream,
	cstr sText,
	size_t iSize,
	xdeadline iDeadline
)
{
	testimapcompresssend Send;

	Send.Stream = pStream;
	Send.Deadline = iDeadline;
	return xrtDeflateWrite(
		pDeflate,
		(xbytesview) { (cbytes)sText, iSize },
		XDEFLATE_FLUSH_SYNC,
		testImapCompressStreamWrite,
		&Send
	);
}



/* 增量解压客户端命令，保留同一压缩块中可能存在的后续命令。 */
static bool testImapCompressReceive(
	xinflate* pInflate,
	testimapcompressbuffer* pPlain,
	xnetstream* pStream,
	cstr sExpected,
	size_t iExpected,
	xdeadline iDeadline
)
{
	while ( pPlain->Size < iExpected ) {
		xnetbytes* pBytes = xrtNetStreamRecv(
			pStream,
			256u,
			iDeadline,
			NULL
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
			testImapCompressBufferWrite,
			pPlain
		);
		xrtNetBytesDestroy(pBytes);
		if ( !bSuccess || (pPlain->Size > sizeof(pPlain->Data)) ) {
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



/* 拒绝 COMPRESS 后继续按明文完成 LOGOUT，验证切换只发生在 tagged OK 后。 */
static int32 testImapCompressRejectServer(ptr pData)
{
	testimapcompressserver* pServer = (testimapcompressserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testImapCompressRawSend(
		pStream,
		"* PREAUTH imap-compress.test ready\r\n",
		sizeof("* PREAUTH imap-compress.test ready\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressRawReceive(
		pStream,
		"A00000001 CAPABILITY\r\n",
		sizeof("A00000001 CAPABILITY\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressRawSend(
		pStream,
		"* CAPABILITY IMAP4rev1 COMPRESS=DEFLATE\r\n"
		"A00000001 OK capability complete\r\n",
		sizeof(
			"* CAPABILITY IMAP4rev1 COMPRESS=DEFLATE\r\n"
			"A00000001 OK capability complete\r\n"
		) - 1u,
		pServer->Deadline
	) && testImapCompressRawReceive(
		pStream,
		"A00000002 COMPRESS DEFLATE\r\n",
		sizeof("A00000002 COMPRESS DEFLATE\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressRawSend(
		pStream,
		"A00000002 NO compression unavailable\r\n",
		sizeof("A00000002 NO compression unavailable\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressRawReceive(
		pStream,
		"A00000003 LOGOUT\r\n",
		sizeof("A00000003 LOGOUT\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressRawSend(
		pStream,
		"* BYE signing off\r\nA00000003 OK logout complete\r\n",
		sizeof(
			"* BYE signing off\r\n"
			"A00000003 OK logout complete\r\n"
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



/* 运行明文协商、同块切换和双向压缩命令的完整服务端转录。 */
static int32 testImapCompressServer(ptr pData)
{
	testimapcompressserver* pServer = (testimapcompressserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	xdeflateconfig DeflateConfig;
	xinflateconfig InflateConfig;
	testimapcompressbuffer Encoded = { { 0 }, 0 };
	testimapcompressbuffer Plain = { { 0 }, 0 };
	unsigned char sSwitch[1200];
	xdeflate* pDeflate;
	xinflate* pInflate;
	size_t iTagged;
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	xrtDeflateConfigInit(&DeflateConfig);
	DeflateConfig.Format = XDEFLATE_RAW;
	xrtInflateConfigInit(&InflateConfig);
	InflateConfig.Format = XINFLATE_RAW;
	pDeflate = xrtDeflateCreate(&DeflateConfig);
	pInflate = xrtInflateCreate(&InflateConfig);
	if ( (pDeflate == NULL) || (pInflate == NULL) ) {
		xrtDeflateDestroy(pDeflate);
		xrtInflateDestroy(pInflate);
		xrtNetStreamDestroy(pStream);
		return 2;
	}
	bSuccess = testImapCompressRawSend(
		pStream,
		"* PREAUTH imap-compress.test ready\r\n",
		sizeof("* PREAUTH imap-compress.test ready\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressRawReceive(
		pStream,
		"A00000001 CAPABILITY\r\n",
		sizeof("A00000001 CAPABILITY\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressRawSend(
		pStream,
		"* CAPABILITY IMAP4rev1 COMPRESS=DEFLATE\r\n"
		"A00000001 OK capability complete\r\n",
		sizeof(
			"* CAPABILITY IMAP4rev1 COMPRESS=DEFLATE\r\n"
			"A00000001 OK capability complete\r\n"
		) - 1u,
		pServer->Deadline
	) && testImapCompressRawReceive(
		pStream,
		"A00000002 COMPRESS DEFLATE\r\n",
		sizeof("A00000002 COMPRESS DEFLATE\r\n") - 1u,
		pServer->Deadline
	) && xrtDeflateWrite(
		pDeflate,
		(xbytesview) {
			(cbytes)"* 1 EXISTS\r\n",
			sizeof("* 1 EXISTS\r\n") - 1u
		},
		XDEFLATE_FLUSH_SYNC,
		testImapCompressBufferWrite,
		&Encoded
	);
	iTagged = sizeof("A00000002 OK compression active\r\n") - 1u;
	if ( bSuccess && ((iTagged + Encoded.Size) <= sizeof(sSwitch)) ) {
		memcpy(sSwitch, "A00000002 OK compression active\r\n", iTagged);
		memcpy(sSwitch + iTagged, Encoded.Data, Encoded.Size);
		bSuccess = testImapCompressRawSend(
			pStream,
			sSwitch,
			iTagged + Encoded.Size,
			pServer->Deadline
		);
	} else {
		bSuccess = false;
	}
	bSuccess = bSuccess && testImapCompressReceive(
		pInflate,
		&Plain,
		pStream,
		"A00000003 NOOP\r\n",
		sizeof("A00000003 NOOP\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressSend(
		pDeflate,
		pStream,
		"A00000003 OK noop complete\r\n",
		sizeof("A00000003 OK noop complete\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressReceive(
		pInflate,
		&Plain,
		pStream,
		"A00000004 LOGOUT\r\n",
		sizeof("A00000004 LOGOUT\r\n") - 1u,
		pServer->Deadline
	) && testImapCompressSend(
		pDeflate,
		pStream,
		"* BYE signing off\r\nA00000004 OK logout complete\r\n",
		sizeof(
			"* BYE signing off\r\n"
			"A00000004 OK logout complete\r\n"
		) - 1u,
		pServer->Deadline
	) && xrtNetStreamClose(pStream) && xrtNetStreamWait(
		pStream,
		XNET_STREAM_WAIT_CLOSE,
		pServer->Deadline,
		NULL
	);
	pServer->Success = bSuccess;
	xrtDeflateDestroy(pDeflate);
	xrtInflateDestroy(pInflate);
	xrtNetStreamDestroy(pStream);
	return bSuccess ? 0 : 3;
}



/* 验证配置边界、压缩切换原子性、预读数据和后续命令状态。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	testimapcompressserver Server;
	ximapclientconfig ClientConfig;
	ximapcompressconfig CompressConfig;
	ximapcompressconfig InvalidConfig;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	ximapclient* pClient;
	xthread* pThread;
	xdeadline Deadline;
	ximapevent Event;

	xrtImapCompressConfigInit(&CompressConfig);
	testRequire(xrtImapCompressConfigValid(&CompressConfig) &&
		(CompressConfig.Level == XDEFLATE_LEVEL_DEFAULT) &&
		(CompressConfig.Strategy == XDEFLATE_STRATEGY_DEFAULT) &&
		(CompressConfig.WindowBits == XDEFLATE_WINDOW_MAX),
		"IMAP COMPRESS default config mismatch");
	InvalidConfig = CompressConfig;
	InvalidConfig.WindowBits = XDEFLATE_WINDOW_MAX + 1u;
	xrtClearError();
	testRequire(!xrtImapCompressConfigValid(&InvalidConfig) &&
		(xrtErrorCode(xrtGetError()) == XDEFLATE_ERROR_CONFIG),
		"IMAP COMPRESS accepted an invalid window");
	xrtClearError();

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP COMPRESS engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP COMPRESS loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapCompressAddress
	), "IMAP COMPRESS listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapCompressResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"IMAP COMPRESS resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testImapCompressRejectServer, &Server, 0);
	testRequire(pThread != NULL,
		"IMAP COMPRESS reject server thread creation failed");
	xrtImapClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "imap-compress.test";
	ClientConfig.Net.Port = TestImapCompressAddress.Port;
	pClient = xrtImapClientOpen(&ClientConfig, Deadline, NULL);
	testRequire((pClient != NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
		"IMAP COMPRESS reject client open failed");
	xrtClearError();
	testRequire(!xrtImapClientCompress(
		pClient,
		&CompressConfig,
		Deadline,
		NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		!xrtImapClientCompressed(pClient) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
		"rejected IMAP COMPRESS changed the transport state");
	xrtClearError();
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL),
		"IMAP COMPRESS reject-session logout failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP COMPRESS reject server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP COMPRESS reject transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtImapClientDestroy(pClient);

	Server.Success = false;
	pThread = xrtThreadCreate(testImapCompressServer, &Server, 0);
	testRequire(pThread != NULL,
		"IMAP COMPRESS server thread creation failed");
	xrtImapClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "imap-compress.test";
	ClientConfig.Net.Port = TestImapCompressAddress.Port;
	pClient = xrtImapClientOpen(&ClientConfig, Deadline, NULL);
	testRequire((pClient != NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED) &&
		((xrtImapClientCapabilities(pClient) &
		 XIMAP_CAP_COMPRESS_DEFLATE) != 0) &&
		!xrtImapClientCompressed(pClient),
		"IMAP COMPRESS client open failed");
	testRequire(xrtImapClientCompress(
		pClient,
		&CompressConfig,
		Deadline,
		NULL
	) && xrtImapClientCompressed(pClient),
		"IMAP COMPRESS negotiation failed");

	xrtClearError();
	testRequire(!xrtImapClientCompress(
		pClient,
		&CompressConfig,
		Deadline,
		NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"IMAP COMPRESS allowed a second negotiation");
	xrtClearError();
	testRequire(xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("NOOP"),
		XRT_STR_LITERAL(""),
		Deadline,
		NULL
	), "compressed IMAP NOOP send failed");
	testRequire((xrtImapClientNext(
		pClient,
		&Event,
		Deadline,
		NULL
	) == XMAIL_NEXT_ITEM) &&
		(Event.Response.Kind == XIMAP_RESPONSE_UNTAGGED) &&
		testMailViewEqual(
			Event.Response.Text,
			XRT_STR_LITERAL("1 EXISTS")
		), "prefetched compressed IMAP response mismatch");
	testRequire(xrtImapClientNext(
		pClient,
		&Event,
		Deadline,
		NULL
	) == XMAIL_NEXT_END, "compressed IMAP NOOP completion failed");
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_CLOSED),
		"compressed IMAP LOGOUT failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP COMPRESS server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP COMPRESS transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtImapClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"IMAP COMPRESS listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP COMPRESS resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP COMPRESS engine destroy failed");
	return 0;
}
