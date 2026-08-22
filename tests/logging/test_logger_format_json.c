#include "../test.h"

#include <math.h>



/* 固定 Writer 不分配内存，并可在指定调用点主动失败。 */
typedef struct testlogjsonoutput {
	uint8 Data[8192];
	size_t Size;
	size_t Calls;
	size_t FailAt;
} testlogjsonoutput;



/* 把流式分段复制到固定输出。 */
static bool testLogJsonWrite(xbytesview Data, ptr pUserData)
{
	testlogjsonoutput* pOutput = (testlogjsonoutput*)pUserData;

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



/* 比较固定输出和完整预期 JSON。 */
static void testLogJsonEqual(
	const testlogjsonoutput* pOutput,
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



/* 创建包含系统信息和原因链的错误测试值。 */
static xerror* testLogJsonError(void)
{
	xerrordesc Desc;
	xerror* pCause;
	xerror* pError;

	pCause = xrtErrorCreate(XERR_VALUE, "demo", 2, "bad");
	testRequire(pCause != NULL, "JSON error cause fixture failed");
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_IO;
	Desc.Code = 9;
	Desc.SystemCode = 32;
	Desc.Domain = "net";
	Desc.Operation = "read";
	Desc.Message = "failed";
	Desc.Data = "{\"fd\":1}";
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	xrtErrorFree(pCause);
	testRequire(pError != NULL, "JSON error fixture failed");
	return pError;
}



/* 验证默认 JSON Lines 布局和全部字段类型。 */
static void testLogJsonLayout(void)
{
	testlogjsonoutput Output;
	xlogjsonconfig Config;
	xlogfield Fields[8];
	xlogrecord Record;
	xerror* pError;
	size_t iWritten;

	memset(&Output, 0, sizeof(Output));
	memset(&Record, 0, sizeof(Record));
	pError = testLogJsonError();
	Fields[0] = xrtLogFieldNull(XRT_STR_LITERAL("null"));
	Fields[1] = xrtLogFieldBool(XRT_STR_LITERAL("bool"), true);
	Fields[2] = xrtLogFieldInt(XRT_STR_LITERAL("int"), INT64_MIN);
	Fields[3] = xrtLogFieldUInt(XRT_STR_LITERAL("uint"), UINT64_MAX);
	Fields[4] = xrtLogFieldFloat(XRT_STR_LITERAL("float"), 1.5);
	Fields[5] = xrtLogFieldString(
		XRT_STR_LITERAL("string"),
		XRT_STR_LITERAL("x\t")
	);
	Fields[6] = xrtLogFieldTime(XRT_STR_LITERAL("time"), -5);
	Fields[7] = xrtLogFieldError(XRT_STR_LITERAL("error"), pError);
	Record.Time = 123;
	Record.Level = XLOG_WARN;
	Record.Logger = XRT_STR_LITERAL("service");
	Record.Message = XRT_STR_LITERAL("hello\n\"");
	Record.Fields = Fields;
	Record.FieldCount = 8u;
	Record.File = XRT_STR_LITERAL("a.c");
	Record.Function = XRT_STR_LITERAL("foo");
	Record.Line = 7u;
	Record.ThreadId = 9u;
	testRequire(
		xrtLogJsonConfigInit(&Config) &&
		xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			&iWritten
		) &&
		(iWritten == Output.Size),
		"default JSON Lines formatting failed"
	);
	testLogJsonEqual(
		&Output,
		"{\"time\":123,\"level\":\"WARN\",\"logger\":\"service\"," 
		"\"message\":\"hello\\n\\\"\",\"source\":{\"file\":\"a.c\"," 
		"\"function\":\"foo\",\"line\":7},\"thread\":9,\"fields\":{" 
		"\"null\":null,\"bool\":true,\"int\":-9223372036854775808," 
		"\"uint\":18446744073709551615,\"float\":1.5,\"string\":\"x\\t\"," 
		"\"time\":-5,\"error\":{\"kind\":7,\"domain\":\"net\",\"code\":9," 
		"\"system_code\":32,\"operation\":\"read\",\"message\":\"failed\"," 
		"\"data\":\"{\\\"fd\\\":1}\",\"cause\":{\"kind\":3," 
		"\"domain\":\"demo\",\"code\":2,\"message\":\"bad\"}}}}\n",
		"default JSON Lines layout changed"
	);
	xrtErrorFree(pError);
}



/* 验证字段数组无损保留重名和时间类型。 */
static void testLogJsonFieldArray(void)
{
	testlogjsonoutput Output;
	xlogjsonconfig Config;
	xlogfield Fields[2];
	xlogrecord Record;

	memset(&Output, 0, sizeof(Output));
	memset(&Record, 0, sizeof(Record));
	Fields[0] = xrtLogFieldInt(XRT_STR_LITERAL("same"), 1);
	Fields[1] = xrtLogFieldTime(XRT_STR_LITERAL("same"), 2);
	Record.Level = XLOG_INFO;
	Record.Fields = Fields;
	Record.FieldCount = 2u;
	testRequire(
		xrtLogJsonConfigInit(&Config),
		"field array config initialization failed"
	);
	Config.Flags = XLOG_JSON_FIELDS;
	Config.FieldStyle = XLOG_JSON_FIELDS_ARRAY;
	testRequire(
		xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			NULL
		),
		"field array JSON formatting failed"
	);
	testLogJsonEqual(
		&Output,
		"{\"fields\":[{\"name\":\"same\",\"type\":\"int\",\"value\":1},"
		"{\"name\":\"same\",\"type\":\"time\",\"value\":2}]}",
		"lossless field array layout changed"
	);
}



