#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CLIENT
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 客户端组合层必须带 Stream 和动态 channels，但不引入任务或同步等待。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;

	#if !defined(XSSH_FEATURE_CLIENT) || \
		!defined(XSSH_FEATURE_CLIENT_CORE) || \
		!defined(XSSH_FEATURE_SESSION_STREAM) || \
		!defined(XSSH_FEATURE_CHANNELS)
		#error "SSH client dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH client unexpectedly enabled task or coroutine runtime"
	#endif

	if ( !xrtSshClientConfigInit(&Config) ||
		!xrtSshClientInit(&Client, &Config, NULL, NULL) ||
		!xrtSshClientClear(&Client) ) {
		return 1;
	}
	return 0;
}
