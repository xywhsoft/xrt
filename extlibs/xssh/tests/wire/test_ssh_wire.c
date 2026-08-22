#include "../test.h"



/* 验证 reader/writer 初始化与无效状态处理。 */
static void testSshWireInit(void)
{
	static const unsigned char arrInput[] = { 1u, 2u, 3u };
	xsshreader Reader = { XRT_BYTES_INIT("keep"), 2u };
	xsshwriter Writer = { NULL, 0u, 0u };
	xbytesview Invalid = { NULL, 1u };

	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrInput, sizeof(arrInput) }
	) && (xrtSshReaderRemaining(&Reader) == sizeof(arrInput)),
		"ssh reader init failed");
	testRequire(!xrtSshReaderInit(&Reader, Invalid) &&
		(Reader.Source.Data == arrInput) && (Reader.Position == 0u),
		"ssh reader invalid init changed state");
	testRequire(xrtSshWriterInit(&Writer, NULL, 0u) &&
		(Writer.Data == NULL) && (Writer.Capacity == 0u) &&
		(Writer.Size == 0u), "ssh empty writer init failed");
	testRequire(!xrtSshWriterInit(&Writer, NULL, 1u) &&
		(Writer.Data == NULL) && (Writer.Capacity == 0u),
		"ssh writer invalid init changed state");
	testRequire(!xrtSshReaderInit(NULL, XRT_BYTES_LITERAL("")) &&
		!xrtSshWriterInit(NULL, NULL, 0u) &&
		(xrtSshReaderRemaining(NULL) == 0u) &&
		(xrtSshWriterRemaining(NULL) == 0u) &&
		(xrtSshWriterReserve(NULL, 0u) == XSSH_ERROR_ARGUMENT),
		"ssh init accepted null object");
}



/* 验证 writer 剩余容量和只检查、不提交的预留契约。 */
static void testSshWireWriterReserve(void)
{
	unsigned char arrOutput[8];
	xbytesview arrInputs[1];
	xsshwriter Writer;
	xsshwriter Invalid = { NULL, 1u, 0u };

	testRequire(xrtSshWriterInit(&Writer, arrOutput, sizeof(arrOutput)),
		"ssh writer reserve setup failed");
	Writer.Size = 3u;
	testRequire((xrtSshWriterRemaining(&Writer) == 5u) &&
		(xrtSshWriterReserve(&Writer, 0u) == XSSH_OK) &&
		(xrtSshWriterReserve(&Writer, 5u) == XSSH_OK) &&
		(xrtSshWriterReserve(&Writer, 6u) == XSSH_ERROR_SPACE) &&
		(Writer.Size == 3u), "ssh writer reserve changed state");
	testRequire((xrtSshWriterRemaining(&Invalid) == 0u) &&
		(xrtSshWriterReserve(&Invalid, 0u) == XSSH_ERROR_ARGUMENT),
		"ssh writer reserve accepted invalid state");
	Invalid.Data = arrOutput;
	Invalid.Capacity = 1u;
	Invalid.Size = 2u;
	testRequire((xrtSshWriterRemaining(&Invalid) == 0u) &&
		(xrtSshWriterReserve(&Invalid, 0u) == XSSH_ERROR_ARGUMENT),
		"ssh writer reserve accepted oversized position");

	Writer.Size = 0u;
	arrInputs[0] = XRT_BYTES_LITERAL("external");
	testRequire(xrtSshWriterReserveInputs(
		&Writer,
		5u,
		arrInputs,
		1u
	) == XSSH_OK, "ssh writer rejected disjoint input");
	arrInputs[0].Data = arrOutput + 2u;
	arrInputs[0].Size = 1u;
	testRequire((xrtSshWriterReserveInputs(
		&Writer,
		5u,
		arrInputs,
		1u
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh writer accepted overlapping input");
	testRequire(xrtSshWriterReserveInputs(
		&Writer,
		1u,
		NULL,
		1u
	) == XSSH_ERROR_ARGUMENT, "ssh writer accepted missing input array");
}



/* 验证整数、boolean 和原始字节的网络字节序往返。 */
static void testSshWireNumbers(void)
{
	unsigned char arrWire[64];
	xsshwriter Writer;
	xsshreader Reader;
	xbytesview Raw;
	uint8 iByte;
	uint32 iU32;
	uint64 iU64;
	bool bValue;

	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshWriteByte(&Writer, 0xa5u) == XSSH_OK) &&
		(xrtSshWriteBool(&Writer, true) == XSSH_OK) &&
		(xrtSshWriteU32(&Writer, UINT32_C(0x12345678)) == XSSH_OK) &&
		(xrtSshWriteU64(&Writer, UINT64_C(0x0102030405060708)) == XSSH_OK) &&
		(xrtSshWriteBytes(&Writer, XRT_BYTES_LITERAL("raw")) == XSSH_OK),
		"ssh numeric writer failed");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshReadByte(&Reader, &iByte) == XSSH_OK) &&
		(iByte == 0xa5u) &&
		(xrtSshReadBool(&Reader, &bValue) == XSSH_OK) && bValue &&
		(xrtSshReadU32(&Reader, &iU32) == XSSH_OK) &&
		(iU32 == UINT32_C(0x12345678)) &&
		(xrtSshReadU64(&Reader, &iU64) == XSSH_OK) &&
		(iU64 == UINT64_C(0x0102030405060708)) &&
		(xrtSshReadBytes(&Reader, 3u, &Raw) == XSSH_OK) &&
		testSshBytesEqual(Raw, XRT_BYTES_LITERAL("raw")) &&
		(xrtSshReaderRemaining(&Reader) == 0u),
		"ssh numeric reader mismatch");

	arrWire[0] = 0x7fu;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, 1u }
	) && (xrtSshReadBool(&Reader, &bValue) == XSSH_OK) && bValue,
		"ssh boolean rejected nonzero true value");
}



