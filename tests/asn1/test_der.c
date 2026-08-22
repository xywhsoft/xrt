#include "../test.h"



/* 验证零拷贝游标、Peek、Enter 和常用 Universal 类型读取器。 */
static void testDerCursor(void)
{
	static const uint8 Document[] = {
		0x30, 0x13,
		0x01, 0x01, 0xFF,
		0x02, 0x02, 0x00, 0x80,
		0x06, 0x03, 0x2A, 0x03, 0x04,
		0x03, 0x02, 0x00, 0xA5,
		0x04, 0x01, 0x7F
	};
	static const uint8 Oid[] = { 0x2A, 0x03, 0x04 };
	xdercursor Root;
	xdercursor Children;
	xdervalue Value;
	xdervalue Peeked;
	xbytesview Bytes;
	uint64 iInteger;
	uint8 iUnused;
	bool bBoolean;

	testRequire(xrtDerValidate(Document, sizeof(Document)),
		"valid DER document failed validation");
	testRequire(xrtDerInit(&Root, Document, sizeof(Document)) &&
		(xrtDerRemaining(&Root) == sizeof(Document)),
		"DER root initialization failed");
	testRequire((xrtDerPeek(&Root, &Peeked) == XDER_VALUE) &&
		(Root.Offset == 0) && (Peeked.Raw.Size == sizeof(Document)),
		"DER peek changed cursor or returned wrong view");
	testRequire(xrtDerExpect(
		&Root, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true, &Value
	) && xrtDerDone(&Root) && xrtDerEnter(&Value, &Children),
		"DER sequence enter failed");

	testRequire((xrtDerRead(&Children, &Value) == XDER_VALUE) &&
		xrtDerBoolean(&Value, &bBoolean) && bBoolean,
		"DER boolean read failed");
	testRequire((xrtDerRead(&Children, &Value) == XDER_VALUE) &&
		xrtDerUInt64(&Value, &iInteger) && (iInteger == 128u),
		"DER unsigned integer read failed");
	testRequire((xrtDerRead(&Children, &Value) == XDER_VALUE) &&
		xrtDerOidEqual(&Value, Oid, sizeof(Oid)) &&
		xrtDerOid(&Value, &Bytes) && (Bytes.Size == sizeof(Oid)),
		"DER object identifier read failed");
	testRequire((xrtDerRead(&Children, &Value) == XDER_VALUE) &&
		xrtDerBitString(&Value, &Bytes, &iUnused) && (iUnused == 0) &&
		(Bytes.Size == 1) && (Bytes.Data[0] == 0xA5),
		"DER bit string read failed");
	testRequire((xrtDerRead(&Children, &Value) == XDER_VALUE) &&
		xrtDerOctets(&Value, &Bytes) && (Bytes.Size == 1) &&
		(Bytes.Data[0] == 0x7F), "DER octet string read failed");
	testRequire((xrtDerRead(&Children, &Value) == XDER_DONE) &&
		xrtDerDone(&Children) && (xrtDerRemaining(&Children) == 0),
		"DER child cursor did not finish exactly");
}



/* 验证高标签号和规范长长度都保留完整原始视图。 */
static void testDerTagAndLength(void)
{
	static const uint8 HighTag[] = { 0x5F, 0x81, 0x49, 0x01, 0xAA };
	uint8 LongValue[131];
	xdercursor Cursor;
	xdervalue Value;
	xbytesview Bytes;

	testRequire(xrtDerInit(&Cursor, HighTag, sizeof(HighTag)) &&
		(xrtDerRead(&Cursor, &Value) == XDER_VALUE) &&
		(Value.Tag.Class == XASN1_APPLICATION) && !Value.Tag.Constructed &&
		(Value.Tag.Number == 201u) && (Value.HeaderSize == 4u) &&
		(Value.Value.Size == 1u) && (Value.Value.Data[0] == 0xAA),
		"DER high-tag-number parsing failed");

	LongValue[0] = 0x04;
	LongValue[1] = 0x81;
	LongValue[2] = 0x80;
	memset(LongValue + 3, 0x5A, 128);
	testRequire(xrtDerValidate(LongValue, sizeof(LongValue)) &&
		xrtDerInit(&Cursor, LongValue, sizeof(LongValue)) &&
		(xrtDerRead(&Cursor, &Value) == XDER_VALUE) &&
		xrtDerOctets(&Value, &Bytes) && (Bytes.Size == 128u) &&
		(Bytes.Data[0] == 0x5A) && (Bytes.Data[127] == 0x5A),
		"DER long-form length parsing failed");
}



