#include "../test.h"



/* 测试 Sink 保存最近记录、可配置结果和生命周期计数。 */
typedef struct testlogsinkstate {
	xlogger* Logger;
	xlogsink* Sink;
	xlogresult Result;
	size_t Calls;
	size_t Flushes;
	size_t Drops;
	size_t Fields;
	xloglevel Level;
	uint64 ThreadId;
	uint32 Line;
	bool Nested;
	bool Detach;
	bool FlushResult;
	char LoggerName[64];
	char Message[128];
} testlogsinkstate;



/* 把借用视图复制到测试固定缓冲。 */
static void testLogViewCopy(char* sTarget, size_t iCapacity, xstrview View)
{
	size_t iSize = View.Size < (iCapacity - 1u)
		? View.Size
		: (iCapacity - 1u);

	if ( iSize != 0 ) {
		memcpy(sTarget, View.Data, iSize);
	}
	sTarget[iSize] = 0;
}



/* 记录回调验证借用数据可以在回调期间完整访问。 */
static xlogresult testLogWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	testlogsinkstate* pState = (testlogsinkstate*)pUserData;

	pState->Calls++;
	pState->Fields = pRecord->FieldCount;
	pState->Level = pRecord->Level;
	pState->ThreadId = pRecord->ThreadId;
	pState->Line = pRecord->Line;
	testLogViewCopy(
		pState->LoggerName,
		sizeof(pState->LoggerName),
		pRecord->Logger
	);
	testLogViewCopy(
		pState->Message,
		sizeof(pState->Message),
		pRecord->Message
	);
	if ( pState->Detach ) {
		pState->Detach = false;
		testRequire(
			xrtLogDetach(pState->Logger, pState->Sink),
			"Sink could not detach itself from its callback"
		);
	}
	if ( !pState->Nested && (pState->Logger != NULL) ) {
		pState->Nested = true;
		testRequire(
			xrtLog(
				pState->Logger,
				XLOG_INFO,
				XRT_STR_LITERAL("nested")
			) == XLOG_RESULT_WRITTEN,
			"recursive log submission failed"
		);
	}
	return pState->Result;
}



/* Flush 回调返回测试状态中的显式结果。 */
static bool testLogFlush(ptr pUserData)
{
	testlogsinkstate* pState = (testlogsinkstate*)pUserData;

	pState->Flushes++;
	return pState->FlushResult;
}



/* Drop 回调只记录最后一个引用已经释放。 */
static void testLogDrop(ptr pUserData)
{
	testlogsinkstate* pState = (testlogsinkstate*)pUserData;

	pState->Drops++;
}



/* 创建使用测试状态的 Sink。 */
static xlogsink* testLogSink(
	testlogsinkstate* pState,
	xstrview Name,
	xloglevel Level
)
{
	xlogsinkconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Name = Name;
	Config.Level = Level;
	Config.Write = testLogWrite;
	Config.Flush = testLogFlush;
	Config.Drop = testLogDrop;
	Config.UserData = pState;
	pState->Result = XLOG_RESULT_WRITTEN;
	pState->FlushResult = true;
	pState->Nested = true;
	return xrtLogSinkCreate(&Config);
}



