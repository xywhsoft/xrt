#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_SESSION_CORE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 会话核心只组合协议状态和动态 transcript，不携带网络驱动或随机便利层。 */
int main(void)
{
	xsshtransportcore Core;
	xsshsessioncore Session;

	#if !defined(XSSH_FEATURE_SESSION_CORE) || \
		!defined(XSSH_FEATURE_KEX_EXCHANGE) || \
		!defined(XSSH_FEATURE_AUTH_SESSION) || \
		!defined(XSSH_FEATURE_CONNECTION_SESSION) || \
		!defined(XRT_FEATURE_NET_BUFFER)
		#error "SSH session core dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_SESSION_CORE_RANDOM) || \
		defined(XSSH_FEATURE_TRANSPORT_TCP) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH session core pulled unrelated runtime dependencies"
	#endif

	if ( !xrtSshTransportCoreInit(
		&Core,
		XSSH_ROLE_CLIENT,
		0u,
		NULL,
		0u
	) || !xrtSshSessionCoreInit(
		&Session,
		NULL,
		XSSH_ROLE_CLIENT,
		NULL,
		NULL,
		NULL
	) || (xrtSshSessionCoreAction(
		&Session,
		&Core
	) != XSSH_SESSION_ACTION_WRITE_IDENTIFICATION) ) {
		return 1;
	}
	xrtSshSessionCoreClear(&Session);
	xrtSshTransportCoreClear(&Core);
	return 0;
}
