#include "../test.h"



/* 测试目标允许阻塞第一条记录、注入错误并保存工作线程实际看到的数据。 */
typedef struct testlogasynctarget {
	xevent Entered;
	xevent Release;
	xlogsink* Async;
	bool BlockFirst;
	bool FailWrite;
	bool FailFlush;
	bool Recurse;
	xlogresult RecursiveResult;
	size_t Count;
	char Messages[32][64];
	char Logger[64];
	char File[64];
	char Function[64];
	char FieldName[64];
	char FieldValue[64];
	char ErrorMessage[64];
} testlogasynctarget;



/* 关闭测试把最后一个 Async Sink 引用交给独立线程释放。 */
typedef struct testlogasyncfree {
	xlogsink* Sink;
	xevent Started;
} testlogasyncfree;



/* 测试自身从零结尾文本构造借用视图，不让 Async 模块依赖 String。 */
static xstrview testLogAsyncView(cstr sText)
{
	return (xstrview){ sText, strlen(sText) };
}



/* 把任意字符串视图复制到固定测试缓冲，并保证末尾零字节。 */
static void testLogAsyncViewCopy(
	char* sTarget,
	size_t iCapacity,
	xstrview Value
)
{
	size_t iSize = Value.Size < (iCapacity - 1u)
		? Value.Size
		: (iCapacity - 1u);

	if ( iSize != 0u ) {
		memcpy(sTarget, Value.Data, iSize);
	}
	sTarget[iSize] = 0;
}



/* 创建一个测试错误并设置到当前执行上下文。 */
static void testLogAsyncSetError(cstr sMessage)
{
	xerror* pError = xrtErrorCreate(XERR_IO, "test.async", 7, sMessage);

	testRequire(pError != NULL, "Logger async test error allocation failed");
	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 测试目标在解除阻塞后读取记录，暴露任何借用数据悬空问题。 */
static xlogresult testLogAsyncTargetWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	testlogasynctarget* pTarget = (testlogasynctarget*)pUserData;
	size_t iIndex = pTarget->Count;

	if ( pTarget->BlockFirst && (iIndex == 0u) ) {
		testRequire(
			xrtEventSet(&pTarget->Entered),
			"Logger async target enter signal failed"
		);
		testRequire(
			xrtEventWait(&pTarget->Release) == XWAIT_OK,
			"Logger async target release wait failed"
		);
	}
	if ( pTarget->FailWrite ) {
		pTarget->FailWrite = false;
		testLogAsyncSetError("target write failed");
		return XLOG_RESULT_ERROR;
	}
	if ( iIndex < 32u ) {
		testLogAsyncViewCopy(
			pTarget->Messages[iIndex],
			sizeof(pTarget->Messages[iIndex]),
			pRecord->Message
		);
	}
	pTarget->Count++;
	if ( iIndex == 0u ) {
		testLogAsyncViewCopy(
			pTarget->Logger,
			sizeof(pTarget->Logger),
			pRecord->Logger
		);
		testLogAsyncViewCopy(
			pTarget->File,
			sizeof(pTarget->File),
			pRecord->File
		);
		testLogAsyncViewCopy(
			pTarget->Function,
			sizeof(pTarget->Function),
			pRecord->Function
		);
		if ( pRecord->FieldCount >= 1u ) {
			testLogAsyncViewCopy(
				pTarget->FieldName,
				sizeof(pTarget->FieldName),
				pRecord->Fields[0].Name
			);
			testLogAsyncViewCopy(
				pTarget->FieldValue,
				sizeof(pTarget->FieldValue),
				pRecord->Fields[0].Value.String
			);
		}
		if (
			(pRecord->FieldCount >= 2u) &&
			(pRecord->Fields[1].Value.Error != NULL)
		) {
			testLogAsyncViewCopy(
				pTarget->ErrorMessage,
				sizeof(pTarget->ErrorMessage),
				testLogAsyncView(
					xrtErrorMessage(pRecord->Fields[1].Value.Error)
				)
			);
		}
	}
	if ( pTarget->Recurse ) {
		pTarget->Recurse = false;
		pTarget->RecursiveResult = xrtLogSinkSubmit(
			pTarget->Async,
			pRecord
		);
	}
	return XLOG_RESULT_WRITTEN;
}



