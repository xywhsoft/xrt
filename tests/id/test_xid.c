#include "../test.h"



/* 为排序测试比较两个 XID。 */
static int testXidSort(const void* pLeft, const void* pRight)
{
	return xrtXidCompare((const xid*)pLeft, (const xid*)pRight);
}



/* 验证固定二进制值使用稳定的有序 URL-safe 编码。 */
static void testXidCodec(void)
{
	static const char sExpected[] = "--31-kF40VR71FcA2-oD2l-G3WBJ4GNM";
	xid Value;
	xid Parsed = XID_ZERO;
	char arrText[XID_TEXT_CAPACITY];
	str sText;

	for ( size_t i = 0; i < XID_BINARY_SIZE; i++ ) {
		Value.Data[i] = (uint8)i;
	}
	testRequire(
		xrtXidWrite(&Value, arrText, sizeof(arrText)),
		"XID fixed encoding failed"
	);
	testRequire(
		strcmp(arrText, sExpected) == 0,
		"XID fixed encoding changed"
	);
	testRequire(
		xrtXidParse(XRT_STR_LITERAL(sExpected), &Parsed) &&
		xrtXidEqual(&Value, &Parsed),
		"XID fixed decoding failed"
	);
	sText = xrtXidFormat(&Value);
	testRequire(
		(sText != NULL) && (strcmp(sText, sExpected) == 0),
		"XID allocated formatting failed"
	);
	xrtFree(sText);

	memset(arrText, 'x', sizeof(arrText));
	testRequire(
		!xrtXidWrite(&Value, arrText, XID_TEXT_SIZE) &&
		(arrText[0] == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"XID short output buffer was not rejected"
	);
	xrtClearError();
}



/* 验证格式错误带精确位置，且失败不会覆盖调用方结果。 */
static void testXidInvalidText(void)
{
	xid Sentinel;
	xid Output;
	const xerror* pError;
	size_t iOffset = XRT_NPOS;

	memset(&Sentinel, 0xA5, sizeof(Sentinel));
	Output = Sentinel;
	testRequire(
		!xrtXidParse(XRT_STR_LITERAL("short"), &Output),
		"short XID text was accepted"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.xid") == 0) &&
		(xrtErrorCode(pError) == XID_ERROR_FORMAT) &&
		xrtXidErrorOffset(pError, &iOffset) &&
		(iOffset == 5u) &&
		xrtXidEqual(&Output, &Sentinel),
		"short XID error contract failed"
	);
	xrtClearError();

	Output = Sentinel;
	testRequire(
		!xrtXidParse(
			XRT_STR_LITERAL("--31-kF40VR71FcA2!oD2l-G3WBJ4GNM"),
			&Output
		),
		"invalid XID alphabet byte was accepted"
	);
	pError = xrtGetError();
	testRequire(
		xrtXidErrorOffset(pError, &iOffset) &&
		(iOffset == 17u) &&
		xrtXidEqual(&Output, &Sentinel),
		"invalid XID byte position or transaction changed"
	);
	xrtClearError();

	testRequire(
		!xrtXidParse(
			XRT_STR_LITERAL("--31-kF40VR71FcA2-oD2l-G3WBJ4GNMx"),
			&Output
		) &&
		xrtXidErrorOffset(xrtGetError(), &iOffset) &&
		(iOffset == XID_TEXT_SIZE),
		"long XID text was not rejected at the first extra byte"
	);
	xrtClearError();
}



/* 验证有符号时间布局、二进制顺序和文本顺序完全一致。 */
static void testXidOrder(void)
{
	xid Low = XID_ZERO;
	xid High = XID_ZERO;
	xid NegativeOne = XID_ZERO;
	xid Epoch = XID_ZERO;
	xid Maximum = XID_ZERO;
	char arrLow[XID_TEXT_CAPACITY];
	char arrHigh[XID_TEXT_CAPACITY];
	xtime iLow;
	xtime iHigh;
	xtime iValue;

	High.Data[7] = 1u;
	testRequire(
		xrtXidTime(&Low, &iLow) &&
		xrtXidTime(&High, &iHigh) &&
		(iLow == INT64_MIN) &&
		(iHigh == (INT64_MIN + 1)) &&
		(xrtXidCompare(&Low, &High) < 0),
		"XID ordered time prefix failed"
	);
	testRequire(
		xrtXidWrite(&Low, arrLow, sizeof(arrLow)) &&
		xrtXidWrite(&High, arrHigh, sizeof(arrHigh)) &&
		(strcmp(arrLow, arrHigh) < 0),
		"XID text order differs from binary order"
	);
	NegativeOne.Data[0] = 0x7Fu;
	memset(NegativeOne.Data + 1u, 0xFF, 7u);
	Epoch.Data[0] = 0x80u;
	memset(Maximum.Data, 0xFF, 8u);
	testRequire(
		xrtXidTime(&NegativeOne, &iValue) && (iValue == -1) &&
		xrtXidTime(&Epoch, &iValue) && (iValue == 0) &&
		xrtXidTime(&Maximum, &iValue) && (iValue == INT64_MAX),
		"XID full signed time range did not round-trip"
	);
	testRequire(
		(xrtXidCompare(&Low, &NegativeOne) < 0) &&
		(xrtXidCompare(&NegativeOne, &Epoch) < 0) &&
		(xrtXidCompare(&Epoch, &Maximum) < 0),
		"XID signed time ordering changed"
	);
}



/* 验证单个、批量和文本便捷生成路径。 */
static void testXidGeneration(void)
{
	xid arrValues[256];
	xid Parsed;
	xtime iBefore = xrtNow();
	xtime iGenerated;
	xtime iAfter;
	str sText;

	testRequire(xrtXidMake(&arrValues[0]), "single XID generation failed");
	iAfter = xrtNow();
	testRequire(
		!xrtXidIsZero(&arrValues[0]) &&
		xrtXidTime(&arrValues[0], &iGenerated) &&
		(iGenerated >= iBefore) && (iGenerated <= iAfter),
		"generated XID time is outside the call interval"
	);
	testRequire(
		xrtXidMakeMany(arrValues, 256u),
		"batch XID generation failed"
	);
	qsort(arrValues, 256u, sizeof(arrValues[0]), testXidSort);
	for ( size_t i = 1; i < 256u; i++ ) {
		testRequire(
			xrtXidCompare(&arrValues[i - 1u], &arrValues[i]) < 0,
			"batch XID collision detected"
		);
	}
	testRequire(
		xrtXidMakeMany(NULL, 0u),
		"empty XID batch should be valid"
	);
	testRequire(
		!xrtXidMakeMany(NULL, 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"nonempty null XID batch was accepted"
	);
	xrtClearError();
	testRequire(
		!xrtXidMakeMany(
			(xid*)(uintptr_t)1u,
			(SIZE_MAX / sizeof(xid)) + 1u
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"overflowing XID batch size was accepted"
	);
	xrtClearError();

	sText = xrtXidMakeString();
	testRequire(
		(sText != NULL) &&
		(strlen(sText) == XID_TEXT_SIZE) &&
		xrtXidParse((xstrview){ sText, XID_TEXT_SIZE }, &Parsed),
		"XID string convenience path failed"
	);
	xrtFree(sText);
}



/* 运行 XID 的布局、解析、排序和生成契约。 */
int main(void)
{
	xid Zero = XID_ZERO;

	testRequire(sizeof(xid) == XID_BINARY_SIZE, "XID ABI is not 24 bytes");
	testRequire(xrtXidIsZero(&Zero), "XID zero initializer failed");
	testXidCodec();
	testXidInvalidText();
	testXidOrder();
	testXidGeneration();
	printf("[PASS] XID\n");
	return 0;
}
