#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CLIENT_FORWARD
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 验证 forwarding 客户端单头依赖闭包。 */
int main(void)
{
	xsshclientconfig Config;

	#if !defined(XSSH_FEATURE_CLIENT_FORWARD) || \
		!defined(XSSH_FEATURE_CLIENT) || \
		!defined(XSSH_FEATURE_FORWARD_MESSAGE)
		#error "SSH client forward dependency closure is incomplete"
	#endif

	return xrtSshClientConfigInit(&Config) ? 0 : 1;
}
