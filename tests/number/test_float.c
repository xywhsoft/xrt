#include "../test.h"

#include <float.h>
#include <math.h>



/* 在不违反别名规则的情况下取得 double 位模式。 */
static uint64 testFloatBits(double fValue)
{
	uint64 iBits;

	memcpy(&iBits, &fValue, sizeof(iBits));
	return iBits;
}



/* 从指定 IEEE-754 位模式构造 double。 */
static double testFloatFromBits(uint64 iBits)
{
	double fValue;

	memcpy(&fValue, &iBits, sizeof(fValue));
	return fValue;
}



/* 生成可复现的 64 位随机位模式。 */
static uint64 testFloatNext(uint64* pState)
{
	uint64 iValue = *pState;

	iValue ^= iValue >> 12u;
	iValue ^= iValue << 25u;
	iValue ^= iValue >> 27u;
	*pState = iValue;
	return iValue * UINT64_C(2685821657736338717);
}



/* 校验一个值的稳定文本和精确往返。 */
static void testFloatText(
	double fValue,
	uint32 iFlags,
	cstr sExpected
)
{
	char sOutput[64];
	size_t iSize;
	double fParsed;

	testRequire(xrtNumWrite(
		fValue, sOutput, sizeof(sOutput), &iSize, iFlags),
		"floating-point write failed");
	testRequire((iSize == strlen(sExpected)) &&
		(strcmp(sOutput, sExpected) == 0),
		"floating-point text mismatch");
	testRequire(xrtNumParse(
		(xstrview){ sOutput, iSize },
		0,
		&fParsed
	) && (testFloatBits(fParsed) == testFloatBits(fValue)),
		"floating-point text did not round trip");
}



/* 验证零、普通值、极值、科学计数法和紧凑模式。 */
static void testFloatFormatting(void)
{
	char sOutput[64];
	size_t iSize;
	double fMinimum = testFloatFromBits(UINT64_C(1));

	testFloatText(0.0, 0, "0.0");
	testFloatText(testFloatFromBits(UINT64_C(0x8000000000000000)),
		0, "-0.0");
	testFloatText(1.0, 0, "1.0");
	testFloatText(-1.0, 0, "-1.0");
	testFloatText(3.14, 0, "3.14");
	testFloatText(1.0e10, 0, "10000000000.0");
	testFloatText(1.0e21, 0, "1e+21");
	testFloatText(DBL_MIN, 0, "2.2250738585072014e-308");
	testFloatText(fMinimum, 0, "5e-324");
	testFloatText(DBL_MAX, 0, "1.7976931348623157e+308");
	testFloatText(1.0, (uint32)XNUMBER_FLOAT_COMPACT, "1");
	testFloatText(testFloatFromBits(UINT64_C(0x8000000000000000)),
		(uint32)XNUMBER_FLOAT_COMPACT, "-0");

	testRequire(xrtNumWrite(INFINITY, sOutput, sizeof(sOutput),
		&iSize, 0) && (strcmp(sOutput, "inf") == 0),
		"positive infinity text mismatch");
	testRequire(xrtNumWrite(-INFINITY, sOutput, sizeof(sOutput),
		&iSize, 0) && (strcmp(sOutput, "-inf") == 0),
		"negative infinity text mismatch");
	testRequire(xrtNumWrite(NAN, sOutput, sizeof(sOutput),
		&iSize, 0) && (strcmp(sOutput, "nan") == 0),
		"NaN text mismatch");
}



