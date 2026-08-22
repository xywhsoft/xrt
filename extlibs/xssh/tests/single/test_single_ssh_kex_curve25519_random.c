#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEX_CURVE25519_RANDOM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 随机 Curve25519 单头显式增加 keypair 与 random_secure。 */
int main(void)
{
	unsigned char arrPrivate[32];
	unsigned char arrPublic[32];

	#if !defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM) || \
		!defined(XRT_FEATURE_CRYPTO_X25519_KEYPAIR) || \
		!defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH random Curve25519 dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK)
		#error "SSH random Curve25519 unexpectedly enabled network"
	#endif

	return xrtSshCurve25519KeyPair(arrPrivate, arrPublic) == XSSH_OK ? 0 : 1;
}
