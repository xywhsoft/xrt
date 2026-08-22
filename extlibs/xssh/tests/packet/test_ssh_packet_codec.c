#include "../test.h"



/* 测试使用确定性 padding，生产便捷路径由系统安全随机源提供。 */
static bool testSshPacketCodecPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	uint8 iStart = *(const uint8*)pUserData;
	bytes pBytes = (bytes)pOutput;
	size_t i;

	for ( i = 0u; i < iSize; ++i ) {
		pBytes[i] = (uint8)(iStart + (uint8)i);
	}
	return true;
}



/* 验证 plain 路径、长度探测、序列号和输出重叠保护。 */
static void testSshPacketCodecPlain(void)
{
	unsigned char arrWire[128];
	uint8 iPadding = 0x40u;
	xsshpacketcodec Codec;
	xsshpacketneed Need;
	xsshpacketneed KeepNeed;
	xsshpacketview Packet;
	xsshwriter Writer;
	xsshwriter OverlapWriter;
	xsshreader Reader;

	testRequire((xrtSshPacketCodecInit(&Codec, 0u) == XSSH_OK) &&
		(Codec.ReadMode == XSSH_PACKET_MODE_PLAIN) &&
		(Codec.WriteMode == XSSH_PACKET_MODE_PLAIN) &&
		(xrtSshPacketCodecWriteMeasure(
			&Codec,
			sizeof("plain") - 1u,
			&Need
		) == XSSH_OK) && (Need.WireSize == 16u) &&
		(Need.PacketSize == 12u) && (Need.PlainSize == 0u) &&
		xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketCodecWriteWithPadding(
			&Codec,
			&Writer,
			XRT_BYTES_LITERAL("plain"),
			testSshPacketCodecPadding,
			&iPadding
		) == XSSH_OK) && (Codec.WriteSequence == 1u),
		"ssh plain packet codec write failed");
	memset(&KeepNeed, 0x5a, sizeof(KeepNeed));
	Need = KeepNeed;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, 3u }
	) && (xrtSshPacketCodecInspect(
		&Codec,
		&Reader,
		&Need
	) == XSSH_NEED_MORE) && (memcmp(
		&Need,
		&KeepNeed,
		sizeof(Need)
	) == 0), "ssh packet codec partial inspect changed output");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshPacketCodecInspect(
		&Codec,
		&Reader,
		&Need
	) == XSSH_OK) && (Need.WireSize == Writer.Size) &&
		(Need.PlainSize == 0u) && (Need.PacketSize == Writer.Size - 4u) &&
		(xrtSshPacketCodecRead(
			&Codec,
			&Reader,
			&Packet,
			NULL,
			0u
		) == XSSH_OK) && (Reader.Position == Writer.Size) &&
		(Codec.ReadSequence == 1u) && (Packet.Sequence == 0u) &&
		testSshBytesEqual(Packet.Payload, XRT_BYTES_LITERAL("plain")),
		"ssh plain packet codec read failed");
	testRequire(xrtSshWriterInit(
		&OverlapWriter,
		&Codec,
		sizeof(Codec)
	) && (xrtSshPacketCodecWriteWithPadding(
		&Codec,
		&OverlapWriter,
		XRT_BYTES_LITERAL("overlap"),
		testSshPacketCodecPadding,
		&iPadding
	) == XSSH_ERROR_ARGUMENT) && (OverlapWriter.Size == 0u) &&
		(Codec.WriteSequence == 1u),
		"ssh packet codec accepted state/output overlap");
	xrtSshPacketCodecClear(&Codec);
}



