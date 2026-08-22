#include "../test.h"



/* 固定 Writer 不分配内存，并可在指定调用点主动失败。 */
typedef struct testlogtextoutput {
	uint8 Data[2048];
	size_t Size;
	size_t Calls;
	size_t FailAt;
} testlogtextoutput;



/* 把流式分段复制到固定输出。 */
static bool testLogTextWrite(xbytesview Data, ptr pUserData)
{
	testlogtextoutput* pOutput = (testlogtextoutput*)pUserData;

	pOutput->Calls++;
	if ( pOutput->Calls == pOutput->FailAt ) {
		return false;
	}
	if ( Data.Size > (sizeof(pOutput->Data) - pOutput->Size) ) {
		return false;
	}
	memcpy(pOutput->Data + pOutput->Size, Data.Data, Data.Size);
	pOutput->Size += Data.Size;
	return true;
}



/* 比较固定输出和完整预期文本。 */
static void testLogTextEqual(
	const testlogtextoutput* pOutput,
	cstr sExpected,
	cstr sMessage
)
{
	size_t iSize = strlen(sExpected);

	testRequire(
		(pOutput->Size == iSize) &&
		(memcmp(pOutput->Data, sExpected, iSize) == 0),
		sMessage
	);
}



/* 前置声明供字段和值测试复用固定输出查找。 */
static bool testLogTextContains(
	const testlogtextoutput* pOutput,
	const void* pNeedle,
	size_t iNeedleSize
);



/* 验证完整和简单格式具有稳定布局与单行转义。 */
static void testLogTextLayouts(void)
{
	testlogtextoutput Output;
	xlogtextconfig Config;
	xlogfield Fields[2];
	xlogrecord Record;
	size_t iWritten;

	memset(&Output, 0, sizeof(Output));
	memset(&Record, 0, sizeof(Record));
	Fields[0] = xrtLogFieldInt(XRT_STR_LITERAL("code"), -7);
	Fields[1] = xrtLogFieldString(
		XRT_STR_LITERAL("bad key"),
		XRT_STR_LITERAL("x\t\"")
	);
	Record.Time = 0;
	Record.Level = XLOG_INFO;
	Record.Logger = XRT_STR_LITERAL("service");
	Record.Message = XRT_STR_LITERAL("hello\nworld");
	Record.Fields = Fields;
	Record.FieldCount = 2u;
	Record.File = XRT_STR_LITERAL("a.c");
	Record.Function = XRT_STR_LITERAL("foo");
	Record.Line = 7u;
	Record.ThreadId = 9u;
	testRequire(
		xrtLogTextConfigInit(&Config, XLOG_TEXT_FULL) &&
		xrtLogTextWrite(
			&Record,
			&Config,
			testLogTextWrite,
			&Output,
			&iWritten
		) &&
		(iWritten == Output.Size),
		"full log text formatting failed"
	);
	testLogTextEqual(
		&Output,
		"1970-01-01T00:00:00.000000Z INFO service a.c:7 foo "
		"thread=9 - hello\\nworld code=-7 \"bad key\"=\"x\\t\\\"\"\n",
		"full log text layout changed"
	);

	memset(&Output, 0, sizeof(Output));
	testRequire(
		xrtLogTextConfigInit(&Config, XLOG_TEXT_SIMPLE) &&
		xrtLogTextWrite(
			&Record,
			&Config,
			testLogTextWrite,
			&Output,
			NULL
		),
		"simple log text formatting failed"
	);
	testLogTextEqual(
		&Output,
		"INFO service - hello\\nworld code=-7 \"bad key\"=\"x\\t\\\"\"\n",
		"simple log text layout changed"
	);
}



