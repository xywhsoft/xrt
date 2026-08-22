#include "../test.h"



/* 验证系统安全随机源能够直接驱动 plain packet。 */
static void testSshPacketSecureWrite(void)
{
	unsigned char arrWire[64];
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;
	uint32 iWriteSequence = 4u;
	uint32 iReadSequence = 4u;

	testRequire(xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketWriteSecure(
			&Writer,
			XRT_BYTES_LITERAL("secure"),
			8u,
			&iWriteSequence
		) == XSSH_OK) && (iWriteSequence == 5u),
		"ssh secure packet write failed");
	testRequire(xrtSshReaderInit(
		&Reader,
		(xbytesview){ arrWire, Writer.Size }
	) && (xrtSshPacketRead(
		&Reader,
		8u,
		0u,
		&iReadSequence,
		&Packet
	) == XSSH_OK) && (iReadSequence == 5u) &&
		testSshBytesEqual(Packet.Payload, XRT_BYTES_LITERAL("secure")) &&
		(Packet.PaddingSize >= XSSH_PACKET_PADDING_MIN),
		"ssh secure packet roundtrip failed");
}



/* 运行安全 padding 适配测试。 */
int main(void)
{
	testSshPacketSecureWrite();
	return 0;
}
