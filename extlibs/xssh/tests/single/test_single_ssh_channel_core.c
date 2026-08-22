#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CHANNEL_CORE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Channel core 单头不隐式引入 reply queue、网络或运行时。 */
int main(void)
{
	xsshchannelcore Channel;

	#if !defined(XSSH_FEATURE_CHANNEL_CORE) || \
		!defined(XSSH_FEATURE_CHANNEL_MESSAGE) || \
		!defined(XSSH_FEATURE_CHANNEL_STATE) || \
		!defined(XSSH_FEATURE_CHANNEL_WINDOW)
		#error "SSH channel core dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_REPLY_QUEUE) || defined(XRT_FEATURE_NETWORK) || \
		defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH channel core unexpectedly enabled queue, network or runtime"
	#endif

	return xrtSshChannelCoreOpenInit(
		&Channel,
		0u,
		1024u,
		512u,
		512u
	) ? 0 : 1;
}
