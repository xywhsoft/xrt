#include "../test.h"



#if !defined(TEST_TCP_DIAL_SYNC_BACKEND)
	#define TEST_TCP_DIAL_SYNC_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_DIAL_SYNC_BACKEND_NAME "select"
#endif



typedef struct testtcpdialsync {
	xnetaddr Address;
} testtcpdialsync;



/* 把测试主机稳定解析到本地回环 Listener。 */
static xnetaddrlist* testTcpDialSyncLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testtcpdialsync* pContext = (testtcpdialsync*)pData;
	xnetaddr Address;

	if ( (strcmp(sHost, "sync.test") != 0) ||
		 ((Family != XNET_FAMILY_UNSPEC) &&
		  (Family != XNET_FAMILY_IPV4)) ) {
		return NULL;
	}
	Address = pContext->Address;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1);
}



/* 等待 Stream 正常关闭并释放调用方引用。 */
static void testTcpDialSyncClose(xnetstream* pStream)
{
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		testRequire(xrtNetStreamClose(pStream),
			"TCP Dial sync close request failed");
		testRequire(xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_CLOSE,
			xrtDeadlineAfter(5000000u),
			NULL
		), "TCP Dial sync close wait failed");
	}
	xrtNetStreamDestroy(pStream);
}



/* 验证受管 Resolver、候选竞速与阻塞连接共用同一状态机。 */
int main(void)
{
	testtcpdialsync Context;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetdialconfig DialConfig;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetbytes* pBytes;
	xbytesview View;
	xdeadline iDeadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_DIAL_SYNC_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP Dial sync engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP Dial sync listener address failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Context.Address),
		"TCP Dial sync listener create failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1;
	ResolverConfig.CacheEntries = 0;
	ResolverConfig.Lookup = testTcpDialSyncLookup;
	ResolverConfig.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"TCP Dial sync resolver create failed");
	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Family = XNET_FAMILY_IPV4;
	pClient = xrtNetConnect(
		pEngine,
		pResolver,
		"sync.test",
		Context.Address.Port,
		&DialConfig,
		NULL,
		NULL,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	if ( pClient == NULL ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"[ERROR] kind=%d domain=%s code=%d system=%d operation=%s message=%s data=%s\n",
			(int)xrtErrorKind(pError),
			xrtErrorDomain(pError),
			(int)xrtErrorCode(pError),
			(int)xrtErrorSystemCode(pError),
			xrtErrorOperation(pError),
			xrtErrorMessage(pError),
			xrtErrorData(pError)
		);
	}
	testRequire(pClient != NULL, "TCP Dial sync connect failed");
	pServer = xrtNetListenerAcceptWait(
		pListener,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	testRequire(pServer != NULL, "TCP Dial sync accept failed");
	testRequire(xrtNetStreamSend(
		pClient,
		"dial-sync",
		9
	) == XNET_RESULT_OK, "TCP Dial sync send failed");
	pBytes = xrtNetStreamRecv(
		pServer,
		0,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	testRequire(pBytes != NULL, "TCP Dial sync receive failed");
	View = xrtNetBytesView(pBytes);
	testRequire((View.Size == 9) &&
		(memcmp(View.Data, "dial-sync", 9) == 0),
		"TCP Dial sync payload mismatch");
	xrtNetBytesDestroy(pBytes);

	testTcpDialSyncClose(pClient);
	testTcpDialSyncClose(pServer);
	testRequire(xrtNetListenerClose(pListener),
		"TCP Dial sync listener close failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP Dial sync listener close timed out");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"TCP Dial sync resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP Dial sync engine destroy failed");
	printf(
		"[PASS] TCP Dial sync facade (%s)\n",
		TEST_TCP_DIAL_SYNC_BACKEND_NAME
	);
	return 0;
}
