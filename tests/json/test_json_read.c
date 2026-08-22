#include "../test.h"



/* 从任意字节范围创建 JSON 测试文本视图。 */
static xstrview testJsonText(const void* pData, size_t iSize)
{
	return (xstrview){ (cstr)pData, iSize };
}



/* 要求当前错误属于指定 JSON 代码。 */
static void testJsonError(xjsonerror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.json") == 0),
		sMessage
	);
	testRequire(xrtErrorCode(pError) == (int32)Code, sMessage);
}



/* 验证 null、对象、数组、转义 Unicode 和嵌入零的 DOM 合同。 */
static void testJsonDom(void)
{
	static const char arrJson[] = {
		'{', '"', 'n', 'a', 'm', 'e', '"', ':', '"',
		'A', '\\', 'u', '0', '0', 'D', 'F',
		'\\', 'u', '6', '7', '7', '1',
		'\\', 'u', 'D', '8', '3', '4',
		'\\', 'u', 'D', 'D', '1', 'E', '"', ',',
		'"', 'z', 'e', 'r', 'o', '"', ':', '"',
		'a', '\\', 'u', '0', '0', '0', '0', 'b', '"', ',',
		'"', 'i', 't', 'e', 'm', 's', '"', ':', '[',
		'1', ',', 't', 'r', 'u', 'e', ',', 'n', 'u', 'l', 'l',
		']', '}'
	};
	static const unsigned char arrName[] = {
		'A', 0xC3, 0x9F, 0xE6, 0x9D, 0xB1, 0xF0, 0x9D, 0x84, 0x9E
	};
	xvalue* pNull;
	xvalue* pRoot;
	xvalue* pName;
	xvalue* pZero;
	xvalue* pItems;
	xstrview Text;
	int64 iValue;

	pNull = xrtJsonParse(XRT_STR_LITERAL("null"));
	testRequire(pNull == xrtValueNull(), "JSON null is ambiguous with failure");
	xrtValueRelease(pNull);

	pRoot = xrtJsonParse(testJsonText(arrJson, sizeof(arrJson)));
	testRequire(pRoot != NULL, "JSON object parse failed");
	testRequire(xrtValueType(pRoot) == XVALUE_OBJECT, "JSON root type mismatch");
	pName = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("name"));
	pZero = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("zero"));
	pItems = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("items"));
	testRequire(
		xrtValueGetString(pName, &Text) &&
		(Text.Size == sizeof(arrName)) &&
		(memcmp(Text.Data, arrName, sizeof(arrName)) == 0),
		"JSON Unicode escape result mismatch"
	);
	testRequire(
		xrtValueGetString(pZero, &Text) &&
		(Text.Size == 3u) &&
		(Text.Data[0] == 'a') && (Text.Data[1] == '\0') &&
		(Text.Data[2] == 'b'),
		"JSON embedded zero string mismatch"
	);
	testRequire(
		(xrtValueCount(pItems) == 3u) &&
		xrtValueGetInt(xrtValueArrayGet(pItems, 0), &iValue) &&
		(iValue == 1) &&
		(xrtValueArrayGet(pItems, 2) == xrtValueNull()),
		"JSON array values mismatch"
	);
	xrtValueRelease(pRoot);
}



/* 验证解析只读取明确长度，不依赖末尾零字节。 */
static void testJsonExplicitLength(void)
{
	const char arrJson[] = { '[', '1', ',', '2', ']' };
	xvalue* pRoot = xrtJsonParse(testJsonText(arrJson, sizeof(arrJson)));
	int64 iValue;

	testRequire(pRoot != NULL, "non-terminated JSON input failed");
	testRequire(
		(xrtValueCount(pRoot) == 2u) &&
		xrtValueGetInt(xrtValueArrayGet(pRoot, 1), &iValue) &&
		(iValue == 2),
		"non-terminated JSON content mismatch"
	);
	xrtValueRelease(pRoot);
}



