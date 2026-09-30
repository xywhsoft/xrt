#include "../test.h"

#include <math.h>



/* 要求当前错误属于指定 XLON 代码。 */
static void testXlonError(xxlonerror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.xlon") == 0),
		sMessage
	);
	testRequire(xrtErrorCode(pError) == (int32)Code, sMessage);
}



/* 验证任意值是期望整数。 */
static void testXlonInt(
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



/* 验证任意值是期望无符号整数。 */
static void testXlonUInt(
	const xvalue* pValue,
	uint64 iExpected,
	cstr sMessage
)
{
	uint64 iValue;

	testRequire(
		xrtValueGetUInt(pValue, &iValue) && (iValue == iExpected),
		sMessage
	);
}



/* 验证全部严格 JSON 仍是 XLON 的无损子集。 */
static void testXlonJsonSubset(void)
{
	xvalue* pRoot = xrtXlonParse(XRT_STR_LITERAL(
		"{\"name\":\"xrt\",\"items\":[1,true,null]}"
	));
	xstrview Name;
	xvalue* pItems;

	testRequire(pRoot != NULL, "XLON JSON subset parse failed");
	testRequire(
		xrtValueGetString(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("name")),
			&Name
		) &&
		(Name.Size == 3u) && (memcmp(Name.Data, "xrt", 3u) == 0),
		"XLON JSON subset string mismatch"
	);
	pItems = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("items"));
	testRequire(
		(xrtValueType(pItems) == XVALUE_ARRAY) &&
		(xrtValueCount(pItems) == 3u),
		"XLON JSON subset array mismatch"
	);
	testXlonInt(xrtValueArrayGet(pItems, 0), 1, "XLON subset integer mismatch");
	xrtValueRelease(pRoot);
}



/* 验证显式容器、二进制、时间和非有限浮点的 DOM 表示。 */
static void testXlonBuiltins(void)
{
	static const uint8 arrBytes[] = { 1u, 2u, 3u, 4u };
	xvalue* pRoot = xrtXlonParse(XRT_STR_LITERAL(
		"{"
		"\"map\":intmap{-5:10,2:\"ok\"},"
		"\"set\":set[1,2,3],"
		"\"blob\":bytes(\"AQIDBA==\"),"
		"\"character\":char(\"你\"),"
		"\"when\":time(\"2000-01-02T03:04:05.123456+08:00\"),"
		"\"nan\":float(\"nan\"),"
		"\"inf\":float(\"-inf\"),"
		"\"max\":18446744073709551615"
		"}"
	));
	xvalue* pMap;
	xvalue* pSet;
	xvalue* pBlob;
	xbytesview Bytes;
	xtime Time;
	xtime Expected;
	double fValue;
	uint32 iCharacter;

	testRequire(pRoot != NULL, "XLON builtin parse failed");
	pMap = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("map"));
	pSet = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("set"));
	pBlob = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("blob"));
	testRequire(xrtValueType(pMap) == XVALUE_INT_MAP, "XLON intmap type mismatch");
	testXlonInt(xrtValueIntMapGet(pMap, -5), 10, "XLON intmap value mismatch");
	testRequire(
		(xrtValueType(pSet) == XVALUE_SET) && (xrtValueCount(pSet) == 3u),
		"XLON set mismatch"
	);
	testRequire(
		xrtValueGetBytes(pBlob, &Bytes) &&
		(Bytes.Size == sizeof(arrBytes)) &&
		(memcmp(Bytes.Data, arrBytes, sizeof(arrBytes)) == 0),
		"XLON bytes mismatch"
	);
	testRequire(
		xrtValueGetChar(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("character")),
			&iCharacter
		) && (iCharacter == UINT32_C(0x4F60)),
		"XLON character mismatch"
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
		"XLON time normalization mismatch"
	);
	testRequire(
		xrtValueGetFloat(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("nan")),
			&fValue
		) && isnan(fValue),
		"XLON nan mismatch"
	);
	testRequire(
		xrtValueGetFloat(
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("inf")),
			&fValue
		) && isinf(fValue) && signbit(fValue),
		"XLON infinity mismatch"
	);
	testXlonUInt(
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("max")),
		UINT64_MAX,
		"XLON uint64 mismatch"
	);
	xrtValueRelease(pRoot);
}



