#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_HOSTKEY
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 主机密钥格式单头只闭包到 wire。 */
int main(void)
{
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE] = { 0u };
	unsigned char arrBlob[64];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_HOSTKEY) || !defined(XSSH_FEATURE_WIRE)
		#error "SSH hostkey dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH hostkey unexpectedly enabled crypto, random or network"
	#endif

	return xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) &&
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){ arrPublic, sizeof(arrPublic) }
		) == XSSH_OK) ? 0 : 1;
}