/* 验证严格默认拒绝旧版默认接受的非标准语法和无效 Unicode。 */
static void testJsonStrictSyntax(void)
{
	static const char arrControl[] = { '"', 'a', '\n', 'b', '"' };
	static const unsigned char arrUtf8[] = { '"', 0xC0, 0xAF, '"' };
	static const cstr arrInvalid[] = {
		"/*x*/1",
		"[1,]",
		"{'a':1}",
		"01",
		"+1",
		"0x10",
		"NaN",
		"\"\\v\"",
		"\"\\uD800\"",
		"\"\\uDC00\"",
		"true false"
	};

	for ( size_t i = 0; i < (sizeof(arrInvalid) / sizeof(arrInvalid[0])); i++ ) {
		xrtClearError();
		testRequire(
			!xrtJsonValid(testJsonText(arrInvalid[i], strlen(arrInvalid[i]))),
			"invalid strict JSON was accepted"
		);
		testRequire(xrtGetError() != NULL, "invalid strict JSON set no error");
	}
	testRequire(
		!xrtJsonValid(testJsonText(arrControl, sizeof(arrControl))),
		"raw JSON control byte was accepted"
	);
	testRequire(
		!xrtJsonValid(testJsonText(arrUtf8, sizeof(arrUtf8))),
		"invalid JSON UTF-8 was accepted"
	);
}



/* 验证注释和尾随逗号只能通过运行时配置显式开启。 */
static void testJsonExtensions(void)
{
	xjsonreadconfig Config;
	xvalue* pRoot;
	int64 iValue;

	xrtJsonReadConfigInit(&Config);
	Config.Flags = XJSON_READ_COMMENTS | XJSON_READ_TRAILING_COMMA;
	pRoot = xrtJsonRead(
		XRT_STR_LITERAL("/*head*/ {\"v\": 7, // tail\n}"),
		&Config
	);
	testRequire(pRoot != NULL, "explicit JSON extensions failed");
	testRequire(
		xrtValueGetInt(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("v")),
			&iValue
		) && (iValue == 7),
		"extended JSON value mismatch"
	);
	xrtValueRelease(pRoot);
}



/* 验证重复键三种策略及默认拒绝口径。 */
static void testJsonDuplicates(void)
{
	xjsonreadconfig Config;
	xvalue* pRoot;
	int64 iValue;

	xrtJsonReadConfigInit(&Config);
	xrtClearError();
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("{\"a\":1,\"a\":2}"), &Config) == NULL,
		"duplicate JSON name should fail by default"
	);
	testJsonError(XJSON_ERROR_DUPLICATE, "duplicate JSON error mismatch");

	Config.Duplicate = XJSON_DUPLICATE_KEEP;
	pRoot = xrtJsonRead(XRT_STR_LITERAL("{\"a\":1,\"a\":{\"x\":2}}"), &Config);
	testRequire(pRoot != NULL, "duplicate keep parse failed");
	testRequire(
		xrtValueGetInt(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("a")),
			&iValue
		) && (iValue == 1),
		"duplicate keep policy mismatch"
	);
	xrtValueRelease(pRoot);

	Config.Duplicate = XJSON_DUPLICATE_REPLACE;
	pRoot = xrtJsonRead(XRT_STR_LITERAL("{\"a\":1,\"a\":2}"), &Config);
	testRequire(pRoot != NULL, "duplicate replace parse failed");
	testRequire(
		xrtValueGetInt(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("a")),
			&iValue
		) && (iValue == 2),
		"duplicate replace policy mismatch"
	);
	xrtValueRelease(pRoot);
}



/* 验证 int64 边界和显式有损大整数策略。 */
static void testJsonNumbers(void)
{
	xjsonreadconfig Config;
	xjsonlocation Location;
	xvalue* pValue;
	int64 iInteger;
	double fValue;

	pValue = xrtJsonParse(XRT_STR_LITERAL("-9223372036854775808"));
	testRequire(
		(pValue != NULL) && xrtValueGetInt(pValue, &iInteger) &&
		(iInteger == INT64_MIN),
		"JSON int64 minimum failed"
	);
	xrtValueRelease(pValue);

	xrtJsonReadConfigInit(&Config);
	xrtClearError();
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("9223372036854775808"), &Config) == NULL,
		"oversized JSON integer should fail"
	);
	testJsonError(XJSON_ERROR_NUMBER, "oversized JSON integer error mismatch");
	testRequire(
		xrtJsonErrorLocation(xrtGetError(), &Location) &&
		(Location.Offset == 0u) && (Location.Line == 1u) &&
		(Location.Column == 1u),
		"oversized JSON integer location should point to token start"
	);

	Config.BigInteger = XJSON_BIGINT_FLOAT;
	pValue = xrtJsonRead(XRT_STR_LITERAL("9223372036854775808"), &Config);
	testRequire(
		(pValue != NULL) && xrtValueGetFloat(pValue, &fValue) &&
		(fValue > 9.22e18),
		"explicit JSON big integer float policy failed"
	);
	xrtValueRelease(pValue);

	xrtClearError();
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("1e9999"), &Config) == NULL,
		"overflowing JSON float should fail"
	);
	testJsonError(XJSON_ERROR_NUMBER, "overflowing JSON float error mismatch");
}