/* 验证 DER 对 BER 宽松形式和所有局部非规范值进行严格拒绝。 */
static void testDerRejectsMalformed(void)
{
	static const uint8 Case0[] = { 0x30, 0x80, 0x00, 0x00 };
	static const uint8 Case1[] = { 0x04, 0x81, 0x01, 0x00 };
	static const uint8 Case2[] = { 0x04, 0x82, 0x00, 0x80 };
	static const uint8 Case3[] = { 0x04, 0x02, 0x00 };
	static const uint8 Case4[] = { 0x1F, 0x1E, 0x00 };
	static const uint8 Case5[] = { 0x1F, 0x80, 0x1F, 0x00 };
	static const uint8 Case6[] = { 0x1F, 0x81 };
	static const uint8 Case7[] = { 0x1F, 0x90, 0x80, 0x80, 0x80, 0x00 };
	static const uint8 Case8[] = { 0x01, 0x01, 0x01 };
	static const uint8 Case9[] = { 0x02, 0x00 };
	static const uint8 Case10[] = { 0x02, 0x02, 0x00, 0x7F };
	static const uint8 Case11[] = { 0x02, 0x02, 0xFF, 0x80 };
	static const uint8 Case12[] = { 0x03, 0x00 };
	static const uint8 Case13[] = { 0x03, 0x02, 0x08, 0x00 };
	static const uint8 Case14[] = { 0x03, 0x02, 0x03, 0x07 };
	static const uint8 Case15[] = { 0x05, 0x01, 0x00 };
	static const uint8 Case16[] = { 0x06, 0x00 };
	static const uint8 Case17[] = { 0x06, 0x02, 0x80, 0x01 };
	static const uint8 Case18[] = { 0x06, 0x01, 0x81 };
	static const uint8 Case19[] = { 0x22, 0x00 };
	static const uint8 Case20[] = { 0x10, 0x00 };
	static const uint8 Case21[] = { 0x00, 0x00 };
	static const xbytesview Cases[] = {
		{ Case0, sizeof(Case0) }, { Case1, sizeof(Case1) },
		{ Case2, sizeof(Case2) }, { Case3, sizeof(Case3) },
		{ Case4, sizeof(Case4) }, { Case5, sizeof(Case5) },
		{ Case6, sizeof(Case6) }, { Case7, sizeof(Case7) },
		{ Case8, sizeof(Case8) }, { Case9, sizeof(Case9) },
		{ Case10, sizeof(Case10) }, { Case11, sizeof(Case11) },
		{ Case12, sizeof(Case12) }, { Case13, sizeof(Case13) },
		{ Case14, sizeof(Case14) }, { Case15, sizeof(Case15) },
		{ Case16, sizeof(Case16) }, { Case17, sizeof(Case17) },
		{ Case18, sizeof(Case18) }, { Case19, sizeof(Case19) },
		{ Case20, sizeof(Case20) }, { Case21, sizeof(Case21) }
	};

	for ( size_t i = 0; i < sizeof(Cases) / sizeof(Cases[0]); i++ ) {
		xrtClearError();
		testRequire(!xrtDerValidate(Cases[i].Data, Cases[i].Size) &&
			(xrtGetError() != NULL) &&
			(strcmp(xrtErrorDomain(xrtGetError()), "xrt.asn1") == 0) &&
			((xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) ||
			 (xrtErrorKind(xrtGetError()) == XERR_RANGE)),
			"malformed DER input was accepted or misdiagnosed");
	}
}



