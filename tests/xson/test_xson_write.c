#include "../test.h"

#include <math.h>



/* 使用高级配置把 Value 写成内存文本。 */
static str testXsonWriteText(
	const xvalue* pValue,
	const xxsonwriteconfig* pConfig,
	size_t* pSize
)
{
	xxsonwriter* pWriter = xrtXsonWriterCreate(pConfig);
	str sText = NULL;

	if ( pWriter == NULL ) {
		return NULL;
	}
	if (
		xrtXsonWriterValue(pWriter, pValue) &&
		xrtXsonWriterFinish(pWriter)
	) {
		sText = xrtXsonWriterTake(pWriter, pSize);
	}
	xrtXsonWriterFree(pWriter);
	return sText;
}



/* 要求拥有字符串与预期字节完全一致并释放。 */
static void testXsonText(
	str sText,
	size_t iSize,
	cstr sExpected,
	cstr sMessage
)
{
	size_t iExpected = strlen(sExpected);

	testRequire(sText != NULL, sMessage);
	if (
		(iSize != iExpected) ||
		(memcmp(sText, sExpected, iSize < iExpected ? iSize : iExpected) != 0)
	) {
		fprintf(
			stderr,
			"[XSON text] expected=%s actual=%.*s\n",
			sExpected,
			(int)iSize,
			sText
		);
	}
	testRequire(iSize == iExpected, sMessage);
	testRequire(memcmp(sText, sExpected, iExpected + 1u) == 0, sMessage);
	xrtFree(sText);
}



/* 要求当前错误属于 XSON 域并具有指定错误码。 */
static void testXsonWriteError(xxsonerror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(strcmp(xrtErrorDomain(pError), "xrt.xson") == 0, sMessage);
	testRequire(xrtErrorCode(pError) == (int64)Code, sMessage);
}



/* 构建覆盖全部可移植 XSON 类型的有序 Value 树。 */
static xvalue* testXsonBuildValue(void)
{
	static const uint8 arrBytes[] = { 0u, 1u, 2u, 255u };
	xvalue* pRoot = xrtValueObject();
	xvalue* pArray = xrtValueArray();
	xvalue* pMap = xrtValueIntMap();
	xvalue* pSet = xrtValueSet();
	xtime Time;
	bool bResult;

	if (
		(pRoot == NULL) || (pArray == NULL) ||
		(pMap == NULL) || (pSet == NULL) ||
		!xrtTimeParseRFC3339(
			XRT_STR_LITERAL("2026-07-31T12:34:56.12+08:00"),
			&Time
		)
	) {
		xrtValueRelease(pSet);
		xrtValueRelease(pMap);
		xrtValueRelease(pArray);
		xrtValueRelease(pRoot);
		return NULL;
	}
	bResult =
		xrtValueArrayAppendNew(pArray, xrtValueInt(1)) &&
		xrtValueArrayAppendNew(pArray, xrtValueBool(true)) &&
		xrtValueIntMapSetNew(
			pMap,
			-5,
			xrtValueString(XRT_STR_LITERAL("n"))
		) &&
		xrtValueIntMapSetNew(pMap, 2, xrtValueBool(false)) &&
		xrtValueSetAddNew(
			pSet,
			xrtValueString(XRT_STR_LITERAL("a"))
		) &&
		xrtValueSetAddNew(pSet, xrtValueInt(2)) &&
		xrtValueObjectSet(pRoot, XRT_STR_LITERAL("items"), pArray) &&
		xrtValueObjectSet(pRoot, XRT_STR_LITERAL("map"), pMap) &&
		xrtValueObjectSet(pRoot, XRT_STR_LITERAL("set"), pSet) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("blob"),
			xrtValueBytes((xbytesview){ arrBytes, sizeof(arrBytes) })
		) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("time"),
			xrtValueTime(Time)
		) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("nan"),
			xrtValueFloat(NAN)
		) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("max"),
			xrtValueUInt(UINT64_MAX)
		);
	xrtValueRelease(pSet);
	xrtValueRelease(pMap);
	xrtValueRelease(pArray);
	if ( !bResult ) {
		xrtValueRelease(pRoot);
		return NULL;
	}
	return pRoot;
}



