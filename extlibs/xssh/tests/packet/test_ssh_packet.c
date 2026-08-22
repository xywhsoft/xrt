#include "../test.h"



typedef struct testsshpadding {
	uint8 Start;
	size_t Calls;
	bool Fail;
} testsshpadding;



/* 生成确定性 padding，并支持验证 callback 失败原子性。 */
static bool testSshPacketPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	testsshpadding* pPadding = (testsshpadding*)pUserData;
	bytes pBytes = (bytes)pOutput;
	size_t i;

	pPadding->Calls++;
	for ( i = 0u; i < iSize; ++i ) {
		pBytes[i] = (uint8)(pPadding->Start + (uint8)i);
	}
	return !pPadding->Fail;
}



/* 验证 padding 计算的块边界与输出原子性。 */
static void testSshPacketMeasureCases(void)
{
	uint8 iPadding = 0xaau;
	uint32 iPacket = UINT32_C(0x11223344);

	testRequire((xrtSshPacketMeasure(0u, 8u, &iPadding, &iPacket) == XSSH_OK) &&
		(iPadding == 11u) && (iPacket == 12u),
		"ssh packet empty padding mismatch");
	testRequire((xrtSshPacketMeasure(3u, 8u, &iPadding, &iPacket) == XSSH_OK) &&
		(iPadding == 8u) && (iPacket == 12u),
		"ssh packet three-byte padding mismatch");
	testRequire((xrtSshPacketMeasure(4u, 0u, &iPadding, &iPacket) == XSSH_OK) &&
		(iPadding == 7u) && (iPacket == 12u),
		"ssh packet default block mismatch");
	testRequire((xrtSshPacketMeasure(8u, 8u, &iPadding, &iPacket) == XSSH_OK) &&
		(iPadding == 11u) && (iPacket == 20u),
		"ssh packet aligned padding mismatch");
	testRequire((xrtSshPacketMeasure(4u, 16u, &iPadding, &iPacket) == XSSH_OK) &&
		(iPadding == 7u) && (iPacket == 12u),
		"ssh packet 16-byte block mismatch");

	iPadding = 0xaau;
	iPacket = UINT32_C(0x11223344);
	testRequire((xrtSshPacketMeasure(1u, 7u, &iPadding, &iPacket) ==
		XSSH_ERROR_ARGUMENT) && (iPadding == 0xaau) &&
		(iPacket == UINT32_C(0x11223344)),
		"ssh packet invalid block changed outputs");
	testRequire(xrtSshPacketMeasure(
		(size_t)UINT32_MAX,
		8u,
		&iPadding,
		&iPacket
	) == XSSH_ERROR_OVERFLOW, "ssh packet oversized payload accepted");
}



/* 验证 plain packet 的确定性线路格式、视图和序列号。 */
static void testSshPacketRoundtrip(void)
{
	static const unsigned char arrPayload[] = { 20u, 'a', 'b', 'c' };
	static const unsigned char arrExpected[] = {
		0u, 0u, 0u, 12u, 7u, 20u, 'a', 'b', 'c',
		0xe0u, 0xe1u, 0xe2u, 0xe3u, 0xe4u, 0xe5u, 0xe6u
	};
	unsigned char arrWire[64];
	testsshpadding Padding = { 0xe0u, 0u, false };
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;
	uint32 iWriteSequence = 7u;
	uint32 iReadSequence = 7u;

	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketWrite(
			&Writer,
			(xbytesview){ arrPayload, sizeof(arrPayload) },
			8u,
			&iWriteSequence,
			testSshPacketPadding,
			&Padding
		) == XSSH_OK) && (Writer.Size == sizeof(arrExpected)) &&
		(iWriteSequence == 8u) && (Padding.Calls == 1u) &&
		(memcmp(arrWire, arrExpected, sizeof(arrExpected)) == 0),
		"ssh packet write mismatch");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshPacketRead(
		&Reader,
		8u,
		0u,
		&iReadSequence,
		&Packet
	) == XSSH_OK) && (Reader.Position == Writer.Size) &&
		(iReadSequence == 8u) && (Packet.Sequence == 7u) &&
		(Packet.PacketSize == 12u) && (Packet.PaddingSize == 7u) &&
		testSshBytesEqual(
			Packet.Payload,
			(xbytesview){ arrPayload, sizeof(arrPayload) }
		) && testSshBytesEqual(
			Packet.Padding,
			(xbytesview){ arrExpected + 9u, 7u }
		), "ssh packet read mismatch");
}



/* 验证容量与 padding callback 失败均不提交 writer 或序列。 */
static void testSshPacketWriteAtomic(void)
{
	unsigned char arrWire[16];
	testsshpadding Padding = { 0x80u, 0u, false };
	xsshwriter Writer;
	uint32 iSequence = 3u;

	memset(arrWire, 0x5au, sizeof(arrWire));
	testRequire(xrtSshWriterInit(&Writer, arrWire, 8u) &&
		(xrtSshPacketWrite(
			&Writer,
			XRT_BYTES_LITERAL("data"),
			8u,
			&iSequence,
			testSshPacketPadding,
			&Padding
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u) &&
		(iSequence == 3u) && (Padding.Calls == 0u) &&
		(arrWire[0] == 0x5au), "ssh packet short write changed state");

	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)),
		"ssh packet callback setup failed");
	Padding.Fail = true;
	testRequire((xrtSshPacketWrite(
		&Writer,
		XRT_BYTES_LITERAL("data"),
		8u,
		&iSequence,
		testSshPacketPadding,
		&Padding
	) == XSSH_ERROR_CALLBACK) && (Writer.Size == 0u) &&
		(iSequence == 3u) && (Padding.Calls == 1u) &&
		(arrWire[0] == 0x5au), "ssh packet callback failure was partial");
	testRequire(xrtSshPacketWrite(
		&Writer,
		XRT_BYTES_LITERAL("data"),
		8u,
		&iSequence,
		NULL,
		NULL
	) == XSSH_ERROR_ARGUMENT, "ssh packet accepted missing padding source");
}



