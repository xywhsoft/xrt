#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CLIENT_DIAL
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Dial 裁剪闭包只增加 Resolver/TCP Dial，不引入任务、Future 或协程。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;

	#if !defined(XSSH_FEATURE_CLIENT_DIAL) || \
		!defined(XSSH_FEATURE_CLIENT) || \
		!defined(XRT_FEATURE_NET_TCP_DIAL)
		#error "SSH client dial dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_FUTURE) || defined(XRT_FEATURE_TASK) || \
		defined(XRT_FEATURE_COROUTINE)
		#error "SSH client dial unexpectedly enabled async task layers"
	#endif

	if ( !xrtSshClientConfigInit(&Config) ||
		!xrtSshClientInit(&Client, &Config, NULL, NULL) ||
		!xrtSshClientClear(&Client) ) {
		return 1;
	}
	return 0;
}
