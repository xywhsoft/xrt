#include "../test.h"

#include <locale.h>



/* 在不违反别名规则的情况下取得 double 位模式。 */
static uint64 testLongFloatBits(double fValue)
{
	uint64 iBits;

	memcpy(&iBits, &fValue, sizeof(iBits));
	return iBits;
}



/* 生成可复现的十进制测试数字。 */
static uint32 testLongFloatNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 验证任意长尾数和指数不会要求分配，也不会丢失尺度抵消。 */
static void testLongFloatScale(void)
{
	char* sText = (char*)malloc(12032u);
	double fValue;
	size_t iPosition;

	testRequire(sText != NULL, "long float test allocation failed");

	sText[0] = '1';
	memset(sText + 1, '0', 10000u);
	memcpy(sText + 10001u, "e-10000", 7u);
	testRequire(xrtNumParse(
		(xstrview){ sText, 10008u }, 0, &fValue) &&
		(fValue == 1.0), "long positive scale cancellation failed");

	sText[0] = '0';
	sText[1] = '.';
	memset(sText + 2, '0', 10000u);
	sText[10002u] = '5';
	memcpy(sText + 10003u, "e10001", 6u);
	testRequire(xrtNumParse(
		(xstrview){ sText, 10009u }, 0, &fValue) &&
		(fValue == 5.0), "long fractional scale cancellation failed");

	sText[0] = '0';
	sText[1] = 'e';
	memset(sText + 2, '9', 10000u);
	testRequire(xrtNumParse(
		(xstrview){ sText, 10002u }, 0, &fValue) &&
		(testLongFloatBits(fValue) == 0),
		"zero with huge exponent failed");

	sText[0] = '1';
	sText[1] = 'e';
	memset(sText + 2, '9', 10000u);
	fValue = 7.0;
	testRequire(!xrtNumParse(
		(xstrview){ sText, 10002u }, 0, &fValue) &&
		(fValue == 7.0), "huge positive exponent was accepted");
	xrtClearError();

	sText[0] = '1';
	sText[1] = 'e';
	sText[2] = '-';
	memset(sText + 3, '9', 10000u);
	testRequire(xrtNumParse(
		(xstrview){ sText, 10003u }, 0, &fValue) &&
		(testLongFloatBits(fValue) == 0),
		"huge negative exponent did not underflow to zero");

	iPosition = 0;
	for ( size_t i = 0; i < 3000u; i++ ) {
		sText[iPosition++] = (i == 0) ? '1' : '0';
		if ( (i + 1u) < 3000u ) {
			sText[iPosition++] = '_';
		}
	}
	memcpy(sText + iPosition, "e-2999", 6u);
	iPosition += 6u;
	testRequire(xrtNumParse(
		(xstrview){ sText, iPosition },
		(uint32)XNUMBER_PARSE_SEPARATOR,
		&fValue
	) && (fValue == 1.0), "long separated number failed");

	free(sText);
}



/* 与 C 区域设置下的系统 strtod 比较大量长尾数的最终位模式。 */
static void testLongFloatReference(void)
{
	char sText[1700];
	uint32 iState = UINT32_C(0xA341316C);

	testRequire(setlocale(LC_NUMERIC, "C") != NULL,
		"failed to select C numeric locale");
	for ( size_t i = 0; i < 500u; i++ ) {
		char* sEnd;
		double fReference;
		double fValue;
		size_t iSize = 0;
		int32 iExponent = (int32)(testLongFloatNext(&iState) % 601u) - 300;

		sText[iSize++] = (char)(
			'1' + (char)(testLongFloatNext(&iState) % 9u));
		sText[iSize++] = '.';
		for ( size_t j = 0; j < 1500u; j++ ) {
			sText[iSize++] = (char)(
				'0' + (char)(testLongFloatNext(&iState) % 10u));
		}
		iSize += (size_t)snprintf(
			sText + iSize,
			sizeof(sText) - iSize,
			"e%+d",
			(int)iExponent
		);
		sText[iSize] = 0;

		fReference = strtod(sText, &sEnd);
		testRequire(sEnd == sText + iSize,
			"strtod did not consume long reference input");
		testRequire(xrtNumParse(
			(xstrview){ sText, iSize }, 0, &fValue),
			"long reference input parse failed");
		testRequire(testLongFloatBits(fValue) ==
			testLongFloatBits(fReference),
			"long input differs from system correct-rounding reference");
	}
}



/* 验证显式长度不会把内嵌零字节误当作合法结束位置。 */
static void testLongFloatExplicitLength(void)
{
	static const char sText[] = {
		'1', '.', '5', '\0', '9'
	};
	double fValue = 7.0;

	testRequire(!xrtNumParse(
		(xstrview){ sText, sizeof(sText) }, 0, &fValue) &&
		(fValue == 7.0),
		"embedded zero byte was accepted as token end");
	xrtClearError();
}



/* 执行长输入、外部参考和显式长度测试。 */
int main(void)
{
	testLongFloatScale();
	testLongFloatReference();
	testLongFloatExplicitLength();
	printf("[PASS] number-float-long\n");
	return 0;
}
