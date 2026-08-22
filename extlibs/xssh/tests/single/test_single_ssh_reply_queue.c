#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_REPLY_QUEUE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Reply queue 单头只依赖 core 与 wire 结果码。 */
int main(void)
{
	uint64 arrTokens[1];
	xsshreplyqueue Queue;

	#if !defined(XSSH_FEATURE_REPLY_QUEUE) || !defined(XSSH_FEATURE_WIRE)
		#error "SSH reply queue dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_UNICODE) || \
		defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH reply queue unexpectedly enabled heavy dependencies"
	#endif

	return xrtSshReplyQueueInit(&Queue, arrTokens, 1u) &&
		(xrtSshReplyQueuePush(&Queue, 1u) == XSSH_OK) ? 0 : 1;
}
