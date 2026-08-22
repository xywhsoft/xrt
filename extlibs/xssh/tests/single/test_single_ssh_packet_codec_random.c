#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_PACKET_CODEC_RANDOM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 默认 codec writer 显式携带 CSPRNG，但仍不启用网络。 */
int main(void)
{
	unsigned char arrWire[64];
	xsshpacketcodec Codec;
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_PACKET_CODEC_RANDOM) || \
		!defined(XSSH_FEATURE_PACKET_CODEC) || \
		!defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH random packet codec dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK)
		#error "SSH random packet codec unexpectedly enabled network"
	#endif

	if ( (xrtSshPacketCodecInit(&Codec, 0u) != XSSH_OK) ||
		!xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
		(xrtSshPacketCodecWrite(
			&Codec,
			&Writer,
			XRT_BYTES_LITERAL("random")
		) != XSSH_OK) ) {
		xrtSshPacketCodecClear(&Codec);
		return 1;
	}
	xrtSshPacketCodecClear(&Codec);
	return 0;
}
