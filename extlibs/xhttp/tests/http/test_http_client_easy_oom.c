#include "../test.h"



/* OOM 用例只在线程间发布预取消调用的最终网络结果。 */
typedef struct test_http_easy_oom_result {
	xatomic32 Completed;
	xnetresult Result;
} test_http_easy_oom_result;



/* 保存预取消调用终态并释放 callback 未产生的响应。 */
static void testHttpEasyOomDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_easy_oom_result* pState =
		(test_http_easy_oom_result*)pData;

	(void)pCall;
	pState->Result = pResult->Result;
	xrtHttpResponseDestroy(pResult->Response);
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 在固定截止时间内等待预取消调用释放其异步内部引用。 */
static void testHttpEasyOomWait(const xatomic32* pCompleted)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtAtomic32Load(
		pCompleted,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP easy OOM call did not complete"
		);
		xrtThreadYield();
	}
}



/* 执行一次可同步失败或异步预取消的 GET 便利调用。 */
static bool testHttpEasyOomAttempt(
	xhttpclient* pClient,
	const xhttpcalloptions* pOptions
)
{
	test_http_easy_oom_result State;
	xhttpcall* pCall;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Completed, 0);
	pCall = xrtHttpClientGet(
		pClient,
		XRT_STR_LITERAL("http://cancel.test/easy-oom"),
		pOptions,
		testHttpEasyOomDone,
		&State
	);
	if ( pCall == NULL ) {
		return false;
	}
	testHttpEasyOomWait(&State.Completed);
	testRequire(
		State.Result == XNET_RESULT_CANCELLED,
		"HTTP easy OOM pre-cancel result mismatch"
	);
	xrtHttpCallDestroy(pCall);
	return true;
}



/* 扫描提交线程全部逻辑分配点并验证失败恢复与最终资源排空。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions Options;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xcancel* pCancel;
	xdeadline Deadline;
	size_t iFail;
	size_t iFailures = 0;
	bool bComplete = false;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP easy OOM Engine start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	testRequire(
		pClient != NULL,
		"HTTP easy OOM Client create failed"
	);
	pCancel = xrtCancelCreate();
	testRequire(
		(pCancel != NULL) && xrtCancelRequest(pCancel),
		"HTTP easy OOM cancel setup failed"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.Cancel = pCancel;
	Options.Timeout = XHTTP_CLIENT_TIMEOUT_NONE;
	Options.IdleTimeout = XHTTP_CLIENT_TIMEOUT_NONE;

	for ( iFail = 0; iFail < 96u; iFail++ ) {
		bool bAttempt;
		bool bTriggered;

		testRequire(
			xrtMemDebugFailAfter((uint64)iFail),
			"HTTP easy OOM fault setup failed"
		);
		bAttempt = testHttpEasyOomAttempt(
			pClient,
			&Options
		);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( !bAttempt ) {
			testRequire(
				bTriggered &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"HTTP easy failed without injected OOM"
			);
			iFailures++;
			xrtClearError();
			continue;
		}
		testRequire(
			!bTriggered,
			"HTTP easy ignored a triggered allocation fault"
		);
		bComplete = true;
		break;
	}
	testRequire(
		(iFailures != 0) && bComplete,
		"HTTP easy OOM sweep missed failure or success paths"
	);

	xrtCancelDestroy(pCancel);
	xrtHttpClientDestroy(pClient);
	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP easy OOM retained an Engine object"
		);
		xrtThreadYield();
	}
	printf(
		"[PASS] HTTP client convenience OOM fault_points=%u\n",
		(unsigned)iFailures
	);
	return 0;
}


