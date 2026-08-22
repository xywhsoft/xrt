#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_HOSTKEY_ED25519
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Ed25519 主机密钥验证只增加严格验证依赖。 */
int main(void)
{
	#if !defined(XSSH_FEATURE_HOSTKEY_ED25519) || \
		!defined(XSSH_FEATURE_HOSTKEY) || \
		!defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY)
		#error "SSH Ed25519 hostkey dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO_ED25519_SIGN) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH Ed25519 hostkey unexpectedly enabled sign, random or network"
	#endif

	return 0;
}
