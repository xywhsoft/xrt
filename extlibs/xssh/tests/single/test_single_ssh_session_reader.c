#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_SESSION_READER
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 动态读取器闭包只增加 TCP 会话之上的按需工作区。 */
int main(void)
{
	xsshsessiontcpconfig Config;
	xsshsessionreader Reader;
	xsshsessiontcp Session;

	#if !defined(XSSH_FEATURE_SESSION_READER) || \
		!defined(XSSH_FEATURE_SESSION_TCP) || \
		!defined(XRT_FEATURE_NET_BUFFER)
		#error "SSH session reader dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH session reader pulled unrelated runtime dependencies"
	#endif

	if ( !xrtSshSessionTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) || !xrtSshSessionTcpInit(
		&Session,
		NULL,
		&Config,
		0u
	) || !xrtSshSessionReaderInit(
		&Reader,
		NULL,
		&Session
	) ) {
		return 1;
	}
	xrtSshSessionReaderClear(&Reader);
	xrtSshSessionTcpClear(&Session);
	return 0;
}