/* 验证字段构造、源码元数据、过滤、名称覆盖和统计。 */
static void testLogBasic(void)
{
	testlogsinkstate State;
	xlogfield Fields[4];
	xlogger* pLogger;
	xlogsink* pSink;
	xlogstats LoggerStats;
	xlogstats SinkStats;
	xlogrecord Record;

	memset(&State, 0, sizeof(State));
	pLogger = xrtLogCreate(XRT_STR_LITERAL("service"), XLOG_DEBUG);
	pSink = testLogSink(&State, XRT_STR_LITERAL("capture"), XLOG_INFO);
	testRequire(
		(pLogger != NULL) && (pSink != NULL),
		"Logger core fixture creation failed"
	);
	testRequire(
		xrtLogAttach(pLogger, pSink),
		"Sink attach failed"
	);
	Fields[0] = xrtLogFieldBool(XRT_STR_LITERAL("ok"), true);
	Fields[1] = xrtLogFieldInt(XRT_STR_LITERAL("code"), -7);
	Fields[2] = xrtLogFieldUInt(XRT_STR_LITERAL("size"), 9u);
	Fields[3] = xrtLogFieldString(
		XRT_STR_LITERAL("kind"),
		XRT_STR_LITERAL("core")
	);
	testRequire(
		xrtLogSource(
			pLogger,
			XLOG_DEBUG,
			XRT_STR_LITERAL("filtered"),
			NULL,
			0,
			XRT_STR_LITERAL("logger.c"),
			XRT_STR_LITERAL("testLogBasic"),
			10u,
			77u
		) == XLOG_RESULT_SKIPPED,
		"Sink threshold did not filter DEBUG"
	);
	testRequire(
		xrtLogSource(
			pLogger,
			XLOG_WARN,
			XRT_STR_LITERAL("structured"),
			Fields,
			4u,
			XRT_STR_LITERAL("logger.c"),
			XRT_STR_LITERAL("testLogBasic"),
			42u,
			77u
		) == XLOG_RESULT_WRITTEN,
		"structured log was not written"
	);
	testRequire(
		(State.Calls == 1u) && (State.Fields == 4u) &&
		(State.Level == XLOG_WARN) && (State.ThreadId == 77u) &&
		(State.Line == 42u) &&
		(strcmp(State.LoggerName, "service") == 0) &&
		(strcmp(State.Message, "structured") == 0),
		"structured record metadata changed"
	);

	memset(&Record, 0, sizeof(Record));
	Record.Time = 1;
	Record.Level = XLOG_INFO;
	Record.Logger = XRT_STR_LITERAL("spoofed");
	Record.Message = XRT_STR_LITERAL("direct");
	testRequire(
		xrtLogSubmit(pLogger, &Record) == XLOG_RESULT_WRITTEN &&
		(strcmp(State.LoggerName, "service") == 0),
		"Logger did not replace caller-supplied logger name"
	);
	testRequire(
		xrtLogFlush(pLogger) && (State.Flushes == 1u),
		"Logger flush did not reach Sink"
	);
	testRequire(
		xrtLogStats(pLogger, &LoggerStats) &&
		xrtLogSinkStats(pSink, &SinkStats) &&
		(LoggerStats.Submitted == 3u) &&
		(LoggerStats.Written == 2u) &&
		(LoggerStats.Skipped == 1u) &&
		(SinkStats.Submitted == 3u) &&
		(SinkStats.Written == 2u) &&
		(SinkStats.Skipped == 1u),
		"Logger or Sink statistics mismatch"
	);
	testRequire(
		xrtLogDetach(pLogger, pSink) &&
		!xrtLogDetach(pLogger, pSink) &&
		(xrtLogSinkCount(pLogger) == 0u),
		"Sink detach contract failed"
	);
	xrtLogFree(pLogger);
	testRequire(State.Drops == 0u, "external Sink reference was lost");
	xrtLogSinkFree(pSink);
	testRequire(State.Drops == 1u, "Sink Drop did not run exactly once");
}



