#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_FINGERPRINT
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 指纹闭包只需要 Base64、SHA-256 与 wire 结果码。 */
int main(void)
{
	char sOutput[64];
	size_t iOutputSize;

	#if !defined(XSSH_FEATURE_FINGERPRINT) || \
		!defined(XRT_FEATURE_CODEC_BASE64) || \
		!defined(XRT_FEATURE_CRYPTO_SHA256)
		#error "SSH fingerprint dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO_MD5) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH fingerprint unexpectedly enabled MD5, random or network"
	#endif

	return xrtSshHostKeyFingerprintSha256(
		XRT_BYTES_LITERAL("key"),
		sOutput,
		sizeof(sOutput),
		&iOutputSize
	) == XSSH_OK ? 0 : 1;
}
