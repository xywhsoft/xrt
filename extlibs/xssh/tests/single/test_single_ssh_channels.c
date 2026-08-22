#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CHANNELS
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 动态 channel 集合只增加整数映射和网络缓冲，不携带 Stream、任务或协程。 */
int main(void)
{
	xsshchannels Channels;
	xsshchannel* pChannel = NULL;

	#if !defined(XSSH_FEATURE_CHANNELS) || \
		!defined(XSSH_FEATURE_CHANNEL_IO) || \
		!defined(XSSH_FEATURE_CONNECTION_SESSION) || \
		!defined(XRT_FEATURE_INT_MAP)
		#error "SSH channels dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_SESSION_STREAM) || \
		defined(XRT_FEATURE_NET_TCP) || defined(XRT_FEATURE_TASK) || \
		defined(XRT_FEATURE_COROUTINE)
		#error "SSH channels unexpectedly enabled stream or runtime"
	#endif

	if ( !xrtSshChannelsInit(&Channels, NULL, NULL) ||
		(xrtSshChannelsOpen(&Channels, &pChannel) != XSSH_OK) ||
		(pChannel == NULL) ) {
		return 1;
	}
	xrtSshChannelsClear(&Channels);
	return 0;
}
