#include "../test.h"

#include <math.h>



/* 要求当前错误属于指定 XSON 代码。 */
static void testXsonError(xxsonerror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.xson") == 0),
		sMessage
	);
	testRequire(xrtErrorCode(pError) == (int32)Code, sMessage);
}



/* 验证任意值是期望整数。 */
static void testXsonInt(
	const xvalue* pValue,
	int64 iExpected,
	cstr sMessage
)
{
	int64 iValue;

	testRequire(
		xrtValueGetInt(pValue, &iValue) && (iValue == iExpected),
		sMessage
	);
}



/* 验证全部严格 JSON 仍是 XSON 的无损子集。 */
static void testXsonJsonSubset(void)
{
	xvalue* pRoot = xrtXsonParse(XRT_STR_LITERAL(
		"{\"name\":\"xrt\",\"items\":[1,true,null]}"
	));
	xstrview Name;
	xvalue* pItems;

	testRequire(pRoot != NULL, "XSON JSON subset parse failed");
	testRequire(
		xrtValueGetString(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("name")),
			&Name
		) &&
		(Name.Size == 3u) && (memcmp(Name.Data, "xrt", 3u) == 0),
		"XSON JSON subset string mismatch"
	);
	pItems = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("items"));
	testRequire(
		(xrtValueType(pItems) == XVALUE_ARRAY) &&
		(xrtValueCount(pItems) == 3u),
		"XSON JSON subset array mismatch"
	);
	testXsonInt(xrtValueArrayGet(pItems, 0), 1, "XSON subset integer mismatch");
	xrtValueRelease(pRoot);
}



/* 验证显式容器、二进制、时间和非有限浮点的 DOM 表示。 */
static void testXsonBuiltins(void)
{
	static const uint8 arrBytes[] = { 1u, 2u, 3u, 4u };
	xvalue* pRoot = xrtXsonParse(XRT_STR_LITERAL(
		"{"
		"\"map\":intmap{-5:10,2:\"ok\"},"
		"\"set\":set[1,2,3],"
		"\"blob\":bytes(\"AQIDBA==\"),"
		"\"when\":time(\"2000-01-02T03:04:05.123456+08:00\"),"
		"\"nan\":float(\"nan\"),"
		"\"inf\":float(\"-inf\")"
		"}"
	));
	xvalue* pMap;
	xvalue* pSet;
	xvalue* pBlob;
	xbytesview Bytes;
	xtime Time;
	xtime Expected;
	double fValue;

	testRequire(pRoot != NULL, "XSON builtin parse failed");
	pMap = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("map"));
	pSet = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("set"));
	pBlob = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("blob"));
	testRequire(xrtValueType(pMap) == XVALUE_INT_MAP, "XSON intmap type mismatch");
	testXsonInt(xrtValueIntMapGet(pMap, -5), 10, "XSON intmap value mismatch");
	testRequire(
		(xrtValueType(pSet) == XVALUE_SET) && (xrtValueCount(pSet) == 3u),
		"XSON set mismatch"
	);
	testRequire(
		xrtValueGetBytes(pBlob, &Bytes) &&
		(Bytes.Size == sizeof(arrBytes)) &&
		(memcmp(Bytes.Data, arrBytes, sizeof(arrBytes)) == 0),
		"XSON bytes mismatch"
	);
	testRequire(
		xrtTimeParseRFC3339(
			XRT_STR_LITERAL("2000-01-01T19:04:05.123456Z"),
			&Expected
		) &&
		xrtValueGetTime(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("when")),
			&Time
		) &&
		(Time == Expected),
		"XSON time normalization mismatch"
	);
	testRequire(
		xrtValueGetFloat(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("nan")),
			&fValue
		) && isnan(fValue),
		"XSON nan mismatch"
	);
	testRequire(
		xrtValueGetFloat(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("inf")),
			&fValue
		) && isinf(fValue) && signbit(fValue),
		"XSON infinity mismatch"
	);
	xrtValueRelease(pRoot);
}



/* 自定义标签解码器把 demo 标签映射为普通字符串值。 */
static xvalue* testXsonDecode(
	xstrview Tag,
	xstrview Payload,
	ptr pUserData
)
{
	int* pCalls = (int*)pUserData;

	(*pCalls)++;
	if (
		(Tag.Size != 4u) || (memcmp(Tag.Data, "demo", 4u) != 0)
	) {
		return NULL;
	}
	return xrtValueString(Payload);
}



