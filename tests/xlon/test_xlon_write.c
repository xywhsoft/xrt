#include "../test.h"

#include <math.h>



/* 使用高级配置把 Value 写成内存文本。 */
static str testXlonWriteText(
	const xvalue* pValue,
	const xxlonwriteconfig* pConfig,
	size_t* pSize
)
{
	xxlonwriter* pWriter = xrtXlonWriterCreate(pConfig);
	str sText = NULL;

	if ( pWriter == NULL ) {
		return NULL;
	}
	if (
		xrtXlonWriterValue(pWriter, pValue) &&
		xrtXlonWriterFinish(pWriter)
	) {
		sText = xrtXlonWriterTake(pWriter, pSize);
	}
	xrtXlonWriterFree(pWriter);
	return sText;
}



/* 要求拥有字符串与预期字节完全一致并释放。 */
static void testXlonText(
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
			"[XLON text] expected=%s actual=%.*s\n",
			sExpected,
			(int)iSize,
			sText
		);
	}
	testRequire(iSize == iExpected, sMessage);
	testRequire(memcmp(sText, sExpected, iExpected + 1u) == 0, sMessage);
	xrtFree(sText);
}



/* 要求当前错误属于 XLON 域并具有指定错误码。 */
static void testXlonWriteError(xxlonerror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(strcmp(xrtErrorDomain(pError), "xrt.xlon") == 0, sMessage);
	testRequire(xrtErrorCode(pError) == (int64)Code, sMessage);
}



/* 构建覆盖全部可移植 XLON 类型的有序 Value 树。 */
static xvalue* testXlonBuildValue(void)
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
			XRT_STR_LITERAL("character"),
			xrtValueChar(UINT32_C(0x4F60))
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
static void testXlonStringify(void)
{
	xvalue* pRoot = testXlonBuildValue();
	xvalue* pFloat;
	str sText;
	size_t iSize = 0;

	testRequire(pRoot != NULL, "XLON stringify fixture failed");
	sText = xrtXlonStringify(pRoot, false, &iSize);
	testXlonText(
		sText,
		iSize,
		"{\"items\":[1,true],"
		"\"map\":intmap{-5:\"n\",2:false},"
		"\"set\":set[\"a\",2],"
		"\"blob\":bytes(\"AAEC/w==\"),"
		"\"character\":char(\"你\"),"
		"\"time\":time(\"2026-07-31T04:34:56.12Z\"),"
		"\"nan\":float(\"nan\"),"
		"\"max\":18446744073709551615}",
		"compact XLON stringify mismatch"
	);
	pFloat = xrtValueFloat(INFINITY);
	testRequire(pFloat != NULL, "positive infinity fixture failed");
	sText = xrtXlonStringify(pFloat, false, &iSize);
	testXlonText(sText, iSize, "float(\"inf\")", "positive infinity mismatch");
	xrtValueRelease(pFloat);
	pFloat = xrtValueFloat(-INFINITY);
	testRequire(pFloat != NULL, "negative infinity fixture failed");
	sText = xrtXlonStringify(pFloat, false, &iSize);
	testXlonText(sText, iSize, "float(\"-inf\")", "negative infinity mismatch");
	xrtValueRelease(pFloat);
	xrtValueRelease(pRoot);
}



/* 验证 Base64 分块边界不插入填充，且不会建立第二份完整文本。 */
static void testXlonBytesChunking(void)
{
	bytes pData = (bytes)xrtMalloc(3073u);
	xvalue* pValue;
	str sBase64;
	str sText;
	size_t iSize = 0;
	size_t iBase64Size;

	testRequire(pData != NULL, "XLON byte fixture allocation failed");
	for ( size_t i = 0; i < 3073u; i++ ) {
		pData[i] = (uint8)((i * 37u) & 0xFFu);
	}
	sBase64 = xrtBase64EncodeNew(pData, 3073u, NULL);
	pValue = xrtValueBytes((xbytesview){ pData, 3073u });
	testRequire((sBase64 != NULL) && (pValue != NULL), "XLON byte fixture failed");
	iBase64Size = strlen(sBase64);
	sText = xrtXlonStringify(pValue, false, &iSize);
	testRequire(sText != NULL, "XLON chunked bytes write failed");
	testRequire(
		(iSize == (iBase64Size + 9u)) &&
		(memcmp(sText, "bytes(\"", 7u) == 0) &&
		(memcmp(sText + 7u, sBase64, iBase64Size) == 0) &&
		(memcmp(sText + 7u + iBase64Size, "\")", 3u) == 0),
		"XLON chunked Base64 output mismatch"
	);
	xrtFree(sText);
	xrtValueRelease(pValue);
	xrtFree(sBase64);
	xrtFree(pData);
}



