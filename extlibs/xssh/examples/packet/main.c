#include <inttypes.h>
#include <stdio.h>
#include <xssh.h>



/* 示例 padding 源仅用于展示；真实 transport 应接入安全随机或会话 PRNG。 */
static bool exampleSshPadding(void* pOutput, size_t iSize, ptr pUserData)
{
	uint8* pSeed = (uint8*)pUserData;
	bytes pBytes = (bytes)pOutput;
	size_t i;

	for ( i = 0u; i < iSize; ++i ) {
		pBytes[i] = (*pSeed)++;
	}
	return true;
}



/* 构建并读取一个最小 SSH_MSG_IGNORE 风格 payload。 */
int main(void)
{
	unsigned char arrWire[32];
	uint8 iSeed = 1u;
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;

	if ( !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
		(xrtSshPacketWrite(
			&Writer,
			XRT_BYTES_LITERAL("\2ignore"),
			8u,
			NULL,
			exampleSshPadding,
			&iSeed
		) != XSSH_OK) || !xrtSshReaderInit(
			&Reader,
			(xbytesview){ arrWire, Writer.Size }
		) || (xrtSshPacketRead(
			&Reader,
			8u,
			0u,
			NULL,
			&Packet
		) != XSSH_OK) ) {
		return 1;
	}
	printf(
		"packet=%" PRIu32 " payload=%zu padding=%u\n",
		Packet.PacketSize,
		Packet.Payload.Size,
		(unsigned int)Packet.PaddingSize
	);
	return 0;
}