/* 长标签解码器验证 parser 传入完整借用名称和载荷。 */
static xvalue* testXsonLongDecode(
	xstrview Tag,
	xstrview Payload,
	ptr pUserData
)
{
	int* pCalls = (int*)pUserData;

	(*pCalls)++;
	if (
		(Tag.Size != 1024u) ||
		(Tag.Data == NULL) ||
		(memchr(Tag.Data, 'b', Tag.Size) != NULL) ||
		(Payload.Size != 2u) ||
		(memcmp(Payload.Data, "ok", 2u) != 0)
	) {
		return NULL;
	}
	return xrtValueString(Payload);
}



/* 长标签访问器直接验证事件保留完整名称和解码后的载荷。 */
static xxsonvisitaction testXsonLongVisitor(
	const xxsonevent* pEvent,
	ptr pUserData
)
{
	bool* pSeen = (bool*)pUserData;

	if ( pEvent->Type != XXSON_EVENT_CUSTOM ) {
		return XXSON_VISIT_NEXT;
	}
	if (
		(pEvent->Value.Tag.Name.Size != 1024u) ||
		(pEvent->Value.Tag.Name.Data == NULL) ||
		(memchr(pEvent->Value.Tag.Name.Data, 'b', 1024u) != NULL) ||
		(pEvent->Value.Tag.Payload.Size != 2u) ||
		(memcmp(pEvent->Value.Tag.Payload.Data, "ok", 2u) != 0)
	) {
		return XXSON_VISIT_FAIL;
	}
	*pSeen = true;
	return XXSON_VISIT_NEXT;
}



/* 验证扩展标签由输入总预算约束，不存在固定名称长度上限。 */
static void testXsonLongTag(void)
{
	char Source[1030];
	xxsonreadconfig Config;
	xvalue* pValue;
	xstrview Text;
	bool bSeen = false;
	int iCalls = 0;

	memset(Source, 'a', 1024u);
	memcpy(Source + 1024u, "(\"ok\")", 6u);
	xrtXsonReadConfigInit(&Config);
	Config.Flags = XXSON_READ_CUSTOM;
	Config.Decode = testXsonLongDecode;
	Config.DecodeData = &iCalls;
	testRequire(
		xrtXsonVisit(
			(xstrview){ Source, sizeof(Source) },
			&Config,
			testXsonLongVisitor,
			&bSeen
		) == XXSON_VISIT_DONE,
		"XSON long tag visitor failed"
	);
	testRequire(bSeen, "XSON long tag visitor missed custom event");
	pValue = xrtXsonRead(
		(xstrview){ Source, sizeof(Source) },
		&Config
	);
	testRequire(pValue != NULL, "XSON long tag decoder failed");
	testRequire(
		xrtValueGetString(pValue, &Text) &&
		(Text.Size == 2u) &&
		(memcmp(Text.Data, "ok", 2u) == 0),
		"XSON long tag payload mismatch"
	);
	testRequire(iCalls == 1, "XSON long tag decoder call count mismatch");
	xrtValueRelease(pValue);
}



/* 验证自定义标签必须显式开启，DOM 还必须具有解码器。 */
static void testXsonCustom(void)
{
	xxsonreadconfig Config;
	xvalue* pValue;
	xstrview Text;
	int iCalls = 0;

	testRequire(
		xrtXsonParse(XRT_STR_LITERAL("demo(\"ok\")")) == NULL,
		"XSON custom tag was enabled by default"
	);
	testXsonError(
		XXSON_ERROR_UNSUPPORTED,
		"XSON custom default error mismatch"
	);

	xrtXsonReadConfigInit(&Config);
	Config.Flags = XXSON_READ_CUSTOM;
	testRequire(
		xrtXsonRead(XRT_STR_LITERAL("demo(\"ok\")"), &Config) == NULL,
		"XSON custom tag without decoder succeeded"
	);
	testXsonError(
		XXSON_ERROR_UNSUPPORTED,
		"XSON missing custom decoder error mismatch"
	);

	Config.Decode = testXsonDecode;
	Config.DecodeData = &iCalls;
	pValue = xrtXsonRead(XRT_STR_LITERAL("demo(\"a\\nb\")"), &Config);
	testRequire(pValue != NULL, "XSON custom decoder failed");
	testRequire(
		xrtValueGetString(pValue, &Text) &&
		(Text.Size == 3u) && (memcmp(Text.Data, "a\nb", 3u) == 0),
		"XSON custom payload mismatch"
	);
	testRequire(iCalls == 1, "XSON custom decoder call count mismatch");
	xrtValueRelease(pValue);
}