/* 验证完整树检查会拒绝尾随值、未排序 SET 和过深嵌套。 */
static void testDerTreeValidation(void)
{
	static const uint8 Trailing[] = { 0x05, 0x00, 0x05, 0x00 };
	static const uint8 SortedSet[] = {
		0x31, 0x05, 0x02, 0x01, 0x01, 0x04, 0x00
	};
	static const uint8 UnsortedSet[] = {
		0x31, 0x05, 0x04, 0x00, 0x02, 0x01, 0x01
	};
	uint8 Deep[512];
	size_t iOffset = sizeof(Deep) - 2u;
	size_t iSize = 2u;

	Deep[iOffset] = 0x05;
	Deep[iOffset + 1u] = 0;
	for ( size_t i = 0; i < 65u; i++ ) {
		size_t iHeader = iSize < 128u ? 2u : 3u;

		iOffset -= iHeader;
		Deep[iOffset] = 0x30;
		if ( iHeader == 2u ) {
			Deep[iOffset + 1u] = (uint8)iSize;
		} else {
			Deep[iOffset + 1u] = 0x81;
			Deep[iOffset + 2u] = (uint8)iSize;
		}
		iSize += iHeader;
	}

	testRequire(xrtDerValidate(SortedSet, sizeof(SortedSet)),
		"canonically sorted DER set was rejected");
	testRequire(!xrtDerValidate(Trailing, sizeof(Trailing)) &&
		(xrtErrorCode(xrtGetError()) == XASN1_ERROR_TRAILING),
		"DER validator accepted two top-level values");
	testRequire(!xrtDerValidate(UnsortedSet, sizeof(UnsortedSet)) &&
		(xrtErrorCode(xrtGetError()) == XASN1_ERROR_ORDER),
		"DER validator accepted an unsorted set");
	testRequire(!xrtDerValidate(Deep + iOffset, iSize) &&
		(xrtErrorCode(xrtGetError()) == XASN1_ERROR_DEPTH),
		"DER validator accepted excessive nesting");
}



/* 验证类型、范围和游标失败不会发布半个输出。 */
static void testDerFailureAtomicity(void)
{
	static const uint8 Negative[] = { 0x02, 0x01, 0xFF };
	static const uint8 TooWide[] = {
		0x02, 0x0A, 0x00, 0x80, 0, 0, 0, 0, 0, 0, 0, 0
	};
	static const uint8 Sequence[] = { 0x30, 0x02, 0x05, 0x00 };
	xdercursor Cursor;
	xdercursor BeforeCursor;
	xdervalue Value;
	xdervalue BeforeValue;
	xbytesview Bytes = { (const uint8*)1, 77 };
	uint64 iNumber = UINT64_MAX;

	memset(&Value, 0xA5, sizeof(Value));
	BeforeValue = Value;
	testRequire(xrtDerInit(&Cursor, Sequence, sizeof(Sequence)),
		"DER failure cursor initialization failed");
	BeforeCursor = Cursor;
	testRequire(!xrtDerExpect(
		&Cursor, XASN1_UNIVERSAL, (uint32)XASN1_SET, true, &Value
	) && (memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Value, &BeforeValue, sizeof(Value)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XASN1_ERROR_TYPE),
		"DER expect failure changed cursor or output");

	testRequire(xrtDerInit(&Cursor, Negative, sizeof(Negative)) &&
		(xrtDerRead(&Cursor, &Value) == XDER_VALUE) &&
		!xrtDerUnsigned(&Value, &Bytes) && (Bytes.Size == 77) &&
		(xrtErrorCode(xrtGetError()) == XASN1_ERROR_VALUE),
		"DER unsigned helper accepted a negative integer");
	testRequire(xrtDerInit(&Cursor, TooWide, sizeof(TooWide)) &&
		(xrtDerRead(&Cursor, &Value) == XDER_VALUE) &&
		!xrtDerUInt64(&Value, &iNumber) && (iNumber == UINT64_MAX) &&
		(xrtErrorCode(xrtGetError()) == XASN1_ERROR_RANGE),
		"DER uint64 helper accepted an oversized integer");
}



