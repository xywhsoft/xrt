#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CLIENT_SESSION
#define XSSH_IMPLEMENTATION
#include "../../include/xssh/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xssh.h"



/* 验证 session 客户端模块的单头声明与实现闭包。 */
int main(void)
{
	xsshclientconfig Config;

	#if !defined(XSSH_FEATURE_CLIENT_SESSION) || \
		!defined(XSSH_FEATURE_CLIENT) || \
		!defined(XSSH_FEATURE_CHANNEL_REQUEST)
		#error "SSH client session dependency closure is incomplete"
	#endif

	return xrtSshClientConfigInit(&Config) ? 0 : 1;
}
