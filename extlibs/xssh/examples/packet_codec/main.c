#include <stdio.h>
#include <string.h>
#include <xssh.h>



/* 示例固定 padding 只用于展示可重复的 codec 状态迁移。 */
static bool examplePacketCodecPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	(void)pUserData;
	memset(pOutput, 0xa5, iSize);
	return true;
}



/* 展示 NEWKEYS 后双向 AES-GCM 切换与调用方解密工作区。 */
int main(void)
{
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	unsigned char arrWire[128];
	unsigned char arrPlain[64];
	xsshpacketcodec Codec;
	xsshpacketneed Need;
	xsshpacketview Packet;
	xsshwriter Writer;
	xsshreader Reader;

	if ( (xrtSshPacketCodecInit(&Codec, 0u) != XSSH_OK) ||
		(xrtSshPacketCodecSetWriteAesGcm(
			&Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) != XSSH_OK) || (xrtSshPacketCodecSetReadAesGcm(
			&Codec,
			(xbytesview){ arrKey, sizeof(arrKey) },
			(xbytesview){ arrIV, sizeof(arrIV) }
		) != XSSH_OK) || !xrtSshWriterInit(
			&Writer,
			arrWire,
			sizeof(arrWire)
		) || (xrtSshPacketCodecWriteWithPadding(
			&Codec,
			&Writer,
			XRT_BYTES_LITERAL("transport"),
			examplePacketCodecPadding,
			NULL
		) != XSSH_OK) || !xrtSshReaderInit(
			&Reader,
			(xbytesview){ arrWire, Writer.Size }
		) || (xrtSshPacketCodecInspect(
			&Codec,
			&Reader,
			&Need
		) != XSSH_OK) || (Need.PlainSize > sizeof(arrPlain)) ||
		(xrtSshPacketCodecRead(
			&Codec,
			&Reader,
			&Packet,
			arrPlain,
			sizeof(arrPlain)
		) != XSSH_OK) ) {
		xrtSshPacketCodecClear(&Codec);
		return 1;
	}
	printf("wire=%zu plain-workspace=%zu payload=%zu sequence=%u\n",
		Need.WireSize,
		Need.PlainSize,
		Packet.Payload.Size,
		(unsigned int)Packet.Sequence);
	xrtSshPacketCodecClear(&Codec);
	return 0;
}
