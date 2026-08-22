#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_AUTH_PUBLICKEY
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Publickey 单头只闭包到公共认证消息与 host-key 格式。 */
int main(void)
{
	unsigned char arrKey[64];
	unsigned char arrPayload[128];
	unsigned char arrRawKey[XSSH_ED25519_PUBLIC_SIZE] = { 0u };
	xsshwriter KeyWriter;
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_AUTH_PUBLICKEY) || \
		!defined(XSSH_FEATURE_AUTH_MESSAGE) || !defined(XSSH_FEATURE_HOSTKEY)
		#error "SSH publickey auth dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH publickey auth unexpectedly enabled crypto, random or network"
	#endif

	return xrtSshWriterInit(&KeyWriter, arrKey, sizeof(arrKey)) &&
		(xrtSshEd25519PublicKeyWrite(
			&KeyWriter,
			(xbytesview){ arrRawKey, sizeof(arrRawKey) }
		) == XSSH_OK) && xrtSshWriterInit(
			&Writer,
			arrPayload,
			sizeof(arrPayload)
		) && (xrtSshAuthPublicKeyWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, KeyWriter.Size }
		) == XSSH_OK) ? 0 : 1;
}
