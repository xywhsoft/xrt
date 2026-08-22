#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CONNECTION_MESSAGE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Connection message 单头只依赖 wire。 */
int main(void)
{
	unsigned char arrPayload[32];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_CONNECTION_MESSAGE) || !defined(XSSH_FEATURE_WIRE)
		#error "SSH connection message dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_CRYPTO) || \
		defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH connection message unexpectedly enabled network or crypto"
	#endif

	return xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshGlobalFailureWrite(&Writer) == XSSH_OK) ? 0 : 1;
}
