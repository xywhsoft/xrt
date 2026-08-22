#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_SESSION_TCP_RANDOM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 随机 TCP 会话闭包只额外引入安全随机 KEX 与 padding。 */
int main(void)
{
	xsshsessiontcpconfig Config;
	xsshsessiontcp Session;

	#if !defined(XSSH_FEATURE_SESSION_TCP_RANDOM) || \
		!defined(XSSH_FEATURE_SESSION_TCP) || \
		!defined(XSSH_FEATURE_SESSION_CORE_RANDOM) || \
		!defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM) || \
		!defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH random TCP session dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH random TCP session pulled task or coroutine"
	#endif

	if ( !xrtSshSessionTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) || !xrtSshSessionTcpInit(
		&Session,
		NULL,
		&Config,
		0u
	) ) {
		return 1;
	}
	xrtSshSessionTcpClear(&Session);
	return 0;
}
