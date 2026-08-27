#include "../test.h"

#include <math.h>



/* 使用高级配置把 Value 写成内存文本。 */
static str testJsonWriteText(
	const xvalue* pValue,
	const xjsonwriteconfig* pConfig,
	size_t* pSize
)
{
	xjsonwriter* pWriter = xrtJsonWriterCreate(pConfig);
	str sText = NULL;

	if ( pWriter == NULL ) {
		return NULL;
	}
	if ( xrtJsonWriterValue(pWriter, pValue) &&
		 xrtJsonWriterFinish(pWriter) ) {
		sText = xrtJsonWriterTake(pWriter, pSize);
	}
	xrtJsonWriterFree(pWriter);
	return sText;
}



/* 要求拥有字符串与预期字节完全一致并释放。 */
static void testJsonText(
	str sText,
	size_t iSize,
	cstr sExpected,
	cstr sMessage
)
{
	size_t iExpected = strlen(sExpected);

	testRequire(sText != NULL, sMessage);
	testRequire(iSize == iExpected, sMessage);
	testRequire(memcmp(sText, sExpected, iExpected + 1u) == 0, sMessage);
	xrtFree(sText);
}



/* 构建包含对象、数组和转义字符串的标准 JSON Value。 */
static xvalue* testJsonBuildValue(void)
{
	const char arrName[] = { 'A', '\n', 'B' };
	xvalue* pRoot = xrtValueObject();
	xvalue* pItems = xrtValueArray();

	if (
		(pRoot == NULL) || (pItems == NULL) ||
		!xrtValueArrayAppendNew(pItems, xrtValueInt(1)) ||
		!xrtValueArrayAppendNew(pItems, xrtValueBool(true)) ||
		!xrtValueArrayAppendNew(pItems, xrtValueRetain(xrtValueNull())) ||
		!xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("name"),
			xrtValueString((xstrview){ arrName, sizeof(arrName) })
		) ||
		!xrtValueObjectSet(pRoot, XRT_STR_LITERAL("items"), pItems) ||
		!xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("f"),
			xrtValueFloat(1.5)
		) ||
		!xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("u"),
			xrtValueUInt(UINT64_MAX)
		)
	) {
		xrtValueRelease(pItems);
		xrtValueRelease(pRoot);
		return NULL;
	}
	xrtValueRelease(pItems);
	return pRoot;
}



/* 验证默认紧凑输出、美化输出和稳定对象顺序。 */
static void testJsonStringify(void)
{
	xvalue* pRoot = testJsonBuildValue();
	str sText;
	size_t iSize = 0;

	testRequire(pRoot != NULL, "JSON stringify fixture failed");
	sText = xrtJsonStringify(pRoot, false, &iSize);
	testJsonText(
		sText,
		iSize,
		"{\"name\":\"A\\nB\",\"items\":[1,true,null],\"f\":1.5,"
		"\"u\":18446744073709551615}",
		"compact JSON stringify mismatch"
	);
	sText = xrtJsonStringify(pRoot, true, &iSize);
	testJsonText(
		sText,
		iSize,
		"{\n"
		"  \"name\": \"A\\nB\",\n"
		"  \"items\": [\n"
		"    1,\n"
		"    true,\n"
		"    null\n"
		"  ],\n"
		"  \"f\": 1.5,\n"
		"  \"u\": 18446744073709551615\n"
		"}",
		"pretty JSON stringify mismatch"
	);
	xrtValueRelease(pRoot);
}



/* 验证 Unicode、HTML、斜线和非 ASCII 转义选项。 */
static void testJsonEscapes(void)
{
	static const unsigned char arrText[] = {
		'<', '&', '>', '/', 0xC3, 0x9F, 0
	};
	xjsonwriteconfig Config;
	xvalue* pValue = xrtValueString(
		(xstrview){ (cstr)arrText, sizeof(arrText) }
	);
	str sText;
	size_t iSize;

	testRequire(pValue != NULL, "JSON escape fixture failed");
	xrtJsonWriteConfigInit(&Config);
	Config.Flags =
		XJSON_WRITE_ESCAPE_HTML |
		XJSON_WRITE_ESCAPE_SLASH |
		XJSON_WRITE_ESCAPE_NON_ASCII;
	sText = testJsonWriteText(pValue, &Config, &iSize);
	testJsonText(
		sText,
		iSize,
		"\"\\u003C\\u0026\\u003E\\/\\u00DF\\u0000\"",
		"JSON configured escaping mismatch"
	);
	xrtValueRelease(pValue);
}



