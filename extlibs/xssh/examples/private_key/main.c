#include <stdio.h>
#include <xssh.h>



/* 解析已经由调用方读取或映射的 openssh-key-v1 二进制容器。 */
int main(void)
{
	static const unsigned char arrMagic[] = XSSH_PRIVATE_KEY_MAGIC;
	unsigned char arrPublic[32];
	unsigned char arrBinary[128];
	xsshwriter PublicWriter;
	xsshwriter Writer;
	xsshopensshprivatekey PrivateKey;
	bool bEncrypted;

	if ( !xrtSshWriterInit(&PublicWriter, arrPublic, sizeof(arrPublic)) ||
		(xrtSshWriteString(
			&PublicWriter,
			XRT_BYTES_LITERAL("example-key")
		) != XSSH_OK) || !xrtSshWriterInit(
			&Writer,
			arrBinary,
			sizeof(arrBinary)
		) || (xrtSshWriteBytes(
			&Writer,
			(xbytesview){ arrMagic, sizeof(arrMagic) }
		) != XSSH_OK) || (xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL("none")
		) != XSSH_OK) || (xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL("none")
		) != XSSH_OK) || (xrtSshWriteString(
			&Writer,
			(xbytesview){ NULL, 0u }
		) != XSSH_OK) || (xrtSshWriteU32(&Writer, 1u) != XSSH_OK) ||
		(xrtSshWriteString(
			&Writer,
			(xbytesview){ arrPublic, PublicWriter.Size }
		) != XSSH_OK) || (xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL("private")
		) != XSSH_OK) || (xrtSshPrivateKeyRead(
			(xbytesview){ arrBinary, Writer.Size },
			&PrivateKey
		) != XSSH_OK) || (xrtSshPrivateKeyIsEncrypted(
			&PrivateKey,
			&bEncrypted
		) != XSSH_OK) ) {
		return 1;
	}
	printf("keys=%u encrypted=%d\n", PrivateKey.KeyCount, bEncrypted ? 1 : 0);
	return 0;
}
