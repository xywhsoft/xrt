#include "../test.h"



/* 按长度比较借用字段值与字面量。 */
static bool testHttpHeadersText(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证公开配置校验器接受默认配置并拒绝倒置容量与物理溢出。 */
static void testHttpHeadersConfig(void)
{
	xhttpheadersconfig Config;

	xrtHttpHeadersConfigInit(&Config);
	testRequire(xrtHttpHeadersConfigValid(&Config),
		"HTTP Headers default config was rejected");

	Config.InitialFields = Config.MaxFields + 1u;
	testRequire(!xrtHttpHeadersConfigValid(&Config) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Headers accepted initial fields above the limit");
	xrtClearError();

	xrtHttpHeadersConfigInit(&Config);
	Config.InitialBytes = Config.MaxBytes + 1u;
	testRequire(!xrtHttpHeadersConfigValid(&Config) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Headers accepted initial bytes above the limit");
	xrtClearError();

	testRequire(!xrtHttpHeadersConfigValid(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Headers accepted a missing config");
	xrtClearError();
}



/* 验证配置与结构化输出支持未对齐存储并拒绝所有权覆盖和回绕范围。 */
static void testHttpHeadersMemoryContracts(void)
{
	uint8 ConfigStorage[sizeof(xhttpheadersconfig) + 2u];
	uint8 PointerStorage[sizeof(const xhttpfield*) + 2u];
	uint8 ValueStorage[sizeof(xstrview) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 Output[64];
	xhttpheadersconfig Config;
	xhttpheaders* pHeaders;
	const xhttpfield* pField;
	xstrview Value;
	str sBlock;
	size_t iSize;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpHeadersConfigInit(
		(xhttpheadersconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire((ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(Config.InitialFields == 8u) &&
		(Config.InitialBytes == 512u) &&
		(Config.MaxFields == 1024u),
		"HTTP Headers config init did not support unaligned storage");
	Config.InitialFields = 0u;
	Config.InitialBytes = 0u;
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	pHeaders = xrtHttpHeadersCreate(
		(const xhttpheadersconfig*)(const void*)(ConfigStorage + 1u)
	);
	testRequire((pHeaders != NULL) && xrtHttpHeadersAdd(
		pHeaders, XRT_STR_LITERAL("A"), XRT_STR_LITERAL("1")
	), "HTTP Headers did not snapshot unaligned config");

	testRequire(xrtHttpHeadersGetUnique(
		pHeaders,
		XRT_STR_LITERAL("A"),
		(const xhttpfield**)(void*)(PointerStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP Headers unique lookup rejected unaligned output");
	memcpy(&pField, PointerStorage + 1u, sizeof(pField));
	testRequire((pField != NULL) &&
		testHttpHeadersText(pField->Value, "1"),
		"HTTP Headers unique lookup published wrong output");
	testRequire(xrtHttpHeadersGetAll(
		pHeaders,
		XRT_STR_LITERAL("A"),
		(xstrview*)(void*)(ValueStorage + 1u),
		1u
	) == 1u, "HTTP Headers GetAll rejected unaligned output");
	memcpy(&Value, ValueStorage + 1u, sizeof(Value));
	testRequire(testHttpHeadersText(Value, "1"),
		"HTTP Headers GetAll published wrong output");

	testRequire(xrtHttpHeadersAddBlock(
		pHeaders,
		XRT_STR_LITERAL("B: 2\r\n"),
		(size_t*)(void*)(SizeStorage + 1u)
	), "HTTP Headers AddBlock rejected unaligned error output");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == 0u) &&
		(xrtHttpHeadersCount(pHeaders) == 2u),
		"HTTP Headers AddBlock published wrong success offset");
	testRequire(!xrtHttpHeadersAddBlock(
		pHeaders,
		XRT_STR_LITERAL("Bad\n"),
		(size_t*)(void*)(SizeStorage + 1u)
	), "HTTP Headers AddBlock accepted a bare LF");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == 3u) &&
		(xrtHttpHeadersCount(pHeaders) == 2u),
		"HTTP Headers failed block changed state or offset");
	xrtClearError();

	testRequire(xrtHttpHeadersWrite(
		pHeaders,
		NULL,
		0u,
		(size_t*)(void*)(SizeStorage + 1u)
	), "HTTP Headers Write rejected unaligned size output");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire(iSize == strlen("A: 1\r\nB: 2\r\n\r\n"),
		"HTTP Headers Write published wrong unaligned size");
	sBlock = xrtHttpHeadersBuild(
		pHeaders,
		(size_t*)(void*)(SizeStorage + 1u)
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((sBlock != NULL) &&
		(iSize == strlen("A: 1\r\nB: 2\r\n\r\n")),
		"HTTP Headers Build rejected unaligned size output");
	xrtFree(sBlock);

	pField = xrtHttpHeadersGet(
		pHeaders, XRT_STR_LITERAL("A")
	);
	testRequire(pField != NULL,
		"HTTP Headers ownership output fixture was lost");
	testRequire(xrtHttpHeadersGetAll(
		pHeaders,
		XRT_STR_LITERAL("A"),
		(xstrview*)(void*)pField->Name.Data,
		1u
	) == 0u &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		testHttpHeadersText(pField->Name, "A"),
		"HTTP Headers GetAll accepted owned storage output");
	xrtClearError();
	testRequire(xrtHttpHeadersGetUnique(
		pHeaders,
		XRT_STR_LITERAL("A"),
		(const xhttpfield**)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_NEXT_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Headers unique lookup accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpHeadersAddBlock(
		pHeaders,
		XRT_STR_LITERAL("C: 3\r\n"),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtHttpHeadersCount(pHeaders) == 2u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Headers AddBlock accepted wrapping error output");
	xrtClearError();
	testRequire(!xrtHttpHeadersWrite(
		pHeaders,
		NULL,
		0u,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Headers Write accepted wrapping size output");
	xrtClearError();
	memset(Output, 0xA5, sizeof(Output));
	testRequire(!xrtHttpHeadersWrite(
		pHeaders,
		Output,
		sizeof(Output),
		(size_t*)(void*)(Output + 1u)
	) && (Output[0] == 0xA5) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Headers Write accepted overlapping outputs");
	xrtClearError();
	xrtHttpHeadersDestroy(pHeaders);

	xrtHttpHeadersConfigInit((xhttpheadersconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP Headers config init accepted wrapping output");
	xrtClearError();
	testRequire(xrtHttpHeadersCreate(
		(const xhttpheadersconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Headers create accepted wrapping config");
	xrtClearError();
}



/* 验证重复字段、稳定顺序、拥有语义以及增删改查。 */
static void testHttpHeadersOperations(void)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	xhttpheaders* pClone;
	const xhttpfield* pField;
	xstrview AliasName;
	xstrview AliasValue;
	xstrview Values[1];
	size_t iBytes;

	testRequire(pHeaders != NULL, "HTTP Headers create failed");
	testRequire(xrtHttpHeadersAdd(
		pHeaders,
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("text/plain")
	), "HTTP Headers add failed");
	testRequire(xrtHttpHeadersAdd(
		pHeaders,
		XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL("a=1")
	) && xrtHttpHeadersAdd(
		pHeaders,
		XRT_STR_LITERAL("set-cookie"),
		XRT_STR_LITERAL("b=2")
	) && xrtHttpHeadersAdd(
		pHeaders,
		XRT_STR_LITERAL("X-Empty"),
		(xstrview){ NULL, 0 }
	), "HTTP Headers duplicate add failed");
	iBytes = strlen("Content-Type") + strlen("text/plain") +
		strlen("Set-Cookie") + strlen("a=1") +
		strlen("set-cookie") + strlen("b=2") +
		strlen("X-Empty");
	testRequire((xrtHttpHeadersCount(pHeaders) == 4) &&
		(xrtHttpHeadersBytes(pHeaders) == iBytes) &&
		(xrtHttpHeadersData(pHeaders) != NULL),
		"HTTP Headers counters mismatch");
	pField = xrtHttpHeadersGet(
		pHeaders, XRT_STR_LITERAL("CONTENT-TYPE")
	);
	testRequire((pField == xrtHttpHeadersAt(pHeaders, 0)) &&
		testHttpHeadersText(pField->Value, "text/plain") &&
		(pField->Name.Data[pField->Name.Size] == '\0') &&
		(pField->Value.Data[pField->Value.Size] == '\0'),
		"HTTP Headers owned field mismatch");
	testRequire(
		xrtHttpHeadersGetUnique(
			pHeaders,
			XRT_STR_LITERAL("Content-Type"),
			&pField
		) == XHTTP_NEXT_ITEM &&
		testHttpHeadersText(pField->Value, "text/plain"),
		"HTTP Headers unique lookup mismatch"
	);
	testRequire(
		xrtHttpHeadersGetUnique(
			pHeaders,
			XRT_STR_LITERAL("Set-Cookie"),
			&pField
		) == XHTTP_NEXT_ERROR &&
		(pField == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Headers duplicate unique lookup succeeded"
	);
	xrtClearError();
	pField = (const xhttpfield*)pHeaders;
	testRequire(
		xrtHttpHeadersGetUnique(
			pHeaders,
			XRT_STR_LITERAL("Missing"),
			&pField
		) == XHTTP_NEXT_END &&
		(pField == NULL) &&
		(xrtGetError() == NULL),
		"HTTP Headers missing unique lookup mismatch"
	);
	pField = xrtHttpHeadersGet(
		pHeaders,
		XRT_STR_LITERAL("Content-Type")
	);
	testRequire((xrtHttpHeadersCountName(
		pHeaders, XRT_STR_LITERAL("Set-Cookie")
	) == 2) && xrtHttpHeadersHas(
		pHeaders, XRT_STR_LITERAL("set-cookie")
	) && testHttpHeadersText(xrtHttpHeadersGetNth(
		pHeaders, XRT_STR_LITERAL("set-cookie"), 1
	)->Value, "b=2"), "HTTP Headers duplicate lookup mismatch");
	testRequire((xrtHttpHeadersGetAll(
		pHeaders,
		XRT_STR_LITERAL("set-cookie"),
		Values,
		1
	) == 2) && testHttpHeadersText(Values[0], "a=1"),
		"HTTP Headers GetAll query or bounded output mismatch");

	AliasName = pField->Name;
	AliasValue = pField->Value;
	testRequire(xrtHttpHeadersAdd(
		pHeaders, AliasName, AliasValue
	), "HTTP Headers alias add failed");
	testRequire(testHttpHeadersText(
		xrtHttpHeadersAt(pHeaders, 4)->Value, "text/plain"
	), "HTTP Headers alias add changed input");
	pField = xrtHttpHeadersGetNth(
		pHeaders, XRT_STR_LITERAL("Set-Cookie"), 1
	);
	AliasValue = pField->Value;
	testRequire(xrtHttpHeadersSet(
		pHeaders, XRT_STR_LITERAL("Set-Cookie"), AliasValue
	), "HTTP Headers alias set failed");
	testRequire((xrtHttpHeadersCountName(
		pHeaders, XRT_STR_LITERAL("set-cookie")
	) == 1) && testHttpHeadersText(xrtHttpHeadersGet(
		pHeaders, XRT_STR_LITERAL("set-cookie")
	)->Value, "b=2") && testHttpHeadersText(
		xrtHttpHeadersAt(pHeaders, 1)->Name, "Set-Cookie"
	), "HTTP Headers set did not collapse duplicates in place");
	testRequire(xrtHttpHeadersRemove(
		pHeaders, XRT_STR_LITERAL("X-Empty")
	) == 1, "HTTP Headers remove count mismatch");
	testRequire(xrtHttpHeadersCompact(pHeaders),
		"HTTP Headers compact failed");
	pClone = xrtHttpHeadersClone(pHeaders);
	testRequire(pClone != NULL, "HTTP Headers clone failed");
	testRequire(xrtHttpHeadersSet(
		pClone,
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("application/json")
	), "HTTP Headers clone mutation failed");
	testRequire(testHttpHeadersText(xrtHttpHeadersGet(
		pHeaders, XRT_STR_LITERAL("Content-Type")
	)->Value, "text/plain") && testHttpHeadersText(xrtHttpHeadersGet(
		pClone, XRT_STR_LITERAL("Content-Type")
	)->Value, "application/json"),
		"HTTP Headers clone shared mutable storage");
	xrtHttpHeadersClear(pClone);
	testRequire((xrtHttpHeadersCount(pClone) == 0) &&
		(xrtHttpHeadersBytes(pClone) == 0) &&
		(xrtHttpHeadersData(pClone) == NULL),
		"HTTP Headers clear left visible content");
	xrtHttpHeadersDestroy(pClone);
	xrtHttpHeadersDestroy(pHeaders);
}



/* 验证字段块解析、事务追加、规范写出与分配型构建。 */
static void testHttpHeadersBlock(void)
{
	static const char Input[] =
		"Warning: one\r\nWarning:\ttwo \t\r\nX-Last: value";
	static const char Expected[] =
		"Warning: one\r\nWarning: two\r\nX-Last: value\r\n\r\n";
	xhttpheaders* pHeaders;
	str sBuilt;
	size_t iBefore;
	size_t iOffset = SIZE_MAX;
	size_t iSize = 0;

	pHeaders = xrtHttpHeadersParse(
		(xstrview){ Input, sizeof(Input) - 1u },
		NULL,
		&iOffset
	);
	testRequire((pHeaders != NULL) && (iOffset == 0) &&
		(xrtHttpHeadersCount(pHeaders) == 3) &&
		testHttpHeadersText(xrtHttpHeadersGetNth(
			pHeaders, XRT_STR_LITERAL("warning"), 1
		)->Value, "two"), "HTTP Headers block parse mismatch");
	sBuilt = xrtHttpHeadersBuild(pHeaders, &iSize);
	testRequire((sBuilt != NULL) &&
		(iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(sBuilt, Expected, iSize + 1u) == 0),
		"HTTP Headers allocated build mismatch");
	xrtFree(sBuilt);
	testRequire(xrtHttpHeadersAddBlock(
		pHeaders,
		XRT_STR_LITERAL("Tail: yes\r\n\r\n"),
		&iOffset
	) && (xrtHttpHeadersCount(pHeaders) == 4) &&
		testHttpHeadersText(xrtHttpHeadersGet(
			pHeaders, XRT_STR_LITERAL("tail")
		)->Value, "yes"), "HTTP Headers block append mismatch");
	iBefore = xrtHttpHeadersCount(pHeaders);
	testRequire(!xrtHttpHeadersAddBlock(
		pHeaders,
		XRT_STR_LITERAL("Good: yes\r\nBad: no\n"),
		&iOffset
	) && (iOffset == 18) &&
		(xrtHttpHeadersCount(pHeaders) == iBefore) &&
		!xrtHttpHeadersHas(pHeaders, XRT_STR_LITERAL("Good")),
		"HTTP Headers invalid block exposed partial append");
	xrtClearError();
	testRequire(!xrtHttpHeadersAddBlock(
		pHeaders,
		XRT_STR_LITERAL("\r\nAfter: no\r\n"),
		&iOffset
	) && (iOffset == 2) &&
		(xrtHttpHeadersCount(pHeaders) == iBefore),
		"HTTP Headers accepted bytes after the final empty line");
	xrtClearError();
	xrtHttpHeadersDestroy(pHeaders);
}



/* 验证默认实现不再给单字段附加旧版短缓冲限制。 */
static void testHttpHeadersLongValue(void)
{
	char Value[20000];
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	const xhttpfield* pField;

	testRequire(pHeaders != NULL, "HTTP Headers long fixture create failed");
	memset(Value, 'v', sizeof(Value));
	testRequire(xrtHttpHeadersAdd(
		pHeaders,
		XRT_STR_LITERAL("X-Long"),
		(xstrview){ Value, sizeof(Value) }
	), "HTTP Headers rejected a value within the configured total limit");
	pField = xrtHttpHeadersGet(pHeaders, XRT_STR_LITERAL("x-long"));
	testRequire((pField != NULL) && (pField->Value.Size == sizeof(Value)) &&
		(memcmp(pField->Value.Data, Value, sizeof(Value)) == 0),
		"HTTP Headers long value copy mismatch");
	xrtHttpHeadersDestroy(pHeaders);
}



/* 验证字段数、名称、值和总字节限额彼此独立且失败不修改内容。 */
static void testHttpHeadersLimits(void)
{
	xhttpheadersconfig Config;
	xhttpheaders* pHeaders;
	size_t iCount;
	size_t iBytes;

	xrtHttpHeadersConfigInit(&Config);
	Config.InitialFields = 0;
	Config.InitialBytes = 0;
	Config.MaxFields = 2;
	Config.MaxName = 4;
	Config.MaxValue = 5;
	Config.MaxBytes = 8;
	pHeaders = xrtHttpHeadersCreate(&Config);
	testRequire(pHeaders != NULL, "HTTP Headers limited create failed");
	testRequire(xrtHttpHeadersAdd(
		pHeaders, XRT_STR_LITERAL("A"), XRT_STR_LITERAL("12345")
	) && xrtHttpHeadersAdd(
		pHeaders, XRT_STR_LITERAL("B"), (xstrview){ NULL, 0 }
	), "HTTP Headers limited fixture fill failed");
	iCount = xrtHttpHeadersCount(pHeaders);
	iBytes = xrtHttpHeadersBytes(pHeaders);
	testRequire(!xrtHttpHeadersAdd(
		pHeaders, XRT_STR_LITERAL("C"), (xstrview){ NULL, 0 }
	) && (xrtHttpHeadersCount(pHeaders) == iCount) &&
		(xrtHttpHeadersBytes(pHeaders) == iBytes),
		"HTTP Headers field limit changed state");
	xrtClearError();
	testRequire(!xrtHttpHeadersSet(
		pHeaders, XRT_STR_LITERAL("C"), (xstrview){ NULL, 0 }
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtHttpHeadersCount(pHeaders) == iCount),
		"HTTP Headers absent Set ignored the field limit");
	xrtClearError();
	testRequire(!xrtHttpHeadersAdd(
		pHeaders, XRT_STR_LITERAL("Bad Name"), XRT_STR_LITERAL("x")
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtHttpHeadersCount(pHeaders) == iCount),
		"HTTP Headers accepted an invalid field name");
	xrtClearError();
	testRequire(!xrtHttpHeadersAdd(
		pHeaders, XRT_STR_LITERAL("X"), XRT_STR_LITERAL("ok\r\nInjected: yes")
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtHttpHeadersCount(pHeaders) == iCount),
		"HTTP Headers accepted a field-value injection");
	xrtClearError();
	testRequire(!xrtHttpHeadersSet(
		pHeaders, XRT_STR_LITERAL("Long-Name"), XRT_STR_LITERAL("x")
	) && (xrtHttpHeadersCount(pHeaders) == iCount),
		"HTTP Headers name limit changed state");
	xrtClearError();
	testRequire(!xrtHttpHeadersSet(
		pHeaders, XRT_STR_LITERAL("A"), XRT_STR_LITERAL("123456")
	) && testHttpHeadersText(xrtHttpHeadersGet(
		pHeaders, XRT_STR_LITERAL("A")
	)->Value, "12345"), "HTTP Headers value limit changed state");
	xrtClearError();
	testRequire(!xrtHttpHeadersReserve(pHeaders, 3, 8),
		"HTTP Headers reserve ignored field limit");
	xrtClearError();
	testRequire(xrtHttpHeadersRemove(
		pHeaders, XRT_STR_LITERAL("bad name")
	) == 0, "HTTP Headers remove accepted an invalid name");
	xrtClearError();
	xrtHttpHeadersDestroy(pHeaders);

	Config.InitialFields = 3;
	testRequire(xrtHttpHeadersCreate(&Config) == NULL,
		"HTTP Headers accepted initial capacity above its limit");
	xrtClearError();
}



/* 验证容器填满逻辑上限后，重复 Set 仍可回收旧值并保持稳定顺序。 */
static void testHttpHeadersChurn(void)
{
	xhttpheadersconfig Config;
	xhttpheaders* pHeaders;
	char Name[5];
	char Value[96];
	size_t i;
	size_t iRound;

	xrtHttpHeadersConfigInit(&Config);
	Config.InitialFields = 16;
	Config.InitialBytes = 1600;
	Config.MaxFields = 16;
	Config.MaxName = 4;
	Config.MaxValue = sizeof(Value);
	Config.MaxBytes = 1600;
	pHeaders = xrtHttpHeadersCreate(&Config);
	testRequire(pHeaders != NULL,
		"HTTP Headers churn fixture create failed");
	for ( i = 0; i < 16; i++ ) {
		int iName = snprintf(Name, sizeof(Name), "X-%02u", (unsigned)i);

		memset(Value, '0' + (int)(i % 10u), sizeof(Value));
		testRequire((iName == 4) && xrtHttpHeadersAdd(
			pHeaders,
			(xstrview){ Name, (size_t)iName },
			(xstrview){ Value, sizeof(Value) }
		), "HTTP Headers churn fill failed");
	}
	testRequire((xrtHttpHeadersCount(pHeaders) == 16) &&
		(xrtHttpHeadersBytes(pHeaders) == Config.MaxBytes),
		"HTTP Headers churn did not reach the exact logical limit");
	for ( iRound = 0; iRound < 8; iRound++ ) {
		for ( i = 0; i < 16; i++ ) {
			int iName = snprintf(
				Name, sizeof(Name), "X-%02u", (unsigned)i
			);

			memset(
				Value,
				'a' + (int)((iRound + i) % 26u),
				sizeof(Value)
			);
			testRequire((iName == 4) && xrtHttpHeadersSet(
				pHeaders,
				(xstrview){ Name, (size_t)iName },
				(xstrview){ Value, sizeof(Value) }
			), "HTTP Headers Set failed at the exact logical limit");
		}
	}
	for ( i = 0; i < 16; i++ ) {
		const xhttpfield* pField;
		int iName = snprintf(Name, sizeof(Name), "X-%02u", (unsigned)i);
		char iExpected = (char)('a' + ((7u + i) % 26u));

		pField = xrtHttpHeadersAt(pHeaders, i);
		testRequire((iName == 4) && (pField != NULL) &&
			testHttpHeadersText(pField->Name, Name) &&
			(pField->Value.Size == sizeof(Value)) &&
			(pField->Value.Data[0] == iExpected) &&
			(pField->Value.Data[sizeof(Value) - 1u] == iExpected),
			"HTTP Headers churn changed order or content");
	}
	testRequire((xrtHttpHeadersCount(pHeaders) == 16) &&
		(xrtHttpHeadersBytes(pHeaders) == Config.MaxBytes),
		"HTTP Headers churn changed logical counters");
	xrtHttpHeadersDestroy(pHeaders);
}



/* 验证容器交换保持地址稳定并转移全部字段所有权。 */
static void testHttpHeadersSwap(void)
{
	xhttpheaders* pLeft = xrtHttpHeadersCreate(NULL);
	xhttpheaders* pRight = xrtHttpHeadersCreate(NULL);
	const xhttpfield* pField;

	testRequire(
		(pLeft != NULL) && (pRight != NULL) &&
		xrtHttpHeadersAdd(
			pLeft, XRT_STR_LITERAL("X-Left"), XRT_STR_LITERAL("a")
		) &&
		xrtHttpHeadersAdd(
			pRight, XRT_STR_LITERAL("X-Right"), XRT_STR_LITERAL("b")
		) &&
		xrtHttpHeadersSwap(pLeft, pRight) &&
		xrtHttpHeadersSwap(pLeft, pLeft),
		"HTTP Headers swap setup failed"
	);
	pField = xrtHttpHeadersGet(pLeft, XRT_STR_LITERAL("X-Right"));
	testRequire(
		(xrtHttpHeadersCount(pLeft) == 1u) &&
		(pField != NULL) && testHttpHeadersText(
			pField->Value, "b"
		),
		"HTTP Headers swap did not transfer right state"
	);
	pField = xrtHttpHeadersGet(pRight, XRT_STR_LITERAL("X-Left"));
	testRequire(
		(xrtHttpHeadersCount(pRight) == 1u) &&
		(pField != NULL) && testHttpHeadersText(
			pField->Value, "a"
		),
		"HTTP Headers swap did not transfer left state"
	);
	xrtHttpHeadersDestroy(pRight);
	xrtHttpHeadersDestroy(pLeft);
}



/* 验证 Header block 写出查询、容量失败和输入输出重叠契约。 */
static void testHttpHeadersWrite(void)
{
	static const char Expected[] =
		"Host: example.test\r\nConnection: close\r\n\r\n";
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	const xhttpfield* pField;
	char Output[128];
	size_t iSize;

	testRequire((pHeaders != NULL) && xrtHttpHeadersAdd(
		pHeaders, XRT_STR_LITERAL("Host"), XRT_STR_LITERAL("example.test")
	) && xrtHttpHeadersAdd(
		pHeaders, XRT_STR_LITERAL("Connection"), XRT_STR_LITERAL("close")
	), "HTTP Headers write fixture failed");
	testRequire(xrtHttpHeadersWrite(
		pHeaders, NULL, 0, &iSize
	) && (iSize == (sizeof(Expected) - 1u)),
		"HTTP Headers write size query mismatch");
	memset(Output, 0xA5, sizeof(Output));
	testRequire(!xrtHttpHeadersWrite(
		pHeaders, Output, iSize - 1u, &iSize
	) && (Output[0] == (char)0xA5),
		"HTTP Headers short write was not atomic");
	xrtClearError();
	testRequire(xrtHttpHeadersWrite(
		pHeaders, Output, sizeof(Output), &iSize
	) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Output, Expected, iSize) == 0),
		"HTTP Headers block write mismatch");
	pField = xrtHttpHeadersAt(pHeaders, 0);
	testRequire(!xrtHttpHeadersWrite(
		pHeaders, (void*)pField->Name.Data, iSize, &iSize
	), "HTTP Headers write accepted overlapping output");
	xrtClearError();
	xrtHttpHeadersDestroy(pHeaders);
}



/* 运行拥有型 Header 容器的核心契约测试。 */
int main(void)
{
	testHttpHeadersConfig();
	testHttpHeadersMemoryContracts();
	testHttpHeadersOperations();
	testHttpHeadersBlock();
	testHttpHeadersLongValue();
	testHttpHeadersLimits();
	testHttpHeadersChurn();
	testHttpHeadersSwap();
	testHttpHeadersWrite();
	printf("[PASS] http_headers\n");
	return 0;
}
