#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEY_TEXT
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 公钥文本单头只闭包到 Base64、host-key 和 wire。 */
int main(void)
{
	static const char sLine[] =
		"ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	unsigned char arrBlob[64];
	xsshopensshkeyline KeyLine;
	xsshpublickey PublicKey;

	#if !defined(XSSH_FEATURE_KEY_TEXT) || \
		!defined(XSSH_FEATURE_HOSTKEY) || \
		!defined(XRT_FEATURE_CODEC_BASE64)
		#error "SSH key text dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH key text unexpectedly enabled verification, random or network"
	#endif

	return (xrtSshPublicKeyLineRead(
		XRT_STR_LITERAL(sLine),
		&KeyLine
	) == XSSH_OK) && (xrtSshPublicKeyLineDecode(
		&KeyLine,
		arrBlob,
		sizeof(arrBlob),
		&PublicKey
	) == XSSH_OK) ? 0 : 1;
}
