#include "../test.h"

typedef struct testScanExpected {
	xjsonscantype Type;
	size_t Depth, Index;
	xstrview Name, Raw;
} testScanExpected;

typedef struct testScanContext {
	xstrview Input;
	const testScanExpected* Expected;
	size_t Count, Seen, StopAt;
	xjsonvisitaction Action;
	bool SetError;
	xstrview Saved;
} testScanContext;

static bool testScanEqual(xstrview Left, xstrview Right)
{
	return Left.Size == Right.Size && (Left.Size == 0 || memcmp(Left.Data, Right.Data, Left.Size) == 0);
}

static void testScanSpan(xstrview Input, xstrview Part)
{
	testRequire(Part.Data != NULL && (uintptr_t)Part.Data >= (uintptr_t)Input.Data &&
		(uintptr_t)Part.Data - (uintptr_t)Input.Data <= Input.Size &&
		Part.Size <= Input.Size - ((uintptr_t)Part.Data - (uintptr_t)Input.Data), "Scan view escaped input");
}

static xjsonvisitaction testScanVisit(const xjsonscanevent* Event, ptr User)
{
	testScanContext* Context = (testScanContext*)User;
	testScanSpan(Context->Input, Event->Raw);
	if (Event->HasName) testScanSpan(Context->Input, Event->Name);
	else testRequire(Event->Name.Data == NULL && Event->Name.Size == 0, "Scan empty parent name");
	testRequire(Event->Type >= XJSON_SCAN_NULL && Event->Type <= XJSON_SCAN_OBJECT_END, "invalid lexical Scan type");
	if (Context->Expected != NULL) {
		testRequire(Context->Seen < Context->Count, "extra Scan event");
		const testScanExpected* Want = &Context->Expected[Context->Seen];
		testRequire(Event->Type == Want->Type && Event->Depth == Want->Depth && Event->Index == Want->Index &&
			Event->HasName == (Want->Name.Size != 0) && testScanEqual(Event->Name, Want->Name) &&
			testScanEqual(Event->Raw, Want->Raw), "Scan event contract mismatch");
	}
	bool End = Event->Type == XJSON_SCAN_ARRAY_END || Event->Type == XJSON_SCAN_OBJECT_END;
	size_t Offset = (size_t)(Event->Raw.Data - Context->Input.Data) + (End ? Event->Raw.Size - 1u : 0u);
	size_t Line = 1, Column = 1;
	for (size_t Byte = 0; Byte < Offset; ++Byte) {
		if (Context->Input.Data[Byte] == '\n') { ++Line; Column = 1; } else ++Column;
	}
	testRequire(Event->Location.Offset == Offset && Event->Location.Line == Line && Event->Location.Column == Column,
		"Scan location mismatch");
	Context->Saved = Event->Raw; ++Context->Seen;
	if (Context->Seen == Context->StopAt) {
		if (Context->SetError) xrtSetErrorKind(XERR_CANCELLED);
		return Context->Action;
	}
	return XJSON_VISIT_NEXT;
}

#define SCAN_EXPECT(Type, Depth, Index, Name, Raw) {Type, Depth, Index, XRT_STR_INIT(Name), XRT_STR_INIT(Raw)}
#define SCAN_PARAMS "[true,false,null,-0,1e9999,\"a\\u0000b\",{},[]]"
#define SCAN_ROOT "{\"me\\u0074hod\":\"ec\\u0068o\",\"id\":1234567890123456789012345678901234567890,\"params\":" SCAN_PARAMS "}"

