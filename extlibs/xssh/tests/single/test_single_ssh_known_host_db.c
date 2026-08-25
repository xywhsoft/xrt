#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KNOWN_HOST_DB
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 数据库单头只增加已显式选择的 SHA-1 兼容闭包。 */
int main(void)
{
	static const char sSource[] =
		"host.example ssh-rsa AAAAB3NzaC1yc2E=\n";
	static const char sKey[] =
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	unsigned char arrKey[64];
	size_t iKeySize = 0u;
	xsshknownhostcheck Check;

	#if !defined(XSSH_FEATURE_KNOWN_HOST_DB) || \
		!defined(XSSH_FEATURE_KNOWN_HOST_HASH) || \
		!defined(XRT_FEATURE_CRYPTO_SHA1)
		#error "SSH known-host database dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH known-host database unexpectedly enabled random or network"
	#endif

	return xrtBase64Decode(
		sKey,
		sizeof(sKey) - 1u,
		arrKey,
		sizeof(arrKey),
		&iKeySize,
		NULL
	) && (xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sSource),
		XRT_STR_LITERAL("host.example"),
		22u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		&Check
	) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_CHANGED) ? 0 : 1;
}
