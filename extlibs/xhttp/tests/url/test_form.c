#include "../test.h"



/* 验证 WHATWG 安全集合、空格加号规则和严格解码。 */
static void testFormCodec(void)
{
	static const uint8 UnicodeText[] = {
		'1', '+', '1', ' ', 0xE2, 0x89, 0xA1, ' ', '2',
		'%', '2', '0', 0xE2, 0x80, 0xBD
	};
	char Text[128];
	uint8 Data[128];
	size_t iSize;

	testRequire(xrtFormEncode(
		"AZaz09*-._~ +", 13, Text, sizeof(Text), &iSize
	) && (strcmp(Text, "AZaz09*-._%7E+%2B") == 0),
		"form safe-set vector mismatch");
	testRequire(xrtFormEncode(
		UnicodeText, sizeof(UnicodeText), Text, sizeof(Text), &iSize
	) && (strcmp(
		Text, "1%2B1+%E2%89%A1+2%2520%E2%80%BD"
	) == 0), "form UTF-8 byte vector mismatch");
	testRequire(xrtFormDecode(
		(xstrview){ Text, iSize }, Data, sizeof(Data), &iSize
	) && (iSize == sizeof(UnicodeText)) &&
		(memcmp(Data, UnicodeText, sizeof(UnicodeText)) == 0),
		"form codec round trip mismatch");
	testRequire(xrtFormDecode(
		XRT_STR_LITERAL("a+b%2Bc"), Data, sizeof(Data), &iSize
	) && (iSize == 5) && (memcmp(Data, "a b+c", 5) == 0),
		"form plus decode mismatch");
	testRequire(xrtFormDecode(
		XRT_STR_LITERAL("%00"), Data, sizeof(Data), &iSize
	) && (iSize == 1) && (Data[0] == 0),
		"form embedded-zero decode mismatch");
}



/* 验证全部字节都能无损往返。 */
static void testFormAllBytes(void)
{
	uint8 Input[256];
	char Encoded[769];
	uint8 Decoded[256];
	size_t iEncoded;
	size_t iDecoded;
	size_t i;

	for ( i = 0; i < sizeof(Input); i++ ) {
		Input[i] = (uint8)i;
	}
	testRequire(xrtFormEncode(
		Input, sizeof(Input), Encoded, sizeof(Encoded), &iEncoded
	) && (iEncoded == 634), "form all-byte encoded size mismatch");
	testRequire(xrtFormDecode(
		(xstrview){ Encoded, iEncoded }, Decoded, sizeof(Decoded), &iDecoded
	) && (iDecoded == sizeof(Input)) &&
		(memcmp(Decoded, Input, sizeof(Input)) == 0),
		"form all-byte round trip mismatch");
}



/* 按字节验证一个原地解码字段。 */
static void testFormField(
	const xformfield* pField,
	const void* pName,
	size_t iName,
	const void* pValue,
	size_t iValue
)
{
	testRequire((pField->Name.Size == iName) &&
		(pField->Value.Size == iValue),
		"form parsed field size mismatch");
	testRequire((iName == 0) ||
		(memcmp(pField->Name.Data, pName, iName) == 0),
		"form parsed field name mismatch");
	testRequire((iValue == 0) ||
		(memcmp(pField->Value.Data, pValue, iValue) == 0),
		"form parsed field value mismatch");
}