static void testScanKnown(void)
{
	static const testScanExpected Expected[] = {
		SCAN_EXPECT(XJSON_SCAN_OBJECT_BEGIN, 0, 0, "", "{"),
		SCAN_EXPECT(XJSON_SCAN_STRING, 1, 0, "\"me\\u0074hod\"", "\"ec\\u0068o\""),
		SCAN_EXPECT(XJSON_SCAN_NUMBER, 1, 0, "\"id\"", "1234567890123456789012345678901234567890"),
		SCAN_EXPECT(XJSON_SCAN_ARRAY_BEGIN, 1, 0, "\"params\"", "["),
		SCAN_EXPECT(XJSON_SCAN_BOOL, 2, 0, "", "true"), SCAN_EXPECT(XJSON_SCAN_BOOL, 2, 1, "", "false"),
		SCAN_EXPECT(XJSON_SCAN_NULL, 2, 2, "", "null"), SCAN_EXPECT(XJSON_SCAN_NUMBER, 2, 3, "", "-0"),
		SCAN_EXPECT(XJSON_SCAN_NUMBER, 2, 4, "", "1e9999"), SCAN_EXPECT(XJSON_SCAN_STRING, 2, 5, "", "\"a\\u0000b\""),
		SCAN_EXPECT(XJSON_SCAN_OBJECT_BEGIN, 2, 6, "", "{"), SCAN_EXPECT(XJSON_SCAN_OBJECT_END, 2, 6, "", "{}"),
		SCAN_EXPECT(XJSON_SCAN_ARRAY_BEGIN, 2, 7, "", "["), SCAN_EXPECT(XJSON_SCAN_ARRAY_END, 2, 7, "", "[]"),
		SCAN_EXPECT(XJSON_SCAN_ARRAY_END, 1, 0, "\"params\"", SCAN_PARAMS), SCAN_EXPECT(XJSON_SCAN_OBJECT_END, 0, 0, "", SCAN_ROOT)
	};
	xstrview Input = XRT_STR_LITERAL(" \n" SCAN_ROOT "\t"); xjsonreadconfig Config; xrtJsonReadConfigInit(&Config);
	testScanContext Context = {0}; Context.Input = Input; Context.Expected = Expected;
	Context.Count = sizeof(Expected) / sizeof(Expected[0]);
	testRequire(xrtJsonScan(Input, &Config, testScanVisit, &Context) == XJSON_VISIT_DONE && Context.Seen == Context.Count,
		"lossless Scan fixture");
	testRequire(testScanEqual(Context.Saved, XRT_STR_LITERAL(SCAN_ROOT)), "Scan views did not survive callback/return");
	/* Duplicate members are emitted; application protocol fields decide how to
	 * handle them. Native BigInteger policy does not narrow Scan numbers. */
	Context = (testScanContext){0}; Context.Input = XRT_STR_LITERAL("{\"id\":1,\"\\u0069d\":1e99999}");
	testRequire(xrtJsonScan(Context.Input, &Config, testScanVisit, &Context) == XJSON_VISIT_DONE && Context.Seen == 4,
		"raw duplicate member or numeric preservation");
}

static xjsonvisitresult testScanRun(xstrview Input, xjsonreadconfig* Config, testScanContext* Context)
{
	memset(Context, 0, sizeof(*Context)); Context->Input = Input;
	return xrtJsonScan(Input, Config, testScanVisit, Context);
}

static void testScanInvalidAndLimits(void)
{
	static const cstr Invalid[] = {"01", "+1", "--1", "1e", "1.e2", "NaN", "Infinity", "0x10", "1 2", "[1,]",
		"{\"x\":1,}", "{\"x\" 1}", "\"\\uD800\"", "\"\\uDC00\"", "\"\\z\"", "\"\xff\"", "[", "{", "\"", "\"a\nb\""};
	xjsonreadconfig Config; xrtJsonReadConfigInit(&Config); testScanContext Context;
	for (size_t Index = 0; Index < sizeof(Invalid)/sizeof(Invalid[0]); ++Index) {
		testRequire(testScanRun((xstrview){Invalid[Index], strlen(Invalid[Index])}, &Config, &Context) == XJSON_VISIT_ERROR,
			"Scan accepted invalid JSON grammar"); xrtClearError();
	}
	Config.MaxStringBytes = 2;
	testRequire(testScanRun(XRT_STR_LITERAL("\"\\u0061\\u0062\""), &Config, &Context) == XJSON_VISIT_DONE, "decoded string limit boundary");
	Config.MaxStringBytes = 1;
	testRequire(testScanRun(XRT_STR_LITERAL("\"\\u0061\\u0062\""), &Config, &Context) == XJSON_VISIT_ERROR, "decoded string limit");
	testRequire(xrtErrorCode(xrtGetError()) == XJSON_ERROR_LIMIT, "Scan string limit code");
	Config.MaxStringBytes = 4;
	testRequire(testScanRun(XRT_STR_LITERAL("\"\\uD834\\uDD1E\""), &Config, &Context) == XJSON_VISIT_DONE, "surrogate decoded limit");
	Config.MaxStringBytes = 3;
	testRequire(testScanRun(XRT_STR_LITERAL("\"\\uD834\\uDD1E\""), &Config, &Context) == XJSON_VISIT_ERROR, "surrogate limit enforcement");
	xrtJsonReadConfigInit(&Config); Config.MaxDepth = 1;
	testRequire(testScanRun(XRT_STR_LITERAL("[[]]"), &Config, &Context) == XJSON_VISIT_ERROR, "Scan depth limit");
	xrtJsonReadConfigInit(&Config); Config.MaxValues = 3;
	testRequire(testScanRun(XRT_STR_LITERAL("[0,0]"), &Config, &Context) == XJSON_VISIT_DONE, "Scan value limit boundary");
	Config.MaxValues = 2;
	testRequire(testScanRun(XRT_STR_LITERAL("[0,0]"), &Config, &Context) == XJSON_VISIT_ERROR, "Scan value limit");
	xrtJsonReadConfigInit(&Config); Config.MaxContainerItems = 1;
	testRequire(testScanRun(XRT_STR_LITERAL("[0,0]"), &Config, &Context) == XJSON_VISIT_ERROR, "Scan array item limit");
	testRequire(testScanRun(XRT_STR_LITERAL("{\"a\":0,\"b\":0}"), &Config, &Context) == XJSON_VISIT_ERROR, "Scan object item limit");
	xrtJsonReadConfigInit(&Config); Config.MaxInputBytes = 2;
	testRequire(testScanRun(XRT_STR_LITERAL("null"), &Config, &Context) == XJSON_VISIT_ERROR && Context.Seen == 0, "Scan input limit preflight");
	xrtJsonReadConfigInit(&Config); Config.Flags = XJSON_READ_COMMENTS | XJSON_READ_TRAILING_COMMA;
	testRequire(testScanRun(XRT_STR_LITERAL("/*c*/[1,]"), &Config, &Context) == XJSON_VISIT_DONE, "Scan explicit extensions");
	Config.Flags = UINT32_MAX;
	testRequire(testScanRun(XRT_STR_LITERAL("0"), &Config, &Context) == XJSON_VISIT_ERROR && Context.Seen == 0, "Scan invalid config");
	xrtJsonReadConfigInit(&Config);
	testRequire(xrtJsonScan(XRT_STR_LITERAL("0"), &Config, NULL, NULL) == XJSON_VISIT_ERROR, "Scan NULL visitor");
	testRequire(testScanRun((xstrview){NULL, 1}, &Config, &Context) == XJSON_VISIT_ERROR && Context.Seen == 0, "Scan NULL input");
	testRequire(testScanRun((xstrview){(cstr)(uintptr_t)(UINTPTR_MAX-1u), 4}, &Config, &Context) == XJSON_VISIT_ERROR &&
		Context.Seen == 0, "Scan wrapped input");
	xrtClearError();
}

