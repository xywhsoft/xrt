#include <xrt/detail/wait.h>
#include "../../src/internal/xrt_mail_net.h"
#include "../../../../tests/fixtures/tls_server.h"



static xnetaddr TestMailNetTlsAddress;
static bool TestMailNetTlsRejectCalled;
static xatomic32 TestMailNetTlsResolveCalls;

typedef struct testmailnettlslookup {
	xatomic32 Entered;
	xatomic32 Release;
	xatomic32 Exited;
	xatomic32 Returned;
	xcancel* Cancel;
} testmailnettlslookup;

static testmailnettlslookup TestMailNetTlsLookup;




typedef struct testmailnettlspark {
	xatomic32 Entered;
	xatomic32 Release;
	xatomic32 Exited;
} testmailnettlspark;




static void testMailNetTlsParkWorker(xnetworker* pWorker, ptr pData)
{
	testmailnettlspark* pPark = (testmailnettlspark*)pData;

	(void)pWorker;
	xrtAtomic32Store(&pPark->Entered, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pPark->Release, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(&pPark->Exited, 1, XMEMORY_RELEASE);
}




static void testMailNetTlsWaitFlag(const xatomic32* pFlag, cstr sMessage)
{
	double Deadline = __xrtWaitAfter(INT64_C(3000));

	while ( xrtAtomic32Load(pFlag, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!__xrtWaitExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}

/* 拨号取消是异步清理；资源退休须等待 Worker 完成回调。 */
static void testMailNetTlsDestroyEngine(xnetengine* pEngine)
{
	double Deadline = __xrtWaitAfter(INT64_C(3000));

	for ( ;; ) {
		xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);

		testRequire(Result != XNET_RETIRE_ERROR,
			"mail TLS runtime engine retirement failed");
		if ( Result == XNET_RETIRE_READY ) {
			return;
		}
		testRequire(!__xrtWaitExpired(Deadline),
			"mail TLS runtime dial resources did not retire");
		xrtThreadYield();
	}
}




static int32 testMailNetTlsCancelAfterDelay(ptr pData)
{
	xrtSleep(20);
	return xrtCancelRequest((xcancel*)pData) ? 0 : 1;
}

static void testMailNetTlsLookupInit(xcancel* pCancel)
{
	xrtAtomic32Init(&TestMailNetTlsLookup.Entered, 0);
	xrtAtomic32Init(&TestMailNetTlsLookup.Release, 0);
	xrtAtomic32Init(&TestMailNetTlsLookup.Exited, 0);
	xrtAtomic32Init(&TestMailNetTlsLookup.Returned, 0);
	TestMailNetTlsLookup.Cancel = pCancel;
}

static int32 testMailNetTlsReleaseLookupAfterReturn(ptr pData)
{
	testmailnettlslookup* pLookup = (testmailnettlslookup*)pData;
	double Deadline = __xrtWaitAfter(INT64_C(3000));

	while ( xrtAtomic32Load(&pLookup->Entered, XMEMORY_ACQUIRE) == 0 &&
		!__xrtWaitExpired(Deadline) ) {
		xrtThreadYield();
	}
	if ( pLookup->Cancel != NULL &&
		xrtAtomic32Load(&pLookup->Entered, XMEMORY_ACQUIRE) != 0 ) {
		(void)xrtCancelRequest(pLookup->Cancel);
	}
	while ( xrtAtomic32Load(&pLookup->Returned, XMEMORY_ACQUIRE) == 0 &&
		!__xrtWaitExpired(Deadline) ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(&pLookup->Release, 1, XMEMORY_RELEASE);
	return xrtAtomic32Load(&pLookup->Returned, XMEMORY_ACQUIRE) != 0 ? 0 : 1;
}


static xtlsverifydecision testMailNetTlsReject(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pPeer;
	(void)pContext;
	TestMailNetTlsRejectCalled = true;
	return XTLS_VERIFY_REJECT;
}



/* 把测试域名解析到本地 TLS Listener。 */
static xnetaddrlist* testMailNetTlsResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "mail.test") != 0 &&
		strcmp(sHost, "stall.mail.test") != 0 ) {
		return NULL;
	}
	(void)xrtAtomic32FetchAdd(&TestMailNetTlsResolveCalls, 1,
		XMEMORY_RELEASE);
	if ( strcmp(sHost, "stall.mail.test") == 0 ) {
		xrtAtomic32Store(&TestMailNetTlsLookup.Entered, 1, XMEMORY_RELEASE);
		while ( xrtAtomic32Load(&TestMailNetTlsLookup.Release,
			XMEMORY_ACQUIRE) == 0 ) {
			xrtThreadYield();
		}
		xrtAtomic32Store(&TestMailNetTlsLookup.Exited, 1, XMEMORY_RELEASE);
	}
	Address = TestMailNetTlsAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 等待测试 Future 成功完成。 */
static bool testMailNetTlsFuture(
	xfuture* pFuture,
	double iDeadline
)
{
	return (pFuture != NULL) &&
		(__xrtFutureWaitUntil(pFuture, iDeadline) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 验证真实证书握手、加密双向收发和认证关闭。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xtlslistenerconfig ListenerConfig;
	xtlsverifierconfig VerifierConfig;
	xmailnetconfig Config;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xtlsverifier* pRejectVerifier;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xtlslistener* pListener;
	xtlsstream* pServer;
	__xmailtransport Transport;
	xfuture* pFuture;
	xnetbytes* pBytes;
	xbytesview Bytes;
	xstrview Line;
	double Deadline;
	xcancel* pCancel;
	xthread* pCanceller;
	testmailnettlspark Park;
	xnetstream* pTcp;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"mail TLS runtime fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"mail TLS runtime verifier creation failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"mail TLS runtime engine start failed");
	xrtTlsListenerConfigInit(&ListenerConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "mail TLS runtime loopback address failed");
	ListenerConfig.Tls.Context = pContext;
	ListenerConfig.Tls.Identity = pIdentity;
	pListener = xrtTlsListenerStart(
		pEngine,
		&ListenerConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire((pListener != NULL) && xrtTlsListenerLocal(
		pListener,
		&TestMailNetTlsAddress
	), "mail TLS runtime listener start failed");

	xrtNetResolverConfigInit(&ResolverConfig);
	xrtAtomic32Init(&TestMailNetTlsResolveCalls, 0);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testMailNetTlsResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"mail TLS runtime resolver creation failed");
	xrtMailNetConfigInit(&Config);
	Config.Engine = pEngine;
	Config.Resolver = pResolver;
	Config.Host = "mail.test";
	Config.Port = TestMailNetTlsAddress.Port;
	Config.Security = XMAIL_SECURITY_TLS;
	Config.Tls.Context = pContext;
	Config.Tls.Verifier = pVerifier;
	/* 隐式 TLS 同样必须在提交异步拨号前处理预取消与过期。 */
	pCancel = xrtCancelCreate();
	testRequire((pCancel != NULL) && xrtCancelRequest(pCancel),
		"mail TLS runtime cancel fixture failed");
	xrtClearError();
	testRequire(!__xrtMailTransportOpen(
		&Transport, &Config, __xrtWaitAfter(INT64_C(3000)), pCancel
	) && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_CANCELLED &&
		Transport.Tls == NULL && Transport.Tcp == NULL &&
		xrtAtomic32Load(&TestMailNetTlsResolveCalls, XMEMORY_ACQUIRE) == 0,
		"mail TLS runtime pre-cancelled dial reached resolver");
	__xrtMailTransportDestroy(&Transport);
	xrtCancelDestroy(pCancel);
	xrtClearError();
	testRequire(!__xrtMailTransportOpen(
		&Transport, &Config, __xrtWaitAfter(0), NULL
	) && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_TIMEOUT &&
		Transport.Tls == NULL && Transport.Tcp == NULL &&
		xrtAtomic32Load(&TestMailNetTlsResolveCalls, XMEMORY_ACQUIRE) == 0,
		"mail TLS runtime expired dial reached resolver");
	__xrtMailTransportDestroy(&Transport);

	/* 隐式 TLS 的 DNS 查询进行中，取消须先于解析器释放而返回。 */
	Config.Host = "stall.mail.test";
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "mail TLS stalled cancel creation failed");
	testMailNetTlsLookupInit(pCancel);
	pCanceller = xrtThreadCreate(testMailNetTlsReleaseLookupAfterReturn,
		&TestMailNetTlsLookup, 0);
	testRequire(pCanceller != NULL,
		"mail TLS stalled cancel helper creation failed");
	{
		double Quick = __xrtWaitAfter(INT64_C(500));
		xrtClearError();
		testRequire(!__xrtMailTransportOpen(
			&Transport, &Config, __xrtWaitAfter(INT64_C(3000)), pCancel
		) && xrtGetError() != NULL &&
			xrtErrorKind(xrtGetError()) == XERR_CANCELLED &&
			!__xrtWaitExpired(Quick) &&
			Transport.Tls == NULL && Transport.Tcp == NULL,
			"mail TLS in-flight lookup cancellation was delayed");
	}
	xrtAtomic32Store(&TestMailNetTlsLookup.Returned, 1, XMEMORY_RELEASE);
	testRequire(xrtThreadWaitFor(pCanceller, INT64_C(3000)) ==
		XWAIT_OK && xrtThreadExitCode(pCanceller) == 0,
		"mail TLS stalled cancel helper did not finish");
	xrtThreadDestroy(pCanceller);
	testMailNetTlsWaitFlag(&TestMailNetTlsLookup.Exited,
		"mail TLS cancelled lookup did not release");
	__xrtMailTransportDestroy(&Transport);
	xrtCancelDestroy(pCancel);
	pServer = __xrtTlsListenerAcceptWait(
		pListener, __xrtWaitAfter(INT64_C(200)), NULL);
	testRequire(pServer == NULL,
		"mail TLS cancelled lookup caused a late session");
	xrtClearError();

	/* TLS 拨号的同一阶段到期时，也不能等待 DNS 回调完成。 */
	testMailNetTlsLookupInit(NULL);
	pCanceller = xrtThreadCreate(testMailNetTlsReleaseLookupAfterReturn,
		&TestMailNetTlsLookup, 0);
	testRequire(pCanceller != NULL,
		"mail TLS stalled timeout helper creation failed");
	{
		double Quick = __xrtWaitAfter(INT64_C(700));
		xrtClearError();
		testRequire(!__xrtMailTransportOpen(
			&Transport, &Config, __xrtWaitAfter(INT64_C(200)), NULL
		) && xrtGetError() != NULL &&
			xrtErrorKind(xrtGetError()) == XERR_TIMEOUT &&
			!__xrtWaitExpired(Quick) &&
			Transport.Tls == NULL && Transport.Tcp == NULL,
			"mail TLS in-flight lookup timeout was delayed");
	}
	xrtAtomic32Store(&TestMailNetTlsLookup.Returned, 1, XMEMORY_RELEASE);
	testRequire(xrtThreadWaitFor(pCanceller, INT64_C(3000)) ==
		XWAIT_OK && xrtThreadExitCode(pCanceller) == 0,
		"mail TLS stalled timeout helper did not finish");
	xrtThreadDestroy(pCanceller);
	testMailNetTlsWaitFlag(&TestMailNetTlsLookup.Exited,
		"mail TLS timed-out lookup did not release");
	__xrtMailTransportDestroy(&Transport);
	pServer = __xrtTlsListenerAcceptWait(
		pListener, __xrtWaitAfter(INT64_C(200)), NULL);
	testRequire(pServer == NULL,
		"mail TLS timed-out lookup caused a late session");
	xrtClearError();
	Config.Host = "mail.test";

	Deadline = __xrtWaitAfter(INT64_C(10000));
	testRequire(__xrtMailTransportOpen(
		&Transport,
		&Config,
		Deadline,
		NULL
	), "mail TLS runtime transport open failed");
	pServer = __xrtTlsListenerAcceptWait(pListener, Deadline, NULL);
	testRequire(pServer != NULL,
		"mail TLS runtime server accept failed");

	pFuture = xrtTlsStreamSendAsync(
		pServer,
		"220 mail.test ready\r\n",
		21u
	);
	testRequire(testMailNetTlsFuture(pFuture, Deadline),
		"mail TLS runtime server send failed");
	xrtFutureDestroy(pFuture);
	testRequire(__xrtMailTransportLine(
		&Transport,
		&Line,
		Deadline,
		NULL
	) && (Line.Size == 19u) &&
		(memcmp(Line.Data, "220 mail.test ready", 19u) == 0),
		"mail TLS runtime response mismatch");
	testRequire(__xrtMailTransportSend(
		&Transport,
		"EHLO client.test\r\n",
		18u,
		Deadline,
		NULL
	), "mail TLS runtime client send failed");
	pFuture = xrtTlsStreamRecvAsync(pServer, 18u);
	testRequire(testMailNetTlsFuture(pFuture, Deadline),
		"mail TLS runtime server receive failed");
	pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);
	testRequire(pBytes != NULL,
		"mail TLS runtime receive ownership failed");
	Bytes = xrtNetBytesView(pBytes);
	testRequire((Bytes.Size == 18u) &&
		(memcmp(Bytes.Data, "EHLO client.test\r\n", 18u) == 0),
		"mail TLS runtime command mismatch");
	xrtNetBytesDestroy(pBytes);

	/* 等待 TLS 明文时超时或取消，不得消耗随后到达的回复。 */
	xrtClearError();
	testRequire(!__xrtMailTransportLine(
		&Transport, &Line, __xrtWaitAfter(INT64_C(20)), NULL
	) && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_TIMEOUT &&
		xrtTlsStreamState(Transport.Tls) == XTLS_STREAM_OPEN,
		"mail TLS receive timeout poisoned the stream");
	pFuture = xrtTlsStreamSendAsync(
		pServer,
		"250 after timeout\r\n",
		sizeof("250 after timeout\r\n") - 1u
	);
	testRequire(testMailNetTlsFuture(pFuture, Deadline),
		"mail TLS timeout recovery server send failed");
	xrtFutureDestroy(pFuture);
	testRequire(__xrtMailTransportLine(
		&Transport, &Line, Deadline, NULL
	) && Line.Size == sizeof("250 after timeout") - 1u &&
		memcmp(Line.Data, "250 after timeout", Line.Size) == 0,
		"mail TLS receive timeout lost the next reply");
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "mail TLS receive cancel creation failed");
	pCanceller = xrtThreadCreate(testMailNetTlsCancelAfterDelay,
		pCancel, 0);
	testRequire(pCanceller != NULL,
		"mail TLS receive canceller creation failed");
	xrtClearError();
	testRequire(!__xrtMailTransportLine(
		&Transport, &Line, Deadline, pCancel
	) && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_CANCELLED &&
		xrtTlsStreamState(Transport.Tls) == XTLS_STREAM_OPEN,
		"mail TLS receive cancellation poisoned the stream");
	testRequire(xrtThreadWaitFor(pCanceller, INT64_C(3000)) ==
		XWAIT_OK && xrtThreadExitCode(pCanceller) == 0,
		"mail TLS receive canceller did not finish");
	xrtThreadDestroy(pCanceller);
	xrtCancelDestroy(pCancel);
	pFuture = xrtTlsStreamSendAsync(
		pServer,
		"250 after cancel\r\n",
		sizeof("250 after cancel\r\n") - 1u
	);
	testRequire(testMailNetTlsFuture(pFuture, Deadline),
		"mail TLS cancel recovery server send failed");
	xrtFutureDestroy(pFuture);
	testRequire(__xrtMailTransportLine(
		&Transport, &Line, Deadline, NULL
	) && Line.Size == sizeof("250 after cancel") - 1u &&
		memcmp(Line.Data, "250 after cancel", Line.Size) == 0,
		"mail TLS receive cancellation lost the next reply");

	/* 阻塞所属 Worker，使取消发生在 TLS 发送 Future 开始之前。 */
	pTcp = xrtTlsStreamTransport(Transport.Tls);
	testRequire(pTcp != NULL, "mail TLS transport was unavailable");
	xrtAtomic32Init(&Park.Entered, 0);
	xrtAtomic32Init(&Park.Release, 0);
	xrtAtomic32Init(&Park.Exited, 0);
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(pTcp)),
		testMailNetTlsParkWorker,
		&Park
	), "mail TLS worker park failed");
	testMailNetTlsWaitFlag(&Park.Entered,
		"mail TLS worker did not park");
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "mail TLS send cancel creation failed");
	pCanceller = xrtThreadCreate(testMailNetTlsCancelAfterDelay,
		pCancel, 0);
	testRequire(pCanceller != NULL,
		"mail TLS send canceller creation failed");
	xrtClearError();
	testRequire(!__xrtMailTransportSend(
		&Transport, "DROP\r\n", 6u, Deadline, pCancel
	) && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_CANCELLED,
		"mail TLS queued send ignored cancellation");
	testRequire(xrtThreadWaitFor(pCanceller, INT64_C(3000)) ==
		XWAIT_OK && xrtThreadExitCode(pCanceller) == 0,
		"mail TLS send canceller did not finish");
	xrtThreadDestroy(pCanceller);
	xrtCancelDestroy(pCancel);
	xrtAtomic32Store(&Park.Release, 1, XMEMORY_RELEASE);
	testMailNetTlsWaitFlag(&Park.Exited,
		"mail TLS worker did not resume");
	pFuture = xrtTlsStreamRecvAsync(pServer, 1u);
	testRequire(pFuture != NULL && __xrtFutureWaitUntil(
		pFuture, __xrtWaitAfter(INT64_C(20))
	) == XWAIT_TIMEOUT,
		"mail TLS cancelled send leaked bytes to the server");
	(void)xrtFutureCancel(pFuture);
	xrtFutureDestroy(pFuture);
	testRequire(__xrtMailTransportSend(
		&Transport, "NOOP\r\n", 6u, Deadline, NULL
	), "mail TLS send did not recover after cancellation");
	pFuture = xrtTlsStreamRecvAsync(pServer, 6u);
	testRequire(testMailNetTlsFuture(pFuture, Deadline),
		"mail TLS recovery receive failed");
	pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);
	testRequire(pBytes != NULL, "mail TLS recovery bytes were lost");
	Bytes = xrtNetBytesView(pBytes);
	testRequire(Bytes.Size == 6u &&
		memcmp(Bytes.Data, "NOOP\r\n", 6u) == 0,
		"mail TLS recovery received unexpected bytes");
	xrtNetBytesDestroy(pBytes);

	/* 协议已完成后仍可能有分片到达的压缩尾字节。关闭必须持续消费
	   TLS 明文，避免读取背压挡住后面的 close_notify；超过 ReadChunk
	   的尾部同时验证关闭路径按有界块循环消费。 */
	{
		unsigned char Tail[8193];

		memset(Tail, 0x5a, sizeof(Tail));
		Deadline = __xrtWaitAfter(INT64_C(3000));
		pFuture = xrtTlsStreamSendAsync(pServer, Tail, sizeof(Tail));
		testRequire(testMailNetTlsFuture(pFuture, Deadline),
			"mail TLS unread close tail send failed");
		xrtFutureDestroy(pFuture);
		while ( xrtTlsStreamAvailable(Transport.Tls) != sizeof(Tail) ) {
			testRequire(!__xrtWaitExpired(Deadline),
				"mail TLS unread close tail was not buffered");
			xrtThreadYield();
		}
	}
	pFuture = xrtTlsStreamWaitAsync(pServer, XTLS_STREAM_WAIT_CLOSE);
	testRequire((pFuture != NULL) && xrtTlsStreamClose(pServer),
		"mail TLS runtime server close request failed");
	testRequire(__xrtMailTransportClose(&Transport, Deadline),
		"mail TLS runtime client close failed");
	testRequire(testMailNetTlsFuture(pFuture, Deadline),
		"mail TLS runtime server close failed");
	xrtFutureDestroy(pFuture);
	testRequire(xrtTlsStreamState(Transport.Tls) == XTLS_STREAM_CLOSED &&
		xrtTlsStreamAvailable(Transport.Tls) == 0,
		"mail TLS close retained unread plaintext or lost authenticated shutdown");
	__xrtMailTransportDestroy(&Transport);
	xrtTlsStreamDestroy(pServer);

	/* 验证器拒绝时，隐式 TLS 拨号必须失败且不能留下活动传输。 */
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testMailNetTlsReject;
	pRejectVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pRejectVerifier != NULL,
		"mail TLS runtime reject verifier creation failed");
	Config.Tls.Verifier = pRejectVerifier;
	TestMailNetTlsRejectCalled = false;
	xrtClearError();
	Deadline = __xrtWaitAfter(INT64_C(10000));
	testRequire(!__xrtMailTransportOpen(
		&Transport, &Config, Deadline, NULL
	) && TestMailNetTlsRejectCalled &&
		xrtGetError() != NULL && Transport.Tls == NULL &&
		Transport.Tcp == NULL,
		"mail TLS verification rejection left an active transport");
	__xrtMailTransportDestroy(&Transport);
	xrtTlsVerifierRelease(pRejectVerifier);

	testRequire(xrtTlsListenerClose(pListener),
		"mail TLS runtime listener close request failed");
	while ( xrtTlsListenerState(pListener) != XTLS_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtTlsListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"mail TLS runtime resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	testMailNetTlsDestroyEngine(pEngine);
	return 0;
}
