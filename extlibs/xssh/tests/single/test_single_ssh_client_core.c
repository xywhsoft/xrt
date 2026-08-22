#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CLIENT_CORE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 客户端动作核心需要 TCP 会话和安全随机，但不引入 Stream、任务或协程。 */
int main(void)
{
	xsshclientcoreconfig Config;
	xsshclientcore Client;

	#if !defined(XSSH_FEATURE_CLIENT_CORE) || \
		!defined(XSSH_FEATURE_SESSION_READER) || \
		!defined(XSSH_FEATURE_SESSION_TCP_RANDOM) || \
		!defined(XSSH_FEATURE_KEXINIT_RANDOM) || \
		!defined(XSSH_FEATURE_AUTH_PASSWORD)
		#error "SSH client core dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_SESSION_STREAM) || \
		defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH client core unexpectedly enabled stream or task runtime"
	#endif

	if ( !xrtSshClientCoreConfigInit(&Config) ||
		!xrtSshClientCoreInit(&Client, &Config) ) {
		return 1;
	}
	xrtSshClientCoreClear(&Client);
	return 0;
}
