#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_SESSION_STREAM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Stream 便利层只增加 callback TCP 驱动，不携带任务或随机策略。 */
int main(void)
{
	xsshsessiontcpconfig Config;
	xsshsessionstream Session;

	#if !defined(XSSH_FEATURE_SESSION_STREAM) || \
		!defined(XSSH_FEATURE_SESSION_READER) || \
		!defined(XSSH_FEATURE_SESSION_TCP) || \
		!defined(XRT_FEATURE_NET_TCP)
		#error "SSH session stream dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE) || \
		defined(XRT_FEATURE_NET_TCP_FUTURE)
		#error "SSH session stream pulled unrelated runtime dependencies"
	#endif

	if ( !xrtSshSessionTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) || !xrtSshSessionStreamInit(
		&Session,
		&Config,
		NULL,
		NULL
	) || (xrtSshSessionStreamState(
		&Session
	) != XSSH_SESSION_STREAM_CREATED) ||
		(xrtSshSessionStreamTcp(&Session) != NULL) ||
		(xrtSshSessionStreamSession(&Session) != NULL) ||
		!xrtSshSessionStreamClear(&Session) ) {
		return 1;
	}
	return 0;
}