/* 验证预检不修改输入，正式解析原地解码并统一缺失值为空值。 */
static void testFormParse(void)
{
	static const char Source[] =
		"name=alice+bob&&empty=&flag&%00=%7E&?a=b&";
	char Text[sizeof(Source)];
	char Before[sizeof(Source)];
	xformfield Fields[8];
	size_t iCount;

	memcpy(Text, Source, sizeof(Source));
	memcpy(Before, Text, sizeof(Text));
	testRequire(xrtFormParse(
		Text, sizeof(Source) - 1u, NULL, 0, &iCount, NULL
	) && (iCount == 5) && (memcmp(Text, Before, sizeof(Text)) == 0),
		"form parse preflight mutated input");
	testRequire(xrtFormParse(
		Text, sizeof(Source) - 1u, Fields,
		sizeof(Fields) / sizeof(Fields[0]), &iCount, NULL
	) && (iCount == 5), "form in-place parse failed");
	testFormField(&Fields[0], "name", 4, "alice bob", 9);
	testFormField(&Fields[1], "empty", 5, "", 0);
	testFormField(&Fields[2], "flag", 4, "", 0);
	{
		static const uint8 Zero[] = { 0 };

		testFormField(&Fields[3], Zero, 1, "~", 1);
	}
	testFormField(&Fields[4], "?a", 2, "b", 1);
}



