#include "../test.h"

#include "../test_fault_allocator.h"



#define TEST_LOG_OOM_SINKS 257u



/* OOM Sink 状态只验证创建失败时是否错误接管生命周期。 */
typedef struct testlogoomstate {
	size_t Drops;
} testlogoomstate;



/* OOM 测试 Sink 安静接受记录。 */
static xlogresult testLogOomWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	(void)pRecord;
	(void)pUserData;
	return XLOG_RESULT_WRITTEN;
}



/* OOM 测试记录最终生命周期回调。 */
static void testLogOomDrop(ptr pUserData)
{
	testlogoomstate* pState = (testlogoomstate*)pUserData;

	pState->Drops++;
}



/* 使用指定名称创建 OOM 测试 Sink。 */
static xlogsink* testLogOomSink(
	testlogoomstate* pState,
	xstrview Name
)
{
	xlogsinkconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Name = Name;
	Config.Level = XLOG_TRACE;
	Config.Write = testLogOomWrite;
	Config.Drop = testLogOomDrop;
	Config.UserData = pState;
	return xrtLogSinkCreate(&Config);
}



/* 验证大对象创建失败不接管调用方数据且可以恢复。 */
static void testLogOomCreate(
	testfaultallocator* pAllocator,
	xstrview LargeName
)
{
	testlogoomstate State;
	xlogger* pLogger;
	xlogsink* pSink;
	size_t iLive;

	memset(&State, 0, sizeof(State));
	iLive = pAllocator->Live;
	pAllocator->FailAt = pAllocator->Calls + 1u;
	pAllocator->Hit = false;
	pLogger = xrtLogCreate(LargeName, XLOG_TRACE);
	testRequire(
		(pLogger == NULL) && pAllocator->Hit &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(pAllocator->Live == iLive),
		"Logger creation OOM was not transactional"
	);
	xrtClearError();

	pAllocator->FailAt = pAllocator->Calls + 1u;
	pAllocator->Hit = false;
	pSink = testLogOomSink(&State, LargeName);
	testRequire(
		(pSink == NULL) && pAllocator->Hit &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(State.Drops == 0u) && (pAllocator->Live == iLive),
		"Sink creation OOM transferred UserData or leaked"
	);
	xrtClearError();

	pAllocator->FailAt = SIZE_MAX;
	pLogger = xrtLogCreate(LargeName, XLOG_TRACE);
	pSink = testLogOomSink(&State, LargeName);
	testRequire(
		(pLogger != NULL) && (pSink != NULL),
		"Logger objects did not recover after creation OOM"
	);
	xrtLogFree(pLogger);
	xrtLogSinkFree(pSink);
	testRequire(State.Drops == 1u, "recovered Sink lifecycle failed");
}



/* 验证超过小块池阈值的快照分配失败不改变已发布 Sink 集合。 */
static void testLogOomSnapshot(testfaultallocator* pAllocator)
{
	testlogoomstate State;
	xlogsink* arrSinks[TEST_LOG_OOM_SINKS];
	xlogger* pLogger;
	size_t iLive;

	memset(&State, 0, sizeof(State));
	memset(arrSinks, 0, sizeof(arrSinks));
	pAllocator->FailAt = SIZE_MAX;
	pLogger = xrtLogCreate(XRT_STR_LITERAL("snapshot"), XLOG_TRACE);
	testRequire(pLogger != NULL, "snapshot OOM Logger creation failed");
	for ( size_t i = 0; i < TEST_LOG_OOM_SINKS; i++ ) {
		arrSinks[i] = testLogOomSink(
			&State,
			XRT_STR_LITERAL("sink")
		);
		testRequire(arrSinks[i] != NULL, "snapshot OOM Sink creation failed");
	}
	for ( size_t i = 0; i < (TEST_LOG_OOM_SINKS - 1u); i++ ) {
		testRequire(
			xrtLogAttach(pLogger, arrSinks[i]),
			"snapshot OOM fixture attach failed"
		);
	}
	testRequire(
		xrtLogSinkCount(pLogger) == (TEST_LOG_OOM_SINKS - 1u),
		"snapshot OOM fixture count mismatch"
	);
	iLive = pAllocator->Live;
	pAllocator->FailAt = pAllocator->Calls + 1u;
	pAllocator->Hit = false;
	testRequire(
		!xrtLogAttach(pLogger, arrSinks[TEST_LOG_OOM_SINKS - 1u]) &&
		pAllocator->Hit &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtLogSinkCount(pLogger) == (TEST_LOG_OOM_SINKS - 1u)) &&
		(pAllocator->Live == iLive),
		"snapshot allocation OOM changed the published Sink set"
	);
	xrtClearError();
	pAllocator->FailAt = SIZE_MAX;
	testRequire(
		xrtLogAttach(pLogger, arrSinks[TEST_LOG_OOM_SINKS - 1u]) &&
		(xrtLogSinkCount(pLogger) == TEST_LOG_OOM_SINKS),
		"snapshot attach did not recover after OOM"
	);
	xrtLogFree(pLogger);
	for ( size_t i = 0; i < TEST_LOG_OOM_SINKS; i++ ) {
		xrtLogSinkFree(arrSinks[i]);
	}
	testRequire(
		State.Drops == TEST_LOG_OOM_SINKS,
		"snapshot OOM cleanup lost Sink lifetimes"
	);
}



/* 执行 Logger 大对象和大快照故障注入。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	char arrLargeName[2048];

	memset(arrLargeName, 'L', sizeof(arrLargeName));
	testRequire(
		xrtSetAllocator(&Allocator),
		"Logger OOM allocator install failed"
	);
	testLogOomCreate(
		&State,
		(xstrview){ arrLargeName, sizeof(arrLargeName) }
	);
	testLogOomSnapshot(&State);
	xrtClearError();
	testMemoryDebugDrain("Logger core OOM memory debug reset failed");
	printf("[PASS] Logger core OOM\n");
	return 0;
}