/* 验证回调可递归提交，也可在自身回调内安全移除。 */
static void testLogReentryAndDetach(void)
{
	testlogsinkstate Recursive;
	testlogsinkstate SelfDetach;
	xlogger* pLogger;
	xlogsink* pRecursive;
	xlogsink* pSelfDetach;

	memset(&Recursive, 0, sizeof(Recursive));
	memset(&SelfDetach, 0, sizeof(SelfDetach));
	pLogger = xrtLogCreate(XRT_STR_LITERAL("reentry"), XLOG_TRACE);
	pRecursive = testLogSink(
		&Recursive,
		XRT_STR_LITERAL("recursive"),
		XLOG_TRACE
	);
	pSelfDetach = testLogSink(
		&SelfDetach,
		XRT_STR_LITERAL("self-detach"),
		XLOG_TRACE
	);
	testRequire(
		(pLogger != NULL) && (pRecursive != NULL) &&
		(pSelfDetach != NULL),
		"reentry fixture creation failed"
	);
	Recursive.Logger = pLogger;
	Recursive.Nested = false;
	SelfDetach.Logger = pLogger;
	SelfDetach.Sink = pSelfDetach;
	SelfDetach.Detach = true;
	testRequire(
		xrtLogAttach(pLogger, pRecursive) &&
		xrtLogAttach(pLogger, pSelfDetach),
		"reentry fixture attach failed"
	);
	testRequire(
		xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("outer")) ==
			XLOG_RESULT_WRITTEN,
		"outer recursive record failed"
	);
	testRequire(
		(Recursive.Calls == 2u) && (SelfDetach.Calls == 2u) &&
		(xrtLogSinkCount(pLogger) == 1u),
		"recursive or self-detach callback contract failed"
	);
	testRequire(
		xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("after detach")) ==
			XLOG_RESULT_WRITTEN &&
		(Recursive.Calls == 3u) && (SelfDetach.Calls == 2u),
		"detached Sink received a newly submitted record"
	);
	xrtLogFree(pLogger);
	xrtLogSinkFree(pRecursive);
	xrtLogSinkFree(pSelfDetach);
	testRequire(
		(Recursive.Drops == 1u) && (SelfDetach.Drops == 1u),
		"reentry Sink lifetime failed"
	);
}



/* 验证共享 Sink、默认 Logger 和引用所有权可以独立组合。 */
static void testLogSharingAndDefault(void)
{
	testlogsinkstate State;
	xlogger* pFirst;
	xlogger* pSecond;
	xlogger* pDefault;
	xlogsink* pSink;

	memset(&State, 0, sizeof(State));
	pFirst = xrtLogCreate(XRT_STR_LITERAL("first"), XLOG_TRACE);
	pSecond = xrtLogCreate(XRT_STR_LITERAL("second"), XLOG_TRACE);
	pSink = testLogSink(&State, XRT_STR_LITERAL("shared"), XLOG_TRACE);
	testRequire(
		(pFirst != NULL) && (pSecond != NULL) && (pSink != NULL),
		"shared Sink fixture creation failed"
	);
	testRequire(
		xrtLogAttach(pFirst, pSink) && xrtLogAttach(pSecond, pSink),
		"one Sink could not be shared by two Loggers"
	);
	testRequire(
		!xrtLogAttach(pFirst, pSink) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS),
		"duplicate Sink attach did not report exists"
	);
	xrtClearError();
	testRequire(xrtLogSetDefault(pFirst), "default Logger setup failed");
	xrtLogFree(pFirst);
	pFirst = NULL;
	pDefault = xrtLogDefault();
	testRequire(pDefault != NULL, "default Logger did not retain its object");
	testRequire(
		xrtLog(pDefault, XLOG_INFO, XRT_STR_LITERAL("default")) ==
			XLOG_RESULT_WRITTEN,
		"default Logger reference was unusable"
	);
	xrtLogFree(pDefault);
	testRequire(xrtLogSetDefault(NULL), "default Logger clear failed");
	testRequire(xrtLogDefault() == NULL, "default Logger remained installed");
	testRequire(
		xrtLog(pSecond, XLOG_INFO, XRT_STR_LITERAL("shared")) ==
			XLOG_RESULT_WRITTEN &&
		(State.Calls == 2u),
		"shared Sink stopped after first Logger release"
	);
	xrtLogFree(pSecond);
	testRequire(State.Drops == 0u, "shared Sink external reference was lost");
	xrtLogSinkFree(pSink);
	testRequire(State.Drops == 1u, "shared Sink Drop count mismatch");
}