/* 验证所有读取预算都在跨越边界前失败。 */
static void testJsonLimits(void)
{
	xjsonreadconfig Config;
	xvalue* pValue;

	xrtJsonReadConfigInit(&Config);
	Config.MaxInputBytes = 2u;
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("null"), &Config) == NULL,
		"JSON input limit was not enforced"
	);

	xrtJsonReadConfigInit(&Config);
	Config.MaxStringBytes = 2u;
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("\"abc\""), &Config) == NULL,
		"JSON string limit was not enforced"
	);

	xrtJsonReadConfigInit(&Config);
	Config.MaxValues = 2u;
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("[1,2]"), &Config) == NULL,
		"JSON value count limit was not enforced"
	);

	xrtJsonReadConfigInit(&Config);
	Config.MaxContainerItems = 1u;
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("[1,2]"), &Config) == NULL,
		"JSON container item limit was not enforced"
	);

	xrtJsonReadConfigInit(&Config);
	Config.MaxDepth = 1u;
	pValue = xrtJsonRead(XRT_STR_LITERAL("[]"), &Config);
	testRequire(pValue != NULL, "JSON root depth budget rejected root container");
	xrtValueRelease(pValue);
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("[[]]"), &Config) == NULL,
		"JSON nesting depth limit was not enforced"
	);
}



/* 验证读取配置拒绝零初始化、未知标志和非零保留字段。 */
static void testJsonReadConfig(void)
{
	xjsonreadconfig Config;

	memset(&Config, 0, sizeof(Config));
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("null"), &Config) == NULL,
		"zeroed JSON read config should fail"
	);
	testJsonError(XJSON_ERROR_CONFIG, "zeroed JSON read config error mismatch");

	xrtJsonReadConfigInit(&Config);
	Config.Flags = UINT32_C(0x80000000);
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("null"), &Config) == NULL,
		"unknown JSON read flag should fail"
	);
	testJsonError(XJSON_ERROR_CONFIG, "unknown JSON read flag error mismatch");

	xrtJsonReadConfigInit(&Config);
	Config.Reserved[0] = 1u;
	testRequire(
		xrtJsonRead(XRT_STR_LITERAL("null"), &Config) == NULL,
		"reserved JSON read field should fail"
	);
	testJsonError(XJSON_ERROR_CONFIG, "reserved JSON read field error mismatch");
}



/* 访问器统计直接解析事件并在指定数字处正常停止。 */
typedef struct testjsonvisitstate {
	size_t Events;
	size_t Names;
	size_t LastDepth;
	xjsoneventtype Types[8];
	bool StopAtTwo;
	bool Fail;
	bool SetError;
} testjsonvisitstate;