/* 验证输入不足和输出不足不会发布部分状态。 */
static void testSshWireAtomic(void)
{
	static const unsigned char arrPartial[] = { 0u, 0u, 0u, 4u, 'x' };
	unsigned char arrOutput[8] = { 0xaau, 0xbbu, 0xccu, 0xddu };
	xsshreader Reader;
	xsshwriter Writer;
	xbytesview Value = XRT_BYTES_LITERAL("keep");
	uint64 iNumber = UINT64_C(0x1122334455667788);

	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrPartial, sizeof(arrPartial) }
	) && (xrtSshReadString(&Reader, &Value) == XSSH_NEED_MORE) &&
		(Reader.Position == 0u) &&
		testSshBytesEqual(Value, XRT_BYTES_LITERAL("keep")),
		"ssh partial string changed reader or output");
	testRequire((xrtSshReadU64(&Reader, &iNumber) == XSSH_NEED_MORE) &&
		(Reader.Position == 0u) &&
		(iNumber == UINT64_C(0x1122334455667788)),
		"ssh partial integer changed reader or output");

	testRequire(xrtSshWriterInit(&Writer, arrOutput, 3u),
		"ssh short writer setup failed");
	Writer.Size = 1u;
	testRequire((xrtSshWriteU32(&Writer, 1u) == XSSH_ERROR_SPACE) &&
		(Writer.Size == 1u) && (arrOutput[0] == 0xaau) &&
		(arrOutput[1] == 0xbbu), "ssh short integer write was partial");
	testRequire((xrtSshWriteString(&Writer, XRT_BYTES_LITERAL("x")) ==
		XSSH_ERROR_SPACE) && (Writer.Size == 1u) &&
		(arrOutput[1] == 0xbbu), "ssh short string write was partial");
	testRequire((xrtSshWriteBytes(
		&Writer,
		(xbytesview){ NULL, 1u }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 1u),
		"ssh invalid raw write changed state");
}