/* 测试 Flush 回调可以精确注入一次目标错误。 */
static bool testLogAsyncTargetFlush(ptr pUserData)
{
	testlogasynctarget* pTarget = (testlogasynctarget*)pUserData;

	if ( !pTarget->FailFlush ) {
		return true;
	}
	pTarget->FailFlush = false;
	testLogAsyncSetError("target flush failed");
	return false;
}



/* 初始化测试目标的两个人工复位事件。 */
static void testLogAsyncTargetInit(testlogasynctarget* pTarget)
{
	memset(pTarget, 0, sizeof(testlogasynctarget));
	testRequire(
		xrtEventInit(&pTarget->Entered, true, false),
		"Logger async entered event init failed"
	);
	testRequire(
		xrtEventInit(&pTarget->Release, true, false),
		"Logger async release event init failed"
	);
}



/* 释放测试目标事件。 */
static void testLogAsyncTargetUnit(testlogasynctarget* pTarget)
{
	testRequire(
		xrtEventUnit(&pTarget->Release),
		"Logger async release event unit failed"
	);
	testRequire(
		xrtEventUnit(&pTarget->Entered),
		"Logger async entered event unit failed"
	);
}



/* 创建一个借用测试状态的同步目标 Sink。 */
static xlogsink* testLogAsyncTarget(testlogasynctarget* pTarget)
{
	xlogsinkconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Name = XRT_STR_LITERAL("test-target");
	Config.Level = XLOG_TRACE;
	Config.Write = testLogAsyncTargetWrite;
	Config.Flush = testLogAsyncTargetFlush;
	Config.UserData = pTarget;
	return xrtLogSinkCreate(&Config);
}



/* 提交一条最小文本记录。 */
static xlogresult testLogAsyncSubmit(xlogsink* pSink, cstr sMessage)
{
	xlogrecord Record;

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = testLogAsyncView(sMessage);
	return xrtLogSinkSubmit(pSink, &Record);
}