/* 记录事件、名称和数字原始 token。 */
static xjsonvisitaction testJsonVisitor(
	const xjsonevent* pEvent,
	ptr pUserData
)
{
	testjsonvisitstate* pState = (testjsonvisitstate*)pUserData;

	if ( pState->Events < (sizeof(pState->Types) / sizeof(pState->Types[0])) ) {
		pState->Types[pState->Events] = pEvent->Type;
	}
	pState->Events++;
	pState->LastDepth = pEvent->Depth;
	if ( pEvent->HasName ) {
		pState->Names++;
	}
	if (
		pState->StopAtTwo &&
		(pEvent->Type == XJSON_EVENT_INT) &&
		(pEvent->Value.Integer == 2)
	) {
		testRequire(
			(pEvent->Raw.Size == 1u) && (pEvent->Raw.Data[0] == '2'),
			"JSON visitor raw number mismatch"
		);
		return XJSON_VISIT_STOP;
	}
	if ( pState->Fail ) {
		if ( pState->SetError ) {
			xerror* pError = xrtErrorCreate(
				XERR_VALUE,
				"test.json.visitor",
				77,
				"visitor failure"
			);

			testRequire(pError != NULL, "JSON visitor test error create failed");
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return XJSON_VISIT_FAIL;
	}
	return XJSON_VISIT_NEXT;
}



/* 验证完整访问、提前停止和错误位置提取。 */
static void testJsonVisit(void)
{
	xjsonreadconfig Config;
	testjsonvisitstate State;
	xjsonlocation Location;
	xerror* pStale;

	xrtJsonReadConfigInit(&Config);
	memset(&State, 0, sizeof(State));
	testRequire(
		xrtJsonVisit(
			XRT_STR_LITERAL("{\"a\":[1,2],\"b\":true}"),
			&Config,
			testJsonVisitor,
			&State
		) == XJSON_VISIT_DONE,
		"JSON visitor did not complete"
	);
	testRequire(
		(State.Events == 7u) && (State.Names == 2u) &&
		(State.LastDepth == 0u),
		"JSON visitor event metadata mismatch"
	);
	testRequire(
		(State.Types[0] == XJSON_EVENT_OBJECT_BEGIN) &&
		(State.Types[1] == XJSON_EVENT_ARRAY_BEGIN) &&
		(State.Types[2] == XJSON_EVENT_INT) &&
		(State.Types[3] == XJSON_EVENT_INT) &&
		(State.Types[4] == XJSON_EVENT_ARRAY_END) &&
		(State.Types[5] == XJSON_EVENT_BOOL) &&
		(State.Types[6] == XJSON_EVENT_OBJECT_END),
		"JSON visitor public event type mapping mismatch"
	);

	memset(&State, 0, sizeof(State));
	State.StopAtTwo = true;
	testRequire(
		xrtJsonVisit(
			XRT_STR_LITERAL("[1,2,3]"),
			&Config,
			testJsonVisitor,
			&State
		) == XJSON_VISIT_STOPPED,
		"JSON visitor early stop mismatch"
	);

	/* 调用前旧错误不能遮蔽 visitor 的通用失败错误。 */
	pStale = xrtErrorCreate(XERR_VALUE, "test.stale", 1, "stale error");
	testRequire(pStale != NULL, "JSON stale error create failed");
	xrtSetError(pStale);
	xrtErrorFree(pStale);
	memset(&State, 0, sizeof(State));
	State.Fail = true;
	testRequire(
		xrtJsonVisit(
			XRT_STR_LITERAL("1"),
			&Config,
			testJsonVisitor,
			&State
		) == XJSON_VISIT_ERROR,
		"JSON visitor failure was ignored"
	);
	testJsonError(XJSON_ERROR_STATE, "JSON visitor generic error mismatch");

	/* visitor 主动建立的新错误必须跨解析层原样传播。 */
	memset(&State, 0, sizeof(State));
	State.Fail = true;
	State.SetError = true;
	testRequire(
		xrtJsonVisit(
			XRT_STR_LITERAL("1"),
			&Config,
			testJsonVisitor,
			&State
		) == XJSON_VISIT_ERROR,
		"JSON visitor specific failure was ignored"
	);
	testRequire(
		strcmp(xrtErrorDomain(xrtGetError()), "test.json.visitor") == 0,
		"JSON visitor specific error was overwritten"
	);

	xrtClearError();
	testRequire(
		xrtJsonParse(XRT_STR_LITERAL("{\"a\":}")) == NULL,
		"invalid JSON for location test was accepted"
	);
	testRequire(
		xrtJsonErrorLocation(xrtGetError(), &Location) &&
		(Location.Offset == 5u) && (Location.Line == 1u) &&
		(Location.Column == 6u),
		"JSON error location mismatch"
	);
}



/* 运行 JSON 读取、验证和直接访问的完整合同测试。 */
int main(void)
{
	testJsonDom();
	testJsonExplicitLength();
	testJsonStrictSyntax();
	testJsonExtensions();
	testJsonDuplicates();
	testJsonNumbers();
	testJsonLimits();
	testJsonReadConfig();
	testJsonVisit();
	printf("[PASS] JSON read\n");
	return 0;
}