/* 验证直接 writer 可逐层写出四类容器、标签和内建标量。 */
static void testXlonDirectWriter(void)
{
	static const uint8 arrBytes[] = { 1u, 2u };
	char arrLongTag[1024];
	xxlonwriteconfig Config;
	xxlonwriter* pWriter;
	str sText;
	size_t iSize = 0;

	xrtXlonWriteConfigInit(&Config);
	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(pWriter != NULL, "XLON direct writer create failed");
	testRequire(
		xrtXlonWriterObject(pWriter) &&
		xrtXlonWriterName(pWriter, XRT_STR_LITERAL("map")) &&
		xrtXlonWriterIntMap(pWriter) &&
		xrtXlonWriterKey(pWriter, -1) &&
		xrtXlonWriterSet(pWriter) &&
		xrtXlonWriterString(pWriter, XRT_STR_LITERAL("x")) &&
		xrtXlonWriterInt(pWriter, 2) &&
		xrtXlonWriterUInt(pWriter, UINT64_MAX) &&
		xrtXlonWriterEnd(pWriter) &&
		xrtXlonWriterEnd(pWriter) &&
		xrtXlonWriterName(pWriter, XRT_STR_LITERAL("array")) &&
		xrtXlonWriterArray(pWriter) &&
		xrtXlonWriterNull(pWriter) &&
		xrtXlonWriterBool(pWriter, true) &&
		xrtXlonWriterFloat(pWriter, INFINITY) &&
		xrtXlonWriterBytes(
			pWriter,
			(xbytesview){ arrBytes, sizeof(arrBytes) }
		) &&
		xrtXlonWriterTag(
			pWriter,
			XRT_STR_LITERAL("app.id"),
			XRT_STR_LITERAL("42")
		) &&
		xrtXlonWriterEnd(pWriter) &&
		xrtXlonWriterEnd(pWriter) &&
		xrtXlonWriterFinish(pWriter),
		"XLON direct writer sequence failed"
	);
	sText = xrtXlonWriterTake(pWriter, &iSize);
	testXlonText(
		sText,
		iSize,
		"{\"map\":intmap{-1:set[\"x\",2,18446744073709551615]},"
		"\"array\":[null,true,float(\"inf\"),bytes(\"AQI=\"),app.id(\"42\")]}",
		"XLON direct writer output mismatch"
	);
	xrtXlonWriterFree(pWriter);

	memset(arrLongTag, 'a', sizeof(arrLongTag));
	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) &&
		xrtXlonWriterTag(
			pWriter,
			(xstrview){ arrLongTag, sizeof(arrLongTag) },
			XRT_STR_LITERAL("x")
		) &&
		xrtXlonWriterFinish(pWriter),
		"XLON long direct tag failed"
	);
	sText = xrtXlonWriterTake(pWriter, &iSize);
	testRequire(
		(sText != NULL) &&
		(iSize == (sizeof(arrLongTag) + 5u)) &&
		(memcmp(sText, arrLongTag, sizeof(arrLongTag)) == 0) &&
		(memcmp(sText + sizeof(arrLongTag), "(\"x\")", 5u) == 0),
		"XLON long direct tag output mismatch"
	);
	xrtFree(sText);
	xrtXlonWriterFree(pWriter);

	Config.Flags |= XXLON_WRITE_PRETTY;
	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) &&
		xrtXlonWriterSet(pWriter) &&
		xrtXlonWriterInt(pWriter, 1) &&
		xrtXlonWriterEnd(pWriter) &&
		xrtXlonWriterFinish(pWriter),
		"pretty XLON writer failed"
	);
	sText = xrtXlonWriterTake(pWriter, &iSize);
	testXlonText(sText, iSize, "set[\n  1\n]", "pretty XLON mismatch");
	xrtXlonWriterFree(pWriter);
}



/* Sink 状态同时覆盖收集、主动失败和回调重入。 */
typedef struct testxlonsink {
	xbuffer Buffer;
	xxlonwriter* Writer;
	bool Fail;
	bool Reenter;
	bool SetError;
} testxlonsink;



