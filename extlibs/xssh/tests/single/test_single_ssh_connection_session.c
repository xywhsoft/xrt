#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CONNECTION_SESSION
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Connection session 单头只组合协议状态，不隐式引入网络或认证策略。 */
int main(void)
{
	xsshconnectionsession Session;
	xsshreplyqueue Queue;
	uint64 arrTokens[2];

	#if !defined(XSSH_FEATURE_CONNECTION_SESSION) || \
		!defined(XSSH_FEATURE_CHANNEL_CORE) || \
		!defined(XSSH_FEATURE_CONNECTION_MESSAGE) || \
		!defined(XSSH_FEATURE_REPLY_QUEUE) || \
		!defined(XSSH_FEATURE_TRANSPORT_CORE)
		#error "SSH connection session dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_AUTH_SESSION) || \
		defined(XSSH_FEATURE_TRANSPORT_TCP) || \
		defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_TASK) || \
		defined(XRT_FEATURE_COROUTINE)
		#error "SSH connection session unexpectedly enabled auth, network or runtime"
	#endif

	return xrtSshReplyQueueInit(&Queue, arrTokens, 2u) &&
		xrtSshConnectionSessionInit(
			&Session,
			XSSH_ROLE_CLIENT,
			NULL,
			NULL,
			&Queue
		) ? 0 : 1;
}