/* 验证非有限浮点策略和 Unicode 转义。 */
static void testLogJsonPolicies(void)
{
	testlogjsonoutput Output;
	xlogjsonconfig Config;
	xlogfield Fields[3];
	xlogrecord Record;
	size_t iWritten = SIZE_MAX;

	memset(&Output, 0, sizeof(Output));
	memset(&Record, 0, sizeof(Record));
	Fields[0] = xrtLogFieldFloat(XRT_STR_LITERAL("nan"), NAN);
	Fields[1] = xrtLogFieldFloat(XRT_STR_LITERAL("pos"), INFINITY);
	Fields[2] = xrtLogFieldFloat(XRT_STR_LITERAL("neg"), -INFINITY);
	Record.Level = XLOG_INFO;
	Record.Fields = Fields;
	Record.FieldCount = 3u;
	testRequire(xrtLogJsonConfigInit(&Config), "policy config failed");
	Config.Flags = XLOG_JSON_FIELDS;
	testRequire(
		!xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			&iWritten
		) &&
		(iWritten == 0) &&
		(Output.Size == 0) &&
		(xrtErrorCode(xrtGetError()) == XLOG_ERROR_JSON_VALUE),
		"non-finite reject policy changed"
	);
	xrtClearError();
	Config.NonFinite = XLOG_JSON_NONFINITE_NULL;
	testRequire(
		xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			NULL
		),
		"non-finite null policy failed"
	);
	testLogJsonEqual(
		&Output,
		"{\"fields\":{\"nan\":null,\"pos\":null,\"neg\":null}}",
		"non-finite null layout changed"
	);
	memset(&Output, 0, sizeof(Output));
	Config.NonFinite = XLOG_JSON_NONFINITE_STRING;
	testRequire(
		xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			NULL
		),
		"non-finite string policy failed"
	);
	testLogJsonEqual(
		&Output,
		"{\"fields\":{\"nan\":\"NaN\",\"pos\":\"Infinity\"," 
		"\"neg\":\"-Infinity\"}}",
		"non-finite string layout changed"
	);

	memset(&Output, 0, sizeof(Output));
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("<&中");
	Config.Flags = XLOG_JSON_MESSAGE;
	Config.EscapeFlags =
		XJSON_WRITE_ESCAPE_HTML |
		XJSON_WRITE_ESCAPE_NON_ASCII;
	testRequire(
		xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			NULL
		),
		"JSON escape policy failed"
	);
	testLogJsonEqual(
		&Output,
		"{\"message\":\"\\u003C\\u0026\\u4E2D\"}",
		"JSON escape policy layout changed"
	);
}



/* 验证错误原因深度、UTF-8、Writer 失败和旧错误恢复。 */
static void testLogJsonErrors(void)
{
	testlogjsonoutput Output;
	xlogjsonconfig Config;
	xlogfield Field;
	xlogrecord Record;
	xerror* pError;
	xerror* pPrevious;
	size_t iWritten = SIZE_MAX;
	const char arrInvalid[] = { (char)0xC3, (char)0x28 };

	memset(&Output, 0, sizeof(Output));
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("message");
	testRequire(xrtLogJsonConfigInit(&Config), "error config failed");
	Config.Flags = XLOG_JSON_MESSAGE;
	pPrevious = xrtErrorCreate(XERR_VALUE, "previous", 1, "old");
	testRequire(pPrevious != NULL, "previous JSON error fixture failed");
	xrtSetError(pPrevious);
	testRequire(
		xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			&iWritten
		) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "previous") == 0),
		"successful JSON formatting changed previous error"
	);
	memset(&Output, 0, sizeof(Output));
	Output.FailAt = 1u;
	testRequire(
		!xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			&iWritten
		) &&
		(iWritten == 0) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.log") == 0) &&
		(xrtErrorCode(xrtGetError()) == XLOG_ERROR_JSON_OUTPUT),
		"JSON Writer failure contract mismatch"
	);
	xrtClearError();
	xrtErrorFree(pPrevious);

	memset(&Output, 0, sizeof(Output));
	Record.Message = (xstrview){ arrInvalid, sizeof(arrInvalid) };
	testRequire(
		!xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			&iWritten
		) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.json") == 0) &&
		(iWritten == Output.Size) &&
		(Output.Size != 0),
		"invalid UTF-8 JSON error contract mismatch"
	);
	xrtClearError();

	memset(&Output, 0, sizeof(Output));
	memset(&Record, 0, sizeof(Record));
	pError = testLogJsonError();
	Field = xrtLogFieldError(XRT_STR_LITERAL("error"), pError);
	Record.Level = XLOG_INFO;
	Record.Fields = &Field;
	Record.FieldCount = 1u;
	Config.Flags = XLOG_JSON_FIELDS;
	Config.MaxErrorDepth = 1u;
	testRequire(
		!xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			&iWritten
		) &&
		(iWritten == 0) &&
		(Output.Size == 0) &&
		(xrtErrorCode(xrtGetError()) == XLOG_ERROR_JSON_DEPTH),
		"error cause depth was not rejected before output"
	);
	xrtClearError();
	xrtErrorFree(pError);

	Field = xrtLogFieldError(XRT_STR_LITERAL("error"), NULL);
	Config.MaxErrorDepth = 0;
	testRequire(
		!xrtLogJsonWrite(
			&Record,
			&Config,
			testLogJsonWrite,
			&Output,
			NULL
		) &&
		(xrtErrorCode(xrtGetError()) == XLOG_ERROR_JSON_CONFIG),
		"invalid JSON config was accepted"
	);
	xrtClearError();
}



/* 执行流式 JSON Lines 格式器完整回归。 */
int main(void)
{
	testLogJsonLayout();
	testLogJsonFieldArray();
	testLogJsonPolicies();
	testLogJsonErrors();
	printf("[PASS] Logger JSON format\n");
	return 0;
}
