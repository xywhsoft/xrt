#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CHANNEL_IO
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Channel I/O 单头只引入动态网络缓冲，不携带 TCP、任务或连接表。 */
int main(void)
{
	xsshchannelioconfig Config;
	xsshchannelcore Channel;
	xsshchannelio Io;

	#if !defined(XSSH_FEATURE_CHANNEL_IO) || \
		!defined(XSSH_FEATURE_CHANNEL_CORE) || \
		!defined(XRT_FEATURE_NET_BUFFER)
		#error "SSH channel I/O dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_CONNECTION_SESSION) || \
		defined(XSSH_FEATURE_TRANSPORT_TCP) || \
		defined(XRT_FEATURE_NET_TCP) || defined(XRT_FEATURE_TASK) || \
		defined(XRT_FEATURE_COROUTINE)
		#error "SSH channel I/O unexpectedly enabled session, TCP or runtime"
	#endif

	xrtSshChannelIoConfigInit(&Config);
	Config.ReceiveLimit = 16u;
	Config.SendLimit = 16u;
	return xrtSshChannelCoreOpenInit(
		&Channel,
		1u,
		16u,
		8u,
		4u
	) && xrtSshChannelIoInit(
		&Io,
		NULL,
		&Channel,
		&Config
	) ? 0 : 1;
}
