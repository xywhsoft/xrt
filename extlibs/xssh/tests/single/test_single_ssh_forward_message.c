#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_FORWARD_MESSAGE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Forward message 单头闭包包含 global 与 channel envelope。 */
int main(void)
{
	unsigned char arrPayload[64];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_FORWARD_MESSAGE) || \
		!defined(XSSH_FEATURE_CONNECTION_MESSAGE) || \
		!defined(XSSH_FEATURE_CHANNEL_MESSAGE)
		#error "SSH forwarding message dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_CRYPTO) || \
		defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH forwarding message unexpectedly enabled network or crypto"
	#endif

	return xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshTcpipForwardWrite(
			&Writer,
			XRT_BYTES_LITERAL("127.0.0.1"),
			0u
		) == XSSH_OK) ? 0 : 1;
}