/* 验证完整记录深拷贝、默认配置、目标身份和基础统计。 */
static void testLogAsyncCopy(void)
{
	testlogasynctarget State;
	xlogasyncconfig Config;
	xlogasyncstats Stats;
	xlogrecord Record;
	xlogfield Fields[2];
	xlogsink* pTarget;
	xlogsink* pAsync;
	xerror* pFieldError;
	char sLogger[] = "borrowed-logger";
	char sMessage[] = "borrowed-message";
	char sFile[] = "borrowed.c";
	char sFunction[] = "borrowedFunction";
	char sName[] = "field-name";
	char sValue[] = "field-value";

	testLogAsyncTargetInit(&State);
	State.BlockFirst = true;
	pTarget = testLogAsyncTarget(&State);
	testRequire(pTarget != NULL, "Logger async copy target create failed");
	testRequire(
		xrtLogAsyncConfigInit(&Config),
		"Logger async default config failed"
	);
	testRequire(
		(Config.Level == XLOG_TRACE) &&
		(Config.Full == XLOG_ASYNC_DROP_NEWEST) &&
		(Config.Shutdown == XLOG_ASYNC_DRAIN) &&
		(Config.Capacity == XLOG_ASYNC_CAPACITY_DEFAULT) &&
		(Config.RecordLimit == XLOG_ASYNC_RECORD_LIMIT_DEFAULT) &&
		(Config.ByteLimit == XLOG_ASYNC_BYTE_LIMIT_DEFAULT),
		"Logger async defaults changed"
	);
	pAsync = xrtLogAsync(pTarget, NULL);
	testRequire(pAsync != NULL, "Logger async copy sink create failed");
	testRequire(
		xrtLogAsyncTarget(pAsync) == pTarget,
		"Logger async target identity mismatch"
	);

	pFieldError = xrtErrorCreate(
		XERR_VALUE,
		"test.async",
		8,
		"borrowed-error"
	);
	testRequire(pFieldError != NULL, "Logger async field error create failed");
	Fields[0] = xrtLogFieldString(
		testLogAsyncView(sName),
		testLogAsyncView(sValue)
	);
	Fields[1] = xrtLogFieldError(XRT_STR_LITERAL("error"), pFieldError);
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Logger = testLogAsyncView(sLogger);
	Record.Message = testLogAsyncView(sMessage);
	Record.Fields = Fields;
	Record.FieldCount = 2u;
	Record.File = testLogAsyncView(sFile);
	Record.Function = testLogAsyncView(sFunction);
	testRequire(
		xrtLogSinkSubmit(pAsync, &Record) == XLOG_RESULT_WRITTEN,
		"Logger async deep copy submit failed"
	);
	testRequire(
		xrtEventWait(&State.Entered) == XWAIT_OK,
		"Logger async deep copy target did not enter"
	);
	memset(sLogger, 'x', strlen(sLogger));
	memset(sMessage, 'x', strlen(sMessage));
	memset(sFile, 'x', strlen(sFile));
	memset(sFunction, 'x', strlen(sFunction));
	memset(sName, 'x', strlen(sName));
	memset(sValue, 'x', strlen(sValue));
	xrtErrorFree(pFieldError);
	testRequire(
		xrtEventSet(&State.Release),
		"Logger async deep copy release failed"
	);
	testRequire(xrtLogSinkFlush(pAsync), "Logger async deep copy flush failed");
	testRequire(
		strcmp(State.Logger, "borrowed-logger") == 0 &&
		strcmp(State.Messages[0], "borrowed-message") == 0 &&
		strcmp(State.File, "borrowed.c") == 0 &&
		strcmp(State.Function, "borrowedFunction") == 0 &&
		strcmp(State.FieldName, "field-name") == 0 &&
		strcmp(State.FieldValue, "field-value") == 0 &&
		strcmp(State.ErrorMessage, "borrowed-error") == 0,
		"Logger async retained borrowed record data"
	);
	testRequire(
		xrtLogAsyncStats(pAsync, &Stats) &&
		(Stats.Enqueued == 1u) &&
		(Stats.Processed == 1u) &&
		(Stats.Written == 1u) &&
		(Stats.Queued == 0u) &&
		(Stats.QueueBytes == 0u) &&
		(Stats.PeakQueued >= 1u) &&
		(Stats.PeakBytes != 0u),
		"Logger async copy statistics mismatch"
	);
	xrtClearError();
	testRequire(
		xrtLogAsyncLastError(pAsync) == NULL &&
		(xrtGetError() == NULL),
		"Logger async empty last error changed context"
	);
	xrtLogSinkFree(pAsync);
	xrtLogSinkFree(pTarget);
	testLogAsyncTargetUnit(&State);
}



