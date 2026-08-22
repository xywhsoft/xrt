#include "../test.h"



/* 生成可复现的 64 位测试值。 */
static uint64 testNumberNext(uint64* pState)
{
	uint64 iValue = *pState;

	iValue ^= iValue >> 12u;
	iValue ^= iValue << 25u;
	iValue ^= iValue >> 27u;
	*pState = iValue;
	return iValue * UINT64_C(2685821657736338717);
}



/* 验证十进制边界、符号和旧实现曾触发未定义行为的 INT64_MIN。 */
static void testIntegerDecimal(void)
{
	static const struct {
		int64 Value;
		cstr Text;
	} arrSigned[] = {
		{ INT64_MIN, "-9223372036854775808" },
		{ -INT64_C(1), "-1" },
		{ INT64_C(0), "0" },
		{ INT64_C(1), "1" },
		{ INT64_MAX, "9223372036854775807" }
	};
	char sOutput[80];
	size_t iSize;
	uint64 iUnsigned;
	int64 iSigned;

	for ( size_t i = 0; i < (sizeof(arrSigned) / sizeof(arrSigned[0])); i++ ) {
		testRequire(xrtIntWrite(arrSigned[i].Value, 10, sOutput,
			sizeof(sOutput), &iSize, 0), "signed decimal write failed");
		testRequire((iSize == strlen(arrSigned[i].Text)) &&
			(strcmp(sOutput, arrSigned[i].Text) == 0),
			"signed decimal text mismatch");
		testRequire(xrtIntParse((xstrview){ sOutput, iSize }, 10, 0, &iSigned) &&
			(iSigned == arrSigned[i].Value), "signed decimal round trip failed");
	}
	testRequire(xrtUIntWrite(UINT64_MAX, 10, sOutput,
		sizeof(sOutput), &iSize, 0) &&
		(strcmp(sOutput, "18446744073709551615") == 0),
		"UINT64_MAX decimal text mismatch");
	testRequire(xrtUIntParse((xstrview){ sOutput, iSize }, 10, 0, &iUnsigned) &&
		(iUnsigned == UINT64_MAX), "UINT64_MAX decimal round trip failed");

	testRequire(xrtIntWrite(42, 10, sOutput, sizeof(sOutput),
		&iSize, (uint32)XNUMBER_PLUS) && (strcmp(sOutput, "+42") == 0),
		"explicit positive sign mismatch");
}



/* 验证所有基数、大小写、前缀和分配便捷层。 */
static void testIntegerBases(void)
{
	char sOutput[80];
	size_t iSize;
	uint64 iValue;
	str sAllocated;

	testRequire(xrtUIntWrite(UINT64_C(255), 16, sOutput, sizeof(sOutput),
		&iSize, (uint32)XNUMBER_PREFIX) &&
		(strcmp(sOutput, "0xff") == 0), "lowercase hexadecimal mismatch");
	testRequire(xrtUIntWrite(UINT64_C(255), 16, sOutput, sizeof(sOutput),
		&iSize, (uint32)XNUMBER_PREFIX | (uint32)XNUMBER_UPPER) &&
		(strcmp(sOutput, "0XFF") == 0), "uppercase hexadecimal mismatch");
	testRequire(xrtUIntWrite(UINT64_C(9), 2, sOutput, sizeof(sOutput),
		&iSize, (uint32)XNUMBER_PREFIX) &&
		(strcmp(sOutput, "0b1001") == 0), "binary prefix mismatch");
	testRequire(xrtIntWrite(-INT64_C(9), 8, sOutput, sizeof(sOutput),
		&iSize, (uint32)XNUMBER_PREFIX) &&
		(strcmp(sOutput, "-0o11") == 0), "signed octal prefix mismatch");
	testRequire(xrtUIntWrite(UINT64_MAX, 36, sOutput, sizeof(sOutput),
		&iSize, (uint32)XNUMBER_UPPER), "base36 write failed");
	testRequire(xrtUIntParse((xstrview){ sOutput, iSize }, 36, 0, &iValue) &&
		(iValue == UINT64_MAX), "base36 round trip failed");

	sAllocated = xrtIntString(INT64_MIN, 10, 0);
	testRequire((sAllocated != NULL) &&
		(strcmp(sAllocated, "-9223372036854775808") == 0),
		"allocated signed text mismatch");
	xrtFree(sAllocated);
	sAllocated = xrtUIntString(UINT64_MAX, 16,
		(uint32)XNUMBER_PREFIX | (uint32)XNUMBER_UPPER);
	testRequire((sAllocated != NULL) &&
		(strcmp(sAllocated, "0XFFFFFFFFFFFFFFFF") == 0),
		"allocated unsigned text mismatch");
	xrtFree(sAllocated);
}



