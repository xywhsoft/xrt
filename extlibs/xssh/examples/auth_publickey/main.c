#include <stdio.h>

#include <xssh.h>



/* 构建 Ed25519 publickey 探测并读取算法。 */
int main(void)
{
	unsigned char arrRawKey[XSSH_ED25519_PUBLIC_SIZE] = { 0u };
	unsigned char arrKey[64];
	unsigned char arrPayload[160];
	xsshwriter KeyWriter;
	xsshwriter Writer;
	xsshauthpublickey PublicKey;

	if ( !xrtSshWriterInit(&KeyWriter, arrKey, sizeof(arrKey)) ||
		(xrtSshEd25519PublicKeyWrite(
			&KeyWriter,
			(xbytesview){ arrRawKey, sizeof(arrRawKey) }
		) != XSSH_OK) || !xrtSshWriterInit(
			&Writer,
			arrPayload,
			sizeof(arrPayload)
		) || (xrtSshAuthPublicKeyWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, KeyWriter.Size }
		) != XSSH_OK) || (xrtSshAuthPublicKeyRead(
			(xbytesview){ arrPayload, Writer.Size },
			&PublicKey
		) != XSSH_OK) ) {
		return 1;
	}
	printf("algorithm=%.*s bytes=%zu\n",
		(int)PublicKey.Algorithm.Size,
		PublicKey.Algorithm.Data,
		Writer.Size);
	return 0;
}
