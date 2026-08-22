#include "../test.h"



typedef struct testaesgcmpadding {
	uint8 Start;
	bool Fail;
} testaesgcmpadding;



/* 为测试向量提供确定性 padding。 */
static bool testSshAesGcmPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	testaesgcmpadding* pPadding = (testaesgcmpadding*)pUserData;
	bytes pBytes = (bytes)pOutput;
	size_t i;

	for ( i = 0u; i < iSize; ++i ) {
		pBytes[i] = (uint8)(pPadding->Start + (uint8)i);
	}
	return !pPadding->Fail;
}



/* 验证 RFC 5647 明文体对齐边界。 */
static void testSshAesGcmMeasureCases(void)
{
	uint8 iPadding = 0u;
	uint32 iPacket = 0u;

	testRequire((xrtSshAesGcmMeasure(0u, &iPadding, &iPacket) == XSSH_OK) &&
		(iPadding == 15u) && (iPacket == 16u),
		"ssh aes-gcm empty measure mismatch");
	testRequire((xrtSshAesGcmMeasure(11u, &iPadding, &iPacket) == XSSH_OK) &&
		(iPadding == 4u) && (iPacket == 16u),
		"ssh aes-gcm aligned measure mismatch");
	testRequire((xrtSshAesGcmMeasure(12u, &iPadding, &iPacket) == XSSH_OK) &&
		(iPadding == 19u) && (iPacket == 32u),
		"ssh aes-gcm next-block measure mismatch");
}



/* 验证历史 OpenSSH AES-128-GCM 向量和双向状态推进。 */
static void testSshAesGcmVector(void)
{
	static const unsigned char arrExpected[] = {
		0x00u, 0x00u, 0x00u, 0x10u, 0x54u, 0x52u, 0x9bu, 0xf4u,
		0xdau, 0x70u, 0xceu, 0xf7u, 0xddu, 0x64u, 0x49u, 0xa8u,
		0xfcu, 0x63u, 0x1au, 0x0fu, 0x54u, 0xe8u, 0xdau, 0x32u,
		0xe3u, 0xe3u, 0xb3u, 0x29u, 0x6du, 0x34u, 0x5cu, 0x28u,
		0x31u, 0xa8u, 0x52u, 0xbau
	};
	static const unsigned char arrPayload[] = { 20u, 'a', 'e', 'a', 'd' };
	unsigned char arrKey[16];
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = {
		0x10u, 0x11u, 0x12u, 0x13u,
		0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u
	};
	unsigned char arrWire[128];
	unsigned char arrPlain[64];
	testaesgcmpadding Padding = { 0xe0u, false };
	xsshaesgcm WriteState;
	xsshaesgcm ReadState;
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;
	uint32 iWriteSequence = 9u;
	uint32 iReadSequence = 9u;
	uint64 iInvocation = 0u;
	size_t i;

	for ( i = 0u; i < sizeof(arrKey); ++i ) {
		arrKey[i] = (uint8)i;
	}
	testRequire((xrtSshAesGcmInit(
		&WriteState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) == XSSH_OK) && (xrtSshAesGcmInit(
		&ReadState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) == XSSH_OK), "ssh aes-gcm state init failed");
	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshAesGcmWrite(
			&Writer,
			(xbytesview){ arrPayload, sizeof(arrPayload) },
			&WriteState,
			&iWriteSequence,
			testSshAesGcmPadding,
			&Padding
		) == XSSH_OK) && (Writer.Size == sizeof(arrExpected)) &&
		(iWriteSequence == 10u) &&
		(memcmp(arrWire, arrExpected, sizeof(arrExpected)) == 0) &&
		(xrtSshAesGcmInvocation(&WriteState, &iInvocation) == XSSH_OK) &&
		(iInvocation == 2u), "ssh aes-gcm vector write mismatch");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshAesGcmRead(
		&Reader,
		&ReadState,
		0u,
		&iReadSequence,
		&Packet,
		arrPlain,
		sizeof(arrPlain)
	) == XSSH_OK) && (Reader.Position == Writer.Size) &&
		(iReadSequence == 10u) && (Packet.Sequence == 9u) &&
		(Packet.PacketSize == 16u) && (Packet.PaddingSize == 10u) &&
		testSshBytesEqual(
			Packet.Payload,
			(xbytesview){ arrPayload, sizeof(arrPayload) }
		) && (Packet.Padding.Data == arrPlain + 6u) &&
		(Packet.Padding.Size == 10u), "ssh aes-gcm vector read mismatch");
	xrtSshAesGcmClear(&WriteState);
	xrtSshAesGcmClear(&ReadState);
}