/* 验证成功路径保存旧错误，失败路径建立稳定日志错误。 */
static void testLogErrorsAndResults(void)
{
	testlogsinkstate Written;
	testlogsinkstate Dropped;
	testlogsinkstate Failed;
	xlogger* pLogger;
	xlogsink* pWritten;
	xlogsink* pDropped;
	xlogsink* pFailed;
	xerror* pPrevious;

	memset(&Written, 0, sizeof(Written));
	memset(&Dropped, 0, sizeof(Dropped));
	memset(&Failed, 0, sizeof(Failed));
	pLogger = xrtLogCreate(XRT_STR_LITERAL("results"), XLOG_TRACE);
	pWritten = testLogSink(&Written, XRT_STR_LITERAL("written"), XLOG_TRACE);
	pDropped = testLogSink(&Dropped, XRT_STR_LITERAL("dropped"), XLOG_TRACE);
	pFailed = testLogSink(&Failed, XRT_STR_LITERAL("failed"), XLOG_TRACE);
	testRequire(
		(pLogger != NULL) && (pWritten != NULL) &&
		(pDropped != NULL) && (pFailed != NULL),
		"result fixture creation failed"
	);
	Dropped.Result = XLOG_RESULT_DROPPED;
	Failed.Result = XLOG_RESULT_ERROR;
	pPrevious = xrtErrorCreate(
		XERR_VALUE,
		"test.previous",
		7,
		"preserved"
	);
	testRequire(pPrevious != NULL, "previous error creation failed");
	xrtSetError(pPrevious);
	testRequire(
		xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("no sink")) ==
			XLOG_RESULT_SKIPPED &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.previous") == 0),
		"empty Logger changed the caller error"
	);
	testRequire(
		xrtLogAttach(pLogger, pWritten) && xrtLogAttach(pLogger, pDropped),
		"result Sink attach failed"
	);
	testRequire(
		xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("merged")) ==
			XLOG_RESULT_WRITTEN &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.previous") == 0),
		"successful result merge or error isolation failed"
	);
	testRequire(
		xrtLogAttach(pLogger, pFailed),
		"failing Sink attach failed"
	);
	testRequire(
		xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("failure")) ==
			XLOG_RESULT_ERROR &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.log") == 0) &&
		(Written.Calls == 2u) && (Dropped.Calls == 2u) &&
		(Failed.Calls == 1u),
		"Sink failure did not preserve first error or continue dispatch"
	);
	xrtClearError();
	xrtErrorFree(pPrevious);
	xrtLogFree(pLogger);
	xrtLogSinkFree(pWritten);
	xrtLogSinkFree(pDropped);
	xrtLogSinkFree(pFailed);
}



/* 验证非法记录在回调前被拒绝且不污染输出统计。 */
static void testLogInvalid(void)
{
	testlogsinkstate State;
	xlogsink* pSink;
	xlogrecord Record;
	xlogfield Field;

	memset(&State, 0, sizeof(State));
	pSink = testLogSink(&State, XRT_STR_LITERAL("invalid"), XLOG_TRACE);
	testRequire(pSink != NULL, "invalid-record fixture creation failed");
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("invalid");
	Field = xrtLogFieldString(
		XRT_STR_LITERAL("bad"),
		(xstrview){ NULL, 1u }
	);
	Record.Fields = &Field;
	Record.FieldCount = 1u;
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(State.Calls == 0u),
		"invalid record reached Sink callback"
	);
	xrtClearError();
	testRequire(
		(strcmp(xrtLogLevelName(XLOG_FATAL), "FATAL") == 0) &&
		(strcmp(xrtLogLevelName((xloglevel)99), "") == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"log level name validation failed"
	);
	xrtClearError();
	xrtLogSinkFree(pSink);
}



/* 执行 Logger 同步核心完整契约回归。 */
int main(void)
{
	testLogBasic();
	testLogReentryAndDetach();
	testLogSharingAndDefault();
	testLogErrorsAndResults();
	testLogInvalid();
	printf("[PASS] Logger core\n");
	return 0;
}
