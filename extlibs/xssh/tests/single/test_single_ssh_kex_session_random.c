#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEX_SESSION_RANDOM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 随机 KEX 便利层必须完整闭合系统 CSPRNG 与确定性会话。 */
int main(void)
{
	#if !defined(XSSH_FEATURE_KEX_SESSION_RANDOM) || \
		!defined(XSSH_FEATURE_KEX_SESSION) || \
		!defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM) || \
		!defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH KEX session random dependency closure is incomplete"
	#endif

	return 0;
}
