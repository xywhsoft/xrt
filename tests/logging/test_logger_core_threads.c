#include "../test.h"
#include "../test_thread.h"



#define TEST_LOG_THREADS 8u
#define TEST_LOG_PER_THREAD 5000u
#define TEST_LOG_CHANGES 2000u



/* 并发 Sink 只通过原子计数观察回调。 */
typedef struct testlogthreadstate {
	xatomic64 Calls;
} testlogthreadstate;



/* 写入线程共享同一个 Logger。 */
typedef struct testlogwritercontext {
	xlogger* Logger;
} testlogwritercontext;



/* 修改线程反复发布快照和进程默认引用。 */
typedef struct testlogchangecontext {
	xlogger* Logger;
	xlogsink* Optional;
} testlogchangecontext;



/* 并发 Sink 无分配地记录调用次数。 */
static xlogresult testLogThreadWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	testlogthreadstate* pState = (testlogthreadstate*)pUserData;

	if ( pRecord->Level != XLOG_INFO ) {
		return XLOG_RESULT_ERROR;
	}
	(void)xrtAtomic64FetchAdd(&pState->Calls, 1u, XMEMORY_RELAXED);
	return XLOG_RESULT_WRITTEN;
}



/* 创建并发计数 Sink。 */
static xlogsink* testLogThreadSink(
	testlogthreadstate* pState,
	xstrview Name
)
{
	xlogsinkconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Name = Name;
	Config.Level = XLOG_TRACE;
	Config.Write = testLogThreadWrite;
	Config.UserData = pState;
	return xrtLogSinkCreate(&Config);
}



/* 连续提交固定数量记录并同时读取默认 Logger 引用。 */
static int testLogWriterRun(ptr pData)
{
	testlogwritercontext* pContext = (testlogwritercontext*)pData;

	for ( size_t i = 0; i < TEST_LOG_PER_THREAD; i++ ) {
		xlogger* pDefault;

		if (
			xrtLog(
				pContext->Logger,
				XLOG_INFO,
				XRT_STR_LITERAL("concurrent")
			) != XLOG_RESULT_WRITTEN
		) {
			return 1;
		}
		pDefault = xrtLogDefault();
		xrtLogFree(pDefault);
	}
	return 0;
}



/* 反复附加移除可选 Sink，并替换默认 Logger 引用。 */
static int testLogChangeRun(ptr pData)
{
	testlogchangecontext* pContext = (testlogchangecontext*)pData;

	for ( size_t i = 0; i < TEST_LOG_CHANGES; i++ ) {
		if (
			!xrtLogAttach(pContext->Logger, pContext->Optional) ||
			!xrtLogSinkSetLevel(
				pContext->Optional,
				(i & 1u) == 0 ? XLOG_TRACE : XLOG_WARN
			) ||
			!xrtLogSetDefault((i & 1u) == 0 ? pContext->Logger : NULL) ||
			!xrtLogDetach(pContext->Logger, pContext->Optional)
		) {
			return 1;
		}
	}
	return xrtLogSetDefault(NULL) ? 0 : 1;
}



/* 验证并发提交、快照替换、阈值和默认引用没有竞态或计数丢失。 */
int main(void)
{
	testlogthreadstate PrimaryState;
	testlogthreadstate OptionalState;
	testlogwritercontext WriterContext;
	testlogchangecontext ChangeContext;
	testthread arrThreads[TEST_LOG_THREADS + 1u];
	xlogstats Stats;
	xlogger* pLogger;
	xlogsink* pPrimary;
	xlogsink* pOptional;
	uint64 iExpected = TEST_LOG_THREADS * TEST_LOG_PER_THREAD;

	xrtAtomic64Init(&PrimaryState.Calls, 0);
	xrtAtomic64Init(&OptionalState.Calls, 0);
	pLogger = xrtLogCreate(XRT_STR_LITERAL("threads"), XLOG_TRACE);
	pPrimary = testLogThreadSink(
		&PrimaryState,
		XRT_STR_LITERAL("primary")
	);
	pOptional = testLogThreadSink(
		&OptionalState,
		XRT_STR_LITERAL("optional")
	);
	testRequire(
		(pLogger != NULL) && (pPrimary != NULL) && (pOptional != NULL) &&
		xrtLogAttach(pLogger, pPrimary),
		"Logger thread fixture creation failed"
	);
	WriterContext.Logger = pLogger;
	ChangeContext.Logger = pLogger;
	ChangeContext.Optional = pOptional;
	for ( size_t i = 0; i < TEST_LOG_THREADS; i++ ) {
		arrThreads[i].Proc = testLogWriterRun;
		arrThreads[i].Data = &WriterContext;
	}
	arrThreads[TEST_LOG_THREADS].Proc = testLogChangeRun;
	arrThreads[TEST_LOG_THREADS].Data = &ChangeContext;
	testThreadsStart(arrThreads, TEST_LOG_THREADS + 1u);
	testThreadsJoin(arrThreads, TEST_LOG_THREADS + 1u);
	for ( size_t i = 0; i < (TEST_LOG_THREADS + 1u); i++ ) {
		testRequire(
			arrThreads[i].Result == 0,
			"concurrent Logger worker failed"
		);
	}
	testRequire(
		xrtAtomic64Load(&PrimaryState.Calls, XMEMORY_RELAXED) == iExpected,
		"primary Sink lost concurrent records"
	);
	testRequire(
		xrtLogStats(pLogger, &Stats) &&
		(Stats.Submitted == iExpected) &&
		(Stats.Written == iExpected) &&
		(Stats.Failed == 0u) &&
		(xrtLogSinkCount(pLogger) == 1u) &&
		(xrtLogDefault() == NULL),
		"concurrent Logger statistics or final state mismatch"
	);
	xrtLogFree(pLogger);
	xrtLogSinkFree(pPrimary);
	xrtLogSinkFree(pOptional);
	printf("[PASS] Logger core threads\n");
	return 0;
}