/* 自定义标签解码器把 demo 标签映射为普通字符串值。 */
static xvalue* testXlonDecode(
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
static xvalue* testXlonLongDecode(
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
static xxlonvisitaction testXlonLongVisitor(
	const xxlonevent* pEvent,
	ptr pUserData
)
{
	bool* pSeen = (bool*)pUserData;

	if ( pEvent->Type != XXLON_EVENT_CUSTOM ) {
		return XXLON_VISIT_NEXT;
	}
	if (
		(pEvent->Value.Tag.Name.Size != 1024u) ||
		(pEvent->Value.Tag.Name.Data == NULL) ||
		(memchr(pEvent->Value.Tag.Name.Data, 'b', 1024u) != NULL) ||
		(pEvent->Value.Tag.Payload.Size != 2u) ||
		(memcmp(pEvent->Value.Tag.Payload.Data, "ok", 2u) != 0)
	) {
		return XXLON_VISIT_FAIL;
	}
	*pSeen = true;
	return XXLON_VISIT_NEXT;
}



/* 验证扩展标签由输入总预算约束，不存在固定名称长度上限。 */
static void testXlonLongTag(void)
{
	char Source[1030];
	xxlonreadconfig Config;
	xvalue* pValue;
	xstrview Text;
	bool bSeen = false;
	int iCalls = 0;

	memset(Source, 'a', 1024u);
	memcpy(Source + 1024u, "(\"ok\")", 6u);
	xrtXlonReadConfigInit(&Config);
	Config.Flags = XXLON_READ_CUSTOM;
	Config.Decode = testXlonLongDecode;
	Config.DecodeData = &iCalls;
	testRequire(
		xrtXlonVisit(
			(xstrview){ Source, sizeof(Source) },
			&Config,
			testXlonLongVisitor,
			&bSeen
		) == XXLON_VISIT_DONE,
		"XLON long tag visitor failed"
	);
	testRequire(bSeen, "XLON long tag visitor missed custom event");
	pValue = xrtXlonRead(
		(xstrview){ Source, sizeof(Source) },
		&Config
	);
	testRequire(pValue != NULL, "XLON long tag decoder failed");
	testRequire(
		xrtValueGetString(pValue, &Text) &&
		(Text.Size == 2u) &&
		(memcmp(Text.Data, "ok", 2u) == 0),
		"XLON long tag payload mismatch"
	);
	testRequire(iCalls == 1, "XLON long tag decoder call count mismatch");
	xrtValueRelease(pValue);
}



/* 验证自定义标签必须显式开启，DOM 还必须具有解码器。 */
static void testXlonCustom(void)
{
	xxlonreadconfig Config;
	xvalue* pValue;
	xstrview Text;
	int iCalls = 0;

	testRequire(
		xrtXlonParse(XRT_STR_LITERAL("demo(\"ok\")")) == NULL,
		"XLON custom tag was enabled by default"
	);
	testXlonError(
		XXLON_ERROR_UNSUPPORTED,
		"XLON custom default error mismatch"
	);

	xrtXlonReadConfigInit(&Config);
	Config.Flags = XXLON_READ_CUSTOM;
	testRequire(
		xrtXlonRead(XRT_STR_LITERAL("demo(\"ok\")"), &Config) == NULL,
		"XLON custom tag without decoder succeeded"
	);
	testXlonError(
		XXLON_ERROR_UNSUPPORTED,
		"XLON missing custom decoder error mismatch"
	);

	Config.Decode = testXlonDecode;
	Config.DecodeData = &iCalls;
	pValue = xrtXlonRead(XRT_STR_LITERAL("demo(\"a\\nb\")"), &Config);
	testRequire(pValue != NULL, "XLON custom decoder failed");
	testRequire(
		xrtValueGetString(pValue, &Text) &&
		(Text.Size == 3u) && (memcmp(Text.Data, "a\nb", 3u) == 0),
		"XLON custom payload mismatch"
	);
	testRequire(iCalls == 1, "XLON custom decoder call count mismatch");
	xrtValueRelease(pValue);
}



/* 访问状态记录关键容器、整数键、二进制和自定义标签事件。 */
typedef struct testxlonvisitstate {
	size_t Events;
	bool SawIntKey;
	bool SawBytes;
	bool SawCustom;
	bool SawUInt;
	bool Stop;
	bool Fail;
} testxlonvisitstate;



/* 记录事件并按测试状态请求停止或失败。 */
static xxlonvisitaction testXlonVisitor(
	const xxlonevent* pEvent,
	ptr pUserData
)
{
	testxlonvisitstate* pState = (testxlonvisitstate*)pUserData;

	pState->Events++;
	if (
		(pEvent->Key.Type == XVALUE_KEY_INT) &&
		(pEvent->Key.Integer == -7)
	) {
		pState->SawIntKey = true;
	}
	if (
		(pEvent->Type == XXLON_EVENT_BYTES) &&
		(pEvent->Value.Bytes.Size == 1u) &&
		(pEvent->Value.Bytes.Data[0] == UINT8_C(0xFF))
	) {
		pState->SawBytes = true;
	}
	if (
		(pEvent->Type == XXLON_EVENT_UINT) &&
		(pEvent->Value.Unsigned == UINT64_MAX)
	) {
		pState->SawUInt = true;
	}
	if (
		(pEvent->Type == XXLON_EVENT_CUSTOM) &&
		(pEvent->Value.Tag.Name.Size == 4u)
	) {
		pState->SawCustom = true;
	}
	if ( pState->Fail ) {
		return XXLON_VISIT_FAIL;
	}
	if ( pState->Stop && (pState->Events == 2u) ) {
		return XXLON_VISIT_STOP;
	}
	return XXLON_VISIT_NEXT;
}



/* 验证访问器是直接事件路径，并保持键、标签和控制语义。 */
static void testXlonVisit(void)
{
	xxlonreadconfig Config;
	testxlonvisitstate State;

	xrtXlonReadConfigInit(&Config);
	Config.Flags = XXLON_READ_CUSTOM;
	memset(&State, 0, sizeof(State));
	testRequire(
		xrtXlonVisit(
			XRT_STR_LITERAL(
				"[intmap{-7:1},bytes(\"/w==\"),demo(\"x\"),"
				"18446744073709551615]"
			),
			&Config,
			testXlonVisitor,
			&State
		) == XXLON_VISIT_DONE,
		"XLON visitor failed"
	);
	testRequire(
		State.SawIntKey && State.SawBytes && State.SawCustom && State.SawUInt,
		"XLON visitor event content mismatch"
	);

	memset(&State, 0, sizeof(State));
	State.Stop = true;
	testRequire(
		xrtXlonVisit(
			XRT_STR_LITERAL("[1,2,3]"),
			&Config,
			testXlonVisitor,
			&State
		) == XXLON_VISIT_STOPPED,
		"XLON visitor stop mismatch"
	);

	memset(&State, 0, sizeof(State));
	State.Fail = true;
	testRequire(
		xrtXlonVisit(
			XRT_STR_LITERAL("1"),
			&Config,
			testXlonVisitor,
			&State
		) == XXLON_VISIT_ERROR,
		"XLON visitor failure was ignored"
	);
	testXlonError(XXLON_ERROR_STATE, "XLON visitor failure error mismatch");
}



/* 验证旧版猜测语法、不安全 class 和畸形内建标签均被拒绝。 */
static void testXlonStrict(void)
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
			!xrtXlonValid((xstrview){ arrInvalid[i], strlen(arrInvalid[i]) }),
			"invalid XLON was accepted"
		);
		testRequire(xrtGetError() != NULL, "invalid XLON set no error");
	}
}



