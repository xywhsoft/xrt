#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_AUTH_SESSION
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Auth session 单头只闭合认证 guard、公共消息与 transport core。 */
int main(void)
{
	xsshauthsession Session;

	#if !defined(XSSH_FEATURE_AUTH_SESSION) || \
		!defined(XSSH_FEATURE_AUTH_GUARD) || \
		!defined(XSSH_FEATURE_AUTH_MESSAGE) || \
		!defined(XSSH_FEATURE_TRANSPORT_CORE)
		#error "SSH auth session dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_AUTH_PASSWORD) || \
		defined(XSSH_FEATURE_AUTH_PUBLICKEY) || \
		defined(XSSH_FEATURE_AUTH_KEYBOARD) || \
		defined(XSSH_FEATURE_AUTH_HOSTBASED) || \
		defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH auth session unexpectedly enabled methods, network or random"
	#endif

	return xrtSshAuthSessionInit(&Session, XSSH_ROLE_CLIENT) ? 0 : 1;
}