/* 验证队列满时丢弃最新或覆盖最旧记录的精确差异。 */
static void testLogAsyncOverflow(xlogasyncfull Full)
{
	testlogasynctarget State;
	xlogasyncconfig Config;
	xlogasyncstats Stats;
	xlogsink* pTarget;
	xlogsink* pAsync;
	xlogresult Third;

	testLogAsyncTargetInit(&State);
	State.BlockFirst = true;
	pTarget = testLogAsyncTarget(&State);
	testRequire(pTarget != NULL, "Logger async overflow target create failed");
	testRequire(xrtLogAsyncConfigInit(&Config), "Logger async config failed");
	Config.Capacity = 1u;
	Config.Full = Full;
	pAsync = xrtLogAsync(pTarget, &Config);
	testRequire(pAsync != NULL, "Logger async overflow sink create failed");
	testRequire(
		testLogAsyncSubmit(pAsync, "A") == XLOG_RESULT_WRITTEN,
		"Logger async first overflow submit failed"
	);
	testRequire(
		xrtEventWait(&State.Entered) == XWAIT_OK,
		"Logger async overflow target did not block"
	);
	testRequire(
		testLogAsyncSubmit(pAsync, "B") == XLOG_RESULT_WRITTEN,
		"Logger async second overflow submit failed"
	);
	Third = testLogAsyncSubmit(pAsync, "C");
	if ( Full == XLOG_ASYNC_DROP_NEWEST ) {
		testRequire(
			Third == XLOG_RESULT_DROPPED,
			"Logger async newest overflow result mismatch"
		);
	} else {
		testRequire(
			Third == XLOG_RESULT_WRITTEN,
			"Logger async oldest overflow result mismatch"
		);
	}
	testRequire(xrtEventSet(&State.Release), "Logger async release failed");
	testRequire(xrtLogSinkFlush(pAsync), "Logger async overflow flush failed");
	testRequire(
		(State.Count == 2u) &&
		(strcmp(State.Messages[0], "A") == 0) &&
		(strcmp(
			State.Messages[1],
			Full == XLOG_ASYNC_DROP_NEWEST ? "B" : "C"
		) == 0),
		"Logger async overflow ordering mismatch"
	);
	testRequire(
		xrtLogAsyncStats(pAsync, &Stats) &&
		(
			(Full == XLOG_ASYNC_DROP_NEWEST &&
			 Stats.DroppedNewest == 1u &&
			 Stats.DroppedOldest == 0u) ||
			(Full == XLOG_ASYNC_DROP_OLDEST &&
			 Stats.DroppedNewest == 0u &&
			 Stats.DroppedOldest == 1u)
		),
		"Logger async overflow statistics mismatch"
	);
	xrtLogSinkFree(pAsync);
	xrtLogSinkFree(pTarget);
	testLogAsyncTargetUnit(&State);
}



/* 验证记录硬上限在分配和入队前拒绝过大数据。 */
static void testLogAsyncRecordLimit(void)
{
	testlogasynctarget State;
	xlogasyncconfig Config;
	xlogrecord Record;
	xlogsink* pTarget;
	xlogsink* pAsync;
	char sMessage[512];

	testLogAsyncTargetInit(&State);
	pTarget = testLogAsyncTarget(&State);
	testRequire(pTarget != NULL, "Logger async limit target create failed");
	testRequire(xrtLogAsyncConfigInit(&Config), "Logger async config failed");
	Config.RecordLimit = 256u;
	Config.ByteLimit = 512u;
	pAsync = xrtLogAsync(pTarget, &Config);
	testRequire(pAsync != NULL, "Logger async limit sink create failed");
	memset(sMessage, 'z', sizeof(sMessage));
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = (xstrview){ sMessage, sizeof(sMessage) };
	testRequire(
		xrtLogSinkSubmit(pAsync, &Record) == XLOG_RESULT_ERROR &&
		xrtErrorFind(
			xrtGetError(),
			"xrt.log",
			XLOG_ERROR_ASYNC_RECORD
		) != NULL,
		"Logger async record limit was not enforced"
	);
	xrtClearError();
	xrtLogSinkFree(pAsync);
	xrtLogSinkFree(pTarget);
	testLogAsyncTargetUnit(&State);
}



