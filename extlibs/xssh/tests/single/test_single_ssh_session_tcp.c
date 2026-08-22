#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_SESSION_TCP
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* TCP 会话闭包只组合确定性 transport 和协议核心。 */
int main(void)
{
	xsshsessiontcpconfig Config;
	xsshsessiontcp Session;

	#if !defined(XSSH_FEATURE_SESSION_TCP) || \
		!defined(XSSH_FEATURE_SESSION_CORE) || \
		!defined(XSSH_FEATURE_TRANSPORT_TCP) || \
		!defined(XRT_FEATURE_NET_TCP)
		#error "SSH TCP session dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH TCP session pulled unrelated runtime dependencies"
	#endif

	if ( !xrtSshSessionTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) || !xrtSshSessionTcpInit(
		&Session,
		NULL,
		&Config,
		0u
	) || (xrtSshSessionTcpAction(&Session) !=
		XSSH_SESSION_ACTION_WRITE_IDENTIFICATION) ) {
		return 1;
	}
	xrtSshSessionTcpClear(&Session);
	return 0;
}
