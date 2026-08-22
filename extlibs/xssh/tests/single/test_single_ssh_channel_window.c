#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CHANNEL_WINDOW
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Channel window 单头只依赖纯数值 wire 契约。 */
int main(void)
{
	xsshchannelwindow Window;

	#if !defined(XSSH_FEATURE_CHANNEL_WINDOW) || !defined(XSSH_FEATURE_WIRE)
		#error "SSH channel window dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_UNICODE) || \
		defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH channel window unexpectedly enabled heavy dependencies"
	#endif

	return xrtSshChannelWindowInit(
		&Window,
		32u,
		16u,
		32u,
		16u,
		16u
	) && (xrtSshChannelSendLimit(&Window) == 16u) ? 0 : 1;
}