/* 验证双向 NEWKEYS 切换、工作区探测和失败原子性。 */
static void testSshPacketCodecAesGcm(void)
{
	unsigned char arrKey[16] = { 0u };
	unsigned char arrBadKey[15] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = {
		1u, 2u, 3u, 4u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 5u
	};
	unsigned char arrWire[128];
	unsigned char arrPlain[64];
	unsigned char arrKeep[64];
	uint8 iPadding = 0x80u;
	xsshpacketcodec Codec;
	xsshpacketneed Need;
	xsshpacketview Packet;
	xsshwriter Writer;
	xsshreader Reader;
	uint64 iInvocation;
	size_t iWireSize;

	testRequire((xrtSshPacketCodecInit(&Codec, 64u) == XSSH_OK) &&
		(xrtSshPacketCodecSetWriteAesGcm(
			&Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) == XSSH_OK) && (Codec.ReadMode == XSSH_PACKET_MODE_PLAIN) &&
		(Codec.WriteMode == XSSH_PACKET_MODE_AES_GCM) &&
		(xrtSshPacketCodecWriteMeasure(
			&Codec,
			sizeof("newkeys") - 1u,
			&Need
		) == XSSH_OK) && (Need.WireSize == 36u) &&
		(Need.PacketSize == 16u) && (Need.PlainSize == 0u) &&
		xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketCodecWriteWithPadding(
			&Codec,
			&Writer,
			XRT_BYTES_LITERAL("newkeys"),
			testSshPacketCodecPadding,
			&iPadding
		) == XSSH_OK), "ssh packet codec write NEWKEYS switch failed");
	iWireSize = Writer.Size;
	testRequire((xrtSshPacketCodecSetReadAesGcm(
		&Codec,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) == XSSH_OK) && xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshPacketCodecInspect(
		&Codec,
		&Reader,
		&Need
	) == XSSH_OK) && (Need.WireSize == iWireSize) &&
		(Need.PlainSize == Need.PacketSize) &&
		(Need.PlainSize <= sizeof(arrPlain)),
		"ssh packet codec encrypted inspect failed");
	memset(arrPlain, 0xcc, sizeof(arrPlain));
	memcpy(arrKeep, arrPlain, sizeof(arrKeep));
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize - 1u }
	) && (xrtSshPacketCodecRead(
		&Codec,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain)
	) == XSSH_NEED_MORE) && (Reader.Position == 0u) &&
		(Codec.ReadSequence == 0u) && (memcmp(
		arrPlain,
		arrKeep,
		sizeof(arrPlain)
	) == 0), "ssh packet codec truncation changed state");
	arrWire[iWireSize - 1u] ^= 1u;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshPacketCodecRead(
		&Codec,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain)
	) == XSSH_ERROR_AUTHENTICATION) && (Reader.Position == 0u) &&
		(Codec.ReadSequence == 0u) && (memcmp(
		arrPlain,
		arrKeep,
		sizeof(arrPlain)
	) == 0), "ssh packet codec bad tag changed state");
	arrWire[iWireSize - 1u] ^= 1u;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshPacketCodecRead(
		&Codec,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain)
	) == XSSH_OK) && (Codec.ReadSequence == 1u) &&
		(Codec.WriteSequence == 1u) && (Packet.Sequence == 0u) &&
		testSshBytesEqual(Packet.Payload, XRT_BYTES_LITERAL("newkeys")),
		"ssh packet codec encrypted roundtrip failed");
	testRequire((xrtSshAesGcmInvocation(
		&Codec.WriteAesGcm,
		&iInvocation
	) == XSSH_OK) && (iInvocation == 6u) &&
		(xrtSshPacketCodecSetWriteAesGcm(
			&Codec,
			(xbytesview){ arrBadKey, sizeof(arrBadKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) == XSSH_ERROR_ARGUMENT) && (xrtSshAesGcmInvocation(
			&Codec.WriteAesGcm,
			&iInvocation
		) == XSSH_OK) && (iInvocation == 6u) &&
		(xrtSshPacketCodecResetReadSequence(&Codec) == XSSH_OK) &&
		(xrtSshPacketCodecResetWriteSequence(&Codec) == XSSH_OK) &&
		(Codec.ReadSequence == 0u) && (Codec.WriteSequence == 0u),
		"ssh packet codec switch or strict reset was not atomic");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, iWireSize }
	) && (xrtSshPacketCodecRead(
		&Codec,
		&Reader,
		&Packet,
		&Codec,
		sizeof(Codec)
	) == XSSH_ERROR_ARGUMENT),
		"ssh packet codec accepted plaintext/state overlap");
	xrtSshPacketCodecClear(&Codec);
}