/* 验证 SSH string 的借用视图、空值与声明长度边界。 */
static void testSshWireString(void)
{
	static const unsigned char arrTooLong[] = { 0xffu, 0xffu, 0xffu, 0xffu };
	unsigned char arrWire[16];
	unsigned char iDummy = 0u;
	xsshwriter Writer;
	xsshreader Reader;
	xbytesview Value;

	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshWriteString(&Writer, XRT_BYTES_LITERAL("abc")) == XSSH_OK) &&
		(xrtSshWriteString(&Writer, (xbytesview){ NULL, 0u }) == XSSH_OK),
		"ssh string write failed");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshReadString(&Reader, &Value) == XSSH_OK) &&
		testSshBytesEqual(Value, XRT_BYTES_LITERAL("abc")) &&
		(xrtSshReadString(&Reader, &Value) == XSSH_OK) &&
		(Value.Size == 0u), "ssh string read mismatch");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrTooLong, sizeof(arrTooLong) }
	) && (xrtSshReadString(&Reader, &Value) == XSSH_NEED_MORE) &&
		(Reader.Position == 0u), "ssh oversized declared string advanced input");

	#if SIZE_MAX > UINT32_MAX
		testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
			(xrtSshWriteString(
				&Writer,
				(xbytesview){ &iDummy, (size_t)UINT32_MAX + 1u }
			) == XSSH_ERROR_OVERFLOW) && (Writer.Size == 0u),
			"ssh oversized string was not rejected before access");
	#else
		(void)iDummy;
	#endif
}



/* 验证 name-list 语法、去重与首选算法协商。 */
static void testSshWireNameList(void)
{
	static const char arrControl[] = { 'a', ',', 0x1f, 'b' };
	static const char arrDelete[] = { 'a', ',', 0x7f, 'b' };
	static const char arrHigh[] = { 'a', ',', (char)0x80, 'b' };
	unsigned char arrWire[64];
	xsshwriter Writer;
	xstrview Match = XRT_STR_LITERAL("keep");

	testRequire(xrtSshNameListValid((xstrview){ NULL, 0u }) &&
		xrtSshNameListValid(XRT_STR_LITERAL("curve25519-sha256,aes128-gcm")) &&
		!xrtSshNameListValid(XRT_STR_LITERAL(",a")) &&
		!xrtSshNameListValid(XRT_STR_LITERAL("a,")) &&
		!xrtSshNameListValid(XRT_STR_LITERAL("a,,b")) &&
		!xrtSshNameListValid(XRT_STR_LITERAL("a b")) &&
		!xrtSshNameListValid((xstrview){ arrControl, sizeof(arrControl) }) &&
		!xrtSshNameListValid((xstrview){ arrDelete, sizeof(arrDelete) }) &&
		!xrtSshNameListValid((xstrview){ arrHigh, sizeof(arrHigh) }),
		"ssh name-list validation mismatch");
	testRequire(xrtSshNameListContains(
		XRT_STR_LITERAL("a,b,c"),
		XRT_STR_LITERAL("b")
	) && !xrtSshNameListContains(
		XRT_STR_LITERAL("a,bb,c"),
		XRT_STR_LITERAL("b")
	) && xrtSshNameListHasDuplicate(XRT_STR_LITERAL("a,b,a")) &&
		!xrtSshNameListHasDuplicate(XRT_STR_LITERAL("a,b,c")),
		"ssh name-list membership mismatch");
	testRequire((xrtSshNameListFirstMatch(
		XRT_STR_LITERAL("b,a"),
		XRT_STR_LITERAL("a,b"),
		&Match
	) == XSSH_OK) && testSshTextEqual(Match, XRT_STR_LITERAL("b")),
		"ssh name-list preference order mismatch");
	Match = XRT_STR_LITERAL("keep");
	testRequire((xrtSshNameListFirstMatch(
		XRT_STR_LITERAL("x,y"),
		XRT_STR_LITERAL("a,b"),
		&Match
	) == XSSH_ERROR_UNSUPPORTED) &&
		testSshTextEqual(Match, XRT_STR_LITERAL("keep")),
		"ssh name-list miss changed output");
	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshWriteNameList(&Writer, XRT_STR_LITERAL("a,b")) == XSSH_OK) &&
		(Writer.Size == 7u) &&
		(xrtSshWriteNameList(&Writer, XRT_STR_LITERAL("a,,b")) ==
			XSSH_ERROR_ARGUMENT) && (Writer.Size == 7u),
		"ssh name-list write mismatch");
}



