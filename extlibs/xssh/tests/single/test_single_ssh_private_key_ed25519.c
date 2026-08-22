#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_PRIVATE_KEY_ED25519
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 构建借用身份并执行签名，验证 Ed25519 私钥闭包不携带 PEM 或网络。 */
int main(void)
{
	unsigned char arrSeed[XRT_ED25519_SEED_SIZE] = { 1u };
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE];
	unsigned char arrBlob[64];
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	xsshwriter Writer;
	xsshed25519identity Identity;

	#if !defined(XSSH_FEATURE_PRIVATE_KEY_ED25519) || \
		!defined(XRT_FEATURE_CRYPTO_ED25519_SIGN)
		#error "SSH Ed25519 private-key dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_PEM) || defined(XRT_FEATURE_NETWORK)
		#error "SSH Ed25519 private-key unexpectedly enabled PEM or network"
	#endif

	if ( !xrtEd25519Public(arrSeed, arrPublic) ||
		!xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) ||
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){ arrPublic, sizeof(arrPublic) }
		) != XSSH_OK) ) {
		return 1;
	}
	Identity.PublicKeyBlob.Data = arrBlob;
	Identity.PublicKeyBlob.Size = Writer.Size;
	Identity.Seed.Data = arrSeed;
	Identity.Seed.Size = sizeof(arrSeed);
	Identity.PublicKey.Data = arrPublic;
	Identity.PublicKey.Size = sizeof(arrPublic);
	Identity.Comment.Data = NULL;
	Identity.Comment.Size = 0u;
	return xrtSshPrivateKeyEd25519Sign(
		&Identity,
		XRT_BYTES_LITERAL("message"),
		arrSignature
	) == XSSH_OK ? 0 : 1;
}