/* 验证非有限浮点的拒绝、null 和字符串策略。 */
static void testJsonNonFinite(void)
{
	xjsonwriteconfig Config;
	xvalue* pValue = xrtValueFloat(INFINITY);
	str sText;
	size_t iSize;

	testRequire(pValue != NULL, "JSON non-finite fixture failed");
	xrtJsonWriteConfigInit(&Config);
	xrtClearError();
	testRequire(
		testJsonWriteText(pValue, &Config, &iSize) == NULL,
		"non-finite JSON float should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_UNSUPPORTED,
		"non-finite JSON error mismatch"
	);

	Config.NonFinite = XJSON_NONFINITE_NULL;
	sText = testJsonWriteText(pValue, &Config, &iSize);
	testJsonText(sText, iSize, "null", "non-finite null policy mismatch");

	Config.NonFinite = XJSON_NONFINITE_STRING;
	sText = testJsonWriteText(pValue, &Config, &iSize);
	testJsonText(
		sText,
		iSize,
		"\"Infinity\"",
		"non-finite string policy mismatch"
	);
	xrtValueRelease(pValue);
}



/* 验证扩展 Value 默认拒绝及显式 null、skip、容器兼容映射。 */
static void testJsonExtendedValues(void)
{
	const unsigned char arrBytes[] = { 1, 2, 3 };
	xjsonwriteconfig Config;
	xvalue* pBytes = xrtValueBytes(
		(xbytesview){ arrBytes, sizeof(arrBytes) }
	);
	xvalue* pRoot = xrtValueObject();
	xvalue* pSet = xrtValueSet();
	xvalue* pMap = xrtValueIntMap();
	xvalue* pNested = xrtValueObject();
	xvalue* pNestedArray = xrtValueArray();
	str sText;
	size_t iSize;

	testRequire(
		(pBytes != NULL) && (pRoot != NULL) && (pSet != NULL) &&
		(pMap != NULL) && (pNested != NULL) && (pNestedArray != NULL),
		"JSON extended fixture failed"
	);
	xrtJsonWriteConfigInit(&Config);
	testRequire(
		testJsonWriteText(pBytes, &Config, &iSize) == NULL,
		"JSON bytes should fail by default"
	);

	Config.Unsupported = XJSON_UNSUPPORTED_NULL;
	sText = testJsonWriteText(pBytes, &Config, &iSize);
	testJsonText(sText, iSize, "null", "JSON unsupported null policy mismatch");

	testRequire(
		xrtValueObjectSet(pRoot, XRT_STR_LITERAL("blob"), pBytes) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("ok"),
			xrtValueInt(1)
		),
		"JSON skip fixture setup failed"
	);
	Config.Unsupported = XJSON_UNSUPPORTED_SKIP;
	sText = testJsonWriteText(pRoot, &Config, &iSize);
	testJsonText(sText, iSize, "{\"ok\":1}", "JSON unsupported skip mismatch");

	testRequire(
		xrtValueSetAddNew(pSet, xrtValueString(XRT_STR_LITERAL("a"))) &&
		xrtValueSetAddNew(pSet, xrtValueString(XRT_STR_LITERAL("b"))) &&
		xrtValueIntMapSetNew(pMap, -1, xrtValueBool(false)) &&
		xrtValueIntMapSetNew(pMap, 2, xrtValueInt(7)),
		"JSON compatible container fixture setup failed"
	);
	Config.Unsupported = XJSON_UNSUPPORTED_REJECT;
	Config.Flags = XJSON_WRITE_CONTAINER_COMPAT;
	sText = testJsonWriteText(pSet, &Config, &iSize);
	testJsonText(sText, iSize, "[\"a\",\"b\"]", "JSON set mapping mismatch");
	testRequire(
		xrtValueObjectSet(pNested, XRT_STR_LITERAL("items"), pSet),
		"JSON nested set fixture setup failed"
	);
	sText = testJsonWriteText(pNested, &Config, &iSize);
	testJsonText(
		sText,
		iSize,
		"{\"items\":[\"a\",\"b\"]}",
		"JSON nested set mapping mismatch"
	);
	testRequire(
		xrtValueArrayAppend(pNestedArray, pSet),
		"JSON array nested set fixture setup failed"
	);
	sText = testJsonWriteText(pNestedArray, &Config, &iSize);
	testJsonText(
		sText,
		iSize,
		"[[\"a\",\"b\"]]",
		"JSON array nested set mapping mismatch"
	);
	sText = testJsonWriteText(pMap, &Config, &iSize);
	testJsonText(
		sText,
		iSize,
		"{\"-1\":false,\"2\":7}",
		"JSON int-map mapping mismatch"
	);

	xrtValueRelease(pNestedArray);
	xrtValueRelease(pNested);
	xrtValueRelease(pMap);
	xrtValueRelease(pSet);
	xrtValueRelease(pRoot);
	xrtValueRelease(pBytes);
}