/* 验证跨协议层复用的 language tag 语法。 */
static void testSshWireLanguage(void)
{
	static const char arrSpace[] = { 'e', 'n', ' ', 'U', 'S' };
	static const char arrControl[] = { 'e', 'n', '\n' };
	static const char arrDelete[] = { 'e', 'n', 0x7f };
	static const char arrHigh[] = { 'e', 'n', (char)0x80 };

	testRequire(xrtSshLanguageValid((xstrview){ NULL, 0u }) &&
		xrtSshLanguageValid(XRT_STR_LITERAL("en-US")) &&
		!xrtSshLanguageValid((xstrview){ NULL, 1u }) &&
		!xrtSshLanguageValid((xstrview){ arrSpace, sizeof(arrSpace) }) &&
		!xrtSshLanguageValid((xstrview){ arrControl, sizeof(arrControl) }) &&
		!xrtSshLanguageValid((xstrview){ arrDelete, sizeof(arrDelete) }) &&
		!xrtSshLanguageValid((xstrview){ arrHigh, sizeof(arrHigh) }),
		"ssh language validation mismatch");
}



/* 验证非负 mpint 的规范化和错误回滚。 */
static void testSshWireMpint(void)
{
	static const unsigned char arrMagnitude[] = { 0u, 0u, 0x80u, 1u };
	static const unsigned char arrExpected[] = {
		0u, 0u, 0u, 3u, 0u, 0x80u, 1u
	};
	static const unsigned char arrBadZero[] = { 0u, 0u, 0u, 1u, 0u };
	static const unsigned char arrBadNegative[] = { 0u, 0u, 0u, 1u, 0x80u };
	static const unsigned char arrBadExtend[] = { 0u, 0u, 0u, 2u, 0u, 0x7fu };
	unsigned char arrWire[16];
	xsshwriter Writer;
	xsshreader Reader;
	xbytesview Value = XRT_BYTES_LITERAL("keep");

	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshWriteMpint(
			&Writer,
			(xbytesview){ arrMagnitude, sizeof(arrMagnitude) }
		) == XSSH_OK) && (Writer.Size == sizeof(arrExpected)) &&
		(memcmp(arrWire, arrExpected, sizeof(arrExpected)) == 0),
		"ssh positive mpint encoding mismatch");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshReadMpint(&Reader, &Value) == XSSH_OK) &&
		testSshBytesEqual(Value, (xbytesview){ arrExpected + 4u, 3u }),
		"ssh positive mpint read mismatch");
	testRequire(xrtSshWriterInit(&Writer, arrWire, 4u) &&
		(xrtSshWriteMpint(&Writer, XRT_BYTES_LITERAL("\0\0")) == XSSH_OK) &&
		(Writer.Size == 4u) &&
		(memcmp(arrWire, "\0\0\0\0", 4u) == 0),
		"ssh zero mpint encoding mismatch");

	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrBadZero, sizeof(arrBadZero) }
	) && (xrtSshReadMpint(&Reader, &Value) == XSSH_ERROR_PROTOCOL) &&
		(Reader.Position == 0u), "ssh noncanonical zero mpint advanced reader");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrBadNegative, sizeof(arrBadNegative) }
	) && (xrtSshReadMpint(&Reader, &Value) == XSSH_ERROR_PROTOCOL) &&
		(Reader.Position == 0u), "ssh negative positive-mpint advanced reader");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrBadExtend, sizeof(arrBadExtend) }
	) && (xrtSshReadMpint(&Reader, &Value) == XSSH_ERROR_PROTOCOL) &&
		(Reader.Position == 0u), "ssh extended positive mpint advanced reader");
}