/* 验证只读表单可以按解码后名称直接查找，并通过游标遍历重复字段。 */
static void testFormFind(void)
{
	static const xstrview Text =
		XRT_STR_INIT("na+me=first&na%20me=%00%2B&flag&?a=b");
	uint8 Value[16];
	uint8 Before[16];
	size_t iOffset = 0;
	size_t iSize = 77;

	testRequire(xrtFormFind(
		Text, XRT_BYTES_LITERAL("na me"), &iOffset,
		Value, sizeof(Value), &iSize
	) == XFORM_FIND_FOUND && (iSize == 5) &&
		(memcmp(Value, "first", 5) == 0),
		"form first decoded-name lookup failed");
	testRequire(xrtFormFind(
		Text, XRT_BYTES_LITERAL("na me"), &iOffset,
		Value, sizeof(Value), &iSize
	) == XFORM_FIND_FOUND && (iSize == 2) &&
		(Value[0] == 0) && (Value[1] == (uint8)'+'),
		"form duplicate decoded-name lookup failed");
	testRequire(xrtFormFind(
		Text, XRT_BYTES_LITERAL("na me"), &iOffset,
		Value, sizeof(Value), &iSize
	) == XFORM_FIND_END, "form duplicate lookup did not finish");

	iOffset = 0;
	testRequire(xrtFormFind(
		Text, XRT_BYTES_LITERAL("flag"), &iOffset,
		NULL, 0, &iSize
	) == XFORM_FIND_FOUND && (iSize == 0),
		"form missing-equals field did not map to an empty value");
	iOffset = 0;
	testRequire(xrtFormFind(
		Text, XRT_BYTES_LITERAL("?a"), &iOffset,
		Value, sizeof(Value), &iSize
	) == XFORM_FIND_FOUND && (iSize == 1) && (Value[0] == (uint8)'b'),
		"form lookup incorrectly stripped a leading question mark");

	memset(Value, 0xA5, sizeof(Value));
	memcpy(Before, Value, sizeof(Value));
	iOffset = 0;
	iSize = 77;
	testRequire(xrtFormFind(
		XRT_STR_LITERAL("target=ok&late=%GG"),
		XRT_BYTES_LITERAL("target"), &iOffset,
		Value, sizeof(Value), &iSize
	) == XFORM_FIND_ERROR && (iOffset == 0) && (iSize == 77) &&
		(memcmp(Value, Before, sizeof(Value)) == 0),
		"form lookup returned before validating a malformed trailing field");
	iOffset = 0;
	iSize = 77;
	testRequire(xrtFormFind(
		XRT_STR_LITERAL("a=long"), XRT_BYTES_LITERAL("a"), &iOffset,
		Value, 2, &iSize
	) == XFORM_FIND_ERROR && (iOffset == 0) && (iSize == 4) &&
		(memcmp(Value, Before, sizeof(Value)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"form lookup short output was not atomic");
}



/* 验证字段序列化始终写出等号并正确编码二进制名称和值。 */
static void testFormWrite(void)
{
	static const uint8 ZeroName[] = { 0 };
	static const xformfield Fields[] = {
		{ XRT_BYTES_INIT("a b"), XRT_BYTES_INIT("~+") },
		{ XRT_BYTES_INIT(""), XRT_BYTES_INIT("") },
		{ { ZeroName, 1 }, XRT_BYTES_INIT("x") }
	};
	char Text[64];
	str sBuilt;
	size_t iSize;

	memset(Text, 0x5A, sizeof(Text));
	testRequire(xrtFormWrite(
		Fields, sizeof(Fields) / sizeof(Fields[0]),
		NULL, 0, &iSize
	) && (iSize == 18), "form write size mismatch");
	testRequire(xrtFormWrite(
		Fields, sizeof(Fields) / sizeof(Fields[0]),
		Text, iSize, &iSize
	) && (memcmp(Text, "a+b=%7E%2B&=&%00=x", iSize) == 0) &&
		((uint8)Text[iSize] == UINT8_C(0x5A)),
		"form write bytes or no-terminator contract mismatch");
	sBuilt = xrtFormBuild(
		Fields, sizeof(Fields) / sizeof(Fields[0]), &iSize
	);
	testRequire((sBuilt != NULL) &&
		(strcmp(sBuilt, "a+b=%7E%2B&=&%00=x") == 0),
		"form allocated build mismatch");
	xrtFree(sBuilt);
}



/* 验证原地 codec、限额、格式、容量与重叠失败契约。 */
static void testFormFailures(void)
{
	char Buffer[64];
	char Before[64];
	xformfield Fields[4];
	xformlimits Limits = { 2, 4, 5, 8 };
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	size_t iSize;

	memcpy(Buffer, "a b+", 4);
	testRequire(xrtFormEncode(
		Buffer, 4, Buffer, sizeof(Buffer), &iSize
	) && (strcmp(Buffer, "a+b%2B") == 0),
		"form in-place encode failed");
	testRequire(xrtFormDecode(
		(xstrview){ Buffer, iSize }, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 4) && (memcmp(Buffer, "a b+", 4) == 0),
		"form in-place decode failed");

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 77;
	testRequire(!xrtFormDecode(
		XRT_STR_LITERAL("ok%GG"), Buffer, sizeof(Buffer), &iSize
	) && (iSize == 77) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XCODEC_ERROR_PERCENT_FORMAT),
		"form malformed percent failure was not atomic");
	iSize = 77;
	testRequire(!xrtFormEncode(
		"a b", 3, Buffer, 3, &iSize
	) && (iSize == 3) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"form short encode was not atomic");

	memcpy(Buffer, "a=1&b=2&c=3", 11);
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 77;
	testRequire(!xrtFormParse(
		Buffer, 11, Fields, 2, &iSize, NULL
	) && (iSize == 3) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"form field-capacity failure mutated input");
	iSize = 77;
	testRequire(!xrtFormParse(
		Buffer, 11, Fields, 4, &iSize, &Limits
	) && (iSize == 77) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"form explicit limits were not enforced atomically");

	memcpy(Buffer, "a=1&b=%GG", 9);
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 77;
	testRequire(!xrtFormParse(
		Buffer, 9, Fields, 4, &iSize, NULL
	) && (iSize == 77) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"form late malformed field partially decoded input");

	iSize = 77;
	testRequire(!xrtFormParse(
		(ptr)Wrapped.Data, Wrapped.Size, Fields, 4, &iSize, NULL
	) && (iSize == 77) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"form accepted a wrapped input range");

	{
		xformfield Field = {
			{ (cbytes)Buffer, 1 }, XRT_BYTES_LITERAL("1")
		};

		iSize = 77;
		testRequire(!xrtFormWrite(
			&Field, 1, Buffer, sizeof(Buffer), &iSize
		) && (iSize == 77),
			"form accepted overlapping output and field bytes");
	}
}



/* 执行 form-urlencoded codec、解析、构建和安全边界测试。 */
int main(void)
{
	testFormCodec();
	testFormAllBytes();
	testFormParse();
	testFormFind();
	testFormWrite();
	testFormFailures();
	printf("[PASS] form_urlencoded\n");
	return 0;
}