/* 验证背压写事务只在可靠入队后消费 sequence 与 GCM nonce。 */
static void testSshPacketCodecWriteTransaction(void)
{
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = {
		1u, 2u, 3u, 4u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 5u
	};
	unsigned char arrFirst[128];
	unsigned char arrSecond[128];
	uint8 iPadding = 0x60u;
	xsshpacketcodec Codec;
	xsshwriter First;
	xsshwriter Second;
	uint64 iInvocation = 0u;

	testRequire((xrtSshPacketCodecInit(&Codec, 0u) == XSSH_OK) &&
		(xrtSshPacketCodecSetWriteAesGcm(
			&Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) == XSSH_OK) && xrtSshWriterInit(
			&First,
			arrFirst,
			sizeof(arrFirst)
		) && (xrtSshPacketCodecWritePrepareWithPadding(
			&Codec,
			&First,
			XRT_BYTES_LITERAL("queued"),
			testSshPacketCodecPadding,
			&iPadding
		) == XSSH_OK) && Codec.WritePending &&
		(Codec.WriteSequence == 0u) && (xrtSshAesGcmInvocation(
			&Codec.WriteAesGcm,
			&iInvocation
		) == XSSH_OK) && (iInvocation == 5u),
		"ssh packet codec prepare consumed write state");
	testRequire(xrtSshWriterInit(
		&Second,
		arrSecond,
		sizeof(arrSecond)
	) && (xrtSshPacketCodecWritePrepareWithPadding(
		&Codec,
		&Second,
		XRT_BYTES_LITERAL("duplicate"),
		testSshPacketCodecPadding,
		&iPadding
	) == XSSH_ERROR_STATE) && (Second.Size == 0u) &&
		(xrtSshPacketCodecResetWriteSequence(&Codec) == XSSH_ERROR_STATE) &&
		(xrtSshPacketCodecSetWriteAesGcm(
			&Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) == XSSH_ERROR_STATE),
		"ssh packet codec allowed mutation during pending write");
	testRequire((xrtSshPacketCodecWriteAbort(&Codec) == XSSH_OK) &&
		!Codec.WritePending && (Codec.WriteSequence == 0u) &&
		xrtSshWriterInit(&Second, arrSecond, sizeof(arrSecond)) &&
		(xrtSshPacketCodecWritePrepareWithPadding(
			&Codec,
			&Second,
			XRT_BYTES_LITERAL("queued"),
			testSshPacketCodecPadding,
			&iPadding
		) == XSSH_OK) && (Second.Size == First.Size) &&
		(memcmp(arrFirst, arrSecond, First.Size) == 0),
		"ssh packet codec aborted write could not be retried");
	testRequire((xrtSshPacketCodecWriteCommit(&Codec) == XSSH_OK) &&
		!Codec.WritePending && (Codec.WriteSequence == 1u) &&
		(xrtSshAesGcmInvocation(
			&Codec.WriteAesGcm,
			&iInvocation
		) == XSSH_OK) && (iInvocation == 6u) &&
		(xrtSshPacketCodecWriteCommit(&Codec) == XSSH_ERROR_STATE) &&
		(xrtSshPacketCodecWriteAbort(&Codec) == XSSH_ERROR_STATE),
		"ssh packet codec write commit boundary failed");
	xrtSshPacketCodecClear(&Codec);
}



/* 验证 codec 上限同时约束收发方向。 */
static void testSshPacketCodecLimits(void)
{
	unsigned char arrWire[64] = { 0u };
	uint8 iPadding = 0u;
	xsshpacketcodec Codec;
	xsshpacketneed Need;
	xsshwriter Writer;
	xsshreader Reader;

	testRequire((xrtSshPacketCodecInit(&Codec, 16u) == XSSH_OK) &&
		xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketCodecWriteWithPadding(
			&Codec,
			NULL,
			XRT_BYTES_LITERAL("payload larger than limit"),
			testSshPacketCodecPadding,
			&iPadding
		) == XSSH_ERROR_ARGUMENT),
		"ssh packet codec invalid writer was accepted");
	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketCodecWriteMeasure(
			&Codec,
			sizeof("payload larger than limit") - 1u,
			&Need
		) == XSSH_ERROR_OVERFLOW) &&
		(xrtSshPacketCodecWriteWithPadding(
			&Codec,
			&Writer,
			XRT_BYTES_LITERAL("payload larger than limit"),
			testSshPacketCodecPadding,
			&iPadding
		) == XSSH_ERROR_OVERFLOW) && (Writer.Size == 0u),
		"ssh packet codec write limit was ignored");
	arrWire[3] = 5u;
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, sizeof(arrWire) }
	) && (xrtSshPacketCodecInspect(
		&Codec,
		&Reader,
		&Need
	) == XSSH_ERROR_PROTOCOL),
		"ssh packet codec accepted misaligned plain length");
	xrtSshPacketCodecClear(&Codec);
	testRequire(xrtSshPacketCodecWriteMeasure(
		&Codec,
		1u,
		&Need
	) == XSSH_ERROR_STATE,
		"ssh cleared packet codec write measure remained usable");
	testRequire(xrtSshPacketCodecInspect(
		&Codec,
		&Reader,
		&Need
	) == XSSH_ERROR_STATE, "ssh cleared packet codec remained usable");
}



/* 运行统一 packet codec 的协议、状态和边界测试。 */
int main(void)
{
	testSshPacketCodecPlain();
	testSshPacketCodecAesGcm();
	testSshPacketCodecWriteTransaction();
	testSshPacketCodecLimits();
	return 0;
}
