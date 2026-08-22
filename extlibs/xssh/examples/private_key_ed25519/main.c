#include <stdio.h>
#include <xssh.h>



/* 使用借用 Ed25519 身份生成原始签名。 */
int main(void)
{
	unsigned char arrSeed[XRT_ED25519_SEED_SIZE] = { 1u };
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE];
	unsigned char arrBlob[64];
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	xsshwriter Writer;
	xsshed25519identity Identity;

	if ( !xrtEd25519Public(arrSeed, arrPublic) ||
		!xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) ||
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){ arrPublic, sizeof(arrPublic) }
		) != XSSH_OK) ) {
		return 1;
	}
	Identity.PublicKeyBlob = (xbytesview){ arrBlob, Writer.Size };
	Identity.Seed = (xbytesview){ arrSeed, sizeof(arrSeed) };
	Identity.PublicKey = (xbytesview){ arrPublic, sizeof(arrPublic) };
	Identity.Comment = (xbytesview){ NULL, 0u };
	if ( xrtSshPrivateKeyEd25519Sign(
		&Identity,
		XRT_BYTES_LITERAL("message"),
		arrSignature
	) != XSSH_OK ) {
		return 1;
	}
	printf("signature-size=%u\n", XSSH_ED25519_SIGNATURE_SIZE);
	xrtSecureZero(arrSeed, sizeof(arrSeed));
	xrtSecureZero(arrSignature, sizeof(arrSignature));
	return 0;
}