/* 验证直接写入器不需要先构建 Value DOM。 */
static void testJsonDirectWriter(void)
{
	xjsonwriteconfig Config;
	xjsonwriter* pWriter;
	str sText;
	size_t iSize;

	xrtJsonWriteConfigInit(&Config);
	pWriter = xrtJsonWriterCreate(&Config);
	testRequire(pWriter != NULL, "JSON direct writer create failed");
	testRequire(
		xrtJsonWriterObject(pWriter) &&
		xrtJsonWriterName(pWriter, XRT_STR_LITERAL("code")) &&
		xrtJsonWriterInt(pWriter, 200) &&
		xrtJsonWriterName(pWriter, XRT_STR_LITERAL("max")) &&
		xrtJsonWriterUInt(pWriter, UINT64_MAX) &&
		xrtJsonWriterName(pWriter, XRT_STR_LITERAL("items")) &&
		xrtJsonWriterArray(pWriter) &&
		xrtJsonWriterString(pWriter, XRT_STR_LITERAL("x")) &&
		xrtJsonWriterNull(pWriter) &&
		xrtJsonWriterEnd(pWriter) &&
		xrtJsonWriterEnd(pWriter) &&
		xrtJsonWriterFinish(pWriter),
		"JSON direct writer sequence failed"
	);
	sText = xrtJsonWriterTake(pWriter, &iSize);
	testJsonText(
		sText,
		iSize,
		"{\"code\":200,\"max\":18446744073709551615,"
		"\"items\":[\"x\",null]}",
		"JSON direct writer output mismatch"
	);
	xrtJsonWriterFree(pWriter);

	pWriter = xrtJsonWriterCreate(&Config);
	testRequire(pWriter != NULL, "JSON invalid-state writer create failed");
	testRequire(
		xrtJsonWriterObject(pWriter) &&
		xrtJsonWriterName(pWriter, XRT_STR_LITERAL("missing")),
		"JSON invalid-state setup failed"
	);
	xrtClearError();
	testRequire(
		!xrtJsonWriterFinish(pWriter),
		"JSON writer accepted object name without value"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_STATE,
		"JSON writer state error mismatch"
	);
	xrtJsonWriterFree(pWriter);
}



/* Sink 测试状态同时覆盖收集、主动失败和回调重入。 */
typedef struct testjsonsink {
	xbuffer Buffer;
	xjsonwriter* Writer;
	bool Fail;
	bool Reenter;
	bool SetError;
} testjsonsink;