/* 验证全部内建类型具有明确、稳定且可往返的文本表示。 */
static void testXsonStringify(void)
{
	xvalue* pRoot = testXsonBuildValue();
	xvalue* pFloat;
	str sText;
	size_t iSize = 0;

	testRequire(pRoot != NULL, "XSON stringify fixture failed");
	sText = xrtXsonStringify(pRoot, false, &iSize);
	testXsonText(
		sText,
		iSize,
		"{\"items\":[1,true],"
		"\"map\":intmap{-5:\"n\",2:false},"
		"\"set\":set[\"a\",2],"
		"\"blob\":bytes(\"AAEC/w==\"),"
		"\"time\":time(\"2026-07-31T04:34:56.12Z\"),"
		"\"nan\":float(\"nan\"),"
		"\"max\":18446744073709551615}",
		"compact XSON stringify mismatch"
	);
	pFloat = xrtValueFloat(INFINITY);
	testRequire(pFloat != NULL, "positive infinity fixture failed");
	sText = xrtXsonStringify(pFloat, false, &iSize);
	testXsonText(sText, iSize, "float(\"inf\")", "positive infinity mismatch");
	xrtValueRelease(pFloat);
	pFloat = xrtValueFloat(-INFINITY);
	testRequire(pFloat != NULL, "negative infinity fixture failed");
	sText = xrtXsonStringify(pFloat, false, &iSize);
	testXsonText(sText, iSize, "float(\"-inf\")", "negative infinity mismatch");
	xrtValueRelease(pFloat);
	xrtValueRelease(pRoot);
}



/* 验证 Base64 分块边界不插入填充，且不会建立第二份完整文本。 */
static void testXsonBytesChunking(void)
{
	bytes pData = (bytes)xrtMalloc(3073u);
	xvalue* pValue;
	str sBase64;
	str sText;
	size_t iSize = 0;
	size_t iBase64Size;

	testRequire(pData != NULL, "XSON byte fixture allocation failed");
	for ( size_t i = 0; i < 3073u; i++ ) {
		pData[i] = (uint8)((i * 37u) & 0xFFu);
	}
	sBase64 = xrtBase64EncodeNew(pData, 3073u, NULL);
	pValue = xrtValueBytes((xbytesview){ pData, 3073u });
	testRequire((sBase64 != NULL) && (pValue != NULL), "XSON byte fixture failed");
	iBase64Size = strlen(sBase64);
	sText = xrtXsonStringify(pValue, false, &iSize);
	testRequire(sText != NULL, "XSON chunked bytes write failed");
	testRequire(
		(iSize == (iBase64Size + 9u)) &&
		(memcmp(sText, "bytes(\"", 7u) == 0) &&
		(memcmp(sText + 7u, sBase64, iBase64Size) == 0) &&
		(memcmp(sText + 7u + iBase64Size, "\")", 3u) == 0),
		"XSON chunked Base64 output mismatch"
	);
	xrtFree(sText);
	xrtValueRelease(pValue);
	xrtFree(sBase64);
	xrtFree(pData);
}