/* 验证 AES-256-GCM 使用同一 packet 契约。 */
static void testSshAesGcm256Roundtrip(void)
{
	unsigned char arrKey[32];
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = {
		0x20u, 0x21u, 0x22u, 0x23u,
		0u, 0u, 0u, 0u, 0u, 0u, 0u, 5u
	};
	unsigned char arrWire[128];
	unsigned char arrPlain[64];
	testaesgcmpadding Padding = { 0x70u, false };
	xsshaesgcm WriteState;
	xsshaesgcm ReadState;
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;
	size_t i;

	for ( i = 0u; i < sizeof(arrKey); ++i ) {
		arrKey[i] = (uint8)(0x80u + i);
	}
	testRequire((xrtSshAesGcmInit(
		&WriteState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) == XSSH_OK) && (xrtSshAesGcmInit(
		&ReadState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) == XSSH_OK) && xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshAesGcmWrite(
			&Writer,
			XRT_BYTES_LITERAL("aes256"),
			&WriteState,
			NULL,
			testSshAesGcmPadding,
			&Padding
		) == XSSH_OK) && xrtSshReaderInit(
			&Reader,
			(xbytesview){ arrWire, Writer.Size }
		) && (xrtSshAesGcmRead(
			&Reader,
			&ReadState,
			0u,
			NULL,
			&Packet,
			arrPlain,
			sizeof(arrPlain)
		) == XSSH_OK) &&
		testSshBytesEqual(Packet.Payload, XRT_BYTES_LITERAL("aes256")),
		"ssh aes-256-gcm roundtrip failed");
	xrtSshAesGcmClear(&WriteState);
	xrtSshAesGcmClear(&ReadState);
}



/* 验证认证失败、截断、容量不足和 callback 失败都不推进状态。 */
static void testSshAesGcmFailureAtomic(void)
{
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	unsigned char arrWire[128];
	unsigned char arrPlain[64];
	unsigned char arrKeep[64];
	testaesgcmpadding Padding = { 0x30u, false };
	xsshaesgcm WriteState;
	xsshaesgcm ReadState;
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;
	xsshpacketview KeepPacket;
	uint32 iSequence = 7u;
	uint64 iInvocation = 0u;
	size_t iWireSize;

	testRequire((xrtSshAesGcmInit(
		&WriteState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) == XSSH_OK) && (xrtSshAesGcmInit(
		&ReadState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) == XSSH_OK) && xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshAesGcmWrite(
			&Writer,
			XRT_BYTES_LITERAL("bad-tag"),
			&WriteState,
			NULL,
			testSshAesGcmPadding,
			&Padding
		) == XSSH_OK), "ssh aes-gcm failure setup failed");
	iWireSize = Writer.Size;
	arrWire[iWireSize - 1u] ^= 1u;
	memset(arrPlain, 0xcc, sizeof(arrPlain));
	memcpy(arrKeep, arrPlain, sizeof(arrPlain));
	memset(&KeepPacket, 0x5a, sizeof(KeepPacket));
	Packet = KeepPacket;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshAesGcmRead(
		&Reader,
		&ReadState,
		0u,
		&iSequence,
		&Packet,
		arrPlain,
		sizeof(arrPlain)
	) == XSSH_ERROR_AUTHENTICATION) && (Reader.Position == 0u) &&
		(iSequence == 7u) && (memcmp(&Packet, &KeepPacket, sizeof(Packet)) == 0) &&
		(memcmp(arrPlain, arrKeep, sizeof(arrPlain)) == 0) &&
		(xrtSshAesGcmInvocation(&ReadState, &iInvocation) == XSSH_OK) &&
		(iInvocation == 0u), "ssh aes-gcm bad tag changed state");
	arrWire[iWireSize - 1u] ^= 1u;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize - 1u }
	) && (xrtSshAesGcmRead(
		&Reader,
		&ReadState,
		0u,
		&iSequence,
		&Packet,
		arrPlain,
		sizeof(arrPlain)
	) == XSSH_NEED_MORE) && (Reader.Position == 0u) &&
		(iSequence == 7u), "ssh aes-gcm truncation changed state");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshAesGcmRead(
		&Reader,
		&ReadState,
		0u,
		&iSequence,
		&Packet,
		arrPlain,
		1u
	) == XSSH_ERROR_SPACE) && (Reader.Position == 0u) &&
		(iSequence == 7u), "ssh aes-gcm short plain buffer changed state");

	Padding.Fail = false;
	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshAesGcmWrite(
			&Writer,
			(xbytesview){ arrWire, 4u },
			&WriteState,
			&iSequence,
			testSshAesGcmPadding,
			&Padding
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u) &&
		(iSequence == 7u) &&
		(xrtSshAesGcmInvocation(&WriteState, &iInvocation) == XSSH_OK) &&
		(iInvocation == 1u), "ssh aes-gcm accepted overlapping payload");

	Padding.Fail = true;
	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)),
		"ssh aes-gcm callback writer setup failed");
	testRequire((xrtSshAesGcmWrite(
		&Writer,
		XRT_BYTES_LITERAL("callback"),
		&WriteState,
		&iSequence,
		testSshAesGcmPadding,
		&Padding
	) == XSSH_ERROR_CALLBACK) && (Writer.Size == 0u) &&
		(iSequence == 7u), "ssh aes-gcm callback failure changed state");
	WriteState.Invocation = UINT64_MAX;
	testRequire(xrtSshAesGcmWrite(
		&Writer,
		XRT_BYTES_LITERAL("exhausted"),
		&WriteState,
		&iSequence,
		testSshAesGcmPadding,
		&Padding
	) == XSSH_ERROR_STATE, "ssh aes-gcm reused exhausted nonce");
	xrtSshAesGcmClear(&WriteState);
	xrtSshAesGcmClear(&ReadState);
}



/* 运行 SSH AES-GCM packet 契约测试。 */
int main(void)
{
	testSshAesGcmMeasureCases();
	testSshAesGcmVector();
	testSshAesGcm256Roundtrip();
	testSshAesGcmFailureAtomic();
	return 0;
}