/* 验证负 mpint 的二进制补码编码和规范读取。 */
static void testSshWireSignedMpint(void)
{
	static const unsigned char arrMinusOne[] = { 0u, 0u, 0u, 1u, 0xffu };
	static const unsigned char arrMinus128[] = { 0u, 0u, 0u, 1u, 0x80u };
	static const unsigned char arrMinus129[] = { 0u, 0u, 0u, 2u, 0xffu, 0x7fu };
	static const unsigned char arrBadNegative[] = { 0u, 0u, 0u, 2u, 0xffu, 0x80u };
	unsigned char arrWire[16];
	unsigned char arrMagnitude[] = { 0x81u };
	xsshwriter Writer;
	xsshreader Reader;
	xbytesview Value = XRT_BYTES_LITERAL("keep");
	bool bNegative = false;

	testRequire(xrtSshWriterInit(&Writer, arrWire, 5u) &&
		(xrtSshWriteSignedMpint(
			&Writer,
			XRT_BYTES_LITERAL("\1"),
			true
		) == XSSH_OK) && (Writer.Size == sizeof(arrMinusOne)) &&
		(memcmp(arrWire, arrMinusOne, sizeof(arrMinusOne)) == 0),
		"ssh minus one encoding mismatch");
	testRequire(xrtSshWriterInit(&Writer, arrWire, 5u) &&
		(xrtSshWriteSignedMpint(
			&Writer,
			XRT_BYTES_LITERAL("\x80"),
			true
		) == XSSH_OK) &&
		(memcmp(arrWire, arrMinus128, sizeof(arrMinus128)) == 0),
		"ssh minus 128 exact-capacity encoding mismatch");
	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshWriteSignedMpint(
			&Writer,
			(xbytesview){ arrMagnitude, sizeof(arrMagnitude) },
			true
		) == XSSH_OK) && (Writer.Size == sizeof(arrMinus129)) &&
		(memcmp(arrWire, arrMinus129, sizeof(arrMinus129)) == 0),
		"ssh minus 129 encoding mismatch");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshReadSignedMpint(&Reader, &Value, &bNegative) == XSSH_OK) &&
		bNegative && testSshBytesEqual(
			Value,
			(xbytesview){ arrMinus129 + 4u, 2u }
		), "ssh signed mpint read mismatch");

	Value = XRT_BYTES_LITERAL("keep");
	bNegative = false;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrBadNegative, sizeof(arrBadNegative) }
	) && (xrtSshReadSignedMpint(
		&Reader,
		&Value,
		&bNegative
	) == XSSH_ERROR_PROTOCOL) && (Reader.Position == 0u) &&
		!bNegative && testSshBytesEqual(Value, XRT_BYTES_LITERAL("keep")),
		"ssh noncanonical signed mpint changed state");
}