/* 验证直接 writer 可逐层写出四类容器、标签和内建标量。 */
static void testXsonDirectWriter(void)
{
	static const uint8 arrBytes[] = { 1u, 2u };
	char arrLongTag[1024];
	xxsonwriteconfig Config;
	xxsonwriter* pWriter;
	str sText;
	size_t iSize = 0;

	xrtXsonWriteConfigInit(&Config);
	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(pWriter != NULL, "XSON direct writer create failed");
	testRequire(
		xrtXsonWriterObject(pWriter) &&
		xrtXsonWriterName(pWriter, XRT_STR_LITERAL("map")) &&
		xrtXsonWriterIntMap(pWriter) &&
		xrtXsonWriterKey(pWriter, -1) &&
		xrtXsonWriterSet(pWriter) &&
		xrtXsonWriterString(pWriter, XRT_STR_LITERAL("x")) &&
		xrtXsonWriterInt(pWriter, 2) &&
		xrtXsonWriterUInt(pWriter, UINT64_MAX) &&
		xrtXsonWriterEnd(pWriter) &&
		xrtXsonWriterEnd(pWriter) &&
		xrtXsonWriterName(pWriter, XRT_STR_LITERAL("array")) &&
		xrtXsonWriterArray(pWriter) &&
		xrtXsonWriterNull(pWriter) &&
		xrtXsonWriterBool(pWriter, true) &&
		xrtXsonWriterFloat(pWriter, INFINITY) &&
		xrtXsonWriterBytes(
			pWriter,
			(xbytesview){ arrBytes, sizeof(arrBytes) }
		) &&
		xrtXsonWriterTag(
			pWriter,
			XRT_STR_LITERAL("app.id"),
			XRT_STR_LITERAL("42")
		) &&
		xrtXsonWriterEnd(pWriter) &&
		xrtXsonWriterEnd(pWriter) &&
		xrtXsonWriterFinish(pWriter),
		"XSON direct writer sequence failed"
	);
	sText = xrtXsonWriterTake(pWriter, &iSize);
	testXsonText(
		sText,
		iSize,
		"{\"map\":intmap{-1:set[\"x\",2,18446744073709551615]},"
		"\"array\":[null,true,float(\"inf\"),bytes(\"AQI=\"),app.id(\"42\")]}",
		"XSON direct writer output mismatch"
	);
	xrtXsonWriterFree(pWriter);

	memset(arrLongTag, 'a', sizeof(arrLongTag));
	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) &&
		xrtXsonWriterTag(
			pWriter,
			(xstrview){ arrLongTag, sizeof(arrLongTag) },
			XRT_STR_LITERAL("x")
		) &&
		xrtXsonWriterFinish(pWriter),
		"XSON long direct tag failed"
	);
	sText = xrtXsonWriterTake(pWriter, &iSize);
	testRequire(
		(sText != NULL) &&
		(iSize == (sizeof(arrLongTag) + 5u)) &&
		(memcmp(sText, arrLongTag, sizeof(arrLongTag)) == 0) &&
		(memcmp(sText + sizeof(arrLongTag), "(\"x\")", 5u) == 0),
		"XSON long direct tag output mismatch"
	);
	xrtFree(sText);
	xrtXsonWriterFree(pWriter);

	Config.Flags |= XXSON_WRITE_PRETTY;
	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) &&
		xrtXsonWriterSet(pWriter) &&
		xrtXsonWriterInt(pWriter, 1) &&
		xrtXsonWriterEnd(pWriter) &&
		xrtXsonWriterFinish(pWriter),
		"pretty XSON writer failed"
	);
	sText = xrtXsonWriterTake(pWriter, &iSize);
	testXsonText(sText, iSize, "set[\n  1\n]", "pretty XSON mismatch");
	xrtXsonWriterFree(pWriter);
}



/* Sink 状态同时覆盖收集、主动失败和回调重入。 */
typedef struct testxsonsink {
	xbuffer Buffer;
	xxsonwriter* Writer;
	bool Fail;
	bool Reenter;
	bool SetError;
} testxsonsink;