/* 同步消费输出，并按测试配置注入失败或重入。 */
static bool testJsonSinkWrite(xbytesview Data, ptr pUserData)
{
	testjsonsink* pSink = (testjsonsink*)pUserData;

	if ( pSink->Reenter ) {
		pSink->Reenter = false;
		(void)xrtJsonWriterNull(pSink->Writer);
	}
	if ( pSink->Fail ) {
		if ( pSink->SetError ) {
			xerror* pError = xrtErrorCreate(
				XERR_VALUE,
				"test.json.sink",
				88,
				"sink failure"
			);

			testRequire(pError != NULL, "JSON sink test error create failed");
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	return xrtBufferAppend(&pSink->Buffer, Data);
}



/* 验证 sink 输出、上限、失败传播和不可重入合同。 */
static void testJsonSink(void)
{
	xjsonwriteconfig Config;
	testjsonsink Sink;
	xjsonwriter* pWriter;
	xvalue* pValue = xrtValueInt(1234);
	xerror* pStale;

	memset(&Sink, 0, sizeof(Sink));
	testRequire(xrtBufferInit(&Sink.Buffer), "JSON sink buffer init failed");
	xrtJsonWriteConfigInit(&Config);
	testRequire(
		xrtJsonWrite(pValue, &Config, testJsonSinkWrite, &Sink) &&
		(Sink.Buffer.Size == 4u) &&
		(memcmp(Sink.Buffer.Data, "1234", 4u) == 0),
		"JSON sink output mismatch"
	);
	xrtBufferClear(&Sink.Buffer);

	Config.MaxOutputBytes = 3u;
	xrtClearError();
	testRequire(
		!xrtJsonWrite(pValue, &Config, testJsonSinkWrite, &Sink) &&
		(Sink.Buffer.Size == 0),
		"JSON output limit was not atomic for one token"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_LIMIT,
		"JSON output limit error mismatch"
	);

	xrtJsonWriteConfigInit(&Config);
	Sink.Fail = true;
	pStale = xrtErrorCreate(XERR_VALUE, "test.stale", 1, "stale error");
	testRequire(pStale != NULL, "JSON stale sink error create failed");
	xrtSetError(pStale);
	xrtErrorFree(pStale);
	testRequire(
		!xrtJsonWrite(pValue, &Config, testJsonSinkWrite, &Sink),
		"JSON sink failure was ignored"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_OUTPUT,
		"JSON sink failure error mismatch"
	);

	Sink.SetError = true;
	testRequire(
		!xrtJsonWrite(pValue, &Config, testJsonSinkWrite, &Sink),
		"JSON sink specific failure was ignored"
	);
	testRequire(
		strcmp(xrtErrorDomain(xrtGetError()), "test.json.sink") == 0,
		"JSON sink specific error was overwritten"
	);
	Sink.Fail = false;
	Sink.SetError = false;

	pWriter = xrtJsonWriterCreateSink(&Config, testJsonSinkWrite, &Sink);
	testRequire(pWriter != NULL, "JSON reentrant sink writer create failed");
	Sink.Writer = pWriter;
	Sink.Reenter = true;
	xrtClearError();
	testRequire(
		!xrtJsonWriterInt(pWriter, 1),
		"JSON sink callback reentry was accepted"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_STATE,
		"JSON sink reentry error mismatch"
	);
	xrtJsonWriterFree(pWriter);

	xrtValueRelease(pValue);
	xrtBufferUnit(&Sink.Buffer);
}



/* 验证写出配置拒绝零初始化、未知标志和非零保留字段。 */
static void testJsonWriteConfig(void)
{
	xjsonwriteconfig Config;
	xvalue* pValue = xrtValueNull();

	xrtClearError();
	testRequire(
		!xrtJsonWriterNull(NULL),
		"null JSON writer was accepted"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null JSON writer error mismatch"
	);

	memset(&Config, 0, sizeof(Config));
	testRequire(
		xrtJsonWriterCreate(&Config) == NULL,
		"zeroed JSON write config should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_CONFIG,
		"zeroed JSON write config error mismatch"
	);

	xrtJsonWriteConfigInit(&Config);
	Config.Flags = UINT32_C(0x80000000);
	testRequire(
		xrtJsonWrite(pValue, &Config, testJsonSinkWrite, NULL) == false,
		"unknown JSON write flag should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_CONFIG,
		"unknown JSON write flag error mismatch"
	);

	xrtJsonWriteConfigInit(&Config);
	Config.Reserved[0] = 1u;
	testRequire(
		xrtJsonWriterCreate(&Config) == NULL,
		"reserved JSON write field should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_CONFIG,
		"reserved JSON write field error mismatch"
	);

	xrtJsonWriteConfigInit(&Config);
	Config.Indent = 17u;
	testRequire(
		xrtJsonWriterCreate(&Config) == NULL,
		"oversized JSON indentation should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XJSON_ERROR_CONFIG,
		"oversized JSON indentation error mismatch"
	);
	xrtValueRelease(pValue);
}



/* 运行 JSON Value 写出、直接写入和 sink 合同测试。 */
int main(void)
{
	testJsonStringify();
	testJsonEscapes();
	testJsonNonFinite();
	testJsonExtendedValues();
	testJsonDirectWriter();
	testJsonSink();
	testJsonWriteConfig();
	printf("[PASS] JSON write\n");
	return 0;
}