/* 验证后台目标错误、Flush 错误和递归记录都可独立观测。 */
static void testLogAsyncErrors(void)
{
	testlogasynctarget State;
	xlogasyncstats Stats;
	xlogsink* pTarget;
	xlogsink* pAsync;
	xerror* pError;

	testLogAsyncTargetInit(&State);
	State.FailWrite = true;
	pTarget = testLogAsyncTarget(&State);
	testRequire(pTarget != NULL, "Logger async error target create failed");
	pAsync = xrtLogAsync(pTarget, NULL);
	testRequire(pAsync != NULL, "Logger async error sink create failed");
	testRequire(
		testLogAsyncSubmit(pAsync, "write-error") == XLOG_RESULT_WRITTEN,
		"Logger async failed to accept target error record"
	);
	testRequire(xrtLogSinkFlush(pAsync), "Logger async error barrier failed");
	pError = xrtLogAsyncLastError(pAsync);
	testRequire(
		(pError != NULL) &&
		(xrtErrorFind(
			pError,
			"xrt.log",
			XLOG_ERROR_ASYNC_TARGET
		) != NULL),
		"Logger async target error was not retained"
	);
	xrtErrorFree(pError);

	State.FailFlush = true;
	testRequire(
		!xrtLogSinkFlush(pAsync) &&
		(xrtErrorFind(
			xrtGetError(),
			"xrt.log",
			XLOG_ERROR_ASYNC_FLUSH
		) != NULL),
		"Logger async flush error was not propagated"
	);
	xrtClearError();
	testRequire(xrtLogSinkFlush(pAsync), "Logger async flush did not recover");

	State.Async = pAsync;
	State.Recurse = true;
	testRequire(
		testLogAsyncSubmit(pAsync, "recursive") == XLOG_RESULT_WRITTEN,
		"Logger async recursive outer submit failed"
	);
	testRequire(xrtLogSinkFlush(pAsync), "Logger async recursive flush failed");
	testRequire(
		(State.RecursiveResult == XLOG_RESULT_DROPPED) &&
		xrtLogAsyncStats(pAsync, &Stats) &&
		(Stats.ReentrantDrops == 1u) &&
		(Stats.Failed >= 2u),
		"Logger async recursion or error statistics mismatch"
	);
	xrtLogSinkFree(pAsync);
	xrtLogSinkFree(pTarget);
	testLogAsyncTargetUnit(&State);
}



/* 验证最后一个包装器引用默认会排空全部已接受记录。 */
static void testLogAsyncDrain(void)
{
	testlogasynctarget State;
	xlogsink* pTarget;
	xlogsink* pAsync;

	testLogAsyncTargetInit(&State);
	pTarget = testLogAsyncTarget(&State);
	testRequire(pTarget != NULL, "Logger async drain target create failed");
	pAsync = xrtLogAsync(pTarget, NULL);
	testRequire(pAsync != NULL, "Logger async drain sink create failed");
	for ( size_t i = 0; i < 16u; i++ ) {
		testRequire(
			testLogAsyncSubmit(pAsync, "drain") == XLOG_RESULT_WRITTEN,
			"Logger async drain submit failed"
		);
	}
	xrtLogSinkFree(pAsync);
	testRequire(State.Count == 16u, "Logger async shutdown did not drain queue");
	xrtLogSinkFree(pTarget);
	testLogAsyncTargetUnit(&State);
}



/* 验证一行创建并附加 Helper 的目标引用和 Logger 所有权。 */
static void testLogAsyncAdd(void)
{
	testlogasynctarget State;
	xlogger* pLogger;
	xlogsink* pTarget;

	testLogAsyncTargetInit(&State);
	pTarget = testLogAsyncTarget(&State);
	testRequire(pTarget != NULL, "Logger async add target create failed");
	pLogger = xrtLogCreate(XRT_STR_LITERAL("async-add"), XLOG_TRACE);
	testRequire(pLogger != NULL, "Logger async add logger create failed");
	testRequire(
		xrtLogAddAsync(pLogger, pTarget, NULL),
		"Logger async add helper failed"
	);
	xrtLogSinkFree(pTarget);
	testRequire(
		xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("added")) ==
		XLOG_RESULT_WRITTEN,
		"Logger async add submit failed"
	);
	testRequire(xrtLogFlush(pLogger), "Logger async add flush failed");
	testRequire(
		(State.Count == 1u) &&
		(strcmp(State.Messages[0], "added") == 0) &&
		(strcmp(State.Logger, "async-add") == 0),
		"Logger async add helper ownership mismatch"
	);
	xrtLogFree(pLogger);
	testLogAsyncTargetUnit(&State);
}