static void testScanControl(void)
{
	xjsonreadconfig Config; xrtJsonReadConfigInit(&Config);
	testScanContext Context = {0}; Context.Input = XRT_STR_LITERAL("[1,2] invalid trailing text");
	Context.StopAt = 2; Context.Action = XJSON_VISIT_STOP;
	xrtSetErrorKind(XERR_TIMEOUT); const xerror* Previous = xrtGetError();
	testRequire(xrtJsonScan(Context.Input, &Config, testScanVisit, &Context) == XJSON_VISIT_STOPPED && Context.Seen == 2 &&
		xrtGetError() == Previous, "Scan stop/error identity");
	Context.Seen = 0; Context.Action = XJSON_VISIT_FAIL;
	testRequire(xrtJsonScan(Context.Input, &Config, testScanVisit, &Context) == XJSON_VISIT_ERROR &&
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_STATE, "Scan fallback callback error");
	Context.Seen = 0; Context.SetError = true;
	testRequire(xrtJsonScan(Context.Input, &Config, testScanVisit, &Context) == XJSON_VISIT_ERROR &&
		xrtErrorKind(xrtGetError()) == XERR_CANCELLED, "Scan replaced callback error");
	xrtClearError();
}

static void testScanAllocation(void)
{
	xstrview Input = XRT_STR_LITERAL(SCAN_ROOT); xjsonreadconfig Config; xrtJsonReadConfigInit(&Config);
	testScanContext Context;
	xrtSetErrorKind(XERR_TIMEOUT); const xerror* Previous = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index = 0; Index < 10000; ++Index)
		testRequire(testScanRun(Input, &Config, &Context) == XJSON_VISIT_DONE, "hot Scan");
	testRequire(xrtGetError() == Previous, "Scan success changed ambient error");
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);
	testRequire(After.BlockAllocCalls == Before.BlockAllocCalls && After.MallocCalls == Before.MallocCalls &&
		After.CallocCalls == Before.CallocCalls && After.ReallocCalls == Before.ReallocCalls, "Scan allocated");
	xrtMemStatsEnable(false);
	#endif
	xrtClearError();
	xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "Scan quota");
	testRequire(testScanRun(Input, &Config, &Context) == XJSON_VISIT_DONE, "Scan under one-byte quota");
	testRequire(testScanRun(XRT_STR_LITERAL("[1,]"), &Config, &Context) == XJSON_VISIT_ERROR &&
		xrtErrorKind(xrtGetError()) == XERR_MEMORY, "Scan diagnostic OOM fallback");
	testRequire(xrtAllocationBudgetLeave(&Scope), "Scan quota leave");
	xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0 && Info.Rejections != 0, "Scan quota leak");
	xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testScanKnown(); testScanInvalidAndLimits(); testScanControl(); testScanAllocation();
	printf("[PASS] JSON Scan: raw spans/parent identity/numeric text/Unicode/limits/control/zero-heap/OOM\n");
	return 0;
}
