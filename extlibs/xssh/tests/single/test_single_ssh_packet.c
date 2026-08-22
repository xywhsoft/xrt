#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_PACKET
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 为单头 packet 测试生成固定 padding。 */
static bool testSingleSshPadding(void* pOutput, size_t iSize, ptr pUserData)
{
	(void)pUserData;
	memset(pOutput, 0xa5, iSize);
	return true;
}



/* SSH packet 单头只闭合 wire 与 XRT core。 */
int main(void)
{
	unsigned char arrWire[32];
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;

	#if !defined(XSSH_FEATURE_PACKET) || !defined(XSSH_FEATURE_WIRE)
		#error "XSSH_MODULE_SSH_PACKET dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_CRYPTO) || \
		defined(XRT_FEATURE_NETWORK)
		#error "ssh packet unexpectedly enabled random, crypto or network"
	#endif

	return xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketWrite(
			&Writer,
			XRT_BYTES_LITERAL("packet"),
			8u,
			NULL,
			testSingleSshPadding,
			NULL
		) == XSSH_OK) && xrtSshReaderInit(
			&Reader,
			(xbytesview){ arrWire, Writer.Size }
		) && (xrtSshPacketRead(
			&Reader,
			8u,
			0u,
			NULL,
			&Packet
		) == XSSH_OK) && (Packet.Payload.Size == 6u) ? 0 : 1;
}
