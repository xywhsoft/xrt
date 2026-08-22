#include <stdio.h>
#include <xssh.h>



/* 构建并读取一个 Ed25519 主机公钥 blob。 */
int main(void)
{
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE] = { 1u };
	unsigned char arrBlob[64];
	xsshwriter Writer;
	xbytesview PublicKey;

	if ( !xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) ||
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){ arrPublic, sizeof(arrPublic) }
		) != XSSH_OK) || (xrtSshEd25519PublicKeyRead(
			(xbytesview){ arrBlob, Writer.Size },
			&PublicKey
		) != XSSH_OK) ) {
		return 1;
	}
	printf("ed25519-key=%zu blob=%zu\n", PublicKey.Size, Writer.Size);
	return 0;
}
