#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEX_SHA256
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* SHA-256 KEX 单头只增加摘要实现。 */
int main(void)
{
	unsigned char arrShared[1] = { 1u };
	unsigned char arrHash[XSSH_SHA256_SIZE];
	xsshkexhashsha256 Input = { 0 };

	#if !defined(XSSH_FEATURE_KEX_SHA256) || \
		!defined(XRT_FEATURE_CRYPTO_SHA256)
		#error "SSH KEX SHA-256 dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH KEX SHA-256 unexpectedly enabled random or network"
	#endif

	Input.SharedSecret.Data = arrShared;
	Input.SharedSecret.Size = sizeof(arrShared);
	return xrtSshKexHashSha256(&Input, arrHash) == XSSH_OK ? 0 : 1;
}