/* 验证截断和畸形 packet 不推进 reader、序列或输出。 */
static void testSshPacketReadAtomic(void)
{
	static const unsigned char arrShortLength[] = { 0u, 0u, 0u };
	static const unsigned char arrTooSmall[] = { 0u, 0u, 0u, 4u, 4u, 0u, 0u, 0u };
	static const unsigned char arrBadAlign[] = { 0u, 0u, 0u, 11u, 4u };
	static const unsigned char arrBadPadding[] = {
		0u, 0u, 0u, 12u, 3u, 1u, 2u, 3u,
		4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u
	};
	unsigned char arrWire[32];
	testsshpadding Padding = { 0x40u, 0u, false };
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;
	xsshpacketview Keep;
	uint32 iSequence = 10u;

	memset(&Keep, 0x5a, sizeof(Keep));
	Packet = Keep;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrShortLength, sizeof(arrShortLength) }
	) && (xrtSshPacketRead(
		&Reader,
		8u,
		0u,
		&iSequence,
		&Packet
	) == XSSH_NEED_MORE) && (Reader.Position == 0u) &&
		(iSequence == 10u) && (memcmp(&Packet, &Keep, sizeof(Packet)) == 0),
		"ssh packet partial length changed state");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrTooSmall, sizeof(arrTooSmall) }
	) && (xrtSshPacketRead(
		&Reader,
		8u,
		0u,
		&iSequence,
		&Packet
	) == XSSH_ERROR_PROTOCOL) && (Reader.Position == 0u) &&
		(iSequence == 10u), "ssh packet accepted too-small packet_length");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrBadAlign, sizeof(arrBadAlign) }
	) && (xrtSshPacketRead(
		&Reader,
		8u,
		0u,
		&iSequence,
		&Packet
	) == XSSH_ERROR_PROTOCOL) && (Reader.Position == 0u),
		"ssh packet accepted unaligned packet_length");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrBadPadding, sizeof(arrBadPadding) }
	) && (xrtSshPacketRead(
		&Reader,
		8u,
		0u,
		&iSequence,
		&Packet
	) == XSSH_ERROR_PROTOCOL) && (Reader.Position == 0u),
		"ssh packet accepted short padding");

	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketWrite(
			&Writer,
			XRT_BYTES_LITERAL("data"),
			8u,
			NULL,
			testSshPacketPadding,
			&Padding
		) == XSSH_OK), "ssh packet truncated setup failed");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size - 1u }
	) && (xrtSshPacketRead(
		&Reader,
		8u,
		0u,
		&iSequence,
		&Packet
	) == XSSH_NEED_MORE) && (Reader.Position == 0u) &&
		(iSequence == 10u), "ssh packet truncation changed state");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshPacketRead(
		&Reader,
		8u,
		8u,
		&iSequence,
		&Packet
	) == XSSH_ERROR_OVERFLOW) && (Reader.Position == 0u),
		"ssh packet maximum was ignored");
}



/* 验证粘包读取与 uint32 序列号自然回绕。 */
static void testSshPacketConcatAndWrap(void)
{
	unsigned char arrWire[64];
	testsshpadding Padding = { 0x10u, 0u, false };
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;
	uint32 iWriteSequence = UINT32_MAX;
	uint32 iReadSequence = UINT32_MAX;

	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketWrite(
			&Writer,
			XRT_BYTES_LITERAL("a"),
			8u,
			&iWriteSequence,
			testSshPacketPadding,
			&Padding
		) == XSSH_OK) && (iWriteSequence == 0u) &&
		(xrtSshPacketWrite(
			&Writer,
			XRT_BYTES_LITERAL("bc"),
			8u,
			&iWriteSequence,
			testSshPacketPadding,
			&Padding
		) == XSSH_OK) && (iWriteSequence == 1u),
		"ssh concatenated packet write failed");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshPacketRead(
		&Reader,
		8u,
		0u,
		&iReadSequence,
		&Packet
	) == XSSH_OK) && (Packet.Sequence == UINT32_MAX) &&
		testSshBytesEqual(Packet.Payload, XRT_BYTES_LITERAL("a")) &&
		(iReadSequence == 0u) && (xrtSshPacketRead(
		&Reader,
		8u,
		0u,
		&iReadSequence,
		&Packet
	) == XSSH_OK) && (Packet.Sequence == 0u) &&
		testSshBytesEqual(Packet.Payload, XRT_BYTES_LITERAL("bc")) &&
		(iReadSequence == 1u) && (Reader.Position == Writer.Size),
		"ssh concatenated packet read mismatch");
}



/* 运行 SSH packet framing 与失败原子性测试。 */
int main(void)
{
	testSshPacketMeasureCases();
	testSshPacketRoundtrip();
	testSshPacketWriteAtomic();
	testSshPacketReadAtomic();
	testSshPacketConcatAndWrap();
	return 0;
}