/* 独立线程先通知主线程，再释放最后一个 Async Sink 引用。 */
static int32 testLogAsyncFreeRun(ptr pData)
{
	testlogasyncfree* pFree = (testlogasyncfree*)pData;

	if ( !xrtEventSet(&pFree->Started) ) {
		return 1;
	}
	xrtLogSinkFree(pFree->Sink);
	return 0;
}



/* 验证 DISCARD 只完成正在执行的目标调用，并丢弃尚未开始的记录。 */
static void testLogAsyncDiscard(void)
{
	testlogasynctarget State;
	testlogasyncfree Free;
	xlogasyncconfig Config;
	xlogsink* pTarget;
	xlogsink* pAsync;
	xthread* pThread;

	testLogAsyncTargetInit(&State);
	State.BlockFirst = true;
	pTarget = testLogAsyncTarget(&State);
	testRequire(pTarget != NULL, "Logger async discard target create failed");
	testRequire(xrtLogAsyncConfigInit(&Config), "Logger async config failed");
	Config.Capacity = 4u;
	Config.Shutdown = XLOG_ASYNC_DISCARD;
	pAsync = xrtLogAsync(pTarget, &Config);
	testRequire(pAsync != NULL, "Logger async discard sink create failed");
	testRequire(
		testLogAsyncSubmit(pAsync, "running") == XLOG_RESULT_WRITTEN,
		"Logger async discard running submit failed"
	);
	testRequire(
		xrtEventWait(&State.Entered) == XWAIT_OK,
		"Logger async discard target did not block"
	);
	testRequire(
		testLogAsyncSubmit(pAsync, "discard-1") == XLOG_RESULT_WRITTEN &&
		testLogAsyncSubmit(pAsync, "discard-2") == XLOG_RESULT_WRITTEN,
		"Logger async discard queue submit failed"
	);
	memset(&Free, 0, sizeof(Free));
	Free.Sink = pAsync;
	testRequire(
		xrtEventInit(&Free.Started, true, false),
		"Logger async discard start event init failed"
	);
	pThread = xrtThreadCreate(testLogAsyncFreeRun, &Free, 0u);
	testRequire(pThread != NULL, "Logger async discard free thread failed");
	testRequire(
		xrtEventWait(&Free.Started) == XWAIT_OK,
		"Logger async discard free thread did not start"
	);
	xrtSleep(50u);
	testRequire(
		xrtEventSet(&State.Release),
		"Logger async discard target release failed"
	);
	testRequire(
		xrtThreadWait(pThread) == XWAIT_OK &&
		(xrtThreadExitCode(pThread) == 0),
		"Logger async discard free thread failed"
	);
	xrtThreadDestroy(pThread);
	testRequire(
		(State.Count == 1u) &&
		(strcmp(State.Messages[0], "running") == 0),
		"Logger async discard processed queued records"
	);
	testRequire(
		xrtEventUnit(&Free.Started),
		"Logger async discard start event unit failed"
	);
	xrtLogSinkFree(pTarget);
	testLogAsyncTargetUnit(&State);
}



/* 覆盖 Async Sink 的深拷贝、流控、错误和生命周期契约。 */
int main(void)
{
	testLogAsyncCopy();
	testLogAsyncOverflow(XLOG_ASYNC_DROP_NEWEST);
	testLogAsyncOverflow(XLOG_ASYNC_DROP_OLDEST);
	testLogAsyncRecordLimit();
	testLogAsyncErrors();
	testLogAsyncDrain();
	testLogAsyncAdd();
	testLogAsyncDiscard();
	printf("[PASS] Logger async\n");
	return 0;
}
