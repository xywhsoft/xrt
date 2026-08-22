#include "../test.h"

#include <float.h>
#include <math.h>



/* 从指定位模式构造 double。 */
static double testFormatFloat(uint64 iBits)
{
	double fValue;

	memcpy(&fValue, &iBits, sizeof(fValue));
	return fValue;
}



/* 验证有符号整数展示文本。 */
static void testSignedIntegerFormat(int64 iValue, xstrview Format,
	cstr sExpected)
{
	char sOutput[128];
	size_t iSize = SIZE_MAX;
	str sText;

	testRequire(xrtIntFormatTo(
		iValue, Format, sOutput, sizeof(sOutput), &iSize),
		"signed integer format failed");
	testRequire((iSize == strlen(sExpected)) &&
		(strcmp(sOutput, sExpected) == 0),
		"signed integer format mismatch");
	sText = xrtIntFormat(iValue, Format);
	testRequire((sText != NULL) && (strcmp(sText, sExpected) == 0),
		"allocated signed integer format mismatch");
	xrtFree(sText);
}



/* 验证无符号整数展示文本。 */
static void testUnsignedIntegerFormat(uint64 iValue, xstrview Format,
	cstr sExpected)
{
	char sOutput[128];
	size_t iSize = SIZE_MAX;

	testRequire(xrtUIntFormatTo(
		iValue, Format, sOutput, sizeof(sOutput), &iSize),
		"unsigned integer format failed");
	testRequire((iSize == strlen(sExpected)) &&
		(strcmp(sOutput, sExpected) == 0),
		"unsigned integer format mismatch");
}



/* 验证浮点展示文本。 */
static void testFloatFormat(double fValue, xstrview Format,
	cstr sExpected)
{
	char sOutput[2048];
	size_t iSize = SIZE_MAX;
	str sText;

	if ( !xrtNumFormatTo(
			fValue, Format, sOutput, sizeof(sOutput), &iSize) ) {
		fprintf(stderr, "format failed: %a %.*s (%s)\n",
			fValue, (int)Format.Size, Format.Data,
			xrtErrorMessage(xrtGetError()));
		testRequire(false, "floating-point format failed");
	}
	if ( (iSize != strlen(sExpected)) ||
		 (strcmp(sOutput, sExpected) != 0) ) {
		fprintf(stderr, "format mismatch: %a %.*s actual=%s expected=%s\n",
			fValue, (int)Format.Size, Format.Data, sOutput, sExpected);
		testRequire(false, "floating-point format mismatch");
	}
	sText = xrtNumFormat(fValue, Format);
	testRequire((sText != NULL) && (strcmp(sText, sExpected) == 0),
		"allocated floating-point format mismatch");
	xrtFree(sText);
}



