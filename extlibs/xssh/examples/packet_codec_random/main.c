#include <stdio.h>
#include <xssh.h>



/* 展示使用系统安全随机 padding 的默认 packet codec 写入。 */
int main(void)
{
	unsigned char arrWire[128];
	xsshpacketcodec Codec;
	xsshwriter Writer;

	if ( (xrtSshPacketCodecInit(&Codec, 0u) != XSSH_OK) ||
		!xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
		(xrtSshPacketCodecWrite(
			&Codec,
			&Writer,
			XRT_BYTES_LITERAL("production-padding")
		) != XSSH_OK) ) {
		xrtSshPacketCodecClear(&Codec);
		return 1;
	}
	printf("wire=%zu sequence=%u\n",
		Writer.Size,
		(unsigned int)Codec.WriteSequence);
	xrtSshPacketCodecClear(&Codec);
	return 0;
}
