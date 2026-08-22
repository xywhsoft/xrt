#include "../test.h"



/* printf 测试 Sink 保存消息的显式长度和元数据。 */
typedef struct testlogprintfstate {
	uint8 Message[128];
	size_t Size;
	size_t Fields;
	uint32 Line;
	uint64 ThreadId;
} testlogprintfstate;



/* 复制包含零字节的格式化消息。 */
static xlogresult testLogPrintfWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	testlogprintfstate* pState = (testlogprintfstate*)pUserData;

	if ( pRecord->Message.Size > sizeof(pState->Message) ) {
		return XLOG_RESULT_ERROR;
	}
	memcpy(pState->Message, pRecord->Message.Data, pRecord->Message.Size);
	pState->Size = pRecord->Message.Size;
	pState->Fields = pRecord->FieldCount;
	pState->Line = pRecord->Line;
	pState->ThreadId = pRecord->ThreadId;
	return XLOG_RESULT_WRITTEN;
}



/* 通过已有 va_list 验证 V 入口。 */
static xlogresult testLogPrintfV(
	xlogger* pLogger,
	cstr sFormat,
	...
)
{
	va_list Args;
	xlogresult Result;

	va_start(Args, sFormat);
	Result = xrtLogPrintfV(pLogger, XLOG_INFO, sFormat, Args);
	va_end(Args);
	return Result;
}



/* 验证所有 printf 层级入口、嵌入零和安全格式错误。 */
int main(void)
{
	testlogprintfstate State;
	xlogsinkconfig Config;
	xlogfield Field;
	xlogger* pLogger;
	xlogsink* pSink;

	memset(&State, 0, sizeof(State));
	memset(&Config, 0, sizeof(Config));
	Config.Name = XRT_STR_LITERAL("printf");
	Config.Level = XLOG_TRACE;
	Config.Write = testLogPrintfWrite;
	Config.UserData = &State;
	pLogger = xrtLogCreate(XRT_STR_LITERAL("printf"), XLOG_TRACE);
	pSink = xrtLogSinkCreate(&Config);
	testRequire(
		(pLogger != NULL) && (pSink != NULL) &&
		xrtLogAttach(pLogger, pSink),
		"Logger printf fixture creation failed"
	);
	testRequire(
		xrtLogPrintf(pLogger, XLOG_INFO, "value=%d", 42) ==
			XLOG_RESULT_WRITTEN &&
		(State.Size == 8u) &&
		(memcmp(State.Message, "value=42", 8u) == 0),
		"Logger printf output mismatch"
	);
	testRequire(
		testLogPrintfV(pLogger, "hex=%02X", 15) == XLOG_RESULT_WRITTEN &&
		(State.Size == 6u) &&
		(memcmp(State.Message, "hex=0F", 6u) == 0),
		"Logger printf V output mismatch"
	);
	Field = xrtLogFieldInt(XRT_STR_LITERAL("code"), 7);
	testRequire(
		xrtLogFieldsPrintf(
			pLogger,
			XLOG_WARN,
			&Field,
			1u,
			"field=%s",
			"ok"
		) == XLOG_RESULT_WRITTEN &&
		(State.Fields == 1u),
		"Logger fields printf metadata mismatch"
	);
	testRequire(
		xrtLogSourcePrintf(
			pLogger,
			XLOG_ERROR,
			NULL,
			0,
			XRT_STR_LITERAL("source.c"),
			XRT_STR_LITERAL("main"),
			91u,
			123u,
			"binary:%c:end",
			0
		) == XLOG_RESULT_WRITTEN &&
		(State.Size == 12u) && (State.Message[7] == 0) &&
		(State.Line == 91u) && (State.ThreadId == 123u),
		"Logger source printf lost binary length or metadata"
	);
	xrtClearError();
	testRequire(
		xrtLogPrintf(pLogger, XLOG_INFO, "bad%n", &State.Line) ==
			XLOG_RESULT_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"Logger printf accepted unsafe percent-n"
	);
	xrtClearError();
	xrtLogSinkFree(pSink);
	xrtLogFree(pLogger);
	printf("[PASS] Logger printf\n");
	return 0;
}