/* 访问状态记录关键容器、整数键、二进制和自定义标签事件。 */
typedef struct testxsonvisitstate {
	size_t Events;
	bool SawIntKey;
	bool SawBytes;
	bool SawCustom;
	bool Stop;
	bool Fail;
} testxsonvisitstate;



/* 记录事件并按测试状态请求停止或失败。 */
static xxsonvisitaction testXsonVisitor(
	const xxsonevent* pEvent,
	ptr pUserData
)
{
	testxsonvisitstate* pState = (testxsonvisitstate*)pUserData;

	pState->Events++;
	if (
		(pEvent->Key.Type == XVALUE_KEY_INT) &&
		(pEvent->Key.Integer == -7)
	) {
		pState->SawIntKey = true;
	}
	if (
		(pEvent->Type == XXSON_EVENT_BYTES) &&
		(pEvent->Value.Bytes.Size == 1u) &&
		(pEvent->Value.Bytes.Data[0] == UINT8_C(0xFF))
	) {
		pState->SawBytes = true;
	}
	if (
		(pEvent->Type == XXSON_EVENT_CUSTOM) &&
		(pEvent->Value.Tag.Name.Size == 4u)
	) {
		pState->SawCustom = true;
	}
	if ( pState->Fail ) {
		return XXSON_VISIT_FAIL;
	}
	if ( pState->Stop && (pState->Events == 2u) ) {
		return XXSON_VISIT_STOP;
	}
	return XXSON_VISIT_NEXT;
}



/* 验证访问器是直接事件路径，并保持键、标签和控制语义。 */
static void testXsonVisit(void)
{
	xxsonreadconfig Config;
	testxsonvisitstate State;

	xrtXsonReadConfigInit(&Config);
	Config.Flags = XXSON_READ_CUSTOM;
	memset(&State, 0, sizeof(State));
	testRequire(
		xrtXsonVisit(
			XRT_STR_LITERAL(
				"[intmap{-7:1},bytes(\"/w==\"),demo(\"x\")]"
			),
			&Config,
			testXsonVisitor,
			&State
		) == XXSON_VISIT_DONE,
		"XSON visitor failed"
	);
	testRequire(
		State.SawIntKey && State.SawBytes && State.SawCustom,
		"XSON visitor event content mismatch"
	);

	memset(&State, 0, sizeof(State));
	State.Stop = true;
	testRequire(
		xrtXsonVisit(
			XRT_STR_LITERAL("[1,2,3]"),
			&Config,
			testXsonVisitor,
			&State
		) == XXSON_VISIT_STOPPED,
		"XSON visitor stop mismatch"
	);

	memset(&State, 0, sizeof(State));
	State.Fail = true;
	testRequire(
		xrtXsonVisit(
			XRT_STR_LITERAL("1"),
			&Config,
			testXsonVisitor,
			&State
		) == XXSON_VISIT_ERROR,
		"XSON visitor failure was ignored"
	);
	testXsonError(XXSON_ERROR_STATE, "XSON visitor failure error mismatch");
}



/* 验证旧版猜测语法、不安全 class 和畸形内建标签均被拒绝。 */
static void testXsonStrict(void)
{
	static const cstr arrInvalid[] = {
		"[1:2]",
		"{1,2}",
		"class(\"AQ==\")",
		"time(2000-01-02 03:04:05)",
		"bytes(\"A===\")",
		"time(\"2000-01-02 03:04:05\")",
		"float(\"infinity\")",
		"set{1,2}",
		"intmap[1:2]",
		"set[1,]",
		"intmap{1:2,}"
	};

	for ( size_t i = 0; i < (sizeof(arrInvalid) / sizeof(arrInvalid[0])); i++ ) {
		xrtClearError();
		testRequire(
			!xrtXsonValid((xstrview){ arrInvalid[i], strlen(arrInvalid[i]) }),
			"invalid XSON was accepted"
		);
		testRequire(xrtGetError() != NULL, "invalid XSON set no error");
	}
}



