#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_PACKET_RANDOM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 安全 padding 单头只闭包到 random_secure，不携带密码和网络。 */
int main(void)
{
	unsigned char arrWire[32];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_PACKET_RANDOM) || \
		!defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH packet random dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_NETWORK)
		#error "SSH packet random unexpectedly enabled crypto or network"
	#endif

	return xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) &&
		(xrtSshPacketWriteSecure(
			&Writer,
			XRT_BYTES_LITERAL("random"),
			8u,
			NULL
		) == XSSH_OK) ? 0 : 1;
}
