#include "../test.h"

#include <xrt/http_structured.h>



/* 写出再解析 Integer 和 Decimal，验证定点值不漂移。 */
static void testStructuredNumberRoundTrip(void)
{
	char arrOutput[64];
	xhttpstructuredbare Parsed;
	xhttpstructuredvalue Value;
	int64 iValue;
	size_t iOffset;
	size_t iSize;

	memset(&Value, 0, sizeof(Value));
	for ( iValue = -INT64_C(1000000);
		iValue <= INT64_C(1000000); iValue += INT64_C(9973) ) {
		Value.Type = XHTTP_STRUCTURED_INTEGER;
		Value.Number = iValue;
		testRequire(
			xrtHttpStructuredBareWrite(
				&Value, arrOutput, sizeof(arrOutput), &iSize
			),
			"structured Integer property write failed"
		);
		iOffset = 0;
		testRequire(
			(xrtHttpStructuredBareNext(
				(xstrview){ arrOutput, iSize },
				&iOffset, &Parsed
			) == XHTTP_NEXT_ITEM) &&
			(iOffset == iSize) &&
			(Parsed.Type == Value.Type) &&
			(Parsed.Number == Value.Number),
			"structured Integer property round trip failed"
		);

		Value.Type = XHTTP_STRUCTURED_DECIMAL;
		testRequire(
			xrtHttpStructuredBareWrite(
				&Value, arrOutput, sizeof(arrOutput), &iSize
			),
			"structured Decimal property write failed"
		);
		iOffset = 0;
		testRequire(
			(xrtHttpStructuredBareNext(
				(xstrview){ arrOutput, iSize },
				&iOffset, &Parsed
			) == XHTTP_NEXT_ITEM) &&
			(iOffset == iSize) &&
			(Parsed.Type == Value.Type) &&
			(Parsed.Number == Value.Number),
			"structured Decimal property round trip failed"
		);
	}
}



/* 写出再解码任意长度二进制，验证 Base64 边界。 */
static void testStructuredBytesRoundTrip(void)
{
	unsigned char arrData[96];
	unsigned char arrDecoded[96];
	char arrOutput[192];
	xhttpstructuredbare Parsed;
	xhttpstructuredvalue Value;
	size_t iDecoded;
	size_t iOffset;
	size_t iSize;
	size_t i;
	size_t j;

	for ( i = 0; i < sizeof(arrData); i++ ) {
		arrData[i] = (unsigned char)((i * 131u) + 17u);
	}
	memset(&Value, 0, sizeof(Value));
	Value.Type = XHTTP_STRUCTURED_BYTES;
	for ( i = 0; i <= sizeof(arrData); i++ ) {
		Value.Data = (xstrview){ (const char*)arrData, i };
		testRequire(
			xrtHttpStructuredBareWrite(
				&Value, arrOutput, sizeof(arrOutput), &iSize
			),
			"structured Bytes property write failed"
		);
		iOffset = 0;
		testRequire(
			(xrtHttpStructuredBareNext(
				(xstrview){ arrOutput, iSize },
				&iOffset, &Parsed
			) == XHTTP_NEXT_ITEM) &&
			xrtHttpStructuredBytesDecode(
				&Parsed, arrDecoded, sizeof(arrDecoded), &iDecoded
			) && (iDecoded == i),
			"structured Bytes property parse failed"
		);
		for ( j = 0; j < i; j++ ) {
			testRequire(
				arrDecoded[j] == arrData[j],
				"structured Bytes property data mismatch"
			);
		}
	}
}



/* 写出再解码可打印 ASCII，重点覆盖两种 String 转义。 */
static void testStructuredStringRoundTrip(void)
{
	char arrData[96];
	char arrDecoded[96];
	char arrOutput[256];
	xhttpstructuredbare Parsed;
	xhttpstructuredvalue Value;
	size_t iDecoded;
	size_t iOffset;
	size_t iSize;
	size_t i;
	size_t j;

	memset(&Value, 0, sizeof(Value));
	Value.Type = XHTTP_STRUCTURED_STRING;
	for ( i = 0; i <= sizeof(arrData); i++ ) {
		for ( j = 0; j < i; j++ ) {
			arrData[j] = (char)(0x20u +
				((j * 37u + i) % 0x5Fu));
		}
		Value.Data = (xstrview){ arrData, i };
		testRequire(
			xrtHttpStructuredBareWrite(
				&Value, arrOutput, sizeof(arrOutput), &iSize
			),
			"structured String property write failed"
		);
		iOffset = 0;
		testRequire(
			(xrtHttpStructuredBareNext(
				(xstrview){ arrOutput, iSize },
				&iOffset, &Parsed
			) == XHTTP_NEXT_ITEM) &&
			xrtHttpStructuredStringDecode(
				&Parsed, arrDecoded, sizeof(arrDecoded), &iDecoded
			) && (iDecoded == i) &&
			((i == 0) || (memcmp(arrDecoded, arrData, i) == 0)),
			"structured String property round trip failed"
		);
	}
}



/* 运行 Structured Fields 写出与解析闭环属性测试。 */
int main(void)
{
	testStructuredNumberRoundTrip();
	testStructuredBytesRoundTrip();
	testStructuredStringRoundTrip();
	printf("[PASS] http_structured_property\n");
	return 0;
}
