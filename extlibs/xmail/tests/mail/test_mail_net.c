#include "../../src/internal/xrt_mail_net.h"
#include "../test.h"



static xnetaddr TestMailNetAddress;
static xatomic32 TestMailNetResolveCalls;

typedef struct testmailnetlookup {
	xatomic32 Entered;
	xatomic32 Release;
	xatomic32 Exited;
	xatomic32 Returned;
	xcancel* Cancel;
} testmailnetlookup;

static testmailnetlookup TestMailNetLookup;




typedef struct testmailnetpark {
	xatomic32 Entered;
	xatomic32 Release;
	xatomic32 Exited;
} testmailnetpark;




/* 暂停客户端 Worker，使已接受的写入保持占用发送预算。 */
static void testMailNetParkWorker(xnetworker* pWorker, ptr pData)
{
	testmailnetpark* pPark = (testmailnetpark*)pData;

	(void)pWorker;
	xrtAtomic32Store(&pPark->Entered, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pPark->Release, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(&pPark->Exited, 1, XMEMORY_RELEASE);
}




static void testMailNetWaitFlag(const xatomic32* pFlag, cstr sMessage)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(3000000));

	while ( xrtAtomic32Load(pFlag, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}

/* 取消后拨号对象仍由 Worker 异步释放，须有界等待退休。 */
static void testMailNetDestroyEngine(xnetengine* pEngine)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(3000000));

	for ( ;; ) {
		xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);

		testRequire(Result != XNET_RETIRE_ERROR,
			"mail network engine retirement failed");
		if ( Result == XNET_RETIRE_READY ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(Deadline),
			"mail network dial resources did not retire");
		xrtThreadYield();
	}
}




static int32 testMailNetCancelAfterDelay(ptr pData)
{
	xrtSleep(20);
	return xrtCancelRequest((xcancel*)pData) ? 0 : 1;
}




static int32 testMailNetReleaseAfterDelay(ptr pData)
{
	testmailnetpark* pPark = (testmailnetpark*)pData;

	xrtSleep(20);
	xrtAtomic32Store(&pPark->Release, 1, XMEMORY_RELEASE);
	return 0;
}

static int32 testMailNetReleaseLookupAfterReturn(ptr pData)
{
	testmailnetlookup* pLookup = (testmailnetlookup*)pData;
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(3000000));

	while ( xrtAtomic32Load(&pLookup->Entered, XMEMORY_ACQUIRE) == 0 &&
		!xrtDeadlineExpired(Deadline) ) {
		xrtThreadYield();
	}
	if ( pLookup->Cancel != NULL &&
		xrtAtomic32Load(&pLookup->Entered, XMEMORY_ACQUIRE) != 0 ) {
		(void)xrtCancelRequest(pLookup->Cancel);
	}
	while ( xrtAtomic32Load(&pLookup->Returned, XMEMORY_ACQUIRE) == 0 &&
		!xrtDeadlineExpired(Deadline) ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(&pLookup->Release, 1, XMEMORY_RELEASE);
	return xrtAtomic32Load(&pLookup->Returned, XMEMORY_ACQUIRE) != 0 ? 0 : 1;
}

static void testMailNetLookupInit(xcancel* pCancel)
{
	xrtAtomic32Init(&TestMailNetLookup.Entered, 0);
	xrtAtomic32Init(&TestMailNetLookup.Release, 0);
	xrtAtomic32Init(&TestMailNetLookup.Exited, 0);
	xrtAtomic32Init(&TestMailNetLookup.Returned, 0);
	TestMailNetLookup.Cancel = pCancel;
}




static void testMailNetExpectBytes(
	xnetstream* pServer,
	cstr sExpected,
	size_t iExpected
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(3000000));
	size_t iOffset = 0;

	while ( iOffset < iExpected ) {
		xnetbytes* pReceived = xrtNetStreamRecv(
			pServer, iExpected - iOffset, Deadline, NULL
		);
		xbytesview Received;

		testRequire(pReceived != NULL, "mail backpressure receive failed");
		Received = xrtNetBytesView(pReceived);
		testRequire((Received.Size != 0) &&
			(Received.Size <= iExpected - iOffset) &&
			(memcmp(Received.Data, sExpected + iOffset, Received.Size) == 0),
			"mail backpressure received unexpected bytes");
		iOffset += Received.Size;
		xrtNetBytesDestroy(pReceived);
	}
}



