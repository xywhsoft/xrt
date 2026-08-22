#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CHANNEL_STATE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Channel state 单头只依赖 wire 结果码。 */
int main(void)
{
	xsshchannelstate State;

	#if !defined(XSSH_FEATURE_CHANNEL_STATE) || !defined(XSSH_FEATURE_WIRE)
		#error "SSH channel state dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_UNICODE) || \
		defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH channel state unexpectedly enabled heavy dependencies"
	#endif

	return xrtSshChannelStateInit(&State) &&
		xrtSshChannelCanSendData(&State) ? 0 : 1;
}