/* 验证 DER 写入器产生规范编码，并且失败不改变输出。 */
static void testDerWrite(void)
{
	static const uint8 Expected[] = {
		0x30, 0x17,
		0x02, 0x01, 0x7F,
		0x02, 0x01, 0xFF,
		0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01,
		0x03, 0x02, 0x00, 0xA0,
		0x05, 0x00
	};
	static const uint8 UnsortedSet[] = {
		0x04, 0x00, 0x02, 0x01, 0x01
	};
	xbuffer Content;
	xbuffer Output;
	xdercursor Cursor;
	xdervalue Value;
	int64 iSigned;
	uint64 iUnsigned;
	size_t iBefore;

	testRequire(xrtBufferInit(&Content) && xrtBufferInit(&Output),
		"DER writer buffer initialization failed");
	testRequire(xrtDerAppendUInt64(&Content, 127u) &&
		xrtDerAppendInt64(&Content, -1) &&
		xrtDerAppendOid(&Content, XRT_STR_LITERAL("1.2.840.113549.1.1.1")) &&
		xrtDerAppendBitString(&Content, XRT_BYTES_LITERAL("\xA0"), 0) &&
		xrtDerAppendNull(&Content) &&
		xrtDerAppend(
			&Output, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true,
			xrtBufferView(&Content)
		) && (Output.Size == sizeof(Expected)) &&
		(memcmp(Output.Data, Expected, sizeof(Expected)) == 0) &&
		xrtDerValidate(Output.Data, Output.Size),
		"DER writer did not produce the expected canonical document");

	testRequire(xrtDerInit(&Cursor, Output.Data, Output.Size) &&
		xrtDerExpect(&Cursor, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true, &Value) &&
		xrtDerEnter(&Value, &Cursor) &&
		(xrtDerRead(&Cursor, &Value) == XDER_VALUE) &&
		xrtDerUInt64(&Value, &iUnsigned) && (iUnsigned == 127u) &&
		(xrtDerRead(&Cursor, &Value) == XDER_VALUE) &&
		xrtDerInt64(&Value, &iSigned) && (iSigned == -1),
		"DER signed/unsigned writer round trip failed");

	iBefore = Output.Size;
	testRequire(!xrtDerAppend(
		&Output, XASN1_UNIVERSAL, (uint32)XASN1_SET, true,
		(xbytesview){ UnsortedSet, sizeof(UnsortedSet) }
	) && (Output.Size == iBefore),
		"DER writer accepted invalid constructed content or changed output");

	xrtBufferUnit(&Output);
	xrtBufferUnit(&Content);
}



/* 验证写入器在 128 和 256 字节边界生成最短的长格式长度。 */
static void testDerWriteLongLength(void)
{
	uint8 Content128[128];
	uint8 Content256[256];
	xbuffer Output128;
	xbuffer Output256;

	memset(Content128, 0x5A, sizeof(Content128));
	memset(Content256, 0xA5, sizeof(Content256));
	testRequire(xrtBufferInit(&Output128) && xrtBufferInit(&Output256),
		"DER long-length writer buffer initialization failed");

	testRequire(xrtDerAppend(
		&Output128, XASN1_UNIVERSAL, (uint32)XASN1_OCTET_STRING, false,
		(xbytesview){ Content128, sizeof(Content128) }
	) && (Output128.Size == (sizeof(Content128) + 3u)) &&
		(Output128.Data[0] == 0x04) && (Output128.Data[1] == 0x81) &&
		(Output128.Data[2] == 0x80) &&
		(Output128.Data[Output128.Size - 1u] == 0x5A) &&
		xrtDerValidate(Output128.Data, Output128.Size),
		"DER writer encoded the 128-byte length incorrectly");

	testRequire(xrtDerAppend(
		&Output256, XASN1_UNIVERSAL, (uint32)XASN1_OCTET_STRING, false,
		(xbytesview){ Content256, sizeof(Content256) }
	) && (Output256.Size == (sizeof(Content256) + 4u)) &&
		(Output256.Data[0] == 0x04) && (Output256.Data[1] == 0x82) &&
		(Output256.Data[2] == 0x01) && (Output256.Data[3] == 0x00) &&
		(Output256.Data[Output256.Size - 1u] == 0xA5) &&
		xrtDerValidate(Output256.Data, Output256.Size),
		"DER writer encoded the 256-byte length incorrectly");

	xrtBufferUnit(&Output256);
	xrtBufferUnit(&Output128);
}



/* 执行 ASN.1 DER 游标、规范编码、完整树和失败原子性测试。 */
int main(void)
{
	testDerCursor();
	testDerTagAndLength();
	testDerRejectsMalformed();
	testDerTreeValidation();
	testDerFailureAtomicity();
	testDerWrite();
	testDerWriteLongLength();
	printf("[PASS] asn1_der\n");
	return 0;
}