/* 验证严格解析、自动基数、空白和数字分隔符。 */
static void testIntegerParsing(void)
{
	uint64 iUnsigned = 7;
	int64 iSigned = 7;

	testRequire(xrtUIntParse(XRT_STR_LITERAL("0xff"), 0,
		(uint32)XNUMBER_PARSE_PREFIX, &iUnsigned) &&
		(iUnsigned == UINT64_C(255)), "auto hexadecimal parse failed");
	testRequire(xrtUIntParse(XRT_STR_LITERAL("0B1010"), 2,
		(uint32)XNUMBER_PARSE_PREFIX, &iUnsigned) &&
		(iUnsigned == UINT64_C(10)), "explicit binary prefix parse failed");
	testRequire(xrtUIntParse(XRT_STR_LITERAL("077"), 0, 0, &iUnsigned) &&
		(iUnsigned == UINT64_C(77)), "auto base should default to decimal");
	testRequire(xrtIntParse(XRT_STR_LITERAL(" \t-0x7f\r\n"), 0,
		(uint32)XNUMBER_PARSE_SPACE | (uint32)XNUMBER_PARSE_PREFIX,
		&iSigned) && (iSigned == -INT64_C(127)),
		"signed prefixed whitespace parse failed");
	testRequire(xrtUIntParse(XRT_STR_LITERAL("18_446_744_073_709_551_615"),
		10, (uint32)XNUMBER_PARSE_SEPARATOR, &iUnsigned) &&
		(iUnsigned == UINT64_MAX), "separated UINT64_MAX parse failed");
	testRequire(xrtIntParse(XRT_STR_LITERAL("+9_223_372_036_854_775_807"),
		10, (uint32)XNUMBER_PARSE_SEPARATOR, &iSigned) &&
		(iSigned == INT64_MAX), "separated INT64_MAX parse failed");
}



/* 验证格式、范围、配置和容量失败不发布部分结果。 */
static void testIntegerFailures(void)
{
	static const cstr arrInvalid[] = {
		"",
		"_1",
		"1_",
		"1__2",
		"12x",
		"0x",
		"--1"
	};
	char sOutput[16];
	char sBefore[16];
	size_t iSize;
	uint64 iUnsigned = UINT64_C(123);
	int64 iSigned = INT64_C(123);

	memset(sOutput, 0x5A, sizeof(sOutput));
	memcpy(sBefore, sOutput, sizeof(sOutput));
	iSize = 99;
	testRequire(!xrtUIntWrite(UINT64_C(12345), 10, sOutput, 5,
		&iSize, 0) && (iSize == 5) &&
		(memcmp(sOutput, sBefore, sizeof(sOutput)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"capacity failure contract mismatch");
	xrtClearError();

	for ( size_t i = 0; i < (sizeof(arrInvalid) / sizeof(arrInvalid[0])); i++ ) {
		uint32 iFlags = (uint32)XNUMBER_PARSE_PREFIX |
			(uint32)XNUMBER_PARSE_SEPARATOR;
		xstrview Text = { arrInvalid[i], strlen(arrInvalid[i]) };

		testRequire(!xrtUIntParse(Text, 0,
			iFlags, &iUnsigned) && (iUnsigned == UINT64_C(123)),
			"invalid unsigned text changed output");
		xrtClearError();
	}
	testRequire(!xrtUIntParse(
		XRT_STR_LITERAL("18446744073709551616"), 10, 0, &iUnsigned) &&
		(iUnsigned == UINT64_C(123)) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_RANGE),
		"unsigned overflow contract mismatch");
	xrtClearError();
	testRequire(!xrtIntParse(
		XRT_STR_LITERAL("9223372036854775808"), 10, 0, &iSigned) &&
		(iSigned == INT64_C(123)), "positive signed overflow was accepted");
	xrtClearError();
	testRequire(!xrtIntParse(
		XRT_STR_LITERAL("-9223372036854775809"), 10, 0, &iSigned) &&
		(iSigned == INT64_C(123)), "negative signed overflow was accepted");
	xrtClearError();
	testRequire(!xrtUIntParse(XRT_STR_LITERAL("-1"), 10, 0, &iUnsigned) &&
		(iUnsigned == UINT64_C(123)), "unsigned parser accepted a sign");
	xrtClearError();
	testRequire(!xrtUIntWrite(1, 3, sOutput, sizeof(sOutput), &iSize,
		(uint32)XNUMBER_PREFIX) &&
		(xrtErrorCode(xrtGetError()) == XNUMBER_ERROR_CONFIG),
		"unsupported prefix base was accepted");
	xrtClearError();
	testRequire(!xrtUIntParse(XRT_STR_LITERAL("0x10"), 8,
		(uint32)XNUMBER_PARSE_PREFIX, &iUnsigned) &&
		(iUnsigned == UINT64_C(123)), "mismatched prefix was accepted");
	xrtClearError();
}



/* 对大量位模式和全部基数执行格式化解析往返。 */
static void testIntegerRandomRoundTrip(void)
{
	uint64 iState = UINT64_C(0xD1B54A32D192ED03);
	char sOutput[80];
	char sLibc[32];

	for ( size_t i = 0; i < 200000u; i++ ) {
		uint64 iValue = testNumberNext(&iState);
		uint32 iBase = (uint32)((iValue % UINT64_C(35)) + UINT64_C(2));
		size_t iSize;
		uint64 iParsed;

		testRequire(xrtUIntWrite(iValue, iBase, sOutput,
			sizeof(sOutput), &iSize, 0), "random integer write failed");
		testRequire(xrtUIntParse((xstrview){ sOutput, iSize },
			iBase, 0, &iParsed) && (iParsed == iValue),
			"random integer round trip failed");
		if ( iBase == 10u ) {
			(void)snprintf(sLibc, sizeof(sLibc), "%llu",
				(unsigned long long)iValue);
			testRequire(strcmp(sOutput, sLibc) == 0,
				"random decimal differs from libc");
		}
	}
}



/* 执行整数数值层全部契约测试。 */
int main(void)
{
	testIntegerDecimal();
	testIntegerBases();
	testIntegerParsing();
	testIntegerFailures();
	testIntegerRandomRoundTrip();
	printf("[PASS] number-integer\n");
	return 0;
}
