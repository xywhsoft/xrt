#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CHANNEL_REQUEST
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Channel request 单头闭包必须包含公共消息层。 */
int main(void)
{
	unsigned char arrPayload[32];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_CHANNEL_REQUEST) || \
		!defined(XSSH_FEATURE_CHANNEL_MESSAGE) || !defined(XSSH_FEATURE_WIRE) || \
		!defined(XRT_FEATURE_UNICODE)
		#error "SSH channel request dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_CRYPTO) || \
		defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH channel request unexpectedly enabled network or crypto"
	#endif

	return xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelShellWrite(&Writer, 1u, true) == XSSH_OK) ? 0 : 1;
}
