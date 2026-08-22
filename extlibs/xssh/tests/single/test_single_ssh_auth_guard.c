#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_AUTH_GUARD
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Auth guard 单头只依赖 wire，不隐式拉入时钟或网络。 */
int main(void)
{
	xsshauthguard Guard;

	#if !defined(XSSH_FEATURE_AUTH_GUARD) || !defined(XSSH_FEATURE_WIRE)
		#error "SSH auth guard dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_TIME) || defined(XRT_FEATURE_NETWORK) || \
		defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH auth guard unexpectedly enabled time, network or crypto"
	#endif

	return xrtSshAuthGuardInit(&Guard, NULL, 0u) ? 0 : 1;
}