/* 验证重复键策略同时覆盖对象和整数映射。 */
static void testXsonDuplicates(void)
{
	xxsonreadconfig Config;
	xvalue* pRoot;

	testRequire(
		xrtXsonParse(XRT_STR_LITERAL("intmap{1:2,1:3}")) == NULL,
		"duplicate XSON intmap key was accepted"
	);
	testXsonError(XXSON_ERROR_DUPLICATE, "XSON duplicate error mismatch");

	xrtXsonReadConfigInit(&Config);
	Config.Duplicate = XXSON_DUPLICATE_KEEP;
	pRoot = xrtXsonRead(XRT_STR_LITERAL("intmap{1:2,1:3}"), &Config);
	testRequire(pRoot != NULL, "XSON duplicate keep failed");
	testXsonInt(xrtValueIntMapGet(pRoot, 1), 2, "XSON duplicate keep mismatch");
	xrtValueRelease(pRoot);

	Config.Duplicate = XXSON_DUPLICATE_REPLACE;
	pRoot = xrtXsonRead(XRT_STR_LITERAL("{\"a\":2,\"a\":3}"), &Config);
	testRequire(pRoot != NULL, "XSON duplicate replace failed");
	testXsonInt(
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("a")),
		3,
		"XSON duplicate replace mismatch"
	);
	xrtValueRelease(pRoot);
}



/* 验证运行时语法选项和主要资源上限。 */
static void testXsonConfigAndLimits(void)
{
	xxsonreadconfig Config;
	xvalue* pRoot;

	xrtXsonReadConfigInit(&Config);
	Config.Flags = XXSON_READ_COMMENTS | XXSON_READ_TRAILING_COMMA;
	pRoot = xrtXsonRead(XRT_STR_LITERAL("/*x*/set[1,2,]"), &Config);
	testRequire(pRoot != NULL, "XSON explicit syntax options failed");
	xrtValueRelease(pRoot);

	xrtXsonReadConfigInit(&Config);
	Config.MaxDecodedBytes = 1u;
	testRequire(
		xrtXsonRead(XRT_STR_LITERAL("bytes(\"AQI=\")"), &Config) == NULL,
		"XSON decoded byte limit was ignored"
	);
	testXsonError(XXSON_ERROR_LIMIT, "XSON decoded limit error mismatch");

	xrtXsonReadConfigInit(&Config);
	Config.MaxContainerItems = 1u;
	testRequire(
		xrtXsonRead(XRT_STR_LITERAL("set[1,2]"), &Config) == NULL,
		"XSON container limit was ignored"
	);
	testXsonError(XXSON_ERROR_LIMIT, "XSON container limit error mismatch");

	xrtXsonReadConfigInit(&Config);
	Config.Reserved[0] = 1u;
	testRequire(
		xrtXsonRead(XRT_STR_LITERAL("null"), &Config) == NULL,
		"XSON reserved config field was accepted"
	);
	testXsonError(XXSON_ERROR_CONFIG, "XSON config error mismatch");
}



/* 验证错误位置使用稳定的字节偏移和一基行列。 */
static void testXsonLocation(void)
{
	xxsonlocation Location;

	xrtClearError();
	testRequire(
		xrtXsonParse(XRT_STR_LITERAL("set[1,\n]")) == NULL,
		"invalid XSON location input was accepted"
	);
	testRequire(
		xrtXsonErrorLocation(xrtGetError(), &Location) &&
		(Location.Offset == 7u) && (Location.Line == 2u) &&
		(Location.Column == 1u),
		"XSON error location mismatch"
	);
}



/* 运行 XSON 读取、验证和直接访问的完整契约测试。 */
int main(void)
{
	testXsonJsonSubset();
	testXsonBuiltins();
	testXsonCustom();
	testXsonLongTag();
	testXsonVisit();
	testXsonStrict();
	testXsonDuplicates();
	testXsonConfigAndLimits();
	testXsonLocation();
	printf("[PASS] XSON read\n");
	return 0;
}