/* 验证重复键策略同时覆盖对象和整数映射。 */
static void testXlonDuplicates(void)
{
	xxlonreadconfig Config;
	xvalue* pRoot;

	testRequire(
		xrtXlonParse(XRT_STR_LITERAL("intmap{1:2,1:3}")) == NULL,
		"duplicate XLON intmap key was accepted"
	);
	testXlonError(XXLON_ERROR_DUPLICATE, "XLON duplicate error mismatch");

	xrtXlonReadConfigInit(&Config);
	Config.Duplicate = XXLON_DUPLICATE_KEEP;
	pRoot = xrtXlonRead(XRT_STR_LITERAL("intmap{1:2,1:3}"), &Config);
	testRequire(pRoot != NULL, "XLON duplicate keep failed");
	testXlonInt(xrtValueIntMapGet(pRoot, 1), 2, "XLON duplicate keep mismatch");
	xrtValueRelease(pRoot);

	Config.Duplicate = XXLON_DUPLICATE_REPLACE;
	pRoot = xrtXlonRead(XRT_STR_LITERAL("{\"a\":2,\"a\":3}"), &Config);
	testRequire(pRoot != NULL, "XLON duplicate replace failed");
	testXlonInt(
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("a")),
		3,
		"XLON duplicate replace mismatch"
	);
	xrtValueRelease(pRoot);
}



