#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_PRIVATE_KEY_PEM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 用调用方缓冲往返最小容器，验证 PEM 适配闭包。 */
int main(void)
{
	static const unsigned char arrMagic[] = XSSH_PRIVATE_KEY_MAGIC;
	unsigned char arrPublic[32];
	unsigned char arrBinary[128];
	unsigned char arrDecoded[128];
	char sPem[512];
	xsshwriter PublicWriter;
	xsshwriter Writer;
	xsshopensshprivatekey PrivateKey;
	size_t iPemSize;
	size_t iDecodedSize;

	#if !defined(XSSH_FEATURE_PRIVATE_KEY_PEM) || \
		!defined(XRT_FEATURE_PEM)
		#error "SSH private-key PEM dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO_ED25519) || defined(XRT_FEATURE_NETWORK)
		#error "SSH private-key PEM unexpectedly enabled Ed25519 or network"
	#endif

	if ( !xrtSshWriterInit(&PublicWriter, arrPublic, sizeof(arrPublic)) ||
		(xrtSshWriteString(
			&PublicWriter,
			XRT_BYTES_LITERAL("test-key")
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
		) != XSSH_OK) ) {
		return 1;
	}
	return xrtPemEncode(
		XSSH_PRIVATE_KEY_PEM_LABEL,
		arrBinary,
		Writer.Size,
		sPem,
		sizeof(sPem),
		&iPemSize
	) && (xrtSshPrivateKeyPemRead(
		(xstrview){ sPem, iPemSize },
		arrDecoded,
		sizeof(arrDecoded),
		&iDecodedSize,
		&PrivateKey
	) == XSSH_OK) && (iDecodedSize == Writer.Size) ? 0 : 1;
}
