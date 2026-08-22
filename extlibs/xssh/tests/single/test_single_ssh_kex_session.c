#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEX_SESSION
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 确定性 KEX 会话不得隐式携带系统安全随机或签名私钥实现。 */
int main(void)
{
	xsshkexsession Session;

	#if !defined(XSSH_FEATURE_KEX_SESSION) || \
		!defined(XSSH_FEATURE_TRANSPORT_CORE) || \
		!defined(XSSH_FEATURE_KEX_CURVE25519) || \
		!defined(XSSH_FEATURE_HOSTKEY_ED25519)
		#error "SSH KEX session dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_KEX_SESSION_RANDOM) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_CRYPTO_ED25519_SIGN)
		#error "SSH KEX session core pulled convenience-only dependencies"
	#endif

	if ( !xrtSshKexSessionInit(&Session, XSSH_ROLE_CLIENT) ) {
		return 1;
	}
	xrtSshKexSessionClear(&Session);
	return 0;
}