/* 验证运行时语法选项和主要资源上限。 */
static void testXlonConfigAndLimits(void)
{
	xxlonreadconfig Config;
	xvalue* pRoot;

	xrtXlonReadConfigInit(&Config);
	Config.Flags = XXLON_READ_COMMENTS | XXLON_READ_TRAILING_COMMA;
	pRoot = xrtXlonRead(XRT_STR_LITERAL("/*x*/set[1,2,]"), &Config);
	testRequire(pRoot != NULL, "XLON explicit syntax options failed");
	xrtValueRelease(pRoot);

	xrtXlonReadConfigInit(&Config);
	Config.MaxDecodedBytes = 1u;
	testRequire(
		xrtXlonRead(XRT_STR_LITERAL("bytes(\"AQI=\")"), &Config) == NULL,
		"XLON decoded byte limit was ignored"
	);
	testXlonError(XXLON_ERROR_LIMIT, "XLON decoded limit error mismatch");

	xrtXlonReadConfigInit(&Config);
	Config.MaxContainerItems = 1u;
	testRequire(
		xrtXlonRead(XRT_STR_LITERAL("set[1,2]"), &Config) == NULL,
		"XLON container limit was ignored"
	);
	testXlonError(XXLON_ERROR_LIMIT, "XLON container limit error mismatch");

	xrtXlonReadConfigInit(&Config);
	Config.Reserved[0] = 1u;
	testRequire(
		xrtXlonRead(XRT_STR_LITERAL("null"), &Config) == NULL,
		"XLON reserved config field was accepted"
	);
	testXlonError(XXLON_ERROR_CONFIG, "XLON config error mismatch");
}



/* 验证错误位置使用稳定的字节偏移和一基行列。 */
static void testXlonLocation(void)
{
	xxlonlocation Location;

	xrtClearError();
	testRequire(
		xrtXlonParse(XRT_STR_LITERAL("set[1,\n]")) == NULL,
		"invalid XLON location input was accepted"
	);
	testRequire(
		xrtXlonErrorLocation(xrtGetError(), &Location) &&
		(Location.Offset == 7u) && (Location.Line == 2u) &&
		(Location.Column == 1u),
		"XLON error location mismatch"
	);
}



/* 运行 XLON 读取、验证和直接访问的完整契约测试。 */
int main(void)
{
	testXlonJsonSubset();
	testXlonBuiltins();
	testXlonCustom();
	testXlonLongTag();
	testXlonVisit();
	testXlonStrict();
	testXlonDuplicates();
	testXlonConfigAndLimits();
	testXlonLocation();
	printf("[PASS] XLON read\n");
	return 0;
}