/* 验证时区、原样消息和全部字段类型。 */
static void testLogTextValues(void)
{
	testlogtextoutput Output;
	xlogtextconfig Config;
	xlogfield Fields[8];
	xlogrecord Record;
	xerror* pError;
	const char arrMessage[] = { 'a', '\n', 'b', 0, 'c' };

	memset(&Output, 0, sizeof(Output));
	memset(&Record, 0, sizeof(Record));
	pError = xrtErrorCreate(XERR_VALUE, "demo", 3, "bad\nvalue");
	testRequire(pError != NULL, "text error field fixture failed");
	Fields[0] = xrtLogFieldNull(XRT_STR_LITERAL("null"));
	Fields[1] = xrtLogFieldBool(XRT_STR_LITERAL("bool"), true);
	Fields[2] = xrtLogFieldInt(XRT_STR_LITERAL("int"), INT64_MIN);
	Fields[3] = xrtLogFieldUInt(XRT_STR_LITERAL("uint"), UINT64_MAX);
	Fields[4] = xrtLogFieldFloat(XRT_STR_LITERAL("float"), 1.5);
	Fields[5] = xrtLogFieldString(XRT_STR_LITERAL("string"), XRT_STR_LITERAL("v"));
	Fields[6] = xrtLogFieldTime(XRT_STR_LITERAL("time"), 0);
	Fields[7] = xrtLogFieldError(XRT_STR_LITERAL("error"), pError);
	Record.Time = 0;
	Record.Level = XLOG_WARN;
	Record.Message = (xstrview){ arrMessage, sizeof(arrMessage) };
	Record.Fields = Fields;
	Record.FieldCount = 8u;
	testRequire(
		xrtLogTextConfigInit(&Config, XLOG_TEXT_MESSAGE),
		"message log text config failed"
	);
	Config.Flags |= XLOG_TEXT_TIME | XLOG_TEXT_RAW_MESSAGE;
	Config.UtcOffset = (5 * 3600) + (30 * 60) + 15;
	testRequire(
		xrtLogTextWrite(
			&Record,
			&Config,
			testLogTextWrite,
			&Output,
			NULL
		),
		"raw log text value formatting failed"
	);
	testRequire(
		(Output.Size > sizeof(arrMessage)) &&
		testLogTextContains(
			&Output,
			"1970-01-01T05:30:15.000000+05:30:15 - ",
			sizeof("1970-01-01T05:30:15.000000+05:30:15 - ") - 1u
		),
		"second-precision UTC offset formatting changed"
	);
	testRequire(
		testLogTextContains(
			&Output,
			arrMessage,
			sizeof(arrMessage)
		),
		"raw message did not preserve control and zero bytes"
	);
	testRequire(
		testLogTextContains(
			&Output,
			"error={kind=3,domain=\"demo\",code=3,message=\"bad\\nvalue\"}",
			sizeof("error={kind=3,domain=\"demo\",code=3,message=\"bad\\nvalue\"}") - 1u
		),
		"error field formatting changed"
	);
	xrtErrorFree(pError);
}



/* 在固定输出中查找字节片段，避免测试依赖非标准 memmem。 */
static bool testLogTextContains(
	const testlogtextoutput* pOutput,
	const void* pNeedle,
	size_t iNeedleSize
)
{
	if ( iNeedleSize > pOutput->Size ) {
		return false;
	}
	for ( size_t i = 0; i <= (pOutput->Size - iNeedleSize); i++ ) {
		if ( memcmp(pOutput->Data + i, pNeedle, iNeedleSize) == 0 ) {
			return true;
		}
	}
	return false;
}



/* 验证 Writer 失败建立稳定错误，并且成功恢复旧错误。 */
static void testLogTextErrors(void)
{
	testlogtextoutput Output;
	xlogtextconfig Config;
	xlogrecord Record;
	xerror* pPrevious;
	size_t iWritten = SIZE_MAX;

	memset(&Output, 0, sizeof(Output));
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("message");
	testRequire(
		xrtLogTextConfigInit(&Config, XLOG_TEXT_MESSAGE),
		"error log text config failed"
	);
	pPrevious = xrtErrorCreate(XERR_VALUE, "previous", 1, "old");
	testRequire(pPrevious != NULL, "previous text error creation failed");
	xrtSetError(pPrevious);
	testRequire(
		xrtLogTextWrite(
			&Record,
			&Config,
			testLogTextWrite,
			&Output,
			&iWritten
		) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "previous") == 0),
		"successful text formatting changed previous error"
	);
	memset(&Output, 0, sizeof(Output));
	Output.FailAt = 1u;
	testRequire(
		!xrtLogTextWrite(
			&Record,
			&Config,
			testLogTextWrite,
			&Output,
			&iWritten
		) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.log") == 0) &&
		(iWritten == 0u),
		"text Writer failure contract mismatch"
	);
	xrtClearError();
	xrtErrorFree(pPrevious);
	Config.UtcOffset = 90000;
	testRequire(
		!xrtLogTextWrite(
			&Record,
			&Config,
			testLogTextWrite,
			&Output,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid text config was accepted"
	);
	xrtClearError();
}



/* 执行流式文本格式器完整回归。 */
int main(void)
{
	testLogTextLayouts();
	testLogTextValues();
	testLogTextErrors();
	printf("[PASS] Logger text format\n");
	return 0;
}