/* 同步消费输出，并按测试配置注入失败或重入。 */
static bool testXsonSinkWrite(xbytesview Data, ptr pUserData)
{
	testxsonsink* pSink = (testxsonsink*)pUserData;

	if ( pSink->Reenter ) {
		pSink->Reenter = false;
		(void)xrtXsonWriterNull(pSink->Writer);
	}
	if ( pSink->Fail ) {
		if ( pSink->SetError ) {
			xerror* pError = xrtErrorCreate(
				XERR_VALUE,
				"test.xson.sink",
				88,
				"sink failure"
			);

			testRequire(pError != NULL, "XSON sink test error create failed");
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	return xrtBufferAppend(&pSink->Buffer, Data);
}



/* 验证 sink 输出上限、具体错误传播和不可重入合同。 */
static void testXsonSink(void)
{
	xxsonwriteconfig Config;
	testxsonsink Sink;
	xxsonwriter* pWriter;
	xvalue* pValue = xrtValueInt(1234);
	xerror* pStale;

	memset(&Sink, 0, sizeof(Sink));
	testRequire(xrtBufferInit(&Sink.Buffer), "XSON sink buffer init failed");
	xrtXsonWriteConfigInit(&Config);
	testRequire(
		xrtXsonWrite(pValue, &Config, testXsonSinkWrite, &Sink) &&
		(Sink.Buffer.Size == 4u) &&
		(memcmp(Sink.Buffer.Data, "1234", 4u) == 0),
		"XSON sink output mismatch"
	);
	xrtBufferClear(&Sink.Buffer);

	Config.MaxOutputBytes = 3u;
	xrtClearError();
	testRequire(
		!xrtXsonWrite(pValue, &Config, testXsonSinkWrite, &Sink) &&
		(Sink.Buffer.Size == 0),
		"XSON output limit was not atomic for one token"
	);
	testXsonWriteError(XXSON_ERROR_LIMIT, "XSON output limit error mismatch");

	xrtXsonWriteConfigInit(&Config);
	Sink.Fail = true;
	pStale = xrtErrorCreate(XERR_VALUE, "test.stale", 1, "stale error");
	testRequire(pStale != NULL, "XSON stale sink error create failed");
	xrtSetError(pStale);
	xrtErrorFree(pStale);
	testRequire(
		!xrtXsonWrite(pValue, &Config, testXsonSinkWrite, &Sink),
		"XSON sink failure was ignored"
	);
	testXsonWriteError(XXSON_ERROR_OUTPUT, "XSON sink failure error mismatch");

	Sink.SetError = true;
	testRequire(
		!xrtXsonWrite(pValue, &Config, testXsonSinkWrite, &Sink),
		"XSON sink specific failure was ignored"
	);
	testRequire(
		strcmp(xrtErrorDomain(xrtGetError()), "test.xson.sink") == 0,
		"XSON sink specific error was overwritten"
	);
	Sink.Fail = false;
	Sink.SetError = false;

	pWriter = xrtXsonWriterCreateSink(&Config, testXsonSinkWrite, &Sink);
	testRequire(pWriter != NULL, "XSON reentrant sink writer create failed");
	Sink.Writer = pWriter;
	Sink.Reenter = true;
	xrtClearError();
	testRequire(
		!xrtXsonWriterInt(pWriter, 1),
		"XSON sink callback reentry was accepted"
	);
	testXsonWriteError(XXSON_ERROR_STATE, "XSON sink reentry error mismatch");
	xrtXsonWriterFree(pWriter);

	xrtValueRelease(pValue);
	xrtBufferUnit(&Sink.Buffer);
}



/* 自定义编码器状态用于注入成功、失败、保留标签和重入。 */
typedef struct testxsonencoder {
	xxsonwriter* Writer;
	int Mode;
	int Calls;
} testxsonencoder;



/* 把 Pointer 映射为显式标签，并按模式验证错误传播边界。 */
static xxsoncoderesult testXsonEncode(
	const xvalue* pValue,
	xstrview* pTag,
	xstrview* pPayload,
	ptr pUserData
)
{
	testxsonencoder* pEncoder = (testxsonencoder*)pUserData;

	pEncoder->Calls++;
	if ( xrtValueType(pValue) != XVALUE_POINTER ) {
		return XXSON_CODE_UNSUPPORTED;
	}
	if ( pEncoder->Mode == 1 ) {
		*pTag = XRT_STR_LITERAL("bytes");
		*pPayload = XRT_STR_LITERAL("bad");
		return XXSON_CODE_OK;
	}
	if ( pEncoder->Mode == 2 ) {
		xerror* pError = xrtErrorCreate(
			XERR_VALUE,
			"test.xson.encode",
			91,
			"encode failure"
		);

		testRequire(pError != NULL, "XSON encoder error create failed");
		xrtSetError(pError);
		xrtErrorFree(pError);
		return XXSON_CODE_ERROR;
	}
	if ( pEncoder->Mode == 3 ) {
		return XXSON_CODE_UNSUPPORTED;
	}
	if ( pEncoder->Mode == 4 ) {
		return (xxsoncoderesult)99;
	}
	if ( pEncoder->Mode == 5 ) {
		(void)xrtXsonWriterNull(pEncoder->Writer);
	}
	*pTag = XRT_STR_LITERAL("app.ptr");
	*pPayload = XRT_STR_LITERAL("42");
	return XXSON_CODE_OK;
}



/* 验证不支持值策略和自定义编码器是唯一、受保护的扩展入口。 */
static void testXsonCustomWrite(void)
{
	int iTarget = 42;
	testxsonencoder Encoder;
	xxsonwriteconfig Config;
	xxsonwriter* pWriter;
	xvalue* pPointer = xrtValuePointer(&iTarget);
	xvalue* pArray = xrtValueArray();
	str sText;
	size_t iSize = 0;

	testRequire((pPointer != NULL) && (pArray != NULL), "XSON custom fixture failed");
	xrtXsonWriteConfigInit(&Config);
	testRequire(
		testXsonWriteText(pPointer, &Config, &iSize) == NULL,
		"XSON pointer should fail without encoder"
	);
	testXsonWriteError(XXSON_ERROR_UNSUPPORTED, "XSON pointer error mismatch");

	testRequire(
		xrtValueArrayAppend(pArray, pPointer) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(7)),
		"XSON skip fixture setup failed"
	);
	Config.Unsupported = XXSON_UNSUPPORTED_SKIP;
	sText = testXsonWriteText(pArray, &Config, &iSize);
	testXsonText(sText, iSize, "[7]", "XSON unsupported skip mismatch");
	testRequire(
		testXsonWriteText(pPointer, &Config, &iSize) == NULL,
		"XSON root pointer was skipped"
	);
	testXsonWriteError(XXSON_ERROR_UNSUPPORTED, "XSON root skip error mismatch");

	memset(&Encoder, 0, sizeof(Encoder));
	xrtXsonWriteConfigInit(&Config);
	Config.Encode = testXsonEncode;
	Config.EncodeData = &Encoder;
	sText = testXsonWriteText(pPointer, &Config, &iSize);
	testXsonText(sText, iSize, "app.ptr(\"42\")", "XSON custom encode mismatch");
	testRequire(Encoder.Calls == 1, "XSON custom encoder call count mismatch");

	Encoder.Mode = 1;
	testRequire(
		testXsonWriteText(pPointer, &Config, &iSize) == NULL,
		"XSON custom encoder used reserved tag"
	);
	testXsonWriteError(XXSON_ERROR_UNSUPPORTED, "XSON reserved tag error mismatch");

	Encoder.Mode = 2;
	testRequire(
		testXsonWriteText(pPointer, &Config, &iSize) == NULL,
		"XSON custom encoder failure was ignored"
	);
	testRequire(
		strcmp(xrtErrorDomain(xrtGetError()), "test.xson.encode") == 0,
		"XSON custom encoder error was overwritten"
	);

	Encoder.Mode = 3;
	testRequire(
		testXsonWriteText(pPointer, &Config, &iSize) == NULL,
		"XSON custom unsupported result was accepted"
	);
	testXsonWriteError(XXSON_ERROR_UNSUPPORTED, "XSON custom unsupported mismatch");

	Encoder.Mode = 4;
	testRequire(
		testXsonWriteText(pPointer, &Config, &iSize) == NULL,
		"XSON invalid custom result was accepted"
	);
	testXsonWriteError(XXSON_ERROR_STATE, "XSON invalid custom result mismatch");

	Encoder.Mode = 5;
	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(pWriter != NULL, "XSON custom reentry writer create failed");
	Encoder.Writer = pWriter;
	xrtClearError();
	testRequire(
		!xrtXsonWriterValue(pWriter, pPointer),
		"XSON custom encoder reentry was accepted"
	);
	testXsonWriteError(XXSON_ERROR_STATE, "XSON custom reentry error mismatch");
	xrtXsonWriterFree(pWriter);

	xrtValueRelease(pArray);
	xrtValueRelease(pPointer);
}



/* 验证配置、资源上限、标签和 writer 状态都失败关闭。 */
static void testXsonWriteState(void)
{
	static const char arrInvalidUtf8[] = { (char)0xC0, (char)0x80 };
	xxsonwriteconfig Config;
	xxsonwriter* pWriter;
	xvalue* pValue = xrtValueInt(12);

	memset(&Config, 0, sizeof(Config));
	testRequire(xrtXsonWriterCreate(&Config) == NULL, "zeroed XSON config was accepted");
	testXsonWriteError(XXSON_ERROR_CONFIG, "zeroed XSON config error mismatch");

	xrtXsonWriteConfigInit(&Config);
	Config.Flags = UINT32_C(0x80000000);
	testRequire(xrtXsonWriterCreate(&Config) == NULL, "unknown XSON flag was accepted");
	testXsonWriteError(XXSON_ERROR_CONFIG, "unknown XSON flag error mismatch");

	xrtXsonWriteConfigInit(&Config);
	Config.Reserved[0] = 1u;
	testRequire(xrtXsonWriterCreate(&Config) == NULL, "reserved XSON field was accepted");
	testXsonWriteError(XXSON_ERROR_CONFIG, "reserved XSON field error mismatch");

	xrtXsonWriteConfigInit(&Config);
	Config.MaxOutputBytes = 1u;
	testRequire(
		testXsonWriteText(pValue, &Config, NULL) == NULL,
		"XSON output limit was ignored"
	);
	testXsonWriteError(XXSON_ERROR_LIMIT, "XSON output limit mismatch");

	xrtXsonWriteConfigInit(&Config);
	Config.MaxDepth = 1u;
	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) && xrtXsonWriterArray(pWriter) &&
		!xrtXsonWriterArray(pWriter),
		"XSON depth limit was ignored"
	);
	testXsonWriteError(XXSON_ERROR_LIMIT, "XSON depth error mismatch");
	xrtXsonWriterFree(pWriter);

	xrtXsonWriteConfigInit(&Config);
	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) && xrtXsonWriterObject(pWriter) &&
		!xrtXsonWriterInt(pWriter, 1),
		"XSON object accepted a value without name"
	);
	testXsonWriteError(XXSON_ERROR_STATE, "XSON missing name error mismatch");
	xrtXsonWriterFree(pWriter);

	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) && xrtXsonWriterInt(pWriter, 1) &&
		!xrtXsonWriterInt(pWriter, 2),
		"XSON writer accepted a second root"
	);
	testXsonWriteError(XXSON_ERROR_STATE, "XSON second root error mismatch");
	xrtXsonWriterFree(pWriter);

	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) &&
		!xrtXsonWriterString(
			pWriter,
			(xstrview){ arrInvalidUtf8, sizeof(arrInvalidUtf8) }
		),
		"XSON writer accepted invalid UTF-8"
	);
	testXsonWriteError(XXSON_ERROR_UNSUPPORTED, "XSON UTF-8 error mismatch");
	xrtXsonWriterFree(pWriter);

	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) &&
		!xrtXsonWriterTag(
			pWriter,
			XRT_STR_LITERAL("time"),
			XRT_STR_LITERAL("x")
		),
		"XSON writer accepted reserved direct tag"
	);
	testXsonWriteError(XXSON_ERROR_UNSUPPORTED, "XSON direct tag error mismatch");
	xrtXsonWriterFree(pWriter);

	pWriter = xrtXsonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) && !xrtXsonWriterFinish(pWriter),
		"XSON writer finished without a root"
	);
	testXsonWriteError(XXSON_ERROR_STATE, "XSON empty writer error mismatch");
	xrtXsonWriterFree(pWriter);
	xrtValueRelease(pValue);
}



/* 运行 XSON Value 写出、直接写入、扩展和 sink 合同测试。 */
int main(void)
{
	testXsonStringify();
	testXsonBytesChunking();
	testXsonDirectWriter();
	testXsonSink();
	testXsonCustomWrite();
	testXsonWriteState();
	printf("[PASS] XSON write\n");
	return 0;
}