/* 同步消费输出，并按测试配置注入失败或重入。 */
static bool testXlonSinkWrite(xbytesview Data, ptr pUserData)
{
	testxlonsink* pSink = (testxlonsink*)pUserData;

	if ( pSink->Reenter ) {
		pSink->Reenter = false;
		(void)xrtXlonWriterNull(pSink->Writer);
	}
	if ( pSink->Fail ) {
		if ( pSink->SetError ) {
			xerror* pError = xrtErrorCreate(
				XERR_VALUE,
				"test.xlon.sink",
				88,
				"sink failure"
			);

			testRequire(pError != NULL, "XLON sink test error create failed");
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	return xrtBufferAppend(&pSink->Buffer, Data);
}



/* 验证 sink 输出上限、具体错误传播和不可重入合同。 */
static void testXlonSink(void)
{
	xxlonwriteconfig Config;
	testxlonsink Sink;
	xxlonwriter* pWriter;
	xvalue* pValue = xrtValueInt(1234);
	xerror* pStale;

	memset(&Sink, 0, sizeof(Sink));
	testRequire(xrtBufferInit(&Sink.Buffer), "XLON sink buffer init failed");
	xrtXlonWriteConfigInit(&Config);
	testRequire(
		xrtXlonWrite(pValue, &Config, testXlonSinkWrite, &Sink) &&
		(Sink.Buffer.Size == 4u) &&
		(memcmp(Sink.Buffer.Data, "1234", 4u) == 0),
		"XLON sink output mismatch"
	);
	xrtBufferClear(&Sink.Buffer);

	Config.MaxOutputBytes = 3u;
	xrtClearError();
	testRequire(
		!xrtXlonWrite(pValue, &Config, testXlonSinkWrite, &Sink) &&
		(Sink.Buffer.Size == 0),
		"XLON output limit was not atomic for one token"
	);
	testXlonWriteError(XXLON_ERROR_LIMIT, "XLON output limit error mismatch");

	xrtXlonWriteConfigInit(&Config);
	Sink.Fail = true;
	pStale = xrtErrorCreate(XERR_VALUE, "test.stale", 1, "stale error");
	testRequire(pStale != NULL, "XLON stale sink error create failed");
	xrtSetError(pStale);
	xrtErrorFree(pStale);
	testRequire(
		!xrtXlonWrite(pValue, &Config, testXlonSinkWrite, &Sink),
		"XLON sink failure was ignored"
	);
	testXlonWriteError(XXLON_ERROR_OUTPUT, "XLON sink failure error mismatch");

	Sink.SetError = true;
	testRequire(
		!xrtXlonWrite(pValue, &Config, testXlonSinkWrite, &Sink),
		"XLON sink specific failure was ignored"
	);
	testRequire(
		strcmp(xrtErrorDomain(xrtGetError()), "test.xlon.sink") == 0,
		"XLON sink specific error was overwritten"
	);
	Sink.Fail = false;
	Sink.SetError = false;

	pWriter = xrtXlonWriterCreateSink(&Config, testXlonSinkWrite, &Sink);
	testRequire(pWriter != NULL, "XLON reentrant sink writer create failed");
	Sink.Writer = pWriter;
	Sink.Reenter = true;
	xrtClearError();
	testRequire(
		!xrtXlonWriterInt(pWriter, 1),
		"XLON sink callback reentry was accepted"
	);
	testXlonWriteError(XXLON_ERROR_STATE, "XLON sink reentry error mismatch");
	xrtXlonWriterFree(pWriter);

	xrtValueRelease(pValue);
	xrtBufferUnit(&Sink.Buffer);
}



/* 自定义编码器状态用于注入成功、失败、保留标签和重入。 */
typedef struct testxlonencoder {
	xxlonwriter* Writer;
	int Mode;
	int Calls;
} testxlonencoder;



/* 把 Pointer 映射为显式标签，并按模式验证错误传播边界。 */
static xxloncoderesult testXlonEncode(
	const xvalue* pValue,
	xstrview* pTag,
	xstrview* pPayload,
	ptr pUserData
)
{
	testxlonencoder* pEncoder = (testxlonencoder*)pUserData;

	pEncoder->Calls++;
	if ( xrtValueType(pValue) != XVALUE_POINTER ) {
		return XXLON_CODE_UNSUPPORTED;
	}
	if ( pEncoder->Mode == 1 ) {
		*pTag = XRT_STR_LITERAL("bytes");
		*pPayload = XRT_STR_LITERAL("bad");
		return XXLON_CODE_OK;
	}
	if ( pEncoder->Mode == 2 ) {
		xerror* pError = xrtErrorCreate(
			XERR_VALUE,
			"test.xlon.encode",
			91,
			"encode failure"
		);

		testRequire(pError != NULL, "XLON encoder error create failed");
		xrtSetError(pError);
		xrtErrorFree(pError);
		return XXLON_CODE_ERROR;
	}
	if ( pEncoder->Mode == 3 ) {
		return XXLON_CODE_UNSUPPORTED;
	}
	if ( pEncoder->Mode == 4 ) {
		return (xxloncoderesult)99;
	}
	if ( pEncoder->Mode == 5 ) {
		(void)xrtXlonWriterNull(pEncoder->Writer);
	}
	*pTag = XRT_STR_LITERAL("app.ptr");
	*pPayload = XRT_STR_LITERAL("42");
	return XXLON_CODE_OK;
}



/* 验证不支持值策略和自定义编码器是唯一、受保护的扩展入口。 */
static void testXlonCustomWrite(void)
{
	int iTarget = 42;
	testxlonencoder Encoder;
	xxlonwriteconfig Config;
	xxlonwriter* pWriter;
	xvalue* pPointer = xrtValuePointer(&iTarget);
	xvalue* pArray = xrtValueArray();
	str sText;
	size_t iSize = 0;

	testRequire((pPointer != NULL) && (pArray != NULL), "XLON custom fixture failed");
	xrtXlonWriteConfigInit(&Config);
	testRequire(
		testXlonWriteText(pPointer, &Config, &iSize) == NULL,
		"XLON pointer should fail without encoder"
	);
	testXlonWriteError(XXLON_ERROR_UNSUPPORTED, "XLON pointer error mismatch");

	testRequire(
		xrtValueArrayAppend(pArray, pPointer) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(7)),
		"XLON skip fixture setup failed"
	);
	Config.Unsupported = XXLON_UNSUPPORTED_SKIP;
	sText = testXlonWriteText(pArray, &Config, &iSize);
	testXlonText(sText, iSize, "[7]", "XLON unsupported skip mismatch");
	testRequire(
		testXlonWriteText(pPointer, &Config, &iSize) == NULL,
		"XLON root pointer was skipped"
	);
	testXlonWriteError(XXLON_ERROR_UNSUPPORTED, "XLON root skip error mismatch");

	memset(&Encoder, 0, sizeof(Encoder));
	xrtXlonWriteConfigInit(&Config);
	Config.Encode = testXlonEncode;
	Config.EncodeData = &Encoder;
	sText = testXlonWriteText(pPointer, &Config, &iSize);
	testXlonText(sText, iSize, "app.ptr(\"42\")", "XLON custom encode mismatch");
	testRequire(Encoder.Calls == 1, "XLON custom encoder call count mismatch");

	Encoder.Mode = 1;
	testRequire(
		testXlonWriteText(pPointer, &Config, &iSize) == NULL,
		"XLON custom encoder used reserved tag"
	);
	testXlonWriteError(XXLON_ERROR_UNSUPPORTED, "XLON reserved tag error mismatch");

	Encoder.Mode = 2;
	testRequire(
		testXlonWriteText(pPointer, &Config, &iSize) == NULL,
		"XLON custom encoder failure was ignored"
	);
	testRequire(
		strcmp(xrtErrorDomain(xrtGetError()), "test.xlon.encode") == 0,
		"XLON custom encoder error was overwritten"
	);

	Encoder.Mode = 3;
	testRequire(
		testXlonWriteText(pPointer, &Config, &iSize) == NULL,
		"XLON custom unsupported result was accepted"
	);
	testXlonWriteError(XXLON_ERROR_UNSUPPORTED, "XLON custom unsupported mismatch");

	Encoder.Mode = 4;
	testRequire(
		testXlonWriteText(pPointer, &Config, &iSize) == NULL,
		"XLON invalid custom result was accepted"
	);
	testXlonWriteError(XXLON_ERROR_STATE, "XLON invalid custom result mismatch");

	Encoder.Mode = 5;
	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(pWriter != NULL, "XLON custom reentry writer create failed");
	Encoder.Writer = pWriter;
	xrtClearError();
	testRequire(
		!xrtXlonWriterValue(pWriter, pPointer),
		"XLON custom encoder reentry was accepted"
	);
	testXlonWriteError(XXLON_ERROR_STATE, "XLON custom reentry error mismatch");
	xrtXlonWriterFree(pWriter);

	xrtValueRelease(pArray);
	xrtValueRelease(pPointer);
}



/* 验证配置、资源上限、标签和 writer 状态都失败关闭。 */
static void testXlonWriteState(void)
{
	static const char arrInvalidUtf8[] = { (char)0xC0, (char)0x80 };
	xxlonwriteconfig Config;
	xxlonwriter* pWriter;
	xvalue* pValue = xrtValueInt(12);

	memset(&Config, 0, sizeof(Config));
	testRequire(xrtXlonWriterCreate(&Config) == NULL, "zeroed XLON config was accepted");
	testXlonWriteError(XXLON_ERROR_CONFIG, "zeroed XLON config error mismatch");

	xrtXlonWriteConfigInit(&Config);
	Config.Flags = UINT32_C(0x80000000);
	testRequire(xrtXlonWriterCreate(&Config) == NULL, "unknown XLON flag was accepted");
	testXlonWriteError(XXLON_ERROR_CONFIG, "unknown XLON flag error mismatch");

	xrtXlonWriteConfigInit(&Config);
	Config.Reserved[0] = 1u;
	testRequire(xrtXlonWriterCreate(&Config) == NULL, "reserved XLON field was accepted");
	testXlonWriteError(XXLON_ERROR_CONFIG, "reserved XLON field error mismatch");

	xrtXlonWriteConfigInit(&Config);
	Config.MaxOutputBytes = 1u;
	testRequire(
		testXlonWriteText(pValue, &Config, NULL) == NULL,
		"XLON output limit was ignored"
	);
	testXlonWriteError(XXLON_ERROR_LIMIT, "XLON output limit mismatch");

	xrtXlonWriteConfigInit(&Config);
	Config.MaxDepth = 1u;
	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) && xrtXlonWriterArray(pWriter) &&
		!xrtXlonWriterArray(pWriter),
		"XLON depth limit was ignored"
	);
	testXlonWriteError(XXLON_ERROR_LIMIT, "XLON depth error mismatch");
	xrtXlonWriterFree(pWriter);

	xrtXlonWriteConfigInit(&Config);
	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) && xrtXlonWriterObject(pWriter) &&
		!xrtXlonWriterInt(pWriter, 1),
		"XLON object accepted a value without name"
	);
	testXlonWriteError(XXLON_ERROR_STATE, "XLON missing name error mismatch");
	xrtXlonWriterFree(pWriter);

	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) && xrtXlonWriterInt(pWriter, 1) &&
		!xrtXlonWriterInt(pWriter, 2),
		"XLON writer accepted a second root"
	);
	testXlonWriteError(XXLON_ERROR_STATE, "XLON second root error mismatch");
	xrtXlonWriterFree(pWriter);

	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) &&
		!xrtXlonWriterString(
			pWriter,
			(xstrview){ arrInvalidUtf8, sizeof(arrInvalidUtf8) }
		),
		"XLON writer accepted invalid UTF-8"
	);
	testXlonWriteError(XXLON_ERROR_UNSUPPORTED, "XLON UTF-8 error mismatch");
	xrtXlonWriterFree(pWriter);

	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) &&
		!xrtXlonWriterTag(
			pWriter,
			XRT_STR_LITERAL("time"),
			XRT_STR_LITERAL("x")
		),
		"XLON writer accepted reserved direct tag"
	);
	testXlonWriteError(XXLON_ERROR_UNSUPPORTED, "XLON direct tag error mismatch");
	xrtXlonWriterFree(pWriter);

	pWriter = xrtXlonWriterCreate(&Config);
	testRequire(
		(pWriter != NULL) && !xrtXlonWriterFinish(pWriter),
		"XLON writer finished without a root"
	);
	testXlonWriteError(XXLON_ERROR_STATE, "XLON empty writer error mismatch");
	xrtXlonWriterFree(pWriter);
	xrtValueRelease(pValue);
}



/* 运行 XLON Value 写出、直接写入、扩展和 sink 合同测试。 */
int main(void)
{
	testXlonStringify();
	testXlonBytesChunking();
	testXlonDirectWriter();
	testXlonSink();
	testXlonCustomWrite();
	testXlonWriteState();
	printf("[PASS] XLON write\n");
	return 0;
}
