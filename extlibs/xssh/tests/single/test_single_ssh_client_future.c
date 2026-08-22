#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CLIENT_FUTURE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Future 裁剪闭包不引入 Task、协程或同步等待。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;

	#if !defined(XSSH_FEATURE_CLIENT_FUTURE) || \
		!defined(XSSH_FEATURE_CLIENT) || \
		!defined(XRT_FEATURE_FUTURE) || \
		!defined(XRT_FEATURE_SPIN)
		#error "SSH client Future dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH client Future unexpectedly enabled task or coroutine"
	#endif

	if ( !xrtSshClientConfigInit(&Config) ||
		!xrtSshClientInit(&Client, &Config, NULL, NULL) ||
		!xrtSshClientClear(&Client) ) {
		return 1;
	}
	return 0;
}