/* 把测试主机映射到本地监听地址。 */
static xnetaddrlist* testMailNetResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "mail.test") != 0 &&
		strcmp(sHost, "stall.test") != 0 ) {
		return NULL;
	}
	(void)xrtAtomic32FetchAdd(&TestMailNetResolveCalls, 1, XMEMORY_RELEASE);
	if ( strcmp(sHost, "stall.test") == 0 ) {
		xrtAtomic32Store(&TestMailNetLookup.Entered, 1, XMEMORY_RELEASE);
		while ( xrtAtomic32Load(&TestMailNetLookup.Release,
			XMEMORY_ACQUIRE) == 0 ) {
			xrtThreadYield();
		}
		xrtAtomic32Store(&TestMailNetLookup.Exited, 1, XMEMORY_RELEASE);
	}
	Address = TestMailNetAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 等待 Stream 进入最终关闭状态。 */
static bool testMailNetClosed(xnetstream* pStream)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(3000000));

	while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 验证动态多行缓存、完整发送和外部 Engine/Resolver 所有权。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xmailnetconfig Config;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetstream* pServer;
	xnetbytes* pCommand;
	xbytesview Command;
	__xmailtransport Transport;
	xstrview Line;
	xdeadline Deadline;
	xcancel* pCancel;
	xthread* pCanceller;
	testmailnetpark Park;
	bool bClosed;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"mail network engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "mail network loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestMailNetAddress
	), "mail network listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	xrtAtomic32Init(&TestMailNetResolveCalls, 0);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testMailNetResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "mail network resolver creation failed");

	xrtMailNetConfigInit(&Config);
	Config.Engine = pEngine;
	Config.Resolver = pResolver;
	Config.Host = "mail.test";
	Config.Port = TestMailNetAddress.Port;
	testRequire(xrtMailNetConfigValid(&Config),
		"mail network plain config validation failed");
	/* 预取消或已到期的连接不得触发 DNS/拨号副作用。 */
	pCancel = xrtCancelCreate();
	testRequire((pCancel != NULL) && xrtCancelRequest(pCancel),
		"mail network cancel fixture failed");
	xrtClearError();
	testRequire(!__xrtMailTransportOpen(
		&Transport, &Config, xrtDeadlineAfter(UINT64_C(3000000)), pCancel
	) && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_CANCELLED &&
		Transport.Tcp == NULL &&
		xrtAtomic32Load(&TestMailNetResolveCalls, XMEMORY_ACQUIRE) == 0,
		"mail network pre-cancelled dial reached resolver");
	__xrtMailTransportDestroy(&Transport);
	xrtCancelDestroy(pCancel);
	xrtClearError();
	testRequire(!__xrtMailTransportOpen(
		&Transport, &Config, xrtDeadlineAfter(0), NULL
	) && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_TIMEOUT &&
		Transport.Tcp == NULL &&
		xrtAtomic32Load(&TestMailNetResolveCalls, XMEMORY_ACQUIRE) == 0,
		"mail network expired dial reached resolver");
	__xrtMailTransportDestroy(&Transport);

	/* DNS 查询已在运行时，取消必须先于查询释放返回，且不得迟到拨号。 */
	Config.Host = "stall.test";
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "mail stalled lookup cancel creation failed");
	testMailNetLookupInit(pCancel);
	pCanceller = xrtThreadCreate(testMailNetReleaseLookupAfterReturn,
		&TestMailNetLookup, 0);
	testRequire(pCanceller != NULL, "mail stalled lookup helper creation failed");
	{
		xdeadline Quick = xrtDeadlineAfter(UINT64_C(500000));
		xrtClearError();
		testRequire(!__xrtMailTransportOpen(
			&Transport, &Config, xrtDeadlineAfter(UINT64_C(3000000)), pCancel
		) && xrtGetError() != NULL &&
			xrtErrorKind(xrtGetError()) == XERR_CANCELLED &&
			!xrtDeadlineExpired(Quick) && Transport.Tcp == NULL,
			"mail in-flight lookup cancellation did not return promptly");
	}
	xrtAtomic32Store(&TestMailNetLookup.Returned, 1, XMEMORY_RELEASE);
	testRequire(xrtThreadWaitFor(pCanceller, UINT64_C(3000000)) ==
		XWAIT_OK && xrtThreadExitCode(pCanceller) == 0,
		"mail stalled lookup cancel helper did not finish");
	xrtThreadDestroy(pCanceller);
	testMailNetWaitFlag(&TestMailNetLookup.Exited,
		"mail cancelled lookup did not release");
	__xrtMailTransportDestroy(&Transport);
	xrtCancelDestroy(pCancel);
	pServer = xrtNetListenerAcceptWait(
		pListener, xrtDeadlineAfter(UINT64_C(200000)), NULL);
	testRequire(pServer == NULL,
		"mail cancelled lookup caused a late connection");
	xrtClearError();

	/* 同一窗口内到期也必须返回超时，不等待解析器解除阻塞。 */
	testMailNetLookupInit(NULL);
	pCanceller = xrtThreadCreate(testMailNetReleaseLookupAfterReturn,
		&TestMailNetLookup, 0);
	testRequire(pCanceller != NULL, "mail stalled timeout helper creation failed");
	{
		xdeadline Quick = xrtDeadlineAfter(UINT64_C(700000));
		xrtClearError();
		testRequire(!__xrtMailTransportOpen(
			&Transport, &Config, xrtDeadlineAfter(UINT64_C(200000)), NULL
		) && xrtGetError() != NULL &&
			xrtErrorKind(xrtGetError()) == XERR_TIMEOUT &&
			!xrtDeadlineExpired(Quick) && Transport.Tcp == NULL,
			"mail in-flight lookup timeout did not return promptly");
	}
	xrtAtomic32Store(&TestMailNetLookup.Returned, 1, XMEMORY_RELEASE);
	testRequire(xrtThreadWaitFor(pCanceller, UINT64_C(3000000)) ==
		XWAIT_OK && xrtThreadExitCode(pCanceller) == 0,
		"mail stalled timeout helper did not finish");
	xrtThreadDestroy(pCanceller);
	testMailNetWaitFlag(&TestMailNetLookup.Exited,
		"mail timed-out lookup did not release");
	__xrtMailTransportDestroy(&Transport);
	pServer = xrtNetListenerAcceptWait(
		pListener, xrtDeadlineAfter(UINT64_C(200000)), NULL);
	testRequire(pServer == NULL,
		"mail timed-out lookup caused a late connection");
	xrtClearError();
	Config.Host = "mail.test";

	Deadline = xrtDeadlineAfter(UINT64_C(3000000));
	testRequire(__xrtMailTransportOpen(
		&Transport,
		&Config,
		Deadline,
		NULL
	), "mail network plain transport open failed");
	pServer = xrtNetListenerAcceptWait(pListener, Deadline, NULL);
	testRequire(pServer != NULL, "mail network server accept failed");

	testRequire(xrtNetStreamSend(
		pServer,
		"220 mail.test ready\r\n250 queued\r\n",
		33u
	) == XNET_RESULT_OK, "mail network server response send failed");
	testRequire(__xrtMailTransportLine(
		&Transport,
		&Line,
		Deadline,
		NULL
	) && testMailViewEqual(Line, XRT_STR_LITERAL("220 mail.test ready")),
		"mail network first buffered line mismatch");
	testRequire(__xrtMailTransportLine(
		&Transport,
		&Line,
		Deadline,
		NULL
	) && testMailViewEqual(Line, XRT_STR_LITERAL("250 queued")),
		"mail network second buffered line mismatch");
	testRequire(__xrtMailTransportSend(
		&Transport,
		"EHLO client.test\r\n",
		18u,
		Deadline,
		NULL
	), "mail network client command send failed");
	pCommand = xrtNetStreamRecv(pServer, 0, Deadline, NULL);
	testRequire(pCommand != NULL, "mail network server command receive failed");
	Command = xrtNetBytesView(pCommand);
	testRequire((Command.Size == 18u) &&
		(memcmp(Command.Data, "EHLO client.test\r\n", 18u) == 0),
		"mail network command bytes mismatch");
	xrtNetBytesDestroy(pCommand);

	testRequire(xrtNetStreamClose(pServer),
		"mail network server close request failed");
	bClosed = __xrtMailTransportClose(&Transport, Deadline);
	testRequire(bClosed && testMailNetClosed(pServer),
		"mail network graceful close failed");
	__xrtMailTransportDestroy(&Transport);
	xrtNetStreamDestroy(pServer);

	/* 对端在 CRLF 前关闭，不能把未完成的回复当作完整线路。 */
	Deadline = xrtDeadlineAfter(UINT64_C(3000000));
	testRequire(__xrtMailTransportOpen(
		&Transport, &Config, Deadline, NULL
	), "mail network truncated-line transport open failed");
	pServer = xrtNetListenerAcceptWait(pListener, Deadline, NULL);
	testRequire(pServer != NULL,
		"mail network truncated-line server accept failed");
	testRequire(xrtNetStreamSend(pServer, "220 partial", 11u) ==
		XNET_RESULT_OK && xrtNetStreamClose(pServer),
		"mail network truncated-line fixture send failed");
	xrtClearError();
	testRequire(!__xrtMailTransportLine(
		&Transport, &Line, Deadline, NULL
	) && xrtErrorKind(xrtGetError()) == XERR_CLOSED,
		"mail network accepted a truncated line after close");
	(void)__xrtMailTransportAbort(&Transport);
	__xrtMailTransportDestroy(&Transport);
	testRequire(testMailNetClosed(pServer),
		"mail network truncated-line server did not close");
	xrtNetStreamDestroy(pServer);

	/* Worker 停顿时精确占满写预算，令下一个分片必经 AGAIN。 */
	Config.Dial.Stream.WriteHighWater = 4;
	Config.Dial.Stream.WriteLowWater = 0;
	Config.Dial.Stream.WriteLimit = 4;
	Config.WriteChunk = 4;
	Deadline = xrtDeadlineAfter(UINT64_C(3000000));
	testRequire(__xrtMailTransportOpen(&Transport, &Config, Deadline, NULL),
		"mail backpressure transport open failed");
	pServer = xrtNetListenerAcceptWait(pListener, Deadline, NULL);
	testRequire(pServer != NULL, "mail backpressure server accept failed");
	xrtAtomic32Init(&Park.Entered, 0);
	xrtAtomic32Init(&Park.Release, 0);
	xrtAtomic32Init(&Park.Exited, 0);
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(Transport.Tcp)),
		testMailNetParkWorker,
		&Park
	), "mail backpressure worker park failed");
	testMailNetWaitFlag(&Park.Entered,
		"mail backpressure worker did not park");
	testRequire(xrtNetStreamSend(Transport.Tcp, "AAAA", 4) ==
		XNET_RESULT_OK && xrtNetStreamPending(Transport.Tcp) == 4,
		"mail backpressure first send did not fill queue");
	xrtClearError();
	testRequire(!__xrtMailTransportRawSend(
		&Transport, "B", 1, xrtDeadlineAfter(UINT64_C(20000)), NULL
	) && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_TIMEOUT &&
		xrtNetStreamPending(Transport.Tcp) == 4,
		"mail backpressure wait ignored deadline or queued extra bytes");
	xrtAtomic32Store(&Park.Release, 1, XMEMORY_RELEASE);
	testMailNetWaitFlag(&Park.Exited,
		"mail backpressure worker did not resume");
	testMailNetExpectBytes(pServer, "AAAA", 4);

	/* 重新占满预算，再从另一线程取消正在等待写就绪的发送。 */
	xrtAtomic32Store(&Park.Entered, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Park.Release, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Park.Exited, 0, XMEMORY_RELEASE);
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(Transport.Tcp)),
		testMailNetParkWorker,
		&Park
	), "mail backpressure second worker park failed");
	testMailNetWaitFlag(&Park.Entered,
		"mail backpressure second worker did not park");
	testRequire(xrtNetStreamSend(Transport.Tcp, "CCCC", 4) ==
		XNET_RESULT_OK && xrtNetStreamPending(Transport.Tcp) == 4,
		"mail backpressure second send did not fill queue");
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL,
		"mail backpressure cancel creation failed");
	pCanceller = xrtThreadCreate(testMailNetCancelAfterDelay, pCancel, 0);
	testRequire(pCanceller != NULL,
		"mail backpressure canceller creation failed");
	xrtClearError();
	testRequire(!__xrtMailTransportRawSend(
		&Transport, "D", 1, Deadline, pCancel
	) && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_CANCELLED &&
		xrtNetStreamPending(Transport.Tcp) == 4,
		"mail backpressure wait ignored cancellation or queued extra bytes");
	testRequire(xrtThreadWaitFor(pCanceller, UINT64_C(3000000)) ==
		XWAIT_OK && xrtThreadExitCode(pCanceller) == 0,
		"mail backpressure canceller did not finish");
	xrtThreadDestroy(pCanceller);
	xrtCancelDestroy(pCancel);
	xrtAtomic32Store(&Park.Release, 1, XMEMORY_RELEASE);
	testMailNetWaitFlag(&Park.Exited,
		"mail backpressure second worker did not resume");
	testMailNetExpectBytes(pServer, "CCCC", 4);
	testRequire(xrtNetStreamPending(Transport.Tcp) == 0,
		"mail backpressure queue did not drain");
	xrtClearError();
	testRequire(xrtNetStreamRecv(
		pServer, 1, xrtDeadlineAfter(UINT64_C(20000)), NULL
	) == NULL && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_TIMEOUT,
		"mail backpressure sent bytes after timeout or cancellation");

	/* 背压解除时，发送应在写就绪唤醒后仅重试一次目标字节。 */
	xrtAtomic32Store(&Park.Entered, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Park.Release, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Park.Exited, 0, XMEMORY_RELEASE);
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(Transport.Tcp)),
		testMailNetParkWorker,
		&Park
	), "mail backpressure third worker park failed");
	testMailNetWaitFlag(&Park.Entered,
		"mail backpressure third worker did not park");
	testRequire(xrtNetStreamSend(Transport.Tcp, "EEEE", 4) ==
		XNET_RESULT_OK && xrtNetStreamPending(Transport.Tcp) == 4,
		"mail backpressure third send did not fill queue");
	pCanceller = xrtThreadCreate(testMailNetReleaseAfterDelay, &Park, 0);
	testRequire(pCanceller != NULL,
		"mail backpressure worker release thread creation failed");
	xrtClearError();
	testRequire(__xrtMailTransportRawSend(
		&Transport, "F", 1, xrtDeadlineAfter(UINT64_C(3000000)), NULL
	), "mail backpressure did not recover after write became ready");
	testRequire(xrtThreadWaitFor(pCanceller, UINT64_C(3000000)) ==
		XWAIT_OK && xrtThreadExitCode(pCanceller) == 0,
		"mail backpressure worker release thread did not finish");
	xrtThreadDestroy(pCanceller);
	testMailNetWaitFlag(&Park.Exited,
		"mail backpressure third worker did not resume");
	testMailNetExpectBytes(pServer, "EEEEF", 5);
	(void)__xrtMailTransportAbort(&Transport);
	__xrtMailTransportDestroy(&Transport);
	testRequire(testMailNetClosed(pServer),
		"mail backpressure server did not close");
	xrtNetStreamDestroy(pServer);

	testRequire(xrtNetListenerClose(pListener),
		"mail network listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"mail network resolver destroy failed");
	testMailNetDestroyEngine(pEngine);
	return 0;
}