/* 验证常用语法、边界舍入、下溢和显式特殊值。 */
static void testFloatParsing(void)
{
	double fValue;

	testRequire(xrtNumParse(XRT_STR_LITERAL(".5"), 0, &fValue) &&
		(fValue == 0.5), "leading decimal point parse failed");
	testRequire(xrtNumParse(XRT_STR_LITERAL("1."), 0, &fValue) &&
		(fValue == 1.0), "trailing decimal point parse failed");
	testRequire(xrtNumParse(XRT_STR_LITERAL("+1.25e+2"), 0, &fValue) &&
		(fValue == 125.0), "signed exponent parse failed");
	testRequire(xrtNumParse(XRT_STR_LITERAL("  -1_234.5_0e-1  "),
		(uint32)XNUMBER_PARSE_SPACE |
		(uint32)XNUMBER_PARSE_SEPARATOR, &fValue) &&
		(fValue == -123.45), "separator and whitespace parse failed");

	testRequire(xrtNumParse(
		XRT_STR_LITERAL("1.7976931348623157e308"), 0, &fValue) &&
		(testFloatBits(fValue) == testFloatBits(DBL_MAX)),
		"DBL_MAX parse failed");
	testRequire(xrtNumParse(
		XRT_STR_LITERAL("2.2250738585072014e-308"), 0, &fValue) &&
		(testFloatBits(fValue) == testFloatBits(DBL_MIN)),
		"DBL_MIN parse failed");
	testRequire(xrtNumParse(
		XRT_STR_LITERAL("4.9406564584124654e-324"), 0, &fValue) &&
		(testFloatBits(fValue) == UINT64_C(1)),
		"minimum subnormal parse failed");
	testRequire(xrtNumParse(
		XRT_STR_LITERAL("2.4703282292062327e-324"), 0, &fValue) &&
		(testFloatBits(fValue) == 0),
		"subnormal halfway-to-even parse failed");
	testRequire(xrtNumParse(
		XRT_STR_LITERAL("2.4703282292062328e-324"), 0, &fValue) &&
		(testFloatBits(fValue) == UINT64_C(1)),
		"subnormal above-halfway parse failed");
	testRequire(xrtNumParse(XRT_STR_LITERAL(
		"9007199254740991.4999999999999999999999999999999995"),
		0, &fValue) &&
		(testFloatBits(fValue) == UINT64_C(0x433FFFFFFFFFFFFF)),
		"large integer halfway regression failed");
	testRequire(xrtNumParse(XRT_STR_LITERAL(
		"1.50000000000000011102230246251565404236316680908203125"),
		0, &fValue) &&
		(testFloatBits(fValue) == UINT64_C(0x3FF8000000000000)),
		"fraction halfway-to-even regression failed");
	testRequire(xrtNumParse(XRT_STR_LITERAL("1e-400"), 0, &fValue) &&
		(testFloatBits(fValue) == 0),
		"positive underflow contract mismatch");
	testRequire(xrtNumParse(XRT_STR_LITERAL("-1e-400"), 0, &fValue) &&
		(testFloatBits(fValue) == UINT64_C(0x8000000000000000)),
		"negative underflow contract mismatch");

	testRequire(xrtNumParse(XRT_STR_LITERAL("-Infinity"),
		(uint32)XNUMBER_PARSE_SPECIAL, &fValue) &&
		(testFloatBits(fValue) == UINT64_C(0xFFF0000000000000)),
		"infinity parse failed");
	testRequire(xrtNumParse(XRT_STR_LITERAL("NaN"),
		(uint32)XNUMBER_PARSE_SPECIAL, &fValue) &&
		((testFloatBits(fValue) & UINT64_C(0x7FF0000000000000)) ==
		UINT64_C(0x7FF0000000000000)) &&
		((testFloatBits(fValue) & UINT64_C(0x000FFFFFFFFFFFFF)) != 0),
		"NaN parse failed");
}



/* 验证失败原子、错误分类和查询写入契约。 */
static void testFloatFailures(void)
{
	static const cstr arrInvalid[] = {
		"",
		".",
		"+",
		"1e",
		"1e+",
		"_1",
		"1_",
		"1__2",
		"1_.0",
		"1._0",
		"1.2x",
		"nan"
	};
	char sOutput[16];
	char sBefore[16];
	size_t iSize = 99;
	double fValue = 7.0;

	memset(sOutput, 0x5A, sizeof(sOutput));
	memcpy(sBefore, sOutput, sizeof(sOutput));
	testRequire(!xrtNumWrite(123.5, sOutput, 5, &iSize, 0) &&
		(iSize == 5) &&
		(memcmp(sOutput, sBefore, sizeof(sOutput)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"floating-point capacity failure contract mismatch");
	xrtClearError();

	for ( size_t i = 0; i < (sizeof(arrInvalid) / sizeof(arrInvalid[0])); i++ ) {
		xstrview Text = { arrInvalid[i], strlen(arrInvalid[i]) };

		testRequire(!xrtNumParse(
			Text, (uint32)XNUMBER_PARSE_SEPARATOR, &fValue) &&
			(testFloatBits(fValue) == testFloatBits(7.0)),
			"invalid floating-point text changed output");
		xrtClearError();
	}
	testRequire(!xrtNumParse(
		XRT_STR_LITERAL("1.7976931348623159e308"), 0, &fValue) &&
		(testFloatBits(fValue) == testFloatBits(7.0)) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_RANGE),
		"floating-point overflow contract mismatch");
	xrtClearError();
	testRequire(!xrtNumWrite(
		1.0, sOutput, sizeof(sOutput), &iSize, UINT32_C(0x80000000)) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_CONFIG),
		"invalid floating-point write flag was accepted");
	xrtClearError();
}



/* 对两百万个有限 double 位模式执行文本精确往返。 */
static void testFloatRandomRoundTrip(void)
{
	uint64 iState = UINT64_C(0xD1B54A32D192ED03);
	char sOutput[64];

	for ( size_t i = 0; i < 2000000u; i++ ) {
		uint64 iBits = testFloatNext(&iState);
		double fValue;
		double fParsed;
		size_t iSize;

		if ( (iBits & UINT64_C(0x7FF0000000000000)) ==
			UINT64_C(0x7FF0000000000000) ) {
			continue;
		}
		fValue = testFloatFromBits(iBits);
		testRequire(xrtNumWrite(
			fValue, sOutput, sizeof(sOutput), &iSize, 0),
			"random floating-point write failed");
		testRequire(iSize <= 25u,
			"random floating-point text exceeded stable bound");
		testRequire(xrtNumParse(
			(xstrview){ sOutput, iSize }, 0, &fParsed),
			"random floating-point parse failed");
		testRequire(testFloatBits(fParsed) == iBits,
			"random floating-point round trip mismatch");
	}
}



/* 执行浮点数值层全部基础与随机契约测试。 */
int main(void)
{
	testFloatFormatting();
	testFloatParsing();
	testFloatFailures();
	testFloatRandomRoundTrip();
	printf("[PASS] number-float\n");
	return 0;
}