/* 验证格式语法、分组、精确舍入、特殊值和失败原子性。 */
int main(void)
{
	char sBefore[32];
	char sOutput[32];
	char sWide[2048];
	size_t iSize;

	testSignedIntegerFormat(-42, XRT_STR_LITERAL(""), "-42");
	testSignedIntegerFormat(42, XRT_STR_LITERAL("+08d"), "+0000042");
	testSignedIntegerFormat(INT64_MIN, XRT_STR_LITERAL(",d"),
		"-9,223,372,036,854,775,808");
	testSignedIntegerFormat(255, XRT_STR_LITERAL("#010X"),
		"0X000000FF");
	testSignedIntegerFormat(1234, XRT_STR_LITERAL("010,d"),
		"000,001,234");
	testUnsignedIntegerFormat(UINT64_C(0xDEADBEEF),
		XRT_STR_LITERAL("_X"), "DEAD_BEEF");
	testUnsignedIntegerFormat(UINT64_MAX, XRT_STR_LITERAL(",d"),
		"18,446,744,073,709,551,615");
	testSignedIntegerFormat(65, XRT_STR_LITERAL("c"), "A");
	testSignedIntegerFormat(20320, XRT_STR_LITERAL("c"),
		"\xE4\xBD\xA0");
	testSignedIntegerFormat(0x10FFFF, XRT_STR_LITERAL("c"),
		"\xF4\x8F\xBF\xBF");
	testSignedIntegerFormat(65, XRT_STR_LITERAL("4c"), "   A");
	testSignedIntegerFormat(0, XRT_STR_LITERAL("c"), "");
	testSignedIntegerFormat(-1, XRT_STR_LITERAL("c"), "");
	testSignedIntegerFormat(0xD800, XRT_STR_LITERAL("c"), "");
	testUnsignedIntegerFormat(UINT64_C(0x110000),
		XRT_STR_LITERAL("c"), "");
	testUnsignedIntegerFormat(UINT64_MAX, XRT_STR_LITERAL("c"), "");

	testFloatFormat(1.0, XRT_STR_LITERAL(""), "1.0");
	testFloatFormat(12.345, XRT_STR_LITERAL(".2"), "12.35");
	testFloatFormat(3.5, XRT_STR_LITERAL("+08.2f"), "+0003.50");
	testFloatFormat(1234567.89, XRT_STR_LITERAL(",.2f"),
		"1,234,567.89");
	testFloatFormat(2.5, XRT_STR_LITERAL(".0f"), "2");
	testFloatFormat(3.5, XRT_STR_LITERAL(".0f"), "4");
	testFloatFormat(2.5, XRT_STR_LITERAL("#.0f"), "2.");
	testFloatFormat(1234.6, XRT_STR_LITERAL(".3e"), "1.235e+03");
	testFloatFormat(12346.0, XRT_STR_LITERAL(".4g"), "1.235e+04");
	testFloatFormat(12.34, XRT_STR_LITERAL(".4g"), "12.34");
	testFloatFormat(12.0, XRT_STR_LITERAL("#.4g"), "12.00");
	testFloatFormat(0.1234, XRT_STR_LITERAL(".2%"), "12.34%");
	testFloatFormat(-0.0, XRT_STR_LITERAL(".2f"), "-0.00");
	testFloatFormat(DBL_MAX, XRT_STR_LITERAL(".2e"), "1.80e+308");
	testFloatFormat(testFormatFloat(UINT64_C(1)),
		XRT_STR_LITERAL(".3e"), "4.941e-324");
	testFloatFormat(INFINITY, XRT_STR_LITERAL("F"), "INF");
	testFloatFormat(-INFINITY, XRT_STR_LITERAL("+F"), "-INF");
	testFloatFormat(NAN, XRT_STR_LITERAL("+F"), "+NAN");
	testFloatFormat(testFormatFloat(UINT64_C(0xFFF8000000000001)),
		XRT_STR_LITERAL("+F"), "+NAN");

	testRequire(xrtNumFormatTo(DBL_MAX, XRT_STR_LITERAL(".1000f"),
		sWide, sizeof(sWide), &iSize) && (iSize == 1310u) &&
		(sWide[309] == '.'),
		"maximum precision fixed format mismatch");
	for ( size_t i = 310u; i < iSize; i++ ) {
		testRequire(sWide[i] == '0',
			"maximum precision fixed tail mismatch");
	}
	testRequire(xrtNumFormatTo(testFormatFloat(UINT64_C(1)),
		XRT_STR_LITERAL(".1000e"), sWide, sizeof(sWide), &iSize) &&
		(iSize == 1007u) && (strstr(sWide, "e-324") != NULL),
		"minimum subnormal maximum precision mismatch");
	testRequire(xrtNumFormatTo(DBL_MAX, XRT_STR_LITERAL(".0%"),
		sWide, sizeof(sWide), &iSize) && (iSize == 312u) &&
		(sWide[iSize - 1u] == '%') && (strstr(sWide, "inf") == NULL),
		"exact percentage scaling overflowed");

	iSize = SIZE_MAX;
	testRequire(xrtNumFormatTo(12.5, XRT_STR_LITERAL(".2f"),
		NULL, 0, &iSize) && (iSize == 5),
		"floating-point format query mismatch");
	memset(sOutput, 0xA5, sizeof(sOutput));
	memcpy(sBefore, sOutput, sizeof(sOutput));
	iSize = SIZE_MAX;
	testRequire(!xrtNumFormatTo(12.5, XRT_STR_LITERAL(".2f"),
		sOutput, 5, &iSize) && (iSize == 5) &&
		(memcmp(sOutput, sBefore, sizeof(sOutput)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"short format target changed output");
	xrtClearError();

	testRequire(!xrtNumFormatTo(1.0, XRT_STR_LITERAL("."),
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_FORMAT),
		"missing precision digits must fail");
	xrtClearError();
	testRequire(!xrtNumFormatTo(1.0, XRT_STR_LITERAL(".1001f"),
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_FORMAT),
		"excess precision must fail");
	xrtClearError();
	testRequire(!xrtIntFormatTo(1, XRT_STR_LITERAL(",x"),
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_FORMAT),
		"invalid integer grouping must fail");
	xrtClearError();
	testRequire(!xrtIntFormatTo(1, XRT_STR_LITERAL(".2d"),
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_FORMAT),
		"integer precision must fail");
	xrtClearError();
	testRequire(!xrtIntFormatTo(65, XRT_STR_LITERAL("+c"),
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_FORMAT),
		"character sign must fail");
	xrtClearError();
	testRequire(!xrtIntFormatTo(65, XRT_STR_LITERAL("#c"),
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_FORMAT),
		"character alternate form must fail");
	xrtClearError();
	testRequire(!xrtIntFormatTo(65, XRT_STR_LITERAL("04c"),
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_FORMAT),
		"character zero padding must fail");
	xrtClearError();
	testRequire(!xrtIntFormatTo(65, XRT_STR_LITERAL(",c"),
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_FORMAT),
		"character grouping must fail");
	xrtClearError();
	testRequire(!xrtIntFormatTo(65, XRT_STR_LITERAL(".1c"),
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_FORMAT),
		"character precision must fail");
	xrtClearError();
	iSize = SIZE_MAX;
	testRequire(xrtIntFormatTo(20320, XRT_STR_LITERAL("c"),
		NULL, 0, &iSize) && (iSize == 3),
		"character format query mismatch");
	memset(sOutput, 0xA5, sizeof(sOutput));
	memcpy(sBefore, sOutput, sizeof(sOutput));
	iSize = SIZE_MAX;
	testRequire(!xrtIntFormatTo(20320, XRT_STR_LITERAL("c"),
		sOutput, 3, &iSize) && (iSize == 3) &&
		(memcmp(sOutput, sBefore, sizeof(sOutput)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"short character target changed output");
	xrtClearError();
	testRequire(!xrtNumFormatTo(1.0, (xstrview){ NULL, 1 },
		sOutput, sizeof(sOutput), &iSize) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid format view must fail");
	xrtClearError();
	printf("[PASS] number-format\n");
	return 0;
}
