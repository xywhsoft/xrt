#include "../test.h"



#if !defined(TEST_TLS_DIAL_BACKEND)
	#define TEST_TLS_DIAL_BACKEND XNET_PORT_SELECT
#endif



typedef struct test_tls_dial_timer {
	xatomic32 Entered;
	xatomic32 Gate;
	xatomic32 Done;
	xatomic32 ReservedDone;
	xnetresult Result;
	xnetresult ReservedResult;
} test_tls_dial_timer;



/* 在截止时间前等待 Timer 或 Resolver 事件。 */
static void testTlsDialTimerWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 受控查询在开闸前保持 TLS Dial 活动。 */
static xnetaddrlist* testTlsDialTimerLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	test_tls_dial_timer* pContext =
		(test_tls_dial_timer*)pData;
	xnetaddr Address;

	(void)sHost;
	(void)Family;
	(void)xrtAtomic32FetchAdd(
		&pContext->Entered,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pContext->Gate,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 预留 Timer 的终态证明槽位已经归还。 */
static void testTlsDialReservedTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	test_tls_dial_timer* pContext =
		(test_tls_dial_timer*)pData;

	(void)pWorker;
	testRequire(Id != 0, "TLS dial reserved Timer lost its identity");
	pContext->ReservedResult = Result;
	xrtAtomic32Store(
		&pContext->ReservedDone,
		1,
		XMEMORY_RELEASE
	);
}



/* 恢复阶段的显式取消必须发布唯一终态。 */
static void testTlsDialTimerDone(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	test_tls_dial_timer* pContext =
		(test_tls_dial_timer*)pData;

	(void)pDial;
	testRequire((Result == XNET_RESULT_CANCELLED) &&
		(pStream == NULL) && (pError != NULL),
		"TLS dial Timer recovery terminal mismatch");
	pContext->Result = Result;
	xrtAtomic32Store(&pContext->Done, 1, XMEMORY_RELEASE);
}



/* 验证全过程 Timer 拒绝失败原子，并且槽位恢复后可以重新拨号。 */
int main(void)
{
	test_tls_dial_timer Test;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xtlsclientconfig ClientConfig;
	xtlsdialconfig DialConfig;
	xnetenginestats Stats;
	xtlscontext* pTlsContext;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xtlsdial* pDial;
	uint64 iReserved;

	memset(&Test, 0, sizeof(Test));
	pTlsContext = xrtTlsContextCreate(NULL);
	testRequire(pTlsContext != NULL,
		"TLS dial Timer context creation failed");
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pTlsContext;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_DIAL_BACKEND;
	EngineConfig.Workers = 1u;
	EngineConfig.TimerLimit = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS dial Timer engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Lookup = testTlsDialTimerLookup;
	ResolverConfig.LookupData = &Test;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"TLS dial Timer resolver creation failed");
	iReserved = xrtNetEngineAfter(
		pEngine,
		0,
		60000000u,
		testTlsDialReservedTimer,
		&Test
	);
	testRequire(iReserved != 0,
		"TLS dial Timer slot reservation failed");
	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.Timeout = 0;
	DialConfig.Timeout = 1000000u;
	xrtClearError();
	testRequire((xrtTlsDial(
		pEngine,
		pResolver,
		"timer-reject.tls.test",
		443,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL,
		testTlsDialTimerDone,
		&Test
	) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_AGAIN) &&
		xrtNetEngineStats(pEngine, &Stats) &&
		(Stats.TimersRejected != 0),
		"TLS dial Timer rejection was not failure-atomic");
	xrtClearError();
	testRequire(xrtNetEngineTimerCancel(pEngine, iReserved),
		"TLS dial reserved Timer cancellation failed");
	testTlsDialTimerWait(&Test.ReservedDone, 1u,
		"TLS dial reserved Timer did not finish");
	testRequire(Test.ReservedResult == XNET_RESULT_CANCELLED,
		"TLS dial reserved Timer cancellation result mismatch");

	pDial = xrtTlsDial(
		pEngine,
		pResolver,
		"timer-recover.tls.test",
		443,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL,
		testTlsDialTimerDone,
		&Test
	);
	testRequire(pDial != NULL,
		"TLS dial did not recover after Timer rejection");
	testTlsDialTimerWait(&Test.Entered, 1u,
		"TLS dial recovery lookup did not start");
	testRequire(xrtTlsDialCancel(pDial),
		"TLS dial recovery cancellation failed");
	testTlsDialTimerWait(&Test.Done, 1u,
		"TLS dial recovery did not finish");
	xrtAtomic32Store(&Test.Gate, 1, XMEMORY_RELEASE);
	xrtTlsDialDestroy(pDial);
	testRequire(xrtNetResolverDestroy(pResolver),
		"TLS dial Timer resolver destroy failed");
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		xrtThreadYield();
	}
	xrtTlsContextRelease(pTlsContext);
	printf("[PASS] TLS dial total Timer rejection and recovery\n");
	return 0;
}
