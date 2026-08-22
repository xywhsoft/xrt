#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEX_CURVE25519
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Curve25519 单头不携带随机密钥对和网络。 */
int main(void)
{
	unsigned char arrPrivate[32] = { 1u };
	unsigned char arrPublic[32];

	#if !defined(XSSH_FEATURE_KEX_CURVE25519) || \
		!defined(XRT_FEATURE_CRYPTO_X25519)
		#error "SSH Curve25519 dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO_X25519_KEYPAIR) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH Curve25519 unexpectedly enabled keypair, random or network"
	#endif

	return xrtSshCurve25519Public(arrPrivate, arrPublic) == XSSH_OK ? 0 : 1;
}
