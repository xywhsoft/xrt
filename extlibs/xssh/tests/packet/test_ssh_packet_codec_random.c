#include "../test.h"



/* 验证默认 CSPRNG 便利层同时覆盖 plain 与 AES-GCM 当前方向。 */
int main(void)
{
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	unsigned char arrWire[128];
	unsigned char arrPlain[64];
	xsshpacketcodec Codec;
	xsshpacketview Packet;
	xsshwriter Writer;
	xsshreader Reader;

	testRequire((xrtSshPacketCodecInit(&Codec, 0u) == XSSH_OK) &&
		xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketCodecWrite(
			&Codec,
			&Writer,
			XRT_BYTES_LITERAL("plain-random")
		) == XSSH_OK) && xrtSshReaderInit(
			&Reader,
			(xbytesview){ arrWire, Writer.Size }
		) && (xrtSshPacketCodecRead(
			&Codec,
			&Reader,
			&Packet,
			NULL,
			0u
		) == XSSH_OK) && testSshBytesEqual(
			Packet.Payload,
			XRT_BYTES_LITERAL("plain-random")
		), "ssh random plain codec roundtrip failed");
	testRequire(xrtSshWriterInit(
		&Writer,
		arrWire,
		sizeof(arrWire)
	) && (xrtSshPacketCodecWritePrepare(
		&Codec,
		&Writer,
		XRT_BYTES_LITERAL("random-pending")
	) == XSSH_OK) && Codec.WritePending &&
		(xrtSshPacketCodecWriteAbort(&Codec) == XSSH_OK) &&
		!Codec.WritePending,
		"ssh random codec transaction failed");
	testRequire((xrtSshPacketCodecSetWriteAesGcm(
		&Codec,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) == XSSH_OK) && (xrtSshPacketCodecSetReadAesGcm(
		&Codec,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) == XSSH_OK) && xrtSshWriterInit(
		&Writer,
		arrWire,
		sizeof(arrWire)
	) && (xrtSshPacketCodecWrite(
		&Codec,
		&Writer,
		XRT_BYTES_LITERAL("aead-random")
	) == XSSH_OK) && xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshPacketCodecRead(
		&Codec,
		&Reader,
		&Packet,
		arrPlain,
		sizeof(arrPlain)
	) == XSSH_OK) && testSshBytesEqual(
		Packet.Payload,
		XRT_BYTES_LITERAL("aead-random")
	), "ssh random AES-GCM codec roundtrip failed");
	xrtSshPacketCodecClear(&Codec);
	return 0;
}