/* 构造边界 identification 行并验证增量 banner 解析。 */
static void testSshWireBanner(void)
{
	static const char sPrelude[] = "notice\r\nSSH-2.0-xssh_test comment\r\nrest";
	static const char sCompat[] = "SSH-1.99-compat\n";
	char arrExact[XSSH_IDENTIFICATION_MAX + 1u];
	char arrLong[XSSH_IDENTIFICATION_MAX + 2u];
	char arrWire[XSSH_IDENTIFICATION_MAX];
	xstrview Banner = XRT_STR_LITERAL("keep");
	xsshwriter Writer;
	size_t iConsumed = SIZE_MAX;

	testRequire((xrtSshBannerRead(
		XRT_STR_LITERAL(sPrelude),
		&Banner,
		&iConsumed
	) == XSSH_OK) && testSshTextEqual(
		Banner,
		XRT_STR_LITERAL("SSH-2.0-xssh_test comment")
	) && (iConsumed == sizeof("notice\r\nSSH-2.0-xssh_test comment\r\n") - 1u),
		"ssh banner prelude parse mismatch");
	testRequire((xrtSshBannerRead(
		XRT_STR_LITERAL(sCompat),
		&Banner,
		&iConsumed
	) == XSSH_OK) && testSshTextEqual(
		Banner,
		XRT_STR_LITERAL("SSH-1.99-compat")
	), "ssh 1.99 banner parse mismatch");

	Banner = XRT_STR_LITERAL("keep");
	iConsumed = 77u;
	testRequire((xrtSshBannerRead(
		XRT_STR_LITERAL("SSH-2.0-partial\r"),
		&Banner,
		&iConsumed
	) == XSSH_NEED_MORE) &&
		testSshTextEqual(Banner, XRT_STR_LITERAL("keep")) &&
		(iConsumed == 77u), "ssh partial banner changed outputs");
	testRequire(xrtSshBannerRead(
		XRT_STR_LITERAL("SSH-1.5-old\r\n"),
		&Banner,
		&iConsumed
	) == XSSH_ERROR_UNSUPPORTED, "ssh unsupported version was accepted");
	testRequire(xrtSshBannerRead(
		XRT_STR_LITERAL("bad\0line\r\nSSH-2.0-x\r\n"),
		&Banner,
		&iConsumed
	) == XSSH_ERROR_PROTOCOL, "ssh banner accepted NUL control");
	testRequire(xrtSshBannerRead(
		XRT_STR_LITERAL("bad\rline\n"),
		&Banner,
		&iConsumed
	) == XSSH_ERROR_PROTOCOL, "ssh banner accepted bare CR");
	testRequire(xrtSshBannerRead(
		XRT_STR_LITERAL("SSH-2.0-\n"),
		&Banner,
		&iConsumed
	) == XSSH_ERROR_PROTOCOL, "ssh banner accepted empty software version");
	testRequire(xrtSshBannerRead(
		XRT_STR_LITERAL("SSH-2.0-xssh-test\r\n"),
		&Banner,
		&iConsumed
	) == XSSH_ERROR_PROTOCOL, "ssh banner accepted softwareversion hyphen");

	memset(arrExact, 'x', sizeof(arrExact));
	memcpy(arrExact, "SSH-2.0-", 8u);
	arrExact[XSSH_IDENTIFICATION_MAX - 1u] = '\n';
	arrExact[XSSH_IDENTIFICATION_MAX] = '\0';
	testRequire(xrtSshBannerRead(
		(xstrview){ arrExact, XSSH_IDENTIFICATION_MAX },
		&Banner,
		&iConsumed
	) == XSSH_OK, "ssh exact-limit banner was rejected");
	memset(arrLong, 'x', sizeof(arrLong));
	memcpy(arrLong, "SSH-2.0-", 8u);
	arrLong[XSSH_IDENTIFICATION_MAX] = '\n';
	arrLong[XSSH_IDENTIFICATION_MAX + 1u] = '\0';
	testRequire(xrtSshBannerRead(
		(xstrview){ arrLong, XSSH_IDENTIFICATION_MAX + 1u },
		&Banner,
		&iConsumed
	) == XSSH_ERROR_OVERFLOW, "ssh over-limit banner was accepted");
	testRequire(xrtSshBannerRead(
		(xstrview){ arrLong, XSSH_IDENTIFICATION_MAX },
		&Banner,
		&iConsumed
	) == XSSH_ERROR_OVERFLOW, "ssh over-limit partial line was retained");

	/* 写入路径只接受本端 SSH-2.0 identification，并保持失败事务性。 */
	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshBannerWrite(
			&Writer,
			XRT_STR_LITERAL("SSH-2.0-xssh test")
		) == XSSH_OK) && (Writer.Size == sizeof("SSH-2.0-xssh test\r\n") - 1u) &&
		(memcmp(arrWire, "SSH-2.0-xssh test\r\n", Writer.Size) == 0),
		"ssh banner write mismatch");
	Writer.Size = 0u;
	testRequire((xrtSshBannerWrite(
		&Writer,
		XRT_STR_LITERAL("SSH-1.99-compat")
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u) &&
		(xrtSshBannerWrite(
			&Writer,
			XRT_STR_LITERAL("SSH-2.0- bad")
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u) &&
		(xrtSshBannerWrite(
			&Writer,
			XRT_STR_LITERAL("SSH-2.0-xssh-test")
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh invalid local banner changed writer");
	memset(arrExact, 'x', XSSH_IDENTIFICATION_MAX - 2u);
	memcpy(arrExact, "SSH-2.0-", 8u);
	testRequire((xrtSshBannerWrite(
		&Writer,
		(xstrview){ arrExact, XSSH_IDENTIFICATION_MAX - 2u }
	) == XSSH_OK) && (Writer.Size == XSSH_IDENTIFICATION_MAX),
		"ssh exact-limit local banner was rejected");
}



/* 运行 SSH wire 的协议与边界测试。 */
int main(void)
{
	testSshWireInit();
	testSshWireWriterReserve();
	testSshWireNumbers();
	testSshWireAtomic();
	testSshWireString();
	testSshWireNameList();
	testSshWireLanguage();
	testSshWireMpint();
	testSshWireSignedMpint();
	testSshWireBanner();
	return 0;
}
